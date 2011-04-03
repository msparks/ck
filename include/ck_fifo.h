/*
 * Copyright 2010-2011 Samy Al Bahra.
 * Copyright 2011 David Joseph.
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

#ifndef _CK_FIFO_H
#define _CK_FIFO_H

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <stddef.h>

#ifndef CK_F_FIFO_SPSC
#define CK_F_FIFO_SPSC
struct ck_fifo_spsc_entry {
	void *value;
	struct ck_fifo_spsc_entry *next;
};
typedef struct ck_fifo_spsc_entry ck_fifo_spsc_entry_t;

struct ck_fifo_spsc {
	struct ck_fifo_spsc_entry *head;
	char pad[CK_MD_CACHELINE - sizeof(struct ck_fifo_spsc_entry *)];
	struct ck_fifo_spsc_entry *tail;
	struct ck_fifo_spsc_entry *head_snapshot;
	struct ck_fifo_spsc_entry *garbage;
};
typedef struct ck_fifo_spsc ck_fifo_spsc_t;

CK_CC_INLINE static void
ck_fifo_spsc_init(struct ck_fifo_spsc *fifo, struct ck_fifo_spsc_entry *stub)
{

	ck_pr_store_ptr(&stub->next, NULL);
	ck_pr_store_ptr(&fifo->head, stub);
	ck_pr_store_ptr(&fifo->tail, stub);
	ck_pr_store_ptr(&fifo->head_snapshot, stub);
	ck_pr_store_ptr(&fifo->garbage, stub);
	return;
}

CK_CC_INLINE static void
ck_fifo_spsc_enqueue(struct ck_fifo_spsc *fifo,
		     struct ck_fifo_spsc_entry *entry,
		     void *value)
{

	ck_pr_store_ptr(&entry->value, value);
	ck_pr_store_ptr(&entry->next, NULL);
	ck_pr_store_ptr(&fifo->tail->next, entry);
	ck_pr_fence_store();
	ck_pr_store_ptr(&fifo->tail, entry);
	return;
}

CK_CC_INLINE static bool 
ck_fifo_spsc_dequeue(struct ck_fifo_spsc *fifo, void *value)
{
	struct ck_fifo_spsc_entry *stub, *entry;
	void *store;

	/*
	 * The head pointer is guaranteed to always point to a stub entry.
	 * If the stub entry does not point to an entry, then the queue is
	 * empty.
	 */
	stub = ck_pr_load_ptr(&fifo->head);
	ck_pr_fence_load();
	entry = stub->next;
	if (entry == NULL)
		return (false);

	store = ck_pr_load_ptr(&entry->value);
	ck_pr_store_ptr(value, store);
	ck_pr_store_ptr(&fifo->head, entry);
	return (true);
}

/*
 * Recycle a node. This technique for recycling nodes is based on
 * Dmitriy Vyukov's work.
 */
CK_CC_INLINE static struct ck_fifo_spsc_entry * 
ck_fifo_spsc_recycle(struct ck_fifo_spsc *fifo)
{
	struct ck_fifo_spsc_entry *p, *garbage;

	garbage = ck_pr_load_ptr(&fifo->garbage);
	p = ck_pr_load_ptr(&fifo->head_snapshot);

	if (garbage == p) {
		p = ck_pr_load_ptr(&fifo->head);
		ck_pr_store_ptr(&fifo->head_snapshot, p);
		if (garbage == p)
			return (NULL);
	}

	p = garbage;
	ck_pr_store_ptr(&fifo->garbage, garbage->next);
	return (p);
}

#define CK_FIFO_SPSC_ISEMPTY(f)	((f)->head->next == NULL)
#define CK_FIFO_SPSC_FIRST(f)	((f)->head->next)
#define CK_FIFO_SPSC_NEXT(m)	((m)->next)
#define CK_FIFO_SPSC_FOREACH(fifo, entry)			\
	for ((entry) = CK_FIFO_SPSC_FIRST(fifo);		\
	     (entry) != NULL;					\
	     (entry) = CK_FIFO_SPSC_NEXT(entry))
#define CK_FIFO_SPSC_FOREACH_SAFE(fifo, entry, T)		\
	for ((entry) = CK_FIFO_SPSC_FIRST(fifo);		\
	     (entry) != NULL && ((T) = (entry)->next, 1);	\
	     (entry) = (T))

#endif /* CK_F_FIFO_SPSC */

#ifdef CK_F_PR_CAS_PTR_2
#ifndef CK_F_FIFO_MPMC
#define CK_F_FIFO_MPMC
struct ck_fifo_mpmc_entry;
struct ck_fifo_mpmc_pointer {
	struct ck_fifo_mpmc_entry *pointer;
	char *generation CK_CC_PACKED;
} CK_CC_ALIGN(16);

struct ck_fifo_mpmc_entry {
	void *value;
	struct ck_fifo_mpmc_pointer next;
};
typedef struct ck_fifo_mpmc_entry ck_fifo_mpmc_entry_t;

struct ck_fifo_mpmc {
	struct ck_fifo_mpmc_pointer head;
	char pad[CK_MD_CACHELINE - sizeof(struct ck_fifo_mpmc_pointer)];
	struct ck_fifo_mpmc_pointer tail;
};
typedef struct ck_fifo_mpmc ck_fifo_mpmc_t;

