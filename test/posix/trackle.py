
""" Python binding to Trackle Library C interface """

import ctypes
import platform

from trackle_enums import *

match platform.system():
    case "Darwin":
        __DLL_EXTENSION = "dylib"
    case "Linux":
        __DLL_EXTENSION = "so"
    case _:
        raise NotImplementedError("Operating system not supported")

__lib = ctypes.cdll.LoadLibrary(f"lib/trackle_library.{__DLL_EXTENSION}")

new = __lib.newTrackle
new.argtypes = None
new.restype = ctypes.c_void_p

init = __lib.trackleInit
init.argtypes = [ctypes.c_void_p]
init.restype = None

setDeviceId = __lib.trackleSetDeviceId
setDeviceId.argtypes = [ctypes.c_void_p, (ctypes.c_uint8*12)]
setDeviceId.restype = None

setKeys = __lib.trackleSetKeys
setKeys.argtypes = [ctypes.c_void_p, (ctypes.c_uint8*121)]
setKeys.restype = None

LOG_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p,
                          ctypes.c_void_p)
setLogCallback = __lib.trackleSetLogCallback
setLogCallback.argtypes = [ctypes.c_void_p, LOG_CB]
setLogCallback.restype = None

setLogLevel = __lib.trackleSetLogLevel
setLogLevel.argtypes = [ctypes.c_void_p, ctypes.c_int]
setLogLevel.restype = None

setEnabled = __lib.trackleSetEnabled
setEnabled.argtypes = [ctypes.c_void_p, ctypes.c_bool]
setEnabled.restype = None

MILLIS_CB = ctypes.CFUNCTYPE(ctypes.c_uint32)
setMillis = __lib.trackleSetMillis
setMillis.argtypes = [ctypes.c_void_p, MILLIS_CB]
setMillis.restype = None

SEND_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)
setSendCallback = __lib.trackleSetSendCallback
setSendCallback.argtypes = [ctypes.c_void_p, SEND_UDP_CB]
setSendCallback.restype = None

RECV_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)
setReceiveCallback = __lib.trackleSetReceiveCallback
setReceiveCallback.argtypes = [ctypes.c_void_p, RECV_UDP_CB]
setReceiveCallback.restype = None

CONNECT_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int)
setConnectCallback = __lib.trackleSetConnectCallback
setConnectCallback.argtypes = [ctypes.c_void_p, CONNECT_UDP_CB]
setConnectCallback.restype = None

DISCONNECT_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int)
setDisconnectCallback = __lib.trackleSetDisconnectCallback
setDisconnectCallback.argtypes = [ctypes.c_void_p, DISCONNECT_UDP_CB]
setDisconnectCallback.restype = None

SYSTEM_TIME_CB = ctypes.CFUNCTYPE(None, ctypes.c_long, ctypes.c_uint, ctypes.c_void_p)
setSystemTimeCallback = __lib.trackleSetSystemTimeCallback
setSystemTimeCallback.argtypes = [ctypes.c_void_p, SYSTEM_TIME_CB]
setSystemTimeCallback.restype = None

SLEEP_CB = ctypes.CFUNCTYPE(None, ctypes.c_uint32)
setSleepCallback = __lib.trackleSetSleepCallback
setSleepCallback.argtypes = [ctypes.c_void_p, SLEEP_CB]
setSleepCallback.restype = None

REBOOT_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p)
setSystemRebootCallback = __lib.trackleSetSystemRebootCallback
setSystemRebootCallback.argtypes = [ctypes.c_void_p, REBOOT_CB]
setSystemRebootCallback.restype = None

setFirmwareVersion = __lib.trackleSetFirmwareVersion
setFirmwareVersion.argtypes = [ctypes.c_int]
setFirmwareVersion.restype = None

setOtaMethod = __lib.trackleSetOtaMethod
setOtaMethod.argtypes = [ctypes.c_int]
setOtaMethod.restype = None

OTA_UPDATE_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32)
setOtaUpdateCallback = __lib.trackleSetOtaUpdateCallback
setOtaUpdateCallback.argtypes = [ctypes.c_void_p, OTA_UPDATE_CB]
setOtaUpdateCallback.restype = None

setOtaUpdateDone = __lib.trackleSetOtaUpdateDone
setOtaUpdateDone.argtypes = [ctypes.c_void_p, ctypes.c_int]
setOtaUpdateDone.restype = None

