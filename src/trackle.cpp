//
//  Trackle.cpp
//
//  Created by Flavio Ferrandi on 14/09/17.
//  Copyright © 2017 Flavio Ferrandi. All rights reserved.
//

#include "trackle.h"
#include "protocol_defs.h"
#include "version.h"
#include <vector>
#include <map>
#include <math.h>

#include "hal_platform.h"

#include <stddef.h>
#include <stdint.h>
#include <sstream>
#include <iomanip>

#include "dtls_protocol.h"

#define USER_VAR_MAX_COUNT 10
#define USER_VAR_KEY_LENGTH 64

#define USER_FUNC_MAX_COUNT 4
#define USER_FUNC_KEY_LENGTH 64
#define USER_FUNC_ARG_LENGTH 622

#define RECONNECTION_TIMEOUT 5000
#define DIAGNOSTIC_TIMEOUT 5000

#define MAX_COUNTER 9999999

const uint32_t PUBLISH_EVENT_FLAG_PUBLIC = 0x0;
const uint32_t PUBLISH_EVENT_FLAG_PRIVATE = 0x1;
const int CLAIM_CODE_SIZE = 63;

// DICHIARAZIONI  ------------------------------------------------------------
trackle::protocol::DTLSProtocol protocol_instance;
ProtocolFacade *protocol = &protocol_instance;

TrackleKeys keys;
TrackleCallbacks callbacks;
TrackleDescriptor descriptor;

connectCallback *connectCb = NULL;
disconnectCallback *disconnectCb = NULL;
receiveCallback *receiveCb = NULL;
sendCallback *sendCb = NULL;
publishCompletionCallback *completedPublishCb = NULL;
publishSendCallback *sendPublishCb = NULL;
prepareFirmwareUpdateCallback *prepareFirmwareCb = NULL;
firmwareChunkCallback *firmwareChunkCb = NULL;
finishFirmwareUpdateCallback *finishUpdateCb = NULL;
usedMemoryCallback *usedMemoryCb = NULL;
randomNumberCallback *getRandomCb = NULL;
rebootCallback *systemRebootCb = NULL;
firmwareUrlUpdateCallback *firmwareUrlCb = NULL;
pincodeCallback *pincodeCb = NULL;
connectionStatusCallback *connectionStatusCb = NULL;
updateStateCallback *updateStateCb = NULL;

uint32_t counter = 0; // MAX_COUNTER 9.999.999
uint32_t prefix = 0;  // 4.294.967.296 -> 1.990.000.000

/**
 * It generates a random number in the range [1, 199] and uses it as the prefix for the publish counter
 *
 * @return The next publish counter.
 */

uint32_t getNextPublishCounter()
{
    uint32_t p = prefix;
    if (p == 0)
    { // init
// get an unbiased random in (0, 199], so that we get ids=prefix+counter (p_ppc_ccc_ccc) in the range [10_000_000, 1_999_999_999]
#if MAX_COUNTER != 9999999
#error "The current MAX_COUNTER value requires a tweak in getNextPublishCounter()"
#endif
        constexpr uint32_t top = 199;
        constexpr uint32_t max_v = 0xFFFFFFFF / top * top;
        for (int i = 0; i < 20; ++i)
        {
            uint32_t r = HAL_RNG_GetRandomNumber();
            if (r >= max_v)
            {
                p = (r % top) + 1;
                break;
            }
        }
        if (p == 0)
        {
            prefix = 0xFFFFFFFF;
            LOG(WARN, "Couldn't generate a proper random prefix for the publish counter; use 0");
        }
    }
    else if (p == 0xFFFFFFFF)
    { // fallback on error
        p = 0;
    }
    counter++;
    if (counter >= MAX_COUNTER)
    {
        counter = 0;
    }
    return p | counter;
}

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

/**
 * It takes a pointer to a char array and the length of the array, and returns a string containing the
 * hexadecimal representation of the data in the array
 *
 * @param data The data to be converted to hex
 * @param len The length of the data to be converted.
 *
 * @return A string of hexadecimal characters.
 */
std::string hexStr(char *data, int len)
{
    std::string s(len * 2, ' ');
    for (int i = 0; i < len; ++i)
    {
        s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
}

/*** CONNECTION STATUS ***/
/*
 SOCKET_NOT_CONNECTED
 SOCKET_CONNECTING
 SOCKET_CONNECTED
 SOCKET_READY
 */
Connection_Status_Type connectionStatus = SOCKET_NOT_CONNECTED;
int cloudStatus = -1;

trackle::protocol::Connection_Properties_Type connectionPropTypeList[6] = {
    {30, 5, 2},   // UNDEFINED
    {30, 5, 2},   // WIFI
    {30, 5, 2},   // ETHERNET
    {30, 5, 2},   // CELLULAR
    {150, 10, 5}, // NBIOT
};                // in seconds

uint32_t pingInterval = 0;
Connection_Type connectionType = UNDEFINED;
trackle::protocol::Connection_Properties_Type connectionPropType;

bool cloudEnabled = true;
bool connectToCloud = false;
system_tick_t millis_last_disconnection = 0;
system_tick_t millis_started_at = 0;
system_tick_t millis_last_check_diagnostic = 0;
system_tick_t millis_diagnostic = 0;

// OTA
Ota_Method otaMethod = NO_OTA;
bool updates_pending = false;
bool updates_enabled = true;
bool updates_forced = false;

system_tick_t millis_last_sent_received_time = 0;
system_tick_t millis_last_sent_health_check = 0;
system_tick_t health_check_interval = 0;

// ------------------------------------------------------------

string string_device_id;
char device_id[DEVICE_ID_LENGTH];
// 294byte + 1 byte (len n server address) + n byte server address + 2 byte server port
// byte aggiuntivi dopo chiave: \x10\x74\x65\x73\x74\x2e\x69\x6f\x74\x72\x65\x61\x64\x79\x2e\x69\x74\x16\x33
unsigned char server_public_key[PUBLIC_KEY_LENGTH];
unsigned char client_private_key[PRIVATE_KEY_LENGTH];
map<int16_t, int32_t> diagnostic;
char claim_code[CLAIM_CODE_SIZE + 1];

// TRACKLE.VARIABLE ------------------------------------------------------------

struct CloudVariableTypeBase
{
    char userVarKey[USER_VAR_KEY_LENGTH + 1];
    Data_TypeDef userVarType;
    Data_TypeDef stringVarType;
    void *value;
    void (*funct)(const char *);
    CloudVariableTypeBase(void *val, const char *varKey, Data_TypeDef type)
    {
        strncpy(userVarKey, varKey, sizeof(userVarKey) - 1);
        userVarKey[sizeof(userVarKey) - 1] = '\0';
        userVarType = type;
        value = val;
        funct = NULL;
    };
    CloudVariableTypeBase(void (*fn)(const char *), const char *varKey, Data_TypeDef type)
    {
        strncpy(userVarKey, varKey, sizeof(userVarKey) - 1);
        userVarKey[sizeof(userVarKey) - 1] = '\0';
        userVarType = type;
        value = NULL;
        funct = fn;
    };
};

std::vector<CloudVariableTypeBase> vars;

/**
 * It searches the vars array for a variable with the given key, and returns a pointer to that variable
 * if found, or NULL if not found
 *
 * @param varKey The key of the variable to be found.
 *
 * @return A pointer to the variable.
 */
CloudVariableTypeBase *find_var_by_key(const char *varKey)
{
    for (int i = (int)vars.size(); i-- > 0;)
    {
        if (0 == strncmp(vars[i].userVarKey, varKey, USER_VAR_KEY_LENGTH))
        {
            return &vars[i];
        }
    }
    return NULL;
}

/**
 * It takes a long integer and returns a string
 *
 * @param number The number to be converted to a string.
 *
 * @return A string
 */
static string int_to_string(long number)
{
    std::string out_string;
    std::stringstream ss;
    ss << number;
    out_string = ss.str();
    return out_string;
}

/**
 * It returns the type of a variable, given its key
 *
 * @param varKey The variable key.
 *
 * @return The type of the variable.
 */
int userVarType(const char *varKey)
{
    CloudVariableTypeBase *item = find_var_by_key(varKey);
    return item ? item->userVarType : -1;
}

void Trackle::setEnabled(bool status)
{
    cloudEnabled = status;
}

bool Trackle::isEnabled()
{
    return cloudEnabled;
}

bool Trackle::addGet(const char *varKey, void (*fn)(const char *), Data_TypeDef userVarType)
{
    if (!varKey)
    {
        LOG(WARN, "Tried to set variable with NULL name");
        return false;
    }
    if (varKey[0] == '\0')
    {
        LOG(WARN, "Tried to set variable with empty name");
        return false;
    }
    if (!fn)
    {
        LOG(WARN, "Tried to set variable callback\"%s\" with NULL pointer", varKey);
        return false;
    }

    CloudVariableTypeBase *old_item = find_var_by_key(varKey);

    if (old_item)
    {
        LOG(WARN, "Tried to add already-existing var (\"%s\" exists)", old_item->userVarKey);
        return false;
    }

    // TODO CHECK MAX VAR NUMBER

    if (userVarType == VAR_BOOLEAN)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_BOOLEAN);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as boolean with value \"%d\"", item.userVarKey, 0);
    }
    else if (userVarType == VAR_INT)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_INT);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as int with value \"%d\"", item.userVarKey, 0);
    }
    else if (userVarType == VAR_LONG)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_LONG);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as long with value \"%d\"", item.userVarKey, 0);
    }
    else if (userVarType == VAR_STRING)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_STRING);
        item.stringVarType = VAR_STRING;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as string value \"%s\"", item.userVarKey, "");
    }
    else if (userVarType == VAR_JSON)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_JSON);
        item.stringVarType = VAR_JSON;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as json value \"%s\"", item.userVarKey, "");
    }
    else if (userVarType == VAR_CHAR)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_STRING);
        item.stringVarType = VAR_CHAR;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as char value \"%s\"", item.userVarKey, "");
    }
    else if (userVarType == VAR_DOUBLE)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(fn, varKey, VAR_DOUBLE);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as double with value \"%f\"", item.userVarKey, 0);
    }
    else
    {
        LOG(WARN, "Tried to set var \"%s\" with unknown type %d)", fn, 0);
        return false;
    }

    return true;
}

