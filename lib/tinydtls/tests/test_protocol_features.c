#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../dtls.c"
#include "../tinydtls_set_get_millis.h"
#include "../tinydtls_set_rand.h"

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                       \
    fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition);   \
    return 0;                                                               \
  }                                                                         \
} while (0)

#define MAX_CAPTURED_RECORDS 16

typedef struct {
  uint8_t data[MAX_CAPTURED_RECORDS][DTLS_MAX_BUF];
  size_t length[MAX_CAPTURED_RECORDS];
  size_t count;
} capture_t;

typedef struct {
  const uint8_t *random;
  const uint8_t *session_id;
  uint8_t session_id_length;
  const uint8_t *ticket;
  uint16_t ticket_length;
} parsed_client_hello_t;

static capture_t capture;
static uint32_t fake_millis;
static uint32_t fake_random = 0x12345678;

static uint32_t
test_rand(void)
{
  fake_random = fake_random * 1664525u + 1013904223u;
  return fake_random;
}

static void
test_get_millis(uint32_t *value)
{
  *value = fake_millis;
}

static int
test_write(struct dtls_context_t *ctx, session_t *session,
           uint8_t *data, size_t length)
{
  size_t index = capture.count;
  (void)ctx;
  (void)session;

  if (index >= MAX_CAPTURED_RECORDS || length > DTLS_MAX_BUF)
    return -1;
  memcpy(capture.data[index], data, length);
  capture.length[index] = length;
  capture.count++;
  return (int)length;
}

static int
test_verify_key(struct dtls_context_t *ctx, const session_t *session,
                const unsigned char *x, const unsigned char *y,
                size_t key_size)
{
  (void)ctx;
  (void)session;
  (void)x;
  (void)y;
  (void)key_size;
  return 0;
}

static void
reset_capture(void)
{
  memset(&capture, 0, sizeof(capture));
}

static void
setup_context(dtls_context_t *ctx, dtls_handler_t *handler)
{
  memset(ctx, 0, sizeof(*ctx));
  memset(handler, 0, sizeof(*handler));
  handler->write = test_write;
  handler->verify_ecdsa_key = test_verify_key;
  ctx->h = handler;
}

static dtls_peer_t *
new_client_peer(void)
{
  session_t session;
  dtls_peer_t *peer;

  memset(&session, 0, sizeof(session));
  peer = dtls_new_peer(&session);
  if (!peer)
    return NULL;
  peer->role = DTLS_CLIENT;
  peer->handshake_params = dtls_handshake_new();
  if (!peer->handshake_params) {
    dtls_free_peer(peer);
    return NULL;
  }
  return peer;
}

static void
set_ticket(dtls_session_ticket_t *ticket, const char *value,
           uint32_t lifetime_hint)
{
  size_t length = strlen(value);

  memset(ticket, 0, sizeof(*ticket));
  memcpy(ticket->ticket, value, length);
  ticket->ticket_length = (uint16_t)length;
  ticket->lifetime_hint = lifetime_hint;
  ticket->received_at = fake_millis;
  memset(ticket->master_secret, 0x5a, DTLS_MASTER_SECRET_LENGTH);
  ticket->cipher_suite = TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8;
  ticket->extended_master_secret = 1;
  ticket->valid = 1;
}

static int
parse_client_hello(const uint8_t *record, size_t record_length,
                   parsed_client_hello_t *parsed)
{
  const uint8_t *p;
  const uint8_t *end = record + record_length;
  uint16_t field_length;

  memset(parsed, 0, sizeof(*parsed));
  if (record_length < DTLS_RH_LENGTH + DTLS_HS_LENGTH + 35 ||
      record[0] != DTLS_CT_HANDSHAKE ||
      record[DTLS_RH_LENGTH] != DTLS_HT_CLIENT_HELLO)
    return 0;

  p = record + DTLS_RH_LENGTH + DTLS_HS_LENGTH;
  p += sizeof(uint16_t);
  parsed->random = p;
  p += DTLS_RANDOM_LENGTH;
  if (p >= end || *p > DTLS_SESSION_ID_LENGTH)
    return 0;
  parsed->session_id_length = *p++;
  if ((size_t)(end - p) < parsed->session_id_length)
    return 0;
  parsed->session_id = p;
  p += parsed->session_id_length;

  if (p >= end || (size_t)(end - p - 1) < p[0])
    return 0;
  p += sizeof(uint8_t) + p[0];

  if ((size_t)(end - p) < sizeof(uint16_t))
    return 0;
  field_length = dtls_uint16_to_int(p);
  p += sizeof(uint16_t);
  if ((size_t)(end - p) < field_length)
    return 0;
  p += field_length;

  if (p >= end || (size_t)(end - p - 1) < p[0])
    return 0;
  p += sizeof(uint8_t) + p[0];

  if ((size_t)(end - p) < sizeof(uint16_t))
    return 0;
  field_length = dtls_uint16_to_int(p);
  p += sizeof(uint16_t);
  if ((size_t)(end - p) != field_length)
    return 0;

  while (field_length >= 4) {
    uint16_t type = dtls_uint16_to_int(p);
    uint16_t length = dtls_uint16_to_int(p + 2);
    p += 4;
    field_length -= 4;
    if (length > field_length)
      return 0;
    if (type == TLS_EXT_SESSION_TICKET) {
      parsed->ticket = p;
      parsed->ticket_length = length;
    }
    p += length;
    field_length -= length;
  }
  return field_length == 0;
}

