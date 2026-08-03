#include "logging.h"
LOG_SOURCE_CATEGORY("comm.dtls")

#include "dtls_message_channel.h"

#include "protocol.h"

#include <stdio.h>
#include <string.h>

#define ECDSA_KEY_LENGTH 32

unsigned char ecdsa_priv_key[ECDSA_KEY_LENGTH];
unsigned char ecdsa_pub_key_x[ECDSA_KEY_LENGTH];
unsigned char ecdsa_pub_key_y[ECDSA_KEY_LENGTH];
unsigned char server_certificate[DTLS_PUBLIC_KEY_LENGTH];

bool valid_dtls_session = false;

static bool extract_pub_priv_keys(const uint8_t *key, size_t key_size)
{
	if (!key || key_size < 2 || key[0] != 0x30 || (key[1] & 0x80) != 0)
	{
		return false;
	}

	const size_t der_size = (size_t)key[1] + 2;
	if (der_size > key_size)
	{
		return false;
	}

	memset(ecdsa_priv_key, 0, sizeof(ecdsa_priv_key));
	memset(ecdsa_pub_key_x, 0, sizeof(ecdsa_pub_key_x));
	memset(ecdsa_pub_key_y, 0, sizeof(ecdsa_pub_key_y));

	bool private_key_found = false;
	bool public_key_found = false;
	size_t i = 2;
	while (i < der_size)
	{
		if (der_size - i < 2 || (key[i + 1] & 0x80) != 0)
		{
			return false;
		}

		const size_t field_size = key[i + 1];
		const size_t field_end = i + 2 + field_size;
		if (field_end > der_size)
		{
			return false;
		}

		if (key[i] == 0x04)
		{
			if (field_size == 0 || field_size > sizeof(ecdsa_priv_key))
			{
				return false;
			}
			memcpy(ecdsa_priv_key + sizeof(ecdsa_priv_key) - field_size,
				   key + i + 2, field_size);
			private_key_found = true;
		}
		else if (key[i] == 0xa1)
		{
			/* [1] BIT STRING, uncompressed P-256 point: 03 42 00 04 || X || Y */
			if (field_size != 68 ||
				key[i + 2] != 0x03 || key[i + 3] != 0x42 ||
				key[i + 4] != 0x00 || key[i + 5] != 0x04)
			{
				return false;
			}
			memcpy(ecdsa_pub_key_x, key + i + 6, sizeof(ecdsa_pub_key_x));
			memcpy(ecdsa_pub_key_y, key + i + 6 + sizeof(ecdsa_pub_key_x),
				   sizeof(ecdsa_pub_key_y));
			public_key_found = true;
		}
		i = field_end;
	}

	return i == der_size && private_key_found && public_key_found;
}

static int
dtls_event(struct dtls_context_t *ctx, session_t *session,
		   dtls_alert_level_t level, unsigned short code)
{
	LOG(TRACE, "dtls_event alert: %d %d", level, code);
	return 0;
}

static int
get_server_certificate(struct dtls_context_t *ctx,
					   const session_t *session,
					   const dtls_server_certificate_t **result)
{

	static dtls_server_certificate_t server_key = {
		.pub_key = server_certificate,
	};

	(void)ctx;
	(void)session;

	*result = &server_key;
	return 0;
}

static int
get_ecdsa_key(struct dtls_context_t *ctx,
			  const session_t *session,
			  const dtls_ecdsa_key_t **result)
{
	static dtls_ecdsa_key_t ecdsa_key = {
		.curve = DTLS_ECDH_CURVE_SECP256R1,
		.priv_key = ecdsa_priv_key,
		.pub_key_x = ecdsa_pub_key_x,
		.pub_key_y = ecdsa_pub_key_y,
	};

	(void)ctx;
	(void)session;

	*result = &ecdsa_key;
	return 0;
}

