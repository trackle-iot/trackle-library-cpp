/*******************************************************************************
 *
 * Copyright (c) 2011, 2012, 2013, 2014, 2015 Olaf Bergmann (TZI) and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v. 1.0 which accompanies this distribution.
 *
 * The Eclipse Public License is available at http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Olaf Bergmann  - initial API and implementation
 *    Hauke Mehrtens - memory optimization, ECC integration
 *
 *******************************************************************************/

#include <stdio.h>

#include "tinydtls.h"

#include <assert.h>

#include "global.h"
#include "dtls_debug.h"
#include "numeric.h"
#include "dtls.h"
#include "crypto.h"
#include "ccm.h"
#include "uECC.h"
#include "dtls_prng.h"
#include "netq.h"

#ifndef uECC_CURVE
#define ECC_CURVE uECC_secp256r1()
#endif
#define SIGN_HASH_SIZE 32
#define ECDSA_SIGN_SIZE 64
#define ECDH_PUB_KEY_SIZE 64
#define ECDH_PUB_KEY_X_SIZE 32
#define ECDH_PUB_KEY_Y_SIZE 32
#define ECDH_PRIV_KEY_SIZE 32

#define HMAC_UPDATE_SEED(Context, Seed, Length) \
  if (Seed)                                     \
  dtls_hmac_update(Context, (Seed), (Length))

static struct dtls_cipher_context_t cipher_context;

void crypto_init(void)
{
  uECC_set_rng(dtls_prng);
}

static dtls_handshake_parameters_t *dtls_handshake_malloc(void)
{
  return malloc(sizeof(dtls_handshake_parameters_t));
}

static void dtls_handshake_dealloc(dtls_handshake_parameters_t *handshake)
{
  free(handshake);
}

static dtls_security_parameters_t *dtls_security_malloc(void)
{
  return malloc(sizeof(dtls_security_parameters_t));
}

static void dtls_security_dealloc(dtls_security_parameters_t *security)
{
  free(security);
}

dtls_handshake_parameters_t *dtls_handshake_new(void)
{
  dtls_handshake_parameters_t *handshake;

  handshake = dtls_handshake_malloc();
  if (!handshake)
  {
    dtls_crit("can not allocate a handshake struct\n");
    return NULL;
  }

  memset(handshake, 0, sizeof(*handshake));

  /* initialize the handshake hash wrt. the hard-coded DTLS version */
  dtls_debug("DTLSv12: initialize HASH_SHA256\n");
  /* TLS 1.2:  PRF(secret, label, seed) = P_<hash>(secret, label + seed) */
  /* FIXME: we use the default SHA256 here, might need to support other
            hash functions as well */
  dtls_hash_init(&handshake->hs_state.hs_hash);
  return handshake;
}

void dtls_handshake_free(dtls_handshake_parameters_t *handshake)
{
  if (!handshake)
    return;

  netq_delete_all(&handshake->reorder_queue);
  dtls_handshake_dealloc(handshake);
}

dtls_security_parameters_t *dtls_security_new(void)
{
  dtls_security_parameters_t *security;

  security = dtls_security_malloc();
  if (!security)
  {
    dtls_crit("can not allocate a security struct\n");
    return NULL;
  }

  memset(security, 0, sizeof(*security));

  security->cipher = TLS_NULL_WITH_NULL_NULL;
  security->compression = TLS_COMPRESSION_NULL;

  return security;
}

void dtls_security_free(dtls_security_parameters_t *security)
{
  if (!security)
    return;

  dtls_security_dealloc(security);
}

