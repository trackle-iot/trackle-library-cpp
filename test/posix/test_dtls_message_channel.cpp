#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Include the implementation to exercise its file-local callbacks and key
 * parser. Unused channel methods are removed by the linker's dead-code pass.
 */
#include "../../src/dtls_message_channel.cpp"

#define CHECK(condition)                                                        \
	do                                                                          \
	{                                                                           \
		if (!(condition))                                                       \
		{                                                                       \
			fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); \
			return false;                                                       \
		}                                                                       \
	} while (0)

static void build_valid_private_key(uint8_t key[121])
{
	memset(key, 0, 121);
	size_t i = 0;

	key[i++] = 0x30;
	key[i++] = 0x77;
	key[i++] = 0x02;
	key[i++] = 0x01;
	key[i++] = 0x01;

	key[i++] = 0x04;
	key[i++] = 0x20;
	for (uint8_t value = 1; value <= 32; ++value)
	{
		key[i++] = value;
	}

	key[i++] = 0xa0;
	key[i++] = 0x0a;
	i += 10;

	key[i++] = 0xa1;
	key[i++] = 0x44;
	key[i++] = 0x03;
	key[i++] = 0x42;
	key[i++] = 0x00;
	key[i++] = 0x04;
	for (uint8_t value = 1; value <= 64; ++value)
	{
		key[i++] = value;
	}
}

static bool test_valid_private_key()
{
	uint8_t key[121];
	build_valid_private_key(key);

	CHECK(extract_pub_priv_keys(key, sizeof(key)));
	for (size_t i = 0; i < ECDSA_KEY_LENGTH; ++i)
	{
		CHECK(ecdsa_priv_key[i] == i + 1);
		CHECK(ecdsa_pub_key_x[i] == i + 1);
		CHECK(ecdsa_pub_key_y[i] == i + 33);
	}
	return true;
}

static bool test_rejects_malformed_private_keys()
{
	uint8_t key[121];
	build_valid_private_key(key);
	CHECK(!extract_pub_priv_keys(NULL, sizeof(key)));
	CHECK(!extract_pub_priv_keys(key, sizeof(key) - 1));

	build_valid_private_key(key);
	key[0] = 0x31;
	CHECK(!extract_pub_priv_keys(key, sizeof(key)));

	build_valid_private_key(key);
	key[1] = 0x78;
	CHECK(!extract_pub_priv_keys(key, sizeof(key)));

	build_valid_private_key(key);
	key[6] = ECDSA_KEY_LENGTH + 1;
	CHECK(!extract_pub_priv_keys(key, sizeof(key)));

	build_valid_private_key(key);
	key[56] = 0x02;
	CHECK(!extract_pub_priv_keys(key, sizeof(key)));
	return true;
}

static bool test_plaintext_buffer_bounds()
{
	dtls_context_t context = {};
	Dtls_data data = {};
	uint8_t payload[PROTOCOL_BUFFER_SIZE + 1];
	memset(payload, 0xa5, sizeof(payload));
	context.app = &data;

	CHECK(read_from_peer(&context, NULL, payload, PROTOCOL_BUFFER_SIZE) == 0);
	CHECK(data.read_len == PROTOCOL_BUFFER_SIZE);
	CHECK(data.read_error == trackle::protocol::NO_ERROR);
	CHECK(memcmp(data.read_buf, payload, PROTOCOL_BUFFER_SIZE) == 0);

	CHECK(read_from_peer(&context, NULL, payload, sizeof(payload)) < 0);
	CHECK(data.read_len == 0);
	CHECK(data.read_error == trackle::protocol::INSUFFICIENT_STORAGE);
	return true;
}

static int failing_send(const unsigned char *, size_t, void *)
{
	return -1;
}

static bool test_send_error_propagation()
{
	dtls_context_t context = {};
	Dtls_data data = {};
	uint8_t payload = 0;
	context.app = &data;
	data.send = failing_send;

	CHECK(send_to_peer(&context, NULL, &payload, sizeof(payload)) < 0);
	CHECK(data.transport_error == trackle::protocol::IO_ERROR_GENERIC_SEND);
	return true;
}

int main()
{
	static_assert(PUBLIC_KEY_LENGTH == DTLS_PUBLIC_KEY_LENGTH,
				  "Trackle and tinyDTLS server key lengths must match");

	const struct
	{
		const char *name;
		bool (*run)();
	} tests[] = {
		{"valid private key", test_valid_private_key},
		{"malformed private keys", test_rejects_malformed_private_keys},
		{"plaintext buffer bounds", test_plaintext_buffer_bounds},
		{"send error propagation", test_send_error_propagation},
	};

	for (const auto &test : tests)
	{
		if (!test.run())
		{
			return 1;
		}
		printf("PASS: %s\n", test.name);
	}
	return 0;
}
