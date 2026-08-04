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

static bool handle_updates_pending(const char *data)
{
    updates_pending = (strcmp(data, "true") == 0 ? true : false);
    return true;
}

static bool handle_updates_forced(const char *data)
{
    updates_forced = (strcmp(data, "true") == 0 ? true : false);
    return true;
}

static void handle_owners(const char *data)
{
    owners.clear();
    if (data == NULL)
    {
        return;
    }

    std::stringstream ss(data);
    while (ss.good())
    {
        string substr;
        getline(ss, substr, ',');
        owners.push_back(substr.c_str());
    }

    if (owners.size() > 0)
    {
        LOG(INFO, "Device is claimed by one owner.");
        if (deviceClaimedCb)
        {
            (*deviceClaimedCb)();
        }
    }
}

static void handle_reset(const char *data)
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

static void handle_pin_code(const char *data)
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

static void parse_ota_signature(const char *signature)
{
    ota_data.has_signature = true;

    if (signature == NULL || strlen(signature) < 136) // 68*2 = 136 hex chars minimum
    {
        LOG(WARN, "No firmware signature");
        ota_data.has_signature = false;
        return;
    }

    uint8_t raw_signature[80]; // Fixed buffer, theoretical max is 72 bytes
    uint8_t result_r[DTLS_EC_KEY_SIZE];
    uint8_t result_s[DTLS_EC_KEY_SIZE];
    size_t sig_len = strlen(signature) / 2;

    if (sig_len > sizeof(raw_signature))
    {
        LOG(ERROR, "Signature too long: %zu bytes", sig_len);
        ota_data.has_signature = false;
        return;
    }

    for (size_t i = 0; i < sig_len; i++)
    {
        sscanf(signature + 2 * i, "%2hhx", &raw_signature[i]);
    }

    // DER parsing: skip SEQUENCE header (30 XX)
    uint8_t *sig_data = raw_signature + 2;
    size_t data_len = sig_len - 2;

    int r_consumed = dtls_asn1_integer_to_ec_key(sig_data, data_len, result_r, DTLS_EC_KEY_SIZE);
    if (r_consumed <= 0)
    {
        LOG(ERROR, "Failed to parse r from signature");
        ota_data.has_signature = false;
        return;
    }

    sig_data += r_consumed;
    data_len -= r_consumed;
    int s_consumed = dtls_asn1_integer_to_ec_key(sig_data, data_len, result_s, DTLS_EC_KEY_SIZE);
    if (s_consumed <= 0)
    {
        LOG(ERROR, "Failed to parse s from signature");
        ota_data.has_signature = false;
        return;
    }

    memcpy(ota_data.firmware_signature, result_r, DTLS_EC_KEY_SIZE);
    memcpy(ota_data.firmware_signature + DTLS_EC_KEY_SIZE, result_s, DTLS_EC_KEY_SIZE);
    ota_data.has_signature = true;

    char signature_hex[2 * DTLS_EC_KEY_SIZE * 2 + 1];
    for (int i = 0; i < DTLS_EC_KEY_SIZE * 2; i++)
    {
        sprintf(signature_hex + 2 * i, "%02x", ota_data.firmware_signature[i]);
    }
    LOG(INFO, "Firmware signature in hex: %s", signature_hex);
}

static void start_ota_update(Trackle *trackle, const char *url, uint32_t crc)
{
    int ota_error = (*otaUpdateCb)(url, crc);
    char ota_cloud_message[256];

    if (ota_error == NO_ERROR)
    {
        ota_data.running = true;
        sprintf(ota_cloud_message, "started,%s", ota_data.ota_job_id);
        trackle->publish(OTA_EVENT_NAME, ota_cloud_message, PRIVATE);
        LOG(INFO, "otaUpdateCb OTA start successfully, job_id %s", ota_data.ota_job_id);
    }
    else
    {
        sprintf(ota_cloud_message, "failed,%s,%d", ota_data.ota_job_id, ota_error);
        trackle->publish(OTA_EVENT_NAME, ota_cloud_message, PRIVATE);
        LOG(INFO, "otaUpdateCb OTA start error, job_id %s", ota_data.ota_job_id);
    }
}

