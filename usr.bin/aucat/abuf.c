/*	$OpenBSD: abuf.c,v 1.19 2010/04/03 17:40:33 ratchov Exp $	*/
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
 */
/*
 * TODO
 *
 *	use blocks instead of frames for WOK and ROK macros. If necessary
 *	(unlikely) define reader block size and writer blocks size to
 *	ease pipe/socket implementation
 */
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aparams.h"
#include "aproc.h"
#include "conf.h"
#ifdef DEBUG
#include "dbg.h"
#endif

#ifdef DEBUG
void
abuf_dbg(struct abuf *buf)
{
	if (buf->wproc) {
		aproc_dbg(buf->wproc);
	} else {
		dbg_puts("none");
	}
	dbg_puts(buf->inuse ? "=>" : "->");
	if (buf->rproc) {
		aproc_dbg(buf->rproc);
	} else {
		dbg_puts("none");
	}
}

void
abuf_dump(struct abuf *buf)
{
	abuf_dbg(buf);
	dbg_puts(": used = ");
	dbg_putu(buf->used);
	dbg_puts("/");
	dbg_putu(buf->len);
	dbg_puts(" start = ");
	dbg_putu(buf->start);
	dbg_puts("\n");
}
#endif

struct abuf *
abuf_new(unsigned nfr, struct aparams *par)
{
	struct abuf *buf;
	unsigned len, bpf;

	bpf = aparams_bpf(par);
	len = nfr * bpf;
	buf = malloc(sizeof(struct abuf) + len);
	if (buf == NULL) {
#ifdef DEBUG
		dbg_puts("couldn't allocate abuf of ");
		dbg_putu(nfr);
		dbg_puts("fr * ");
		dbg_putu(bpf);
		dbg_puts("bpf\n");
		dbg_panic();
#else
		err(1, "malloc");
#endif
	}
	buf->bpf = bpf;
	buf->cmin = par->cmin;
	buf->cmax = par->cmax;
	buf->inuse = 0;

	/*
	 * fill fifo pointers
	 */
	buf->len = nfr;
	buf->used = 0;
	buf->start = 0;
	buf->rproc = NULL;
	buf->wproc = NULL;
	buf->duplex = NULL;
	return buf;
}

void
abuf_del(struct abuf *buf)
{
	if (buf->duplex)
		buf->duplex->duplex = NULL;
#ifdef DEBUG
	if (buf->rproc || buf->wproc) {
		abuf_dbg(buf);
		dbg_puts(": can't delete referenced buffer\n");
		dbg_panic();
	}
	if (ABUF_ROK(buf)) {
		/*
		 * XXX : we should call abort(), here.
		 * However, poll() doesn't seem to return POLLHUP,
		 * so the reader is never destroyed; instead it appears	
		 * as blocked. Fix file_poll(), if fixable, and add
		 * a call to abord() here.
		 */
		if (debug_level >= 3) {
			abuf_dbg(buf);
			dbg_puts(": deleting non-empty buffer, used = ");
			dbg_putu(buf->used);
			dbg_puts("\n");
		}
	}
#endif
	free(buf);
}

/*
 * Clear buffer contents.
 */
void
abuf_clear(struct abuf *buf)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(buf);
		dbg_puts(": cleared\n");
	}
#endif
	buf->used = 0;
	buf->start = 0;
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
	if (start >= buf->len)
		start -= buf->len;
#ifdef DEBUG
	if (start >= buf->len || used > buf->used) {
		abuf_dump(buf);
		dbg_puts(": rgetblk: bad ofs = ");
		dbg_putu(ofs);
		dbg_puts("\n");
		dbg_panic();
	}
#endif
	count = buf->len - start;
	if (count > used)
		count = used;
	*rsize = count;
	return (unsigned char *)buf + sizeof(struct abuf) + start * buf->bpf;
}

/*
 * Discard the block at the start postion.
 */
void
abuf_rdiscard(struct abuf *buf, unsigned count)
{
#ifdef DEBUG
	if (count > buf->used) {
		abuf_dump(buf);
		dbg_puts(": rdiscard: bad count = ");
		dbg_putu(count);
		dbg_puts("\n");
		dbg_panic();
	}
#endif
	buf->used -= count;
	buf->start += count;
	if (buf->start >= buf->len)
		buf->start -= buf->len;
}

