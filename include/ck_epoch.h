/*
 * Copyright 2011 Samy Al Bahra.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_EPOCH_H
#define _CK_EPOCH_H

/*
 * The implementation here is inspired from the work described in:
 *   Fraser, K. 2004. Practical Lock-Freedom. PhD Thesis, University
 *   of Cambridge Computing Laboratory.
 */

#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_stack.h>
#include <stdbool.h>

/*
 * CK_EPOCH_LENGTH must be a power of 2.
 */
#ifndef CK_EPOCH_LENGTH
#define CK_EPOCH_LENGTH 4
#endif

typedef void (*ck_epoch_destructor_t)(ck_stack_entry_t *);

enum {
	CK_EPOCH_USED = 0,
	CK_EPOCH_FREE = 1
};

struct ck_epoch;
struct ck_epoch_record {
	unsigned int active;
	unsigned int epoch;
	ck_stack_t pending[CK_EPOCH_LENGTH];
	unsigned int n_pending;
	unsigned int status;
	unsigned int delta;
	unsigned int n_peak;
	uint64_t     n_reclamations;
	struct ck_epoch *global;
	ck_stack_entry_t record_next;
} CK_CC_CACHELINE;
typedef struct ck_epoch_record ck_epoch_record_t;

struct ck_epoch {
	unsigned int epoch;
	char pad[CK_MD_CACHELINE - sizeof(unsigned int)];
	ck_stack_t records;
	unsigned int threshold;
	unsigned int n_free;
	ck_epoch_destructor_t destroy;
};
typedef struct ck_epoch ck_epoch_t;

CK_STACK_CONTAINER(struct ck_epoch_record, record_next, ck_epoch_record_container)

CK_CC_INLINE static void
ck_epoch_init(struct ck_epoch *global,
	      unsigned int threshold,
	      ck_epoch_destructor_t destroy)
{

	ck_stack_init(&global->records);
	global->epoch = 1;
	global->n_free = 0;
	global->destroy = destroy;
	global->threshold = threshold;
	ck_pr_fence_store();	
	return;
}

CK_CC_INLINE static struct ck_epoch_record *
ck_epoch_recycle(struct ck_epoch *global)
{
	struct ck_epoch_record *record;
	ck_stack_entry_t *cursor;
	unsigned int status;

	if (ck_pr_load_uint(&global->n_free) == 0)
		return (NULL);

	CK_STACK_FOREACH(&global->records, cursor) {
		record = ck_epoch_record_container(cursor);

		if (ck_pr_load_uint(&record->status) == CK_EPOCH_FREE) {
			status = ck_pr_fas_uint(&record->status, CK_EPOCH_USED);
			if (status == CK_EPOCH_FREE) {
				ck_pr_dec_uint(&global->n_free);
				return record;
			}
		}
	}

	return NULL;
}

CK_CC_INLINE static void
ck_epoch_register(struct ck_epoch *global, struct ck_epoch_record *record)
{
	size_t i;

	record->status = CK_EPOCH_USED;
	record->active = 0;
	record->epoch = 0;
	record->delta = 0;
	record->n_pending = 0;
	record->n_peak = 0;
	record->n_reclamations = 0;
	record->global = global;

	for (i = 0; i < CK_EPOCH_LENGTH; i++)
		ck_stack_init(&record->pending[i]);

	ck_pr_fence_store();
	ck_stack_push_upmc(&global->records, &record->record_next);
	return;
}

