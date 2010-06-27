/*	$OpenBSD: kern_bufq.c,v 1.7 2010/06/27 04:29:31 kettenis Exp $	*/
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

#ifdef BUFQ_DEBUG
#define	BUFQDBG_INIT		0x0001
#define	BUFQDBG_DRAIN		0x0002
#define	BUFQDBG_DISKSORT	0x0004
#define	BUFQDBG_FIFO		0x0008
int	bqdebug = 0;
#define	DPRINTF(p...)		do { if (bqdebug) printf(p); } while (0)
#define	DNPRINTF(n, p...)	do { if ((n) & bqdebug) printf(p); } while (0)
#else
#define	DPRINTF(p...)		/* p */
#define	DNPRINTF(n, p...)	/* n, p */
#endif

struct buf *(*bufq_dequeue[BUFQ_HOWMANY])(struct bufq *, int) = {
	bufq_disksort_dequeue,
	bufq_fifo_dequeue
};
void (*bufq_queue[BUFQ_HOWMANY])(struct bufq *, struct buf *) = {
	bufq_disksort_queue,
	bufq_fifo_queue
};
void (*bufq_requeue[BUFQ_HOWMANY])(struct bufq *, struct buf *) = {
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

	DNPRINTF(BUFQDBG_INIT, "%s: initing bufq %p of type %i\n",
	    __func__, bq, type);

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

	return (bq);
}

void
bufq_destroy(struct bufq *bq)
{
	bufq_drain(bq);

	DNPRINTF(BUFQDBG_INIT, "%s: destroying bufq %p\n", __func__, bq);

	if (bq->bufq_data != NULL)
		free(bq->bufq_data, M_DEVBUF);

	free(bq, M_DEVBUF);
}

void
bufq_drain(struct bufq *bq)
{
	struct buf	*bp;
	int		 s;

	DNPRINTF(BUFQDBG_DRAIN, "%s: draining bufq %p\n",
	    __func__, bq);

	while ((bp = BUFQ_DEQUEUE(bq)) != NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		s = splbio();
		biodone(bp);
		splx(s);
	}
}

void
bufq_disksort_queue(struct bufq *bq, struct buf *bp)
{
	struct buf	*bufq;

	bufq = (struct buf  *)bq->bufq_data;

	DNPRINTF(BUFQDBG_DISKSORT, "%s: queueing bp %p in bufq %p\n",
	    __func__, bp, bq);

	mtx_enter(&bq->bufq_mtx);
	disksort(bufq, bp);
	mtx_leave(&bq->bufq_mtx);
}

void
bufq_disksort_requeue(struct bufq *bq, struct buf *bp)
{
	struct buf	*bufq;

	bufq = (struct buf *)bq->bufq_data;

	DNPRINTF(BUFQDBG_DISKSORT, "%s: requeueing bp % in bufq %p\n",
	    __func__, bp, bufq);

	mtx_enter(&bq->bufq_mtx);
	bp->b_actf = bufq->b_actf;
	bufq->b_actf = bp;
	if (bp->b_actf == NULL)
		bufq->b_actb = &bp->b_actf;
	mtx_leave(&bq->bufq_mtx);
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

	DNPRINTF(BUFQDBG_DISKSORT, "%s: %s buf %p from bufq %p\n", __func__, 
	    peeking ? "peeking at" : "dequeueing", bp, bq);

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

	DNPRINTF(BUFQDBG_FIFO, "%s: queueing bp %p in bufq %p\n",
	    __func__, bp, bq);

	mtx_enter(&bq->bufq_mtx);
	TAILQ_INSERT_TAIL(head, bp, b_bufq.bufq_data_fifo.bqf_entries);
	mtx_leave(&bq->bufq_mtx);
}

void
bufq_fifo_requeue(struct bufq *bq, struct buf *bp)
{
	struct bufq_fifo_head	*head = bq->bufq_data;

	DNPRINTF(BUFQDBG_FIFO, "%s: requeueing bp % in bufq %p\n",
	    __func__, bp, bq);

	mtx_enter(&bq->bufq_mtx);
	TAILQ_INSERT_HEAD(head, bp, b_bufq.bufq_data_fifo.bqf_entries);
	mtx_leave(&bq->bufq_mtx);
}

struct buf *
bufq_fifo_dequeue(struct bufq *bq, int peeking)
{
	struct	bufq_fifo_head	*head = bq->bufq_data;
	struct	buf		*bp;

	mtx_enter(&bq->bufq_mtx);
	bp = TAILQ_FIRST(head);
	if (bp != NULL && !peeking)
		TAILQ_REMOVE(head, bp, b_bufq.bufq_data_fifo.bqf_entries);
	mtx_leave(&bq->bufq_mtx);

	DNPRINTF(BUFQDBG_FIFO, "%s: %s buf %p from bufq %p\n", __func__, 
	    peeking ? "peeking at" : "dequeueing", bp, bq);

	return (bp);
}

int
bufq_fifo_init(struct bufq *bq)
{
	struct bufq_fifo_head	*head;

	head = malloc(sizeof(*head), M_DEVBUF, M_NOWAIT);
	if (head == NULL)
		return (ENOMEM);

	TAILQ_INIT(head);
	bq->bufq_data = head;

	return (0);
}
