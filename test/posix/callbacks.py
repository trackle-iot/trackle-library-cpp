import ctypes
import platform

match platform.system():
    case "Darwin":
        __DLL_EXTENSION = "dylib"
    case "Linux":
        __DLL_EXTENSION = "so"
    case _:
        raise Exception("invalid os")

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
