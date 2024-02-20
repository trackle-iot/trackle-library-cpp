import ctypes
import platform
import zlib
import struct
import threading
import logging
import multiprocessing as mp
import types

import requests as req

from trackle_enums import OtaError
import messages as msgs

LOG_LEVEL = 100 # 100 means all logs disabled, otherwise, choose the level you desire
logging.basicConfig(level=LOG_LEVEL, format="[%(levelname)s] %(processName)s : %(msg)s")

match platform.system():
    case "Darwin":
        __DLL_EXTENSION = "dylib"
    case "Linux":
        __DLL_EXTENSION = "so"
    case _:
        raise NotImplementedError("Operating system not supported")

__lib = ctypes.cdll.LoadLibrary(f"lib/callbacks.{__DLL_EXTENSION}")

log = __lib.Callbacks_log_cb
log.argtypes = (ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p)
log.restype = None

get_millis = __lib.Callbacks_get_millis_cb
get_millis.argtypes = None
get_millis.restype = ctypes.c_uint32

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
                      trackle_lock: threading.Lock):

    """ Return OTA callback to be registered in Trackle Library. This is a closure. """

    def ota_thread_code(url, expected_crc32):
        """ OTA thread function code """

        def crc32_le(b):
            """ Calculate CRC32 with bytes in little-endian """
            crc32_bytes = zlib.crc32(b).to_bytes(4, 'little')
            crc32_int = struct.unpack(">I", crc32_bytes)
            return crc32_int[0]

        def set_done(value):
            """ Call trackleSetOtaUpdateDone on Trackle instance """
            with trackle_lock:
                trackle_module.setOtaUpdateDone(trackle_instance, value)

        # If this device is configured to have OTA failing for test purpose, fail
        if reason_for_failure is not None:
            to_tester_queue.put({"msg":str(reason_for_failure)})
            logging.error(reason_for_failure)
            set_done(reason_for_failure)
            return
        
        # Else download firmware and behave as a normal device during OTA
        result = req.get(url, timeout=30)
        if result.status_code == 200:
            calculated_crc32 = crc32_le(result.content)
            if calculated_crc32 == expected_crc32:
                to_tester_queue.put({"msg":msgs.CRC32_CORRECT})
                logging.info("correct crc32")
                set_done(OtaError.OTA_ERR_OK)
            elif expected_crc32 == 0:
                to_tester_queue.put({"msg":msgs.CRC32_NOT_CHECKED})
                logging.info("not checking crc32")
                set_done(OtaError.OTA_ERR_OK)
            else:
                to_tester_queue.put({"msg":msgs.CRC32_MISMATCH})
                logging.error(f"crc32 don't match (got '{calculated_crc32}, expected '{expected_crc32}'')")
                set_done(OtaError.OTA_ERR_VALIDATE_FAILED)
        else:
            to_tester_queue.put({"msg":msgs.DOWNLOAD_INTERRUPTED})
            logging.error("download interrupted")
            set_done(OtaError.OTA_ERR_GENERIC)

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
        logging.info("OTA URL received")
        return OtaError.OTA_ERR_OK

    return ota_callback
