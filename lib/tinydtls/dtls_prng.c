/*******************************************************************************
 *
 * Copyright (c) 2011-2019 Olaf Bergmann (TZI) and others.
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
 *    Jon Shallow    - platform dependent prng support
 *
 *******************************************************************************/

#include <stdlib.h>
#include <inttypes.h>

#include "tinydtls.h"

static uint32_t defaultRand()
{
    return (uint32_t)rand();
}

static uint32_t (*customRand)() = defaultRand;

void TinyDtls_set_rand(uint32_t (*newCustomRand)())
{
    customRand = newCustomRand;
}

int dtls_prng(unsigned char *buf, size_t len)
{
    size_t klen = len;
    while (len--)
        *buf++ = customRand() & 0xFF;
    return klen;
}