static int
verify_ecdsa_key(struct dtls_context_t *ctx,
				 const session_t *session,
				 const unsigned char *other_pub_x,
				 const unsigned char *other_pub_y,
				 size_t key_size)
{
	(void)ctx;
	(void)session;
	(void)other_pub_x;
	(void)other_pub_y;
	(void)key_size;
	return 0;
}

static int read_from_peer(struct dtls_context_t *ctx,
						  session_t *session, uint8 *data, size_t len)
{

	Dtls_data *t_dtls_data = (Dtls_data *)ctx->app;
	t_dtls_data->read_len = 0;
	if (len > sizeof(t_dtls_data->read_buf))
	{
		t_dtls_data->read_error = trackle::protocol::INSUFFICIENT_STORAGE;
		return -1;
	}

	memset(t_dtls_data->read_buf, 0, sizeof(t_dtls_data->read_buf));
	memcpy(t_dtls_data->read_buf, data, len);
	t_dtls_data->read_len = (uint32_t)len;

	return 0;
}

static int send_to_peer(struct dtls_context_t *ctx,
						session_t *session, uint8 *data, size_t len)
{

	Dtls_data *t_dtls_data = (Dtls_data *)ctx->app;
	int res = t_dtls_data->send(data, (int)len, t_dtls_data->channel);
	if (res < 0)
	{
		t_dtls_data->transport_error = trackle::protocol::IO_ERROR_GENERIC_SEND;
	}
	return res;
}

namespace trackle
{
	namespace protocol
	{
		system_tick_t (*getMillis)() = NULL;

#define EXIT_ERROR(x, msg)                                                                       \
	if (x)                                                                                       \
	{                                                                                            \
		LOG(WARN, "DTLS init failure: " #msg ": %c%04X", (x < 0) ? '-' : ' ', (x < 0) ? -x : x); \
		return UNKNOWN;                                                                          \
	}

		ProtocolError DTLSMessageChannel::init(
			const uint8_t *core_private, size_t core_private_len,
			const uint8_t *server_public, size_t server_public_len,
			Callbacks &callbacks,
			message_id_t *coap_state)
		{
			init();

			this->coap_state = coap_state;
			this->callbacks = callbacks;

			getMillis = callbacks.millis;

			dtls_init();

			if (!extract_pub_priv_keys(core_private, core_private_len))
			{
				LOG(ERROR, "Invalid DTLS client private key");
				return IO_ERROR_PARSING_SERVER_PUBLIC_KEY;
			}

			if (!server_public || server_public_len != DTLS_PUBLIC_KEY_LENGTH)
			{
				LOG(ERROR, "Invalid DTLS server public key length: %u",
					(unsigned)server_public_len);
				return IO_ERROR_PARSING_SERVER_PUBLIC_KEY;
			}

			memset(server_certificate, 0, sizeof(server_certificate));
			memcpy(server_certificate, server_public, server_public_len);

			static dtls_handler_t cb = {
				.write = send_to_peer,
				.read = read_from_peer,
				.event = dtls_event,
				.get_user_parameters = NULL,
				.get_server_certificate = get_server_certificate,
				.get_ecdsa_key = get_ecdsa_key,
				.verify_ecdsa_key = verify_ecdsa_key,
			};

			dtls_data.send = sendCallback; // send callback
			dtls_data.channel = (void *)this;
			dtls_data.read_len = 0;
			dtls_data.read_error = NO_ERROR;
			dtls_data.transport_error = NO_ERROR;

			dtls_context = dtls_new_context(&dtls_data);
			if (!dtls_context)
			{
				LOG(ERROR, "Cannot create DTLS context");
				return UNKNOWN;
			}

			dtls_set_handler(dtls_context, &cb);
			return NO_ERROR;
		}

		inline int DTLSMessageChannel::send(const uint8_t *data, size_t len)
		{
			return callbacks.send(data, len, callbacks.tx_context);
		}

		void DTLSMessageChannel::reset_session()
		{
			LOG(TRACE, "DTLSMessageChannel::reset_session");
			this->status = INIT;

			dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);
			if (peer)
			{
				dtls_reset_peer(dtls_context, peer);
			}
		}

