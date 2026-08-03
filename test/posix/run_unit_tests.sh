#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD_DIR="${TMPDIR:-/tmp}/trackle-library-tests"
CXX=${CXX:-c++}

mkdir -p "$BUILD_DIR"

case "$(uname -s)" in
  Darwin)
    DEAD_CODE_FLAGS="-Wl,-dead_strip"
    ;;
  *)
    DEAD_CODE_FLAGS="-Wl,--gc-sections"
    ;;
esac

"$CXX" \
  -std=gnu++20 -Wall -Wextra \
  -Wno-unused-parameter \
  -Wno-deprecated-declarations \
  -Wno-deprecated-enum-enum-conversion \
  -Wno-deprecated-this-capture \
  -ffunction-sections -fdata-sections \
  -fsanitize=address,undefined \
  -I"$ROOT/include" \
  -I"$ROOT/lib/tinydtls" \
  -I"$ROOT/lib/micro-ecc" \
  "$ROOT/test/posix/test_dtls_message_channel.cpp" \
  $DEAD_CODE_FLAGS \
  -o "$BUILD_DIR/test_dtls_message_channel"

"$BUILD_DIR/test_dtls_message_channel"
