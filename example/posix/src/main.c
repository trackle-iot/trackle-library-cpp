/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.

  This software is free software; you can redistribute it and/or
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

// Standard library includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

// Trackle libraries includes
#include <trackle_interface.h>

// Local firmware includes
#include "trackle_hardcoded_credentials.h"
#include "callbacks.h"

#define MAIN_LOOP_PERIOD_MS 20 // Main loop period in milliseconds

#define SOFTWARE_VERSION 1
#define SOFTWARE_BUILD 2

// Cloud POST functions
static int funSuccess(const char *args, bool isOwner, const char *funName);
static int funFailure(const char *args, bool isOwner, const char *funName);
static int incrementCloudNumber(const char *args, bool isOwner, const char *funName);

// Cloud GET functions
static void *getCloudNumberMessage(const char *args, const char *varName);
static void *getHalfCloudNumber(const char *args, const char *varName);
static void *getLargeVariable(const char *args, const char *varName);
const char *get_large_properties_callback(const char *args);

// Cloud GET variables
static int cloudNumber = 0;

// Global pointer to Trackle instance (needed for OTA callback)
static Trackle *g_trackle_s = NULL;

int main()
{
    srand(time(NULL));

    printf("Starting up C example ...\n");

    printf("Device ID:");
    for (int i = 0; i < DEVICE_ID_LENGTH; i++)
        printf("%02X ", HARDCODED_DEVICE_ID[i]);
    printf("\n");

    // Create Trackle instance
    Trackle *trackle_s = newTrackle();
    g_trackle_s = trackle_s;  // Set global pointer for OTA callback
    trackleInit(trackle_s);
    trackleSetDeviceId(trackle_s, HARDCODED_DEVICE_ID);

    trackleSetLogCallback(trackle_s, Callbacks_log_cb);
    trackleSetLogLevel(trackle_s, TRACKLE_INFO);

    // Initialize Trackle
    trackleSetEnabled(trackle_s, true);

    // Set cloud credentials
    trackleSetKeys(trackle_s, HARDCODED_PRIVATE_KEY);
    trackleSetFirmwareVersion(trackle_s, SOFTWARE_VERSION);
    trackleSetFirmwareBuild(trackle_s, SOFTWARE_BUILD);
    trackleSetOtaMethod(trackle_s, NO_OTA);
    trackleSetConnectionType(trackle_s, CONNECTION_TYPE_WIFI);
    
    // Optional: configure OTA verification key (required for OTA with signature verification)
    // Uncomment and define HARDCODED_FIRMWARE_KEY if you want to enable OTA verification
    // trackleSetOtaVerificationKey(trackle_s, HARDCODED_FIRMWARE_KEY, sizeof(HARDCODED_FIRMWARE_KEY));
    
    // Optional: configure OTA update callback (required for OTA updates)
    // Uncomment and implement firmware_ota_callback function if you want to enable OTA
    // trackleSetOtaUpdateCallback(trackle_s, firmware_ota_callback);

    // Registering internal callbacks
    trackleSetMillis(trackle_s, Callbacks_get_millis_cb);
    trackleSetSendCallback(trackle_s, Callbacks_send_udp_cb);
    trackleSetReceiveCallback(trackle_s, Callbacks_receive_udp_cb);
    trackleSetConnectCallback(trackle_s, Callbacks_connect_udp_cb);
    trackleSetDisconnectCallback(trackle_s, Callbacks_disconnect_udp_cb);
    trackleSetSystemTimeCallback(trackle_s, Callbacks_set_time_cb);
    trackleSetSystemRebootCallback(trackle_s, Callbacks_reboot_cb);
    trackleSetPublishHealthCheckInterval(trackle_s, 60 * 60 * 1000);
    trackleSetCompletedPublishCallback(trackle_s, Callbacks_complete_publish);
    // trackleSetGetPropertyCallback(trackle_s, get_large_properties_callback);

    // Registering POST functions callable from cloud
    tracklePost(trackle_s, "funSuccess", funSuccess, ALL_USERS);
    tracklePost(trackle_s, "funFailure", funFailure, ALL_USERS);
    tracklePost(trackle_s, "incrementCloudNumber", incrementCloudNumber, ALL_USERS);

    // Registering values GETtable from cloud as result of a function call
    trackleGet(trackle_s, "getCloudNumberMessage", getCloudNumberMessage, VAR_STRING);
    trackleGet(trackle_s, "getHalfCloudNumber", getHalfCloudNumber, VAR_JSON);
    trackleGet(trackle_s, "longVar", getLargeVariable, VAR_JSON);

    printf("Startup completed. Running.\n");

    // Callbacks_setConnectionOverride(true, "192.168.1.177", 5684);
    trackleConnect(trackle_s);

    uint32_t msg_key = 0;
    uint32_t prevPubMillis = 0;

    for (;;)
    {
        trackleLoop(trackle_s);
        Callbacks_sleep_ms_cb(MAIN_LOOP_PERIOD_MS);
        if (Callbacks_get_millis_cb() - prevPubMillis > 10000)
        {
            // tracklePublish(trackle_s, "greetings", "Hello world!", 30, PRIVATE, WITH_ACK, msg_key);
            // tracklePublish(trackle_s, "greetings", get_large_properties_callback(""), 30, PRIVATE, WITH_ACK, msg_key);
            prevPubMillis = Callbacks_get_millis_cb();
            msg_key++;
        }
    }

    printf("Closing\n");
    deleteTrackle(trackle_s);

    return 0;
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *args, bool isOwner, const char *funName)
{
    return 1;
}

