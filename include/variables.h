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

#include <string.h>
#include "protocol_defs.h"
#include "message_channel.h"
#include "messages.h"
#include "trackle_descriptor.h"
#include "logging.h"

namespace trackle
{
    namespace protocol
    {

        class Variables
        {

        public:
            ProtocolError decode_variable_request(char variable_key[MAX_VARIABLE_KEY_LENGTH + 1], char variable_arg[MAX_VARIABLE_ARG_LENGTH + 1], Message &message, bool &has_block2, uint8_t &block2_num)
            {
                uint8_t *queue = message.buf();
                uint8_t queue_offset = 8;

                // copy the variable key
                size_t variable_key_length;
                if (queue[7] == 0x0d)
                {
                    variable_key_length = 0x0d + (queue[8] & 0xFF);
                    queue_offset++;
                }
                else
                {
                    variable_key_length = queue[7] & 0x0F;
                }
                if (variable_key_length > MAX_VARIABLE_KEY_LENGTH)
                {
                    variable_key_length = MAX_VARIABLE_KEY_LENGTH;
                }

                memcpy(variable_key, queue + queue_offset, variable_key_length);
                memset(variable_key + variable_key_length, 0, MAX_VARIABLE_KEY_LENGTH + 1 - variable_key_length);

                // read arguments
                size_t q_index = queue_offset + variable_key_length;
                size_t opt_idx = q_index; // Will point to options start

                // Initialize argument and Block2 option values
                variable_arg[0] = 0; // null terminate string (default: no args)
                has_block2 = false;
                block2_num = 0;

                // Helper function to decode extended option value (delta or length)
                auto decode_extended = [&](uint8_t value) -> uint16_t {
                    if (value == 13)
                    {
                        opt_idx++;
                        if (opt_idx >= message.length())
                            return 0;
                        return 13 + queue[opt_idx];
                    }
                    else if (value == 14)
                    {
                        opt_idx++;
                        if (opt_idx + 1 >= message.length())
                            return 0;
                        uint16_t result = 269 + (queue[opt_idx] << 8 | queue[opt_idx + 1]);
                        opt_idx++;
                        return result;
                    }
                    return value;
                };

                // Parse options to find Uri-Query (option 15) and Block2 (option 23)
                uint16_t prev_opt_num = 11; // Uri-Path is option 11
                bool found_uri_query = false;

                while (opt_idx < message.length() && queue[opt_idx] != 0xFF)
                {
                    uint8_t opt_byte = queue[opt_idx];
                    uint16_t opt_delta = (opt_byte >> 4) & 0x0F;
                    uint16_t opt_length = opt_byte & 0x0F;

                    // Handle extended delta and length
                    opt_delta = decode_extended(opt_delta);
                    if (opt_idx >= message.length())
                        break;

                    opt_length = decode_extended(opt_length);
                    if (opt_idx >= message.length())
                        break;

                    // Calculate cumulative option number
                    uint16_t option_num = prev_opt_num + opt_delta;
                    prev_opt_num = option_num;

                    opt_idx++; // Skip option header

                    // Check if this is Uri-Query option (option number 15)
                    if (option_num == 15 && !found_uri_query)
                    {
                        found_uri_query = true;
                        // Extract argument from Uri-Query value
                        if (opt_length > 0 && opt_length <= MAX_VARIABLE_ARG_LENGTH && 
                            opt_idx + opt_length <= message.length())
                        {
                            memcpy(variable_arg, queue + opt_idx, opt_length);
                            variable_arg[opt_length] = 0; // null terminate string
                        }
                        // Continue parsing to find Block2 if present
                    }
                    // Check if this is Block2 option (option number 23)
                    else if (option_num == 23)
                    {
                        has_block2 = true;
                        if (opt_length >= 2)
                        {
                            uint16_t block2_val = (queue[opt_idx] << 8) | queue[opt_idx + 1];
                            block2_num = (block2_val >> 4) & 0x0FFF;
                        }
                        else if (opt_length == 1)
                        {
                            // Handle Block2 with length=1 (non-standard)
                            block2_num = (queue[opt_idx] >> 4) & 0x0F;
                        }
                        // Continue parsing in case there are more options (though unlikely after Block2)
                    }

                    opt_idx += opt_length; // Skip option value
                }

                return NO_ERROR;
            }

            ProtocolError handle_variable_request(char *variable_key, char *variable_arg, Message &message, MessageChannel &channel, token_t token, message_id_t message_id,
                                                  TrackleReturnType::Enum (*variable_type)(const char *variable_key),
                                                  const void *(*get_variable)(const char *variable_key))
            {
                bool has_block2 = false;
                uint8_t block2_num = 0;

                ProtocolError err = decode_variable_request(variable_key, variable_arg, message, has_block2, block2_num);

                // args too long, send error 400
                if (err == IO_ERROR_GENERIC_RECEIVE)
                {
                    LOG(WARN, "GET variable request failed: variable '%s' arguments too long (max %zu bytes), returning 400 Bad Request", variable_key, MAX_VARIABLE_ARG_LENGTH);
                    Message response;
                    channel.response(message, response, 16);
                    size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 0), 0, 0);
                    response.set_id(message_id);
                    response.set_length(response_length);
                    return channel.send(response);
                }

