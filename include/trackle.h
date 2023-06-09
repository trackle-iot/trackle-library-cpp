/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#ifndef Trackle_h
#define Trackle_h

#include "defines.h"
#include "diagnostic.h"
#include <string>

using namespace std;

#ifdef __cplusplus

class Trackle
{

private:
        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         * @param data the data to be sent
         * @param ttl Time to live in seconds.
         * @param eventType type of event, public or private.
         * @param eventFlag event flags, with or without ack.
         * @param msg_id the message id, if you want to use it.
         *
         * @return The return value is a boolean value.
         */
        bool sendPublish(const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, string msg_id);

        /**
         * @brief It sends a subscription request to the server
         *
         * @param eventName The name of the event you want to subscribe to.
         * @param eventScope The scope of the event. This can be one of the following:
         * @param deviceId The device ID of the device you want to subscribe to. If you want to subscribe to
         * all devices, set this to NULL.
         *
         * @return A boolean value.
         */
        bool registerEvent(const char *eventName, Subscription_Scope_Type eventScope, const char *deviceID);

        /**
         * @brief It add a subscription to the subscriptions list
         *
         * @param eventName the name of the event to subscribe to
         * @param handler a function pointer to the function that will be called when the event is triggered.
         * @param handlerData a pointer to a data structure that will be passed to the handler function when it
         * is called.
         * @param scope the scope of the subscription.
         * @param deviceId The device ID of the device you want to subscribe to. If you want to subscribe to
         * all devices, set this to NULL.
         * @param reserved reserved for future use, must be NULL
         *
         * @return A boolean value.
         */
        bool addSubscription(const char *eventName, EventHandler handler, void *handlerData, Subscription_Scope_Type scope, const char *deviceID, void *reserved);

        /**
         * @brief It adds a variable to the list of variables that can be set by the cloud
         *
         * @param varKey The name of the variable.
         * @param fn The function that will be called when the variable is requested.
         * @param userVarType The type of variable you want to add.
         *
         * @return result of operation.
         */
        bool addGet(const char *varKey, void *(*fn)(const char *), Data_TypeDef userVarType);

public:
        /**
         * @brief It sets up the callbacks and descriptor for the cloud
         */
        Trackle();

        /**
         * @brief The destructor for the class
         */
        ~Trackle();

        /**
         * @brief Sets the cloudEnabled variable to the value of the status variable
         *
         * @param status true or false
         */
        void setEnabled(bool status);

        /**
         * @brief Returns a boolean value that indicates whether the Trackle is enabled or not
         *
         * @return The cloudEnabled variable is being returned.
         */
        bool isEnabled();

        /**
         * @brief Add a getter function for a boolean variable.
         *
         * @param varKey The name of the variable you want to get.
         * @param fn The function to be called when the variable is received.
         *
         * @return The return value is the result of the addGet function.
         */
        bool get(const char *varKey, user_variable_bool_cb_t fn);

        /**
         * @brief Add a getter function for a variable of type int.
         *
         * @param varKey The name of the variable you want to get.
         * @param fn The function to be called when the variable is received.
         *
         * @return The return value is the result of the addGet function.
         */
        bool get(const char *varKey, user_variable_int_cb_t fn);

        /**
         * @brief Add a getter function for a variable of type double.
         *
         * @param varKey The name of the variable you want to get.
         * @param fn The function to be called when the variable is received.
         *
         * @return The return value is the result of the addGet function.
         */
        bool get(const char *varKey, user_variable_double_cb_t fn);

        /**
         * @brief Add a getter function for a variable of type char.
         *
         * @param varKey The name of the variable you want to get.
         * @param fn The function to be called when the variable is received.
         *
         * @return The return value is the result of the addGet function.
         */
        bool get(const char *varKey, user_variable_char_cb_t fn);

        /**
         * @brief Add a getter function for a variable of type int.
         *
         * @param varKey The name of the variable you want to get.
         * @param fn The function to be called when the variable is received.
         * @param type The type of data you want to get.
         *
         * @return The return value is the result of the addGet function.
         */
        bool get(const char *varKey, void *(*fn)(const char *), Data_TypeDef type);

