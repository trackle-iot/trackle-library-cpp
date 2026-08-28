/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2017 IOTREADY S.r.l. All rights reserved.
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
#include "file_transfer.h"

using namespace trackle::protocol;

// describe length:
// {"f":[...],"v":{...},"i":30.1,"o":0,"p":102,"s":"3.2.0"} // min len is 50
// TOTAL LEN = 50 + 20 * (MAX_VARIABLE_KEY_LENGTH + 3) + 20 * (MAX_FUNCTION_KEY_LENGTH + 5) + (MAX_COMPONENTS_LIST_LENGTH + 7) - 2 = 1015

uint16_t connection_retry = 0;
uint32_t connection_timeout = DEFAULT_CONNECTION_TIMEOUT;

const char *OTA_EVENT_NAME = "trackle/device/update/status";
struct _ota_data ota_data;

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
randomNumberCallback *getRandomCb = NULL;
rebootCallback *systemRebootCb = NULL;
otaUpdateCallback *otaUpdateCb = NULL;
deviceClaimedCallback *deviceClaimedCb = NULL;
pincodeCallback *pincodeCb = NULL;
connectionStatusCallback *connectionStatusCb = NULL;
updateStateCallback *updateStateCb = NULL;

uint32_t counter = 0;
uint32_t prefix = 0;
uint8_t token = 0;

uint32_t getNextPublishCounter()
{
    uint32_t p = prefix;

    if (p == 0)
    {
        prefix = (HAL_RNG_GetRandomNumber() % 199) + 1;
        p = prefix;
    }

    if (p == 0)
    {
        LOG(WARN, "Couldn't generate a proper random prefix for the publish counter; use 0");
    }
    else
    {
        LOG(TRACE, "Generated a random prefix: %" PRIu32, p);
    }

    counter++;
    if (counter >= MAX_COUNTER)
    {
        counter = 0;
    }

    uint32_t base = MAX_COUNTER + 1;
    return (p * base) + counter;
}

uint8_t getNextToken()
{
    token++;
    if (token == 0)
    {
        token = 1;
    }
    return token;
}

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

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

Connection_Status_Type connectionStatus = SOCKET_NOT_CONNECTED;
int cloudStatus = -1;

trackle::protocol::Connection_Properties_Type connectionPropTypeList[5] = {
    {30, 2, 10, 2},
    {30, 2, 10, 2},
    {30, 2, 10, 2},
    {30, 10, 10, 2},
    {150, 2, 20, 5},
};

void increase_connection_timeout()
{
    if (connection_retry < MAX_RECONNECTION_RETRY_INCREMENT)
    {
        connection_retry++;
    }
    connection_timeout = pow(2, connection_retry) * RECONNECTION_TIMEOUT;
    double x = (rand() % 512) / (double)1000;
    connection_timeout += x * connection_timeout;
}

void reset_connection_timeout()
{
    connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
    connection_retry = 0;
}

uint32_t pingInterval = 0;
uint8_t coapPingRatio = 0;
Connection_Type connectionType = CONNECTION_TYPE_UNDEFINED;
trackle::protocol::Connection_Properties_Type connectionPropType;

bool cloudEnabled = true;
bool connectToCloud = false;
system_tick_t millis_last_disconnection = 0;
system_tick_t millis_started_at = 0;

Ota_Method otaMethod = NO_OTA;
bool updates_pending = false;
bool updates_enabled = true;
bool updates_forced = false;

system_tick_t millis_last_sent_received_time = 0;
system_tick_t millis_last_sent_health_check = 0;
system_tick_t health_check_interval = 0;

string string_device_id;
char t_device_id[DEVICE_ID_LENGTH];
unsigned char server_public_key[PUBLIC_KEY_LENGTH] = {0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x2B, 0x19, 0x9D, 0xC9, 0xF2, 0xB0, 0x2D, 0xD1, 0xF1, 0x7D, 0xF0, 0x2B, 0xD1, 0xEC, 0xD1, 0x57, 0xD6, 0x74, 0x51, 0xD7, 0x9C, 0x09, 0xE1, 0x70, 0x43, 0x4A, 0x5B, 0xC2, 0x40, 0xC0, 0x49, 0x67, 0x34, 0xC8, 0xA4, 0xF8, 0xB4, 0xF7, 0xFB, 0xB4, 0xD0, 0x3F, 0xCC, 0xAF, 0x1F, 0xAA, 0x2E, 0x1D, 0x76, 0x82, 0xCF, 0x3A, 0x1A, 0x0B, 0x42, 0x38, 0x14, 0x6D, 0x54, 0x42, 0x05, 0xDC, 0x4D, 0x27};
unsigned char client_private_key[PRIVATE_KEY_LENGTH];
char claim_code[CLAIM_CODE_SIZE + 1];
char components_list[COMPONENTS_LIST_SIZE + 1];
char describe_imei[DESCRIBE_ATTR_SIZE + 1];
char describe_iccid[DESCRIBE_ATTR_SIZE + 1];

