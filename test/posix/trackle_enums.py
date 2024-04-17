
""" Enums accepted by Trackle Library's C interface """

import enum

class PublishVisibility(enum.IntEnum):
    """ Visibility of a published event """
    PUBLIC = ord('e')
    PRIVATE = ord('E')

class PublishType(enum.IntEnum):
    """ Confirmability of a published event """
    NO_ACK = 0x2
    WITH_ACK = 0x8

class PermissionDef(enum.IntEnum):
    """ Access permission of a POST function """
    ALL_USERS = 1
    OWNER_ONLY = 2

class LogLevel(enum.IntEnum):
    """ Available log levels """
    TRACE = 1
    INFO = 30
    WARN = 40
    ERROR = 50
    PANIC = 60
    NO_LOG = 70

class OTAMethod(enum.IntEnum):
    """ Available OTA methods """
    NO_OTA = 0
    PUSH = 1
    SEND_URL = 2

class ConnectionType(enum.IntEnum):
    """ Technology used to connect to the cloud """
    UNDEFINED = 0
    WIFI = 1
    ETHERNET = 2
    LTE = 3
    NBIOT = 4
    CAT_M = 5

class OtaError(enum.IntEnum):
    """ States of OTA process """
    OTA_ERR_OK = 0,             #  No error
    OTA_ERR_ALREADY_RUNNING = 1 #  OTA already in progress
    OTA_ERR_PARTITION = 2       #  partition error (not found, invalid, conflict, etc..)
    OTA_ERR_MEMORY = 3          #  not enough free memory
    OTA_ERR_VALIDATE_FAILED = 4 #  image validation failed (crc, wrong platform, etc..)
    OTA_ERR_INCOMPLETE = 5      #  download interrupter
    OTA_ERR_COMPLETING = 6      #  download completed but image not validated
    OTA_ERR_GENERIC = 7         #  all other errors
