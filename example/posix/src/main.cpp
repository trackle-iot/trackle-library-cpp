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

// Trackle libraries includes
#include <trackle.h>

// Local firmware includes
#include "trackle_hardcoded_credentials.h"
#include "callbacks.h"

#define MAIN_LOOP_PERIOD_MS 20 // Main loop period in milliseconds

#define SOFTWARE_VERSION 1

// Cloud POST functions
static int funSuccess(const char *args, ...);
static int funFailure(const char *args, ...);
static int incrementCloudNumber(const char *args, ...);

// Cloud GET functions
static void *getCloudNumberMessage(const char *args);
static void *getHalfCloudNumber(const char *args);

// Cloud GET variables
static int cloudNumber = 0;

int main()
{
    std::cout << "Starting up C++ example ...\n";

    std::cout << "Device ID:";
    for (int i = 0; i < DEVICE_ID_LENGTH; i++)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(HARDCODED_DEVICE_ID[i]) << " ";
    std::cout << std::endl;

    // Create Trackle instance
    Trackle trackleInst;

    trackleInst.setDeviceId(HARDCODED_DEVICE_ID);

    trackleInst.setLogCallback(Callbacks_log_cb);
    trackleInst.setLogLevel(TRACKLE_INFO);

    // Initialize Trackle
    trackleInst.setEnabled(true);

    // Set cloud credentials
    trackleInst.setKeys(HARDCODED_PRIVATE_KEY);
    trackleInst.setFirmwareVersion(SOFTWARE_VERSION);
    trackleInst.setOtaMethod(NO_OTA);
    trackleInst.setConnectionType(CONNECTION_TYPE_WIFI);

    // Registering internal callbacks
    trackleInst.setMillis(Callbacks_get_millis_cb);
    trackleInst.setSendCallback(Callbacks_send_udp_cb);
    trackleInst.setReceiveCallback(Callbacks_receive_udp_cb);
    trackleInst.setConnectCallback(Callbacks_connect_udp_cb);
    trackleInst.setDisconnectCallback(Callbacks_disconnect_udp_cb);
    trackleInst.setSystemTimeCallback(Callbacks_set_time_cb);
    trackleInst.setSleepCallback(Callbacks_sleep_ms_cb);
    trackleInst.setSystemRebootCallback(Callbacks_reboot_cb);
    trackleInst.setPublishHealthCheckInterval(60 * 60 * 1000);

    // Registering POST functions callable from cloud
    trackleInst.post("funSuccess", funSuccess, ALL_USERS);
    trackleInst.post("funFailure", funFailure, ALL_USERS);
    trackleInst.post("incrementCloudNumber", incrementCloudNumber, ALL_USERS);

    // Registering values GETtable from cloud as result of a function call
    trackleInst.get("getCloudNumberMessage", getCloudNumberMessage, VAR_STRING);
    trackleInst.get("getHalfCloudNumber", getHalfCloudNumber, VAR_JSON);

    std::cout << "Startup completed. Running.\n";

    trackleInst.connect();

    uint32_t prevPubMillis = 0;
    for (;;)
    {
        trackleInst.loop();
        Callbacks_sleep_ms_cb(MAIN_LOOP_PERIOD_MS);
        if (Callbacks_get_millis_cb() - prevPubMillis > 5000)
        {
            trackleInst.publish("greetings", "Hello world!", 30, PRIVATE, EMPTY_FLAGS);
            prevPubMillis = Callbacks_get_millis_cb();
        }
    }

    std::cout << "Closing\n";

    return 0;
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *args, ...)
{
    return 1;
}

static int funFailure(const char *args, ...)
{
    return -1;
}

static int incrementCloudNumber(const char *args, ...)
{
    cloudNumber++;
    return 1;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberBuffer[1024];

static void *getCloudNumberMessage(const char *args)
{
    std::stringstream cnStream;
    cnStream << "The number is " << cloudNumber << "!";
    strncpy(cloudNumberBuffer, cnStream.str().c_str(), 1023);
    cloudNumberBuffer[1023] = '\0';
    return cloudNumberBuffer;
}

static void *getHalfCloudNumber(const char *args)
{
    std::stringstream cnStream;
    cnStream << "{\"halfCloudNumber\":" << (cloudNumber / 2) << "}";
    strncpy(cloudNumberBuffer, cnStream.str().c_str(), 1023);
    cloudNumberBuffer[1023] = '\0';
    return cloudNumberBuffer;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------