bool Trackle::addGet(const char *varKey, void *userVar, Data_TypeDef userVarType)
{
    if (!varKey)
    {
        LOG(WARN, "Tried to set variable with NULL name");
        return false;
    }
    if (varKey[0] == '\0')
    {
        LOG(WARN, "Tried to set variable with empty name");
        return false;
    }
    if (!userVar)
    {
        LOG(WARN, "Tried to set variable \"%s\" with NULL pointer", varKey);
        return false;
    }

    CloudVariableTypeBase *old_item = find_var_by_key(varKey);

    if (old_item)
    {
        LOG(WARN, "Tried to add already-existing var \"%s\" (\"%s\" exists)", userVar, old_item->userVarKey);
        return false;
    }

    // TODO CHECK MAX VAR NUMBER

    if (userVarType == VAR_BOOLEAN)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_BOOLEAN);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as boolean with value \"%d\"", item.userVarKey, *(bool *)item.value);
    }
    else if (userVarType == VAR_INT)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_INT);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as int with value \"%d\"", item.userVarKey, *(int *)item.value);
    }
    else if (userVarType == VAR_LONG)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_LONG);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as long with value \"%d\"", item.userVarKey, *(long *)item.value);
    }
    else if (userVarType == VAR_STRING)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_STRING);
        item.stringVarType = VAR_STRING;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as string value \"%s\"", item.userVarKey, (*(string *)item.value).c_str());
    }
    else if (userVarType == VAR_JSON)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_JSON);
        item.stringVarType = VAR_JSON;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as string value \"%s\"", item.userVarKey, (*(string *)item.value).c_str());
    }
    else if (userVarType == VAR_CHAR)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_STRING);
        item.stringVarType = VAR_CHAR;
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as char value \"%s\"", item.userVarKey, item.value);
    }
    else if (userVarType == VAR_DOUBLE)
    {
        CloudVariableTypeBase item = CloudVariableTypeBase(userVar, varKey, VAR_DOUBLE);
        vars.push_back(item);
        LOG(TRACE, "Set variable \"%s\" as double with value \"%f\"", item.userVarKey, *(double *)item.value);
    }
    else
    {
        LOG(WARN, "Tried to set var \"%s\" with unknown type %d)", userVar, (int)userVarType);
        return false;
    }

    return true;
}

bool Trackle::get(const char *varKey, user_variable_bool_cb_t *fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, ((void (*)(const char *))(fn)), VAR_BOOLEAN);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_int_cb_t *fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, ((void (*)(const char *))(fn)), VAR_INT);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_double_cb_t *fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, ((void (*)(const char *))(fn)), VAR_DOUBLE);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_char_cb_t *fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, ((void (*)(const char *))(fn)), VAR_CHAR);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, void (*fn)(const char *), Data_TypeDef type)
{
    return addGet(varKey, fn, type);
}

// TRACKLE.FUNCTION ------------------------------------------------------------

struct CloudFunctionTypeBase
{
    user_function_int_char_t *pUserFunc;
    Function_PermissionDef permission;
    char userFuncKey[USER_FUNC_KEY_LENGTH + 1];
    CloudFunctionTypeBase(const char *funcKey, user_function_int_char_t *userFunc, Function_PermissionDef perms)
    {
        strncpy(userFuncKey, funcKey, sizeof(userFuncKey));
        userFuncKey[sizeof(userFuncKey) - 1] = '\0';
        pUserFunc = userFunc;
        permission = perms;
    };
};

std::vector<CloudFunctionTypeBase> funcs;
std::vector<string> owners;

/**
 * Check if the user_id is in the owners vector
 *
 * @param user_id The user ID of the user to check.
 *
 * @return A boolean value.
 */
bool user_is_owner(const char *user_id)
{
    if (!user_id)
    {
        return NULL;
    }
    for (int i = (int)owners.size(); i-- > 0;)
    {
        if (0 == strcmp(owners[i].c_str(), user_id))
        {
            return true;
        }
    }
    return false;
}

/**
 * It searches the `funcs` array for a function with the given `funcKey` and returns a pointer to the
 * function if found, or `NULL` if not found
 *
 * @param funcKey The key of the function to be found.
 *
 * @return A pointer to the function that matches the key.
 */
CloudFunctionTypeBase *find_func_by_key(const char *funcKey)
{
    if (!funcKey)
    {
        return NULL;
    }
    for (int i = (int)funcs.size(); i-- > 0;)
    {
        if (0 == strncmp(funcs[i].userFuncKey, funcKey, USER_FUNC_KEY_LENGTH))
        {
            return &funcs[i];
        }
    }
    return NULL;
}