		void DTLSMessageChannel::handshake_failed()
		{
#if DTLS_SESSION_TICKET
			if (ticket_offered_for_handshake)
			{
				++ticket_handshake_failures;
				LOG(WARN, "Session Ticket: handshake failure %u/%u",
					(unsigned)ticket_handshake_failures,
					(unsigned)TICKET_HANDSHAKE_FAILURE_LIMIT);

				if (ticket_handshake_failures >= TICKET_HANDSHAKE_FAILURE_LIMIT)
				{
					LOG(WARN, "Session Ticket: discarding ticket after repeated failures");
					memset(&dtls_context->session_ticket, 0,
						   sizeof(dtls_context->session_ticket));
					ticket_handshake_failures = 0;
				}
			}
#endif
			ticket_offered_for_handshake = false;
			reset_session();
		}

		void DTLSMessageChannel::handshake_succeeded()
		{
			ticket_offered_for_handshake = false;
			ticket_handshake_failures = 0;
			this->status = INIT;
		}

		inline int DTLSMessageChannel::recv(uint8_t *data, size_t len)
		{
			int size = callbacks.receive(data, len, callbacks.tx_context);

			// ignore 0 and 1 byte UDP packets which are used to keep alive the connection.
			if (size >= 0 && size <= 1)
				size = 0;
			return size;
		}

		int DTLSMessageChannel::sendCallback(const unsigned char *buf, size_t len, void *ctx)
		{
			DTLSMessageChannel *channel = (DTLSMessageChannel *)ctx;
			return channel->send(buf, len);
		}

		void DTLSMessageChannel::init()
		{
		}

		void DTLSMessageChannel::dispose()
		{
			memset(ecdsa_priv_key, 0, ECDSA_KEY_LENGTH);
			memset(ecdsa_pub_key_x, 0, ECDSA_KEY_LENGTH);
			memset(ecdsa_pub_key_y, 0, ECDSA_KEY_LENGTH);
			memset(server_certificate, 0, DTLS_PUBLIC_KEY_LENGTH);
		}

		// #if defined(ESP32)
		struct dtls_timing_context
		{
			uint32_t snapshot;
			uint32_t fin_ms;
		};

		void dtls_timing_set_delay(void *data, uint32_t fin_ms)
		{
			struct dtls_timing_context *ctx = (struct dtls_timing_context *)data;
			ctx->fin_ms = fin_ms;

			if (fin_ms != 0)
			{
				ctx->snapshot = (*getMillis)();
			}
		}

		int dtls_timing_get_delay(void *data)
		{
			struct dtls_timing_context *ctx = (struct dtls_timing_context *)data;
			unsigned long elapsed_ms;

			if (ctx->fin_ms == 0)
			{
				return -1;
			}

			elapsed_ms = (*getMillis)() - ctx->snapshot;

			if (elapsed_ms >= ctx->fin_ms)
			{
				return 1;
			}

			return 0;
		}
		// #endif

		void DTLSMessageChannel::init_status()
		{
			this->status = INIT;
		}

		ProtocolError DTLSMessageChannel::setup_context()
		{
			return NO_ERROR;
		}

