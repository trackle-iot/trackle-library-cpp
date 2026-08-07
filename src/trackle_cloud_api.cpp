/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2026 IOTREADY S.r.l. All rights reserved.
 *
 * This source code is licensed under the Trackle Source-Available License
 * Agreement found in the LICENSE file in the root directory of this source tree.
 * Commercial deployment requires one paid Device License Key per device.
 */

#include "trackle.h"
#include "trackle_internal.h"
#include "protocol_defs.h"
#include "version.h"
#include <vector>

#include <algorithm>

#include "hal_platform.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "dtls_protocol.h"
#include "tinydtls.h"
#include "uECC.h"
#include "tinydtls_set_rand.h"
#include "tinydtls_set_get_millis.h"
#include "messages.h"
#include "diagnostic.h"
#include "appender.h"

using namespace trackle::protocol;

// TRACKLE.VARIABLE ------------------------------------------------------------

struct CloudVariableTypeBase
{
    char userVarKey[MAX_VARIABLE_KEY_LENGTH + 1];
    Data_TypeDef userVarType;
    Data_TypeDef stringVarType;

    // According to the following SO answer and comments, it's not safe to cast function pointers to void pointers (void*),
    // but it's safe to cast a function pointer type to another function pointer type.
    // For this reason, here we keep a reference to callback function using void *(*)(const char*).
    // It's client's responsibility to cast such pointer to the correct type according to userVarType.
    // URL to SO answer: https://stackoverflow.com/questions/36645660/why-cant-i-cast-a-function-pointer-to-void
    void *(*funct)(const char *, const char *);

    CloudVariableTypeBase(user_variable_pointer_t fn, const char *varKey, Data_TypeDef type)
    {
        strncpy(userVarKey, varKey, sizeof(userVarKey) - 1);
        userVarKey[sizeof(userVarKey) - 1] = '\0';
        userVarType = type;
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
        if (0 == strncmp(vars[i].userVarKey, varKey, MAX_VARIABLE_KEY_LENGTH))
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

bool Trackle::addGet(const char *varKey, user_variable_pointer_t fn, Data_TypeDef userVarType)
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

    if (vars.size() >= MAX_VARIABLE_COUNT)
    {
        LOG(WARN, "Maximum allowed limit of %d gets reached", MAX_VARIABLE_COUNT);
        return false;
    }

    CloudVariableTypeBase *old_item = find_var_by_key(varKey);

    if (old_item)
    {
        LOG(WARN, "Tried to add already-existing var (\"%s\" exists)", old_item->userVarKey);
        return false;
    }

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

bool Trackle::get(const char *varKey, user_variable_bool_cb_t fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, (user_variable_pointer_t)(fn), VAR_BOOLEAN);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_int32_cb_t fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, (user_variable_pointer_t)(fn), VAR_INT);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_double_cb_t fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, (user_variable_pointer_t)(fn), VAR_DOUBLE);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_char_cb_t fn)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    return addGet(varKey, (user_variable_pointer_t)(fn), VAR_CHAR);
#pragma GCC diagnostic pop
}

bool Trackle::get(const char *varKey, user_variable_pointer_t fn, Data_TypeDef type)
{
    return addGet(varKey, fn, type);
}

// TRACKLE.FUNCTION ------------------------------------------------------------

struct CloudFunctionTypeBase
{
    user_function_int_char_t pUserFunc;
    Function_PermissionDef permission;
    char userFuncKey[MAX_FUNCTION_KEY_LENGTH + 1];
    CloudFunctionTypeBase(const char *funcKey, user_function_int_char_t userFunc, Function_PermissionDef perms)
    {
        strncpy(userFuncKey, funcKey, sizeof(userFuncKey));
        userFuncKey[sizeof(userFuncKey) - 1] = '\0';
        pUserFunc = userFunc;
        permission = perms;
    };
};

std::vector<CloudFunctionTypeBase> funcs;

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
        if (0 == strncmp(funcs[i].userFuncKey, funcKey, MAX_FUNCTION_KEY_LENGTH))
        {
            return &funcs[i];
        }
    }
    return NULL;
}

bool Trackle::post(const char *funcKey, user_function_int_char_t func, Function_PermissionDef permission)
{
    if (funcs.size() >= MAX_FUNCTION_COUNT)
    {
        LOG(WARN, "Maximum allowed limit of %d posts reached", MAX_FUNCTION_COUNT);
        return false;
    }

    CloudFunctionTypeBase *old_item = find_func_by_key(funcKey);

    if (old_item)
    {
        LOG(WARN, "Tried to add already-existing function \"%s\" (\"%s\" exists)", funcKey, old_item->userFuncKey);
        return false;
    }

    CloudFunctionTypeBase item = CloudFunctionTypeBase(funcKey, func, permission);
    funcs.push_back(item);
    LOG(TRACE, "Set %s function \"%s\"", (permission == ALL_USERS ? "PUBLIC" : "OWNER ONLY"), item.userFuncKey);
    return true;
}

