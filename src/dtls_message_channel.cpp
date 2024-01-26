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

uint8_t malformed[15] = {0x16, 0xfe, 0xfd, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
uint8_t malformed_counter = 0;
bool valid_dtls_session = false;

void extract_pub_priv_keys(const uint8_t *key)
{
	uint8_t len = key[1];

	int i = 2;
	while (i < (2 + len))
	{
		if (key[i] == 0x04)
		{
			int key_len = key[i + 1];
			memcpy(ecdsa_priv_key + (DTLS_EC_KEY_SIZE - key_len), key + i + 2, key_len);
		}
		else if (key[i] == 0xa1)
		{
			memcpy(ecdsa_pub_key_x, key + i + 6, 32);
			memcpy(ecdsa_pub_key_y, key + i + 32 + 6, 32);
		}
		i += (2 + key[i + 1]);
	}
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
	t_dtls_data->read_len = (int)len;

	memset(t_dtls_data->read_buf, 0, sizeof(t_dtls_data->read_buf));
	memcpy(t_dtls_data->read_buf, data, len);

	return 0;
}

static int send_to_peer(struct dtls_context_t *ctx,
						session_t *session, uint8 *data, size_t len)
{

	Dtls_data *t_dtls_data = (Dtls_data *)ctx->app;
	size_t res = t_dtls_data->send(data, (int)len, t_dtls_data->channel);
	return (int)res;
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
			const uint8_t *device_id, Callbacks &callbacks,
			message_id_t *coap_state)
		{
			init();

			this->coap_state = coap_state;
			this->callbacks = callbacks;
			this->device_id = device_id;

			getMillis = callbacks.millis;

			dtls_init();

			extract_pub_priv_keys(core_private); // extract client public and private key

			// copy server public key
			memcpy(server_certificate, server_public, server_public_len);

			static dtls_handler_t cb = {
				.write = send_to_peer,
				.read = read_from_peer,
				.event = dtls_event,
				//.event = NULL,
				.get_psk_info = NULL,
				.get_server_certificate = get_server_certificate,
				.get_ecdsa_key = get_ecdsa_key,
				.verify_ecdsa_key = verify_ecdsa_key,
			};

			dtls_data.send = sendCallback; // send callback
			dtls_data.channel = (void *)this;

			dtls_context = dtls_new_context(&dtls_data);
			if (!dtls_context)
			{
				LOG(TRACE, "Cannot create context");
				exit(-1);
			}

			dtls_set_handler(dtls_context, &cb);
			return NO_ERROR;
		}

		/*
		 * Inspects the move session flag to amend the application data record to a move session record.
		 * See: https://github.com/trackle-iot/knowledge/blob/8df146d88c4237e90553f3fd6d8465ab58ec79e0/services/dtls-ip-change.md
		 */
		inline int DTLSMessageChannel::send(const uint8_t *data, size_t len)
		{
			// if (move_session && len && data[0] == 0x70)
			if (move_session && len && data[0] == 23)
			{
				LOG(TRACE, "DTLSMessageChannel -> move_session");
				// buffer for a new packet that contains the device ID length and a byte for the length appended to the existing data.
				uint8_t d[len + DEVICE_ID_LEN + 1];
				memcpy(d, data, len);					   // original application data
				d[0] = 254;								   // move session record type
				memcpy(d + len, device_id, DEVICE_ID_LEN); // set the device ID
				d[len + DEVICE_ID_LEN] = DEVICE_ID_LEN;	   // set the device ID length as the last byte in the packet
				int result = callbacks.send(d, len + DEVICE_ID_LEN + 1, callbacks.tx_context);
				// hide the increased length from DTLS
				if (result == int(len + DEVICE_ID_LEN + 1))
					result = len;
				return result;
			}
			else
				return callbacks.send(data, len, callbacks.tx_context);
		}

		void DTLSMessageChannel::reset_session()
		{
			LOG(TRACE, "DTLSMessageChannel::reset_session");
			cancel_move_session();

			dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);
			if (peer)
			{
				dtls_reset_peer(dtls_context, peer);
			}
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
			int ret = -1;

#define MAX_READ_BUF 1000
			static uint8 buf[MAX_READ_BUF];

			static dtls_timing_context time_cb;
			int8_t connection_status = -1;
			int8_t timeout_status = 0;
			int res = 0;
			bool toConnect = false;

			switch (this->status)
			{
			case INIT:
			{

				dtls_timing_set_delay(&time_cb, this->handshake_timeout);

				/* delete peer if not connected */
				dtls_peer_t *peer = dtls_get_peer(dtls_context, &dst);

				if (!peer)
				{
					LOG(TRACE, "peer not extists");
					toConnect = true;
				}
				else if (peer && peer->state != DTLS_STATE_CONNECTED)
				{
					LOG(TRACE, "dtls_reset_peer");
					peer->state = DTLS_STATE_CLOSING;
					dtls_reset_peer(dtls_context, peer);
					toConnect = true;
				}
				else
				{
					LOG(TRACE, "DTLS_STATE_CONNECTED");
					toConnect = false;
				}

				if (toConnect)
				{
					res = dtls_connect(dtls_context, &dst);
					LOG(TRACE, "dtls_connect: %d", res);
				}

				if (res < 0)
				{
					LOG(TRACE, "dtls_connect error %d", res);
					return IO_ERROR_GENERIC_ESTABLISH;
				}
				else if (res == 0)
				{
					// resume session
					LOG(TRACE, "trying to restore session....");
					flags |= Protocol::SKIP_SESSION_RESUME_HELLO;
					return SESSION_RESUMED;
				}
				else
				{
					LOG(TRACE, "starting handshake");
					this->status = HANDSHAKE;
				}

				break;
			}

			case HANDSHAKE:
			{

				dtls_data.read_len = 0;
				int len = callbacks.receive(buf, MAX_READ_BUF, callbacks.tx_context);

				if (len > 0)
				{
					dtls_handle_message(dtls_context, &dst, buf, len);
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
							return IO_ERROR_GENERIC_ESTABLISH;
						}
					}
				}

				/* new session created */
				if (ret == 0)
				{
					LOG(TRACE, "valid session created");
					valid_dtls_session = true;
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
			int len = callbacks.receive(buf, buflen, callbacks.tx_context);

			if (len > 0)
			{
				dtls_handle_message(dtls_context, &dst, buf, len);
				memset(buf, 0, buflen);
				memcpy(buf, dtls_data.read_buf, dtls_data.read_len);

				// check malformed TODO add check len 15
				int res = memcmp(dtls_data.read_buf, malformed, dtls_data.read_len);
				if (res == 0)
				{
					LOG(TRACE, "Malformed dtls packet");
					malformed_counter++;

					// todo scrivere funzionamento
					if (malformed_counter == 1)
					{
						LOG(TRACE, "Handle ip change");
						this->command(MessageChannel::MOVE_SESSION, nullptr);

						// send ping
						Message message;
						create(message);
						size_t len = Messages::ping(message.buf(), 0);
						message.set_length(len);
						send(message);

						return NO_ERROR;
					}
					else
					{
						LOG(TRACE, "Too much malformed packet, disconnecting.....");
						this->command(MessageChannel::CLOSE, nullptr);

						return IO_ERROR_GENERIC_RECEIVE;
					}
				}
				else // packet ok
				{
					malformed_counter = 0;
				}
			}

			message.set_length(dtls_data.read_len);
			if (dtls_data.read_len > 0)
			{
				cancel_move_session();
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

		/**
		 * Once data has been successfully received we can stop
		 * sending move-session messages.
		 * This is also used to reset the expiration counter.
		 */
		void DTLSMessageChannel::cancel_move_session()
		{
			LOG(TRACE, "cancel_move_session");

			if (move_session)
			{
				move_session = false;
				command(SAVE_SESSION);
			}
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

			int ret = dtls_write(dtls_context, &dst, message.buf(), message.length());
			return (ret >= 0 ? NO_ERROR : IO_ERROR_GENERIC_ESTABLISH);
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

			case MOVE_SESSION:
				move_session = true;
				break;

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
