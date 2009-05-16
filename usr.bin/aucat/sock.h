/*	$OpenBSD: sock.h,v 1.6 2009/05/16 12:20:31 ratchov Exp $	*/
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
#ifndef SOCK_H
#define SOCK_H

#include "pipe.h"
#include "aparams.h"
#include "amsg.h"

struct sock {
	struct pipe pipe;
	/*
	 * socket and protocol specific stuff, mainly used
	 * to decode/encode messages in the stream.
	 */
	struct amsg rmsg, wmsg;		/* messages being sent/received */
	unsigned rtodo;			/* input bytes not read yet */
	unsigned wtodo;			/* output bytes not written yet */
#define SOCK_RDATA	0		/* data chunk being read */
#define SOCK_RMSG	1		/* amsg query being processed */
#define SOCK_RRET	2		/* amsg reply being returned */
	unsigned rstate;		/* state of the read-end FSM */
#define SOCK_WIDLE	0		/* nothing to do */
#define SOCK_WMSG	1		/* amsg being written */
#define SOCK_WDATA	2		/* data chunk being written */
	unsigned wstate;		/* state of the write-end FSM */
#define SOCK_HELLO	0		/* waiting for HELLO message */
#define SOCK_INIT	1		/* parameter negotiation */
#define SOCK_START	2		/* filling play buffers */
#define SOCK_RUN	3		/* attached to the mix / sub */
	unsigned pstate;		/* one of the above */
	unsigned mode;			/* a set of AMSG_PLAY, AMSG_REC */
	struct aparams rpar;		/* read (ie play) parameters */
	struct aparams wpar;		/* write (ie rec) parameters */
	int delta;			/* pos. change to send */
	int tickpending;		/* delta waiting to be transmitted */
	unsigned bufsz;			/* total buffer size */
	unsigned round;			/* block size */
	unsigned xrun;			/* one of AMSG_IGNORE, ... */
	int vol;			/* requested volume */
	int maxweight;			/* max dynamic range */
	struct aparams templ_rpar;	/* template for rpar */
	struct aparams templ_wpar;	/* template for wpar */
};

struct sock *sock_new(struct fileops *, int fd, char *,
    struct aparams *, struct aparams *, int);
extern struct fileops sock_ops;

#endif /* !defined(SOCK_H) */
