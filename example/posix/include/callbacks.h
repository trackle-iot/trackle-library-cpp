/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2022 IOTREADY S.r.l. All rights reserved.
 *
 * This source code is licensed under the Trackle Source-Available License
 * Agreement found in the LICENSE file in the root directory of this source tree.
 * Commercial deployment requires one paid Device License Key per device.
 */

#ifndef CALLBACKS_H_
#define CALLBACKS_H_

#include <time.h>
#include <inttypes.h>

#include <trackle_interface.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void Callbacks_setConnectionOverride(bool override, char *address, int port);
    system_tick_t Callbacks_get_millis_cb();
    void Callbacks_sleep_ms_cb(uint32_t milliseconds);
    void Callbacks_set_time_cb(time_t time, unsigned int param, void *reserved);
    int Callbacks_connect_udp_cb(const char *address, int port);
    int Callbacks_disconnect_udp_cb();
    int Callbacks_send_udp_cb(const unsigned char *buf, uint32_t buflen, void *tmp);
    int Callbacks_receive_udp_cb(unsigned char *buf, uint32_t buflen, void *tmp);
    void Callbacks_log_cb(const char *msg, int level, const char *category, void *attribute, void *reserved);
    void Callbacks_reboot_cb(const char *data);
    void Callbacks_complete_publish(int error, const void *data, void *callbackData, void *reserved);

#ifdef __cplusplus
}
#endif

#endif