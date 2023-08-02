//
//  Trackle.cpp
//
//  Created by Flavio Ferrandi on 14/09/17.
//  Copyright © 2017 Flavio Ferrandi. All rights reserved.
//

#include "trackle_interface.h"
#include "trackle.h"
#include <vector>
#include <cstdarg>

#include "hal_platform.h"

/// Set to true after \ref trackleInit
static bool initialized = false;

#define IF_NOT_INITIALIZED_WARNING()                                     \
    do                                                                   \
    {                                                                    \
        if (!initialized)                                                \
            printf("WARNING: %s called before initTrackle\n", __func__); \
    } while (0)

// TRACKLE.VARIABLE ------------------------------------------------------------

// set status of cloud
void trackleSetEnabled(Trackle *v, bool status)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setEnabled(status);
}

// get status of cloud
bool trackleIsEnabled(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->isEnabled();
}

bool trackleGet(Trackle *v, const char *varKey, void *(*fn)(const char *), Data_TypeDef type)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->get(varKey, fn, (Data_TypeDef)type);
}

/*bool trackleGetInt(Trackle* v, const char* varKey, int* var) {
    return v->get(varKey, var, VAR_INT);
}*/

// TRACKLE.FUNCTION ------------------------------------------------------------

bool tracklePost(Trackle *v, const char *funcKey, user_function_int_char_t *func, Function_PermissionDef permission)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->post(funcKey, func, permission);
}

// TRACKLE.PUBLISH

bool tracklePublish(Trackle *v, const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag, uint32_t msg_key)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->publish(eventName, data, ttl, (Event_Type)eventType, (Event_Flags)eventFlag, msg_key);
}

bool trackleSyncState(Trackle *v, const char *data)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->syncState(data);
}

bool trackleGetTime(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->getTime();
}

// TRACKLE.SUBSCRIBE
bool trackleSubscribe(Trackle *v, const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceID)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->subscribe(eventName, (EventHandler)handler, (Subscription_Scope_Type)scope, deviceID);
}

void trackleUnsubscribe(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    v->unsubscribe();
}

// TRACKLE.CALLBACK ------------------------------------------------------------

void trackleSetMillis(Trackle *v, millisCallback *millis)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setMillis(millis);
}

void trackleSetSendCallback(Trackle *v, sendCallback *send)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSendCallback(send);
}

void trackleSetReceiveCallback(Trackle *v, receiveCallback *receive)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setReceiveCallback(receive);
}

bool trackleConnected(Trackle *v)
{
    return v->connected();
}

Connection_Status_Type trackleGetConnectionStatus(Trackle *v)
{
    return v->getConnectionStatus();
}

void trackleSetConnectCallback(Trackle *v, connectCallback *connect)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setConnectCallback(connect);
}

void trackleSetDisconnectCallback(Trackle *v, disconnectCallback *disconnect)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setDisconnectCallback(disconnect);
}

void trackleSetCompletedPublishCallback(Trackle *v, publishCompletionCallback *publish)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setCompletedPublishCallback(publish);
}

void trackleSetSendPublishCallback(Trackle *v, publishSendCallback *publish)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSendPublishCallback(publish);
}

void trackleSetPrepareForFirmwareUpdateCallback(Trackle *v, prepareFirmwareUpdateCallback *prepare)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setPrepareForFirmwareUpdateCallback(prepare);
}

void trackleSetSaveFirmwareChunkCallback(Trackle *v, firmwareChunkCallback *chunk)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSaveFirmwareChunkCallback(chunk);
}

void trackleSetFinishFirmwareUpdateCallback(Trackle *v, finishFirmwareUpdateCallback *finish)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setFinishFirmwareUpdateCallback(finish);
}

void trackleSetFirmwareUrlUpdateCallback(Trackle *v, firmwareUrlUpdateCallback *firmwareUrl)
{
    v->setFirmwareUrlUpdateCallback(firmwareUrl);
}

void trackleSetPincodeCallback(Trackle *v, pincodeCallback *pincode)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setPincodeCallback(pincode);
}

void trackleSetSleepCallback(Trackle *v, sleepCallback *sleep)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSleepCallback(sleep);
}

