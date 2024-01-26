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
 *
 *******************************************************************************/

/**
 * @file dtls_time.h
 * @brief Clock Handling
 */

#ifndef _DTLS_DTLS_TIME_H_
#define _DTLS_DTLS_TIME_H_

#include <stdint.h>

#include "tinydtls.h"

/**
 * @defgroup clock Clock Handling
 * Default implementation of internal clock. You should redefine this if
 * you do not have time() and gettimeofday().
 * @{
 */

#ifndef CLOCK_SECOND
#define CLOCK_SECOND 1000
#endif

typedef uint32_t clock_time_t;
typedef clock_time_t dtls_tick_t;

#ifndef DTLS_TICKS_PER_SECOND
#define DTLS_TICKS_PER_SECOND CLOCK_SECOND
#endif /* DTLS_TICKS_PER_SECOND */

void dtls_ticks(dtls_tick_t *t);

/* see https://godbolt.org/z/YchexKaeT */
#define DTLS_OFFSET_TIME (((clock_time_t)~0) >> 1)
/** Checks if A is before (or equal) B. Considers 32 bit time overflow */
#define DTLS_IS_BEFORE_TIME(A, B) ((clock_time_t)(DTLS_OFFSET_TIME + (B) - (A)) >= DTLS_OFFSET_TIME)

/** @} */

#endif /* _DTLS_DTLS_TIME_H_ */
