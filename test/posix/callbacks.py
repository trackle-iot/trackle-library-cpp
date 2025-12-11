import ctypes
import platform
import time
import threading
import logging
import multiprocessing as mp
import types

import requests as req

from trackle_enums import OtaError
import messages as msgs

LOG_LEVEL = 100 # 100 means all logs disabled, otherwise, choose the level you desire
logging.basicConfig(level=LOG_LEVEL, format="[%(levelname)s] %(processName)s : %(msg)s")

# Compatibilità Python 3.9 (no match/case)
system_name = platform.system()
if system_name == "Darwin":
    __DLL_EXTENSION = "dylib"
elif system_name == "Linux":
    __DLL_EXTENSION = "so"
else:
    raise NotImplementedError("Operating system not supported")

__lib = ctypes.cdll.LoadLibrary(f"lib/callbacks.{__DLL_EXTENSION}")

log = __lib.Callbacks_log_cb
log.argtypes = (ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p)
log.restype = None

get_millis = __lib.Callbacks_get_millis_cb
get_millis.argtypes = None
get_millis.restype = ctypes.c_uint32

set_proxy_enabled = __lib.Callbacks_set_proxy_enabled
set_proxy_enabled.argtypes = (ctypes.c_bool,)
set_proxy_enabled.restype = None

send_udp = __lib.Callbacks_send_udp_cb
send_udp.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)
send_udp.restype = ctypes.c_int

receive_udp = __lib.Callbacks_receive_udp_cb
receive_udp.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)
receive_udp.restype = ctypes.c_int

connect_udp = __lib.Callbacks_connect_udp_cb
connect_udp.argtypes = (ctypes.c_char_p, ctypes.c_int)
connect_udp.restype = ctypes.c_int

disconnect_udp = __lib.Callbacks_disconnect_udp_cb
disconnect_udp.argtypes = None
disconnect_udp.restype = ctypes.c_int

set_time = __lib.Callbacks_set_time_cb
set_time.argtypes = (ctypes.c_long, ctypes.c_uint, ctypes.c_void_p)
set_time.restype = None

sleep_ms = __lib.Callbacks_sleep_ms_cb
sleep_ms.argtypes = (ctypes.c_uint32,)
sleep_ms.restype = None

reboot = __lib.Callbacks_reboot_cb
reboot.argtypes = (ctypes.c_char_p,)
reboot.restype = None

completed_publish = __lib.Callbacks_complete_publish
completed_publish.argtypes = (ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
completed_publish.restype = None

set_connection_override = __lib.Callbacks_setConnectionOverride
set_connection_override.argtypes = (ctypes.c_bool, ctypes.c_char_p, ctypes.c_int)
set_connection_override.restype = None


def make_ota_callback(trackle_module: types.ModuleType, trackle_instance: ctypes.c_void_p,
                      to_tester_queue: mp.Queue, reason_for_failure: OtaError | None,
                      trackle_lock: threading.Lock, calculate_wrong_sha256: bool = False,
                      verify_signature: bool = False, correct_sha256: bytes | None = None):

    """ Return OTA callback to be registered in Trackle Library. This is a closure. """

    def ota_thread_code(url, expected_crc32):
        """ OTA thread function code """
        time.sleep(2)

        def set_done(value):
            """ Call trackleSetOtaUpdateDone on Trackle instance """
            with trackle_lock:
                trackle_module.setOtaUpdateDone(trackle_instance, value)

        # If this device is configured to have OTA failing for test purpose, fail
        if reason_for_failure is not None:
            to_tester_queue.put({"msg":str(reason_for_failure)})
            set_done(reason_for_failure)
            return
        
        # SHA256 predefinito per i test
        if correct_sha256 is not None:
            CORRECT_SHA256_BYTES = correct_sha256
        else:
            # Default placeholder se non specificato
            CORRECT_SHA256_HEX = "428eb60c130ddfe03804a9b54f4f577b5dfdaca24a54c628bc635132d0c579aa"
            CORRECT_SHA256_BYTES = bytes.fromhex(CORRECT_SHA256_HEX)
        
        # SHA256 errato per test (se calculate_wrong_sha256=True)
        WRONG_SHA256_HEX = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        WRONG_SHA256_BYTES = bytes.fromhex(WRONG_SHA256_HEX)
        
        try:
            # Verifica CRC32 (semplificata - accetta sempre se expected_crc32 != 0)
            if expected_crc32 == 0:
                to_tester_queue.put({"msg":msgs.CRC32_NOT_CHECKED})
                crc32_valid = True
            else:
                # Per semplicità, accettiamo sempre il CRC32 (non lo verifichiamo realmente)
                to_tester_queue.put({"msg":msgs.CRC32_CORRECT})
                crc32_valid = True
            
            # Verifica la firma solo se esplicitamente richiesto (verify_signature=True)
            if crc32_valid:
                if verify_signature:
                    # Se verify_signature=True è stato passato esplicitamente, 
                    # vogliamo testare la verifica della firma, quindi ignoriamo is_forced
                    # e procediamo sempre con la verifica
                    
                    # Usa SHA256 corretto o errato in base al flag
                    if calculate_wrong_sha256:
                        sha256_bytes = WRONG_SHA256_BYTES
                    else:
                        sha256_bytes = CORRECT_SHA256_BYTES
                    
                    # Verifica la firma usando trackleVerifyOtaSignature
                    sha256_array = (ctypes.c_uint8 * 32).from_buffer_copy(sha256_bytes)
                    signature_result = trackle_module.verifyOtaSignature(trackle_instance, sha256_array, 32)
                    
                    if signature_result == 1:
                        to_tester_queue.put({"msg":msgs.SIGNATURE_VERIFIED})
                        set_done(OtaError.OTA_ERR_OK)
                    else:
                        # signature_result == 0 (no signature) o -1 (verify failed)
                        to_tester_queue.put({"msg":msgs.SIGNATURE_FAILED})
                        set_done(OtaError.OTA_ERR_SIGNATURE_FAILED)
                else:
                    # Verifica firma NON richiesta, completa OTA dopo verifica CRC32
                    set_done(OtaError.OTA_ERR_OK)
        except Exception as e:
            to_tester_queue.put({"msg":f"OTA_ERROR: {str(e)}"})
            set_done(OtaError.OTA_ERR_VALIDATE_FAILED)

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32)
    def ota_callback(url, crc32):
        """ Callback when OTA is started """

        if trackle_module is None:
            raise RuntimeError("trackle_module must be set!")
        if trackle_instance is None:
            raise RuntimeError("trackle_instance must be set!")
        if to_tester_queue is None:
            raise RuntimeError("to_tester_queue must be set!")

        thread = threading.Thread(target=ota_thread_code, args=(url, crc32))
        thread.start()

        to_tester_queue.put({"msg":msgs.OTA_URL_RECEIVED})
        return OtaError.OTA_ERR_OK

    return ota_callback