bool Trackle::post(const char *funcKey, user_function_int_char_t *func, Function_PermissionDef permission)
{
    CloudFunctionTypeBase *old_item = find_func_by_key(funcKey);

    if (old_item)
    {
        LOG(WARN, "Tried to add already-existing function \"%s\" (\"%s\" exists)", funcKey, old_item->userFuncKey);
        return false;
    }

    // TODO CHECK MAX FUNCTION NUMBER

    CloudFunctionTypeBase item = CloudFunctionTypeBase(funcKey, func, permission);
    funcs.push_back(item);
    LOG(TRACE, "Set %s function \"%s\"", (permission == ALL_USERS ? "PUBLIC" : "OWNER ONLY"), item.userFuncKey);
    return true;
}

// TRACKLE.PUBLISH

/**
 * It converts from the API flags to the communications lib flags
 * The event visibility flag (public/private) is encoded differently. The other flags map directly.
 *
 * @param flags The flags for the event.
 *
 * @return The flags with the private flag removed and the public flag set.
 */
inline uint32_t convert(uint32_t flags)
{
    bool priv = flags & PUBLISH_EVENT_FLAG_PRIVATE;
    flags &= ~PUBLISH_EVENT_FLAG_PRIVATE;
    flags |= !priv ? EventType::PUBLIC : EventType::PRIVATE;
    return flags;
}

bool Trackle::sendPublish(const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, string msg_id)
{
    if (!cloudEnabled)
        return false;

    uint32_t flags = eventType | eventFlag;
    flags = convert(flags);

    trackle_protocol_send_event_data d = {};

    if (eventFlag & WITH_ACK)
    { // se c'è il flag WITH_ACK

        uint32_t key = 0;
        if (msg_id.compare("") == 0)
        {
            key = getNextPublishCounter();
        }
        else
        {
            key = atoi(msg_id.c_str());
        }

        d.handler_callback = completedPublishCb;
        d.handler_data = (void *)key;

        if (connectionStatus == SOCKET_READY)
        { // publish send ok
            LOG(TRACE, "sendPublishCb OK");
            if (sendPublishCb)
                (*sendPublishCb)(eventName, data, int_to_string(key).c_str(), true);
        }
        else
        { // publish send error
            LOG(TRACE, "sendPublishCb ERROR");
            if (sendPublishCb)
                (*sendPublishCb)(eventName, data, int_to_string(key).c_str(), false);
        }
    }

    LOG(TRACE, "sendPublish %s: %s ", eventName, data);
    int res = 0;
    if (connectionStatus == SOCKET_READY)
        res = trackle_protocol_send_event(protocol, eventName, data, ttl, flags, &d);
    return res;
}

bool Trackle::publish(const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, string msg_id)
{
    return sendPublish(eventName, data, ttl, eventType, eventFlag, msg_id);
}

bool Trackle::publish(string eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, string msg_id)
{
    return sendPublish(eventName.c_str(), data, ttl, eventType, eventFlag, msg_id);
}

bool Trackle::publish(const char *eventName, const char *data, Event_Type eventType, Event_Flags eventFlag, string msg_id)
{
    return sendPublish(eventName, data, DEFAULT_TTL, eventType, eventFlag, msg_id);
}

bool Trackle::publish(string eventName, const char *data, Event_Type eventType, Event_Flags eventFlag, string msg_id)
{
    return sendPublish(eventName.c_str(), data, DEFAULT_TTL, eventType, eventFlag, msg_id);
}

bool Trackle::publish(const char *eventName)
{
    return sendPublish(eventName, NULL, DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, "");
}

bool Trackle::publish(string eventName)
{
    return sendPublish(eventName.c_str(), NULL, DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, "");
}

bool Trackle::syncState(const char *data)
{
    return sendPublish("trackle/p", data, DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, "");
}

bool Trackle::syncState(string data)
{
    return sendPublish("trackle/p", data.c_str(), DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, "");
}

// TRACKLE.SUBSCRIBE

/**
 * It checks if the socket is ready.
 *
 * @return A boolean value.
 */
bool cloud_flag_connected(void)
{
    if (connectionStatus == SOCKET_READY)
        return true;
    else
        return false;
}

/**
 * Convert a Subscription_Scope_Type enum to a SubscriptionScope::Enum enum.
 *
 * @param subscription_type This is the type of subscription you want to create. It can be either
 * MY_DEVICES or FIREHOSE.
 *
 * @return A pointer to a new instance of the Subscription class.
 */
SubscriptionScope::Enum convert(Subscription_Scope_Type subscription_type)
{
    return (subscription_type == MY_DEVICES) ? SubscriptionScope::MY_DEVICES : SubscriptionScope::FIREHOSE;
}

bool Trackle::registerEvent(const char *eventName, Subscription_Scope_Type eventScope, const char *deviceId)
{
    bool success;
    if (deviceId)
    {
        success = trackle_protocol_send_subscription_device(protocol, eventName, deviceId);
    }
    else
    {
        SubscriptionScope::Enum scope = convert(eventScope);
        success = trackle_protocol_send_subscription_scope(protocol, eventName, scope);
    }

    LOG(TRACE, "register_event %d\n", success);
    return success;
}

bool Trackle::addSubscription(const char *eventName, EventHandler handler, void *handlerData,
                              Subscription_Scope_Type scope, const char *deviceId, void *reserved)

