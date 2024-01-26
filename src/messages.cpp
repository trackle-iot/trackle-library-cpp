#include "messages.h"

namespace trackle
{
	namespace protocol
	{

		CoAPMessageType::Enum Messages::decodeType(const uint8_t *buf, size_t length)
		{
			if (length < 4)
				return CoAPMessageType::ERROR;

			char path = 0;
			// 4 bytes for CoAP header
			// 1 byte for the option length
			// plus length of token
			size_t path_idx = 5 + (buf[0] & 0x0F);
			if (path_idx < length)
				path = buf[path_idx];

			switch (CoAP::code(buf))
			{
			case CoAPCode::GET:
				switch (path)
				{
				case 'v':
					return CoAPMessageType::VARIABLE_REQUEST;
				case 'd':
					return CoAPMessageType::DESCRIBE;
				default:
					break;
				}
				break;
			case CoAPCode::POST:
				switch (path)
				{
				case 'E':
				case 'e':
					return CoAPMessageType::EVENT;
				case 'h':
					return CoAPMessageType::HELLO;
				case 'f':
					return CoAPMessageType::FUNCTION_CALL;
				case 's':
					return CoAPMessageType::SAVE_BEGIN;
				case 'u':
					return CoAPMessageType::UPDATE_BEGIN;
				case 'c':
					return CoAPMessageType::CHUNK;
				default:
					break;
				}
				break;
			case CoAPCode::PUT:
				switch (path)
				{
				case 'k':
					return CoAPMessageType::KEY_CHANGE;
				case 'p':
					return CoAPMessageType::UPDATE_PROPERTY;
				case 'u':
					return CoAPMessageType::UPDATE_DONE;
				case 's':
					// todo - use a single message SIGNAL and decode the rest of the message to determine desired state
					if (buf[8])
						return CoAPMessageType::SIGNAL_START;
					else
						return CoAPMessageType::SIGNAL_STOP;
				default:
					break;
				}
				break;
			case CoAPCode::EMPTY:
				switch (CoAP::type(buf))
				{
				case CoAPType::CON:
					return CoAPMessageType::PING;
				default:
					return CoAPMessageType::EMPTY_ACK;
				}
				break;
			// todo - we should look at the original request (via the token) to determine the type of the response.
			case CoAPCode::CONTENT:
				return CoAPMessageType::TIME;
			default:
				break;
			}
			return CoAPMessageType::ERROR;
		}

		size_t Messages::hello(uint8_t *buf, message_id_t message_id, uint8_t flags,
							   uint16_t platform_id, uint16_t product_id,
							   uint16_t product_firmware_version, bool confirmable, const uint8_t *device_id, uint16_t device_id_len)
		{
			// TODO: why no token? because the response is not sent separately. But really we should use a token for all messages that expect a response.
			buf[0] = COAP_MSG_HEADER(confirmable ? CoAPType::CON : CoAPType::NON, 0);
			buf[1] = 0x02; // POST
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			buf[4] = 0xb1; // Uri-Path option of length 1
			buf[5] = 'h';
			buf[6] = 0xff; // payload marker
			buf[7] = product_id >> 8;
			buf[8] = product_id & 0xff;
			buf[9] = product_firmware_version >> 8;
			buf[10] = product_firmware_version & 0xff;
			buf[11] = 0; // reserved flags
			buf[12] = flags;
			buf[13] = platform_id >> 8;
			buf[14] = platform_id & 0xFF;
			size_t len = 15;
			if (device_id)
			{
				buf[15] = device_id_len >> 8;
				buf[16] = device_id_len & 0xFF;
				len += 2;
				for (size_t i = 0; i < device_id_len; i++)
				{
					buf[len++] = device_id[i];
				}
			}
			return len;
		}

		size_t Messages::update_done(uint8_t *buf, message_id_t message_id, const uint8_t *result, size_t result_len, bool confirmable)
		{
			// why not with a token? this is sent in response to the server's UpdateDone message.
			size_t sz = 6;
			buf[0] = confirmable ? 0x40 : 0x50; // confirmable/non-confirmable, no token
			buf[1] = 0x03;						// PUT
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			buf[4] = 0xb1; // Uri-Path option of length 1
			buf[5] = 'u';
			if (result && result_len)
			{
				buf[sz++] = 0xff; // payload marker
				memcpy(buf + sz, result, result_len);
				sz += result_len;
			}
			return sz;
		}

		size_t Messages::update_done(uint8_t *buf, message_id_t message_id, bool confirmable)
		{
			return update_done(buf, message_id, NULL, 0, confirmable);
		}

