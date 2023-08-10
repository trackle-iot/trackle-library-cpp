#include "logging.h"
#include "trackle.h"
LOG_SOURCE_CATEGORY("comm.protocol")

#include "protocol.h"
#include "chunked_transfer.h"
#include "subscriptions.h"
#include "functions.h"
const int HANDSHAKE_TIMEOUT = 4000;

namespace trackle
{
    namespace protocol
    {

        /**
         * Sends an empty acknowledgement for the given message
         */
        ProtocolError Protocol::send_empty_ack(Message &message, message_id_t msg_id)
        {
            message.set_length(Messages::empty_ack(message.buf(), 0, 0));
            message.set_id(msg_id);
            return channel.send(message);
        }

        /**
         * Decodes and dispatches a received message to its handler.
         */
        ProtocolError Protocol::handle_received_message(Message &message,
                                                        CoAPMessageType::Enum &message_type)
        {
            last_message_millis = callbacks.millis();
            pinger.message_received();
            uint8_t *queue = message.buf();
            message_type = Messages::decodeType(queue, message.length());
            // todo - not all requests/responses have tokens. These device requests do not use tokens:
            // Update Done, ChunkMissed, event, ping, hello
            token_t token = queue[4];
            message_id_t msg_id = CoAP::message_id(queue);
            CoAPCode::Enum code = CoAP::code(queue);
            CoAPType::Enum type = CoAP::type(queue);

            if (CoAPType::is_reply(type))
            {
                LOG(TRACE, "Reply received: type=%d, code=%d", type, code);
                // printf("Reply received: type=%d, code=%d\n", type, code);
                //  todo - this is a little too simple in the case of an empty ACK for a separate response
                //  the message should then be bound to the token. see CH19037
                if (type == CoAPType::RESET)
                { // RST is sent with an empty code. It's like an unspecified error
                    LOG(TRACE, "Reset received, setting error code to internal server error.");
                    code = CoAPCode::INTERNAL_SERVER_ERROR;
                }
                notify_message_complete(msg_id, code, token);
            }

            ProtocolError error = NO_ERROR;
            //  todo - would be good to separate requests from responses here.
            switch (message_type)
            {
            case CoAPMessageType::DESCRIBE:
            {
                // 4 bytes header, 1 byte token, 2 bytes Uri-Path
                // 2 bytes optional single character Uri-Query for describe flags
                int descriptor_type = DESCRIBE_DEFAULT;
                if (message.length() > 8 && queue[8] <= DESCRIBE_MAX)
                {
                    descriptor_type = queue[8];
                }
                else if (message.length() > 8)
                {
                    LOG(WARN, "Invalid DESCRIBE flags %02x", queue[8]);
                }
                error = send_description(token, msg_id, descriptor_type);
                break;
            }

            case CoAPMessageType::FUNCTION_CALL:
                return functions.handle_function_call(token, msg_id, message, channel,
                                                      descriptor.call_function);

            case CoAPMessageType::VARIABLE_REQUEST:
            {
                char variable_key[MAX_VARIABLE_KEY_LENGTH + 1];
                char variable_args[MAX_FUNCTION_ARG_LENGTH + 1];
                return variables.handle_variable_request(variable_key, variable_args, message,
                                                         channel, token, msg_id,
                                                         descriptor.variable_type,
                                                         descriptor.get_variable);
            }
            case CoAPMessageType::SAVE_BEGIN:
                // fall through
            case CoAPMessageType::UPDATE_BEGIN:
                return chunkedTransfer.handle_update_begin(token, message, channel);

            case CoAPMessageType::CHUNK:
                return chunkedTransfer.handle_chunk(token, message, channel);
            case CoAPMessageType::UPDATE_DONE:
                return chunkedTransfer.handle_update_done(token, message, channel);

            case CoAPMessageType::EVENT:
                return subscriptions.handle_event(message, descriptor.call_event_handler, channel);

            case CoAPMessageType::KEY_CHANGE:
                return handle_key_change(message);

            case CoAPMessageType::UPDATE_PROPERTY:
                return properties.handle_property_call(token, msg_id, message, channel,
                                                       descriptor.update_state);

            case CoAPMessageType::SIGNAL_START:
                message.set_length(
                    Messages::coded_ack(message.buf(), token,
                                        ChunkReceivedCode::OK, queue[2], queue[3]));
                callbacks.signal(true, 0, NULL);
                return channel.send(message);

            case CoAPMessageType::SIGNAL_STOP:
                message.set_length(
                    Messages::coded_ack(message.buf(), token,
                                        ChunkReceivedCode::OK, queue[2], queue[3]));
                callbacks.signal(false, 0, NULL);
                return channel.send(message);

            case CoAPMessageType::HELLO:
                if (message.get_type() == CoAPType::CON)
                    send_empty_ack(message, msg_id);
                descriptor.ota_upgrade_status_sent();
                break;

            case CoAPMessageType::TIME:
                handle_time_response(
                    queue[6] << 24 | queue[7] << 16 | queue[8] << 8 | queue[9]);
                break;

            case CoAPMessageType::PING:
                message.set_length(
                    Messages::empty_ack(message.buf(), queue[2], queue[3]));
                error = channel.send(message);
                break;

            case CoAPMessageType::ERROR:
            default:; // drop it on the floor
            }

            // all's well
            return error;
        }