size_t
dtls_p_hash(dtls_hashfunc_t h,
            const unsigned char *key, size_t keylen,
            const unsigned char *label, size_t labellen,
            const unsigned char *random1, size_t random1len,
            const unsigned char *random2, size_t random2len,
            unsigned char *buf, size_t buflen)
{
  dtls_hmac_context_t hmac;

  unsigned char A[DTLS_HMAC_DIGEST_SIZE];
  unsigned char tmp[DTLS_HMAC_DIGEST_SIZE];
  size_t dlen;    /* digest length */
  size_t len = 0; /* result length */
  (void)h;

  dtls_hmac_init(&hmac, key, keylen);

  /* calculate A(1) from A(0) == seed */
  HMAC_UPDATE_SEED(&hmac, label, labellen);
  HMAC_UPDATE_SEED(&hmac, random1, random1len);
  HMAC_UPDATE_SEED(&hmac, random2, random2len);

  dlen = dtls_hmac_finalize(&hmac, A);

  while (len < buflen)
  {
    dtls_hmac_init(&hmac, key, keylen);
    dtls_hmac_update(&hmac, A, dlen);

    HMAC_UPDATE_SEED(&hmac, label, labellen);
    HMAC_UPDATE_SEED(&hmac, random1, random1len);
    HMAC_UPDATE_SEED(&hmac, random2, random2len);

    dlen = dtls_hmac_finalize(&hmac, tmp);

    if ((len + dlen) < buflen)
    {
      memcpy(&buf[len], tmp, dlen);
      len += dlen;
    }
    else
    {
      memcpy(&buf[len], tmp, buflen - len);
      break;
    }

    /* calculate A(i+1) */
    dtls_hmac_init(&hmac, key, keylen);
    dtls_hmac_update(&hmac, A, dlen);
    dtls_hmac_finalize(&hmac, A);
  }

  /* prevent exposure of sensible data */
  memset(&hmac, 0, sizeof(hmac));
  memset(tmp, 0, sizeof(tmp));
  memset(A, 0, sizeof(A));

  return buflen;
}

size_t
dtls_prf(const unsigned char *key, size_t keylen,
         const unsigned char *label, size_t labellen,
         const unsigned char *random1, size_t random1len,
         const unsigned char *random2, size_t random2len,
         unsigned char *buf, size_t buflen)
{

  /* Clear the result buffer */
  memset(buf, 0, buflen);
  return dtls_p_hash(HASH_SHA256,
                     key, keylen,
                     label, labellen,
                     random1, random1len,
                     random2, random2len,
                     buf, buflen);
}

void dtls_mac(dtls_hmac_context_t *hmac_ctx,
              const unsigned char *record,
              const unsigned char *packet, size_t length,
              unsigned char *buf)
{
  uint16 L;
  dtls_int_to_uint16(L, length);

  assert(hmac_ctx);
  dtls_hmac_update(hmac_ctx, record + 3, sizeof(uint16) + sizeof(uint48));
  dtls_hmac_update(hmac_ctx, record, sizeof(uint8) + sizeof(uint16));
  dtls_hmac_update(hmac_ctx, L, sizeof(uint16));
  dtls_hmac_update(hmac_ctx, packet, length);

  dtls_hmac_finalize(hmac_ctx, buf);
}

static size_t
dtls_ccm_encrypt(aes128_ccm_t *ccm_ctx, const unsigned char *src, size_t srclen,
                 unsigned char *buf,
                 const unsigned char *nonce,
                 const unsigned char *aad, size_t la)
{
  long int len;
  (void)src;

  assert(ccm_ctx);

  len = dtls_ccm_encrypt_message(&ccm_ctx->ctx,
                                 ccm_ctx->tag_length /* M */,
                                 ccm_ctx->l /* L */,
                                 nonce,
                                 buf, srclen,
                                 aad, la);
  return len;
}

static size_t
dtls_ccm_decrypt(aes128_ccm_t *ccm_ctx, const unsigned char *src,
                 size_t srclen, unsigned char *buf,
                 const unsigned char *nonce,
                 const unsigned char *aad, size_t la)
{
  long int len;
  (void)src;

  assert(ccm_ctx);

  len = dtls_ccm_decrypt_message(&ccm_ctx->ctx,
                                 ccm_ctx->tag_length /* M */,
                                 ccm_ctx->l /* L */,
                                 nonce,
                                 buf, srclen,
                                 aad, la);
  return len;
}

