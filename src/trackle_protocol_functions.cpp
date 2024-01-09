#include "protocol_defs.h"
#include "protocol_selector.h"
#include "trackle_protocol_functions.h"
// #include "mbedtls/debug.h"
#include <stdlib.h>

using trackle::CompletionHandler;

/**
 * Handle the cryptographically secure random seed from the cloud by using
 * it to seed the stdlib PRNG.
 * @param seed  A random value from a cryptographically secure random number generator.
 */
void default_random_seed_from_cloud(unsigned int seed)
{
    srand(seed);
}

int trackle_protocol_to_system_error(int error_)
{
    return toSystemError(static_cast<trackle::protocol::ProtocolError>(error_));
}

#include "hal_platform.h"
#include "dtls_protocol.h"

void trackle_protocol_communications_handlers(ProtocolFacade *protocol, CommunicationsHandlers *handlers)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    protocol->set_handlers(*handlers);
}

void trackle_protocol_init(ProtocolFacade *protocol, const char *id,
                           const TrackleKeys &keys,
                           const TrackleCallbacks &callbacks,
                           const TrackleDescriptor &descriptor,
                           const trackle::protocol::Connection_Properties_Type &conPropType,
                           void *reserved)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    (void)reserved;
    protocol->init(id, keys, callbacks, descriptor, conPropType);
}

int trackle_protocol_handshake(ProtocolFacade *protocol, void *)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->begin();
}

bool trackle_protocol_event_loop(ProtocolFacade *protocol, void *)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->event_loop();
}

bool trackle_protocol_is_initialized(ProtocolFacade *protocol)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    return protocol->is_initialized();
}

int trackle_protocol_presence_announcement(ProtocolFacade *protocol, uint8_t *buf, const uint8_t *id, void *)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->presence_announcement(buf, id);
}

int trackle_protocol_post_description(ProtocolFacade *protocol, int desc_flags, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    (void)reserved;
    return protocol->post_description(desc_flags);
}

bool trackle_protocol_send_event(ProtocolFacade *protocol, const char *event_name, const char *data,
                                 int ttl, uint32_t flags, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    CompletionHandler handler;
    if (reserved)
    {
        auto r = static_cast<const trackle_protocol_send_event_data *>(reserved);
        handler = CompletionHandler(r->handler_callback, r->handler_data);
    }
    EventType::Enum event_type = EventType::extract_event_type(flags);
    return protocol->send_event(event_name, data, ttl, event_type, flags, std::move(handler));
}

bool trackle_protocol_send_event_in_blocks(ProtocolFacade *protocol,
                                 int ttl, uint32_t flags, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    CompletionHandler handler;
    if (reserved)
    {
        auto r = static_cast<const trackle_protocol_send_event_data *>(reserved);
        handler = CompletionHandler(r->handler_callback, r->handler_data);
    }
    EventType::Enum event_type = EventType::extract_event_type(flags);
    return protocol->send_event_in_blocks(ttl, event_type, flags, std::move(handler));
}


bool trackle_protocol_send_subscription_device(ProtocolFacade *protocol, const char *event_name, const char *device_id, void *)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->send_subscription(event_name, device_id);
}

bool trackle_protocol_send_subscription_scope(ProtocolFacade *protocol, const char *event_name, SubscriptionScope::Enum scope, void *)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->send_subscription(event_name, scope);
}

bool trackle_protocol_add_event_handler(ProtocolFacade *protocol, const char *event_name,
                                        EventHandler handler, SubscriptionScope::Enum scope, const char *device_id, void *handler_data)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    return protocol->add_event_handler(event_name, handler, handler_data, scope, device_id);
}

bool trackle_protocol_send_time_request(ProtocolFacade *protocol, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    (void)reserved;
    return protocol->send_time_request();
}

void trackle_protocol_send_subscriptions(ProtocolFacade *protocol, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    (void)reserved;
    protocol->send_subscriptions();
}

void trackle_protocol_remove_event_handlers(ProtocolFacade *protocol, const char *event_name, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    (void)reserved;
    protocol->remove_event_handlers(event_name);
}

void trackle_protocol_set_product_id(ProtocolFacade *protocol, product_id_t product_id, unsigned, void *)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    protocol->set_product_id(product_id);
}

void trackle_protocol_set_product_firmware_version(ProtocolFacade *protocol, product_firmware_version_t product_firmware_version, unsigned, void *)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    protocol->set_product_firmware_version(product_firmware_version);
}

void trackle_protocol_get_product_details(ProtocolFacade *protocol, product_details_t *details, void *reserved)
{
    ASSERT_ON_SYSTEM_OR_MAIN_THREAD();
    (void)reserved;
    protocol->get_product_details(*details);
}

/*int trackle_protocol_set_connection_property(ProtocolFacade *protocol, unsigned property_id,
                                           unsigned data, trackle::protocol::connection_properties_t *conn_prop, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    if (property_id == trackle::protocol::Connection::PING)
    {
        protocol->set_keepalive(data, conn_prop->keepalive_source);
    }
    else if (property_id == trackle::protocol::Connection::FAST_OTA)
    {
        protocol->set_fast_ota(data);
    }
    return 0;
}*/

int trackle_protocol_command(ProtocolFacade *protocol, ProtocolCommands::Enum cmd, uint32_t data, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->command(cmd, data);
}

bool trackle_protocol_time_request_pending(ProtocolFacade *protocol, void *reserved)
{
    (void)reserved;
    return protocol->time_request_pending();
}
system_tick_t trackle_protocol_time_last_synced(ProtocolFacade *protocol, time_t *tm, void *reserved)
{
    (void)reserved;
    return protocol->time_last_synced(tm);
}

int trackle_protocol_get_describe_data(ProtocolFacade *protocol, trackle_protocol_describe_data *data, void *reserved)
{
    return protocol->get_describe_data(data, reserved);
}

int trackle_protocol_get_status(ProtocolFacade *protocol, protocol_status *status, void *reserved)
{
    ASSERT_ON_SYSTEM_THREAD();
    return protocol->get_status(status);
}
