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
 * @file dtls_time.c
 * @brief Clock Handling
 */

#include <stdlib.h>

#include "tinydtls.h"
#include "dtls_time.h"

// Dummy callback that causes firmware to abort when called.
// Better than setting dtls_ticks pointer to NULL because,
// in this way, we can know from the dump why the firmware crashes.
static void dtls_ticks_aborting(uint32_t *ticks)
{
  // NOTE FOR DEBUGGERS:
  // if you are reading this because your firmware crashes and a dump trace brought you here,
  // beware that the cause of your firmware's crashes is that no timing function was set
  // by calling TinyDtls_set_get_millis from outside the library.
  abort();
}

static void (*dtls_ticks_callback)(dtls_tick_t *) = dtls_ticks_aborting;

void dtls_ticks(dtls_tick_t *t)
{
  dtls_ticks_callback(t);
}

void TinyDtls_set_get_millis(void (*newGetMillis)(uint32_t *))
{
  dtls_ticks_callback = newGetMillis;
}
