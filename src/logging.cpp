#include "logging.h"

#include <algorithm>
#include <cstdio>
// #include "timer_hal.h"
#include "service_debug.h"
// #include "static_assert.h"

#include <sys/time.h>
#include "dtls_debug.h"

#include "tinydtls.h"

#define STATIC_ASSERT_FIELD_SIZE(struct, field, size) \
    STATIC_ASSERT(field_size_changed_##struct##_##field, sizeof(struct ::field) == size);

#define STATIC_ASSERT_FIELD_OFFSET(struct, field, offset) \
    STATIC_ASSERT(field_offset_changed_##struct##_##field, offsetof(struct, field) == offset);

#define STATIC_ASSERT_FIELD_ORDER(struct, field1, field2)                                                                                \
    STATIC_ASSERT(field_offset_changed_##struct##_##field2,                                                                              \
                  offsetof(struct, field2) == offsetof(struct, field1) + sizeof(struct ::field1) + /* Padding */                         \
                                                  (__alignof__(struct ::field2) - (offsetof(struct, field1) + sizeof(struct ::field1)) % \
                                                                                      __alignof__(struct ::field2)) %                    \
                                                      __alignof__(struct ::field2));

namespace
{

    volatile log_message_callback_type log_msg_callback = 0;
    volatile log_write_callback_type log_write_callback = 0;
    volatile log_enabled_callback_type log_enabled_callback = 0;
    system_tick_t (*getMillis)() = NULL;
} // namespace

typedef LogLevel LoggerOutputLevel; // Compatibility typedef
LoggerOutputLevel log_compat_level = DEFAULT_LEVEL;

typedef void (*debug_output_fn)(const char *);
debug_output_fn log_compat_callback = NULL;

void log_set_level(LogLevel level)
{
    log_compat_level = level;
}

void log_set_millis_callback(millisCallback *millis)
{
    getMillis = millis;
}

void log_set_callbacks(log_message_callback_type log_msg, log_write_callback_type log_write,
                       log_enabled_callback_type log_enabled, void *reserved)
{
    log_msg_callback = log_msg;
    log_write_callback = log_write;
    log_enabled_callback = log_enabled;
}

void log_message_v(int level, const char *category, LogAttributes *attr, void *reserved, const char *fmt, va_list args)
{
    const log_message_callback_type msg_callback = log_msg_callback;

    if (!msg_callback)
    {
        return;
    }

    if (level < log_compat_level)
    {
        return;
    }

    // Set default attributes
    if (!attr->has_time)
    {
        LOG_ATTR_SET(*attr, time, (*getMillis)());
    }
    char buf[LOG_MAX_STRING_LENGTH];
    if (msg_callback)
    {
        const int n = vsnprintf(buf, sizeof(buf), fmt, args);
        if (n > (int)sizeof(buf) - 1)
        {
            buf[sizeof(buf) - 2] = '~';
        }
        msg_callback(buf, level, category, attr, 0);
    }
    else
    {
#if 0
        // Using compatibility callback
        const char* const levelName = log_level_name(level, 0);
        int n = 0;
        if (attr->has_file && attr->has_line && attr->has_function) {
            n = snprintf(buf, sizeof(buf), "%010u %s:%d, %s: %s", (unsigned)attr->time, attr->file, attr->line,
                    attr->function, levelName);
        } else {
            n = snprintf(buf, sizeof(buf), "%010u %s", (unsigned)attr->time, levelName);
        }
        if (n > (int)sizeof(buf) - 1) {
            buf[sizeof(buf) - 2] = '~';
        }
        log_compat_callback(buf);
        log_compat_callback(": ");
        n = vsnprintf(buf, sizeof(buf), fmt, args);
        if (n > (int)sizeof(buf) - 1) {
            buf[sizeof(buf) - 2] = '~';
        }
        log_compat_callback(buf);
        log_compat_callback("\r\n");
#endif
    }
}

void log_message(int level, const char *category, LogAttributes *attr, void *reserved, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_message_v(level, category, attr, reserved, fmt, args);
    va_end(args);
}

void log_write(int level, const char *category, const char *data, size_t size, void *reserved)
{
    if (!size)
    {
        return;
    }
    const log_write_callback_type write_callback = log_write_callback;
    if (write_callback)
    {
        write_callback(data, size, level, category, 0);
    }
    else if (log_compat_callback && level >= log_compat_level)
    {
#if 0
        // Compatibility callback expects null-terminated strings
        if (!data[size - 1]) {
            log_compat_callback(data);
        } else {
            char buf[LOG_MAX_STRING_LENGTH];
            size_t offs = 0;
            do {
                const size_t n = std::min(size - offs, sizeof(buf) - 1);
                memcpy(buf, data + offs, n);
                buf[n] = 0;
                log_compat_callback(buf);
                offs += n;
            } while (offs < size);
        }
#endif
    }
}

void log_printf_v(int level, const char *category, void *reserved, const char *fmt, va_list args)
{
    const log_write_callback_type write_callback = log_write_callback;
    if (!write_callback && (!log_compat_callback || level < log_compat_level))
    {
        return;
    }

    char buf[LOG_MAX_STRING_LENGTH];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    if (n > (int)sizeof(buf) - 1)
    {
        buf[sizeof(buf) - 2] = '~';
        n = sizeof(buf) - 1;
    }
    if (write_callback)
    {
        write_callback(buf, n, level, category, 0);
    }
    else
    {
        log_compat_callback(buf); // Compatibility callback
    }
}

void log_printf_a(int level, const char *category, void *reserved, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_printf_v(level, category, reserved, fmt, args);
    va_end(args);
}

void log_dump(int level, const char *category, const void *data, size_t size, int flags, void *reserved)
{
    const log_write_callback_type write_callback = log_write_callback;
    if (!size || (!write_callback && (!log_compat_callback || level < log_compat_level)))
    {
        return;
    }
    static const char hex[] = "0123456789abcdef";
    char buf[LOG_MAX_STRING_LENGTH / 2 * 2 + 1]; // Hex data is flushed in chunks
    buf[sizeof(buf) - 1] = 0;                    // Compatibility callback expects null-terminated strings
    size_t offs = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const uint8_t b = ((const uint8_t *)data)[i];
        buf[offs++] = hex[b >> 4];
        buf[offs++] = hex[b & 0x0f];
        if (offs == sizeof(buf) - 1)
        {
            if (write_callback)
            {
                write_callback(buf, sizeof(buf) - 1, level, category, 0);
            }
            else
            {
                log_compat_callback(buf);
            }
            offs = 0;
        }
    }
    if (offs)
    {
        if (write_callback)
        {
            write_callback(buf, offs, level, category, 0);
        }
        else
        {
            buf[offs] = 0;
            log_compat_callback(buf);
        }
    }
}

int log_enabled(int level, const char *category, void *reserved)
{
    const log_enabled_callback_type enabled_callback = log_enabled_callback;
    if (enabled_callback)
    {
        return enabled_callback(level, category, 0);
    }
    if (log_compat_callback && level >= log_compat_level)
    { // Compatibility callback
        return 1;
    }
    return 0;
}

const char *log_level_name(int level, void *reserved)
{
    static const char *const names[] = {
        "TRACE",
        "TRACE", // LOG (deprecated)
        "TRACE", // DEBUG (deprecated)
        "INFO",
        "WARN",
        "ERROR",
        "PANIC"};
    const int i = std::max(0, std::min<int>(level / 10, sizeof(names) / sizeof(names[0]) - 1));
    return names[i];
}

// ------------------------------------------ TINYDTLS LOGGING -------------------------------------------------

/**
 * @brief Map log level from TinyDTLS to Trackle Library
 * Mapping performed is best-effort, since there isn't a one-to-one relationships between levels in TinyDTLS and in Trackle Library.
 * @param level TinyDTLS log level
 * @return int Trackle Library log level
 */
static int LogLevel_tinyDtlsToTrackleLib(int level) {
    switch (level)
    {
    case DTLS_LOG_EMERG:
        return LOG_LEVEL_PANIC;
    case DTLS_LOG_ALERT:
    case DTLS_LOG_CRIT:
        return LOG_LEVEL_ERROR;
    case DTLS_LOG_WARN:
    case DTLS_LOG_NOTICE:
        return LOG_LEVEL_WARN;
    case DTLS_LOG_INFO:
    case DTLS_LOG_DEBUG:
        return LOG_LEVEL_INFO;
    }
    return LOG_LEVEL_INFO;
}

/**
 * @brief Map log level from Trackle Library to TinyDTLS
 * Mapping performed is best-effort, since there isn't a one-to-one relationships between levels in TinyDTLS and in Trackle Library.
 * @param level Trackle Library log level
 * @return int TinyDTLS log level
 */
static int LogLevel_trackleLibToTinyDtls(int level) {
    switch (level)
    {
    case LOG_LEVEL_NONE:
    case LOG_LEVEL_PANIC:
        return DTLS_LOG_EMERG;
    case LOG_LEVEL_ERROR:
        return DTLS_LOG_CRIT;
    case LOG_LEVEL_WARN:
        return DTLS_LOG_NOTICE;
    case LOG_LEVEL_INFO:
    case LOG_LEVEL_TRACE:
        return DTLS_LOG_DEBUG;
    }
    return DTLS_LOG_INFO;
}

/**
 * Discard any log printed with this function.
 * This is meant to be used as default function for \ref latest_log_callback.
 * This is done so that \ref latest_log_callback is never invalid, and calling it before set would simply discard the message.
 */
static void discard_log_callback(const char *msg, int level, const char *category, void *attributes, void *reserved)
{
    // Discard log.
}

/**
 * Pointer to the latest log callback function set on a Trackle class.
 * Please note that, since Trackle instances can be many and tinydtls has only one instance, it doesn't matter which Trackle instance set the callback.
 */
static void (*latest_log_callback)(const char *msg, int level, const char *category, void *attributes, void *reserved) = discard_log_callback;

/**
 * Latest log level set on a Trackle class.
 * Please note that, since Trackle instances can be many and tinydtls has only one instance, it doesn't matter which Trackle instance set the log level.
 */
static int latest_log_level = LOG_LEVEL_PANIC;

/**
 * @brief Callback for logging to be passed to tinyDTLS.
 * It performs conversion of logging levels between tinyDTLS and Trackle library, and forwards tinyDTLS logs to log callback set for Trackle library.
 *
 * @param tinydtlsLogLevel Level of the log as given by tinyDTLS
 * @param format Format string of log message (in printf format)
 * @param ... Parameters
 */
void TrackleLib_tinydtls_log_wrapper(unsigned int tinydtlsLogLevel, const char *format, ...)
{
    // Convert tinyDTLS log levels to Trackle library log levels
    const unsigned int tracklelibLogLevel = LogLevel_tinyDtlsToTrackleLib(tinydtlsLogLevel);

    // Build message from format string and parameters
    va_list args;
    char builtMsg[256] = {0};
    va_start(args, format);
    vsprintf(builtMsg, format, args);
    va_end(args);

    // Call set callback to display log message
    latest_log_callback(builtMsg, tracklelibLogLevel, "", NULL, NULL);
}

/**
 * Set latest log callback to be used by tinydtls.
 * @param new_latest_log_callback
 */
void TrackleLib_set_latest_log_callback_for_tinydtls(void (*new_latest_log_callback)(const char *, int, const char *, void *, void *))
{
    latest_log_callback = new_latest_log_callback;
}

/**
 * Set latest log level to tinydtls.
 * @param new_latest_log_level
 */
void TrackleLib_set_latest_log_level_for_tinydtls(int new_latest_log_level)
{
    latest_log_level = new_latest_log_level;
    const unsigned int tinyDtlsLogLevel = LogLevel_trackleLibToTinyDtls(new_latest_log_level);
    TinyDtls_set_log_level(tinyDtlsLogLevel);
}