void trackleSetConnectionStatusCallback(Trackle *v, connectionStatusCallback *connectionStatus)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setConnectionStatusCallback(connectionStatus);
}

void trackleSetUpdateStateCallback(Trackle *v, updateStateCallback *updateState)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setUpdateStateCallback(updateState);
}

void trackleSetClaimCode(Trackle *v, const char *claimCode)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setClaimCode(claimCode);
}

void trackleSetPingInterval(Trackle *v, uint32_t pingInterval)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setPingInterval(pingInterval);
}

void trackleSetSaveSessionCallback(Trackle *v, saveSessionCallback *save)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSaveSessionCallback(save);
}

void trackleSetRestoreSessionCallback(Trackle *v, restoreSessionCallback *restore)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setRestoreSessionCallback(restore);
}

void trackleSetSignalCallback(Trackle *v, signalCallback *signal)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSignalCallback(*signal);
}

void trackleSetSystemTimeCallback(Trackle *v, timeCallback *time)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSystemTimeCallback(time);
}

void trackleSetRandomCallback(Trackle *v, randomNumberCallback *random)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setRandomCallback(random);
}

void trackleSetSystemRebootCallback(Trackle *v, rebootCallback *reboot)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setSystemRebootCallback(reboot);
}

void trackleSetLogCallback(Trackle *v, logCallback *log)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setLogCallback(log);
}

void trackleSetLogLevel(Trackle *v, Log_Level level)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setLogLevel(level);
}

const char *trackleGetLogLevelName(Trackle *v, int level)
{
    return v->getLogLevelName(level);
}

void trackleSetConnectionType(Trackle *v, Connection_Type conn)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setConnectionType(conn);
}

void trackleSetOtaMethod(Trackle *v, Ota_Method method)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setOtaMethod(method);
}

void trackleDisableUpdates(Trackle *v)
{
    v->disableUpdates();
}

void trackleEnableUpdates(Trackle *v)
{
    v->enableUpdates();
}

bool trackleUpdatesEnabled(Trackle *v)
{
    return v->updatesEnabled();
}

bool trackleUpdatesPending(Trackle *v)
{
    return v->updatesPending();
}

bool trackleUpdatesForced(Trackle *v)
{
    return v->updatesForced();
}

void trackleConnectionCompleted(Trackle *v)
{
    v->connectionCompleted();
}

int trackleConnect(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    return v->connect();
}

void trackleDisconnect(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    v->disconnect();
}

void trackleLoop(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    v->loop();
}

// DIAGNOSTIC

void trackleSetPublishHealthCheckInterval(Trackle *v, uint32_t interval)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setPublishHealthCheckInterval(interval);
}

void tracklePublishHealthCheck(Trackle *v)
{
    IF_NOT_INITIALIZED_WARNING();
    v->publishHealthCheck();
}

// SETTER

void trackleSetFirmwareVersion(Trackle *v, int firmwareversion)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setFirmwareVersion(firmwareversion);
}

void trackleSetProductId(Trackle *v, int productid)
{
    IF_NOT_INITIALIZED_WARNING();
    v->setProductId(productid);
}

void trackleSetDeviceId(Trackle *v, const uint8_t deviceid[DEVICE_ID_LENGTH])
{
    IF_NOT_INITIALIZED_WARNING();
    v->setDeviceId(deviceid);
}

void trackleSetKeys(Trackle *v, const uint8_t client[PRIVATE_KEY_LENGTH])
{
    IF_NOT_INITIALIZED_WARNING();
    v->setKeys(client);
}

void trackleInit(Trackle *v)
{
    initialized = true;
}

Trackle *newTrackle(void)
{
    return new Trackle();
}

void deleteTrackle(Trackle *v)
{
    delete v;
}

// DIAGNOSTICA
void trackleDiagnosticSystem(Trackle *v, System key, double value)
{
    IF_NOT_INITIALIZED_WARNING();
    v->diagnosticSystem(key, value);
}

void trackleDiagnosticNetwork(Trackle *v, Network key, double value)
{
    IF_NOT_INITIALIZED_WARNING();
    v->diagnosticNetwork(key, value);
}

void trackleDiagnosticCloud(Trackle *v, Cloud key, double value)
{
    IF_NOT_INITIALIZED_WARNING();
    v->diagnosticCloud(key, value);
}
