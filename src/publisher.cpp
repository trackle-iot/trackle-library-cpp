/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2022 IOTREADY S.r.l. All rights reserved.
 *
 * This source code is licensed under the Trackle Source-Available License
 * Agreement found in the LICENSE file in the root directory of this source tree.
 * Commercial deployment requires one paid Device License Key per device.
 */

#include "publisher.h"

#include "protocol.h"

void trackle::protocol::Publisher::add_ack_handler(message_id_t msg_id, CompletionHandler handler)
{
    protocol->add_ack_handler(msg_id, std::move(handler), SEND_EVENT_ACK_TIMEOUT);
}
