/*	$OpenBSD: kern_bufq.c,v 1.14 2010/07/19 21:39:15 kettenis Exp $	*/
/*
 * Copyright (c) 2010 Thordur I. Bjornsson <thib@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <sys/disklabel.h>

SLIST_HEAD(, bufq)	bufqs = SLIST_HEAD_INITIALIZER(&bufq);
struct mutex		bufqs_mtx = MUTEX_INITIALIZER(IPL_NONE);
int			bufqs_stop;

struct buf *(*bufq_dequeuev[BUFQ_HOWMANY])(struct bufq *, int) = {
	bufq_disksort_dequeue,
	bufq_fifo_dequeue
};
void (*bufq_queuev[BUFQ_HOWMANY])(struct bufq *, struct buf *) = {
	bufq_disksort_queue,
	bufq_fifo_queue
};
void (*bufq_requeuev[BUFQ_HOWMANY])(struct bufq *, struct buf *) = {
	bufq_disksort_requeue,
	bufq_fifo_requeue
};


struct bufq *
bufq_init(int type)
{
	struct bufq	*bq;
	int		 error;

	bq = malloc(sizeof(*bq), M_DEVBUF, M_NOWAIT|M_ZERO);
	KASSERT(bq != NULL);

	mtx_init(&bq->bufq_mtx, IPL_BIO);
	bq->bufq_type = type;

	switch (type) {
	case BUFQ_DISKSORT:
		error = bufq_disksort_init(bq);
		break;
	case BUFQ_FIFO:
		error = bufq_fifo_init(bq);
		break;
	default:
		panic("bufq_init: type %i unknown", type);
		break;
	};

	KASSERT(error == 0);

	mtx_enter(&bufqs_mtx);
	while (bufqs_stop) {
		msleep(&bufqs_stop, &bufqs_mtx, PRIBIO, "bqinit", 0);
	}
	SLIST_INSERT_HEAD(&bufqs, bq, bufq_entries);
	mtx_leave(&bufqs_mtx);

	return (bq);
}

void
bufq_destroy(struct bufq *bq)
{
	bufq_drain(bq);

	if (bq->bufq_data != NULL)
		free(bq->bufq_data, M_DEVBUF);

	mtx_enter(&bufqs_mtx);
	while (bufqs_stop) {
		msleep(&bufqs_stop, &bufqs_mtx, PRIBIO, "bqdest", 0);
	}
	SLIST_REMOVE(&bufqs, bq, bufq, bufq_entries);
	mtx_leave(&bufqs_mtx);

	free(bq, M_DEVBUF);
}

void
bufq_queue(struct bufq *bq, struct buf *bp)
{
	mtx_enter(&bq->bufq_mtx);
	while (bq->bufq_stop) {
		msleep(&bq->bufq_stop, &bq->bufq_mtx, PRIBIO, "bqqueue", 0);
	}
	bufq_queuev[bq->bufq_type](bq, bp);
	mtx_leave(&bq->bufq_mtx);
}

void
bufq_requeue(struct bufq *bq, struct buf *bp)
{
	mtx_enter(&bq->bufq_mtx);
	bufq_requeuev[bq->bufq_type](bq, bp);
	mtx_leave(&bq->bufq_mtx);
}

void
bufq_drain(struct bufq *bq)
{
	struct buf	*bp;
	int		 s;

	while ((bp = BUFQ_DEQUEUE(bq)) != NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		s = splbio();
		biodone(bp);
		splx(s);
	}
}

void
bufq_done(struct bufq *bq, struct buf *bp)
{
	mtx_enter(&bq->bufq_mtx);
	bq->bufq_outstanding--;
	KASSERT(bq->bufq_outstanding >= 0);
	if (bq->bufq_outstanding == 0)
		wakeup(&bq->bufq_outstanding);
	mtx_leave(&bq->bufq_mtx);
	bp->b_bq = NULL;
}

void
bufq_quiesce(void)
{
	struct bufq		*bq;

	mtx_enter(&bufqs_mtx);
	bufqs_stop = 1;
	mtx_leave(&bufqs_mtx);
	SLIST_FOREACH(bq, &bufqs, bufq_entries) {
		mtx_enter(&bq->bufq_mtx);
		bq->bufq_stop = 1;
		while (bq->bufq_outstanding) {
			msleep(&bq->bufq_outstanding, &bq->bufq_mtx,
			    PRIBIO, "bqquies", 0);
		}
		mtx_leave(&bq->bufq_mtx);
	}
}

void
bufq_restart(void)
{
	struct bufq		*bq;

	mtx_enter(&bufqs_mtx);
	SLIST_FOREACH(bq, &bufqs, bufq_entries) {
		mtx_enter(&bq->bufq_mtx);
		bq->bufq_stop = 0;
		wakeup(&bq->bufq_stop);
		mtx_leave(&bq->bufq_mtx);
	}
	bufqs_stop = 0;
	wakeup(&bufqs_stop);
	mtx_leave(&bufqs_mtx);
}

void
bufq_disksort_queue(struct bufq *bq, struct buf *bp)
{
	struct buf	*bufq;

	bufq = (struct buf *)bq->bufq_data;

	bq->bufq_outstanding++;
	bp->b_bq = bq;
	disksort(bufq, bp);
}

void
bufq_disksort_requeue(struct bufq *bq, struct buf *bp)
{
	struct buf	*bufq;

	bufq = (struct buf *)bq->bufq_data;

	bp->b_actf = bufq->b_actf;
	bufq->b_actf = bp;
	if (bp->b_actf == NULL)
		bufq->b_actb = &bp->b_actf;
}

struct buf *
bufq_disksort_dequeue(struct bufq *bq, int peeking)
{
	struct buf	*bufq, *bp;

	mtx_enter(&bq->bufq_mtx);
	bufq = (struct buf *)bq->bufq_data;
	bp = bufq->b_actf;
	if (!peeking) {
		if (bp != NULL)
			bufq->b_actf = bp->b_actf;
		if (bufq->b_actf == NULL)
			bufq->b_actb = &bufq->b_actf;
	}
	mtx_leave(&bq->bufq_mtx);

	return (bp);
}

int
bufq_disksort_init(struct bufq *bq)
{
	int	error = 0;

	bq->bufq_data = malloc(sizeof(struct buf), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	if (bq->bufq_data == NULL)
		error = ENOMEM;

	return (error);
}

void
bufq_fifo_queue(struct bufq *bq, struct buf *bp)
{
	struct bufq_fifo_head	*head = bq->bufq_data;

	bq->bufq_outstanding++;
	bp->b_bq = bq;
	SIMPLEQ_INSERT_TAIL(head, bp, b_bufq.bufq_data_fifo.bqf_entries);
}

void
bufq_fifo_requeue(struct bufq *bq, struct buf *bp)
{
	struct bufq_fifo_head	*head = bq->bufq_data;

	SIMPLEQ_INSERT_HEAD(head, bp, b_bufq.bufq_data_fifo.bqf_entries);
}

struct buf *
bufq_fifo_dequeue(struct bufq *bq, int peeking)
{
	struct	bufq_fifo_head	*head = bq->bufq_data;
	struct	buf		*bp;

	mtx_enter(&bq->bufq_mtx);
	bp = SIMPLEQ_FIRST(head);
	if (bp != NULL && !peeking)
		SIMPLEQ_REMOVE_HEAD(head, b_bufq.bufq_data_fifo.bqf_entries);
	mtx_leave(&bq->bufq_mtx);

	return (bp);
}

int
bufq_fifo_init(struct bufq *bq)
{
	struct bufq_fifo_head	*head;

	head = malloc(sizeof(*head), M_DEVBUF, M_NOWAIT);
	if (head == NULL)
		return (ENOMEM);

	SIMPLEQ_INIT(head);
	bq->bufq_data = head;

	return (0);
}
