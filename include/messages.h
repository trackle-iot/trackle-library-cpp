/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.
  Copyright (c) 2015 Particle Industries, Inc.

  This library is free software; you can redistribute it and/or
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

#pragma once

#include <string>
#include "coap.h"
#include "protocol_defs.h"
#include "events.h"
#include "defines.h"

#define MAX_BLOCK_SIZE 1024
#define MAX_BLOCKS_NUMBER 5

namespace trackle
{
    namespace protocol
    {

        inline uint32_t decode_uint32(unsigned char *buf)
        {
            return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
        }

        inline uint16_t decode_uint16(unsigned char *buf)
        {
            return buf[0] << 8 | buf[1];
        }

        inline uint8_t decode_uint8(unsigned char *buf)
        {
            return buf[0];
        }

#define RESPONSE_CODE(x, y) (x << 5 | y)

        class Messages
        {
        public:

            // ----- BEGIN static fields for multiblock transfer status ------
            static uint8_t blocksBuffer[MAX_BLOCK_SIZE * MAX_BLOCKS_NUMBER]; // Bytes of the message (both sent and not sent)
            static size_t totBytesNumber; // Total number of bytes in the message
            static bool blockTransmissionRunning; // 
            static size_t currBlockIndex; // Sequence number of the current block
            static uint16_t currentToken; // Current token (for future applications)
            static std::string currEventName; // Current event name of the multiblock packet
            static int ttl; // Time to live
            static uint32_t flags; // Flags
            static publishCompletionCallback* completionCb; // Callback called on last block
            // ------ END static fields for multiblock transfer status -------

            static CoAPMessageType::Enum decodeType(const uint8_t *buf, size_t length);
            static size_t describe_post_header(uint8_t buf[], size_t buffer_size, uint16_t message_id, uint8_t desc_flags);
            static size_t hello(uint8_t *buf, message_id_t message_id, uint8_t flags,
                                uint16_t platform_id, uint16_t product_id,
                                uint16_t product_firmware_version, bool confirmable, const uint8_t *device_id, uint16_t device_id_len);

            static size_t update_done(uint8_t *buf, message_id_t message_id, bool confirmable);
            static size_t update_done(uint8_t *buf, message_id_t message_id, const uint8_t *result, size_t result_len, bool confirmable);

            static const size_t function_return_size = 10;

            static size_t function_return(unsigned char *buf, message_id_t message_id, token_t token, int return_value, bool confirmable);

            static size_t variable_value(unsigned char *buf, message_id_t message_id, token_t token, bool return_value);

            static size_t variable_value(unsigned char *buf, message_id_t message_id, token_t token, int32_t return_value);

            static size_t variable_value(unsigned char *buf, message_id_t message_id, token_t token, int64_t return_value); // actually, only 48 bits are transmitted (the sign-extension behavior is up to the server)

            static size_t variable_value(unsigned char *buf, message_id_t message_id, token_t token, double return_value);

            // Returns the length of the buffer to send
            static size_t variable_value(unsigned char *buf, message_id_t message_id, token_t token, const void *return_value, int length);

            static size_t time_request(uint8_t *buf, uint16_t message_id, uint8_t token);

            static size_t chunk_missed(uint8_t *buf, uint16_t message_id, chunk_index_t chunk_index);

            static size_t content(uint8_t *buf, uint16_t message_id, uint8_t token);

            static size_t ping(uint8_t *buf, uint16_t message_id);
            static size_t keep_alive(uint8_t *buf);

            static size_t presence_announcement(unsigned char *buf, const char *id);

            static size_t separate_response_with_payload(unsigned char *buf, uint16_t message_id,
                                                         unsigned char token, unsigned char code, unsigned char *payload,
                                                         unsigned payload_len, bool confirmable);

            static size_t event(uint8_t buf[], uint16_t message_id, const char *event_name,
                                const char *data, int ttl, EventType::Enum event_type, bool confirmable);

            static size_t event_in_blocks(uint8_t buf[], uint16_t message_id, int ttl, EventType::Enum event_type);

            static inline size_t empty_ack(unsigned char *buf,
                                           unsigned char message_id_msb,
                                           unsigned char message_id_lsb)
            {
                return coded_ack(buf, 0, message_id_msb, message_id_lsb);
            };

            static inline size_t coded_ack(unsigned char *buf,
                                           unsigned char code,
                                           unsigned char message_id_msb,
                                           unsigned char message_id_lsb)
            {
                buf[0] = 0x60; // acknowledgment, no token
                buf[1] = code;
                buf[2] = message_id_msb;
                buf[3] = message_id_lsb;
                return 4;
            }

            static inline size_t coded_ack(unsigned char *buf,
                                           unsigned char token,
                                           unsigned char code,
                                           unsigned char message_id_msb,
                                           unsigned char message_id_lsb)
            {
                buf[0] = 0x61; // acknowledgment, one-byte token
                buf[1] = code;
                buf[2] = message_id_msb;
                buf[3] = message_id_lsb;
                buf[4] = token;
                return 5;
            }

            static size_t coded_ack(uint8_t *buf,
                                    uint8_t token,
                                    uint8_t code,
                                    uint8_t message_id_msb,
                                    uint8_t message_id_lsb,
                                    uint8_t *data,
                                    size_t data_len);

            static inline size_t reset(unsigned char *buf,
                                       unsigned char message_id_msb,
                                       unsigned char message_id_lsb)
            {
                buf[0] = 0x70; // reset, no token
                buf[1] = 0;
                buf[2] = message_id_msb;
                buf[3] = message_id_lsb;
                return 4;
            }

            static inline size_t update_ready(unsigned char *buf, message_id_t message_id, token_t token, uint8_t flags, bool confirmable)
            {
                return separate_response_with_payload(buf, message_id, token, 0x44, &flags, 1, confirmable);
            }

            static inline size_t chunk_received(unsigned char *buf, message_id_t message_id, token_t token, ChunkReceivedCode::Enum code, bool confirmable)
            {
                return separate_response(buf, message_id, token, code, confirmable);
            }

            static inline size_t separate_response(unsigned char *buf, message_id_t message_id,
                                                   unsigned char token, unsigned char code, bool confirmable)
            {
                return separate_response_with_payload(buf, message_id, token, code, NULL, 0, confirmable);
            }

            static inline size_t description(unsigned char *buf, message_id_t message_id, token_t token)
            {
                return content(buf, message_id, token);
            }
        };

    }
}