static uint8_t *
add_extension(uint8_t *p, uint16_t type,
              const uint8_t *value, uint16_t length)
{
  dtls_int_to_uint16(p, type);
  dtls_int_to_uint16(p + 2, length);
  if (length)
    memcpy(p + 4, value, length);
  return p + 4 + length;
}

static size_t
build_server_hello(uint8_t *message, size_t capacity,
                   const dtls_handshake_parameters_t *handshake,
                   int resume, int renew_ticket, int include_cid)
{
  uint8_t *p;
  uint8_t *extensions_length;
  uint8_t raw_public_key = TLS_CERT_TYPE_RAW_PUBLIC_KEY;
  uint8_t renegotiation_info = 0;
  uint8_t cid[] = { 4, 'c', 'i', 'd', '2' };
  size_t body_length;
  size_t extensions_size;

  if (capacity < 128)
    return 0;
  memset(message, 0, capacity);
  p = message + DTLS_HS_LENGTH;
  dtls_int_to_uint16(p, DTLS_VERSION);
  p += sizeof(uint16_t);
  memset(p, 0x33, DTLS_RANDOM_LENGTH);
  p += DTLS_RANDOM_LENGTH;

  if (resume) {
    *p++ = handshake->session_id_length;
    memcpy(p, handshake->session_id, handshake->session_id_length);
    p += handshake->session_id_length;
  } else {
    *p++ = 0;
  }

  dtls_int_to_uint16(p, TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8);
  p += sizeof(uint16_t);
  *p++ = TLS_COMPRESSION_NULL;

  extensions_length = p;
  p += sizeof(uint16_t);
  p = add_extension(p, TLS_EXT_EXTENDED_MASTER_SECRET, NULL, 0);
  p = add_extension(p, TLS_EXT_RENEGOTIATION_INFO,
                    &renegotiation_info, sizeof(renegotiation_info));
  if (!resume)
    p = add_extension(p, TLS_EXT_SERVER_CERTIFICATE_TYPE,
                      &raw_public_key, sizeof(raw_public_key));
  if (renew_ticket)
    p = add_extension(p, TLS_EXT_SESSION_TICKET, NULL, 0);
  if (include_cid)
    p = add_extension(p, TLS_EXT_CONNECTION_ID, cid, sizeof(cid));

  extensions_size = (size_t)(p - extensions_length - sizeof(uint16_t));
  dtls_int_to_uint16(extensions_length, extensions_size);
  body_length = (size_t)(p - message - DTLS_HS_LENGTH);

  message[0] = DTLS_HT_SERVER_HELLO;
  dtls_int_to_uint24(message + 1, body_length);
  dtls_int_to_uint24(message + 6, 0);
  dtls_int_to_uint24(message + 9, body_length);
  return DTLS_HS_LENGTH + body_length;
}

static size_t
build_new_session_ticket(uint8_t *message, size_t capacity,
                         uint32_t lifetime_hint, const char *ticket)
{
  uint8_t *p;
  size_t ticket_length = strlen(ticket);
  size_t body_length = sizeof(uint32_t) + sizeof(uint16_t) + ticket_length;

  if (capacity < DTLS_HS_LENGTH + body_length)
    return 0;
  memset(message, 0, DTLS_HS_LENGTH + body_length);
  message[0] = DTLS_HT_NEW_SESSION_TICKET;
  dtls_int_to_uint24(message + 1, body_length);
  dtls_int_to_uint24(message + 9, body_length);
  p = message + DTLS_HS_LENGTH;
  dtls_int_to_uint32(p, lifetime_hint);
  p += sizeof(uint32_t);
  dtls_int_to_uint16(p, ticket_length);
  p += sizeof(uint16_t);
  memcpy(p, ticket, ticket_length);
  return DTLS_HS_LENGTH + body_length;
}

