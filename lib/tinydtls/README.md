# About tinydtls

tinydtls is a library for Datagram Transport Layer Security (DTLS)
covering both the client and the server state machine. It is
implemented in C and provides support for a minimal set of cipher
suites suitable for the Internet of Things.

This library contains functions and structures that can help
constructing a single-threaded UDP server with DTLS support in
C99. The following components are available:

* dtls
  Basic support for DTLS with pre-shared key mode and RPK mode with ECC.

# Notes on IoTReady's customized version of TinyDTLS

## Introduction

This version was customized in order to be used by Trackle Library for establishing a DTLS session towards the Trackle Cloud.

The main reason for doing so was that Trackle Library was written for being platform agnostic, while TinyDTLS supports a limited range of platforms.
By using TinyDTLS, this would have made the whole Trackle Library bound to being used by a limited set of platforms too.

By throwing away as much of the platform-dependent code as possible, and replacing it with functions pointers to be provided by the calling code, this version of TinyDTLS was made as platform agnostic as possible.

## Callbacks

The following functions must be used in order to provide the necessary callback functions:
* `void TinyDtls_set_rand(uint32_t (*newCustomRand)())` (from `tinydtls_set_rand.h`):
  * Provides a function that returns a pseudo-random generated number (initialization must be done by calling code).
* `void TinyDtls_set_get_millis(void (*newGetMillis)(uint32_t *))` (from `tinydtls_set_get_millis.h`):
  * Provides a function that returns the number of milliseconds elapsed since boot.
* `void TinyDtls_set_log_callback(void (*logCallback)(unsigned int, const char *, ...))` (from `tinydtls_set_get_rand.h`):
  * Provides a logging function that prints a log message (first argument is a log-level, the others are the same that you would give to printf).

If you are considering writing a wrapper for a new platform, please note that this functions are currently being called from inside of Trackle Library to set the same callbacks that are being provided to the library itself. So, there should be no reason to call them again.
