#include "events.h"
#include <string.h>

// Private, used by two subscription variants below
uint8_t *subscription_prelude(uint8_t buf[], uint16_t message_id,
                              const char *event_name)
{
  uint8_t *p = buf;
  *p++ = 0x40; // confirmable, no token
  *p++ = 0x01; // code 0.01 GET request
  *p++ = message_id >> 8;
  *p++ = message_id & 0xff;
  *p++ = 0xb1; // one-byte Uri-Path option
  *p++ = 'e';

  if (NULL != event_name)
  {
    size_t len = strnlen(event_name, 63);
    p += event_name_uri_path(p, event_name, len);
  }

  return p;
}

size_t subscription(uint8_t buf[], uint16_t message_id,
                    const char *event_name, const char *device_id)
{
  uint8_t *p = subscription_prelude(buf, message_id, event_name);

  if (NULL != device_id)
  {
    size_t len = strnlen(device_id, 63);

    *p++ = 0xff;
    memcpy(p, device_id, len);
    p += len;
  }

  return p - buf;
}

size_t subscription(uint8_t buf[], uint16_t message_id,
                    const char *event_name, SubscriptionScope::Enum scope)
{
  uint8_t *p = subscription_prelude(buf, message_id, event_name);

  switch (scope)
  {
  case SubscriptionScope::MY_DEVICES:
    *p++ = 0x41; // one-byte Uri-Query option
    *p++ = 'u';
    break;
  case SubscriptionScope::FIREHOSE:
  default:
    // unfiltered firehose is not allowed
    if (NULL == event_name || 0 == *event_name)
    {
      return -1;
    }
  }

  return p - buf;
}

size_t event_name_uri_path(uint8_t buf[], const char *name, size_t name_len)
{
  if (0 == name_len)
  {
    return 0;
  }
  else if (name_len < 13)
  {
    buf[0] = name_len;
    memcpy(buf + 1, name, name_len);
    return name_len + 1;
  }
  else
  {
    buf[0] = 0x0d;
    buf[1] = name_len - 13;
    memcpy(buf + 2, name, name_len);
    return name_len + 2;
  }
}