// TRACKLE.PUBLISH


bool Trackle::sendPublish(const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    if (!cloudEnabled)
    {
        LOG(WARN, "NOT PUBLISHED: cloud disabled");
        return false;
    }

    // reject system events
    if (is_system(eventName))
    {
        LOG(WARN, "NOT PUBLISHED: can't publish system event");
        return false;
    }

    uint32_t flags = eventType | eventFlag;
    flags = convert_publish_flags(flags);

    bool res = false; // global return

    // publish() without payload passes a NULL data, which must never reach strnlen()
    const size_t dataLength = (data != NULL) ? strnlen(data, MAX_BLOCK_SIZE * trackle::protocol::trackle_get_blocks_number() + 1) : 0;

    // if packet size is ok, else return false
    if (dataLength <= MAX_BLOCK_SIZE * trackle::protocol::trackle_get_blocks_number())
    {
        if (eventFlag & WITH_ACK) // if WITH_ACK flag is set
        {

            // calculate msg_key if argument = 0
            if (msg_key == 0)
            {
                msg_key = getNextPublishCounter();
            }

            // if not connected, call sendPublishCb with error and return
            if (connectionStatus != SOCKET_READY)
            {
                LOG(TRACE, "sendPublishCb ERROR");
                if (sendPublishCb)
                    (*sendPublishCb)(eventName, data, msg_key, false);

                LOG(WARN, "NOT PUBLISHED: not connected to cloud");
                return false;
            }

            // if connected continue, create packet block
            trackle_protocol_send_event_data d = {};
            trackle::protocol::block_messages_data *block = trackle::protocol::trackle_get_free_block();
            uint16_t currBlockLength = MAX_BLOCK_SIZE;

            if (block == NULL)
            { // no free block
                LOG(WARN, "NOT PUBLISHED: no free message block");
                return false;
            }

            // Block-wise, more than one packet
            if (dataLength > MAX_BLOCK_SIZE)
            {
                // copy from 2nd block, send 1st block with trackle_protocol_send_event
                memcpy(block->buffer, data + MAX_BLOCK_SIZE, dataLength - MAX_BLOCK_SIZE);
                block->totBytesNumber = dataLength - MAX_BLOCK_SIZE;
                block->totBlockNumber = ceil((double)dataLength / MAX_BLOCK_SIZE);
            }
            else // single packet
            {
                block->totBytesNumber = 0;
                block->totBlockNumber = 1;
                currBlockLength = dataLength;
            }

            block->currBlockIndex = 0;
            block->eventName = std::string(eventName);
            block->token = getNextToken();
            block->msg_key = msg_key;
            block->transmissionRunning = true;
            block->ttl = ttl;
            block->flags = flags;
            block->completionCb = completedPublishCb;

            d.handler_callback = trackle::protocol::genericBlockCompletionCallback;
            d.handler_data = (void *)msg_key;
            d.handler_token = block->token;

            // publish send ok
            LOG(TRACE, "sendPublishCb OK");
            if (sendPublishCb)
                (*sendPublishCb)(eventName, data, msg_key, true);

            LOG(TRACE, "sendPublish %s: %s ", eventName, (data != NULL) ? data : "");

            res = trackle_protocol_send_event(protocol, block->token, block->eventName.c_str(), data, currBlockLength, ttl, block->currBlockIndex, block->totBlockNumber, flags, &d);
            if (!res)
            {
                // If send fails, free the block so it can be reused
                LOG(WARN, "trackle_protocol_send_event failed, freeing block for token 0x%02x", block->token);
                block->transmissionRunning = false;
                block->lastBlockSentTime = 0;
            }
        }
        else // without ACK
        {
            // if not connected return
            if (connectionStatus != SOCKET_READY)
            {
                LOG(WARN, "NOT PUBLISHED: not connected to cloud");
                return false;
            }

            uint16_t totBytesNumber = dataLength;
            uint16_t totBlockNumber = ceil((double)dataLength / MAX_BLOCK_SIZE);
            uint16_t currBlockLength = 0;
            uint8_t token = getNextToken();
            bool res = false;

            if (totBlockNumber == 0)
                totBlockNumber = 1; // an event without payload is still one message to send

            for (int i = 0; i < totBlockNumber; i++)
            {
                currBlockLength = std::min(MAX_BLOCK_SIZE, totBytesNumber - i * MAX_BLOCK_SIZE);
                res = trackle_protocol_send_event(protocol, token, eventName, data + i * MAX_BLOCK_SIZE, currBlockLength, ttl, i, totBlockNumber, flags, NULL);
                if (!res)
                    return false;
            }

            return true;
        }
    }
    else
    {
        LOG(WARN, "NOT PUBLISHED: packet size too big");
    }

    return res;
}

