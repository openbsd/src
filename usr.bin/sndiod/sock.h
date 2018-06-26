/*	$OpenBSD: sock.h,v 1.5 2018/06/26 07:13:54 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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

#include "amsg.h"

struct file;
struct slot;
struct midi;

struct sock {
	struct sock *next;
	int fd;
	struct file *file;
	struct amsg rmsg, wmsg;		/* messages being sent/received */
	unsigned int wmax;		/* max bytes we're allowed to write */
	unsigned int rmax;		/* max bytes we're allowed to read */
	unsigned int rsize;		/* input bytes to read (DATA msg) */
	unsigned int wsize;		/* output bytes to write (DATA msg) */
	unsigned int rtodo;		/* input bytes not read yet */
	unsigned int wtodo;		/* output bytes not written yet */
#define SOCK_RIDLE	0		/* not expecting messages */
#define SOCK_RMSG	1		/* expecting a message */
#define SOCK_RDATA	2		/* data chunk being read */
#define SOCK_RRET	3		/* reply being returned */
	unsigned int rstate;		/* state of the read-end FSM */
#define SOCK_WIDLE	0		/* nothing to do */
#define SOCK_WMSG	1		/* amsg being written */
#define SOCK_WDATA	2		/* data chunk being written */
	unsigned int wstate;		/* state of the write-end FSM */
#define SOCK_AUTH	0		/* waiting for AUTH message */
#define SOCK_HELLO	1		/* waiting for HELLO message */
#define SOCK_INIT	2		/* parameter negotiation */
#define SOCK_START	3		/* filling play buffers */
#define SOCK_STOP	4		/* draining rec buffers */
	unsigned int pstate;		/* one of the above */
	int tickpending;		/* tick waiting to be transmitted */
	int fillpending;		/* flowctl waiting to be transmitted */
	int stoppending;		/* last STOP ack to be sent */
	unsigned int walign;		/* align written data to this */
	unsigned int ralign;		/* read data is aligned to this */
	int lastvol;			/* last volume */
	struct slot *slot;		/* audio device slot number */
	struct midi *midi;		/* midi endpoint */
	struct port *port;		/* midi port */
};

struct sock *sock_new(int fd);
void sock_close(struct sock *);
extern struct sock *sock_list;

#endif /* !defined(SOCK_H) */
