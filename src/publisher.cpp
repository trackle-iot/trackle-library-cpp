#include "publisher.h"

#include "protocol.h"

void trackle::protocol::Publisher::add_ack_handler(message_id_t msg_id, CompletionHandler handler)
{
    protocol->add_ack_handler(msg_id, std::move(handler), SEND_EVENT_ACK_TIMEOUT);
}