        /**
         * @brief It adds a function to the list of functions that can be called by the cloud
         *
         * @param funcKey The name of the function that will be called from the cloud.
         * @param func The function to be called when the cloud function is called.
         * @param permission This is the permission level of the function. It can be either ALL_USERS or
         * OWNER_ONLY.
         *
         * @return A boolean value.
         */
        bool post(const char *funcKey, user_function_int_char_t *func, Function_PermissionDef permission = ALL_USERS);

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         * @param data the data to be sent
         * @param ttl Time to live in seconds.
         * @param eventType type of event, public or private.
         * @param eventFlag event flags, with or without ack.
         * @param msg_id the message id, if you want to use it.
         *
         * @return The return value is a boolean value.
         */
        bool publish(const char *eventName, const char *data, int ttl = DEFAULT_TTL, Event_Type eventType = PUBLIC, Event_Flags eventFlag = EMPTY_FLAGS, string msg_id = "");

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         * @param data the data to be sent
         * @param ttl Time to live in seconds.
         * @param eventType type of event, public or private.
         * @param eventFlag event flags, with or without ack.
         * @param msg_id the message id, if you want to use it.
         *
         * @return The return value is a boolean value.
         */
        bool publish(string eventName, const char *data, int ttl = DEFAULT_TTL, Event_Type eventType = PUBLIC, Event_Flags eventFlag = EMPTY_FLAGS, string msg_id = "");

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         * @param data the data to be sent
         * @param eventType type of event, public or private.
         * @param eventFlag event flags, with or without ack.
         * @param msg_id the message id, if you want to use it.
         *
         * @return The return value is a boolean value.
         */
        bool publish(const char *eventName, const char *data, Event_Type eventType, Event_Flags eventFlag = EMPTY_FLAGS, string msg_id = "");

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         * @param data the data to be sent
         * @param eventFlag event flags, with or without ack.
         * @param msg_id the message id, if you want to use it.
         *
         * @return The return value is a boolean value.
         */
        bool publish(string eventName, const char *data, Event_Type eventType, Event_Flags eventFlag = EMPTY_FLAGS, string msg_id = "");

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         *
         * @return The return value is a boolean value.
         */
        bool publish(const char *eventName);

        /**
         * @brief It sends a publish to the cloud
         *
         * @param eventName the name of the event to publish
         *
         * @return The return value is a boolean value.
         */
        bool publish(string eventName);

        /**
         * @brief It sends a publish to the cloud
         *
         * @param data the data to be sent
         *
         * @return The return value is a boolean value.
         */
        bool syncState(const char *data);

        /**
         * @brief It sends a publish to the cloud
         *
         * @param data the data to be sent
         *
         * @return The return value is a boolean value.
         */
        bool syncState(string data);

        /**
         * @brief It sends a time request to the server
         *
         * @return The return value is a boolean value.
         */
        bool getTime();

        /**
         * @brief It subscribes to an event.
         *
         * @param eventName The name of the event to subscribe to.
         * @param handler The function to be called when the event is triggered.
         *
         * @return A boolean value.
         */
        bool subscribe(const char *eventName, EventHandler handler);

        /**
         * @brief It subscribes to an event.
         *
         * @param eventName The name of the event to subscribe to.
         * @param handler The function that will be called when the event is triggered.
         * @param scope This is the scope of the subscription. It can be either of the following:
         *
         * @return A boolean value.
         */
        bool subscribe(const char *eventName, EventHandler handler, Subscription_Scope_Type scope);

        /**
         * @brief It subscribes to an event.
         *
         * @param eventName The name of the event to subscribe to.
         * @param handler The function that will be called when the event is triggered.
         * @param deviceID The device ID of the device you want to subscribe to.
         *
         * @return A boolean value.
         */
        bool subscribe(const char *eventName, EventHandler handler, const char *deviceID);

        /**
         * @brief It subscribes to an event.
         *
         * @param eventName The name of the event to subscribe to.
         * @param handler The function that will be called when the event is triggered.
         * @param scope The subscription scope, MY_DEVICES or ALL_DEVICES
         * @param deviceID The device ID of the device you want to subscribe to.
         *
         * @return A boolean value.
         */
        bool subscribe(const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceId);

        /**
         * @brief It removes the event handlers that were added in the `subscribe()` function
         */
        void unsubscribe();

        /**
         * @brief This function set millis function callback
         *
         * @param millis The function pointer.
         */
        void setMillis(millisCallback *millis);

        /**
         * @brief It sets the device ID
         *
         * @param deviceid the device ID to set. If NULL, the device ID will be set to all 0 bytes.
         */
        void setDeviceId(const uint8_t deviceId[DEVICE_ID_LENGTH]);

        /**
         * @brief It sets the client keys into the Trackle library
         *
         * @param client The client's private key.
         */
        void setKeys(const uint8_t client[PRIVATE_KEY_LENGTH]);