static void handle_device_update(Trackle *trackle, const char *data)
{
    if (!otaUpdateCb)
    {
        LOG(INFO, "otaUpdateCb not implemented...");
        return;
    }

    char *copy = strdup(data);
    if (copy == NULL)
    {
        LOG(ERROR, "strdup failed");
        return;
    }

    char *saveptr = copy;
    char *url = strtok_r(copy, ",", &saveptr);
    char *crc32 = strtok_r(NULL, ",", &saveptr);
    char *job_id = strtok_r(NULL, ",", &saveptr);
    char *signature = strtok_r(NULL, ",", &saveptr);

    if (!updates_enabled && !updates_forced)
    {
        LOG(WARN, "Ota upgrade refused: enabled %d, forced: %d", updates_enabled, updates_forced);
        char ota_cloud_message[256];
        snprintf(ota_cloud_message, sizeof(ota_cloud_message), "disabled,%s", job_id ? job_id : "");
        trackle->publish(OTA_EVENT_NAME, ota_cloud_message, PRIVATE);
        free(copy);
        return;
    }

    if (ota_data.running)
    {
        LOG(ERROR, "Ota already in progress...");
        char ota_cloud_message[256];
        snprintf(ota_cloud_message, sizeof(ota_cloud_message), "busy,%s", job_id ? job_id : "");
        trackle->publish(OTA_EVENT_NAME, ota_cloud_message, PRIVATE);
        free(copy);
        return;
    }

    LOG(INFO, "otaUpdateCb %s", data);
    memset(ota_data.ota_job_id, 0, 64);
    memset(ota_data.firmware_signature, 0, 64);
    ota_data.ota_job_id[0] = '0';

    uint32_t crc = 0;
    uint32_t ota_type = 0; // 0 undefined, 1 product, 2 developer

    if (url == NULL)
    {
        LOG(ERROR, "not valid url");
    }
    else if (crc32 != NULL && job_id != NULL)
    {
        // product firmware update
        const bool crc_is_valid = (sscanf(crc32, "%" PRIx32 "", &crc) == 1);
        if (!crc_is_valid)
        {
            LOG(ERROR, "Ota upgrade refused: invalid crc32 \"%s\"", crc32);
        }

        snprintf(ota_data.ota_job_id, sizeof(ota_data.ota_job_id), "%s", job_id);
        parse_ota_signature(signature);
        ota_type = crc_is_valid ? 1 : 0;
    }
    else
    {
        // developer firmware update
        crc = 0;
        ota_type = 2;
    }

    if (ota_type > 0)
    {
        start_ota_update(trackle, url, crc);
    }

    free(copy);
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
    Trackle *trackle = (Trackle *)handler;

    if (strcmp(event_name, "trackle/device/updates/pending") == 0)
    {
        replyWithPublish = handle_updates_pending(data);
    }
    else if (strcmp(event_name, "trackle/device/updates/forced") == 0)
    {
        replyWithPublish = handle_updates_forced(data);
    }
    else if (strcmp(event_name, "trackle/device/owners") == 0)
    {
        handle_owners(data);
    }
    else if (strcmp(event_name, "trackle/device/reset") == 0)
    {
        handle_reset(data);
    }
    else if (strcmp(event_name, "trackle/device/update") == 0)
    {
        handle_device_update(trackle, data);
    }
    else if (strcmp(event_name, "trackle/device/pin_code") == 0)
    {
        handle_pin_code(data);
    }

    if (replyWithPublish)
    {
        char event_copy[64], data_copy[16];
        strncpy(event_copy, event_name, sizeof(event_copy) - 1);
        strncpy(data_copy, data ? data : "", sizeof(data_copy) - 1);
        event_copy[sizeof(event_copy) - 1] = '\0';
        data_copy[sizeof(data_copy) - 1] = '\0';
        trackle->publish(event_copy, data_copy, PRIVATE);
    }
}
