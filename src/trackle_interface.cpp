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

// TRACKLE.VARIABLE ------------------------------------------------------------

// set status of cloud
void trackleSetEnabled(Trackle *v, bool status)
{
    v->setEnabled(status);
}

// get status of cloud
bool trackleIsEnabled(Trackle *v)
{
    return v->isEnabled();
}

bool trackleGet(Trackle *v, const char *varKey, void *(*fn)(const char *), Data_TypeDef type)
{
    return v->get(varKey, fn, (Data_TypeDef)type);
}

/*bool trackleGetInt(Trackle* v, const char* varKey, int* var) {
    return v->get(varKey, var, VAR_INT);
}*/

// TRACKLE.FUNCTION ------------------------------------------------------------

bool tracklePost(Trackle *v, const char *funcKey, user_function_int_char_t *func, Function_PermissionDef permission)
{
    return v->post(funcKey, func, permission);
}

// TRACKLE.PUBLISH

bool tracklePublish(Trackle *v, const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag)
{
    return v->publish(eventName, data, ttl, (Event_Type)eventType, (Event_Flags)eventFlag, "");
}

bool trackleSyncState(Trackle *v, const char *data)
{
    return v->syncState(data);
}

bool trackleGetTime(Trackle *v)
{
    return v->getTime();
}

// TRACKLE.SUBSCRIBE
bool trackleSubscribe(Trackle *v, const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceID)
{
    return v->subscribe(eventName, (EventHandler)handler, (Subscription_Scope_Type)scope, deviceID);
}

void trackleUnsubscribe(Trackle *v)
{
    v->unsubscribe();
}

// TRACKLE.CALLBACK ------------------------------------------------------------

void trackleSetMillis(Trackle *v, millisCallback *millis)
{
    v->setMillis(millis);
}

void trackleSetSendCallback(Trackle *v, sendCallback *send)
{
    v->setSendCallback(send);
}

void trackleSetReceiveCallback(Trackle *v, receiveCallback *receive)
{
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
    v->setConnectCallback(connect);
}

void trackleSetDisconnectCallback(Trackle *v, disconnectCallback *disconnect)
{
    v->setDisconnectCallback(disconnect);
}

void trackleSetCompletedPublishCallback(Trackle *v, publishCompletionCallback *publish)
{
    v->setCompletedPublishCallback(publish);
}

void trackleSetSendPublishCallback(Trackle *v, publishSendCallback *publish)
{
    v->setSendPublishCallback(publish);
}

void trackleSetPrepareForFirmwareUpdateCallback(Trackle *v, prepareFirmwareUpdateCallback *prepare)
{
    v->setPrepareForFirmwareUpdateCallback(prepare);
}

void trackleSetSaveFirmwareChunkCallback(Trackle *v, firmwareChunkCallback *chunk)
{
    v->setSaveFirmwareChunkCallback(chunk);
}

void trackleSetFinishFirmwareUpdateCallback(Trackle *v, finishFirmwareUpdateCallback *finish)
{
    v->setFinishFirmwareUpdateCallback(finish);
}

void trackleSetFirmwareUrlUpdateCallback(Trackle *v, firmwareUrlUpdateCallback *firmwareUrl)
{
    v->setFirmwareUrlUpdateCallback(firmwareUrl);
}

void trackleSetPincodeCallback(Trackle *v, pincodeCallback *pincode)
{
    v->setPincodeCallback(pincode);
}

void trackleSetSleepCallback(Trackle *v, sleepCallback *sleep)
{
    v->setSleepCallback(sleep);
}

void trackleSetConnectionStatusCallback(Trackle *v, connectionStatusCallback *connectionStatus)
{
    v->setConnectionStatusCallback(connectionStatus);
}

void trackleSetUpdateStateCallback(Trackle *v, updateStateCallback *updateState)
{
    v->setUpdateStateCallback(updateState);
}

void trackleSetClaimCode(Trackle *v, const char *claimCode)
{
    v->setClaimCode(claimCode);
}

void trackleSetPingInterval(Trackle *v, uint32_t pingInterval)
{
    v->setPingInterval(pingInterval);
}

void trackleSetSaveSessionCallback(Trackle *v, saveSessionCallback *save)
{
    v->setSaveSessionCallback(save);
}

void trackleSetRestoreSessionCallback(Trackle *v, restoreSessionCallback *restore)
{
    v->setRestoreSessionCallback(restore);
}

void trackleSetSignalCallback(Trackle *v, signalCallback *signal)
{
    v->setSignalCallback(*signal);
}

void trackleSetSystemTimeCallback(Trackle *v, timeCallback *time)
{
    v->setSystemTimeCallback(time);
}

void trackleSetRandomCallback(Trackle *v, randomNumberCallback *random)
{
    v->setRandomCallback(random);
}

void trackleSetSystemRebootCallback(Trackle *v, rebootCallback *reboot)
{
    v->setSystemRebootCallback(reboot);
}

void trackleSetLogCallback(Trackle *v, logCallback *log)
{
    v->setLogCallback(log);
}

void trackleSetLogLevel(Trackle *v, Log_Level level)
{
    v->setLogLevel(level);
}

const char *trackleGetLogLevelName(Trackle *v, int level)
{
    return v->getLogLevelName(level);
}

void trackleSetConnectionType(Trackle *v, Connection_Type conn)
{
    v->setConnectionType(conn);
}

void trackleSetOtaMethod(Trackle *v, Ota_Method method)
{
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
    return v->connect();
}

void trackleDisconnect(Trackle *v)
{
    v->disconnect();
}

void trackleLoop(Trackle *v)
{
    v->loop();
}

// DIAGNOSTIC

void trackleSetPublishHealthCheckInterval(Trackle *v, uint32_t interval)
{
    v->setPublishHealthCheckInterval(interval);
}

void tracklePublishHealthCheck(Trackle *v)
{
    v->publishHealthCheck();
}

// SETTER

void trackleSetFirmwareVersion(Trackle *v, int firmwareversion)
{
    v->setFirmwareVersion(firmwareversion);
}

void trackleSetProductId(Trackle *v, int productid)
{
    v->setProductId(productid);
}

void trackleSetDeviceId(Trackle *v, const uint8_t deviceid[DEVICE_ID_LENGTH])
{
    v->setDeviceId(deviceid);
}

void trackleSetKeys(Trackle *v, const uint8_t client[PRIVATE_KEY_LENGTH])
{
    v->setKeys(client);
}

void trackleInit(Trackle *v)
{
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
    v->diagnosticSystem(key, value);
}

void trackleDiagnosticNetwork(Trackle *v, Network key, double value)
{
    v->diagnosticNetwork(key, value);
}

void trackleDiagnosticCloud(Trackle *v, Cloud key, double value)
{
    v->diagnosticCloud(key, value);
}
