#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="${TMPDIR:-/tmp}/tinydtls-tests"
CC=${CC:-cc}

mkdir -p "$BUILD_DIR"

"$CC" \
  -std=c99 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -Wno-unused-parameter \
  -fsanitize=address,undefined \
  -DDTLS_ECC=1 -DDTLSv12 -DWITH_SHA256 -DSHA2_USE_INTTYPES_H \
  -I"$ROOT" -I"$ROOT/../micro-ecc" \
  "$ROOT/tests/test_protocol_features.c" \
  "$ROOT/crypto.c" \
  "$ROOT/ccm.c" \
  "$ROOT/hmac.c" \
  "$ROOT/dtls_time.c" \
  "$ROOT/dtls_debug.c" \
  "$ROOT/dtls_prng.c" \
  "$ROOT/netq.c" \
  "$ROOT/peer.c" \
  "$ROOT/session.c" \
  "$ROOT/aes/rijndael.c" \
  "$ROOT/aes/rijndael_wrap.c" \
  "$ROOT/sha2/sha2.c" \
  "$ROOT/../micro-ecc/uECC.c" \
  -o "$BUILD_DIR/test_protocol_features"

"$BUILD_DIR/test_protocol_features"
