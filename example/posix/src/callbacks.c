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

#include "callbacks.h"

#include <time.h>
#include <stdio.h>

#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

// Socket for connection to cloud
static struct sockaddr_in cloud_addr;
static int cloud_socket = -1;

system_tick_t Callbacks_get_millis_cb()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_nsec + t.tv_sec * 1000000000L) / 1000000;
}

void Callbacks_sleep_ms_cb(uint32_t milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void Callbacks_set_time_cb(time_t time, unsigned int param, void *reserved)
{
    // Since we are on a user machine that should have time already set, we don't do anything here.
}

/**
 * It creates a socket, sets the timeout and calls the trackleConnectionCompleted function
 *
 * @param address the address of the server to connect to.
 * @param port the port to connect to
 *
 * @return The socket descriptor.
 */
int Callbacks_connect_udp_cb(const char *address, int port)
{
    printf("Connecting socket\n");
    int addr_family;
    int ip_protocol;
    char addr_str[128];

    struct hostent *res = gethostbyname(address);

    if (res)
        printf("Dns address %s resolved\n", address);
    else
    {
        printf("error resolving gethostbyname %s\n", address);
        return -1;
    }

    // cloud_addr.sin_addr.s_addr = inet_addr(address);
    memcpy(&cloud_addr.sin_addr.s_addr, res->h_addr, sizeof(cloud_addr.sin_addr.s_addr));

    cloud_addr.sin_family = AF_INET;
    cloud_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    memcpy(addr_str, inet_ntoa(cloud_addr.sin_addr), sizeof(addr_str) - 1);
    addr_str[sizeof(addr_str) - 1] = '\0';

    cloud_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (cloud_socket < 0)
    {
        printf("Unable to create socket: errno %d\n", errno);
        return -3;
    }
    printf("Socket created, sending to %s:%d\n", address, port);

    // setto i timeout di lettura/scrittura del socket
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 1000; // 1ms
    setsockopt(cloud_socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&socket_timeout, sizeof(struct timeval));

    return 1;
}

/**
 * It's a callback function that close the cloud connection
 *
 * @return 1
 */
int Callbacks_disconnect_udp_cb()
{
    if (cloud_socket)
    {
        close(cloud_socket);
        cloud_socket = -1;
    }
    return 1;
}

/**
 * It sends the data to the cloud server
 *
 * @param buf The buffer to send
 * @param buflen the length of the buffer to send
 *
 * @return The number of bytes sent.
 */
int Callbacks_send_udp_cb(const unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t sent = sendto(cloud_socket, (const char *)buf, buflen, 0, (struct sockaddr *)&cloud_addr, sizeof(cloud_addr));
    if ((int)sent > 0)
        printf("send_cb_udp sent %d\n", sent);

    return (int)sent;
}

/**
 * It receives data from the socket and returns the number of bytes received
 *
 * @param buf pointer to the buffer where the received data will be stored
 * @param buflen The maximum number of bytes to receive.
 *
 * @return The number of bytes received.
 */
int Callbacks_receive_udp_cb(unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t res = recvfrom(cloud_socket, (char *)buf, buflen, 0, (struct sockaddr *)NULL, NULL);
    if ((int)res > 0)
        printf("receive_cb_udp received %d\n", res);

    // on timeout error, set bytes received to 0
    if ((int)res < 0 && errno == EAGAIN)
        res = 0;

    return (int)res;
}

void Callbacks_log_cb(const char *msg, int level, const char *category, void *attribute, void *reserved)
{
    printf("%u - Log_cb(lvl=%d): (%s) -> %s\n", Callbacks_get_millis_cb(), level, (category ? category : ""), msg);
}

void Callbacks_reboot_cb(const char *data)
{
    printf("Reboot request ignored\n");
}

void Callbacks_complete_publish(int error, const void *data, void *callbackData, void *reserved)
{
    uint32_t *b = (uint32_t *)callbackData;
    const char *c = (const char *)data;

    printf("callback_complete_publish (%d) result : %d...\n", b, error);
    printf("message: %s...\n", c);
}
