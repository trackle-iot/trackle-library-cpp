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

#include "tinydtls.h"

#include <stdarg.h>
#include <stdio.h>

#include "global.h"
#include "dtls_debug.h"

static int current_log_level = DTLS_LOG_EMERG;

void TinyDtls_set_log_level(int level)
{
  current_log_level = level;
}

int TinyDtls_get_log_level()
{
  return current_log_level;
}

/**
 * Discard any log printed with this function.
 * This is meant to be used as default function for \ref TinyDtls_logCallback.
 * This is done so that \ref TinyDtls_logCallback is never invalid, and calling it before set would simply discard the message.
 */
static void discard_log_callback(unsigned int level, const char *format, ...)
{
  // Discards log message.
}

// Pointer to function to be used for logging
void (*TinyDtls_logCallback)(unsigned int, const char *, ...) = discard_log_callback;

void TinyDtls_set_log_callback(void (*logCallback)(unsigned int, const char *, ...))
{
  TinyDtls_logCallback = logCallback;
}

void TinyDtls_logBuffer(unsigned int level, const char *name, const unsigned char *buff, int len)
{
  char string[32] = {0};
  char *p = string;
  TinyDtls_logCallback(level, "------ HEX DUMP -- START ------");
  TinyDtls_logCallback(level, " NAME: \"%s\"", name);
  TinyDtls_logCallback(level, " LENGTH: %d bytes", len);
  TinyDtls_logCallback(level, "");
  for (int i = 0; i < len; i++)
  {
    p += sprintf(p, "%02X ", buff[i]);
    if ((i + 1) % 10 == 0 || i == len - 1)
    {
      TinyDtls_logCallback(level, " %s", string);
      string[0] = '\0';
      p = string;
    }
  }
  TinyDtls_logCallback(level, "");
  TinyDtls_logCallback(level, "------- HEX DUMP -- END -------");
}
