/*	$OpenBSD: abuf.c,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
/*
 * Simple byte fifo. It has one reader and one writer. The abuf
 * structure is used to interconnect audio processing units (aproc
 * structures).
 *
 * The abuf data is split in two parts: (1) valid data available to the reader
 * (2) space available to the writer, which is not necessarily unused. It works
 * as follows: the write starts filling at offset (start + used), once the data
 * is ready, the writer adds to used the count of bytes available.
 *
 * TODO:
 *
 *	(easy) create abuf_wcommitblk(), abuf_rdiscardblk() instead of tweeking
 *	the fifo pointers by hand. But first, find shorter function names...
 *
 *	(easy) dont initialize aproc-specific stuff in abuf_new(), let the
 *	aproc xxx_new() routines do it
 *
 *	(hard) make abuf_fill() a boolean depending on whether
 *	eof is reached. So the caller can do:
 *
 *		if (!abuf_fill(buf)) {
 *			...
 *		}
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "conf.h"
#include "aproc.h"
#include "abuf.h"

struct abuf *
abuf_new(unsigned nfr, unsigned bpf)
{
	struct abuf *buf;
	unsigned len;

	len = nfr * bpf;
	buf = malloc(sizeof(struct abuf) + len);
	if (buf == NULL) {
		err(1, "abuf_new: malloc");
	}
	buf->bpf = bpf;

	/*
	 * fill fifo pointers
	 */
	buf->len = len;
	buf->used = 0;
	buf->start = 0;
	buf->rproc = NULL;
	buf->wproc = NULL;
	return buf;
}

void
abuf_del(struct abuf *buf)
{
	DPRINTF("abuf_del:\n");
	if (buf->rproc) {
		fprintf(stderr, "abuf_del: has rproc: %s\n", buf->rproc->name);
		abort();
	}
	if (buf->wproc) {
		fprintf(stderr, "abuf_del: has wproc: %s\n", buf->wproc->name);
		abort();
	}
	if (buf->used > 0)
		fprintf(stderr, "abuf_del: used = %u\n", buf->used);
	free(buf);
}

/*
 * Get a pointer to the readable block at the given offset.
 */
unsigned char *
abuf_rgetblk(struct abuf *buf, unsigned *rsize, unsigned ofs)
{
	unsigned count, start, used;

	start = buf->start + ofs;
	used = buf->used - ofs;
	count = buf->len - start;
	if (count > used)
		count = used;
	*rsize = count;
	return buf->data + start;
}

/*
 * Get a pointer to the writable block at offset ofs.
 */
unsigned char *
abuf_wgetblk(struct abuf *buf, unsigned *rsize, unsigned ofs)
{
	unsigned end, avail, count;


	end = buf->start + buf->used + ofs;
	if (end >= buf->len)
		end -= buf->len;
#ifdef DEBUG
	if (end >= buf->len) {
		fprintf(stderr, "abuf_wgetblk: %s -> %s: bad ofs, "
		    "start = %u, used = %u, len = %u, ofs = %u\n",
		    buf->wproc->name, buf->rproc->name,
		    buf->start, buf->used, buf->len, ofs);
		abort();
	}
#endif
	avail = buf->len - (buf->used + ofs);
	count = buf->len - end;
	if (count > avail)
			count = avail;
	*rsize = count;
	return buf->data + end;
}

/*
 * Notify the read end of the buffer that there is input available
 * and that data can be processed again.
 */
void
abuf_flush(struct abuf *buf)
{
	struct aproc *p = buf->rproc;

	for (;;) {
		if (!ABUF_ROK(buf))
			break;
		if (p == NULL || !p->ops->in(p, buf))
			break;
	}
}

/*
 * Notify the write end of the buffer that there is room and data can be
 * written again. This routine can only be called from the out()
 * call-back of the reader.
 *
 * NOTE: The abuf writer may reach eof condition and disappear, dont keep
 * references to abuf->wproc.
 */
void
abuf_fill(struct abuf *buf)
{
	struct aproc *p = buf->wproc;

	for (;;) {
		if (!ABUF_WOK(buf))
			break;
		if (p == NULL || !p->ops->out(p, buf))
			break;
	}
}

/*
 * Run a read/write loop on the buffer until either the reader or the
 * writer blocks, or until the buffer reaches eofs. We can not get hup hear,
 * since hup() is only called from terminal nodes, from the main loop.
 *
 * NOTE: The buffer may disappear (ie. be free()ed) if eof is reached, so
 * do not keep references to the buffer or to its writer or reader.
 */
void
abuf_run(struct abuf *buf)
{
	struct aproc *p;
	int canfill = 1, canflush = 1;

	for (;;) {
		if (ABUF_EOF(buf)) {
			p = buf->rproc;
			DPRINTFN(2, "abuf_run: %s: got eof\n", p->name);
			p->ops->eof(p, buf);
			buf->rproc = NULL;
			abuf_del(buf);
			return;
		}
		if (ABUF_WOK(buf) && canfill && buf->wproc) {
			p = buf->wproc;
			canfill = p->ops->out(p, buf);
		} else if (ABUF_ROK(buf) && canflush) {
			p = buf->rproc;
			canflush = p->ops->in(p, buf);
		} else
			break; /* can neither read nor write */
	}
}

/*
 * Notify the reader that there will be no more input (producer
 * disappeared). The buffer is flushed and eof() is called only if all
 * data is flushed.
 */
void
abuf_eof(struct abuf *buf)
{
#ifdef DEBUG
	if (buf->wproc == NULL) {
		fprintf(stderr, "abuf_eof: no writer\n");
		abort();
	}
#endif
	DPRINTFN(2, "abuf_eof: requested by %s\n", buf->wproc->name);
	buf->wproc = NULL;
	if (buf->rproc != NULL) {
		abuf_flush(buf);
		if (ABUF_ROK(buf)) {
			/*
			 * Could not flush everything, the reader will
			 * have a chance to delete the abuf later.
			 */
			DPRINTFN(2, "abuf_eof: %s will drain the buf later\n",
			    buf->rproc->name);
			return;
		}
		DPRINTFN(2, "abuf_eof: signaling %s\n", buf->rproc->name);
		buf->rproc->ops->eof(buf->rproc, buf);
		buf->rproc = NULL;
	}
	abuf_del(buf);
}


/*
 * Notify the writer that the buffer has no more consumer,
 * and that no more data will accepted.
 */
void
abuf_hup(struct abuf *buf)
{
#ifdef DEBUG
	if (buf->rproc == NULL) {
		fprintf(stderr, "abuf_hup: no reader\n");
		abort();
	}
#endif
	DPRINTFN(2, "abuf_hup: initiated by %s\n", buf->rproc->name);
	buf->rproc = NULL;
	if (buf->wproc != NULL) {
		if (ABUF_ROK(buf)) {
			warnx("abuf_hup: %s: lost %u bytes",
			    buf->wproc->name, buf->used);
			buf->used = 0;
		}
		DPRINTFN(2, "abuf_hup: signaling %s\n", buf->wproc->name);
		buf->wproc->ops->hup(buf->wproc, buf);
		buf->wproc = NULL;
	}
	abuf_del(buf);
}
