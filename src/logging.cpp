#include "logging.h"

#include <algorithm>
#include <cstdio>
//#include "timer_hal.h"
#include "service_debug.h"
//#include "static_assert.h"

#include <sys/time.h>

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

} // namespace

static uint32_t getMillis(void)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return (uint32_t)ms;
}

typedef LogLevel LoggerOutputLevel; // Compatibility typedef
LoggerOutputLevel log_compat_level = DEFAULT_LEVEL;

typedef void (*debug_output_fn)(const char *);
debug_output_fn log_compat_callback = NULL;

void log_set_level(LogLevel level)
{
    log_compat_level = level;
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
        LOG_ATTR_SET(*attr, time, getMillis());
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
