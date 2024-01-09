# Trackle library 

````
                     __     _         _ 
  _                 |    __| |       (_)   
 / |_  _ __ ___  ___|   / /| | ___    _  ___  
/_ __||  _/|__ \/ __|  / / | |/ _ \  | |/ _ \ 
 | |__| |  / _ | (__| /  \ | |  __/ _| | (_) |
  \__/|_|  \___/\___|/  \_\|_|\___|(_)_|\___/
````


[![GitHub version](https://img.shields.io/badge/version-v3.1.0-blue)](https://github.com/trackle-iot/trackle-library-cpp/releases/latest) &nbsp; &nbsp;
[![GitHub stars](https://img.shields.io/github/stars/trackle-iot/trackle-library?style=social)](https://github.com/trackle-iot/trackle-library-cpp/stargazers) 
__________

Complete documentation can be found [here](https://trackle-iot.github.io/trackle-library-cpp/).

## Table of contents
- [Trackle library](#trackle-library)
  - [Table of contents](#table-of-contents)
  - [What is Trackle](#what-is-trackle)
  - [Overview](#overview)
    - [Supported hardware](#supported-hardware)
    - [License](#license)
    - [Download](#download)
    - [Usage and API](#usage-and-api)
      - [Get a Device ID and a private key](#get-a-device-id-and-a-private-key)
      - [Getting started in C++](#getting-started-in-c)
      - [Getting started in C](#getting-started-in-c-1)
      - [Trackle client](#trackle-client)

## What is Trackle
Trackle is an IoT platform that offers all the software and services needed to develop an IoT solution from Device to Cloud. [Trackle website](https://www.trackle.io)

## Overview
This document provides instructions to use the Trackle library and connect your device to Trackle Cloud.

### Supported hardware
Trackle library is hardware agnostic. It depends on tinydtls that is compatible with contiki, esp-idf, posix, riot, windows and zephyr.

### License
Unless stated elsewhere, file headers or otherwise, all files herein are licensed under an LGPLv3 license. For more information, please read the LICENSE file.

### Download
You can download last **Trackle Library** from [here](https://github.com/trackle-iot/trackle-library-cpp/releases/latest).

### Usage and API

#### Get a Device ID and a private key
* Create an account on Trackle Cloud (https://trackle.cloud/)
* Open "My Devices" section from the drawer
* Click the button "Claim a device"
* Select the link "I don't have a device id", then Continue
* The Device Id will be shown on the screen and the private key file will be download with name <device_id>.der where <device_id> is Device ID taken from Trackle.

#### Getting started in C++
Here is a very simple C++ example that create a client and connect it to Trackle cloud

```
#include "trackle.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
...

unsigned char private_key[PRIVATE_KEY_LENGTH] = { ... };
static uint8_t device_id[DEVICE_ID_LENGTH] = { ... };

Trackle trackle;
struct sockaddr_in cloud_addr;
int cloud_socket;

/*
* This function is used to get the current time in milliseconds.
*/
static system_tick_t getMillis(void)
{
	struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return (uint32_t)ms;
}

/**
 * This creates a socket, sets the timeout and return socket creation result
 */
int connect_cb_udp(const char *address, int port)
{
    printf("Connecting socket");
    int addr_family;
    int ip_protocol;
    char addr_str[128];

    struct hostent *res = gethostbyname(address);
    if (res)
    {
        printf("Dns address %s resolved", address);
    }
    else
    {
        printf("error resolving gethostbyname %s resolved", address);
        return -1;
    }

    memcpy(&cloud_addr.sin_addr.s_addr, res->h_addr, sizeof(cloud_addr.sin_addr.s_addr));

    cloud_addr.sin_family = AF_INET;
    cloud_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(cloud_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    cloud_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (cloud_socket < 0)
    {
        printf("Unable to create socket: errno %d", errno);
    }
    printf("Socket created, sending to %s:%d", address, port);

    // setto i timeout di lettura/scrittura del socket
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 1000; // 1ms
    setsockopt(cloud_socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&socket_timeout, sizeof(struct timeval));

    return 1;
}

/**
 * This is a callback function that close the cloud connection
 */
int disconnect_cb()
{
    if (cloud_socket)
        close(cloud_socket);
    return 1;
}

/**
 * This sends the data to the cloud server
 */
int send_cb_udp(const unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t sent = sendto(cloud_socket, (const char *)buf, buflen, 0, (struct sockaddr *)&cloud_addr, sizeof(cloud_addr));
    return (int)sent;
}

/**
 * This receives data from the socket and returns the number of bytes received
 * @return The number of bytes received.
 */
int receive_cb_udp(unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t res = recvfrom(cloud_socket, (char *)buf, buflen, 0, (struct sockaddr *)NULL, NULL);

    // on timeout error, set bytes received to 0
    if ((int)res < 0 && errno == 11)
    {
        res = 0;
    }

    return (int)res;
}

int main(int argc, char *argv[]) {

	trackle.setDeviceId(device_id);
    trackle.setKeys(private_key);

    // configurazione delle callback
    trackle.setMillis(getMillis);
    trackle.setSendCallback(send_cb_udp);
    trackle.setReceiveCallback(receive_cb_udp);
    trackle.setConnectCallback(connect_cb_udp);
    trackle.setDisconnectCallback(disconnect_cb);

    trackle.connect();

    while (1)
    {
        trackle.loop();
        usleep(20 * 1000);
    }

    return 0;
}
```


#### Getting started in C
Here is a very simple C example that create a client and connect it to Trackle cloud

```
#include "trackle_interface.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
...

unsigned char private_key[PRIVATE_KEY_LENGTH] = { ... };
static uint8_t device_id[DEVICE_ID_LENGTH] = { ... };

struct Trackle *trackle_s;
struct sockaddr_in cloud_addr;
int cloud_socket;

/*
* This function is used to get the current time in milliseconds.
*/
static system_tick_t getMillis(void)
{
	struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return (uint32_t)ms;
}

/**
 * This creates a socket and sets the timeout
 */
int connect_cb_udp(const char *address, int port)
{
    printf("Connecting socket");
    int addr_family;
    int ip_protocol;
    char addr_str[128];

    struct hostent *res = gethostbyname(address);
    if (res)
    {
        printf("Dns address %s resolved", address);
    }
    else
    {
        printf("error resolving gethostbyname %s resolved", address);
        return -1;
    }

    memcpy(&cloud_addr.sin_addr.s_addr, res->h_addr, sizeof(cloud_addr.sin_addr.s_addr));

    cloud_addr.sin_family = AF_INET;
    cloud_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(cloud_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    cloud_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (cloud_socket < 0)
    {
        printf("Unable to create socket: errno %d", errno);
    }
    printf("Socket created, sending to %s:%d", address, port);

    // setto i timeout di lettura/scrittura del socket
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 1000; // 1ms
    setsockopt(cloud_socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&socket_timeout, sizeof(struct timeval));

    return 1;
}

/**
 * This is a callback function that close the cloud connection
 */
int disconnect_cb()
{
    if (cloud_socket)
        close(cloud_socket);
    return 1;
}

/**
 * This sends the data to the cloud server
 */
int send_cb_udp(const unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t sent = sendto(cloud_socket, (const char *)buf, buflen, 0, (struct sockaddr *)&cloud_addr, sizeof(cloud_addr));
    return (int)sent;
}

/**
 * This receives data from the socket and returns the number of bytes received
 * @return The number of bytes received.
 */
int receive_cb_udp(unsigned char *buf, uint32_t buflen, void *tmp)
{
    size_t res = recvfrom(cloud_socket, (char *)buf, buflen, 0, (struct sockaddr *)NULL, NULL);

    // on timeout error, set bytes received to 0
    if ((int)res < 0 && errno == 11)
    {
        res = 0;
    }

    return (int)res;
}

int main(int argc, char *argv[]) {

    // dichiarazione della libreria
    trackle_s = newTrackle();

	trackleSetDeviceId(trackle_s, device_id);
    trackleSetKeys(trackle_s, private_key);

    // configurazione delle callback
    trackleSetMillis(trackle_s, getMillis);
    trackleSetSendCallback(trackle_s, send_cb_udp);
    trackleSetReceiveCallback(trackle_s, receive_cb_udp);
    trackleSetConnectCallback(trackle_s, connect_cb_udp);
    trackleSetDisconnectCallback(trackle_s, disconnect_cb);

    trackleConnect(trackle_s);

    while (1)
    {
        trackleLoop(trackle_s);
		usleep(20 * 1000);
    }

    return 0;
}
```

#### Trackle client

The minimal usage flow for Trackle client is as follows (C++ and C):

- **trackle.setDeviceId(device_id) - trackleSetDeviceId(trackle_s, device_id)**:
	set a deviceId to the client. Follow [Get a Device ID and a private key](#get-a-device-id-and-a-private-key) instructions.

- **trackle.setKeys(private_key) - trackleSetKeys(trackle_s, private_key)**:
	set a private key to the client. Follow [Get a Device ID and a private key](#get-a-device-id-and-a-private-key) instructions.

- **trackle.setMillis(getMillis) - trackleSetMillis(trackle_s, getMillis)**:
	configure a callback that return the number of milliseconds at the time, the esp32 begins running the current program

- **trackle.setSendCallback(send_cb_udp) - trackleSetSendCallback(trackle_s, send_cb_udp)**:
	configure a callback to write on udp cloud socket

- **trackle.setReceiveCallback(receive_cb_udp) - trackleSetReceiveCallback(trackle_s, receive_cb_udp)**:
	configure a callback to read the udp cloud socket

- **trackle.setConnectCallback(connect_cb_udp) - trackleSetConnectCallback(trackle_s, connect_cb_udp)**:
	configure a callback to connect the udp cloud socket

- **trackle.setDisconnectCallback(disconnect_cb) - trackleSetDisconnectCallback(trackle_s, disconnect_cb)**:
	configure a callback to disconnect the udp cloud socket

- **trackle.connect() - trackleConnect(trackle_s)**:
	start the cloud connection flow

- **trackle.loop() - trackleLoop(trackle_s)**:
	loop function, to keep the device connected to the cloud. Must be called as soon as possible