CK_CC_INLINE static void
ck_epoch_unregister(struct ck_epoch_record *record)
{
	size_t i;

	record->status = CK_EPOCH_FREE;
	record->active = 0;
	record->epoch = 0;
	record->delta = 0;
	record->n_pending = 0;
	record->n_peak = 0;
	record->n_reclamations = 0;

	for (i = 0; i < CK_EPOCH_LENGTH; i++)
		ck_stack_init(&record->pending[i]);

	ck_pr_inc_uint(&record->global->n_free);
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_epoch_tick(struct ck_epoch *global, struct ck_epoch_record *record)
{
	struct ck_epoch_record *c_record;
	ck_stack_entry_t *cursor;
	unsigned int g_epoch = ck_pr_load_uint(&global->epoch);

	g_epoch &= CK_EPOCH_LENGTH - 1;
	CK_STACK_FOREACH(&global->records, cursor) {
		c_record = ck_epoch_record_container(cursor);
		if (ck_pr_load_uint(&c_record->status) == CK_EPOCH_FREE ||
		    c_record == record)
			continue;

		if (ck_pr_load_uint(&c_record->active) == true &&
		    ck_pr_load_uint(&c_record->epoch) != g_epoch)
			return;
	}

	ck_pr_inc_uint(&global->epoch);
	return;
}

CK_CC_INLINE static bool
ck_epoch_reclaim(struct ck_epoch_record *record)
{
	struct ck_epoch *global = record->global;
	unsigned int g_epoch = ck_pr_load_uint(&global->epoch);
	unsigned int epoch = record->epoch;
	ck_stack_entry_t *next, *cursor;

	g_epoch &= CK_EPOCH_LENGTH - 1;
	if (epoch == g_epoch)
		return false;

	/*
	 * This means all threads with a potential reference to a
	 * hazard pointer will have a view as new as or newer than
	 * the calling thread. No active reference should exist to
	 * any object in the record's pending list.
	 */
	CK_STACK_FOREACH_SAFE(&record->pending[g_epoch], cursor, next) {
		global->destroy(cursor);
		record->n_pending--;
		record->n_reclamations++;
	}

	ck_stack_init(&record->pending[g_epoch]);
	record->epoch = g_epoch;
	record->delta = 0;

	return true;
}

CK_CC_INLINE static void
ck_epoch_write_begin(struct ck_epoch_record *record)
{
	struct ck_epoch *global = record->global;

	ck_pr_store_uint(&record->active, 1);
	ck_pr_fence_store();

	for (;;) {
		if (ck_epoch_reclaim(record) == true)
			break;

		if (++record->delta >= global->threshold) {
			record->delta = 0;
			ck_epoch_tick(global, record);
			continue;
		}

		break;
	}

	return;
}

CK_CC_INLINE static void
ck_epoch_read_begin(struct ck_epoch_record *record)
{
	unsigned int g_epoch = ck_pr_load_uint(&record->global->epoch);

	g_epoch &= CK_EPOCH_LENGTH - 1;
	ck_pr_store_uint(&record->epoch, g_epoch);
	ck_pr_store_uint(&record->active, 1);
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_epoch_end(struct ck_epoch_record *record)
{

	ck_pr_fence_store();
	ck_pr_store_uint(&record->active, 0);
	return;
}

CK_CC_INLINE static void
ck_epoch_retire(struct ck_epoch_record *record, ck_stack_entry_t *entry)
{

	ck_stack_push_spnc(&record->pending[record->epoch], entry);
	record->n_pending += 1;
	return;
}

CK_CC_INLINE static void
ck_epoch_free(struct ck_epoch_record *record, ck_stack_entry_t *entry)
{
	unsigned int epoch = ck_pr_load_uint(&record->epoch);
	struct ck_epoch *global = record->global;

	ck_stack_push_spnc(&record->pending[epoch], entry);
	record->n_pending += 1;

	if (record->n_pending > record->n_peak)
		record->n_peak = record->n_pending;

	if (record->n_pending >= global->threshold && ck_epoch_reclaim(record) == false)
		ck_epoch_tick(global, record);

	return;
}

CK_CC_INLINE static void
ck_epoch_purge(struct ck_epoch_record *record)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	while (record->n_pending > 0) {
		ck_epoch_reclaim(record);
		ck_epoch_tick(record->global, record);
		if (record->n_pending > 0)
			ck_backoff_gb(&backoff);
	}

	return;
}

#endif /* _CK_EPOCH_H */