/*
 * Commit the data written at the end postion.
 */
void
abuf_wcommit(struct abuf *buf, unsigned count)
{
#ifdef DEBUG
	if (count > (buf->len - buf->used)) {
		abuf_dump(buf);
		dbg_puts(": rdiscard: bad count = ");
		dbg_putu(count);
		dbg_puts("\n");
		dbg_panic();
	}
#endif
	buf->used += count;
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
		abuf_dump(buf);
		dbg_puts(": wgetblk: bad ofs = ");
		dbg_putu(ofs);
		dbg_puts("\n");
		dbg_panic();
	}
#endif
	avail = buf->len - (buf->used + ofs);
	count = buf->len - end;
	if (count > avail)
			count = avail;
	*rsize = count;
	return (unsigned char *)buf + sizeof(struct abuf) + end * buf->bpf;
}

/*
 * Flush buffer either by dropping samples or by calling the aproc
 * call-back to consume data. Return 0 if blocked, 1 otherwise.
 */
int
abuf_flush_do(struct abuf *buf)
{
	struct aproc *p;

	p = buf->rproc;
	if (!p)
		return 0;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": in\n");
	}
#endif
	return p->ops->in(p, buf);
}

/*
 * Fill the buffer either by generating silence or by calling the aproc
 * call-back to provide data. Return 0 if blocked, 1 otherwise.
 */
int
abuf_fill_do(struct abuf *buf)
{
	struct aproc *p;

	p = buf->wproc;
	if (!p)
		return 0;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": out\n");
	}
#endif
	return p->ops->out(p, buf);
}

/*
 * Notify the reader that there will be no more input (producer
 * disappeared) and destroy the buffer.
 */
void
abuf_eof_do(struct abuf *buf)
{
	struct aproc *p;

	p = buf->rproc;
	if (p) {
		buf->rproc = NULL;
		LIST_REMOVE(buf, ient);
		buf->inuse++;
#ifdef DEBUG
		if (debug_level >= 4) {
			aproc_dbg(p);
			dbg_puts(": eof\n");
		}
#endif
		p->ops->eof(p, buf);
		buf->inuse--;
	}
	abuf_del(buf);
}

/*
 * Notify the writer that the buffer has no more consumer,
 * and destroy the buffer.
 */
void
abuf_hup_do(struct abuf *buf)
{
	struct aproc *p;

	if (ABUF_ROK(buf)) {
#ifdef DEBUG
		if (debug_level >= 3) {
			abuf_dbg(buf);
			dbg_puts(": hup: lost ");
			dbg_putu(buf->used);
			dbg_puts(" bytes\n");
		}
#endif
		buf->used = 0;
	}
	p = buf->wproc;
	if (p != NULL) {
		buf->wproc = NULL;
		LIST_REMOVE(buf, oent);
		buf->inuse++;
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": hup\n");
		}
#endif
		p->ops->hup(p, buf);
		buf->inuse--;
	}
	abuf_del(buf);
}

/*
 * Notify the read end of the buffer that there is input available
 * and that data can be processed again.
 */
int
abuf_flush(struct abuf *buf)
{
	if (buf->inuse) {
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(buf);
			dbg_puts(": flush blocked (inuse)\n");
		}
#endif
	} else {
		buf->inuse++;
		for (;;) {
			if (!abuf_flush_do(buf))
				break;
		}
		buf->inuse--;
		if (ABUF_HUP(buf)) {
			abuf_hup_do(buf);
			return 0;
		}
	}
	return 1;
}

/*
 * Notify the write end of the buffer that there is room and data can be
 * written again. This routine can only be called from the out()
 * call-back of the reader.
 *
 * Return 1 if the buffer was filled, and 0 if eof condition occured. The
 * reader must detach the buffer on EOF condition, since its aproc->eof()
 * call-back will never be called.
 */
int
abuf_fill(struct abuf *buf)
{
	if (buf->inuse) {
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(buf);
			dbg_puts(": fill blocked (inuse)\n");
		}
#endif
	} else {
		buf->inuse++;
		for (;;) {
			if (!abuf_fill_do(buf))
				break;
		}
		buf->inuse--;
		if (ABUF_EOF(buf)) {
			abuf_eof_do(buf);
			return 0;
		}
	}
	return 1;
}

