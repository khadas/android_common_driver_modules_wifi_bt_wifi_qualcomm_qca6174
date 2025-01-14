/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*Gerbera - WiFi*/

/*===========================================================================
  @file vos_memory.c

  @brief Virtual Operating System Services Memory API
===========================================================================*/

/*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------

===========================================================================*/

/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stacktrace.h>
#include <linux/skbuff.h>

#include "vos_memory.h"
#include "vos_trace.h"
#include "vos_api.h"
#include "vos_diag_core_event.h"

#include "vos_cnss.h"

static DEFINE_SPINLOCK(alloc_lock);

typedef struct
{
   char *name;
   char *value;
}tCfgIniEntry;
#define MAX_CFG_INI_ITEMS   1024
#define DBG_STRING(id) [id] = #id

#ifdef CONFIG_SLUB_DEBUG
#define VOS_MAX_STACK_TRACE			64
#endif

/* pre-alloced at load time
 * following three definition must fit
 * to avoid memory corruption */

struct wcnss_prealloc {
        v_UINT_t occupied;
        v_UINT_t size;
        v_VOID_t *ptr;
};

static struct wcnss_prealloc wcnss_allocs[] = {
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 40 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 256 * 1024, NULL},
	{0, 1024 * 1024, NULL},
};
#if defined(FEATURE_SKB_PRE_ALLOC)
static struct wcnss_prealloc wcnss_skb_allocs[] = {
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 20 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 64 * 1024, NULL},
	{0, 128 * 1024, NULL},
	{0, 128 * 1024, NULL},
};
#endif

/* pre-alloced at load time*/
static v_UINT_t wcnss_prealloc_init(v_VOID_t)
{
	v_UINT_t i,j;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		wcnss_allocs[i].occupied = 0;
		wcnss_allocs[i].ptr = kmalloc(wcnss_allocs[i].size, GFP_KERNEL);
		if (wcnss_allocs[i].ptr == NULL){
			for (j = 0; j < i; j++) {
				kfree(wcnss_allocs[j].ptr);
			}
			pr_err("wcnss_pre: %s: init failed.\n", __func__);
			return -ENOMEM;
		}
	}

#if defined(FEATURE_SKB_PRE_ALLOC)
	for (i = 0; i < ARRAY_SIZE(wcnss_skb_allocs); i++) {
		wcnss_skb_allocs[i].occupied = 0;
		wcnss_skb_allocs[i].ptr =
				__dev_alloc_skb(wcnss_skb_allocs[i].size, (in_interrupt() || in_atomic()) ? GFP_ATOMIC : (GFP_KERNEL | __GFP_RETRY_MAYFAIL));
		if (wcnss_skb_allocs[i].ptr == NULL)
			return -ENOMEM;
	}
#endif
	return 0;
}

static v_VOID_t wcnss_prealloc_deinit(v_VOID_t)
{
	v_UINT_t i;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		wcnss_allocs[i].occupied = 0;
		if (wcnss_allocs[i].ptr != NULL){
			kfree(wcnss_allocs[i].ptr);
		}
	}
#if defined(FEATURE_SKB_PRE_ALLOC)
	for (i = 0; i < ARRAY_SIZE(wcnss_skb_allocs); i++) {
		wcnss_skb_allocs[i].occupied = 0;
		if (wcnss_skb_allocs[i].ptr != NULL){
			dev_kfree_skb(wcnss_skb_allocs[i].ptr);
			wcnss_skb_allocs[i].ptr = NULL;
		}
	}
#endif
}

v_VOID_t *wcnss_prealloc_get(v_UINT_t size)
{
	v_UINT_t i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (wcnss_allocs[i].occupied)
			continue;
		if (wcnss_allocs[i].size >= size) {
			/* we found the slot */
			wcnss_allocs[i].occupied = 1;
			spin_unlock_irqrestore(&alloc_lock, flags);
			return wcnss_allocs[i].ptr;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);

	pr_err("wcnss_pre: %s: prealloc not available at size %d\n", __func__, size);
	return NULL;
}
EXPORT_SYMBOL(wcnss_prealloc_get);

v_SINT_t wcnss_prealloc_put(v_VOID_t *ptr)
{
	v_UINT_t i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		if (wcnss_allocs[i].ptr == ptr) {
			wcnss_allocs[i].occupied = 0;
			spin_unlock_irqrestore(&alloc_lock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);
	return 0;
}
EXPORT_SYMBOL(wcnss_prealloc_put);

v_VOID_t wcnss_prealloc_reset(v_VOID_t)
{
	v_UINT_t i;

	for (i = 0; i < ARRAY_SIZE(wcnss_allocs); i++) {
		wcnss_allocs[i].occupied = 0;
	}
#if defined(FEATURE_SKB_PRE_ALLOC)
	for (i = 0; i < ARRAY_SIZE(wcnss_skb_allocs); i++) {
		wcnss_skb_allocs[i].occupied = 0;
	}
#endif
}
EXPORT_SYMBOL(wcnss_prealloc_reset);

#if defined(FEATURE_SKB_PRE_ALLOC)

struct sk_buff *wcnss_skb_prealloc_get(unsigned int size)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_skb_allocs); i++) {
		if (wcnss_skb_allocs[i].occupied)
			continue;

		if (wcnss_skb_allocs[i].size > size) {
			/* we found the slot */
			wcnss_skb_allocs[i].occupied = 1;
			spin_unlock_irqrestore(&alloc_lock, flags);
			return wcnss_skb_allocs[i].ptr;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);

	pr_err("wcnss_pre: %s: prealloc not available for size: %d\n",
	       __func__, size);

	return NULL;
}
EXPORT_SYMBOL(wcnss_skb_prealloc_get);

int wcnss_skb_prealloc_put(struct sk_buff *skb)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&alloc_lock, flags);
	for (i = 0; i < ARRAY_SIZE(wcnss_skb_allocs); i++) {
		if (wcnss_skb_allocs[i].ptr == skb) {
			wcnss_skb_allocs[i].occupied = 0;
			spin_unlock_irqrestore(&alloc_lock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&alloc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(wcnss_skb_prealloc_put);
#endif
static int __init wlan_prealloc_init(void)
{
        return wcnss_prealloc_init();
}

static void __exit wlan_prealloc_exit(void)
{
        wcnss_prealloc_deinit();
}

module_init(wlan_prealloc_init)
module_exit(wlan_prealloc_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");