static int funFailure(const char *args, bool isOwner, const char *funName)
{
    return -1;
}

static int incrementCloudNumber(const char *args, bool isOwner, const char *funName)
{
    cloudNumber++;
    return 1;
}

// Esempio di callback per valori molto lunghi (usa block2 automaticamente)
const char *get_large_properties_callback(const char *args)
{
    // Questo esempio restituisce un JSON molto grande
    // La libreria userà automaticamente block2 per inviarlo in blocchi
    static char large_json[10000];

    large_json[0] = '{';
    int pos = 1;

    // Genera un JSON con molti dati
    // for (int i = 0; i < 10; i++)
    for (int i = 0; i < 40; i++)
    {
        if (i > 0)
            large_json[pos++] = ',';
        pos += snprintf(large_json + pos, sizeof(large_json) - pos,
                        "\"sensor_%d\":{\"value\":%d,\"timestamp\":%lu}",
                        i, i * 10, (unsigned long)time(NULL));
    }

    large_json[pos++] = '}';
    large_json[pos] = '\0';

    return large_json;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberMessage[64];
static void *getCloudNumberMessage(const char *args, const char *varName)
{
    sprintf(cloudNumberMessage, "The number is %d !", cloudNumber);
    return cloudNumberMessage;
}

static void *getHalfCloudNumber(const char *args, const char *varName)
{
    static char buffer[40];
    buffer[0] = '\0';
    sprintf(buffer, "{\"halfCloudNumber\":%d}", cloudNumber / 2);
    return buffer;
}

static void *getLargeVariable(const char *args, const char *varName)
{
    return get_large_properties_callback(args);
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- OTA callback example --------------------------------------------------------------------------------------------------------------------

/*
 * Example OTA callback implementation
 * This callback is called when the cloud requests a firmware update
 * 
 * @param url The URL of the firmware to download
 * @param crc The expected CRC32 of the firmware
 * @return 0 on success, error code on failure
 */
/*
static int firmware_ota_callback(const char *url, uint32_t crc)
{
    printf("OTA update requested: URL=%s, CRC=0x%08X\n", url, crc);
    
    // TODO: Implement firmware download logic here
    // 1. Download firmware from URL
    // 2. Calculate SHA256 hash during download
    // 3. Verify signature using trackleVerifyOtaSignature
    // 4. Validate CRC if provided
    // 5. Save firmware to flash/storage
    // 6. Call trackleSetOtaUpdateDone with result
    
    // Example implementation flow:
    // uint8_t firmware_hash[32];  // SHA256 hash
    // 
    // if (g_trackle_s == NULL)
    // {
    //     printf("Error: Trackle instance not available\n");
    //     return -1;
    // }
    // 
    // // Download firmware and calculate hash...
    // // (implementation depends on your platform)
    // 
    // // Verify signature (returns 1 on success, 0 if skipped, -1 on error)
    // int verify_result = trackleVerifyOtaSignature(g_trackle_s, firmware_hash, sizeof(firmware_hash));
    // if (verify_result == 1)
    // {
    //     // Signature verified successfully
    //     // Save firmware and prepare for update...
    //     trackleSetOtaUpdateDone(g_trackle_s, 0);  // 0 = success
    //     return 0;
    // }
    // else if (verify_result == -1)
    // {
    //     // Signature verification failed
    //     trackleSetOtaUpdateDone(g_trackle_s, -1);  // error code
    //     return -1;
    // }
    // else
    // {
    //     // Verification skipped (no key set)
    //     // Continue with update anyway...
    //     trackleSetOtaUpdateDone(g_trackle_s, 0);
    //     return 0;
    // }
    
    return 0;
}
*/

// END -- OTA callback example ----------------------------------------------------------------------------------------------------------------------