		ProtocolError DTLSMessageChannel::establish(uint32_t &flags, uint32_t app_state_crc)
		{
			(void)app_state_crc;
			int ret = -1;

#define MAX_READ_BUF 1000
			static uint8 buf[MAX_READ_BUF];

			static dtls_timing_context time_cb;
			int8_t connection_status = -1;
			int8_t timeout_status = 0;
			int res = 0;

			switch (this->status)
			{
			case INIT:
			{

				dtls_timing_set_delay(&time_cb, this->handshake_timeout);

				/*
				 * Trackle invokes establish() after creating a new UDP socket.
				 * A CONNECTED peer belongs to the previous socket/path and its
				 * CID must not be reused. The session ticket is stored in the
				 * context, so resetting the peer preserves ticket resumption.
				 */
				dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);
				if (peer)
				{
					LOG(TRACE, "Resetting DTLS peer before reconnect (state %d)",
						peer->state);
					peer->state = DTLS_STATE_CLOSING;
					dtls_reset_peer(dtls_context, peer);
				}

				ticket_offered_for_handshake = false;

				dtls_data.transport_error = NO_ERROR;
				res = dtls_connect(dtls_context, &dst);
#if DTLS_SESSION_TICKET
				peer = dtls_get_peer(dtls_context, &dst);
				ticket_offered_for_handshake =
					peer && peer->handshake_params &&
					peer->handshake_params->session_ticket_presented;
#endif
				LOG(TRACE, "dtls_connect: %d, ticket offered: %d",
					res, (int)ticket_offered_for_handshake);

				if (res <= 0)
				{
					LOG(TRACE, "dtls_connect error %d", res);
					ProtocolError error = dtls_data.transport_error != NO_ERROR
											  ? static_cast<ProtocolError>(dtls_data.transport_error)
											  : IO_ERROR_GENERIC_ESTABLISH;
					handshake_failed();
					return error;
				}

				if (!ticket_offered_for_handshake)
					ticket_handshake_failures = 0;

				LOG(TRACE, "starting handshake");
				this->status = HANDSHAKE;

				break;
			}

			case HANDSHAKE:
			{

				dtls_data.read_len = 0;
				dtls_data.read_error = NO_ERROR;
				dtls_data.transport_error = NO_ERROR;
				int len = callbacks.receive(buf, MAX_READ_BUF, callbacks.tx_context);

				if (len < 0)
				{
					handshake_failed();
					return IO_ERROR_GENERIC_RECEIVE;
				}

				if (len > 0)
				{
					int dtls_res = dtls_handle_message(dtls_context, &dst, buf, len);
					if (dtls_data.transport_error != NO_ERROR)
					{
						ProtocolError error = static_cast<ProtocolError>(dtls_data.transport_error);
						handshake_failed();
						return error;
					}
					if (dtls_res < 0)
					{
						LOG(WARN, "dtls_handle_message failed: %d", dtls_res);
						handshake_failed();
						return IO_ERROR_GENERIC_ESTABLISH;
					}
					if (dtls_data.read_error != NO_ERROR)
					{
						ProtocolError error = static_cast<ProtocolError>(dtls_data.read_error);
						handshake_failed();
						return error;
					}
					if (dtls_data.read_len > MAX_READ_BUF)
					{
						handshake_failed();
						return INSUFFICIENT_STORAGE;
					}
					memset(buf, 0, MAX_READ_BUF);
					memcpy(buf, dtls_data.read_buf, dtls_data.read_len);
				}

				dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);
				if (peer && peer->state == DTLS_STATE_CONNECTED)
				{
					ret = 0;
				}
				else
				{
					timeout_status = dtls_timing_get_delay(&time_cb);

					if (connection_status != timeout_status)
					{
						connection_status = timeout_status;

						if (connection_status == 1)
						{
							LOG(TRACE, "timeout\n");
							handshake_failed();
							return IO_ERROR_GENERIC_ESTABLISH;
						}
					}
				}

				/* Full and abbreviated handshakes both create a fresh peer/CID. */
				if (ret == 0)
				{
					LOG(TRACE, "valid session created");
					valid_dtls_session = true;
					handshake_succeeded();
#if DTLS_SESSION_TICKET
					if (peer->session_resumed)
					{
						LOG(INFO, "Session Ticket: abbreviated handshake completed");
						flags |= Protocol::SKIP_SESSION_RESUME_HELLO;
						return SESSION_RESUMED;
					}
#endif
					return SESSION_CONNECTED;
				}

				break;
			}
			}