{
    char charDeviceId[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (deviceId != NULL)
    {
        std::string string_device_id = deviceId;
        int L = strnlen(deviceId, 2 * DEVICE_ID_LENGTH + 1);
        if (L == 2 * DEVICE_ID_LENGTH)
        { // hex-string device id
            stringstream ss;
            unsigned int buffer;
            int offset = 0;
            while (offset < string_device_id.length())
            {
                ss.clear();
                ss << hex << string_device_id.substr(offset, 2);
                ss >> buffer;
                charDeviceId[offset / 2] = buffer;
                offset += 2;
            }
        }
        else if (L == DEVICE_ID_LENGTH)
        { // binary device id
            // TODO: does the (L == DEVICE_ID_LENGTH) check work for binary data? is 0 guaranteed never to be a byte of the id?? is the array NUL-terminated even if it contains binary data?
            memcpy(charDeviceId, deviceId, DEVICE_ID_LENGTH);
        }
        else
        { // wrong device id
            LOG(ERROR, "Wrong device id length in subscription");
        }
    }

    SubscriptionScope::Enum eventScope = convert(scope);
    bool success = trackle_protocol_add_event_handler(protocol, eventName, handler, eventScope, charDeviceId, handlerData);
    if (success && cloud_flag_connected())
    {
        registerEvent(eventName, scope, deviceId);
    }
    return success;
}

bool Trackle::subscribe(const char *eventName, EventHandler handler)
{
    return addSubscription(eventName, handler, NULL, ALL_DEVICES, NULL, NULL);
}

bool Trackle::subscribe(const char *eventName, EventHandler handler, Subscription_Scope_Type scope)
{
    return addSubscription(eventName, handler, NULL, scope, NULL, NULL);
}

bool Trackle::subscribe(const char *eventName, EventHandler handler, const char *deviceID)
{
    return addSubscription(eventName, handler, NULL, MY_DEVICES, deviceID, NULL);
}

bool Trackle::subscribe(const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceID)
{
    return addSubscription(eventName, handler, NULL, scope, deviceID, NULL);
}

void Trackle::unsubscribe()
{
    trackle_protocol_remove_event_handlers(protocol, NULL);
}

/**
 * It handles all the events that are sent to the device from the Trackle cloud
 *
 * @param handler This is the pointer to the Trackle object.
 * @param event_name The name of the event that was published.
 * @param data the data that was sent with the event
 */
void subscribe_trackle_handler(void *handler, const char *event_name, const char *data)
{
    LOG(TRACE, "trackle handler %s, %s\n", event_name, data);

    bool replyWithPublish = false;

    // if event trackle/device/updates/pending, set ota pending var
    if (strcmp(event_name, "trackle/device/updates/pending") == 0)
    {
        updates_pending = (strcmp(data, "true") == 0 ? true : false);
        replyWithPublish = true;
    }
    else if (strcmp(event_name, "trackle/device/updates/forced") == 0)
    {
        updates_forced = (strcmp(data, "true") == 0 ? true : false);
        replyWithPublish = true;
    }
    else if (strcmp(event_name, "trackle/device/owners") == 0)
    {

        owners.clear(); // empty vector
        if (data != NULL)
        {
            std::stringstream ss(data);
            while (ss.good())
            {
                string substr;
                getline(ss, substr, ',');
                owners.push_back(substr.c_str());
            }
        }
    }
    else if (strcmp(event_name, "trackle/device/reset") == 0)
    {
        if (systemRebootCb)
        {
            (*systemRebootCb)(data);
        }
        else
        {
            LOG(INFO, "systemRebootCb not implemented...");
        }
    }
    else if (strcmp(event_name, "trackle/device/update") == 0)
    {
        if (firmwareUrlCb)
        {
            (*firmwareUrlCb)(data);
        }
        else
        {
            LOG(INFO, "firmwareUrlCb not implemented...");
        }
    }
    else if (strcmp(event_name, "trackle/device/pin_code") == 0)
    {
        if (pincodeCb)
        {
            (*pincodeCb)(data);
        }
        else
        {
            LOG(INFO, "pincodeCb not implemented...");
        }
    }

    if (replyWithPublish)
        ((Trackle *)handler)->publish(event_name, data, PRIVATE);
}

// TRACKLE.CALLBACK ------------------------------------------------------------

/**
 * If the OTA upgrade was successful, return true, otherwise return false
 *
 * @return The return value is a boolean value.
 */
bool was_ota_upgrade_successful(void) { return false; } // TODO

/**
 * It resets the status of the OTA flash.
 */
void HAL_OTA_Flashed_ResetStatus(void) {}

/**
 * It takes a variable key and returns the variable type
 *
 * @param varKey The variable key you want to get the type of.
 *
 * @return The return type of the variable.
 */
TrackleReturnType::Enum wrapVarTypeInEnum(const char *varKey)
{
    CloudVariableTypeBase *item = find_var_by_key(varKey);
    if (item->userVarType == VAR_BOOLEAN)
    {
        return TrackleReturnType::BOOLEAN;
    }
    else if (item->userVarType == VAR_INT)
    {
        return TrackleReturnType::INT;
    }
    else if (item->userVarType == VAR_LONG)
    {
        return TrackleReturnType::LONG;
    }
    else if (item->userVarType == VAR_STRING)
    {
        return TrackleReturnType::STRING;
    }
    else if (item->userVarType == VAR_JSON)
    {
        return TrackleReturnType::JSON;
    }
    else if (item->userVarType == VAR_DOUBLE)
    {
        return TrackleReturnType::DOUBLE;
    }

    return TrackleReturnType::INT;
}

/* Checking if the variable is calculated. */
bool isVarCalculated(const char *varKey)
{
    CloudVariableTypeBase *item = find_var_by_key(varKey);

    if (item->funct != NULL)
        return true;
    return false;
}

/**
 * It returns the number of functions in the current program
 *
 * @return The number of functions in the program.
 */
int num_functions(void)
{
    LOG(TRACE, "num_functions %d", funcs.size());
    return (int)funcs.size();
}

/**
 * This function returns the user function key for the function at the specified index
 *
 * @param function_index The index of the function in the array of functions.
 *
 * @return The user function key.
 */
const char *getUserFunctionKey(int function_index)
{
    LOG(TRACE, "getUserFunctionKey");
    return funcs[function_index].userFuncKey;
}

/**
 * It prints the event name and data to the log
 *
 * @param event_name The name of the event.
 * @param data The data that was sent with the event.
 */
void event_handler_trackle(const char *event_name, const char *data)
{
    LOG(TRACE, "received event %s: %s", event_name, data);
}

/**
 * It's a callback function that is called by the Trackle server when a user calls the update_state
 * function
 *
 * @param function_key The name of the function to be called.
 * @param arg the argument passed to the function
 * @param user_caller_id The user id of the user who is calling the function.
 * @param callback This is the callback function that will be called when the function is done.
 * @param  `function_key`: the name of the function to be called.
 *
 * @return The return value is the result of the function.
 */

int update_state(const char *function_key, const char *arg, const char *user_caller_id,
                 TrackleDescriptor::FunctionResultCallback callback, void *)
{
    LOG(TRACE, "update state %s with value %s", function_key, arg);
    LOG(TRACE, "user_caller_id %s", user_caller_id);

    if (updateStateCb)
    {
        int result = (*updateStateCb)(function_key, arg, user_is_owner(user_caller_id));
        callback((void *)result, TrackleReturnType::INT);
        return 0;
    }
    return 5; // error 500 updateStateCb not exists
}

/**
 * It calls the function with the given key, passing the given argument, and returns the result to the
 * caller
 *
 * @param function_key The name of the function to call.
 * @param arg The argument passed to the function.
 * @param user_caller_id The user id of the user who called the function.
 * @param callback This is the callback function that will be called when the function is called from
 * the cloud.
 * @param  `function_key`: The name of the function to call.
 *
 * @return The return value is the result of the function call.
 */
int call_function(const char *function_key, const char *arg, const char *user_caller_id,
                  TrackleDescriptor::FunctionResultCallback callback, void *)
{

    LOG(TRACE, "call_function");
    LOG(TRACE, "user_caller_id %s", user_caller_id);

    CloudFunctionTypeBase *function = find_func_by_key(function_key);

    if (function != NULL)
    {
        if (function->permission == ALL_USERS || (function->permission == OWNER_ONLY && user_is_owner(user_caller_id)))
        {
            int result = (*function->pUserFunc)(arg, user_is_owner(user_caller_id));
            callback((void *)result, TrackleReturnType::INT);
            LOG(TRACE, "function %s called with args %s, result = %d", function_key, arg, result);
        }
        else
        {
            LOG(ERROR, "user %s not authorized to call function %s", user_caller_id, function_key);
            return 3;
        }
    }
    else
    {
        LOG(ERROR, "function %s called with args %s, does not exists!", function_key, arg);
        return 4;
    }

    return 0;
}

/**
 * This function returns the number of user variables in the current program
 *
 * @return The number of user variables.
 */
int numUserVariables(void)
{
    LOG(TRACE, "numUserVariables %d", vars.size());
    return (int)vars.size();
}

/**
 * This function returns the key of the user variable at the specified index
 *
 * @param variable_index The index of the variable to get the key for.
 *
 * @return The key of the user variable.
 */
const char *getUserVariableKey(int variable_index)
{
    LOG(TRACE, "getUserVariableKey");
    return vars[variable_index].userVarKey;
}
/**
 * It returns a pointer to the value of the variable
 *
 * @param varKey The name of the variable you want to get the value of.
 *
 * @return The value of the variable.
 */
const void *getUserVar(const char *varKey)
{
    CloudVariableTypeBase *item = find_var_by_key(varKey);

    if (item->funct != NULL)
    { // calculated var, execute and return value
        return (const void *)item->funct;
    }
    else
    { // not calculated, return value from pointer
        if (item->stringVarType == VAR_STRING)
        {
            const char *value = (*(string *)item->value).c_str();
            return (void *)value;
        }
        else
        {
            return item->value;
        }
    }
}

/**
 * It takes the diagnostic map and appends it to the appender
 *
 * @param appender A function pointer to the appender function.
 * @param append This is the appender object that is passed in. It is used to append the metrics to the
 * buffer.
 * @param flags 0x00
 * @param page The page number of the metrics.
 * @param reserved reserved for future use
 *
 * @return A boolean value.
 */
bool appendMetrics(appender_fn appender, void *append, uint32_t flags, uint32_t page, void *reserved)
{

    // add sys:loop (0x00 0x04 already in appender)
    ((Appender *)append)->append(0x02);
    ((Appender *)append)->append((char)0x00);
    ((Appender *)append)->append(0x04);
    ((Appender *)append)->append((char)0x00);

    map<int16_t, int32_t>::iterator itr;
    for (itr = diagnostic.begin(); itr != diagnostic.end(); ++itr)
    {
        ((Appender *)append)->append((char)(itr->first >> 0 & 0xff));
        ((Appender *)append)->append((char)(itr->first >> 8 & 0xff));
        ((Appender *)append)->append((char)(itr->second >> 0 & 0xff));
        ((Appender *)append)->append((char)(itr->second >> 8 & 0xff));
        ((Appender *)append)->append((char)(itr->second >> 16 & 0xff));
        ((Appender *)append)->append((char)(itr->second >> 24 & 0xff));
    }

    return true;
}

/**
 * It returns a string with the system information
 *
 * @param appender A function pointer to the function that will be used to append the data to the
 * buffer.
 * @param append The function to call to append the data to the JSON string.
 * @param reserved Reserved for future use.
 *
 * @return The system information.
 */
bool appendSystemInfo(appender_fn appender, void *append, void *reserved)
{
    product_details_t details;
    details.size = sizeof(details);

    string json = "\"i\":" + int_to_string(connectionPropType.ping_interval) + "." + int_to_string(connectionType) + ",\"o\":" + int_to_string(otaMethod) + ",\"p\":" + int_to_string(PLATFORM_ID) + ",\"s\":\"" + int_to_string(VERSION_MAYOR) + "." + int_to_string(VERSION_MINOR) + "." + int_to_string(VERSION_PATCH) + "\"";

    LOG(ERROR, "%s", json.c_str());
    const char *result = json.c_str();
    ((Appender *)append)->append(result);
    return true;
}

/* CRC-32 (Ethernet, ZIP, etc.) polynomial in reversed bit order. */
#define POLY 0xedb88320

/**
 * Calculate CRC-32
 *
 * @param crc The initial value of the CRC.
 * @param buf The buffer to calculate the CRC32C for.
 * @param len the length of the data to be crc'ed
 *
 * @return The CRC32 checksum of the data.
 */
uint32_t crc32c(uint32_t crc, const unsigned char *buf, uint32_t len)
{
    int k;

    crc = ~crc;
    while (len--)
    {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

/**
 * It takes a pointer to a buffer and a length, and returns a CRC32C value
 *
 * @param data The data to calculate the CRC for.
 * @param len the length of the data to be CRC'd
 *
 * @return The CRC32C checksum of the data.
 */
uint32_t calculateCrc(const unsigned char *data, uint32_t len)
{
    return crc32c(0, data, len);
}

/*** TESTING UTILS ***/

/**
 * It takes a variable key as a parameter, finds the variable in the list of variables, and prints the
 * value of the variable to the console
 *
 * @param varKey The variable key that you want to print the value of.
 */
void printType(const char *varKey)
{

    CloudVariableTypeBase *item = find_var_by_key(varKey);

    if (item->userVarType == VAR_BOOLEAN)
    {
        LOG(TRACE, "ACTUAL BOOL %s: %d", item->userVarKey, *(bool *)item->value);
    }
    else if (item->userVarType == VAR_INT)
    {
        LOG(TRACE, "ACTUAL INT %s: %d", item->userVarKey, *(int *)item->value);
    }
    else if (item->userVarType == VAR_LONG)
    {
        LOG(TRACE, "ACTUAL LONG %s: %d", item->userVarKey, *(long *)item->value);
    }
    else if (item->userVarType == VAR_STRING)
    {
        LOG(TRACE, "ACTUAL STRING %s: %s", item->userVarKey, (*(string *)item->value).c_str());
    }
    else if (item->userVarType == VAR_JSON)
    {
        LOG(TRACE, "ACTUAL JSON %s: %s", item->userVarKey, (*(string *)item->value).c_str());
    }
    else if (item->userVarType == VAR_DOUBLE)
    {
        LOG(TRACE, "ACTUAL DOUBLE %s: %f", item->userVarKey, *(double *)item->value);
    }
}

void Trackle::test(string param)
{
    LOG(TRACE, "=========================================");

    for (int i = (int)vars.size(); i-- > 0;)
    {
        printType(vars[i].userVarKey);
    }

    LOG(TRACE, "-----------------------------------------");

    for (int i = (int)funcs.size(); i-- > 0;)
    {
        LOG(TRACE, "testing function %s with param %s", funcs[i].userFuncKey, param.c_str());
        int result = (*funcs[i].pUserFunc)(param.c_str());
        LOG(TRACE, "function %s result = %d", param.c_str(), result);
    }

    LOG(TRACE, "=========================================");

    string event_string = "test_string";
    publish(event_string);

    publish(event_string, "params2a");
    publish(event_string, "params2b", 120);
    publish(event_string, "params2c", 120, PRIVATE);
    publish(event_string, "params2d", 120, PRIVATE, WITH_ACK);

    publish(event_string, "params3a", PRIVATE);
    publish(event_string, "params3b", WITH_ACK);
    publish(event_string, "params3c", PRIVATE, WITH_ACK);

    LOG(TRACE, "-----------------------------------------");

    const char *event_char = "test_char";
    publish(event_char);

    publish(event_char, "params2a");
    publish(event_char, "params2b", 120);
    publish(event_char, "params2c", 120, PRIVATE);
    publish(event_char, "params2d", 120, PRIVATE, WITH_ACK);

    publish(event_char, "params3a", PRIVATE);
    publish(event_char, "params3b", WITH_ACK);
    publish(event_char, "params3c", PRIVATE, WITH_ACK);

    LOG(TRACE, "=========================================");
}

// CLOUD / CALLBACKS

void Trackle::setMillis(millisCallback *millis)
{
    callbacks.millis = millis;
    millis_started_at = (*callbacks.millis)();
}

/**
 * If the new status is different from the current status, and a callback function has been registered,
 * call the callback function
 *
 * @param newStatus The new connection status.
 */
void setConnectionStatus(Connection_Status_Type newStatus)
{
    if (newStatus != connectionStatus && connectionStatusCb)
    {
        (*connectionStatusCb)(newStatus);
    }
    connectionStatus = newStatus;
}

/**
 * If the connection is ready or if the force parameter is true, then set the connection status to not
 * connected and call the disconnect callback
 *
 * @param error_type The error type.
 * @param force If true, the connection will be closed even if it's not connected.
 */
void connectionError(int error_type, bool force = false)
{
    if (connectionStatus == SOCKET_READY || force)
    { // if connected or trying to connect
        millis_last_disconnection = (*callbacks.millis)();
        LOG(ERROR, "Cloud connection error %d, %lu", error_type, millis_last_disconnection);
        setConnectionStatus(SOCKET_NOT_CONNECTED);
        (*disconnectCb)();
    }
}

/**
 * This function is called by the library to send data to the server
 *
 * @param buf The buffer to send
 * @param buflen the length of the buffer to send
 * @param tmp a pointer to a temporary buffer that can be used by the send function.
 *
 * @return The number of bytes sent.
 */
int wrapSend(const unsigned char *buf, uint32_t buflen, void *tmp)
{
    if (!sendCb)
        return -1;
    int bytes_sent = (*sendCb)(buf, buflen, tmp);
    if (bytes_sent < 0)
    { // if sending error
        connectionError(bytes_sent);
    }
    if (bytes_sent > 0)
    {
        millis_last_sent_received_time = (*callbacks.millis)();
    }
    return bytes_sent;
}

void Trackle::setSendCallback(sendCallback *send)
{
    sendCb = send;
}

/**
 * It calls the receive callback function, and if it returns an error, it calls the connectionError
 * function
 *
 * @param buf The buffer to store the received data in.
 * @param buflen The maximum number of bytes to receive.
 * @param tmp a pointer to a temporary buffer that can be used by the receive callback.
 *
 * @return The number of bytes received.
 */
int wrapReceive(unsigned char *buf, uint32_t buflen, void *tmp)
{
    int bytes_received = (*receiveCb)(buf, buflen, tmp);
    if (bytes_received < 0)
    { // if receive error
        connectionError(bytes_received);
        bytes_received = 0;
    }
    else if (bytes_received > 0)
    {
        millis_last_sent_received_time = (*callbacks.millis)();
    }
    return bytes_received;
}

/**
 * Default DTLS restore session, return -1 which means that the DTLS session will not be resumed
 *
 * @param buffer The buffer containing the session data.
 * @param length The length of the buffer.
 * @param type The type of the session.
 * @param reserved Reserved for future use.
 *
 * @return -1
 */
int default_restore_session(void *buffer, size_t length, uint8_t type, void *reserved)
{
    LOG(TRACE, "DTLS session resume request");
    return -1;
}

/**
 * Default DTLS save session, return -1 which means that the DTLS session will not be saved
 *
 * @param buffer The buffer containing the session data.
 * @param length The length of the buffer.
 * @param type The type of the session.
 * @param reserved This is a pointer to the session structure.
 *
 * @return -1
 */
int default_save_session(const void *buffer, size_t length, uint8_t type, void *reserved)
{
    LOG(TRACE, "DTLS session save request");
    return -1;
}

// note: it should be RAND_MAX >= 0x7FFF to be standard-compliant
#define BITS_IN_RAND (                     \
    RAND_MAX >= 0xFFFFFFFFFFFFFFFFu   ? 64 \
    : RAND_MAX >= 0x00000000FFFFFFFFu ? 32 \
    : RAND_MAX >= 0x0000000000FFFFFFu ? 24 \
    : RAND_MAX >= 0x000000000000FFFFu ? 16 \
    : RAND_MAX >= 0x0000000000000FFFu ? 12 \
                                      : 8)
uint32_t default_random_callback()
{
#if BITS_IN_RAND >= 32
    return (uint32_t)rand();
#else
    int nb = 0;
    int v = 0;
    while (nb < 32)
    {
        v = (v << BITS_IN_RAND) | (rand() & ((1ull << BITS_IN_RAND) - 1));
        nb += BITS_IN_RAND;
    }
    return v;
#endif
}

void Trackle::setReceiveCallback(receiveCallback *receive)
{
    receiveCb = receive;
    callbacks.receive = wrapReceive;
}

bool Trackle::connected()
{
    return (connectionStatus == SOCKET_READY ? true : false);
}

Connection_Status_Type Trackle::getConnectionStatus()
{
    return connectionStatus;
}

void Trackle::setConnectCallback(connectCallback *connect)
{
    connectCb = connect;
}

void Trackle::setDisconnectCallback(disconnectCallback *disconnect)
{
    disconnectCb = disconnect;
}

void Trackle::setCompletedPublishCallback(publishCompletionCallback *publish)
{
    completedPublishCb = publish;
}

void Trackle::setSendPublishCallback(publishSendCallback *publish)
{
    sendPublishCb = publish;
}

void Trackle::setPrepareForFirmwareUpdateCallback(prepareFirmwareUpdateCallback *prepare)
{
    prepareFirmwareCb = prepare;
}

void Trackle::setSaveFirmwareChunkCallback(firmwareChunkCallback *chunk)
{
    firmwareChunkCb = chunk;
}

void Trackle::setFinishFirmwareUpdateCallback(finishFirmwareUpdateCallback *finish)
{
    finishUpdateCb = finish;
}

void Trackle::setFirmwareUrlUpdateCallback(firmwareUrlUpdateCallback *firmwareUrl)
{
    firmwareUrlCb = firmwareUrl;
}

void Trackle::setPincodeCallback(pincodeCallback *pincode)
{
    pincodeCb = pincode;
}

void Trackle::setSleepCallback(sleepCallback *sleep)
{
    callbacks.sleep = sleep;
}

void Trackle::setConnectionStatusCallback(connectionStatusCallback *connectionStatus)
{
    connectionStatusCb = connectionStatus;
}

void Trackle::setUpdateStateCallback(updateStateCallback *updateState)
{
    updateStateCb = updateState;
}

void Trackle::setClaimCode(const char *claimCode)
{
    memset(claim_code, 0, CLAIM_CODE_SIZE);
    memcpy(claim_code, claimCode, CLAIM_CODE_SIZE);
    claim_code[CLAIM_CODE_SIZE] = 0;
}

void Trackle::setSaveSessionCallback(saveSessionCallback *save)
{
    callbacks.save = save;
}

void Trackle::setRestoreSessionCallback(restoreSessionCallback *restore)
{
    callbacks.restore = restore;
}

void Trackle::setSignalCallback(signalCallback *signal)
{
    callbacks.signal = signal;
}

void Trackle::setSystemTimeCallback(timeCallback *time)
{
    callbacks.set_time = time;
}

void Trackle::setRandomCallback(randomNumberCallback *random)
{
    getRandomCb = random;
}

void Trackle::setSystemRebootCallback(rebootCallback *reboot)
{
    systemRebootCb = reboot;
}

void Trackle::setLogCallback(logCallback *log)
{
    log_set_callbacks((log_message_callback_type)log, NULL, NULL, NULL);
}

void Trackle::setLogLevel(Log_Level level)
{
    log_set_level((LoggerOutputLevel)level);
}

const char *Trackle::getLogLevelName(int level)
{
    return log_level_name(level, NULL);
}

void Trackle::setConnectionType(Connection_Type conn)
{
    connectionType = conn;
}

void Trackle::setPingInterval(uint32_t interval)
{
    pingInterval = interval;
}

void Trackle::setOtaMethod(Ota_Method method)
{
    otaMethod = method;
}

void Trackle::disableUpdates()
{
    if (connected())
        publish("trackle/device/updates/enabled", "false", PRIVATE);
    updates_enabled = false;
}

void Trackle::enableUpdates()
{
    if (connected())
        publish("trackle/device/updates/enabled", "true", PRIVATE);
    updates_enabled = true;
}

bool Trackle::updatesEnabled()
{
    return updates_enabled;
}

bool Trackle::updatesPending()
{
    return updates_pending;
}

bool Trackle::updatesForced()
{
    return updates_forced;
}

void Trackle::setPublishHealthCheckInterval(uint32_t interval)
{
    health_check_interval = interval;
}

void Trackle::publishHealthCheck()
{
    trackle_protocol_post_description(protocol, trackle::protocol::DESCRIBE_METRICS);
}

void Trackle::connectionCompleted()
{
    LOG(INFO, "Socket connection completed, starting handshake");
    setConnectionStatus(SOCKET_CONNECTED);
    millis_last_sent_health_check = (*callbacks.millis)(); // reset health check timer on connect

    int result = trackle_protocol_handshake(protocol);

    bool session_resumed = false;

    if (result == trackle::protocol::SESSION_RESUMED)
    {
        LOG(TRACE, "Session resumed");
        session_resumed = true;
    }
    else if (result != 0)
    {
        LOG(ERROR, "Protocol beginning error: %d", result);
        connectionError(2, true);
        return;
    }

    if (!session_resumed)
    {
        LOG(INFO, "Protocol begun successfully");
        uint32_t flags = PRIVATE | EMPTY_FLAGS;
        flags = convert(flags);

        if (claim_code[0] != 0 && (uint8_t)claim_code[0] != 0xff)
        {
            trackle_protocol_send_event(protocol, "trackle/device/claim/code", claim_code, DEFAULT_TTL, flags, NULL);
            LOG(TRACE, "Send trackle/device/claim/code event for code %s", claim_code);
        }
        trackle_protocol_send_event(protocol, "trackle/device/updates/forced", (updates_forced ? "true" : "false"), DEFAULT_TTL, flags, NULL);
        trackle_protocol_send_event(protocol, "trackle/device/updates/enabled", (updates_enabled ? "true" : "false"), DEFAULT_TTL, flags, NULL);
        LOG(TRACE, "Send devices update status");

        trackle_protocol_send_subscriptions(protocol);
        LOG(TRACE, "Send device subscriptions sent");
        trackle_protocol_send_time_request(protocol);
        LOG(TRACE, "Time request sent");
    }
    else
    {
        LOG(INFO, "Cloud connected from existing session.");
    }

    setConnectionStatus(SOCKET_READY);
}

int Trackle::connect()
{

    if (!cloudEnabled)
        return 0;

    if (connected())
        return 0;

    connectToCloud = true;
    millis_last_disconnection = (*callbacks.millis)();

    if (!trackle_protocol_is_initialized(protocol))
    {
        keys.size = sizeof(keys);
        keys.server_public = server_public_key;
        keys.core_private = client_private_key;
        LOG(TRACE, "Initializing protocol...");

        // update connectionPropType value
        connectionPropType.ack_timeout = connectionPropTypeList[connectionType].ack_timeout;
        connectionPropType.handshake_timeout = connectionPropTypeList[connectionType].handshake_timeout;

        if (pingInterval > 0) // ping interval overrided
        {
            connectionPropType.ping_interval = pingInterval;
        }
        else
        {
            connectionPropType.ping_interval = connectionPropTypeList[connectionType].ping_interval;
        }

        trackle_protocol_init(protocol, (const char *)device_id, keys, callbacks, descriptor, connectionPropType);

        void *t = this;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
        trackle_protocol_add_event_handler(protocol, "trackle", (EventHandler)subscribe_trackle_handler, SubscriptionScope::MY_DEVICES, NULL, t);
#pragma GCC diagnostic pop
    }

    if (trackle_protocol_is_initialized(protocol))
    {
        LOG(TRACE, "Protocol already initialized");
        setConnectionStatus(SOCKET_CONNECTING);
        int res = -1;
        string address = "device.trackle.io";
        address = string_device_id + ".udp." + address;
        res = (*connectCb)(address.c_str(), 5684);

        // when the connection is ok, connectCb must call connectionCompleted() and return >= 0
        // If it returns < 0, it's an immediate error
        if (res < 0)
        {
            connectionError(res, true);
            return -1;
        }
    }
    else
    {
        LOG(ERROR, "Protocol not initialized correctly");
        setConnectionStatus(SOCKET_NOT_CONNECTED);
        return -1;
    }

    return 0;
}

void Trackle::disconnect()
{
    connectToCloud = false;
    setConnectionStatus(SOCKET_NOT_CONNECTED);
    (*disconnectCb)();
}

void Trackle::loop()
{

    // updating uptime
    system_tick_t millis_since_check_diagnostic = (*callbacks.millis)() - millis_last_check_diagnostic;
    if (DIAGNOSTIC_TIMEOUT < millis_since_check_diagnostic)
    {
        millis_diagnostic = ((*callbacks.millis)() - millis_started_at) / 1000;
        diagnosticSystem(UPTIME, millis_diagnostic);
        if (usedMemoryCb)
            diagnosticSystem(MEMORY_USED, usedMemoryCb()); // used memory
        millis_last_check_diagnostic = (*callbacks.millis)();
    }

    // ignore if not enabled
    if (!cloudEnabled)
        return;

    // ready or disconnected
    if (connectionStatus == SOCKET_READY || connectionStatus == SOCKET_NOT_CONNECTED)
    {
        int res = trackle_protocol_event_loop(protocol);
        if (!res)
            connectionError(3);
        if (!res && cloudStatus != res)
        {
            LOG(ERROR, "Event loop error");
        }
        cloudStatus = res;
    }

    // ready - check publish diagnostic
    if (connectionStatus == SOCKET_READY && health_check_interval > 0)
    {
        system_tick_t millis_since_last_health_check = (*callbacks.millis)() - millis_last_sent_health_check;
        if (health_check_interval < millis_since_last_health_check)
        {
            millis_last_sent_health_check = (*callbacks.millis)();
            LOG(TRACE, "Sending health check");
            trackle_protocol_post_description(protocol, trackle::protocol::DESCRIBE_METRICS);
        }
    }

    // reconnect
    if (connectionStatus < SOCKET_READY && connectToCloud == true)
    {
        system_tick_t millis_since_disconnection = (*callbacks.millis)() - millis_last_disconnection;

        if (RECONNECTION_TIMEOUT < millis_since_disconnection)
        {
            LOG(INFO, "Cloud reconnection after %lu ms", millis_since_disconnection);

            if (connectionStatus > SOCKET_NOT_CONNECTED)
                connectionError(4, true);

            millis_last_disconnection = (*callbacks.millis)();
            Trackle::connect();
        }
    }
}

// SETTER
void Trackle::setFirmwareVersion(int firmwareversion)
{
    trackle_protocol_set_product_firmware_version(protocol, firmwareversion);
}

void Trackle::setProductId(int productid)
{
    trackle_protocol_set_product_id(protocol, productid);
}

void Trackle::setDeviceId(const uint8_t deviceid[DEVICE_ID_LENGTH])
{
    // clear all bytes (including the termination one)
    memset(device_id, 0x00, sizeof(device_id));
    if (deviceid)
    { // else (if NULL), just leave the all-0 bytes
        memcpy(device_id, deviceid, DEVICE_ID_LENGTH);
    }
    string_device_id = hexStr(device_id, DEVICE_ID_LENGTH);
    LOG(INFO, "device_id %s", string_device_id.c_str());
}

void Trackle::setKeys(const uint8_t client[PRIVATE_KEY_LENGTH])
{
    if (client)
    {
        memcpy(client_private_key, client, PRIVATE_KEY_LENGTH);
    }
}

/**
 * @param flags 1 dry run only.
 * Return 0 on success.
 */
char *file_content;
uint64_t file_index = 0; // uint32_t is enough for correct use; use uint64_t for easier non-overflowing calculations

// TODO updatesPending, enableupdate,

/**
 * It's called to tell the application that a firmware update is about to start
 *
 * @param descriptor a structure containing the following fields:
 * @param flags
 * @param reserved Reserved for future use.
 *
 * @return The return value is the result of the operation.
 */
int default_prepare_for_firmware_update(FileTransfer::Descriptor &descriptor, uint32_t flags, void *reserved)
{
    if (!updates_enabled && !updates_forced)
    {
        LOG(WARN, "Ota upgrade refused: enabled %d, forced: %d", updates_enabled, updates_forced);
        return -1;
    }

    if (prepareFirmwareCb)
    {
        Chunk new_chunk;
        new_chunk.chunk_size = descriptor.chunk_size;
        new_chunk.chunk_count = descriptor.chunk_count(descriptor.chunk_size);
        new_chunk.chunk_address = descriptor.chunk_address;
        new_chunk.file_length = descriptor.file_length;

        (*prepareFirmwareCb)(new_chunk, flags, reserved);
    }
    else
    {
        LOG(TRACE, "prepare_for_firmware_update length: %d", descriptor.file_length);
        file_content = new char[descriptor.file_length];
        file_index = 0;
    }
    return 0;
}

/**
 * It's a callback function that is called by the firmware update library to save the firmware chunk
 *
 * @param descriptor a structure containing the following fields:
 * @param chunk the chunk of data to be saved
 * @param reserved This is a pointer to a structure that is passed to the callback function.
 *
 * @return The return value is the number of bytes written to the file.
 */
int default_save_firmware_chunk(FileTransfer::Descriptor &descriptor, const unsigned char *chunk, void *reserved)
{
    LOG(TRACE, "save_firmware_chunk");

    if (firmwareChunkCb)
    {
        Chunk new_chunk;
        new_chunk.chunk_size = descriptor.chunk_size;
        new_chunk.chunk_count = descriptor.chunk_count(descriptor.chunk_size);
        new_chunk.chunk_address = descriptor.chunk_address;
        new_chunk.file_length = descriptor.file_length;

        (*firmwareChunkCb)(new_chunk, chunk, reserved);
    }
    else
    {
        for (int i = 0; i < descriptor.chunk_size; i++)
        {
            if (file_index + i < descriptor.file_length)
            {
                file_content[file_index + i] = chunk[i];
            }
        }
        file_index += descriptor.chunk_size;
    }

    return 0;
}

/**
 * It's called when the firmware update is complete
 *
 * @param data The file descriptor.
 * @param flags 0x1 - indicates that the file transfer is complete
 *
 * @return The return value is the number of bytes written to the file.
 */
int default_finish_firmware_update(FileTransfer::Descriptor &data, uint32_t flags, void *)
{

    LOG(TRACE, "finish_firmware_update OK");
    if (finishUpdateCb)
    {
        (*finishUpdateCb)(file_content, data.file_length);
        // delete[] file_content;
        return 0;
    }

    return -1;
}

/**
 * This is the default signal function.
 * When the signaling starts or stops, print a message to the log.
 *
 * @param on true if the signaling is starting, false if it's stopping
 * @param param Not used.
 * @param reserved Reserved for future use.
 */
void default_signal_cb(bool on, unsigned int param, void *reserved)
{
    LOG(INFO, "Signaling: %s", (on ? "START" : "STOP"));
}

/**
 * This is the default time function.
 * When received, print server timestamp to the log.
 *
 * @param time The time in seconds since the epoch.
 * @param param Not used.
 */
void default_system_set_time_cb(time_t time, unsigned int param, void *)
{
    LOG(TRACE, "Server time is %lld", (long long)time);
}

Trackle::Trackle(void)
{
    // CONFIGURO IL CLOUD
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.size = sizeof(callbacks);
    callbacks.calculate_crc = calculateCrc;
    callbacks.protocolFactory = PROTOCOL_DTLS;
    callbacks.transport_context = nullptr;

    callbacks.prepare_for_firmware_update = default_prepare_for_firmware_update;
    callbacks.save_firmware_chunk = default_save_firmware_chunk;
    callbacks.finish_firmware_update = default_finish_firmware_update;
    callbacks.set_time = default_system_set_time_cb;
    callbacks.signal = default_signal_cb;
    callbacks.send = wrapSend;
    callbacks.save = default_save_session;
    callbacks.restore = default_restore_session;
    callbacks.sleep = NULL;

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.size = sizeof(descriptor);
    descriptor.ota_upgrade_status_sent = HAL_OTA_Flashed_ResetStatus;
    descriptor.was_ota_upgrade_successful = was_ota_upgrade_successful;
    descriptor.num_functions = num_functions;
    descriptor.get_function_key = getUserFunctionKey;
    descriptor.call_function = call_function;
    descriptor.update_state = update_state;
    descriptor.num_variables = numUserVariables;
    descriptor.get_variable_key = getUserVariableKey;
    descriptor.variable_calculated = isVarCalculated;
    descriptor.variable_type = wrapVarTypeInEnum;
    descriptor.get_variable = getUserVar;
    descriptor.append_system_info = appendSystemInfo;
    descriptor.append_metrics = appendMetrics;

#ifdef PRODUCT_ID
    trackle_protocol_set_product_id(protocol, PRODUCT_ID);
#endif
#ifdef PRODUCT_FIRMWARE_VERSION
    trackle_protocol_set_product_firmware_version(protocol, PRODUCT_FIRMWARE_VERSION);
#endif
}

Trackle::~Trackle()
{
    delete file_content;
    file_content = NULL;
}

// DIAGNOSTICA
void Trackle::setUsedMemoryCallback(usedMemoryCallback *usedmemory)
{
    usedMemoryCb = usedmemory;
}

/**
 * It converts a floating point number to a fixed point number
 *
 * @param value the value to be converted
 * @param shift_bytes the number of bytes to shift the integer part of the float value.
 *
 * @return a 32-bit integer.
 */
int32_t trans_float_diag(double value, int shift_bytes)
{
    double fractpart, intpart;
    fractpart = modf(value, &intpart);
    int32_t result = 0;
    if (shift_bytes == 1)
    {
        result = ((int32_t)(intpart) << 8) | ((int32_t)(fractpart * 256) & 0xff);
    }
    else if (shift_bytes == 2)
    {
        result = ((int32_t)(intpart) << 16) | ((int32_t)(fractpart * 256 * 256) & 0xffff);
    }
    return result;
}

void Trackle::diagnosticPower(Power key, double value)
{
    int32_t right_value = (int32_t)value;
    if (key == BATTERY_CHARGE)
    {
        right_value = trans_float_diag(value, 1);
    }
    addDiagnostic(key, right_value);
}

void Trackle::diagnosticSystem(System key, double value)
{
    addDiagnostic(key, (int32_t)value);
}

void Trackle::diagnosticNetwork(Network key, double value)
{
    int32_t right_value = (int32_t)value;
    if (key == CELLULAR_MOBILE_NETWORK_CODE)
    {
        if (right_value < 100)
            right_value = right_value * -1; // fix 2 digit format mnc
    }
    else if (key == SIGNAL_RSSI || key == SIGNAL_STRENGTH || key == SIGNAL_QUALITY)
    {
        right_value = trans_float_diag(value, 1);
    }
    else if (key == SIGNAL_STRENGTH_V || key == SIGNAL_QUALITY_V)
    {
        right_value = trans_float_diag(value, 2);
    }
    addDiagnostic(key, right_value);
}

void Trackle::addDiagnostic(int key, int32_t value)
{
    diagnostic[key] = value;
}

uint32_t HAL_RNG_GetRandomNumber(void)
{
    return getRandomCb ? (*getRandomCb)() : default_random_callback();
}
