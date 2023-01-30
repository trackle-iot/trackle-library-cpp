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

#ifdef __cplusplus
}
#endif

#endif