        // Callback for blocks in a block publish except the last, that must be handled differently.
        void genericBlockCompletionCallback(int error, const void *data, void *callbackData, void *reserved)
        {

            // If there was an error in sending a block, then the whole blockwise transfer failed.
            // So it was completed with an error or if last packet was sent.
            // In both case the completion callback must be called.

            uint8_t *tokenPtr = static_cast<uint8_t *>(reserved);

            block_messages_data *block = trackle_get_block_by_token(*tokenPtr);
            if (block == NULL)
                return;

            // Remember: totBytesNumber starts from 2nd block
            if ((error != SYSTEM_ERROR_NONE) || ((block->currBlockIndex) * MAX_BLOCK_SIZE >= block->totBytesNumber))
            {
                // Reset trasmission running status
                block->transmissionRunning = false;

                if (block->completionCb == nullptr)
                {
                    LOG(WARN, "publish completed, but no completion callback specified!");
                    return;
                }
                block->completionCb(error, data, callbackData, reserved);
            }

            // Otherwise, do nothing.
        }

        void Protocol::notify_message_complete(message_id_t msg_id, CoAPCode::Enum responseCode, uint8_t token)
        {
            const auto codeClass = (int)responseCode >> 5;
            const auto codeDetail = (int)responseCode & 0x1f;
            LOG(TRACE, "message id %d complete with code %d.%02d", msg_id, codeClass, codeDetail);

            // Server received previous block, send the next.
            if (responseCode == CoAPCode::CONTINUE)
            {
                // printf("RECEIVED CONTINUE\n");
                ack_handlers.setResult(msg_id);

                block_messages_data *block = trackle_get_block_by_token(token);
                if (block == NULL)
                    return;

                block->currBlockIndex++;
                trackle_protocol_send_event_data eventHandler;
                eventHandler.handler_callback = genericBlockCompletionCallback;
                eventHandler.handler_data = reinterpret_cast<void *>(block->msg_key);
                eventHandler.handler_token = token;

                const char *data = reinterpret_cast<const char *>(block->buffer);

                // remember: buffer starts from 2nd block
                uint16_t currBlockLength = std::min(static_cast<size_t>(MAX_BLOCK_SIZE), block->totBytesNumber + MAX_BLOCK_SIZE - block->currBlockIndex * MAX_BLOCK_SIZE);
                uint16_t currentOffset = block->currBlockIndex * MAX_BLOCK_SIZE - MAX_BLOCK_SIZE;

                trackle_protocol_send_event(this, block->token, block->eventName.c_str(), data + currentOffset, currBlockLength, block->ttl, block->currBlockIndex, block->totBlockNumber, block->flags, &eventHandler);
            }
            else if (CoAPCode::is_success(responseCode))
            {
                ack_handlers.setResult(msg_id);
            }
            else
            {
                int error = SYSTEM_ERROR_COAP;
                switch (codeClass)
                {
                case 4:
                    error = SYSTEM_ERROR_COAP_4XX;
                    break;
                case 5:
                    error = SYSTEM_ERROR_COAP_5XX;
                    break;
                }
                ack_handlers.setError(msg_id, error);
            }
        }

