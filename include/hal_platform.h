#ifndef HAL_PLATFORM_H
#define HAL_PLATFORM_H

#ifdef __cplusplus
extern "C"
{
#endif

#define SYSTEM_ERROR_COAP 100
#define SYSTEM_ERROR_COAP_4XX 400
#define SYSTEM_ERROR_COAP_5XX 500

//#define PRODUCT_ID 6
#define PRODUCT_FIRMWARE_VERSION 1

#if defined(_WIN64) || defined(_WIN32) || defined(__WINDOWS__) || defined(WIN32) || defined(WIN64)
#define PLATFORM_ID 100
#elif defined(__ARMEL__)
#define PLATFORM_ID 101
#elif defined(__APPLE__) || defined(__MACH__)
#define PLATFORM_ID 102
#elif defined(__linux)
#define PLATFORM_ID 103
#elif defined(unix) || defined(__unix__) || defined(__unix)
#define PLATFORM_ID 104
#elif defined(__posix)
#define PLATFORM_ID 105
#elif defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO_ESP32_DEV)
#define PLATFORM_ID 106
#else
#define PLATFORM_ID 120
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_PLATFORM_H */