#ifdef DTLS_PSK
int dtls_psk_pre_master_secret(unsigned char *key, size_t keylen,
                               unsigned char *result, size_t result_len)
{
  unsigned char *p = result;

  if (result_len < (2 * (sizeof(uint16) + keylen)))
  {
    return -1;
  }

  dtls_int_to_uint16(p, keylen);
  p += sizeof(uint16);

  memset(p, 0, keylen);
  p += keylen;

  memcpy(p, result, sizeof(uint16));
  p += sizeof(uint16);

  memcpy(p, key, keylen);

  return 2 * (sizeof(uint16) + keylen);
}
#endif /* DTLS_PSK */

#ifdef DTLS_ECC
static void dtls_ec_key_to_uint32(const unsigned char *key, size_t key_size,
                                  uint32_t *result)
{
  int i;

  for (i = (key_size / sizeof(uint32_t)) - 1; i >= 0; i--)
  {
    *result = dtls_uint32_to_int(&key[i * sizeof(uint32_t)]);
    result++;
  }
}

static void dtls_ec_key_from_uint32(const uint32_t *key, size_t key_size,
                                    unsigned char *result)
{
  int i;

  for (i = (key_size / sizeof(uint32_t)) - 1; i >= 0; i--)
  {
    dtls_int_to_uint32(result, key[i]);
    result += 4;
  }
}

/* Build the EC KEY as a ASN.1 positive integer */
/*
 * The public EC key consists of two positive numbers. Converting them into
 * ASN.1 INTEGER requires removing leading zeros, but special care must be
 * taken of the resulting sign. If the first non-zero byte of the 32 byte
 * ec-key has bit 7 set (highest bit), the resultant ASN.1 INTEGER would be
 * interpreted as a negative number. In order to prevent this, a zero in the
 * ASN.1 presentation is prepended if that bit 7 is set.
 */
int dtls_ec_key_asn1_from_uint32(const uint32_t *key, size_t key_size,
                                 uint8_t *buf)
{
  int i = 0;
  uint8_t *lptr;

  /* ASN.1 Integer r */
  dtls_int_to_uint8(buf, 0x02);
  buf += sizeof(uint8);

  lptr = buf;
  /* Length will be filled in later */
  buf += sizeof(uint8);

  dtls_ec_key_from_uint32(key, key_size, buf);

  /* skip leading 0's */
  while (i < (int)key_size && buf[i] == 0)
  {
    ++i;
  }
  assert(i != (int)key_size);
  if (i == (int)key_size)
  {
    dtls_alert("ec key is all zero\n");
    return 0;
  }
  if (buf[i] >= 0x80)
  {
    /*
     * Preserve unsigned by adding leading 0 (i may go negative which is
     * explicitely handled below with the assumption that buf is at least 33
     * bytes in size).
     */
    --i;
  }
  if (i > 0)
  {
    /* remove leading 0's */
    key_size -= i;
    memmove(buf, buf + i, key_size);
  }
  else if (i == -1)
  {
    /* add leading 0 */
    memmove(buf + 1, buf, key_size);
    buf[0] = 0;
    key_size++;
  }
  /* Update the length of positive ASN.1 integer */
  dtls_int_to_uint8(lptr, key_size);
  return key_size + 2;
}

int dtls_ecdh_pre_master_secret(unsigned char *priv_key,
                                unsigned char *pub_key_x,
                                unsigned char *pub_key_y,
                                size_t key_size,
                                unsigned char *result,
                                size_t result_len)
{

  uint8_t pub_key_copy[ECDH_PUB_KEY_SIZE];
  uint8_t priv_key_copy[ECDH_PRIV_KEY_SIZE];
  if (result_len < key_size)
  {
    return -1;
  }
  memcpy(pub_key_copy, pub_key_x, ECDH_PUB_KEY_X_SIZE);
  memcpy(pub_key_copy + ECDH_PUB_KEY_X_SIZE, pub_key_y, ECDH_PUB_KEY_Y_SIZE);
  memcpy(priv_key_copy, priv_key, ECDH_PRIV_KEY_SIZE);
  uECC_shared_secret(pub_key_copy, priv_key_copy, result, ECC_CURVE);

  return key_size;
}