        ProtocolError Protocol::handle_key_change(Message &message)
        {
            uint8_t *buf = message.buf();
            ProtocolError result = NO_ERROR;
            if (CoAP::type(buf) == CoAPType::CON)
            {
                Message response;
                channel.response(message, response, 5);
                size_t sz = Messages::empty_ack(response.buf(), 0, 0);
                response.set_length(sz);
                result = channel.send(response);
            }

            // 4 bytes coAP header, 2 bytes message type option
            // token length, and skip 1 byte for the parameter option header.
            if (message.length() > 7)
            {
                uint8_t *buf = message.buf();
                uint8_t option_idx = 7 + (buf[0] & 0xF);
                if (buf[option_idx] == 1)
                {
                    result = channel.command(MessageChannel::DISCARD_SESSION);
                }
            }
            return result;
        }

        /**
         * Handles the time delivered from the cloud.
         */
        void Protocol::handle_time_response(uint32_t time)
        {
            // deduct latency
            // uint32_t latency = last_chunk_millis ? (callbacks.millis()-last_chunk_millis)/2000 : 0;
            // last_chunk_millis = 0;
            // todo - compute connection latency
            timesync_.handle_time_response(time, callbacks.millis(), callbacks.set_time);
        }

        /**
         * Copy an initialize a block of memory from a source to a target, where the source may be smaller than the target.
         * This handles the case where the caller was compiled using a smaller version of the struct memory than what is the current.
         *
         * @param target            The destination structure
         * @param target_size     The size of the destination structure in bytes
         * @param source            The source structure
         * @param source_size    The size of the source structure in bytes
         */
        void Protocol::copy_and_init(void *target, size_t target_size,
                                     const void *source, size_t source_size)
        {
            memcpy(target, source, source_size);
            memset(((uint8_t *)target) + source_size, 0, target_size - source_size);
        }

        void Protocol::init(const TrackleCallbacks &callbacks,
                            const TrackleDescriptor &descriptor)
        {
            memset(&handlers, 0, sizeof(handlers));
            // the actual instances referenced may be smaller if the caller is compiled
            // against an older version of this library.
            copy_and_init(&this->callbacks, sizeof(this->callbacks), &callbacks,
                          callbacks.size);
            copy_and_init(&this->descriptor, sizeof(this->descriptor), &descriptor,
                          descriptor.size);

            chunkedTransferCallbacks.init(&this->callbacks);
            chunkedTransfer.init(&chunkedTransferCallbacks);

            initialized = true;
        }

        uint32_t Protocol::application_state_checksum(uint32_t (*calc_crc)(const uint8_t *data, uint32_t len), uint32_t subscriptions_crc,
                                                      uint32_t describe_app_crc, uint32_t describe_system_crc)
        {
            uint32_t chk[3];
            chk[0] = subscriptions_crc;
            chk[1] = describe_app_crc;
            chk[2] = describe_system_crc;
            return calc_crc((uint8_t *)chk, sizeof(chk));
        }

        void Protocol::update_subscription_crc()
        {
            if (descriptor.app_state_selector_info)
            {
                uint32_t crc = subscriptions.compute_subscriptions_checksum(callbacks.calculate_crc);
                this->channel.command(Channel::SAVE_SESSION);
                descriptor.app_state_selector_info(TrackleAppStateSelector::SUBSCRIPTIONS, TrackleAppStateUpdate::PERSIST, crc, nullptr);
                this->channel.command(Channel::LOAD_SESSION);
            }
        }

        /**
         * Computes the current checksum from the application cloud state
         */
        uint32_t Protocol::application_state_checksum()
        {
            return descriptor.app_state_selector_info ? application_state_checksum(callbacks.calculate_crc,
                                                                                   subscriptions.compute_subscriptions_checksum(callbacks.calculate_crc),
                                                                                   descriptor.app_state_selector_info(TrackleAppStateSelector::DESCRIBE_APP, TrackleAppStateUpdate::COMPUTE, 0, nullptr),
                                                                                   descriptor.app_state_selector_info(TrackleAppStateSelector::DESCRIBE_SYSTEM, TrackleAppStateUpdate::COMPUTE, 0, nullptr))
                                                      : 0;
        }

