#ifndef Trackle_defines
#define Trackle_defines

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#if defined(__arm__)
#include <cstdlib>
#endif

#define DEFAULT_TTL 60

typedef enum
{
    VAR_BOOLEAN = 1,
    VAR_INT = 2,
    VAR_STRING = 4,
    VAR_CHAR = 5,
    VAR_LONG = 6,
    VAR_JSON = 7,
    VAR_DOUBLE = 9
} Data_TypeDef;

typedef enum
{
    ALL_USERS = 1,
    OWNER_ONLY = 2
} Function_PermissionDef;

typedef enum
{
    SOCKET_NOT_CONNECTED = -1,
    SOCKET_CONNECTING = 0,
    SOCKET_CONNECTED = 1,
    SOCKET_READY = 2
} Connection_Status_Type;

typedef int (*user_function_int_char_t)(const char *paramString, ...);

typedef bool *(*user_variable_bool_cb_t)(const char *paramString);
typedef int *(*user_variable_int_cb_t)(const char *paramString);
typedef double *(*user_variable_double_cb_t)(const char *paramString);
typedef const char *(*user_variable_char_cb_t)(const char *paramString);

typedef void (*EventHandler)(const char *name, const char *data);

typedef enum
{
    PUBLIC = 'e',
    PRIVATE = 'E'
} Event_Type;

typedef enum
{
    EMPTY_FLAGS = 0,
    NO_ACK = 0x2,
    WITH_ACK = 0x8,
    ALL_FLAGS = NO_ACK | WITH_ACK
} Event_Flags;

typedef enum
{
    OTA_ERROR = 0x00,
    OTA_SUCCESS = 0x01,
    OTA_VALIDATE_ONLY = 0x02,
    OTA_DONT_RESET = 0x04
} UpdateFlag;

typedef enum
{
    MY_DEVICES,
    ALL_DEVICES
} Subscription_Scope_Type;

typedef enum
{
    BATTERY_STATE_UNKNOWN = 0,
    BATTERY_STATE_NOT_CHARGING = 1,
    BATTERY_STATE_CHARGING = 2,
    BATTERY_STATE_CHARGED = 3,
    BATTERY_STATE_DISCHARGING = 4,
    BATTERY_STATE_FAULT = 5,
    BATTERY_STATE_DISCONNECTED = 6
} battery_state_t;

typedef enum
{
    POWER_SOURCE_UNKNOWN = 0,
    POWER_SOURCE_VIN = 1,
    POWER_SOURCE_USB_HOST = 2,
    POWER_SOURCE_USB_ADAPTER = 3,
    POWER_SOURCE_USB_OTG = 4,
    POWER_SOURCE_BATTERY = 5
} power_source_t;

typedef enum
{
    NET_ACCESS_TECHNOLOGY_UNKNOWN = 0,
    NET_ACCESS_TECHNOLOGY_NONE = 0,
    NET_ACCESS_TECHNOLOGY_WIFI = 1,
    NET_ACCESS_TECHNOLOGY_GSM = 2,
    NET_ACCESS_TECHNOLOGY_EDGE = 3,
    NET_ACCESS_TECHNOLOGY_UMTS = 4,
    NET_ACCESS_TECHNOLOGY_UTRAN = 4,
    NET_ACCESS_TECHNOLOGY_WCDMA = 4,
    NET_ACCESS_TECHNOLOGY_CDMA = 5,
    NET_ACCESS_TECHNOLOGY_LTE = 6,
    NET_ACCESS_TECHNOLOGY_IEEE802154 = 7,
    NET_ACCESS_TECHNOLOGY_LTE_CAT_M1 = 8,
    NET_ACCESS_TECHNOLOGY_LTE_CAT_NB1 = 9,
} hal_net_access_tech_t;

typedef enum
{
    SOURCE = 24,
    BATTERY_CHARGE = 3,
    BATTERY_STATE = 7
} Power;

