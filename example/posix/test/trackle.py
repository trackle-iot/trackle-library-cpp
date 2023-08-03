import ctypes
import platform
import enum

match platform.system():
    case "Darwin":
        __DLL_EXTENSION = "dylib"
    case "Linux":
        __DLL_EXTENSION = "so"
    case _:
        raise Exception("invalid os")

__lib = ctypes.cdll.LoadLibrary(f"../lib/trackle_library.{__DLL_EXTENSION}")

# Log levels
class LogLevel(enum.IntEnum):
    TRACE = 1
    INFO = 30
    WARN = 40
    ERROR = 50
    PANIC = 60
    NO_LOG = 70

# OTA methods
class OTAMethod(enum.IntEnum):
    NO_OTA = 0
    PUSH = 1
    SEND_URL = 2

# Connection type
class ConnectionType(enum.IntEnum):
    UNDEFINED = 0
    WIFI = 1
    ETHERNET = 2
    LTE = 3
    NBIOT = 4
    CAT_M = 5

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

LOG_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p)
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

SEND_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_void_p)
setSendCallback = __lib.trackleSetSendCallback
setSendCallback.argtypes = [ctypes.c_void_p, SEND_UDP_CB]
setSendCallback.restype = None

RECV_UDP_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_void_p)
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

setConnectionType = __lib.trackleSetConnectionType
setConnectionType.argtypes = [ctypes.c_int]
setConnectionType.restype = None

setPublishHealthCheckInterval = __lib.trackleSetPublishHealthCheckInterval
setPublishHealthCheckInterval.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
setPublishHealthCheckInterval.restype = None

COMPLETED_PUBLISH_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
setCompletedPublishCallback = __lib.trackleSetCompletedPublishCallback
setCompletedPublishCallback.argtypes = [ctypes.c_void_p, COMPLETED_PUBLISH_CB]
setCompletedPublishCallback.restype = None

connect = __lib.trackleConnect
connect.argtypes = [ctypes.c_void_p]
connect.restype = ctypes.c_int

POST_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p)
post = __lib.tracklePost
post.argtypes = [ctypes.c_void_p, ctypes.c_char_p, POST_CB, ctypes.c_int]
post.restype = ctypes.c_bool

GET_CB = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p)
get = __lib.trackleGet
get.argtypes = [ctypes.c_void_p, ctypes.c_char_p, GET_CB, ctypes.c_int]
get.restype = ctypes.c_bool

loop = __lib.trackleLoop
loop.argtypes = [ctypes.c_void_p]
loop.restype = None
