/*	$OpenBSD: aproc.h,v 1.2 2008/06/02 17:06:36 ratchov Exp $	*/
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
#ifndef APROC_H
#define APROC_H

#include <sys/queue.h>

#include "aparams.h"

struct abuf;
struct aproc;
struct file;

struct aproc_ops {
	/*
	 * Name of the ops structure, ie type of the unit.
	 */
	char *name;

	/*
	 * The state of the given input abuf changed (eg. an input block
	 * is ready for processing). This function must get the block
	 * from the input, process it and remove it from the buffer.
	 *
	 * Processing the block will result in a change of the state of
	 * OTHER buffers that are attached to the aproc (eg. the output
	 * buffer was filled), thus this routine MUST notify ALL aproc
	 * structures that are waiting on it; most of the time this
	 * means just calling abuf_flush() on the output buffer.
	 */
	int (*in)(struct aproc *, struct abuf *);

	/*
	 * The state of the given output abuf changed (eg. space for a
	 * new output block was made available) so processing can
	 * continue.  This function must process more input in order to
	 * fill the output block.
	 *
	 * Producing a block will result in the change of the state of
	 * OTHER buffers that are attached to the aproc, thus this
	 * routine MUST notify ALL aproc structures that are waiting on
	 * it; most of the time this means calling abuf_fill() on the
	 * source buffers.
	 *
	 * Before filling input buffers (using abuf_fill()), this
	 * routine must ALWAYS check for eof condition, and if needed,
	 * handle it appropriately and call abuf_hup() to free the input
	 * buffer.
	 */
	int (*out)(struct aproc *, struct abuf *);

	/*
	 * The input buffer is empty and we can no more receive data
	 * from it. The buffer will be destroyed as soon as this call
	 * returns so the abuf pointer will stop being valid after this
	 * call returns. There's no need to drain the buffer because the
	 * in() call-back was just called before.
	 *
	 * If this call reads and/or writes data on other buffers,
	 * abuf_flush() and abuf_fill() must be called appropriately.
	 */
	void (*eof)(struct aproc *, struct abuf *);

	/*
	 * The output buffer can no more accept data (it should be
	 * considered as full). After this function returns, it will be
	 * destroyed and the "abuf" pointer will be no more valid.
	 */
	void (*hup)(struct aproc *, struct abuf *);

	/*
	 * A new input was connected.
	 */
	void (*newin)(struct aproc *, struct abuf *);

	/*
	 * A new output was connected
	 */
	void (*newout)(struct aproc *, struct abuf *);
};

struct aconv {
	/*
	 * Format of the buffer. This part is used by conversion code.
	 */

	int bfirst;		/* bytes to skip at startup */
	unsigned rate;		/* frames per second */
	unsigned pos;		/* current position in the stream */
	unsigned nch;		/* number of channels: nch = cmax - cmin + 1 */
	unsigned bps;		/* bytes per sample (padding included) */
	unsigned shift;		/* shift to get 32bit MSB-justified int */
	int sigbit;		/* sign bits to XOR to unsigned samples */
	int bnext;		/* bytes to skip to reach the next byte */
	int snext;		/* bytes to skip to reach the next sample */
	unsigned cmin;		/* provided/consumed channels */
	unsigned bpf;		/* bytes per frame: bpf = nch * bps */
	int ctx[CHAN_MAX];	/* current frame (for resampling) */
};

/*
 * The aproc structure represents a simple audio processing unit; they are
 * interconnected by abuf structures and form a kind of "circuit". The circuit
 * cannot have loops.
 */
struct aproc {
	char *name;				/* for debug purposes */
	struct aproc_ops *ops;			/* call-backs */
	LIST_HEAD(, abuf) ibuflist;		/* list of inputs */
	LIST_HEAD(, abuf) obuflist;		/* list of outputs */
	union {					/* follow type-specific data */
		struct {			/* file/device io */
			struct file *file;	/* file to read/write */
		} io;
		struct {
			struct aconv ist, ost;
		} conv;
		struct {
#define MIX_DROP 1
			unsigned flags;
		} mix;
		struct {
#define SUB_DROP 1
			unsigned flags;
		} sub;
	} u;
};

void aproc_del(struct aproc *);
void aproc_setin(struct aproc *, struct abuf *);
void aproc_setout(struct aproc *, struct abuf *);

struct aproc *rpipe_new(struct file *);
struct aproc *wpipe_new(struct file *);
struct aproc *mix_new(void);
struct aproc *sub_new(void);
struct aproc *conv_new(char *, struct aparams *, struct aparams *);

#endif /* !defined(FIFO_H) */
