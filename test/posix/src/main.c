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

char message1[5000] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
char message2[5000] = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

// Cloud POST functions
static int funSuccess(const char *args);
static int funFailure(const char *args);
static int incrementCloudNumber(const char *args);

// Cloud GET functions
static const void *getCloudNumberMessage(const char *args);
static const void *getHalfCloudNumber(const char *args);

// Cloud GET variables
static int cloudNumber = 0;

int main()
{
    printf("Starting up C example ...\n");
    srand(time(NULL));

    printf("Device ID:");
    for (int i = 0; i < DEVICE_ID_LENGTH; i++)
        printf("%02X ", HARDCODED_DEVICE_ID[i]);
    printf("\n");

    // Create Trackle instance
    Trackle *trackle_s = newTrackle();
    trackleInit(trackle_s);
    trackleSetDeviceId(trackle_s, HARDCODED_DEVICE_ID);

    trackleSetLogCallback(trackle_s, Callbacks_log_cb);
    trackleSetLogLevel(trackle_s, TRACKLE_INFO);

    // Initialize Trackle
    trackleSetEnabled(trackle_s, true);

    // Set cloud credentials
    trackleSetKeys(trackle_s, HARDCODED_PRIVATE_KEY);
    trackleSetFirmwareVersion(trackle_s, SOFTWARE_VERSION);
    trackleSetOtaMethod(trackle_s, NO_OTA);
    trackleSetConnectionType(trackle_s, CONNECTION_TYPE_WIFI);

    // Registering internal callbacks
    trackleSetMillis(trackle_s, Callbacks_get_millis_cb);
    trackleSetSendCallback(trackle_s, Callbacks_send_udp_cb);
    trackleSetReceiveCallback(trackle_s, Callbacks_receive_udp_cb);
    trackleSetConnectCallback(trackle_s, Callbacks_connect_udp_cb);
    trackleSetDisconnectCallback(trackle_s, Callbacks_disconnect_udp_cb);
    trackleSetSystemTimeCallback(trackle_s, Callbacks_set_time_cb);
    trackleSetSleepCallback(trackle_s, Callbacks_sleep_ms_cb);
    trackleSetSystemRebootCallback(trackle_s, Callbacks_reboot_cb);
    trackleSetPublishHealthCheckInterval(trackle_s, 60 * 60 * 1000);
    trackleSetCompletedPublishCallback(trackle_s, Callbacks_complete_publish);

    // Registering POST functions callable from cloud
    tracklePost(trackle_s, "funSuccess", funSuccess, ALL_USERS);
    tracklePost(trackle_s, "funFailure", funFailure, ALL_USERS);
    tracklePost(trackle_s, "incrementCloudNumber", incrementCloudNumber, ALL_USERS);

    // Registering values GETtable from cloud as result of a function call
    trackleGet(trackle_s, "getCloudNumberMessage", getCloudNumberMessage, VAR_STRING);
    trackleGet(trackle_s, "getHalfCloudNumber", getHalfCloudNumber, VAR_JSON);

    printf("Startup completed. Running.\n");

    trackleConnect(trackle_s);

    uint32_t msg_key = 0;
    uint32_t prevPubMillis = Callbacks_get_millis_cb();
    uint32_t prevSendCMillis = Callbacks_get_millis_cb();
    int test = 0;
    int res;

    for (;;)
    {
        trackleLoop(trackle_s);
        Callbacks_sleep_ms_cb(MAIN_LOOP_PERIOD_MS);

        if (Callbacks_get_millis_cb() - prevPubMillis > 5000)
        {
            /*printf("SENDING B\n");
            res = tracklePublish(trackle_s, "b", "ciao", 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);
            res = tracklePublish(trackle_s, "b", message1, 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);*/

            printf("SENDING A\n");
            res = tracklePublish(trackle_s, "a", "ciao da me", 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);
            res = tracklePublish(trackle_s, "b", message1, 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);
            res = tracklePublish(trackle_s, "c", message1, 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);
            res = tracklePublish(trackle_s, "d", message1, 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);
            res = tracklePublish(trackle_s, "e", message1, 30, PRIVATE, NO_ACK, msg_key);
            printf("RES %d\n", res);

            prevPubMillis = Callbacks_get_millis_cb();
            prevSendCMillis = Callbacks_get_millis_cb();

            msg_key++;
            test = 1;
        }

        if (test == 1 && (Callbacks_get_millis_cb() - prevSendCMillis > 1000))
        {   
            /*printf("SENDING C\n");
            res = tracklePublish(trackle_s, "f", message2, 30, PRIVATE, WITH_ACK, msg_key);
            printf("RES %d\n", res);*/

            test = 0;
        }

    }

    printf("Closing\n");
    deleteTrackle(trackle_s);

    return 0;
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *args)
{
    return 1;
}

static int funFailure(const char *args)
{
    return -1;
}

static int incrementCloudNumber(const char *args)
{
    cloudNumber++;
    return 1;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberMessage[64];
static const void *getCloudNumberMessage(const char *args)
{
    sprintf(cloudNumberMessage, "The number is %d !", cloudNumber);
    return cloudNumberMessage;
}

static const void *getHalfCloudNumber(const char *args)
{
    static char buffer[40];
    buffer[0] = '\0';
    sprintf(buffer, "{\"halfCloudNumber\":%d}", cloudNumber / 2);
    return buffer;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------
