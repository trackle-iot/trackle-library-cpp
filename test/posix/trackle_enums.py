
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