        /**
         * Establish a secure connection and send and process the hello message.
         * Return:
         * NO_ERROR in case handshake is not completed
         * SESSION_CONNECTED in case handshake complete
         * SESSION_RESUMED in case we reuse an old connection
         * negative number in case of error
         */
        int Protocol::begin()
        {
            static system_tick_t t;
            static message_id_t id;

            switch (this->status)
            {

            case CHANNEL_INIT:
            {

                LOG_CATEGORY("comm.protocol.handshake");
                LOG(INFO, "Establish secure connection");
                chunkedTransfer.reset();
                pinger.reset();
                timesync_.reset();

                // FIXME: Pending completion handlers should be cancelled at the end of a previous session
                ack_handlers.clear();
                last_ack_handlers_update = callbacks.millis();

                /*
                 * Reset channel state machine
                 */
                channel.init_status();

                LOG(INFO, "Socket connection completed, starting handshake");

                this->status = CHANNEL_ESTABLISHED;

                break;
            }

            case CHANNEL_ESTABLISHED:
            {

                uint32_t channel_flags = 0;

                ProtocolError error = channel.establish(channel_flags, application_state_checksum());

                /*
                 * NO_ERROR in case handshake is not completed
                 * SESSION_CONNECTED in case handshake complete
                 * SESSION_RESUMED in case we reuse an old connection
                 * negative number in case of error
                 */

                if (error == SESSION_CONNECTED)
                {

                    this->status = SEND_HELLO;
                }
                else if (error == SESSION_RESUMED)
                {

                    // for now, unconditionally move the session on resumption
                    channel.command(MessageChannel::MOVE_SESSION, nullptr);

                    if (channel.is_unreliable() && (channel_flags & SKIP_SESSION_RESUME_HELLO))
                    {
                        LOG(INFO, "resumed session - not sending HELLO message");
                        const auto r = ping(true);
                        if (r != NO_ERROR)
                        {
                            error = r;
                        }
                        // Note: Make sure SESSION_RESUMED gets returned to the calling code
                        return error;
                    }

                    this->status = SEND_HELLO;
                }
                else if (error != 0)
                {

                    LOG(ERROR, "handshake failed with code %d", error);
                    this->status = CHANNEL_INIT;
                    return error;
                }
                else
                {

                    /*
                     * We need to run again...
                     */
                }

                break;
            }

            case SEND_HELLO:
            {

                ProtocolError error;

                LOG(INFO, "Sending HELLO message");
                error = hello(descriptor.was_ota_upgrade_successful(), &id);

                /*
                 * A questo punto devo aspettare un ACK dal server
                 */
                if (WAIT_FOR_ACK == error)
                {
                    t = callbacks.millis();
                    this->status = ACK_WAITING;
                }
                else
                {
                    /*
                     * C'è stato un errore
                     */
                    LOG(ERROR, "Could not send HELLO message: %d", error);
                    this->status = CHANNEL_INIT;
                    return error;
                }

                break;
            }

            case ACK_WAITING:
            {

                if ((callbacks.millis() - t) < HANDSHAKE_TIMEOUT)
                {
                    ProtocolError error;
                    error = wait_ack(id);

                    if (ACK_RECEIVED == error)
                    {
                        LOG(INFO, "Handshake completed");
                        channel.notify_established();

                        /* Send system Describe */
                        error = post_description(DescriptionType::DESCRIBE_DEFAULT);
                        if (error)
                        {
                            this->status = CHANNEL_INIT;
                            return error;
                        }

                        this->status = CHANNEL_INIT;

                        // TODO: This will cause all the application events to be sent even if the session was resumed
                        return SESSION_CONNECTED;
                    }
                }
                else
                {
                    LOG(ERROR, "Handshake: could not receive HELLO ack");
                    this->status = CHANNEL_INIT;
                    return MESSAGE_TIMEOUT;
                }

                break;
            }
            }

            // TODO: This will cause all the application events to be sent even if the session was resumed
            return 0;
        }

