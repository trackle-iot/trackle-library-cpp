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

#ifndef CALLBACKS_H_
#define CALLBACKS_H_

#include <time.h>
#include <inttypes.h>

#include <trackle_interface.h>

#ifdef __cplusplus
extern "C"
{
#endif

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