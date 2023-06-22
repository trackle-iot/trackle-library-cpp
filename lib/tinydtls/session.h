/*******************************************************************************
 *
 * Copyright (c) 2011-2022 Olaf Bergmann (TZI) and others.
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

#ifndef _DTLS_SESSION_H_
#define _DTLS_SESSION_H_

#include <string.h>

#include "tinydtls.h"
#include "global.h"

typedef struct
{
  unsigned char size;     /**< size of session_t::addr */
  unsigned char *addr;    /**< session IP address */
  unsigned short port;    /**< transport layer port */
} session_t;

/**
 * Compares the given session objects. This function returns @c 0
 * when @p a and @p b differ, @c 1 otherwise.
 */
int dtls_session_equals(const session_t *a, const session_t *b);

#endif /* _DTLS_SESSION_H_ */