        /**
         * @brief It sets the send callback function to the one passed in as a parameter
         *
         * @param send A pointer to a function that will be called when a message is ready to be sent.
         */
        void setSendCallback(sendCallback *send);

        /**
         * @brief It sets the receive callback function to the one passed in as a parameter
         *
         * @param receive A function pointer to a function that takes a byte array and a length.
         */
        void setReceiveCallback(receiveCallback *receive);

        /**
         * @brief It sets the connect callback function.
         *
         * @param connect The callback function that will be called when is needed to connect to Trackle
         */
        void setConnectCallback(connectCallback *connect);

        /**
         * @brief It sets the disconnect callback function.
         *
         * @param disconnect This is a callback function that will be called to disconnect from Trackle
         */
        void setDisconnectCallback(disconnectCallback *disconnect);

        /**
         * @brief It sets the callback function for the complete publish
         *
         * @param publish The callback to be called on publish completed
         */
        void setCompletedPublishCallback(publishCompletionCallback *publish);

        /**
         * @brief It sets the callback function for the publish function.
         *
         * @param publish The callback to be called on publish
         */
        void setSendPublishCallback(publishSendCallback *publish);

        /**
         * @brief This function sets the callback function that will be called when it's ready to start a firmware update.
         *
         * @param prepare A function pointer that will be called when it's ready to start a firmware update.
         */
        void setPrepareForFirmwareUpdateCallback(prepareFirmwareUpdateCallback *prepare);

        /**
         * @brief This function sets the callback function that will be called when a firmware chunk is received.
         *
         * @param chunk A pointer to a function that takes a pointer to a buffer and the size of the buffer.
         */
        void setSaveFirmwareChunkCallback(firmwareChunkCallback *save);

        /**
         * @brief This function sets the callback function that will be called when a firmware update is completed.
         *
         * @param finish A callback function that is called when the firmware update is complete.
         */
        void setFinishFirmwareUpdateCallback(finishFirmwareUpdateCallback *finish);

        /**
         * @brief It sets the firmwareUrlCb to the firmwareUrl passed in.
         *
         * @param firmwareUrl The URL of the firmware file.
         */
        void setFirmwareUrlUpdateCallback(firmwareUrlUpdateCallback *firmwareUrl);

        /**
         * @brief It sets the pincode callback function.
         *
         * @param pincode The pincode of the Trackle device.
         */
        void setPincodeCallback(pincodeCallback *pincode);

        /**
         * @brief This function sets the sleep callback function.
         *
         * @param sleep The sleep callback is called when need to sleep.
         * @deprecated No need to call it anymore, will be removed in release 3.0
         */
        void setSleepCallback(sleepCallback *sleep);

        /**
         * @brief It sets the connectionStatusCb to the connectionStatusCallback function.
         *
         * @param connectionStatus This is a callback function that will be called when the connection status
         * changes.
         */
        void setConnectionStatusCallback(connectionStatusCallback *connectionStatus);

        /**
         * @brief It sets the updateStateCb to the updateStateCb.
         *
         * @param updateStateCb This is a callback function that will be called when the Trackle device has a
         * state to update.
         */
        void setUpdateStateCallback(updateStateCallback *updateStateCb);

        /**
         * @brief It copies the claimCode parameter into the claim_code variable and send it to Trackle on connection
         *
         * @param claimCode The claim code of the device.
         */
        void setClaimCode(const char *claimCode);

        /**
         * @brief This function sets the callback function that will be called when it's needed to save the current
         * DTLS session.
         *
         * @param save A pointer to the session save function.
         */
        void setSaveSessionCallback(saveSessionCallback *save);

        /**
         * @brief This function sets the callback function that will be called when it's needed to restore a
         * DTLS session.
         *
         * @param restore A pointer to the session restore function.
         */
        void setRestoreSessionCallback(restoreSessionCallback *restore);

        /**
         * @brief This function sets the callback function for the signal event.
         *
         * @param signal The signal callback function.
         */
        void setSignalCallback(signalCallback *signal);

        /**
         * @brief This function sets the callback function for the system time.
         *
         * @param time The time callback function.
         */
        void setSystemTimeCallback(timeCallback *signal);

        /**
         * @brief This function sets the callback function that will be used to generate random numbers.
         * @param random a pointer to a function that returns a random number between 0 and 1.
         */
        void setRandomCallback(randomNumberCallback *random);

        /**
         * @brief It sets the system reboot callback function.
         *
         * @param reboot This is a function pointer to be called when the device has to be rebooted.
         */
        void setSystemRebootCallback(rebootCallback *reboot);

