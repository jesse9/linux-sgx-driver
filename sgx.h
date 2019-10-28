/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information:
 * Jarkko Sakkinen <jarkko.sakkinen@linux.intel.com>
 * Intel Finland Oy - BIC 0357606-4 - Westendinkatu 7, 02160 Espoo
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *
 * Jarkko Sakkinen <jarkko.sakkinen@linux.intel.com>
 * Suresh Siddha <suresh.b.siddha@intel.com>
 * Serge Ayoun <serge.ayoun@intel.com>
 * Shay Katz-zamir <shay.katz-zamir@intel.com>
 */

#ifndef __ARCH_INTEL_SGX_H__
#define __ARCH_INTEL_SGX_H__

#include <linux/kref.h>
#include <linux/version.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/mmu_notifier.h>
#include <linux/radix-tree.h>
#include <linux/mm.h>
#include "sgx_arch.h"
#include "sgx_user.h"
#include "sgx_asm.h"

#define SGX_EINIT_SPIN_COUNT	20
#define SGX_EINIT_SLEEP_COUNT	50
#define SGX_EINIT_SLEEP_TIME	20
#define SGX_EDMM_SPIN_COUNT	20

#define SGX_VA_SLOT_COUNT 512

struct sgx_epc_page {
	resource_size_t	pa;
	struct list_head list;
	union {
		struct sgx_encl_page *encl_page;
		struct sgx_va_page *va_page;
		struct sgx_page    *xpage;
	};
};

enum sgx_alloc_flags {
	SGX_ALLOC_ATOMIC	= BIT(0),
};

struct sgx_page {
	struct sgx_epc_page *epc_page;
	struct sgx_va_page *va_page;
	unsigned int va_offset;
	unsigned int flags;
};

struct sgx_va_page {
	struct sgx_epc_page *epc_page;
	struct sgx_va_page *va_page;
	unsigned int va_offset;
	unsigned int flags;
	DECLARE_BITMAP(slots, SGX_VA_SLOT_COUNT);
	int nr_slot_evicted;
	struct list_head list;
};

enum sgx_encl_page_flags {
	SGX_ENCL_PAGE_TCS	= BIT(0),
	SGX_ENCL_PAGE_RESERVED	= BIT(1),
	SGX_ENCL_PAGE_TRIM	= BIT(2),
	SGX_ENCL_PAGE_ADDED	= BIT(3),
	SGX_ENCL_PAGE_VA	= BIT(4),
};

struct sgx_encl_page {
	struct sgx_epc_page *epc_page;
	struct sgx_va_page *va_page;
	unsigned int va_offset;
	unsigned int flags;
	unsigned long addr;
};

struct sgx_tgid_ctx {
	struct pid *tgid;
	struct kref refcount;
	struct list_head encl_list;
	struct list_head list;
};

enum sgx_encl_flags {
	SGX_ENCL_INITIALIZED	= BIT(0),
	SGX_ENCL_DEBUG		= BIT(1),
	SGX_ENCL_SECS_EVICTED	= BIT(2),
	SGX_ENCL_SUSPEND	= BIT(3),
	SGX_ENCL_DEAD		= BIT(4),
};

struct sgx_encl {
	unsigned int flags;
	uint64_t attributes;
	uint64_t xfrm;
	unsigned int secs_child_cnt;
	struct mutex lock;
	struct mm_struct *mm;
	struct file *backing;
	struct file *pcmd;
	struct list_head load_list;
	struct kref refcount;
	unsigned long base;
	unsigned long size;
	unsigned long ssaframesize;
	struct list_head va_pages;
	struct radix_tree_root page_tree;
	struct list_head add_page_reqs;
	struct work_struct add_page_work;
	struct sgx_encl_page secs;
	struct sgx_tgid_ctx *tgid_ctx;
	struct list_head encl_list;
	struct mmu_notifier mmu_notifier;
	unsigned int shadow_epoch;
};

struct sgx_epc_bank {
	unsigned long pa;
#ifdef CONFIG_X86_64
	unsigned long va;
#endif
	unsigned long size;
};

extern struct mutex sgx_va2_mutex;
extern struct mutex sgx_tgid_ctx_mutex;
extern struct list_head sgx_tgid_ctx_list;
extern atomic_t sgx_va_pages_cnt;
extern struct workqueue_struct *sgx_add_page_wq;
extern struct sgx_epc_bank sgx_epc_banks[];
extern int sgx_nr_epc_banks;
extern u64 sgx_encl_size_max_32;
extern u64 sgx_encl_size_max_64;
extern u64 sgx_xfrm_mask;
extern u32 sgx_misc_reserved;
extern u32 sgx_xsave_size_tbl[64];
extern bool sgx_has_sgx2;
extern atomic_t sgx_load_list_nr;
extern const struct vm_operations_struct sgx_vm_ops;
extern int vaevict;

#define sgx_pr_ratelimited(level, encl, fmt, ...) \
		pr_ ## level ## _ratelimited("intel_sgx: [%d:0x%p] " fmt, \
				pid_nr((encl)->tgid_ctx->tgid),	\
				(void *)(encl)->base, ##__VA_ARGS__)

