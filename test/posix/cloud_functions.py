
""" Callbacks used specifically by test """

import ctypes
import platform

match platform.system():
    case "Darwin":
        __DLL_EXTENSION = "dylib"
    case "Linux":
        __DLL_EXTENSION = "so"
    case _:
        raise OSError("Operating system not supported")

__lib = ctypes.cdll.LoadLibrary(f"lib/cloud_functions.{__DLL_EXTENSION}")

get_echo_bool = __lib.CloudFun_getEchoBool
get_echo_bool.argtypes = (ctypes.c_char_p,)
get_echo_bool.restype = ctypes.c_bool

get_echo_int = __lib.CloudFun_getEchoInt
get_echo_int.argtypes = (ctypes.c_char_p,)
get_echo_int.restype = ctypes.c_int32

get_echo_double = __lib.CloudFun_getEchoDouble
get_echo_double.argtypes = (ctypes.c_char_p,)
get_echo_double.restype = ctypes.c_double

get_echo_string = __lib.CloudFun_getEchoString
get_echo_string.argtypes = (ctypes.c_char_p,)
get_echo_string.restype = ctypes.c_char_p

get_echo_json = __lib.CloudFun_getEchoJson
get_echo_json.argtypes = (ctypes.c_char_p,)
get_echo_json.restype = ctypes.c_char_p

post_failing = __lib.CloudFun_failingPost
post_failing.argtypes = (ctypes.c_char_p,)
post_failing.restype = ctypes.c_int

post_success = __lib.CloudFun_successPost
post_success.argtypes = (ctypes.c_char_p,)
post_success.restype = ctypes.c_int

post_private = __lib.CloudFun_privatePost
post_private.argtypes = (ctypes.c_char_p,)
post_private.restype = ctypes.c_int

complete_publish = __lib.CloudFun_completePublish
complete_publish.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32),
                             ctypes.POINTER(ctypes.c_uint8))
complete_publish.restype = None

send_publish = __lib.CloudFun_sendPublish
send_publish.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_bool)
send_publish.restype = None

signal_callback = __lib.CloudFun_signalCallback
signal_callback.argtypes = (ctypes.c_bool, ctypes.c_uint, ctypes.c_void_p)
signal_callback.restype = None

reboot = __lib.CloudFun_rebootCallback
reboot.argtypes = (ctypes.c_char_p,)
reboot.restype = None

set_time_callback = __lib.CloudFun_setTimeCallback
set_time_callback.argtypes = (ctypes.c_long, ctypes.c_uint, ctypes.c_void_p)
set_time_callback.restype = None

def was_private_post_executed() -> bool:
    """ Return true if private post for test was executed """
    return ctypes.c_bool.in_dll(__lib, "privatePostExecuted").value

def was_signal_called() -> int:
    """ Return true if signal callback was executed """
    return ctypes.c_bool.in_dll(__lib, "signalCalled").value

def reset_signal_called() -> None:
    """ Reset signal called """
    ctypes.c_bool.in_dll(__lib, "signalCalled").value = False

def was_reboot_called() -> int:
    """ Return true if reboot callback was executed """
    return ctypes.c_bool.in_dll(__lib, "rebootCalled").value

def reset_reboot_called() -> None:
    """ Reset reboot called """
    ctypes.c_bool.in_dll(__lib, "rebootCalled").value = False

def was_get_time_called() -> int:
    """ Return true if get_time callback was executed """
    return ctypes.c_bool.in_dll(__lib, "setTimeCalled").value

def reset_get_time_called() -> None:
    """ Reset get_time called """
    ctypes.c_bool.in_dll(__lib, "setTimeCalled").value = False

get_publish_completed = __lib.CloudFun_getPublishComplete
get_publish_completed.argtypes = (ctypes.c_size_t,)
get_publish_completed.restype = ctypes.c_bool

get_publish_error = __lib.CloudFun_getPublishError
get_publish_error.argtypes = (ctypes.c_size_t,)
get_publish_error.restype = ctypes.c_int

get_publish_send = __lib.CloudFun_getPublishSend
get_publish_send.argtypes = (ctypes.c_size_t,)
get_publish_send.restype = ctypes.c_bool

get_publish_published = __lib.CloudFun_getPublishPublished
get_publish_published.argtypes = (ctypes.c_size_t,)
get_publish_published.restype = ctypes.c_int

reset_publish_completed = __lib.CloudFun_resetPublishComplete
reset_publish_completed.argtypes = (ctypes.c_size_t,)
reset_publish_completed.restype = None

reset_publish_sent = __lib.CloudFun_resetPublishSent
reset_publish_sent.argtypes = (ctypes.c_size_t,)
reset_publish_sent.restype = None
