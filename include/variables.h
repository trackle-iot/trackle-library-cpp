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

namespace trackle
{
    namespace protocol
    {

        class Variables
        {

        public:
            ProtocolError decode_variable_request(char variable_key[MAX_VARIABLE_KEY_LENGTH + 1], char variable_arg[MAX_VARIABLE_ARG_LENGTH + 1], Message &message)
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

                if (message.length() > q_index)
                {
                    size_t variable_arg_length = queue[q_index] & 0x0F;
                    if (variable_arg_length == MAX_OPTION_DELTA_LENGTH + 1)
                    {
                        ++q_index;
                        variable_arg_length = MAX_OPTION_DELTA_LENGTH + 1 + queue[q_index];
                    }
                    else if (variable_arg_length == MAX_OPTION_DELTA_LENGTH + 2)
                    {
                        ++q_index;
                        variable_arg_length = queue[q_index] << 8;
                        ++q_index;
                        variable_arg_length |= queue[q_index];
                        variable_arg_length += 269;
                    }

                    // allocated memory bounds check
                    if (variable_arg_length > MAX_VARIABLE_ARG_LENGTH)
                    {
                        return IO_ERROR_GENERIC_RECEIVE;
                    }

                    memcpy(variable_arg, queue + q_index + 1, variable_arg_length);
                    variable_arg[variable_arg_length] = 0; // null terminate string
                }
                else
                {                        // no arguments
                    variable_arg[0] = 0; // null terminate string
                }

                return NO_ERROR;
            }

            ProtocolError handle_variable_request(char *variable_key, char *variable_arg, Message &message, MessageChannel &channel, token_t token, message_id_t message_id,
                                                  TrackleReturnType::Enum (*variable_type)(const char *variable_key),
                                                  const void *(*get_variable)(const char *variable_key))
            {

                ProtocolError err = decode_variable_request(variable_key, variable_arg, message);

                // args too long, send error 400
                if (err == IO_ERROR_GENERIC_RECEIVE)
                {
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
                size_t response = 0;

                if (TrackleReturnType::BOOLEAN == var_type)
                {
                    const bool result = ((user_variable_bool_cb_t)(get_variable(variable_key)))(variable_arg);
                    response = Messages::variable_value(queue, message_id, token, result);
                }
                else if (TrackleReturnType::INT == var_type)
                {
                    const int32_t result = ((user_variable_int32_cb_t)(get_variable(variable_key)))(variable_arg);
                    response = Messages::variable_value(queue, message_id, token, result);
                }
                else if (TrackleReturnType::STRING == var_type || TrackleReturnType::JSON == var_type)
                {
                    const char *str_val = ((user_variable_char_cb_t)(get_variable(variable_key)))(variable_arg);

                    // 2-byte leading length, 16 potential padding bytes
                    int max_length = message.capacity();
                    int str_length = strlen(str_val);
                    if (str_length > max_length)
                    {
                        str_length = max_length;
                    }
                    response = Messages::variable_value(queue, message_id, token, str_val, str_length);
                }
                else if (TrackleReturnType::DOUBLE == var_type)
                {
                    const double result = ((user_variable_double_cb_t)(get_variable(variable_key)))(variable_arg);
                    response = Messages::variable_value(queue, message_id, token, result);
                }

                message.set_length(response);
                return channel.send(message);
            }
        };
    }
}