static size_t
build_server_finished(dtls_peer_t *peer, uint8_t *message, size_t capacity)
{
  dtls_hash_ctx hash;
  uint8_t digest[DTLS_HMAC_MAX];
  size_t digest_length;

  if (capacity < DTLS_HS_LENGTH + DTLS_FIN_LENGTH)
    return 0;
  memset(message, 0, DTLS_HS_LENGTH + DTLS_FIN_LENGTH);
  copy_hs_hash(peer, &hash);
  digest_length = dtls_hash_finalize(digest, &hash);
  message[0] = DTLS_HT_FINISHED;
  dtls_int_to_uint24(message + 1, DTLS_FIN_LENGTH);
  dtls_int_to_uint24(message + 9, DTLS_FIN_LENGTH);
  dtls_prf(peer->handshake_params->tmp.master_secret,
           DTLS_MASTER_SECRET_LENGTH,
           PRF_LABEL(server), PRF_LABEL_SIZE(server),
           PRF_LABEL(finished), PRF_LABEL_SIZE(finished),
           digest, digest_length,
           message + DTLS_HS_LENGTH, DTLS_FIN_LENGTH);
  return DTLS_HS_LENGTH + DTLS_FIN_LENGTH;
}

static size_t
queue_size(const netq_t *queue)
{
  size_t count = 0;
  while (queue) {
    count++;
    queue = queue->next;
  }
  return count;
}

static int
test_ticket_offer_and_hello_verify_retry(void)
{
  dtls_context_t ctx;
  dtls_handler_t handler;
  dtls_peer_t *peer;
  parsed_client_hello_t first;
  parsed_client_hello_t retry;
  uint8_t first_random[DTLS_RANDOM_LENGTH];
  uint8_t first_session_id[DTLS_SESSION_ID_LENGTH];
  uint8_t cookie[] = { 1, 2, 3, 4 };

  setup_context(&ctx, &handler);
  peer = new_client_peer();
  CHECK(peer != NULL);
  set_ticket(&ctx.session_ticket, "ticket-one", 3600);

  reset_capture();
  CHECK(dtls_send_client_hello(&ctx, peer, NULL, 0) > 0);
  CHECK(capture.count == 1);
  CHECK(parse_client_hello(capture.data[0], capture.length[0], &first));
  CHECK(first.session_id_length == DTLS_SESSION_ID_LENGTH);
  CHECK(first.ticket_length == strlen("ticket-one"));
  CHECK(memcmp(first.ticket, "ticket-one", first.ticket_length) == 0);
  memcpy(first_random, first.random, sizeof(first_random));
  memcpy(first_session_id, first.session_id, sizeof(first_session_id));

  reset_capture();
  CHECK(dtls_send_client_hello(&ctx, peer, cookie, sizeof(cookie)) > 0);
  CHECK(capture.count == 1);
  CHECK(parse_client_hello(capture.data[0], capture.length[0], &retry));
  CHECK(memcmp(retry.random, first_random, sizeof(first_random)) == 0);
  CHECK(retry.session_id_length == sizeof(first_session_id));
  CHECK(memcmp(retry.session_id, first_session_id,
               sizeof(first_session_id)) == 0);
  CHECK(retry.ticket_length == strlen("ticket-one"));
  CHECK(memcmp(retry.ticket, "ticket-one", retry.ticket_length) == 0);

  dtls_stop_retransmission(&ctx, peer);
  dtls_free_peer(peer);
  return 1;
}

static int
test_expired_ticket_is_not_offered(void)
{
  dtls_context_t ctx;
  dtls_handler_t handler;
  dtls_peer_t *peer;
  parsed_client_hello_t hello;

  setup_context(&ctx, &handler);
  peer = new_client_peer();
  CHECK(peer != NULL);
  fake_millis = 1000;
  set_ticket(&ctx.session_ticket, "expired", 1);
  fake_millis = 2500;

  reset_capture();
  CHECK(dtls_send_client_hello(&ctx, peer, NULL, 0) > 0);
  CHECK(parse_client_hello(capture.data[0], capture.length[0], &hello));
  CHECK(hello.session_id_length == 0);
  CHECK(hello.ticket != NULL);
  CHECK(hello.ticket_length == 0);
  CHECK(!ctx.session_ticket.valid);

  dtls_stop_retransmission(&ctx, peer);
  dtls_free_peer(peer);
  return 1;
}