		size_t Messages::function_return(unsigned char *buf, message_id_t message_id, token_t token, int return_value, bool confirmable)
		{
			buf[0] = confirmable ? 0x41 : 0x51; // non-confirmable, one-byte token
			buf[1] = 0x44;						// response code 2.04 CHANGED
			buf[2] = (message_id >> 8) & 0xff;
			buf[3] = (message_id >> 0) & 0xff;
			buf[4] = token;
			buf[5] = 0xff; // payload marker
			buf[6] = (return_value >> 24) & 0xff;
			buf[7] = (return_value >> 16) & 0xff;
			buf[8] = (return_value >> 8) & 0xff;
			buf[9] = (return_value >> 0) & 0xff;
			return function_return_size;
		}

		size_t Messages::variable_value(unsigned char *buf, message_id_t message_id, token_t token, bool return_value)
		{
			size_t size = content(buf, message_id, token);
			buf[size++] = return_value ? 1 : 0;
			return size;
		}

		size_t Messages::variable_value(unsigned char *buf, message_id_t message_id, token_t token, int32_t return_value)
		{
			size_t size = content(buf, message_id, token);
			buf[size++] = (return_value >> 24) & 0xff;
			buf[size++] = (return_value >> 16) & 0xff;
			buf[size++] = (return_value >> 8) & 0xff;
			buf[size++] = (return_value >> 0) & 0xff;
			return size;
		}

		size_t Messages::variable_value(unsigned char *buf, message_id_t message_id, token_t token, int64_t return_value)
		{
			size_t size = content(buf, message_id, token);
			buf[size++] = (return_value >> 40) & 0xff;
			buf[size++] = (return_value >> 32) & 0xff;
			buf[size++] = (return_value >> 24) & 0xff;
			buf[size++] = (return_value >> 16) & 0xff;
			buf[size++] = (return_value >> 8) & 0xff;
			buf[size++] = (return_value >> 0) & 0xff;
			return size;
		}

		size_t Messages::variable_value(unsigned char *buf, message_id_t message_id, token_t token, double return_value)
		{
			size_t size = content(buf, message_id, token);
			memcpy(buf + size, &return_value, 8);
			return size + sizeof(double);
		}

		// Returns the length of the buffer to send
		size_t Messages::variable_value(unsigned char *buf, message_id_t message_id, token_t token, const void *return_value, int length)
		{
			size_t size = content(buf, message_id, token);
			memcpy(buf + size, return_value, length);
			return size + length;
		}

		size_t Messages::time_request(uint8_t *buf, uint16_t message_id, uint8_t token)
		{
			unsigned char *p = buf;

			*p++ = 0x41; // Confirmable, one-byte token
			*p++ = 0x01; // GET request

			*p++ = message_id >> 8;
			*p++ = message_id & 0xff;

			*p++ = token;
			*p++ = 0xb1; // One-byte, Uri-Path option
			*p++ = 't';

			return p - buf;
		}

		size_t Messages::chunk_missed(uint8_t *buf, uint16_t message_id, chunk_index_t chunk_index)
		{
			buf[0] = 0x40; // confirmable, no token
			buf[1] = 0x01; // code 0.01 GET
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			buf[4] = 0xb1; // one-byte Uri-Path option
			buf[5] = 'c';
			buf[6] = 0xff; // payload marker
			buf[7] = chunk_index >> 8;
			buf[8] = chunk_index & 0xff;
			return 9;
		}

		size_t Messages::content(uint8_t *buf, uint16_t message_id, uint8_t token)
		{
			buf[0] = 0x61; // acknowledgment, one-byte token
			buf[1] = 0x45; // response code 2.05 CONTENT
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			buf[4] = token;
			buf[5] = 0xff; // payload marker
			return 6;
		}

		size_t Messages::keep_alive(uint8_t *buf)
		{
			buf[0] = 0x70; // dumb ping, "p"
			return 1;
		}

		size_t Messages::ping(uint8_t *buf, uint16_t message_id)
		{
			buf[0] = 0x40; // Confirmable, no token
			buf[1] = 0x00; // code signifying empty message
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			return 4;
		}

		size_t Messages::presence_announcement(unsigned char *buf, const char *id)
		{
			buf[0] = 0x50; // Non-Confirmable, no token
			buf[1] = 0x02; // Code POST
			buf[2] = 0x00; // message id ignorable in this context
			buf[3] = 0x00;
			buf[4] = 0xb1; // Uri-Path option of length 1
			buf[5] = 'h';
			buf[6] = 0xff; // payload marker
			memcpy(buf + 7, id, 12);
			return 19;
		}