bool Trackle::publish(const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    return sendPublish(eventName, data, ttl, eventType, eventFlag, msg_key);
}

bool Trackle::publish(string eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    return sendPublish(eventName.c_str(), data, ttl, eventType, eventFlag, msg_key);
}

bool Trackle::publish(const char *eventName, const char *data, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    return sendPublish(eventName, data, DEFAULT_TTL, eventType, eventFlag, msg_key);
}

bool Trackle::publish(string eventName, const char *data, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    return sendPublish(eventName.c_str(), data, DEFAULT_TTL, eventType, eventFlag, msg_key);
}

bool Trackle::publish(const char *eventName)
{
    return sendPublish(eventName, NULL, DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, 0);
}

bool Trackle::publish(string eventName)
{
    return sendPublish(eventName.c_str(), NULL, DEFAULT_TTL, PUBLIC, EMPTY_FLAGS, 0);
}

bool Trackle::syncState(const char *data)
{
    return sendPublish("trackle/p", data, DEFAULT_TTL, PUBLIC, WITH_ACK, 0);
}

bool Trackle::syncState(string data)
{
    return sendPublish("trackle/p", data.c_str(), DEFAULT_TTL, PUBLIC, WITH_ACK, 0);
}

bool Trackle::getTime()
{
    return trackle_protocol_send_time_request(protocol);
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

    LOG(TRACE, "register_event %d", success);
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
    if (!item)
    {
        LOG(WARN, "wrapVarTypeInEnum: unknown variable \"%s\"", varKey ? varKey : "(null)");
        return TrackleReturnType::INT;
    }
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
    if (function_index < 0 || function_index >= (int)funcs.size())
    {
        LOG(WARN, "getUserFunctionKey: index %d out of range (size %d)",
            function_index, (int)funcs.size());
        return "";
    }
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
            int result = (*function->pUserFunc)(arg, user_is_owner(user_caller_id), function_key);
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
    if (variable_index < 0 || variable_index >= (int)vars.size())
    {
        LOG(WARN, "getUserVariableKey: index %d out of range (size %d)",
            variable_index, (int)vars.size());
        return "";
    }
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
    if (!item)
    {
        LOG(WARN, "getUserVar: unknown variable \"%s\"", varKey ? varKey : "(null)");
        return NULL;
    }
    return (const void *)item->funct;
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

    string json = "\"i\":" + int_to_string(connectionPropType.dumb_ping_interval) + "." + int_to_string(connectionType) + ",\"o\":" + int_to_string(otaMethod) + ",\"p\":" + int_to_string(PLATFORM_ID) + ",\"s\":\"" + int_to_string(VERSION_MAJOR) + "." + int_to_string(VERSION_MINOR) + "." + int_to_string(VERSION_PATCH) + VERSION_DEV + "\"" + components_list + describe_iccid + describe_imei;

    LOG(TRACE, "%s", json.c_str());
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
    if (!item)
    {
        LOG(WARN, "printType: unknown variable \"%s\"", varKey ? varKey : "(null)");
        return;
    }

    if (item->userVarType == VAR_BOOLEAN)
    {
        LOG(TRACE, "ACTUAL BOOL %s", item->userVarKey);
    }
    else if (item->userVarType == VAR_INT)
    {
        LOG(TRACE, "ACTUAL INT %s", item->userVarKey);
    }
    else if (item->userVarType == VAR_LONG)
    {
        LOG(TRACE, "ACTUAL LONG %s", item->userVarKey);
    }
    else if (item->userVarType == VAR_STRING)
    {
        LOG(TRACE, "ACTUAL STRING %s", item->userVarKey);
    }
    else if (item->userVarType == VAR_JSON)
    {
        LOG(TRACE, "ACTUAL JSON %s", item->userVarKey);
    }
    else if (item->userVarType == VAR_DOUBLE)
    {
        LOG(TRACE, "ACTUAL DOUBLE %s", item->userVarKey);
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
        int result = (*funcs[i].pUserFunc)(param.c_str(), true, funcs[i].userFuncKey);
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
