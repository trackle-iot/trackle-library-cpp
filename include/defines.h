#ifndef Trackle_defines
#define Trackle_defines

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

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
    SOCKET_NOT_CONNECTED = 0,
    SOCKET_CONNECTING,
    SOCKET_READY
} Connection_Status_Type;

typedef enum
{
    CON_ERROR_SEND = -3,
    CON_ERROR_RECEIVE = -2,
    CON_ERROR_SOCKET = -1,
    CON_ERROR_PROTOCOL = 2,
    CON_ERROR_LOOP = 3,
    CON_ERROR_RECONNECTION = 4,
} Cloud_Connection_Error;

typedef int(user_function_int_char_t)(const char *paramString, ...);

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

typedef enum network_disconnect_reason
{
    NETWORK_DISCONNECT_REASON_RESET = 6,  ///< Disconnected to recover from a cloud connection error.
    NETWORK_DISCONNECT_REASON_UNKNOWN = 7 ///< Unspecified disconnection reason.
} network_disconnect_reason;

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
    NET_ACCESS_TECHNOLOGY_LTE = 6,
    NET_ACCESS_TECHNOLOGY_LTE_CAT_M1 = 8,
    NET_ACCESS_TECHNOLOGY_LTE_CAT_NB1 = 9,
} hal_net_access_tech_t;

typedef enum
{
    CONNECTION_TYPE_UNDEFINED = 0,
    CONNECTION_TYPE_WIFI = 1,
    CONNECTION_TYPE_ETHERNET = 2,
    CONNECTION_TYPE_LTE = 3,
    CONNECTION_TYPE_NBIOT = 4,
    CONNECTION_TYPE_CAT_M = 5,
} Connection_Type;

typedef enum
{
    SYSTEM_LAST_RESET_REASON = 1, // sys:reset
    SYSTEM_FREE_MEMORY = 2,       // mem:free
    SYSTEM_BATTERY_CHARGE = 3,    // batt:soc
    SYSTEM_SYSTEM_LOOPS = 4,      // sys:loops
    SYSTEM_APPLICATION_LOOPS = 5, // app:loops
    SYSTEM_UPTIME = 6,            // sys:uptime
    SYSTEM_BATTERY_STATE = 7,     // batt:state
    SYSTEM_POWER_SOURCE = 24,     // pwr::src
    SYSTEM_TOTAL_RAM = 25,        // sys:tram
    SYSTEM_USED_RAM = 26,         // sys:uram
} System;

typedef enum
{
    NETWORK_CONNECTION_STATUS = 8,                                  // net:stat
    NETWORK_CONNECTION_ERROR_CODE = 9,                              // net:err
    NETWORK_DISCONNECTS = 12,                                       // net:dconn
    NETWORK_CONNECTION_ATTEMPTS = 27,                               // net:connatt
    NETWORK_DISCONNECTION_REASON = 28,                              // net:dconnrsn
    NETWORK_IPV4_ADDRESS = 15,                                      // net:ip:addr
    NETWORK_IPV4_GATEWAY = 16,                                      // net.ip:gw
    NETWORK_FLAGS = 17,                                             // net:flags
    NETWORK_COUNTRY_CODE = 18,                                      // net:cntry
    NETWORK_RSSI = 19,                                              // net:rssi
    NETWORK_SIGNAL_STRENGTH_VALUE = 37,                             // net:sigstrv
    NETWORK_SIGNAL_STRENGTH = 33,                                   // net:sigstr
    NETWORK_SIGNAL_QUALITY = 34,                                    // net:sigqual
    NETWORK_SIGNAL_QUALITY_VALUE = 35,                              // net:sigqualv
    NETWORK_ACCESS_TECNHOLOGY = 36,                                 // net:at
    NETWORK_CELLULAR_CELL_GLOBAL_IDENTITY_MOBILE_COUNTRY_CODE = 40, // net:cell:cgi:mcc
    NETWORK_CELLULAR_CELL_GLOBAL_IDENTITY_MOBILE_NETWORK_CODE = 41, // net:cell:cgi:mnc
    NETWORK_CELLULAR_CELL_GLOBAL_IDENTITY_LOCATION_AREA_CODE = 42,  // net:cell:cgi:lac
    NETWORK_CELLULAR_CELL_GLOBAL_IDENTITY_CELL_ID = 43,             // net:cell:cgi:ci
    NETWORK_MAC_ADDRESS_OUI = 91,                                   // net:mac:oui
    NETWORK_MAC_ADDRESS_NIC = 92,                                   // net:mac:nic
} Network;

typedef enum
{
    CLOUD_CONNECTION_STATUS = 10,       // cloud:stat
    CLOUD_CONNECTION_ERROR_CODE = 13,   // cloud:err
    CLOUD_DISCONNECTS = 14,             // cloud:dconn
    CLOUD_CONNECTION_ATTEMPTS = 29,     // cloud:connatt
    CLOUD_DISCONNECTION_REASON = 30,    // cloud:dconnrsn
    CLOUD_REPEATED_MESSAGES = 21,       // coap:resend
    CLOUD_UNACKNOWLEDGED_MESSAGES = 22, // coap:unack
    CLOUD_RATE_LIMITED_EVENTS = 20,     // pub:throttle
    CLOUD_COAP_ROUND_TRIP = 31,         // coap:roundtrip
} Cloud;

typedef enum
{
    TRACKLE_TRACE = 1,
    TRACKLE_INFO = 30,
    TRACKLE_WARN = 40,
    TRACKLE_ERROR = 50,
    TRACKLE_PANIC = 60,
    TRACKLE_NO_LOG = 70
} Log_Level;

typedef enum
{
    NO_OTA = 0,
    PUSH = 1,
    SEND_URL = 2
} Ota_Method;

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
typedef void(publishSendCallback)(const char *eventName, const char *data, uint32_t msg_key, bool published);
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