		size_t Messages::describe_post_header(uint8_t buf[], size_t buffer_size, uint16_t message_id, uint8_t desc_flags)
		{
			const size_t header_size = 9;

			size_t bytes_written;

			if (buffer_size < header_size)
			{
				bytes_written = 0;
			}
			else
			{
				buf[0] = 0x40; // Confirmable, no token
				buf[1] = 0x02; // Type POST
				buf[2] = message_id >> 8;
				buf[3] = message_id & 0xff;
				buf[4] = 0xb1; // Uri-Path option of length 1
				buf[5] = 'd';
				buf[6] = 0x41; // Uri-Query option of length 1
				buf[7] = desc_flags;
				buf[8] = 0xff; // payload marker
				bytes_written = header_size;
			}

			return bytes_written;
		}

		size_t Messages::separate_response_with_payload(unsigned char *buf, uint16_t message_id,
														unsigned char token, unsigned char code, unsigned char *payload,
														unsigned payload_len, bool confirmable)
		{
			buf[0] = confirmable ? 0x41 : 0x51; // confirmable/non-confirmable, one-byte token
			buf[1] = code;
			buf[2] = message_id >> 8;
			buf[3] = message_id & 0xff;
			buf[4] = token;

			size_t len = 5;
			if (payload && payload_len)
			{
				buf[5] = 0xFF;
				memcpy(buf + 6, payload, payload_len);
				len += 1 + payload_len;
			}
			return len;
		}

		size_t Messages::event(uint8_t buf[], uint16_t message_id, uint8_t token, const char *event_name,
							   const char *data, uint16_t length, int ttl, uint8_t block_id,
							   uint8_t block_num, EventType::Enum event_type, bool confirmable)
		{

			uint8_t *p = buf;

			uint8_t cmd = confirmable ? 0x40 : 0x50; // confirmable / non-confirmable, no token
			if (token > 0)
				cmd = cmd + 1; // add 1 byte token

			*p++ = cmd;	 // non-confirmable /confirmable, token
			*p++ = 0x02; // code 0.02 POST request
			*p++ = message_id >> 8;
			*p++ = message_id & 0xff;

			if (token > 0)
				*p++ = token; // add 1 byte token

			*p++ = 0xb1; // one-byte Uri-Path option
			*p++ = event_type;

			// Add event name as URI in message
			size_t name_data_len = strnlen(event_name, MAX_EVENT_NAME_LENGTH);
			p += event_name_uri_path(p, event_name, name_data_len);

			// Block1 option if no TTL option
			uint8_t block1_opt_delta[2] = {0xd1, 0x03};

			if (60 != ttl)
			{
				*p++ = 0x33;
				*p++ = (ttl >> 16) & 0xff;
				*p++ = (ttl >> 8) & 0xff;
				*p++ = ttl & 0xff;

				// Block1 option if TTL option before
				block1_opt_delta[0] = 0xd1;
				block1_opt_delta[1] = 0x00;
			}

			// Specify block1 option value
			if (block_num > 1)
			{
				*p++ = block1_opt_delta[0];
				*p++ = block1_opt_delta[1];

				uint8_t blockOptByte = 0x06;	// 1024 bytes block size
				blockOptByte |= block_id << 4;	// put actual block sequence number in first 4 bits
				if ((block_id + 1) < block_num) // block_id starts from 0
				{
					blockOptByte |= 0x08; // if NOT last block, set 4th bit to 1 ("MORE blocks follow")
				}
				*p++ = blockOptByte;
			}

			// Copy payload block in packet
			if (NULL != data)
			{
				*p++ = 0xff;
				memcpy(p, data, length);
				p += length;
			}

			return p - buf;
		}

		size_t Messages::coded_ack(uint8_t *buf, uint8_t token, uint8_t code,
								   uint8_t message_id_msb, uint8_t message_id_lsb,
								   uint8_t *data, size_t data_len)
		{
			size_t sz = Messages::coded_ack(buf, token, code, message_id_msb, message_id_lsb);
			if (data && data_len)
			{
				buf[sz++] = 0xff; // Payload marker
				memcpy(buf + sz, data, data_len);
				sz += data_len;
			}

			return sz;
		}

		block_messages_data block_messages[MAX_CONCURRENT_MESSAGES];

		block_messages_data *trackle_get_free_block()
		{
			for (uint8_t i = 0; i < MAX_CONCURRENT_MESSAGES; i++)
			{
				if (block_messages[i].transmissionRunning == false)
				{
					return &block_messages[i];
				}
			}

			return NULL;
		}

		block_messages_data *trackle_get_block_by_token(uint8_t token)
		{
			for (uint8_t i = 0; i < MAX_CONCURRENT_MESSAGES; i++)
			{
				if (block_messages[i].token == token)
				{
					return &block_messages[i];
				}
			}

			return NULL;
		}

	}
}