static int
test_ticket_rejection_falls_back_to_full_handshake(void)
{
  dtls_context_t ctx;
  dtls_handler_t handler;
  dtls_peer_t *peer;
  uint8_t server_hello[160];
  size_t server_hello_length;

  setup_context(&ctx, &handler);
  peer = new_client_peer();
  CHECK(peer != NULL);
  set_ticket(&ctx.session_ticket, "rejected", 3600);
  reset_capture();
  CHECK(dtls_send_client_hello(&ctx, peer, NULL, 0) > 0);

  server_hello_length = build_server_hello(
      server_hello, sizeof(server_hello), peer->handshake_params,
      0, 1, 1);
  CHECK(server_hello_length > 0);
  CHECK(check_server_hello(&ctx, peer, server_hello,
                           server_hello_length) == 0);
  CHECK(!peer->handshake_params->session_resumed);
  CHECK(!ctx.session_ticket.valid);
  CHECK(peer->security_params[1] == NULL);
  CHECK(peer->handshake_params->session_ticket_expected);
  CHECK(peer->handshake_params->cid_negotiated);

  dtls_stop_retransmission(&ctx, peer);
  dtls_free_peer(peer);
  return 1;
}

static int
test_abbreviated_handshake_renews_ticket_and_cid(void)
{
  dtls_context_t ctx;
  dtls_handler_t handler;
  dtls_peer_t *peer;
  uint8_t message[160];
  uint8_t ccs = 1;
  size_t message_length;

  setup_context(&ctx, &handler);
  peer = new_client_peer();
  CHECK(peer != NULL);
  set_ticket(&ctx.session_ticket, "old-ticket", 3600);
  reset_capture();
  CHECK(dtls_send_client_hello(&ctx, peer, NULL, 0) > 0);

  message_length = build_server_hello(
      message, sizeof(message), peer->handshake_params, 1, 1, 1);
  CHECK(message_length > 0);
  CHECK(check_server_hello(&ctx, peer, message, message_length) == 0);
  CHECK(peer->handshake_params->session_resumed);
  CHECK(peer->security_params[1] != NULL);
  CHECK(peer->security_params[1]->write_cid_length == 4);
  CHECK(memcmp(peer->security_params[1]->write_cid, "cid2", 4) == 0);
  peer->state = DTLS_STATE_WAIT_CHANGECIPHERSPEC;
  dtls_stop_retransmission(&ctx, peer);

  message_length = build_new_session_ticket(
      message, sizeof(message), 7200, "renewed-ticket");
  CHECK(message_length > 0);
  CHECK(check_new_session_ticket(peer, message, message_length) == 0);
  CHECK(peer->handshake_params->session_ticket_received);
  CHECK(handle_ccs(&ctx, peer, NULL, &ccs, sizeof(ccs)) == 0);
  CHECK(peer->state == DTLS_STATE_WAIT_FINISHED);

  message_length = build_server_finished(
      peer, message, sizeof(message));
  CHECK(message_length > 0);
  reset_capture();
  CHECK(handle_handshake_msg(&ctx, peer, message, message_length) >= 0);
  CHECK(peer->state == DTLS_STATE_CONNECTED);
  CHECK(peer->handshake_params == NULL);
  CHECK(peer->resumed_client_flight_pending);
  CHECK(ctx.session_ticket.valid);
  CHECK(ctx.session_ticket.ticket_length == strlen("renewed-ticket"));
  CHECK(memcmp(ctx.session_ticket.ticket, "renewed-ticket",
               ctx.session_ticket.ticket_length) == 0);
  CHECK(ctx.session_ticket.lifetime_hint == 7200);
  CHECK(capture.count == 2);
  CHECK(capture.data[0][0] == DTLS_CT_CHANGE_CIPHER_SPEC);
  CHECK(capture.data[1][0] == DTLS_CT_TLS12_CID);
  CHECK(queue_size(ctx.sendqueue) == 2);
  {
    netq_t *queued;
    clock_time_t next_retransmission;

    for (queued = ctx.sendqueue; queued; queued = queued->next)
      queued->t = fake_millis - 1;
    reset_capture();
    dtls_check_retransmit(&ctx, &next_retransmission);
    CHECK(capture.count == 2);
    CHECK(queue_size(ctx.sendqueue) == 2);
    CHECK(peer->security_params[1] != NULL);
    CHECK(peer->security_params[1]->epoch == 0);
    CHECK(next_retransmission != 0);
  }

  dtls_stop_retransmission(&ctx, peer);
  dtls_free_peer(peer);
  return 1;
}