void dtls_ecdsa_generate_key(unsigned char *priv_key,
                             unsigned char *pub_key_x,
                             unsigned char *pub_key_y,
                             size_t key_size)
{

  uint8_t tmp_pub_key[ECDH_PUB_KEY_SIZE];
  uint8_t tmp_priv_key[ECDH_PRIV_KEY_SIZE];
  uECC_make_key(tmp_pub_key, tmp_priv_key, ECC_CURVE);

  memcpy(pub_key_x, tmp_pub_key, ECDH_PUB_KEY_X_SIZE);
  memcpy(pub_key_y, tmp_pub_key + ECDH_PUB_KEY_X_SIZE, ECDH_PUB_KEY_Y_SIZE);
  memcpy(priv_key, tmp_priv_key, ECDH_PRIV_KEY_SIZE);
}

/* rfc4492#section-5.4 */
void dtls_ecdsa_create_sig_hash(const unsigned char *priv_key, size_t key_size,
                                const unsigned char *sign_hash, size_t sign_hash_size,
                                uint32_t point_r[9], uint32_t point_s[9])
{

  uint8_t sign[ECDSA_SIGN_SIZE];

  // Check the buffers
  if (priv_key == NULL || key_size < 32)
    return;
  if (sign_hash == NULL || sign_hash_size < 32)
    return;
  uECC_sign(priv_key, sign_hash, sign_hash_size, sign, ECC_CURVE);
  int i;
  for (i = 0; i < 32; i++)
  {
    ((uint8_t *)point_r)[i] = sign[31 - i];
    ((uint8_t *)point_s)[i] = sign[63 - i];
  }
}

void dtls_ecdsa_create_sig(const unsigned char *priv_key, size_t key_size,
                           const unsigned char *client_random, size_t client_random_size,
                           const unsigned char *server_random, size_t server_random_size,
                           const unsigned char *keyx_params, size_t keyx_params_size,
                           uint32_t point_r[9], uint32_t point_s[9])
{
  dtls_hash_ctx data;
  unsigned char sha256hash[DTLS_HMAC_DIGEST_SIZE];

  dtls_hash_init(&data);
  dtls_hash_update(&data, client_random, client_random_size);
  dtls_hash_update(&data, server_random, server_random_size);
  dtls_hash_update(&data, keyx_params, keyx_params_size);
  dtls_hash_finalize(sha256hash, &data);

  dtls_ecdsa_create_sig_hash(priv_key, key_size, sha256hash,
                             sizeof(sha256hash), point_r, point_s);
}

/* rfc4492#section-5.4 */
int dtls_ecdsa_verify_sig_hash(const unsigned char *pub_key_x,
                               const unsigned char *pub_key_y, size_t key_size,
                               const unsigned char *sign_hash, size_t sign_hash_size,
                               unsigned char *result_r, unsigned char *result_s)
{

  uint8_t pub_key_copy[ECDH_PUB_KEY_SIZE];
  uint8_t hash_val[SIGN_HASH_SIZE];
  uint8_t sign[ECDSA_SIGN_SIZE];

  // Check the buffers
  if (pub_key_x == NULL || pub_key_y == NULL || key_size < 32)
    return 0;
  if (sign_hash == NULL || sign_hash_size < 32)
    return 0;
  if (result_r == NULL || result_s == NULL)
    return 0;

  // Copy the public key into a single buffer
  memcpy(pub_key_copy, pub_key_x, ECDH_PUB_KEY_X_SIZE);
  memcpy(pub_key_copy + ECDH_PUB_KEY_X_SIZE, pub_key_y, ECDH_PUB_KEY_Y_SIZE);

  // Copy the signature into a single buffer
  memcpy(sign, result_r, 32);
  memcpy(sign + 32, result_s, 32);

  return uECC_verify(pub_key_copy, hash_val, SIGN_HASH_SIZE, sign, ECC_CURVE);
}

