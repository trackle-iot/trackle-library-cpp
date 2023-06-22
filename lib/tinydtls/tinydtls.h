/* tinydtls.h.  Generated from tinydtls.h.in by configure.  */
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

/**
 * @file tinydtls.h
 * @brief public tinydtls API
 */

#ifndef _DTLS_TINYDTLS_H_
#define _DTLS_TINYDTLS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Add log function that accepts log level as first parameter.
 *
 * @param logCallback Log function to use.
 */
void TinyDtls_set_log_callback(void (*logCallback)(unsigned int, const char *, ...));

/**
 * @brief Add log level for TinyDTLS.
 * 
 * @param level Level to be set for TinyDTLS
 */
void TinyDtls_set_log_level(int level);

#ifdef __cplusplus
};
#endif


#define WITH_SHA256 1

#include "dtls_config.h"

#ifndef DTLS_ECC
#ifndef DTLS_PSK
#error "TinyDTLS requires at least one Cipher suite!"
#endif /* DTLS_PSK */
#endif /* DTLS_ECC */

#endif /* _DTLS_TINYDTLS_H_ */