setConnectionType = __lib.trackleSetConnectionType
setConnectionType.argtypes = [ctypes.c_int]
setConnectionType.restype = None

setClaimCode = __lib.trackleSetClaimCode
setClaimCode.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
setClaimCode.restype = None

setComponentsList = __lib.trackleSetComponentsList
setComponentsList.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
setComponentsList.restype = None

setImei = __lib.trackleSetImei
setImei.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
setImei.restype = None

setIccid = __lib.trackleSetIccid
setIccid.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
setIccid.restype = None

setPublishHealthCheckInterval = __lib.trackleSetPublishHealthCheckInterval
setPublishHealthCheckInterval.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
setPublishHealthCheckInterval.restype = None

COMPLETED_PUBLISH_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_char_p,
                                        ctypes.POINTER(ctypes.c_uint32),
                                        ctypes.POINTER(ctypes.c_uint8))
setCompletedPublishCallback = __lib.trackleSetCompletedPublishCallback
setCompletedPublishCallback.argtypes = [ctypes.c_void_p, COMPLETED_PUBLISH_CB]
setCompletedPublishCallback.restype = None

SEND_PUBLISH_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32,
                                   ctypes.c_bool)
setSendPublishCallback = __lib.trackleSetSendPublishCallback
setSendPublishCallback.argtypes = [ctypes.c_void_p, SEND_PUBLISH_CB]
setSendPublishCallback.restype = None

SIGNAL_CB = ctypes.CFUNCTYPE(None, ctypes.c_bool, ctypes.c_uint, ctypes.c_void_p)
trackleSetSignalCallback = __lib.trackleSetSignalCallback
trackleSetSignalCallback.argtypes = [ctypes.c_void_p, SIGNAL_CB]
trackleSetSignalCallback.restype = None

connect = __lib.trackleConnect
connect.argtypes = [ctypes.c_void_p]
connect.restype = ctypes.c_int

get_time = __lib.trackleGetTime
get_time.argtypes = [ctypes.c_void_p]
get_time.restype = ctypes.c_int

disconnect = __lib.trackleDisconnect
disconnect.argtypes = [ctypes.c_void_p]
disconnect.restype = None

POST_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p)
post = __lib.tracklePost
post.argtypes = [ctypes.c_void_p, ctypes.c_char_p, POST_CB, ctypes.c_int]
post.restype = ctypes.c_bool

GET_BOOL_CB = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_char_p)
register_get_bool = __lib.TestAuxFun_trackleGetBool
register_get_bool.argtypes = (ctypes.c_void_p, ctypes.c_char_p, GET_BOOL_CB)
register_get_bool.restype = ctypes.c_bool

GET_INT32_CB = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_char_p)
register_get_int32 = __lib.TestAuxFun_trackleGetInt32
register_get_int32.argtypes = (ctypes.c_void_p, ctypes.c_char_p, GET_INT32_CB)
register_get_int32.restype = ctypes.c_bool

GET_DOUBLE_CB = ctypes.CFUNCTYPE(ctypes.c_double, ctypes.c_char_p)
register_get_double = __lib.TestAuxFun_trackleGetDouble
register_get_double.argtypes = (ctypes.c_void_p, ctypes.c_char_p, GET_DOUBLE_CB)
register_get_double.restype = ctypes.c_bool

GET_STRING_CB = ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_char_p)
register_get_string = __lib.TestAuxFun_trackleGetString
register_get_string.argtypes = (ctypes.c_void_p, ctypes.c_char_p, GET_STRING_CB)
register_get_string.restype = ctypes.c_bool

GET_JSON_CB = ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_char_p)
register_get_json = __lib.TestAuxFun_trackleGetJson
register_get_json.argtypes = (ctypes.c_void_p, ctypes.c_char_p, GET_JSON_CB)
register_get_json.restype = ctypes.c_bool


publish = __lib.tracklePublish
publish.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
                    ctypes.c_int, ctypes.c_int]
publish.restype = ctypes.c_bool

loop = __lib.trackleLoop
loop.argtypes = [ctypes.c_void_p]
loop.restype = None

connected = __lib.trackleConnected
connected.argtypes = [ctypes.c_void_p]
connected.restype = ctypes.c_bool
