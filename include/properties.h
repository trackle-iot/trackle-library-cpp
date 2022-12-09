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

        class Properties
        {
            char function_arg[MAX_FUNCTION_ARG_LENGTH + 1]; // add one for null terminator

            ProtocolError property_result(MessageChannel &channel, const void *result, TrackleReturnType::Enum, token_t token)
            {
                Message message;
                long res = long(result);

                // fix result code -> 1: ok, -1: generic error, -2: not writable, -3: not exists
                if (res >= 0)
                {
                    res = 1; // ok
                }
                else if (res < -3)
                {
                    res = -3; //
                }

                channel.create(message, Messages::function_return_size);
                size_t length = Messages::function_return(message.buf(), 0, token, res, channel.is_unreliable());
                message.set_length(length);
                return channel.send(message);
            }

        public:
            ProtocolError handle_property_call(token_t token, message_id_t message_id, Message &message, MessageChannel &channel,
                                               int (*update_property)(const char *function_key, const char *arg, const char *user_caller_id, TrackleDescriptor::FunctionResultCallback callback, void *reserved))
            {
                // copy the function key
                char function_key[MAX_FUNCTION_KEY_LENGTH + 1]; // add one for null terminator
                memset(function_key, 0, sizeof(function_key));
                uint8_t *queue = message.buf();
                uint8_t queue_offset = 8;
                size_t function_key_length = queue[7] & 0x0F;
                if (function_key_length == MAX_OPTION_DELTA_LENGTH + 1)
                {
                    function_key_length = MAX_OPTION_DELTA_LENGTH + 1 + queue[8];
                    queue_offset++;
                }
                // else if (function_key_length == MAX_OPTION_DELTA_LENGTH+2)
                // {
                //     // MAX_OPTION_DELTA_LENGTH+2 not supported and not required for function_key_length
                // }
                // allocated memory bounds check
                if (function_key_length > MAX_FUNCTION_KEY_LENGTH)
                {
                    function_key_length = MAX_FUNCTION_KEY_LENGTH;
                    // already memset to 0 (null terminator padded to end)
                }
                memcpy(function_key, queue + queue_offset, function_key_length);

                // How long is the argument?
                size_t q_index = queue_offset + function_key_length;
                size_t function_arg_length = queue[q_index] & 0x0F;
                if (function_arg_length == MAX_OPTION_DELTA_LENGTH + 1)
                {
                    ++q_index;
                    function_arg_length = MAX_OPTION_DELTA_LENGTH + 1 + queue[q_index];
                }
                else if (function_arg_length == MAX_OPTION_DELTA_LENGTH + 2)
                {
                    ++q_index;
                    function_arg_length = queue[q_index] << 8;
                    ++q_index;
                    function_arg_length |= queue[q_index];
                    function_arg_length += 269;
                }

                // Is there an user caller?
                size_t o_index = q_index + function_arg_length;
                char user_caller_id[MAX_USER_CALLER_ID_LEN];
                memset(user_caller_id, 0, MAX_USER_CALLER_ID_LEN);
                if (o_index + 2 < message.length())
                {
                    o_index++; // increase start user_id position in buffer
                    int user_caller_id_length = queue[o_index];
                    if (user_caller_id_length == 0x0d)
                    {
                        o_index++; // increase start user_id position in buffer (length is 2 bytes)
                        user_caller_id_length += queue[o_index];
                    }
                    memcpy(user_caller_id, queue + o_index + 1, user_caller_id_length);
                }

                bool arg_too_long = false;
                // allocated memory bounds check
                if (function_arg_length > MAX_FUNCTION_ARG_LENGTH)
                {
                    function_arg_length = MAX_FUNCTION_ARG_LENGTH;
                    arg_too_long = true;
                    // in case we got here due to inconceivable error, memset with null terminators
                    memset(function_arg, 0, sizeof(function_arg));
                }
                // save a copy of the argument
                memcpy(function_arg, queue + q_index + 1, function_arg_length);
                function_arg[function_arg_length] = 0; // null terminate string

                // call the given user function
                auto callback = [=, &channel](const void *result, TrackleReturnType::Enum resultType)
                {
                    return this->property_result(channel, result, resultType, token);
                };

                int result = 0;
                if (!arg_too_long)
                {
                    result = update_property(function_key, function_arg, user_caller_id, callback, NULL);
                }

                if (result > 0 || arg_too_long)
                {
                    int error_x = 4;
                    int error_y = result;

                    Message response;
                    channel.response(message, response, 16);
                    size_t response_length = Messages::coded_ack(response.buf(), RESPONSE_CODE(error_x, error_y), 0, 0);
                    response.set_id(message_id);
                    response.set_length(response_length);
                    ProtocolError error = channel.send(response);
                    if (error)
                    {
                        return error;
                    }
                }

                return NO_ERROR;
            }
        };

    }
}
