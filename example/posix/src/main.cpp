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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>
#include <cinttypes>
#include <stdlib.h>

// Trackle libraries includes
#include <trackle.h>

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
bool getBoolFn(const char *args, const char *varKey);
int getIntFn(const char *args, const char *varKey);
double getDoubleFn(const char *args, const char *varKey);

// Cloud GET variables
static int cloudNumber = 0;

int main()
{
    srand(time(NULL));

    std::cout << "Starting up C++ example ...\n";

    std::cout << "Device ID:";
    for (int i = 0; i < DEVICE_ID_LENGTH; i++)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(HARDCODED_DEVICE_ID[i]) << " ";
    std::cout << std::endl;

    // Create Trackle instance
    Trackle trackleInst;
    
    // Note: If you enable OTA callback, uncomment the global pointer in firmware_ota_callback
    // and set it here: g_trackleInst = &trackleInst;

    trackleInst.setDeviceId(HARDCODED_DEVICE_ID);

    trackleInst.setLogCallback(Callbacks_log_cb);
    trackleInst.setLogLevel(TRACKLE_INFO);

    // Initialize Trackle
    trackleInst.setEnabled(true);

    // Set cloud credentials
    trackleInst.setKeys(HARDCODED_PRIVATE_KEY);
    trackleInst.setFirmwareVersion(SOFTWARE_VERSION);
    trackleInst.setFirmwareBuild(SOFTWARE_BUILD);
    trackleInst.setOtaMethod(NO_OTA);
    trackleInst.setConnectionType(CONNECTION_TYPE_WIFI);
    
    // Optional: configure OTA verification key (required for OTA with signature verification)
    // Uncomment and define HARDCODED_FIRMWARE_KEY if you want to enable OTA verification
    // trackleInst.setOtaVerificationKey(HARDCODED_FIRMWARE_KEY, sizeof(HARDCODED_FIRMWARE_KEY));
    
    // Optional: configure OTA update callback (required for OTA updates)
    // Uncomment and implement firmware_ota_callback function if you want to enable OTA
    // trackleInst.setOtaUpdateCallback(firmware_ota_callback);

    // Registering internal callbacks
    trackleInst.setMillis(Callbacks_get_millis_cb);
    trackleInst.setSendCallback(Callbacks_send_udp_cb);
    trackleInst.setReceiveCallback(Callbacks_receive_udp_cb);
    trackleInst.setConnectCallback(Callbacks_connect_udp_cb);
    trackleInst.setDisconnectCallback(Callbacks_disconnect_udp_cb);
    trackleInst.setSystemTimeCallback(Callbacks_set_time_cb);
    trackleInst.setSystemRebootCallback(Callbacks_reboot_cb);
    trackleInst.setPublishHealthCheckInterval(60 * 60 * 1000);
    trackleInst.setCompletedPublishCallback(Callbacks_complete_publish);

    // Registering POST functions callable from cloud
    trackleInst.post("funSuccess", funSuccess, ALL_USERS);
    trackleInst.post("funFailure", funFailure, ALL_USERS);
    trackleInst.post("incrementCloudNumber", incrementCloudNumber, ALL_USERS);

    // Registering values GETtable from cloud as result of a function call
    trackleInst.get("getCloudNumberMessage", getCloudNumberMessage, VAR_STRING);
    trackleInst.get("getHalfCloudNumber", getHalfCloudNumber, VAR_JSON);
    trackleInst.get("getInt", getIntFn);
    trackleInst.get("getDouble", getDoubleFn);
    trackleInst.get("getBool", getBoolFn);

    std::cout << "Startup completed. Running.\n";

    trackleInst.connect();

    uint32_t msg_key = 0;
    uint32_t prevPubMillis = 0;
    for (;;)
    {
        trackleInst.loop();
        Callbacks_sleep_ms_cb(MAIN_LOOP_PERIOD_MS);
        if (Callbacks_get_millis_cb() - prevPubMillis > 5000)
        {
            trackleInst.publish("greetings", "Hello world!", 30, PRIVATE, WITH_ACK, msg_key);
            prevPubMillis = Callbacks_get_millis_cb();
            msg_key++;
        }
    }

    std::cout << "Closing\n";

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

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberBuffer[1024];

static void *getCloudNumberMessage(const char *args, const char *varName)
{
    std::stringstream cnStream;
    cnStream << "Var name is " << varName << "! ";
    cnStream << "The number is " << cloudNumber << "!";
    strncpy(cloudNumberBuffer, cnStream.str().c_str(), 1023);
    cloudNumberBuffer[1023] = '\0';
    return cloudNumberBuffer;
}

static void *getHalfCloudNumber(const char *args, const char *varName)
{
    std::stringstream cnStream;
    cnStream << "{\"halfCloudNumber\":" << (cloudNumber / 2) << "}";
    strncpy(cloudNumberBuffer, cnStream.str().c_str(), 1023);
    cloudNumberBuffer[1023] = '\0';
    return cloudNumberBuffer;
}

bool getBoolFn(const char *args, const char *varKey)
{
    bool c = false;
    if (strcmp(args, "1") == 0)
    {
        c = true;
    }

    return c;
}

int getIntFn(const char *args, const char *varKey)
{
    int a = atoi(args);
    return a;
}

double getDoubleFn(const char *args, const char *varKey)
{
    double b = atof(args);
    return b;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- OTA callback example --------------------------------------------------------------------------------------------------------------------

/*
 * Example OTA callback implementation
 * This callback is called when the cloud requests a firmware update
 * 
 * Note: To access the Trackle instance from this callback, you can use a global pointer:
 *   Trackle* g_trackleInst = nullptr;
 *   Then set it in main(): g_trackleInst = &trackleInst;
 * 
 * @param url The URL of the firmware to download
 * @param crc The expected CRC32 of the firmware
 * @return 0 on success, error code on failure
 */
/*
// Global pointer to Trackle instance (set in main())
static Trackle* g_trackleInst = nullptr;

static int firmware_ota_callback(const char *url, uint32_t crc)
{
    std::cout << "OTA update requested: URL=" << url << ", CRC=0x" << std::hex << crc << std::dec << std::endl;
    
    if (g_trackleInst == nullptr)
    {
        std::cerr << "Error: Trackle instance not available" << std::endl;
        return -1;
    }
    
    // TODO: Implement firmware download logic here
    // 1. Download firmware from URL
    // 2. Calculate SHA256 hash during download
    // 3. Verify signature using verifyOtaSignature
    // 4. Validate CRC if provided
    // 5. Save firmware to flash/storage
    // 6. Call setOtaUpdateDone with result
    
    // Example implementation flow:
    // uint8_t firmware_hash[32];  // SHA256 hash
    // 
    // // Download firmware and calculate hash...
    // // (implementation depends on your platform)
    // 
    // // Verify signature (returns 1 on success, 0 if skipped, -1 on error)
    // int verify_result = g_trackleInst->verifyOtaSignature(firmware_hash, sizeof(firmware_hash));
    // if (verify_result == 1)
    // {
    //     // Signature verified successfully
    //     // Save firmware and prepare for update...
    //     g_trackleInst->setOtaUpdateDone(0);  // 0 = success
    //     return 0;
    // }
    // else if (verify_result == -1)
    // {
    //     // Signature verification failed
    //     g_trackleInst->setOtaUpdateDone(-1);  // error code
    //     return -1;
    // }
    // else
    // {
    //     // Verification skipped (no key set)
    //     // Continue with update anyway...
    //     g_trackleInst->setOtaUpdateDone(0);
    //     return 0;
    // }
    
    return 0;
}
*/

// END -- OTA callback example ----------------------------------------------------------------------------------------------------------------------
