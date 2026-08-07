/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2022 IOTREADY S.r.l. All rights reserved.
 * Copyright (c) 2015 Particle Industries, Inc.
 *
 * This source code is licensed under the Trackle Source-Available License
 * Agreement found in the LICENSE file in the root directory of this source tree.
 * Commercial deployment requires one paid Device License Key per device.
 */

#pragma once

#include "protocol_selector.h"

#include "service_debug.h"
#include "message_channel.h"
#include "buffer_message_channel.h"

extern "C"
{
#include "global.h"
#include "dtls_debug.h"
#include "dtls.h"
}

struct Dtls_data
{
	// int (*send)(const unsigned char *buf, uint32_t buflen, void *handle); // Send callback
	void *channel;													  // DTLSMessageChannel
	int (*send)(const unsigned char *buf, size_t len, void *channel); // Send callback
	uint32_t read_len;												  // len of received packet
	uint8_t read_buf[PROTOCOL_BUFFER_SIZE];							  // received buffer
	int read_error;													  // ProtocolError raised by the read callback
	int transport_error;												  // ProtocolError raised by the send callback
};

namespace trackle
{
	namespace protocol
	{
		/**
		 * This implements the lightweight and RSA encrypted handshake, AES session encryption over a TCP Stream.
		 *
		 * The buffer provided to the message starts at offset 2 to allow a 2-byte length to be added.
		 * The buffer length extends to the maximum capacity minus 16 so there is room for PKCS#1v5 padding.
		 */
		class DTLSMessageChannel : public BufferMessageChannel<PROTOCOL_BUFFER_SIZE>
		{
		public:
			struct Callbacks
			{
				/**
				 * An opaque handle for the send/receive context.
				 */
				void *tx_context;

				system_tick_t (*millis)();
				void (*handle_seed)(const uint8_t *seed, size_t length);
				int (*send)(const unsigned char *buf, uint32_t buflen, void *handle);
				int (*receive)(unsigned char *buf, uint32_t buflen, void *handle);

				// persistence
				/**
				 * Saves the given buffer.
				 * Returns 0 on success.
				 */
				int (*save)(const void *data, size_t length, uint8_t type, void *reserved);
				/**
				 * Restore to the given buffer. Returns the number of bytes restored.
				 */
				int (*restore)(void *data, size_t max_length, uint8_t type, void *reserved);

				uint32_t (*calculate_crc)(const uint8_t *data, uint32_t length);
				void (*notify_client_messages_processed)(void *reserved);
			};

		private:
			friend int dtls_rng(void *handle, uint8_t *data, size_t len);

			Callbacks callbacks;
			uint32_t keys_checksum;
			uint32_t handshake_timeout;

			dtls_context_t *dtls_context = NULL;
			session_t dst;
			Dtls_data dtls_data;

			/**
			 * The next message ID for new messages over this channel.
			 */
			message_id_t *coap_state;
			uint8_t ticket_handshake_failures;
			bool ticket_offered_for_handshake;

			void init();
			void dispose();

			/**
			 * C function to call the send/recv methods on a DTLSMessageChannel instance.
			 */
			/*static int send_(void *ctx, const uint8_t *data, size_t len);
			static int recv_(void *ctx, uint8_t *data, size_t len);*/
			static int sendCallback(const unsigned char *buf, size_t len, void *channel);

			int send(const uint8_t *data, size_t len);
			int recv(uint8_t *data, size_t len);

			ProtocolError setup_context();

			void reset_session();
			void handshake_failed();
			void handshake_succeeded();

			static const uint8_t TICKET_HANDSHAKE_FAILURE_LIMIT = 2;

			enum StateEnum
			{
				INIT,
				HANDSHAKE
			};

			enum StateEnum status;

		public:
			DTLSMessageChannel()
				: coap_state(nullptr),
				  ticket_handshake_failures(0),
				  ticket_offered_for_handshake(false) {}

			ProtocolError init(const uint8_t *core_private, size_t core_private_len,
							   // const uint8_t *core_public, size_t core_public_len,
							   const uint8_t *server_public, size_t server_public_len,
							   Callbacks &callbacks,
							   message_id_t *coap_state);

			void set_handshake_timeout(uint32_t timeout)
			{
				handshake_timeout = timeout;
			}

			virtual bool is_unreliable() override;

			virtual ProtocolError establish(uint32_t &flags, uint32_t app_crc) override;

			/**
			 * Retrieve first the 2 byte length from the stream, which determines
			 */
			virtual ProtocolError receive(Message &message) override;

			/**
			 * Sends the given message. The message length is prepended to the message
			 * and the message padded with PKCS#1 padding before being sent using
			 * the send callback.
			 */
			virtual ProtocolError send(Message &message) override;

			virtual ProtocolError notify_established() override;

			virtual ProtocolError command(Command cmd, void *arg = nullptr) override;

			virtual void notify_client_messages_processed() override
			{
				if (callbacks.notify_client_messages_processed)
				{
					callbacks.notify_client_messages_processed(nullptr);
				}
			}

			virtual void init_status() override;
		};

	}
}