/*
 * Run a read/write loop on the buffer until either the reader or the
 * writer blocks, or until the buffer reaches eofs. We can not get hup here,
 * since hup() is only called from terminal nodes, from the main loop.
 *
 * NOTE: The buffer may disappear (ie. be free()ed) if eof is reached, so
 * do not keep references to the buffer or to its writer or reader.
 */
void
abuf_run(struct abuf *buf)
{
	int canfill = 1, canflush = 1;

	if (buf->inuse) {
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(buf);
			dbg_puts(": run blocked (inuse)\n");
		}
#endif
		return;
	}
	buf->inuse++;
	for (;;) {
		if (canfill) {
			if (!abuf_fill_do(buf))
				canfill = 0;
			else
				canflush = 1;
		} else if (canflush) {
			if (!abuf_flush_do(buf))
				canflush = 0;
			else
				canfill = 1;
		} else
			break;
	}
	buf->inuse--;
	if (ABUF_EOF(buf)) {
		abuf_eof_do(buf);
		return;
	}
	if (ABUF_HUP(buf)) {
		abuf_hup_do(buf);
		return;
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
	if (debug_level >= 3) {
		abuf_dbg(buf);
		dbg_puts(": eof requested\n");
	}
	if (buf->wproc == NULL) {
		abuf_dbg(buf);
		dbg_puts(": eof, no writer\n");
		dbg_panic();
	}
#endif
	LIST_REMOVE(buf, oent);
	buf->wproc = NULL;
	if (buf->rproc != NULL) {
		if (!abuf_flush(buf))
			return;
		if (ABUF_ROK(buf)) {
			/*
			 * Could not flush everything, the reader will
			 * have a chance to delete the abuf later.
			 */
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(buf);
				dbg_puts(": eof, blocked (drain)\n");
			}
#endif
			return;
		}
	}
	if (buf->inuse) {
#ifdef DEBUG
		if (debug_level >= 3) {
			abuf_dbg(buf);
			dbg_puts(": eof, blocked (inuse)\n");
		}
#endif
		return;
	}
	abuf_eof_do(buf);
}

/*
 * Notify the writer that the buffer has no more consumer,
 * and that no more data will accepted.
 */
void
abuf_hup(struct abuf *buf)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(buf);
		dbg_puts(": hup requested\n");
	}
	if (buf->rproc == NULL) {
		abuf_dbg(buf);
		dbg_puts(": hup, no reader\n");
		dbg_panic();
	}
#endif
	buf->rproc = NULL;
	LIST_REMOVE(buf, ient);
	if (buf->wproc != NULL) {
		if (buf->inuse) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(buf);
				dbg_puts(": eof, blocked (inuse)\n");
			}
#endif
			return;
		}
	}
	abuf_hup_do(buf);
}

/*
 * Notify the reader of the change of its real-time position
 */
void
abuf_ipos(struct abuf *buf, int delta)
{
	struct aproc *p = buf->rproc;

	if (p && p->ops->ipos) {
		buf->inuse++;
#ifdef DEBUG
		if (debug_level >= 4) {
			aproc_dbg(p);
			dbg_puts(": ipos delta = ");
			dbg_puti(delta);
			dbg_puts("\n");
		}
#endif
		p->ops->ipos(p, buf, delta);
		buf->inuse--;
	}
	if (ABUF_HUP(buf))
		abuf_hup_do(buf);
}

/*
 * Notify the writer of the change of its real-time position
 */
void
abuf_opos(struct abuf *buf, int delta)
{
	struct aproc *p = buf->wproc;

	if (p && p->ops->opos) {
		buf->inuse++;
#ifdef DEBUG
		if (debug_level >= 4) {
			aproc_dbg(p);
			dbg_puts(": opos delta = ");
			dbg_puti(delta);
			dbg_puts("\n");
		}
#endif
		p->ops->opos(p, buf, delta);
		buf->inuse--;
	}
	if (ABUF_HUP(buf))
		abuf_hup_do(buf);
}