        const auto HELLO_FLAG_OTA_UPGRADE_SUCCESSFUL = 0x01;
        const auto HELLO_FLAG_DIAGNOSTICS_SUPPORT = 0x02;
        const auto HELLO_FLAG_IMMEDIATE_UPDATES_SUPPORT = 0x04;
        // Flag 0x08 is reserved to indicate support for the HandshakeComplete message
        const auto HELLO_FLAG_GOODBYE_SUPPORT = 0x10;
        const auto HELLO_FLAG_DEVICE_INITIATED_DESCRIBE = 0x20;
        const auto HELLO_FLAG_COMPRESSED_OTA = 0x40;
        const auto HELLO_FLAG_OTA_PROTOCOL_V3 = 0x80;

        /**
         * Send the hello message over the channel.
         * @param was_ota_upgrade_successful {@code true} if the previous OTA update was successful.
         */
        ProtocolError
        Protocol::hello(bool was_ota_upgrade_successful, message_id_t *id)
        {
            Message message;
            channel.create(message);
            uint8_t flags = was_ota_upgrade_successful ? HELLO_FLAG_OTA_UPGRADE_SUCCESSFUL : 0;
            flags |= HELLO_FLAG_DIAGNOSTICS_SUPPORT | HELLO_FLAG_IMMEDIATE_UPDATES_SUPPORT | HELLO_FLAG_OTA_PROTOCOL_V3;
            size_t len = build_hello(message, flags);
            message.set_length(len);
            message.set_confirm_received(true);
            last_message_millis = callbacks.millis();
            ProtocolError res = channel.send(message);
            *id = message.get_id();
            return res;
        }

        /**
         * Send the hello message over the channel.
         * @param was_ota_upgrade_successful {@code true} if the previous OTA update was successful.
         */
        ProtocolError
        Protocol::wait_ack(message_id_t id)
        {
            ProtocolError res = channel.wait_ack(id);
            return res;
        }

        ProtocolError Protocol::hello_response()
        {
            ProtocolError error = event_loop(CoAPMessageType::HELLO, 0); // read the hello message from the server
            return error;
        }

        /**
         * Wait for a specific message type to be received.
         * @param message_type        The type of message wait for
         * @param timeout            The duration to wait for the message before giving up.
         *
         * @returns NO_ERROR if the message was successfully matched within the timeout.
         * Returns MESSAGE_TIMEOUT if the message wasn't received within the timeout.
         * Other protocol errors may return additional error values.
         */
        ProtocolError Protocol::event_loop(CoAPMessageType::Enum message_type,
                                           system_tick_t timeout)
        {
            system_tick_t start = callbacks.millis();
            LOG(INFO, "waiting %d seconds for message type=%d", timeout / 1000, message_type);
            do
            {
                CoAPMessageType::Enum msgtype;
                ProtocolError error = event_loop(msgtype);
                if (error)
                {
                    LOG(ERROR, "message type=%d, error=%d", (int)msgtype, error);
                    return error;
                }
                if (msgtype == message_type)
                    return NO_ERROR;
                // todo - ideally need a delay here
            } while ((callbacks.millis() - start) < timeout);
            return MESSAGE_TIMEOUT;
        }

        /**
         * Processes one event. Retrieves the type of the event processed, or NONE if no event processed.
         * If an error occurs, the event type is undefined.
         */
        ProtocolError Protocol::event_loop(CoAPMessageType::Enum &message_type)
        {
            // Process expired completion handlers
            const system_tick_t t = callbacks.millis();
            ack_handlers.update(t - last_ack_handlers_update);
            last_ack_handlers_update = t;

            Message message;
            message_type = CoAPMessageType::NONE;
            ProtocolError error = channel.receive(message);
            if (!error)
            {
                if (message.length())
                {
                    error = handle_received_message(message, message_type);
                    LOG(TRACE, "rcv'd message type=%d", (int)message_type);
                }
                else
                {
                    error = event_loop_idle();
                }
            }

            // todo: add dtls_check_retransmit???

            if (error)
            {
                // bail if and only if there was an error
                chunkedTransfer.cancel();
                return error;
            }
            return error;
        }