std::vector<string> owners;

// CLOUD / CALLBACKS

void Trackle::setMillis(millisCallback *millis)
{
    callbacks.millis = millis;
    log_set_millis_callback(millis);
    TrackleLib_set_latest_millis_callback_for_tinydtls(millis);
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
    if (newStatus != connectionStatus)
    {
        connectionStatus = newStatus;

        // if callback was defined, call it
        if (connectionStatusCb)
        {
            (*connectionStatusCb)(newStatus);
        }
    }
}

/**
 * If the connection is ready or if the force parameter is true, then set the connection status to not
 * connected and call the disconnect callback
 *
 * @param error_type The error type.
 * @param force If true, the connection will be closed even if it's not connected.
 */
void connectionError(int error_type, bool force, int protocol_error_code)
{

    // only if it was connected before (real disconnection)
    if (connectionStatus == SOCKET_READY)
    {
        diagnostic::diagnosticCloud(CLOUD_DISCONNECTS, 1);
        diagnostic::diagnosticCloud(CLOUD_DISCONNECTION_REASON, error_type);

        // reset connections attemps and unacked packets for new cloud session
        diagnostic::diagnosticCloud(CLOUD_CONNECTION_ATTEMPTS, 0);
        diagnostic::diagnosticCloud(CLOUD_UNACKNOWLEDGED_MESSAGES, 0);
    }

    // ProtocolError detail: also on failed handshake (not yet SOCKET_READY)
    if (protocol_error_code != 0)
    {
        diagnostic::diagnosticCloud(CLOUD_PROTOCOL_ERROR_CODE, protocol_error_code);
    }

    // if connected or trying to connect
    if (connectionStatus == SOCKET_READY || force)
    {
        millis_last_disconnection = (*callbacks.millis)();

        if (error_type != CLOUD_DISCONNECT_REASON_SOCKET)
            LOG(ERROR, "Cloud connection error %d, %lu", error_type, millis_last_disconnection);

        setConnectionStatus(SOCKET_NOT_CONNECTED);
        if (trackle_protocol_is_initialized(protocol))
            trackle_protocol_command(protocol, ProtocolCommands::DISCONNECT);
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
 * It calls the receive callback function and propagates its result to the protocol layer.
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
    if (bytes_received > 0)
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

#ifdef TRACKLE_USE_EXTERNAL_BUFFER
bool Trackle::setExternalBuffer(uint8_t *extBuffer, size_t size)
{
    return trackle::protocol::trackle_set_external_buffer(extBuffer, size);
}
#endif

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

void Trackle::setOtaUpdateCallback(otaUpdateCallback *updateCb)
{
    otaUpdateCb = updateCb;
}

void Trackle::setOtaUpdateDone(int error_code)
{
    char ota_cloud_message[256];

    if (ota_data.running)
    {
        if (error_code == NO_ERROR)
        {
            sprintf(ota_cloud_message, "success,%s", ota_data.ota_job_id);
        }
        else
        {
            sprintf(ota_cloud_message, "failed,%s,%d", ota_data.ota_job_id, error_code);
        }

        publish(OTA_EVENT_NAME, ota_cloud_message, PRIVATE);
    }
    else
    {
        LOG(ERROR, "Ota not running!");
    }

    ota_data.running = false;
}

void Trackle::setPincodeCallback(pincodeCallback *pincode)
{
    pincodeCb = pincode;
}

void Trackle::setSleepCallback(sleepCallback *sleep)
{
    LOG(WARN, "DEPRECATED setSleepCallback - no need to call it anymore");
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
    memset(claim_code, 0, sizeof(claim_code));

    if (claimCode == NULL)
    {
        LOG(WARN, "claimCode not set: NULL pointer");
        return;
    }

    const size_t len = strnlen(claimCode, CLAIM_CODE_SIZE + 1);
    if (len > (size_t)CLAIM_CODE_SIZE)
    {
        LOG(WARN, "claimCode too long (max %d char)", CLAIM_CODE_SIZE);
        return;
    }

    memcpy(claim_code, claimCode, len);
}

void Trackle::setComponentsList(const char *componentsList)
{
    if (componentsList == NULL)
    {
        LOG(WARN, "componentsList not set: NULL pointer");
        return;
    }

    if (strnlen(componentsList, MAX_COMPONENTS_LIST_LENGTH + 1) > MAX_COMPONENTS_LIST_LENGTH)
    {
        LOG(WARN, "componentsList too long (max %zu char)", MAX_COMPONENTS_LIST_LENGTH);
        return;
    }

    // A truncated attribute would break the JSON of the describe message, so it's dropped entirely.
    const int written = snprintf(components_list, sizeof(components_list), ",\"c\":\"%s\"", componentsList);
    if (written < 0 || (size_t)written >= sizeof(components_list))
    {
        LOG(WARN, "componentsList not set: describe attribute would not fit in %d char", COMPONENTS_LIST_SIZE);
        components_list[0] = 0;
    }
}

void Trackle::setImei(const char *imei)
{
    if (imei == NULL)
    {
        LOG(WARN, "imei not set: NULL pointer");
        return;
    }

    if (strnlen(imei, sizeof(describe_imei)) == sizeof(describe_imei))
    {
        LOG(WARN, "imei too long (max %d char for describe attr)", DESCRIBE_ATTR_SIZE);
        describe_imei[0] = 0;
        return;
    }

    const int written = snprintf(describe_imei, sizeof(describe_imei), ",\"imei\":\"%s\"", imei);
    if (written < 0 || (size_t)written >= sizeof(describe_imei))
    {
        LOG(WARN, "imei not set: describe attribute would not fit in %d char", DESCRIBE_ATTR_SIZE);
        describe_imei[0] = 0;
    }
}

void Trackle::setIccid(const char *iccid)
{
    if (iccid == NULL)
    {
        LOG(WARN, "iccid not set: NULL pointer");
        return;
    }

    if (strnlen(iccid, sizeof(describe_iccid)) == sizeof(describe_iccid))
    {
        LOG(WARN, "iccid too long (max %d char for describe attr)", DESCRIBE_ATTR_SIZE);
        describe_iccid[0] = 0;
        return;
    }

    const int written = snprintf(describe_iccid, sizeof(describe_iccid), ",\"iccid\":\"%s\"", iccid);
    if (written < 0 || (size_t)written >= sizeof(describe_iccid))
    {
        LOG(WARN, "iccid not set: describe attribute would not fit in %d char", DESCRIBE_ATTR_SIZE);
        describe_iccid[0] = 0;
    }
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
    TrackleLib_set_latest_log_callback_for_tinydtls(log);
}

void Trackle::setLogLevel(Log_Level level)
{
    log_set_level((LoggerOutputLevel)level);
    TrackleLib_set_latest_log_level_for_tinydtls(level);
}

const char *Trackle::getLogLevelName(int level)
{
    return log_level_name(level, NULL);
}

void Trackle::setConnectionType(Connection_Type conn)
{
    connectionType = conn;
}

void Trackle::setPingInterval(uint32_t dumbInterval, uint8_t coapRatio)
{
    if (dumbInterval > MAX_PING_INTERVAL)
    {
        LOG(ERROR, "setPingInterval failed! interval too high (max %d seconds)!", MAX_PING_INTERVAL);
    }
    else
    {
        pingInterval = dumbInterval;
        coapPingRatio = coapRatio;
    }
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

void Trackle::setDeviceClaimedCallback(deviceClaimedCallback *claimedCb)
{
    deviceClaimedCb = claimedCb;
}

bool Trackle::setOtaVerificationKey(const uint8_t *firmware_key, size_t length)
{
    if (length < PUB_KEY_OFFSET + PUB_KEY_XY_SIZE)
    {
        LOG(ERROR, "Firmware key is too short! Length: %d", length);
        return false;
    }

    // Check marker 0x04 is present
    if (firmware_key[PUB_KEY_OFFSET] != PUB_KEY_MARKER)
    {
        LOG(ERROR, "Public key EC point marker not found!");
        return false;
    }

    memcpy(ota_data.firmware_signature_key, &firmware_key[PUB_KEY_OFFSET + 1], PUB_KEY_XY_SIZE);
    ota_data.has_firmware_key = true;
    return true;
}

int Trackle::verifyOtaSignature(const uint8_t *firmware_hash, size_t length)
{
    if (!ota_data.has_firmware_key)
    {
        LOG(WARN, "Public key not exits, insecure OTA, skipping validation");
        return 1;
    }

    if (!ota_data.has_signature)
    {
        LOG(WARN, "Signature not received, skipping validation");
        return 0;
    }

    if (length != 32)
    {
        LOG(ERROR, "The hash message must be 32 bytes long!");
        return -1;
    }

    // Verify signature
    uECC_Curve curve = uECC_secp256r1();

    char firmware_signature_key_hex[PUB_KEY_XY_SIZE * 2 + 1];
    char firmware_hash_hex[32 * 2 + 1];
    char firmware_signature_hex[PUB_KEY_XY_SIZE * 2 + 1];
    for (int i = 0; i < PUB_KEY_XY_SIZE; i++)
    {
        sprintf(firmware_signature_key_hex + i * 2, "%02X", ota_data.firmware_signature_key[i]);
    }
    for (int i = 0; i < 32; i++)
    {
        sprintf(firmware_hash_hex + i * 2, "%02X", firmware_hash[i]);
    }
    for (int i = 0; i < PUB_KEY_XY_SIZE; i++)
    {
        sprintf(firmware_signature_hex + i * 2, "%02X", ota_data.firmware_signature[i]);
    }
    LOG(INFO, "Firmware signature key: %s", firmware_signature_key_hex);
    LOG(INFO, "Firmware hash: %s", firmware_hash_hex);
    LOG(INFO, "Firmware signature: %s", firmware_signature_hex);

    int res = uECC_verify(ota_data.firmware_signature_key, firmware_hash, 32, ota_data.firmware_signature, curve);
    LOG(INFO, "uECC_verify result: %d", res);

    // verification failed
    if (res <= 0)
        return -1;

    return 1;
}

void Trackle::setPublishHealthCheckInterval(uint32_t interval)
{
    health_check_interval = interval;
}

void Trackle::publishHealthCheck()
{
    LOG(TRACE, "publishing health check");
    trackle_protocol_post_description(protocol, trackle::protocol::DESCRIBE_METRICS);
}

void Trackle::connectionCompleted()
{
    LOG(WARN, "DEPRECATED connectionCompleted - no need to call it anymore");
}

/*
 * Return:
 * - negative number in case of error
 * - positive number in case of success
 * - 0 in case we need to run again. Handshake is not completed
 */
int completeCloudConnection()
{
    millis_last_sent_health_check = (*callbacks.millis)(); // reset health check timer on connect

    int result = trackle_protocol_handshake(protocol);

    /*
     * Handshake completed?
     */
    if (result == trackle::protocol::SESSION_RESUMED)
    {
        LOG(TRACE, "Session resumed");
        LOG(INFO, "Cloud connected from existing session.");
        setConnectionStatus(SOCKET_READY);

        return 1;
    }
    else if (result == trackle::protocol::SESSION_CONNECTED)
    {
        /*
         * New session created
         */
        LOG(INFO, "Protocol begun successfully");
        uint32_t flags = PRIVATE | EMPTY_FLAGS;
        flags = convert_publish_flags(flags);

        if (claim_code[0] != 0 && (uint8_t)claim_code[0] != 0xff)
        {
            trackle_protocol_send_event(protocol, 0, "trackle/device/claim/code", claim_code, strlen(claim_code), DEFAULT_TTL, 0, 1, flags, NULL);
            LOG(TRACE, "Send trackle/device/claim/code event for code %s", claim_code);
        }
        trackle_protocol_send_event(protocol, 0, "trackle/device/updates/forced", (updates_forced ? "true" : "false"), (updates_forced ? 4 : 5), DEFAULT_TTL, 0, 1, flags, NULL);
        trackle_protocol_send_event(protocol, 0, "trackle/device/updates/enabled", (updates_enabled ? "true" : "false"), (updates_enabled ? 4 : 5), DEFAULT_TTL, 0, 1, flags, NULL);
        LOG(TRACE, "Send devices update status");

        trackle_protocol_send_subscriptions(protocol);
        LOG(TRACE, "Send device subscriptions sent");
        trackle_protocol_send_time_request(protocol);
        LOG(TRACE, "Time request sent");

        setConnectionStatus(SOCKET_READY);

        return 1;
    }
    else if (result != 0) /* Handshake error? */
    {
        LOG(ERROR, "Protocol beginning error: %d", result);
        connectionError(mapProtocolErrorToDisconnectionReason(result), true, result);
        return -1;
    }
    else
    {
        /*
         * Handshake in progress...NO_ERROR
         */
    }

    /*
     * Need run again...
     */
    return 0;
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
            connectionPropType.dumb_ping_interval = pingInterval;
        }
        else
        {
            connectionPropType.dumb_ping_interval = connectionPropTypeList[connectionType].dumb_ping_interval;
        }

        if (coapPingRatio > 0) // coap ping ratio overrided
        {
            connectionPropType.coap_ping_ratio = coapPingRatio;
        }
        else
        {
            connectionPropType.coap_ping_ratio = connectionPropTypeList[connectionType].coap_ping_ratio;
        }

        trackle_protocol_init(protocol, (const char *)t_device_id, keys, callbacks, descriptor, connectionPropType);

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

        // If it returns < 0, it's an immediate error
        if (res < 0)
        {
            connectionError(CLOUD_DISCONNECT_REASON_SOCKET, true);
            return -1;
        }

        // only if network is ok, if not connected to network not increment
        diagnostic::diagnosticCloud(CLOUD_CONNECTION_ATTEMPTS, 1);
    }
    else
    {
        LOG(ERROR, "Protocol not initialized correctly");
        setConnectionStatus(SOCKET_NOT_CONNECTED);
        return -1;
    }

    return 1;
}

void Trackle::disconnect()
{
    connectToCloud = false;
    if (connectionStatus == SOCKET_READY)
    {
        connectionError(CLOUD_DISCONNECT_REASON_USER);
    }
    else
    {
        setConnectionStatus(SOCKET_NOT_CONNECTED);
        if (trackle_protocol_is_initialized(protocol))
            trackle_protocol_command(protocol, ProtocolCommands::DISCONNECT);
        (*disconnectCb)();
    }
}

void Trackle::loop()
{
    // ignore if not enabled
    if (!cloudEnabled)
        return;

    // ready or disconnected
    int protocol_error = 0;
    static int last_protocol_error = 0;
    bool force_diagnostic = false;

    if (connectionStatus == SOCKET_READY /* || connectionStatus == SOCKET_NOT_CONNECTED*/)
    {
        int res = trackle_protocol_event_loop(protocol, &protocol_error);
        if (!res)
        {
            force_diagnostic = (protocol_error > 0 && protocol_error != last_protocol_error);
            // If wrapSend/Receive already closed, status != READY: do not overwrite the reason
            int mapped_error = mapProtocolErrorToDisconnectionReason(protocol_error);
            if (mapped_error != 0)
                connectionError(mapped_error, false, protocol_error);
        }
        else
        {
            // Reset on success
            last_protocol_error = 0;
        }
        if (!res && cloudStatus != res)
        {
            LOG(ERROR, "Event loop error");
        }
        cloudStatus = res;
    }

    // ready - check publish diagnostic (force on protocol error)
    if (connectionStatus == SOCKET_READY && (force_diagnostic || health_check_interval > 0))
    {
        system_tick_t millis_since_last_health_check = (*callbacks.millis)() - millis_last_sent_health_check;
        if (force_diagnostic || health_check_interval < millis_since_last_health_check)
        {
            last_protocol_error = protocol_error;
            millis_last_sent_health_check = (*callbacks.millis)();
            LOG(TRACE, force_diagnostic ? "Sending health check (protocol error)" : "Sending health check");
            trackle_protocol_post_description(protocol, trackle::protocol::DESCRIBE_METRICS);
        }
    }

    /*
     * At startup or after a disconnection event, try to create a new socket.
     * When a new socket is created correctly, connectionStatus is set to SOCKET_CONNECTING
     */
    if (connectionStatus == SOCKET_NOT_CONNECTED && connectToCloud == true)
    {
        system_tick_t millis_since_disconnection = (*callbacks.millis)() - millis_last_disconnection;

        if (connection_timeout < millis_since_disconnection)
        {
            LOG(INFO, "Cloud reconnection after %d ms", connection_timeout);

            millis_last_disconnection = (*callbacks.millis)();

            // create socket
            if (Trackle::connect() > 0)
            { // socket creation ok
                LOG(INFO, "Socket creation completed, starting handshake");
            }
            else // on socket creation error, reset timeout
            {
                reset_connection_timeout();
            }
        }
    }

    /*
     * A new socket was created correctly. Now we need to handshake the connection
     */
    if (connectionStatus == SOCKET_CONNECTING && connectToCloud == true)
    {
        int32_t ret = completeCloudConnection();

        /*
         * There was an error?
         */
        if (ret < 0)
        {
            // on cloud connection error, increase connection retry timeout
            LOG(TRACE, "Cloud connection error, increment reconnection timeout...");
            increase_connection_timeout();
        }
        else if (ret > 0) /* on success connection, reset timeout */
        {
            reset_connection_timeout();
        }
        else
        {
            /*
             * Handshake is not completed. Run again.
             */
        }
    }
}

// SETTER
void Trackle::setFirmwareVersion(int firmwareversion)
{
    trackle_protocol_set_product_firmware_version(protocol, firmwareversion);
}

void Trackle::setFirmwareBuild(int firmwarebuild)
{
    trackle_protocol_set_product_firmware_build(protocol, firmwarebuild);
}

void Trackle::setProductId(int productid)
{
    trackle_protocol_set_product_id(protocol, productid);
}

void Trackle::setDeviceId(const uint8_t deviceid[DEVICE_ID_LENGTH])
{
    // clear all bytes (including the termination one)
    memset(t_device_id, 0x00, sizeof(t_device_id));
    if (deviceid)
    { // else (if NULL), just leave the all-0 bytes
        memcpy(t_device_id, deviceid, DEVICE_ID_LENGTH);
    }
    string_device_id = hexStr(t_device_id, DEVICE_ID_LENGTH);
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
    // default values for diagnostics
    diagnostic::diagnosticCloud(CLOUD_PROTOCOL_ERROR_CODE, CLOUD_PROTOCOL_NO_ERROR);
    diagnostic::diagnosticCloud(CLOUD_DISCONNECTION_REASON, CLOUD_DISCONNECT_REASON_NONE);
    diagnostic::diagnosticCloud(CLOUD_DISCONNECTS, 0);
    diagnostic::diagnosticNetwork(NETWORK_DISCONNECTION_REASON, NETWORK_DISCONNECT_REASON_NONE);
    diagnostic::diagnosticNetwork(NETWORK_DISCONNECTS, 0);  

    // Configure the cloud
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
    descriptor.variable_type = wrapVarTypeInEnum;
    descriptor.get_variable = getUserVar;
    descriptor.append_system_info = appendSystemInfo;
    descriptor.append_metrics = diagnostic::appendMetrics;

    TinyDtls_set_log_callback(TrackleLib_tinydtls_log_wrapper);
    TinyDtls_set_rand(HAL_RNG_GetRandomNumber);
    TinyDtls_set_get_millis(TrackleLib_tinydtls_millis_wrapper);

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

void Trackle::diagnosticCloud(Cloud key, double value)
{
    diagnostic::diagnosticCloud(key, value);
}

void Trackle::diagnosticSystem(System key, double value)
{
    diagnostic::diagnosticSystem(key, value);
}

void Trackle::diagnosticNetwork(Network key, double value)
{
    diagnostic::diagnosticNetwork(key, value);
}

uint32_t HAL_RNG_GetRandomNumber(void)
{
    return getRandomCb ? (*getRandomCb)() : default_random_callback();
}

// ------------------------------------------ TINYDTLS MILLIS -------------------------------------------------

/**
 * Placeholder millis function that always returns 0.
 */
static uint32_t dumb_millis_callback()
{
    return 0;
}

/**
 * Pointer to the latest millis callback function set on a Trackle class.
 * Please note that, since Trackle instances can be many and tinydtls has only one instance, it doesn't matter which Trackle instance set the callback.
 */
static uint32_t (*latest_millis_callback)() = dumb_millis_callback;

void TrackleLib_tinydtls_millis_wrapper(uint32_t *t)
{
    *t = latest_millis_callback();
}

/**
 * Set latest log callback to be used by tinydtls.
 * @param new_latest_millis_callback
 */
void TrackleLib_set_latest_millis_callback_for_tinydtls(uint32_t (*new_latest_millis_callback)())
{
    latest_millis_callback = new_latest_millis_callback;
}