        /**
         * @brief It sets the system logs callback function.
         *
         * @param log A pointer to a function that takes the log as a parameter.
         */
        void setLogCallback(logCallback *log);

        /**
         * @brief It sets the log level for the Trackle library.
         *
         * @param level The log level to set.
         */
        void setLogLevel(Log_Level level);

        /**
         * @brief It returns a string representation of the log level
         *
         * @param level The log level to get the name of.
         *
         * @return The name of the log level.
         */
        const char *getLogLevelName(int level);

        /**
         * @brief This function sets the connection type for the Trackle library
         *
         * @param conn The connection type. This can be UNDEFINED, WIFI, ETHERNET, CELLULAR or NBIOT
         */
        void setConnectionType(Connection_Type conn);

        /**
         * @brief This function sets the interval at which the Trackle will send a ping to the server
         *
         * @param interval The interval in seconds between pings.
         */
        void setPingInterval(uint32_t pingInterval);

        /**
         * @brief It sets the OTA method to the method passed in.
         *
         * @param method The method to use for OTA updates. Can be PUSH or SEND_URL
         */
        void setOtaMethod(Ota_Method method);

        /**
         * @brief It sends a message to the Trackle cloud to disable updates
         */
        void disableUpdates();

        /**
         * @brief It sends a message to the Trackle cloud to enable updates
         */
        void enableUpdates();

        /**
         * @brief This function returns if firmware updates are enabled.
         *
         * @return updates_enabled
         */
        bool updatesEnabled();

        /**
         * @brief This function returns if there are firmware updates pending.
         *
         * @return updates_pending
         */
        bool updatesPending();

        /**
         * @brief This function returns true if the updates are forced, and false if they are not
         *
         * @return updates_forced
         */
        bool updatesForced();

        /**
         * @brief It initializes the protocol, sets the connection status to connecting, and calls the connectCb
         * callback
         *
         * @return The return value is the result of the connect() function.
         */
        int connect();

        /**
         * @brief To be called when the socket connection is completed, it starts the Trackle protocol handshake
         * @deprecated No need to call it anymore, will be removed in release 3.0
         *
         */
        void connectionCompleted();

        /**
         * @brief It checks if the connection is ready.
         *
         * @return true if connection with Trackle cloud is completed.
         */
        bool connected();

        /**
         * @brief This function returns the current connection status of Trackle
         *
         * @return The connection status of the Trackle.
         */
        Connection_Status_Type getConnectionStatus();

        /**
         * @brief It disconnects the device from Trackle cloud.
         */
        void disconnect();

        /**
         * @brief It checks if the device is connected to the cloud, if it is, it checks if it's time to send a health
         * check, if it's not, it checks if it's time to reconnect
         */
        void loop();

        /**
         * @brief This function sets the interval at which the device will publish a health check message
         *
         * @param interval The interval in milliseconds between publishing health checks.
         */
        void setPublishHealthCheckInterval(uint32_t interval);

        /**
         * @brief This function forces publish a health check message
         */
        void publishHealthCheck();

        /**
         * @brief This sets the firmware version in the library.
         *
         * @param firmwareversion The firmware version of the product.
         */
        void setFirmwareVersion(int firmwareversion);

        /**
         * @brief This sets the product id of Trackle.
         *
         * @param productid The product id of the device.
         */
        void setProductId(int productid);

        /**
         * @brief This takes a cloud diagnostic key and a value, and adds the key and value to the diagnostic buffer
         *
         * @param key The key of the diagnostic.
         * @param value The value of the diagnostic.
         */
        void diagnosticCloud(Cloud key, double value);

        /**
         * @brief This takes a system diagnostic key and a value, and adds the key and value to the diagnostic buffer
         *
         * @param key The system to be diagnosed.
         * @param value The value of the diagnostic.
         */
        void diagnosticSystem(System key, double value);

        /**
         * @brief This takes a network diagnostic key and a value, and adds the key and value to the diagnostic buffer
         *
         * @param key The key of the diagnostic parameter.
         * @param value The value of the parameter.
         */
        void diagnosticNetwork(Network key, double value);

        /**
         * @brief This tests all the functions and variables that have been registered with the Trackle class
         *
         * @param param
         */
        void test(string param);
};

/**
 * If the user has provided a callback function, call it. Otherwise, call the default callback
 * function
 *
 * @return A random number.
 */
uint32_t HAL_RNG_GetRandomNumber(void);

#endif

#endif /* Trackle_h */