int dtls_ecdsa_verify_sig(const unsigned char *pub_key_x,
                          const unsigned char *pub_key_y, size_t key_size,
                          const unsigned char *client_random, size_t client_random_size,
                          const unsigned char *server_random, size_t server_random_size,
                          const unsigned char *keyx_params, size_t keyx_params_size,
                          unsigned char *result_r, unsigned char *result_s)
{
  dtls_hash_ctx data;
  unsigned char sha256hash[DTLS_HMAC_DIGEST_SIZE];

  dtls_hash_init(&data);
  dtls_hash_update(&data, client_random, client_random_size);
  dtls_hash_update(&data, server_random, server_random_size);
  dtls_hash_update(&data, keyx_params, keyx_params_size);
  dtls_hash_finalize(sha256hash, &data);

  return dtls_ecdsa_verify_sig_hash(pub_key_x, pub_key_y, key_size, sha256hash,
                                    sizeof(sha256hash), result_r, result_s);
}
#endif /* DTLS_ECC */

int dtls_encrypt_params(const dtls_ccm_params_t *params,
                        const unsigned char *src, size_t length,
                        unsigned char *buf,
                        const unsigned char *key, size_t keylen,
                        const unsigned char *aad, size_t la)
{
  int ret;
  struct dtls_cipher_context_t *ctx = &cipher_context;
  ctx->data.tag_length = params->tag_length;
  ctx->data.l = params->l;

  ret = rijndael_set_key_enc_only(&ctx->data.ctx, key, 8 * keylen);
  if (ret < 0)
  {
    /* cleanup everything in case the key has the wrong size */
    dtls_warn("cannot set rijndael key\n");
    goto error;
  }

  if (src != buf)
    memmove(buf, src, length);
  ret = dtls_ccm_encrypt(&ctx->data, src, length, buf, params->nonce, aad, la);

error:
  return ret;
}

int dtls_encrypt(const unsigned char *src, size_t length,
                 unsigned char *buf,
                 const unsigned char *nonce,
                 const unsigned char *key, size_t keylen,
                 const unsigned char *aad, size_t la)
{
  /* For backwards-compatibility, dtls_encrypt_params is called with
   * M=8 and L=3. */
  const dtls_ccm_params_t params = {nonce, 8, 3};

  return dtls_encrypt_params(&params, src, length, buf, key, keylen, aad, la);
}

int dtls_decrypt_params(const dtls_ccm_params_t *params,
                        const unsigned char *src, size_t length,
                        unsigned char *buf,
                        const unsigned char *key, size_t keylen,
                        const unsigned char *aad, size_t la)
{
  int ret;
  struct dtls_cipher_context_t *ctx = &cipher_context;
  ctx->data.tag_length = params->tag_length;
  ctx->data.l = params->l;

  ret = rijndael_set_key_enc_only(&ctx->data.ctx, key, 8 * keylen);
  if (ret < 0)
  {
    /* cleanup everything in case the key has the wrong size */
    dtls_warn("cannot set rijndael key\n");
    goto error;
  }

  if (src != buf)
    memmove(buf, src, length);
  ret = dtls_ccm_decrypt(&ctx->data, src, length, buf, params->nonce, aad, la);

error:
  return ret;
}

int dtls_decrypt(const unsigned char *src, size_t length,
                 unsigned char *buf,
                 const unsigned char *nonce,
                 const unsigned char *key, size_t keylen,
                 const unsigned char *aad, size_t la)
{
  /* For backwards-compatibility, dtls_encrypt_params is called with
   * M=8 and L=3. */
  const dtls_ccm_params_t params = {nonce, 8, 3};

  return dtls_decrypt_params(&params, src, length, buf, key, keylen, aad, la);
}