#define sgx_dbg(encl, fmt, ...) \
	sgx_pr_ratelimited(debug, encl, fmt, ##__VA_ARGS__)
#define sgx_info(encl, fmt, ...) \
	sgx_pr_ratelimited(info, encl, fmt, ##__VA_ARGS__)
#define sgx_warn(encl, fmt, ...) \
	sgx_pr_ratelimited(warn, encl, fmt, ##__VA_ARGS__)
#define sgx_err(encl, fmt, ...) \
	sgx_pr_ratelimited(err, encl, fmt, ##__VA_ARGS__)
#define sgx_crit(encl, fmt, ...) \
	sgx_pr_ratelimited(crit, encl, fmt, ##__VA_ARGS__)

/*
 *	A va page has a va page which is a va2 page and a va2 page does not have
 *	a va page.
 *	While a regular page has a regular va page which has a va page.
 *	Tha's the way we can differentiate between the two.
 */
#define sgx_is_va(_epage) (_epage->flags & SGX_ENCL_PAGE_VA)
#define sgx_is_epc_page_va(_epc_page) \
	(_epc_page->xpage->flags & SGX_ENCL_PAGE_VA)
#define sgx_set_vapage_index(_va_page, _idx) \
		(_va_page->flags |= (_idx << 8))
#define sgx_get_vapage_index(_va_page) \
		((_va_page->flags & 0xffffff00) >> 8)

static inline void sgx_set_evicted(struct sgx_page *entry)
{
	if (sgx_is_va(entry))
		return;

	entry->va_page->nr_slot_evicted++;
}

static inline void sgx_set_reloaded(struct sgx_page *entry)
{
	if (sgx_is_va(entry))
		return;

	entry->va_page->nr_slot_evicted--;
}

static inline bool sgx_all_slots_evict(struct sgx_encl *encl,
			struct sgx_va_page *page)
{
	/* A VA page has all its slot used for eviction if one
	 * of the next conditions holds:
	 * 1. All 512 of its slots are used
	 * 2. The SECS page was evicted
	 */
	return ((page->nr_slot_evicted == SGX_VA_SLOT_COUNT) ||
		(encl->flags & SGX_ENCL_SECS_EVICTED));
}

static inline unsigned int sgx_alloc_va_slot(struct sgx_va_page *page)
{
	int slot = find_first_zero_bit(page->slots, SGX_VA_SLOT_COUNT);

	if (slot < SGX_VA_SLOT_COUNT)
		set_bit(slot, page->slots);

	return slot << 3;
}

static inline void sgx_free_va_slot(struct sgx_va_page *page,
				    unsigned int offset, bool evicted)
{
	clear_bit(offset >> 3, page->slots);
	if (evicted)
		page->nr_slot_evicted--;
}

static inline bool sgx_va_slots_empty(struct sgx_va_page *page)
{
	int slot = find_first_bit(page->slots, SGX_VA_SLOT_COUNT);

	if (slot == SGX_VA_SLOT_COUNT)
		return true;

	return false;
}

int sgx_encl_find(struct mm_struct *mm, unsigned long addr,
		  struct vm_area_struct **vma);
void sgx_tgid_ctx_release(struct kref *ref);
int sgx_encl_create(struct sgx_secs *secs);
int sgx_encl_add_page(struct sgx_encl *encl, unsigned long addr, void *data,
		      struct sgx_secinfo *secinfo, unsigned int mrmask);
int sgx_encl_init(struct sgx_encl *encl, struct sgx_sigstruct *sigstruct,
		  struct sgx_einittoken *einittoken);
struct sgx_encl_page *sgx_encl_augment(struct vm_area_struct *vma,
				       unsigned long addr, bool write);
void sgx_encl_release(struct kref *ref);

long sgx_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
long sgx_compat_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
#endif

/* Utility functions */
int sgx_test_and_clear_young(struct sgx_encl_page *page, struct sgx_encl *encl);
struct page *sgx_get_backing(struct sgx_encl *encl,
			     struct sgx_page *entry, bool pcmd);
void sgx_put_backing(struct page *backing, bool write);
void sgx_insert_pte(struct sgx_encl *encl,
		    struct sgx_encl_page *encl_page,
		    struct sgx_epc_page *epc_page,
		    struct vm_area_struct *vma);
int sgx_eremove(struct sgx_epc_page *epc_page);
void sgx_zap_tcs_ptes(struct sgx_encl *encl,
		      struct vm_area_struct *vma);
void sgx_invalidate(struct sgx_encl *encl, bool flush_cpus);
void sgx_flush_cpus(struct sgx_encl *encl);

enum sgx_fault_flags {
	SGX_FAULT_RESERVE	= BIT(0),
};

struct sgx_encl_page *sgx_fault_page(struct vm_area_struct *vma,
				     unsigned long addr,
				     unsigned int flags,
				     struct vm_fault *vmf);

int sgx_init_page(struct sgx_encl *encl, struct sgx_encl_page *entry,
	  unsigned long addr, unsigned int alloc_flags, bool already_locked);
int sgx_add_epc_bank(resource_size_t start, unsigned long size, int bank);
int sgx_page_cache_init(void);
void sgx_page_cache_teardown(void);
struct sgx_epc_page *sgx_alloc_page(unsigned int flags);
struct sgx_va_page *sgx_alloc_va_page(unsigned int alloc_flags);
struct sgx_va_page *sgx_alloc_va2_slot_page(unsigned int *slot,
			unsigned int alloc_flags);
void sgx_free_page(struct sgx_epc_page *entry);
void *sgx_get_page(struct sgx_epc_page *entry);
void sgx_put_page(void *epc_page_vaddr);
void sgx_eblock(struct sgx_encl *encl, struct sgx_epc_page *epc_page);
void sgx_etrack(struct sgx_encl *encl, unsigned int epoch);
void sgx_ipi_cb(void *info);
int sgx_eldu(struct sgx_encl *encl, struct sgx_page *encl_page,
	     struct sgx_epc_page *epc_page, bool is_secs);
long modify_range(struct sgx_range *rg, unsigned long flags);
int remove_page(struct sgx_encl *encl, unsigned long address, bool trim);
int sgx_get_encl(unsigned long addr, struct sgx_encl **encl);
unsigned long get_pcmd_offset(struct sgx_page *entry, struct sgx_encl *encl);

/* load list accessors */
#define load_list_insert_epc_page(_epc_page, _encl) \
	{ \
		list_add_tail(&(_epc_page)->list, &(_encl)->load_list); \
		atomic_inc(&sgx_load_list_nr); \
	}
#define load_list_return_list(_list, _encl, _nr) \
	{ \
		list_splice((_list), &_encl->load_list); \
		atomic_add(_nr, &sgx_load_list_nr); \
	}
#define load_list_init(_encl)		INIT_LIST_HEAD(&_encl->load_list)
#define load_is_list_empty(_encl)	(list_empty(&_encl->load_list))
#define load_list_get_first_epc_page(_encl) \
	list_first_entry(&(_encl)->load_list, struct sgx_epc_page, list)
 #define load_list_extract_epc_page_to_list(_epc_page, _list) \
	do { \
		list_move(&_epc_page->list, _list); \
		atomic_dec(&sgx_load_list_nr); \
	} while (0)
#define load_list_del_epc_page(_epc_page) \
	{ \
		list_del(&_epc_page->list); \
		atomic_dec(&sgx_load_list_nr); \
	}
#define load_list_move_tail(_epc_page, _encl) \
		list_move_tail(&_epc_page->list, &_encl->load_list)

/* va page list accessors */
#define va_list_init(_encl)		INIT_LIST_HEAD(&_encl->va_pages)
#define va_list_is_empty(_encl)	(list_empty(&_encl->va_pages))
#define va_list_get_first_va_page(_encl) \
	list_first_entry(&(_encl)->va_pages, struct sgx_va_page, list)
#define va_list_del_va_page(_va_page) \
	{ \
		list_del(&_va_page->list); \
		atomic_dec(&sgx_va_pages_cnt); \
		if (vaevict) { \
			WARN_ON(!_va_page->va_page); \
			mutex_lock(&sgx_va2_mutex); \
			sgx_free_va_slot(_va_page->va_page, \
				_va_page->va_offset, false); \
			mutex_unlock(&sgx_va2_mutex); \
		} \
	}
/*
 *	The 24 msb of va_page->flags are used as a va page index for eviction.
 *	This index defines the offset of the va page in the backing store
 */
#define va_list_insert(_va_page, _encl) \
	{ \
		struct sgx_va_page *_p; \
		unsigned int _i; \
		if (vaevict && !va_list_is_empty(_encl)) { \
			_p = va_list_get_first_va_page(_encl); \
			_i = sgx_get_vapage_index(_p); \
			_i++; \
			sgx_set_vapage_index(_va_page, _i); \
		} \
		list_add(&_va_page->list, &_encl->va_pages); \
	}

/* free list accessor */
#define free_list_insert_epc_page(_epc_page) \
	{ \
		spin_lock(&sgx_free_list_lock); \
		list_add(&_epc_page->list, &sgx_free_list); \
		sgx_nr_free_pages++; \
		spin_unlock(&sgx_free_list_lock); \
	}
#define free_list_is_empty()	(list_empty(&sgx_free_list))
#define free_list_extract_first() \
({ \
	struct sgx_epc_page *_entry = NULL; \
	spin_lock(&sgx_free_list_lock); \
	if (!free_list_is_empty()) { \
		_entry = list_first_entry(&sgx_free_list, struct sgx_epc_page, \
					 list); \
		list_del(&_entry->list); \
		sgx_nr_free_pages--; \
	} \
	spin_unlock(&sgx_free_list_lock); \
	_entry; \
})

#endif /* __ARCH_X86_INTEL_SGX_H__ */
