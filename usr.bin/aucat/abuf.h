/*	$OpenBSD: abuf.h,v 1.25 2012/04/11 06:05:43 ratchov Exp $	*/
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
#ifndef ABUF_H
#define ABUF_H

#include <sys/queue.h>

struct aproc;
struct aparams;

struct abuf {
	LIST_ENTRY(abuf) ient;	/* reader's list of inputs entry */
	LIST_ENTRY(abuf) oent;	/* writer's list of outputs entry */

	/*
	 * fifo parameters
	 */
	unsigned int bpf;		/* bytes per frame */
	unsigned int cmin, cmax;	/* channel range of this buf */
	unsigned int start;		/* offset where data starts */
	unsigned int used;		/* valid data */
	unsigned int len;		/* size of the ring */
	struct aproc *rproc;		/* reader */
	struct aproc *wproc;		/* writer */
	struct abuf *duplex;		/* link to buffer of the other dir */
	unsigned int inuse;		/* in abuf_{flush,fill,run}() */
	unsigned int tickets;		/* max data to (if throttling) */

	/*
	 * Misc reader aproc-specific per-buffer parameters.
	 */
	union {
		struct {
			int weight;		/* dynamic range */	
			int maxweight;		/* max dynamic range allowed */
			unsigned int vol;	/* volume within the vol */
			unsigned int done;	/* frames ready */
			unsigned int xrun;	/* underrun policy */
			int drop;		/* to drop on next read */
		} mix;
		struct {
			unsigned int st;	/* MIDI running status */
			unsigned int used;	/* bytes used from ``msg'' */
			unsigned int idx;	/* actual MIDI message size */
			unsigned int len;	/* MIDI message length */
#define MIDI_MSGMAX	16			/* max size of MIDI msg */
			unsigned char msg[MIDI_MSGMAX];
		} midi;
	} r;

	/*
	 * Misc reader aproc-specific per-buffer parameters.
	 */
	union {
		struct {
			unsigned int todo;	/* frames to process */
		} mix;
		struct {
			unsigned int done;	/* frames copied */
			unsigned int xrun;	/* one of XRUN_XXX */
			int silence;		/* to add on next write */
		} sub;
		struct {
			struct abuf *owner;	/* current input stream */
		} midi;
	} w;
};

/*
 * the buffer contains at least one frame. This macro should
 * be used to check if the buffer can be flushed
 */
#define ABUF_ROK(b) ((b)->used > 0)

/*
 * there's room for at least one frame
 */
#define ABUF_WOK(b) ((b)->len - (b)->used > 0)

/*
 * the buffer is empty and has no writer anymore
 */
#define ABUF_EOF(b) (!ABUF_ROK(b) && (b)->wproc == NULL)

/*
 * the buffer has no reader anymore, note that it's not
 * enough the buffer to be disconnected, because it can
 * be not yet connected buffer (eg. socket play buffer)
 */
#define ABUF_HUP(b) (!ABUF_WOK(b) && (b)->rproc == NULL)

struct abuf *abuf_new(unsigned int, struct aparams *);
void abuf_del(struct abuf *);
void abuf_dbg(struct abuf *);
void abuf_clear(struct abuf *);
unsigned char *abuf_rgetblk(struct abuf *, unsigned int *, unsigned int);
unsigned char *abuf_wgetblk(struct abuf *, unsigned int *, unsigned int);
void abuf_rdiscard(struct abuf *, unsigned int);
void abuf_wcommit(struct abuf *, unsigned int);
int abuf_fill(struct abuf *);
int abuf_flush(struct abuf *);
void abuf_eof(struct abuf *);
void abuf_hup(struct abuf *);
void abuf_run(struct abuf *);
void abuf_ipos(struct abuf *, int);
void abuf_opos(struct abuf *, int);

#endif /* !defined(ABUF_H) */