        void Protocol::build_describe_message(Appender &appender, int desc_flags)
        {
            // diagnostics must be requested in isolation to be a binary packet
            if (descriptor.append_metrics && (desc_flags == DESCRIBE_METRICS))
            {
                appender.append(char(0));                // null byte means binary data
                appender.append(char(DESCRIBE_METRICS)); // uint16 describes the type of binary packet
                appender.append(char(0));                //
                const int flags = 1;                     // binary
                const int page = 0;
                descriptor.append_metrics(append_instance, &appender, flags, page, nullptr);
            }
            else
            {
                appender.append("{");
                bool has_content = false;

                if (desc_flags & DESCRIBE_APPLICATION)
                {
                    has_content = true;
                    appender.append("\"f\":[");

                    int num_keys = descriptor.num_functions();
                    int i;
                    for (i = 0; i < num_keys; ++i)
                    {
                        if (i)
                        {
                            appender.append(',');
                        }
                        appender.append('"');

                        const char *key = descriptor.get_function_key(i);
                        size_t function_name_length = strlen(key);
                        if (MAX_FUNCTION_KEY_LENGTH < function_name_length)
                        {
                            function_name_length = MAX_FUNCTION_KEY_LENGTH;
                        }
                        appender.append((const uint8_t *)key, function_name_length);
                        appender.append('"');
                    }

                    appender.append("],\"v\":{");

                    num_keys = descriptor.num_variables();
                    for (i = 0; i < num_keys; ++i)
                    {
                        if (i)
                        {
                            appender.append(',');
                        }
                        appender.append('"');
                        const char *key = descriptor.get_variable_key(i);
                        size_t variable_name_length = strlen(key);
                        TrackleReturnType::Enum t = descriptor.variable_type(key);
                        if (MAX_VARIABLE_KEY_LENGTH < variable_name_length)
                        {
                            variable_name_length = MAX_VARIABLE_KEY_LENGTH;
                        }
                        appender.append((const uint8_t *)key, variable_name_length);
                        appender.append("\":");
                        appender.append('0' + (char)t);
                    }
                    appender.append('}');
                }

                if (descriptor.append_system_info && (desc_flags & DESCRIBE_SYSTEM))
                {
                    if (has_content)
                    {
                        appender.append(',');
                    }
                    has_content = true;
                    descriptor.append_system_info(append_instance, &appender, nullptr);
                }
                appender.append('}');
            }
        }

        ProtocolError Protocol::generate_and_send_description(MessageChannel &channel, Message &message,
                                                              size_t header_size, int desc_flags)
        {
            ProtocolError error;

            BufferAppender appender((message.buf() + header_size), (message.capacity() - header_size));
            build_describe_message(appender, desc_flags);

            const size_t msglen = (appender.next() - (uint8_t *)message.buf());
            message.set_length(msglen);
            if (appender.overflowed())
            {
                LOG(ERROR, "Describe message overflowed by %d bytes", appender.overflowed());
                // There is no point in continuing to run, the device will be constantly reconnecting
                // to the cloud. It's better to clearly indicate that the describe message is never going
                // to go through to the cloud by going into a panic state, otherwise one would have to
                // sift through logs to find 'Describe message overflowed by %d bytes' message to understand
                // what's going on.
            }

            LOG(INFO, "Posting '%s%s%s' describe message", desc_flags & DESCRIBE_SYSTEM ? "S" : "",
                desc_flags & DESCRIBE_APPLICATION ? "A" : "", desc_flags & DESCRIBE_METRICS ? "M" : "");

            error = channel.send(message);

            if (error == NO_ERROR && descriptor.app_state_selector_info &&
                (desc_flags & DESCRIBE_APPLICATION || desc_flags & DESCRIBE_SYSTEM))
            {
                this->channel.command(Channel::SAVE_SESSION);
                if (desc_flags & DESCRIBE_APPLICATION)
                {
                    // have sent the describe message to the cloud so update the crc
                    descriptor.app_state_selector_info(TrackleAppStateSelector::DESCRIBE_APP,
                                                       TrackleAppStateUpdate::COMPUTE_AND_PERSIST, 0,
                                                       nullptr);
                }
                if (desc_flags & DESCRIBE_SYSTEM)
                {
                    // have sent the describe message to the cloud so update the crc
                    descriptor.app_state_selector_info(TrackleAppStateSelector::DESCRIBE_SYSTEM,
                                                       TrackleAppStateUpdate::COMPUTE_AND_PERSIST, 0,
                                                       nullptr);
                }
                this->channel.command(Channel::LOAD_SESSION);
            }
            // Log error code
            else if (NO_ERROR != error)
            {
                LOG(ERROR, "Channel failed to send message with error-code <%d>", error);
            }

            return error;
        }