static int
test_cid_aad_matches_rfc9146(void)
{
  dtls_peer_t peer;
  dtls_security_parameters_t security;
  uint8_t payload[] = { 'a', 'b' };
  uint8_t *parts[] = { payload };
  size_t lengths[] = { sizeof(payload) };
  uint8_t record[128];
  uint8_t aad[26];
  uint8_t nonce[DTLS_CCM_BLOCKSIZE];
  uint8_t *p = aad;
  size_t record_length = sizeof(record);
  uint16_t encrypted_length;
  int cleartext_length;
  const dtls_ccm_params_t params = { nonce, 8, 3 };

  memset(&peer, 0, sizeof(peer));
  memset(&security, 0, sizeof(security));
  memset(security.key_block, 0x2a, sizeof(security.key_block));
  peer.role = DTLS_CLIENT;
  security.epoch = 1;
  security.cipher_index = get_cipher_index(
      default_user_parameters.cipher_suites,
      TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8);
  security.write_cid_length = 3;
  memcpy(security.write_cid, "cid", 3);

  CHECK(dtls_prepare_record(&peer, &security, DTLS_CT_APPLICATION_DATA,
                            parts, lengths, 1,
                            record, &record_length) == 0);
  CHECK(record[0] == DTLS_CT_TLS12_CID);
  CHECK(memcmp(record + 11, "cid", 3) == 0);
  encrypted_length = dtls_uint16_to_int(record + 14);
  CHECK(record_length == 16 + encrypted_length);
  CHECK(encrypted_length == 8 + sizeof(payload) + 1 + 8);

  memset(p, 0xff, 8);
  p += 8;
  *p++ = DTLS_CT_TLS12_CID;
  *p++ = 3;
  *p++ = DTLS_CT_TLS12_CID;
  memcpy(p, record + 1, 2);
  p += 2;
  memcpy(p, record + 3, 8);
  p += 8;
  memcpy(p, record + 11, 3);
  p += 3;
  dtls_int_to_uint16(p, sizeof(payload) + 1);
  p += 2;
  CHECK((size_t)(p - aad) == sizeof(aad));

  memset(nonce, 0, sizeof(nonce));
  memcpy(nonce, dtls_kb_local_iv(&security, peer.role),
         dtls_kb_iv_size(&security, peer.role));
  memcpy(nonce + dtls_kb_iv_size(&security, peer.role),
         record + 16, 8);
  cleartext_length = dtls_decrypt_params(
      &params, record + 24, encrypted_length - 8, record + 24,
      dtls_kb_local_write_key(&security, peer.role),
      dtls_kb_key_size(&security, peer.role),
      aad, sizeof(aad));
  CHECK(cleartext_length == (int)sizeof(payload) + 1);
  CHECK(memcmp(record + 24, payload, sizeof(payload)) == 0);
  CHECK(record[24 + sizeof(payload)] == DTLS_CT_APPLICATION_DATA);
  return 1;
}

int
main(void)
{
  struct {
    const char *name;
    int (*run)(void);
  } tests[] = {
    { "ticket offer and HelloVerifyRequest retry",
      test_ticket_offer_and_hello_verify_retry },
    { "expired ticket is not offered",
      test_expired_ticket_is_not_offered },
    { "ticket rejection falls back to full handshake",
      test_ticket_rejection_falls_back_to_full_handshake },
    { "abbreviated handshake renews ticket and CID",
      test_abbreviated_handshake_renews_ticket_and_cid },
    { "CID AAD matches RFC 9146",
      test_cid_aad_matches_rfc9146 }
  };
  size_t i;

  fake_millis = 1000;
  TinyDtls_set_rand(test_rand);
  TinyDtls_set_get_millis(test_get_millis);
  dtls_init();

  for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    if (!tests[i].run())
      return EXIT_FAILURE;
    printf("PASS: %s\n", tests[i].name);
  }
  return EXIT_SUCCESS;
}