			return NO_ERROR;
		}

		ProtocolError DTLSMessageChannel::notify_established()
		{
			return NO_ERROR;
		}

		ProtocolError DTLSMessageChannel::receive(Message &message)
		{
			dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);
			if (!peer)
			{
				return INVALID_STATE;
			}

			create(message);
			uint8_t *buf = message.buf();
			uint32_t buflen = (uint32_t)message.capacity();

			dtls_data.read_len = 0;
			dtls_data.read_error = NO_ERROR;
			dtls_data.transport_error = NO_ERROR;
			int len = callbacks.receive(buf, buflen, callbacks.tx_context);

			if (len < 0)
				return IO_ERROR_GENERIC_RECEIVE;

			if (len > 0)
			{
				int dtls_res = dtls_handle_message(dtls_context, &dst, buf, len);
				LOG(TRACE, "dtls_handle_message error %d, dtls_data.read_len %d", dtls_res, dtls_data.read_len);
				if (dtls_data.transport_error != NO_ERROR)
					return static_cast<ProtocolError>(dtls_data.transport_error);
				if (dtls_res < 0)
				{
					// UDP may deliver garbage / truncated DTLS records. Drop them
					// without tearing down an established session.
					LOG(WARN, "dtls_handle_message failed: %d (ignored)", dtls_res);
					dtls_data.read_len = 0;
				}
				else if (dtls_data.read_error != NO_ERROR)
					return static_cast<ProtocolError>(dtls_data.read_error);
			}

			if (dtls_data.read_len > buflen)
				return INSUFFICIENT_STORAGE;

			memset(buf, 0, buflen);
			memcpy(buf, dtls_data.read_buf, dtls_data.read_len);

			message.set_length(dtls_data.read_len);
			if (dtls_data.read_len > 0)
			{
#if defined(DEBUG_BUILD) && 0
				if (LOG_ENABLED(TRACE))
				{
					LOG(TRACE, "msg len %d", message.length());
					for (size_t i = 0; i < message.length(); i++)
					{
						char buf[3];
						char c = message.buf()[i];
						sprintf(buf, "%02x", c);
						LOG_PRINT(TRACE, buf);
					}
					LOG_PRINT(TRACE, "\r\n");
				}
#endif
			}
			return NO_ERROR;
		}

		ProtocolError DTLSMessageChannel::send(Message &message)
		{
			if (message.send_direct())
			{
				// send unencrypted
				int bytes = this->send(message.buf(), message.length());
				return bytes < 0 ? IO_ERROR_GENERIC_SEND : NO_ERROR;
			}

#if defined(DEBUG_BUILD) && 0
			LOG(TRACE, "msg len %d", message.length());
			for (size_t i = 0; i < message.length(); i++)
			{
				char buf[3];
				char c = message.buf()[i];
				sprintf(buf, "%02x", c);
				LOG_PRINT(TRACE, buf);
			}
			LOG_PRINT(TRACE, "\r\n");
#endif

			dtls_data.transport_error = NO_ERROR;
			int ret = dtls_write(dtls_context, &dst, message.buf(), message.length());
			if (dtls_data.transport_error != NO_ERROR)
				return static_cast<ProtocolError>(dtls_data.transport_error);
			return (ret >= 0 ? NO_ERROR : IO_ERROR_GENERIC_SEND);
		}

		bool DTLSMessageChannel::is_unreliable()
		{
			return true;
		}

		ProtocolError DTLSMessageChannel::command(Command command, void *arg)
		{
			switch (command)
			{
			case CLOSE:
				reset_session();
				break;

			case DISCARD_SESSION:
				reset_session();
				return IO_ERROR_DISCARD_SESSION; // force re-establish

			case LOAD_SESSION:
				// sessionPersist.restore(callbacks.restore);
				break;

			case SAVE_SESSION:
				// sessionPersist.save(callbacks.save);
				break;
			}
			return NO_ERROR;
		}
	}
}
