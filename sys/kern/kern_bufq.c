/*
 * Copyright (c) 2008, 2009	Thordur I. Bjornsson <thib@openbsd.org>
 * Copyright (c) 2004		Ted Unangst <tedu@openbsd.org>
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
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/errno.h>

#include <sys/disklabel.h>

/* plain old disksort. */
struct buf	*bufq_disksort_get(struct bufq *, int);
void		 bufq_disksort_add(struct bufq *, struct buf *);
int		 bufq_disksort_init(struct bufq *);

struct bufq *
bufq_init(int type)
{
	struct bufq		*bq;
	int			 error;

	KASSERT(type = BUFQ_DISKSORT);

	bq = malloc(sizeof(*bq), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (bq == NULL)
		return (NULL);

	/* For now, only plain old disksort. */
	bq->bufq_type = type;
	bq->bufq_get = bufq_disksort_get;
	bq->bufq_add = bufq_disksort_add;

	error = bufq_disksort_init(bq);
	if (error) {
		free(bq, M_DEVBUF);
		return (NULL);
	}

	return (bq);
}

void
bufq_destroy(struct bufq *bq)
{
	bufq_drain(bq);

	if (bq->bufq_data != NULL)
		free(bq->bufq_data, M_DEVBUF);

	free(bq, M_DEVBUF);
}

void
bufq_drain(struct bufq *bq)
{
	struct buf	*bp;
	int		 s;

	s = splbio();
	while ((bp = BUFQ_GET(bq)) != NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}
	splx(s);

}

void
bufq_disksort_add(struct bufq *bq, struct buf *bp)
{
	struct buf		*bufq;

	splassert(IPL_BIO);

	bufq = (struct buf  *)bq->bufq_data;

	disksort(bufq, bp);
}

struct buf *
bufq_disksort_get(struct bufq *bq, int peeking)
{
	struct buf		*bufq, *bp;

	splassert(IPL_BIO);

	bufq = (struct buf *)bq->bufq_data;
	bp = bufq->b_actf;
	if (bp == NULL)
		return (NULL);
	if (!peeking)
		bufq->b_actf = bp->b_actf;
	return (bp);
}

int
bufq_disksort_init(struct bufq *bq)
{
	int	error = 0;

	bq->bufq_data = malloc(sizeof(struct buf), M_DEVBUF,
	    M_WAITOK|M_ZERO);

	if (bq->bufq_data == NULL)
		error = ENOMEM;

	return (error);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
db_bufq_print(struct bufq *bq, int full, int (*pr)(const char *, ...))
{
	struct buf		*bp, *dp;


	(*pr)(" type %i\n bufq_add %p bufq_get %p bufq_data %p",
	    bq->bufq_type, bq->bufq_add, bq->bufq_get, bq->bufq_data);

	if (full) {
		printf("bufs on queue:\n");
		bp = (struct buf *)bq->bufq_data;
		while ((dp = bp->b_actf) != NULL) {
			printf("%p\n", bp);
			bp = dp;
		}
	}
}

#endif
