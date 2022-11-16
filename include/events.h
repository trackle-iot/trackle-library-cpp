#ifndef __EVENTS_H
#define __EVENTS_H

#include <stdint.h>
#include <stdlib.h>

namespace EventType
{
  enum Enum : char
  {
    PUBLIC = 'e',  // 0x65
    PRIVATE = 'E', // 0x45
  };

  /**
   * These flags are encoded into the same 32-bit integer that already holds EventType::Enum
   */
  enum Flags
  {
    EMPTY_FLAGS = 0,
    NO_ACK = 0x2,
    WITH_ACK = 0x8,
    ASYNC = 0x10, // not used here, but reserved since it's used in the system layer. Makes conversion simpler.
    ALL_FLAGS = NO_ACK | WITH_ACK | ASYNC
  };

  static_assert((PUBLIC & NO_ACK) == 0 &&
                    (PRIVATE & NO_ACK) == 0 &&
                    (PUBLIC & WITH_ACK) == 0 &&
                    (PRIVATE & WITH_ACK) == 0 &&
                    (PRIVATE & ASYNC) == 0 &&
                    (PUBLIC & ASYNC) == 0,
                "flags should be distinct from event type");

  /**
   * The flags are encoded in with the event type.
   */
  inline Enum extract_event_type(uint32_t &value)
  {
    Enum et = Enum(value & ~ALL_FLAGS);
    value = value & ALL_FLAGS;
    return et;
  }
} // namespace EventType

#if PLATFORM_ID != 3 && PLATFORM_ID != 20
static_assert(sizeof(EventType::Enum) == 1, "EventType size is 1");
#endif

namespace SubscriptionScope
{
  enum Enum
  {
    MY_DEVICES,
    FIREHOSE
  };
}

typedef void (*EventHandler)(const char *event_name, const char *data);
typedef void (*EventHandlerWithData)(void *handler_data, const char *event_name, const char *data);

/**
 *  This is used in a callback so only change by adding fields to the end
 */
struct FilteringEventHandler
{
  char filter[64];
  EventHandler handler;
  void *handler_data;
  SubscriptionScope::Enum scope;
  char device_id[13];
};

size_t subscription(uint8_t buf[], uint16_t message_id,
                    const char *event_name, const char *device_id);

size_t subscription(uint8_t buf[], uint16_t message_id,
                    const char *event_name, SubscriptionScope::Enum scope);

size_t event_name_uri_path(uint8_t buf[], const char *name, size_t name_len);

#endif // __EVENTS_H