                uint8_t *queue = message.buf();
                message.set_id(message_id);

            // get variable value according to type using the descriptor
            TrackleReturnType::Enum var_type = variable_type(variable_key);
            const void *var_fn = get_variable(variable_key);
            if (!var_fn)
            {
                LOG(WARN, "GET variable request failed: variable '%s' not found, returning 404 Not Found", variable_key);
                Message response;
                channel.response(message, response, 16);
                size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 4), 0, 0); // NOT_FOUND
                response.set_id(message_id);
                response.set_length(response_length);
                return channel.send(response);
            }

            // Handle non-string types (BOOLEAN, INT, DOUBLE) - simple response
                if (TrackleReturnType::BOOLEAN == var_type)
                {
                    const bool result = ((user_variable_bool_cb_t)(var_fn))(variable_arg, variable_key);
                    size_t response = Messages::variable_value(queue, message_id, token, result);
                    message.set_length(response);
                    return channel.send(message);
                }
                else if (TrackleReturnType::INT == var_type)
                {
                    const int32_t result = ((user_variable_int32_cb_t)(var_fn))(variable_arg, variable_key);
                    size_t response = Messages::variable_value(queue, message_id, token, result);
                    message.set_length(response);
                    return channel.send(message);
                }
                else if (TrackleReturnType::DOUBLE == var_type)
                {
                    const double result = ((user_variable_double_cb_t)(var_fn))(variable_arg, variable_key);
                    size_t response = Messages::variable_value(queue, message_id, token, result);
                    message.set_length(response);
                    return channel.send(message);
                }
                else if (TrackleReturnType::STRING == var_type || TrackleReturnType::JSON == var_type)
                {
                    block_messages_data *block = NULL;
                    const char *str_val = NULL;
                    size_t str_length = 0;
                    bool is_first_request = (!has_block2 || block2_num == 0);

                    if (is_first_request)
                    {
                        // First request - get variable value from callback
                        str_val = ((user_variable_char_cb_t)(var_fn))(variable_arg, variable_key);
                        if (!str_val)
                        {
                            LOG(WARN, "GET variable request failed: variable '%s' callback returned NULL, returning 404 Not Found", variable_key);
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 4), 0, 0); // NOT_FOUND
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }

                        str_length = strlen(str_val);

                        // If value fits in single response, send simple response
                        if (str_length <= MAX_BLOCK_SIZE)
                        {
                            size_t response = Messages::variable_value(queue, message_id, token, str_val, str_length);
                            message.set_length(response);
                            return channel.send(message);
                        }

                        // Verify if data fits in buffer (like event() in trackle.cpp)
                        if (str_length > MAX_BLOCK_SIZE * BLOCKS_NUMBER)
                        {
                            LOG(WARN, "GET variable request failed: variable '%s' value too large (%zu bytes, max %zu bytes), returning 413 Request Entity Too Large", variable_key, str_length, MAX_BLOCK_SIZE * BLOCKS_NUMBER);
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 13), 0, 0); // REQUEST_ENTITY_TOO_LARGE
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }

                        // Large value - allocate block structure
                        block = trackle_get_free_block();
                        if (!block)
                        {
                            LOG(WARN, "GET variable request failed: variable '%s' no free block available for block transfer, returning 503 Service Unavailable", variable_key);
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(5, 3), 0, 0); // SERVICE_UNAVAILABLE
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }

                        // Initialize block structure
                        block->transmissionRunning = true;
                        block->token = token;
                        block->msg_key = 0;
                        block->currBlockIndex = 0;
                        block->lastBlockSentTime = 0; // Will be set when first block is sent
                        block->eventName = std::string(variable_key);
                        if (variable_arg[0])
                        {
                            block->eventName += '\0';
                            block->eventName += variable_arg;
                        }

                        // Copy from 2nd block to buffer (like event() in trackle.cpp)
                        // totBytesNumber starts from 2nd block, totBlockNumber is total blocks
                        memcpy(block->buffer, str_val + MAX_BLOCK_SIZE, str_length - MAX_BLOCK_SIZE);
                        block->totBytesNumber = str_length - MAX_BLOCK_SIZE;
                        block->totBlockNumber = (str_length + MAX_BLOCK_SIZE - 1) / MAX_BLOCK_SIZE;
                    }
                    else
                    {
                        // Subsequent block request
                        block = trackle_get_block_by_token(token);
                        if (!block || !block->transmissionRunning)
                        {
                            LOG(WARN, "GET variable request failed: block transfer for variable '%s' not found or not running (token: %u, block: %u), returning 408 Request Entity Incomplete", variable_key, token, block2_num);
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 8), 0, 0); // REQUEST_ENTITY_INCOMPLETE
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }

                        block->currBlockIndex = block2_num;

                        // Verify buffer is long enough for this block (like protocol.cpp)
                        size_t buffer_size = MAX_BLOCK_SIZE * (BLOCKS_NUMBER - 1);
                        size_t required_offset = block2_num * MAX_BLOCK_SIZE - MAX_BLOCK_SIZE;
                        if (required_offset >= buffer_size)
                        {
                            // Block requested is beyond buffer - send error
                            LOG(WARN, "GET variable request failed: variable '%s' requested block %u beyond buffer size (offset: %zu, buffer: %zu), returning 413 Request Entity Too Large", variable_key, block2_num, required_offset, buffer_size);
                            block->transmissionRunning = false;
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 13), 0, 0); // REQUEST_ENTITY_TOO_LARGE
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }

                        str_length = block->totBytesNumber + MAX_BLOCK_SIZE; // Total length = buffer + first block
                    }

                    // Prepare block data (like protocol.cpp)
                    const char *block_data = NULL;
                    size_t block_offset = 0;

                    if (block->currBlockIndex == 0)
                    {
                        // First block - use str_val directly
                        if (!str_val)
                        {
                            LOG(WARN, "GET variable request failed: variable '%s' str_val is NULL for first block (block: %u), returning 404 Not Found", variable_key, block->currBlockIndex);
                            block->transmissionRunning = false;
                            Message response;
                            channel.response(message, response, 16);
                            size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 4), 0, 0); // NOT_FOUND
                            response.set_id(message_id);
                            response.set_length(response_length);
                            return channel.send(response);
                        }
                        block_data = str_val;
                        block_offset = 0;
                    }
                    else
                    {
                        // Subsequent block - data is in buffer (buffer starts from 2nd block)
                        block_data = reinterpret_cast<const char *>(block->buffer);
                        block_offset = block->currBlockIndex * MAX_BLOCK_SIZE - MAX_BLOCK_SIZE;
                    }

                    // Validate block number (remember: totBytesNumber starts from 2nd block)
                    if (block->currBlockIndex * MAX_BLOCK_SIZE >= (block->totBytesNumber + MAX_BLOCK_SIZE))
                    {
                        LOG(WARN, "GET variable request failed: variable '%s' invalid block number %u (total blocks: %u, total bytes: %zu), returning 400 Bad Request", variable_key, block->currBlockIndex, block->totBlockNumber, block->totBytesNumber + MAX_BLOCK_SIZE);
                        block->transmissionRunning = false;
                        Message response;
                        channel.response(message, response, 16);
                        size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 0), 0, 0); // BAD_REQUEST
                        response.set_id(message_id);
                        response.set_length(response_length);
                        return channel.send(response);
                    }

                    // Send block with Block2 option
                    Message response_msg;
                    channel.create(response_msg);
                    uint8_t *buf = response_msg.buf();

                    // Build response header
                    buf[0] = 0x61; // ACK, one-byte token
                    buf[1] = 0x45; // 2.05 CONTENT
                    buf[2] = message_id >> 8;
                    buf[3] = message_id & 0xFF;
                    buf[4] = token;

                    size_t pos = 5;

                    // Add Block2 option
                    buf[pos++] = 0xD2; // delta=13 (needs extension), length=2
                    buf[pos++] = 10;   // delta extension: 23 - 13 = 10

                    // Block2 value
                    uint16_t block2_opt = 0x0006; // Block size 1024 (szx=6)
                    block2_opt |= (block->currBlockIndex & 0x0FFF) << 4;
                    bool is_last_block = (block->currBlockIndex + 1 >= block->totBlockNumber);
                    if (!is_last_block)
                    {
                        block2_opt |= 0x0008; // MORE flag
                    }
                    buf[pos++] = (block2_opt >> 8) & 0xFF;
                    buf[pos++] = block2_opt & 0xFF;

                    // Add payload marker and block data (remember: totBytesNumber starts from 2nd block)
                    buf[pos++] = 0xFF;
                    size_t total_length = block->totBytesNumber + MAX_BLOCK_SIZE;
                    size_t block_size = MAX_BLOCK_SIZE;
                    size_t remaining = total_length - (block->currBlockIndex * MAX_BLOCK_SIZE);
                    if (remaining < MAX_BLOCK_SIZE)
                    {
                        block_size = remaining;
                    }
                    memcpy(buf + pos, block_data + block_offset, block_size);
                    pos += block_size;

                    response_msg.set_length(pos);
                    response_msg.set_id(message_id);

                    ProtocolError error = channel.send(response_msg);
                    if (error)
                    {
                        block->transmissionRunning = false;
                        block->lastBlockSentTime = 0;
                        return error;
                    }

                    // Timestamp will be updated by trackle_update_block_sent_time called from protocol.cpp

                    if (is_last_block)
                    {
                        block->transmissionRunning = false;
                        block->lastBlockSentTime = 0;
                    }

                    return NO_ERROR;
                }

                // Variable type not handled or variable not found
                LOG(WARN, "GET variable request failed: variable '%s' type not handled or variable not found (type: %d), returning 404 Not Found", variable_key, var_type);
                Message response;
                channel.response(message, response, 16);
                size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(4, 4), 0, 0); // NOT_FOUND
                response.set_id(message_id);
                response.set_length(response_length);
                return channel.send(response);
            }
        };
    }
}
