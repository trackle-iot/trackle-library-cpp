/*
 * FILE:	sha2.h
 * AUTHOR:	Aaron D. Gifford - http://www.aarongifford.com/
 * 
 * Copyright (c) 2000-2001, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: sha2.h,v 1.1 2001/11/08 00:02:01 adg Exp adg $
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Import u_intXX_t size_t type definitions from system headers.  You
 * may need to change this, or define these things yourself in this
 * file.
 */
#include <sys/types.h>
#include "tinydtls.h"
#include <inttypes.h>


/*** SHA-256/384/512 Various Length Definitions ***********************/
#define DTLS_SHA256_BLOCK_LENGTH		64
#define DTLS_SHA256_DIGEST_LENGTH		32
#define DTLS_SHA256_DIGEST_STRING_LENGTH	(DTLS_SHA256_DIGEST_LENGTH * 2 + 1)
#define DTLS_SHA384_BLOCK_LENGTH		128
#define DTLS_SHA384_DIGEST_LENGTH		48
#define DTLS_SHA384_DIGEST_STRING_LENGTH	(DTLS_SHA384_DIGEST_LENGTH * 2 + 1)
#define DTLS_SHA512_BLOCK_LENGTH		128
#define DTLS_SHA512_DIGEST_LENGTH		64
#define DTLS_SHA512_DIGEST_STRING_LENGTH	(DTLS_SHA512_DIGEST_LENGTH * 2 + 1)

typedef struct _dtls_sha256_ctx {
	uint32_t	state[8];
	uint64_t	bitcount;
	uint8_t	buffer[DTLS_SHA256_BLOCK_LENGTH];
} dtls_sha256_ctx;
typedef struct _dtls_sha512_ctx {
	uint64_t	state[8];
	uint64_t	bitcount[2];
	uint8_t	buffer[DTLS_SHA512_BLOCK_LENGTH];
} dtls_sha512_ctx;

typedef dtls_sha512_ctx dtls_sha384_ctx;


/*** SHA-256/384/512 Function Prototypes ******************************/
#ifndef NOPROTO

#ifdef WITH_SHA256
void dtls_sha256_init(dtls_sha256_ctx *);
void dtls_sha256_update(dtls_sha256_ctx*, const uint8_t*, size_t);
void dtls_sha256_final(uint8_t[DTLS_SHA256_DIGEST_LENGTH], dtls_sha256_ctx*);
char* dtls_sha256_end(dtls_sha256_ctx*, char[DTLS_SHA256_DIGEST_STRING_LENGTH]);
char* dtls_sha256_data(const uint8_t*, size_t, char[DTLS_SHA256_DIGEST_STRING_LENGTH]);
#endif

#ifdef WITH_SHA384
void dtls_sha384_init(dtls_sha384_ctx*);
void dtls_sha384_update(dtls_sha384_ctx*, const uint8_t*, size_t);
void dtls_sha384_final(uint8_t[DTLS_SHA384_DIGEST_LENGTH], dtls_sha384_ctx*);
char* dtls_sha384_end(dtls_sha384_ctx*, char[DTLS_SHA384_DIGEST_STRING_LENGTH]);
char* dtls_sha384_data(const uint8_t*, size_t, char[DTLS_SHA384_DIGEST_STRING_LENGTH]);
#endif

#ifdef WITH_SHA512
void dtls_sha512_init(dtls_sha512_ctx*);
void dtls_sha512_update(dtls_sha512_ctx*, const uint8_t*, size_t);
void dtls_sha512_final(uint8_t[DTLS_SHA512_DIGEST_LENGTH], dtls_sha512_ctx*);
char* dtls_sha512_end(dtls_sha512_ctx*, char[DTLS_SHA512_DIGEST_STRING_LENGTH]);
char* dtls_sha512_data(const uint8_t*, size_t, char[DTLS_SHA512_DIGEST_STRING_LENGTH]);
#endif

#else /* NOPROTO */

#ifdef WITH_SHA256
void dtls_sha256_init();
void dtls_sha256_update();
void dtls_sha256_final();
char* dtls_sha256_end();
char* dtls_sha256_data();
#endif

#ifdef WITH_SHA384
void dtls_sha384_init();
void dtls_sha384_update();
void dtls_sha384_final();
char* dtls_sha384_end();
char* dtls_sha384_data();
#endif

#ifdef WITH_SHA512
void dtls_sha512_init();
void dtls_sha512_update();
void dtls_sha512_final();
char* dtls_sha512_end();
char* dtls_sha512_data();
#endif

#endif /* NOPROTO */

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* __SHA2_H__ */

