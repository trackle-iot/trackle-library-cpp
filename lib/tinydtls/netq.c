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

#include "dtls_debug.h"
#include "netq.h"
#include "utlist.h"

#include <assert.h>

#include <stdlib.h>

static inline netq_t *
netq_malloc_node(size_t size)
{
  return (netq_t *)malloc(sizeof(netq_t) + size);
}

static inline void
netq_free_node(netq_t *node)
{
  free(node);
}

int netq_insert_node(netq_t **queue, netq_t *node)
{
  netq_t *p;

  assert(queue);
  assert(node);

  p = *queue;
  /* comparison considering 32bit overflow */
  while (p && DTLS_IS_BEFORE_TIME(p->t, node->t))
  {
    assert(p != node);
    if (p == node)
      return 0;
    p = p->next;
  }

  if (p)
    LL_PREPEND_ELEM(*queue, p, node);
  else
    LL_APPEND(*queue, node);

  return 1;
}

netq_t *
netq_head(netq_t **queue)
{
  return queue ? *queue : NULL;
}

netq_t *
netq_next(netq_t *p)
{
  if (!p)
    return NULL;

  return p->next;
}

void netq_remove(netq_t **queue, netq_t *p)
{
  if (!queue || !p)
    return;

  LL_DELETE(*queue, p);
}

netq_t *netq_pop_first(netq_t **queue)
{
  netq_t *p = netq_head(queue);

  if (p)
    LL_DELETE(*queue, p);

  return p;
}

netq_t *
netq_node_new(size_t size)
{
  netq_t *node;
  node = netq_malloc_node(size);

  if (node)
  {
    memset(node, 0, sizeof(netq_t));
  }
  else
  {
    dtls_warn("netq_node_new: malloc\n");
  }

  return node;
}

void netq_node_free(netq_t *node)
{
  if (node)
    netq_free_node(node);
}

void netq_delete_all(netq_t **queue)
{
  netq_t *p, *tmp;
  if (queue)
  {
    LL_FOREACH_SAFE(*queue, p, tmp)
    {
      netq_free_node(p);
    }

    *queue = NULL;
  }
}