        ProtocolError Protocol::post_description(int desc_flags)
        {
            Message message;
            channel.create(message);
            const size_t header_size =
                Messages::describe_post_header(message.buf(), message.capacity(), 0, (desc_flags & 0xFF));

            return generate_and_send_description(channel, message, header_size, desc_flags);
        }

        /**
         * Produces and transmits (PIGGYBACK) a describe message.
         * @param desc_flags Flags describing the information to provide. A combination of {@code
         * DESCRIBE_APPLICATION) and {@code DESCRIBE_SYSTEM) flags.
         */
        ProtocolError Protocol::send_description(token_t token, message_id_t msg_id, int desc_flags)
        {
            Message message;
            channel.create(message);
            uint8_t *buf = message.buf();
            message.set_id(msg_id);
            size_t desc = Messages::description(buf, msg_id, token);

            return generate_and_send_description(channel, message, desc, desc_flags);
        }

        int Protocol::ChunkedTransferCallbacks::prepare_for_firmware_update(FileTransfer::Descriptor &data, uint32_t flags, void *reserved)
        {
            return callbacks->prepare_for_firmware_update(data, flags, reserved);
        }

        int Protocol::ChunkedTransferCallbacks::save_firmware_chunk(FileTransfer::Descriptor &descriptor, const unsigned char *chunk, void *reserved)
        {
            return callbacks->save_firmware_chunk(descriptor, chunk, reserved);
        }

        int Protocol::ChunkedTransferCallbacks::finish_firmware_update(FileTransfer::Descriptor &data, uint32_t flags, void *reserved)
        {
            return callbacks->finish_firmware_update(data, flags, reserved);
        }

        uint32_t Protocol::ChunkedTransferCallbacks::calculate_crc(const unsigned char *buf, uint32_t buflen)
        {
            return callbacks->calculate_crc(buf, buflen);
        }

        system_tick_t Protocol::ChunkedTransferCallbacks::millis()
        {
            return callbacks->millis();
        }

        int Protocol::get_describe_data(trackle_protocol_describe_data *data, void *reserved)
        {
            data->maximum_size = 768;             // a conservative guess based on dtls and lightssl encryption overhead and the CoAP data
            BufferAppender2 appender(nullptr, 0); // don't need to store the data, just count the size
            build_describe_message(appender, data->flags);
            data->current_size = appender.dataSize();
            return 0;
        }

#if HAL_PLATFORM_MESH
        int completion_result(int result, completion_handler_data *completion)
        {
            if (completion)
            {
                completion->handler_callback(result, nullptr, completion->handler_data, nullptr);
            }
            return result;
        }

        int Protocol::mesh_command(MeshCommand::Enum cmd, uint32_t data, void *extraData, completion_handler_data *completion)
        {
            LOG(INFO, "handling mesh command %d", cmd);
            switch (cmd)
            {
            case MeshCommand::NETWORK_CREATED:
                return mesh.network_update(*this, next_token(), channel, true, *(MeshCommand::NetworkInfo *)extraData, completion);
            case MeshCommand::NETWORK_UPDATED:
                return mesh.network_update(*this, next_token(), channel, false, *(MeshCommand::NetworkInfo *)extraData, completion);
            case MeshCommand::DEVICE_MEMBERSHIP:
                return mesh.device_joined(*this, next_token(), channel, data, *(MeshCommand::NetworkUpdate *)extraData, completion);
            case MeshCommand::DEVICE_BORDER_ROUTER:
                return mesh.device_gateway(*this, next_token(), channel, data, *(MeshCommand::NetworkUpdate *)extraData, completion);
            default:
                return completion_result(SYSTEM_ERROR_INVALID_ARGUMENT, completion);
            }
        }
#endif // HAL_PLATFORM_MESH
    }
}