CK_CC_INLINE static void
ck_fifo_mpmc_init(struct ck_fifo_mpmc *fifo, struct ck_fifo_mpmc_entry *stub)
{

	ck_pr_store_ptr(&fifo->head.pointer, stub);
	ck_pr_store_ptr(&fifo->head.generation, 0);
	ck_pr_store_ptr(&fifo->tail.pointer, stub);
	ck_pr_store_ptr(&fifo->tail.generation, 0);
	ck_pr_store_ptr(&stub->next.pointer, NULL);
	ck_pr_store_ptr(&stub->next.generation, 0);
	return;
}

CK_CC_INLINE static void
ck_fifo_mpmc_enqueue(struct ck_fifo_mpmc *fifo,
		     struct ck_fifo_mpmc_entry *entry,
		     void *value)
{
	struct ck_fifo_mpmc_pointer tail, next, update;
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	/*
	 * Prepare the upcoming node and make sure to commit the updates
	 * before publishing.
	 */
	entry->value = value;
	entry->next.pointer = NULL;
	entry->next.generation = 0;
	ck_pr_fence_store();

	for (;;) {
		tail.generation = ck_pr_load_ptr(&fifo->tail.generation);
		tail.pointer = ck_pr_load_ptr(&fifo->tail.pointer);

		next.generation = ck_pr_load_ptr(&tail.pointer->next.generation);
		next.pointer = ck_pr_load_ptr(&tail.pointer->next.pointer);

		if (ck_pr_load_ptr(&fifo->tail.generation) != tail.generation) {
			ck_backoff_eb(&backoff);
			continue;
		}

		if (next.pointer != NULL) {
			/*
			 * If the tail pointer has an entry following it then
			 * it needs to be forwarded to the next entry. This
			 * helps us guarantee we are always operating on the
			 * last entry.
			 */
			update.pointer = next.pointer;
			update.generation = tail.generation + 1;
			ck_pr_cas_ptr_2(&fifo->tail, &tail, &update);
		} else {
			/*
			 * Attempt to commit new entry to the end of the
			 * current tail.
			 */
			update.pointer = entry;
			update.generation = next.generation + 1;
			if (ck_pr_cas_ptr_2(&tail.pointer->next, &next, &update) == true)
				break;

			ck_backoff_eb(&backoff);
		}
	}

	/* After a successful insert, forward the tail to the new entry. */
	update.generation = tail.generation + 1;
	ck_pr_cas_ptr_2(&fifo->tail, &tail, &update);
	return;
}

CK_CC_INLINE static bool
ck_fifo_mpmc_dequeue(struct ck_fifo_mpmc *fifo,
		     void *value,
		     ck_fifo_mpmc_entry_t **garbage)
{
	struct ck_fifo_mpmc_pointer head, tail, next, update;
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	for (;;) {
		head.generation = ck_pr_load_ptr(&fifo->head.generation);
		head.pointer = ck_pr_load_ptr(&fifo->head.pointer);

		tail.generation = ck_pr_load_ptr(&fifo->tail.generation);
		tail.pointer = ck_pr_load_ptr(&fifo->tail.pointer);

		next.generation = ck_pr_load_ptr(&head.pointer->next.generation);
		next.pointer = ck_pr_load_ptr(&head.pointer->next.pointer);

		update.pointer = next.pointer;
		if (head.pointer == tail.pointer) {
			/*
			 * The head is guaranteed to always point at a stub
			 * entry. If the stub entry has no references then the
			 * queue is empty.
			 */
			if (next.pointer == NULL)
				return (false);

			/* Forward the tail pointer if necessary. */
			update.generation = tail.generation + 1;
			ck_pr_cas_ptr_2(&fifo->tail, &tail, &update);
		} else {
			/* Save value before commit. */
			*(void **)value = ck_pr_load_ptr(&next.pointer->value);

			/* Forward the head pointer to the next entry. */
			update.generation = head.generation + 1;
			if (ck_pr_cas_ptr_2(&fifo->head, &head, &update) == true)
				break;

			ck_backoff_eb(&backoff);
		}
	}

	*garbage = head.pointer;
	return (true);
}

#define CK_FIFO_MPMC_ISEMPTY(f)	((f)->head.pointer->next.pointer == NULL)
#define CK_FIFO_MPMC_FIRST(f)	((f)->head.pointer->next.pointer)
#define CK_FIFO_MPMC_NEXT(m)	((m)->next.pointer)
#define CK_FIFO_MPMC_FOREACH(fifo, entry)				\
	for ((entry) = CK_FIFO_MPMC_FIRST(fifo);			\
	     (entry) != NULL;						\
	     (entry) = CK_FIFO_MPMC_NEXT(entry))
#define CK_FIFO_MPMC_FOREACH_SAFE(fifo, entry, T)			\
	for ((entry) = CK_FIFO_MPMC_FIRST(fifo);			\
	     (entry) != NULL && ((T) = (entry)->next.pointer, 1);	\
	     (entry) = (T))

#endif /* CK_F_FIFO_MPMC */
#endif /* CK_F_PR_CAS_PTR_2 */

#endif /* _CK_FIFO_H */
