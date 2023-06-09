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

#ifndef trackle_interface_h
#define trackle_interface_h

#include "defines.h"

#define DYNLIB __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct Trackle Trackle;
    typedef struct Diagnostic Diagnostic;

    Trackle *newTrackle(void) DYNLIB;
    void deleteTrackle(Trackle *v) DYNLIB;

    void trackleInit(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::setEnabled()
     * @trackle
     * @copydetails Trackle::setEnabled()
     */
    void trackleSetEnabled(Trackle *v, bool status) DYNLIB;

    /*!
     * @copybrief Trackle::isEnabled()
     * @trackle
     * @copydetails Trackle::isEnabled()
     */
    bool trackleIsEnabled(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::get()
     * @trackle
     * @copydetails Trackle::get()
     */
    bool trackleGet(Trackle *v, const char *varKey, void *(*fn)(const char *), Data_TypeDef type) DYNLIB;

    /*!
     * @copybrief Trackle::post()
     * @trackle
     * @copydetails Trackle::post()
     */
    bool tracklePost(Trackle *v, const char *funcKey, user_function_int_char_t *func, Function_PermissionDef permission) DYNLIB;

    /*!
     * @copybrief Trackle::publish()
     * @trackle
     * @copydetails Trackle::publish()
     */
    bool tracklePublish(Trackle *v, const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag) DYNLIB;

    /*!
     * @copybrief Trackle::syncState()
     * @trackle
     * @copydetails Trackle::syncState()
     */
    bool trackleSyncState(Trackle *v, const char *data) DYNLIB;

    /*!
     * @copybrief Trackle::getTime()
     * @trackle
     * @copydetails Trackle::getTime()
     */
    bool trackleGetTime(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::subscribe()
     * @trackle
     * @copydetails Trackle::subscribe()
     */
    bool trackleSubscribe(Trackle *v, const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceID) DYNLIB;

    /*!
     * @copybrief Trackle::unsubscribe()
     * @trackle
     * @copydetails Trackle::unsubscribe()
     */
    void trackleUnsubscribe(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::setMillis()
     * @trackle
     * @copydetails Trackle::setMillis()
     */
    void trackleSetMillis(Trackle *v, millisCallback *millis) DYNLIB;

    /*!
     * @copybrief Trackle::setDeviceId()
     * @trackle
     * @copydetails Trackle::setDeviceId()
     */
    void trackleSetDeviceId(Trackle *v, const uint8_t deviceid[DEVICE_ID_LENGTH]) DYNLIB;

    /*!
     * @copybrief Trackle::setKeys()
     * @trackle
     * @copydetails Trackle::setKeys()
     */
    void trackleSetKeys(Trackle *v, const uint8_t client[PRIVATE_KEY_LENGTH]) DYNLIB;

    /*!
     * @copybrief Trackle::setSendCallback()
     * @trackle
     * @copydetails Trackle::setSendCallback()
     */
    void trackleSetSendCallback(Trackle *v, sendCallback *send) DYNLIB;

    /*!
     * @copybrief Trackle::setReceiveCallback()
     * @trackle
     * @copydetails Trackle::setReceiveCallback()
     */
    void trackleSetReceiveCallback(Trackle *v, receiveCallback *receive) DYNLIB;

    /*!
     * @copybrief Trackle::setConnectCallback()
     * @trackle
     * @copydetails Trackle::setConnectCallback()
     */
    void trackleSetConnectCallback(Trackle *v, connectCallback *connect) DYNLIB;

    /*!
     * @copybrief Trackle::setDisconnectCallback()
     * @trackle
     * @copydetails Trackle::setDisconnectCallback()
     */
    void trackleSetDisconnectCallback(Trackle *v, disconnectCallback *disconnect) DYNLIB;

    /*!
     * @copybrief Trackle::setCompletedPublishCallback()
     * @trackle
     * @copydetails Trackle::setCompletedPublishCallback()
     */
    void trackleSetCompletedPublishCallback(Trackle *v, publishCompletionCallback *publish) DYNLIB;

    /*!
     * @copybrief Trackle::setSendPublishCallback()
     * @trackle
     * @copydetails Trackle::setSendPublishCallback()
     */
    void trackleSetSendPublishCallback(Trackle *v, publishSendCallback *publish) DYNLIB;

    /*!
     * @copybrief Trackle::setPrepareForFirmwareUpdateCallback()
     * @trackle
     * @copydetails Trackle::setPrepareForFirmwareUpdateCallback()
     */
    void trackleSetPrepareForFirmwareUpdateCallback(Trackle *v, prepareFirmwareUpdateCallback *prepare) DYNLIB;

    /*!
     * @copybrief Trackle::setSaveFirmwareChunkCallback()
     * @trackle
     * @copydetails Trackle::setSaveFirmwareChunkCallback()
     */
    void trackleSetSaveFirmwareChunkCallback(Trackle *v, firmwareChunkCallback *chunk) DYNLIB;

    /*!
     * @copybrief Trackle::setFinishFirmwareUpdateCallback()
     * @trackle
     * @copydetails Trackle::setFinishFirmwareUpdateCallback()
     */
    void trackleSetFinishFirmwareUpdateCallback(Trackle *v, finishFirmwareUpdateCallback *finish) DYNLIB;

    /*!
     * @copybrief Trackle::setFirmwareUrlUpdateCallback()
     * @trackle
     * @copydetails Trackle::setFirmwareUrlUpdateCallback()
     */
    void trackleSetFirmwareUrlUpdateCallback(Trackle *v, firmwareUrlUpdateCallback *update) DYNLIB;

    /*!
     * @copybrief Trackle::setPincodeCallback()
     * @trackle
     * @copydetails Trackle::setPincodeCallback()
     */
    void trackleSetPincodeCallback(Trackle *v, pincodeCallback *pincode) DYNLIB;

    /*!
     * @copybrief Trackle::setSleepCallback()
     * @trackle
     * @copydetails Trackle::setSleepCallback()
     */
    void trackleSetSleepCallback(Trackle *v, sleepCallback *sleep) DYNLIB;

    /*!
     * @copybrief Trackle::setConnectionStatusCallback()
     * @trackle
     * @copydetails Trackle::setConnectionStatusCallback()
     */
    void trackleSetConnectionStatusCallback(Trackle *v, connectionStatusCallback *connectionStatus) DYNLIB;

    /*!
     * @copybrief Trackle::setUpdateStateCallback()
     * @trackle
     * @copydetails Trackle::setUpdateStateCallback()
     */
    void trackleSetUpdateStateCallback(Trackle *v, updateStateCallback *updateState) DYNLIB;

    /*!
     * @copybrief Trackle::setClaimCode()
     * @trackle
     * @copydetails Trackle::setClaimCode()
     */
    void trackleSetClaimCode(Trackle *v, const char *claimCode) DYNLIB;

    /*!
     * @copybrief Trackle::setSaveSessionCallback()
     * @trackle
     * @copydetails Trackle::setSaveSessionCallback()
     */
    void trackleSetSaveSessionCallback(Trackle *v, saveSessionCallback *save) DYNLIB;

    /*!
     * @copybrief Trackle::setRestoreSessionCallback()
     * @trackle
     * @copydetails Trackle::setRestoreSessionCallback()
     */
    void trackleSetRestoreSessionCallback(Trackle *v, restoreSessionCallback *restore) DYNLIB;

    /*!
     * @copybrief Trackle::setSignalCallback()
     * @trackle
     * @copydetails Trackle::setSignalCallback()
     */
    void trackleSetSignalCallback(Trackle *v, signalCallback *signal) DYNLIB;

    /*!
     * @copybrief Trackle::setSystemTimeCallback()
     * @trackle
     * @copydetails Trackle::setSystemTimeCallback()
     */
    void trackleSetSystemTimeCallback(Trackle *v, timeCallback *time) DYNLIB;

    /*!
     * @copybrief Trackle::setSystemRebootCallback()
     * @trackle
     * @copydetails Trackle::setSystemRebootCallback()
     */
    void trackleSetSystemRebootCallback(Trackle *v, rebootCallback *reboot) DYNLIB;

    /*!
     * @copybrief Trackle::setRandomCallback()
     * @trackle
     * @copydetails Trackle::setRandomCallback()
     */
    void trackleSetRandomCallback(Trackle *v, randomNumberCallback *random) DYNLIB;

    /*!
     * @copybrief Trackle::setLogCallback()
     * @trackle
     * @copydetails Trackle::setLogCallback()
     */
    void trackleSetLogCallback(Trackle *v, logCallback *log) DYNLIB;

    /*!
     * @copybrief Trackle::setLogLevel()
     * @trackle
     * @copydetails Trackle::setLogLevel()
     */
    void trackleSetLogLevel(Trackle *v, Log_Level level) DYNLIB;

    /*!
     * @copybrief Trackle::getLogLevelName()
     * @trackle
     * @copydetails Trackle::getLogLevelName()
     */
    const char *trackleGetLogLevelName(Trackle *v, int level) DYNLIB;

    /*!
     * @copybrief Trackle::setConnectionType()
     * @trackle
     * @copydetails Trackle::setConnectionType()
     */
    void trackleSetConnectionType(Trackle *v, Connection_Type conn) DYNLIB;

    /*!
     * @copybrief Trackle::setPingInterval()
     * @trackle
     * @copydetails Trackle::setPingInterval()
     */
    void trackleSetPingInterval(Trackle *v, uint32_t pingInterval) DYNLIB;

    /*!
     * @copybrief Trackle::setOtaMethod()
     * @trackle
     * @copydetails Trackle::setOtaMethod()
     */
    void trackleSetOtaMethod(Trackle *v, Ota_Method method) DYNLIB;

    /*!
     * @copybrief Trackle::disableUpdates()
     * @trackle
     * @copydetails Trackle::disableUpdates()
     */
    void trackleDisableUpdates(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::enableUpdates()
     * @trackle
     * @copydetails Trackle::enableUpdates()
     */
    void trackleEnableUpdates(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::updatesEnabled()
     * @trackle
     * @copydetails Trackle::updatesEnabled()
     */
    bool trackleUpdatesEnabled(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::updatesPending()
     * @trackle
     * @copydetails Trackle::updatesPending()
     */
    bool trackleUpdatesPending(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::updatesForced()
     * @trackle
     * @copydetails Trackle::updatesForced()
     */
    bool trackleUpdatesForced(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::connect()
     * @trackle
     * @copydetails Trackle::connect()
     */
    int trackleConnect(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::connectionCompleted()
     * @trackle
     * @copydetails Trackle::connectionCompleted()
     */
    void trackleConnectionCompleted(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::connected()
     * @trackle
     * @copydetails Trackle::connected()
     */
    bool trackleConnected(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::getConnectionStatus()
     * @trackle
     * @copydetails Trackle::getConnectionStatus()
     */
    Connection_Status_Type trackleGetConnectionStatus(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::disconnect()
     * @trackle
     * @copydetails Trackle::disconnect()
     */
    void trackleDisconnect(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::loop()
     * @trackle
     * @copydetails Trackle::loop()
     */
    void trackleLoop(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::setPublishHealthCheckInterval()
     * @trackle
     * @copydetails Trackle::setPublishHealthCheckInterval()
     */
    void trackleSetPublishHealthCheckInterval(Trackle *v, uint32_t interval) DYNLIB;

    /*!
     * @copybrief Trackle::publishHealthCheck()
     * @trackle
     * @copydetails Trackle::publishHealthCheck()
     */
    void tracklePublishHealthCheck(Trackle *v) DYNLIB;

    /*!
     * @copybrief Trackle::setFirmwareVersion()
     * @trackle
     * @copydetails Trackle::setFirmwareVersion()
     */
    void trackleSetFirmwareVersion(Trackle *v, int firmwareversion) DYNLIB;

    /*!
     * @copybrief Trackle::setProductId()
     * @trackle
     * @copydetails Trackle::setProductId()
     */
    void trackleSetProductId(Trackle *v, int productid) DYNLIB;

    /*!
     * @copybrief Trackle::diagnosticCloud()
     * @trackle
     * @copydetails Trackle::diagnosticCloud()
     */
    void trackleDiagnosticCloud(Trackle *v, Cloud key, double value) DYNLIB;

    /*!
     * @copybrief Trackle::diagnosticSystem()
     * @trackle
     * @copydetails Trackle::diagnosticSystem()
     */
    void trackleDiagnosticSystem(Trackle *v, System key, double value) DYNLIB;

    /*!
     * @copybrief Trackle::diagnosticNetwork()
     * @trackle
     * @copydetails Trackle::diagnosticNetwork()
     */
    void trackleDiagnosticNetwork(Trackle *v, Network key, double value) DYNLIB;

#ifdef __cplusplus
}
#endif

#endif