typedef enum
{
    UPTIME = 6,
    MEMORY_TOTAL = 25,
    MEMORY_USED = 26
} System;

typedef enum
{
    CELLULAR_MOBILE_COUNTRY_CODE = 40,
    CELLULAR_MOBILE_NETWORK_CODE = 41,
    CELLULAR_LOCATION_AREA_CODE = 42,
    CELLULAR_CELL_ID = 43,
    SIGNAL_RSSI = 19,
    SIGNAL_STRENGTH = 33,
    SIGNAL_STRENGTH_V = 37,
    SIGNAL_QUALITY = 34,
    SIGNAL_QUALITY_V = 35,
    SIGNAL_AT = 36
} Network;

typedef enum
{
    CONNECTION_STATUS = 10,
    CONNECTION_ERROR = 13,
    CONNECTION_DISCONNECT = 14,
    CONNECTION_ATTEMPS = 29
} Cloud;

typedef enum
{
    TRACE = 1,
    INFO = 30,
    WARN = 40,
    ERROR = 50,
    PANIC = 60,
    NO_LOG = 70
} Log_Level;

typedef enum
{
    NO_OTA = 0,
    PUSH = 1,
    SEND_URL = 2
} Ota_Method;

typedef enum
{
    UNDEFINED = 0,
    WIFI = 1,
    ETHERNET = 2,
    CELLULAR = 3,
    NBIOT = 4
} Connection_Type;

struct Chunk
{
    uint32_t chunk_count;
    uint32_t chunk_address;
    uint16_t chunk_size;
    uint32_t file_length;
};

// The size of the persisted data
#define SessionPersistBaseSize 208
#define SessionPersistVariableSize (sizeof(int) + sizeof(int) + sizeof(size_t))

typedef struct SessionPersistDataOpaque
{
    uint16_t size;
    uint8_t data[SessionPersistBaseSize - sizeof(uint16_t) + SessionPersistVariableSize];
} SessionPersistDataOpaque;

#define DEVICE_ID_LENGTH 12
#define PUBLIC_KEY_LENGTH 92
#define PRIVATE_KEY_LENGTH 122

typedef uint32_t system_tick_t;

typedef system_tick_t(millisCallback)(void);
typedef int(sendCallback)(const unsigned char *buf, uint32_t buflen, void *tmp);
typedef int(receiveCallback)(unsigned char *buf, uint32_t buflen, void *tmp);
typedef int(connectCallback)(const char *address, int port);
typedef int(disconnectCallback)(void);
typedef void(publishCompletionCallback)(int error, const void *data, void *callbackData, void *reserved);
typedef void(publishSendCallback)(const char *eventName, const char *data, const char *key, bool published);
typedef void(prepareFirmwareUpdateCallback)(struct Chunk data, uint32_t flags, void *reserved);
typedef void(firmwareChunkCallback)(struct Chunk data, const unsigned char *chunk, void *);
typedef void(finishFirmwareUpdateCallback)(char *data, uint32_t fileSize);
typedef void(firmwareUrlUpdateCallback)(const char *data);
typedef void(connectionStatusCallback)(Connection_Status_Type status);
typedef int(updateStateCallback)(const char *function_key, const char *arg, ...);
typedef void(signalCallback)(bool on, unsigned int param, void *reserved);
typedef void(timeCallback)(time_t time, unsigned int param, void *);
typedef void(rebootCallback)(const char *data);
typedef void(pincodeCallback)(const char *data);
typedef void(sleepCallback)(uint32_t millis);
typedef void(logCallback)(const char *msg, int level, const char *category, void *attribute, void *reserved);
typedef int(restoreSessionCallback)(void *buffer, size_t length, uint8_t type, void *reserved);
typedef int(saveSessionCallback)(const void *buffer, size_t length, uint8_t type, void *reserved);
typedef uint32_t(randomNumberCallback)(void);

#endif
