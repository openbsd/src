/*	$OpenBSD: libsa_priv.h,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
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
#ifndef LIBSA_PRIV_H
#define LIBSA_PRIV_H

#include <sys/param.h>
#include "libsa.h"

/*
 * private ``handle'' structure
 */
struct sa_hdl {
	struct sa_ops *ops;
	void (*cb_pos)(void *, int);	/* call-back for realpos changes */
	void *cb_addr;			/* user priv. data */
	unsigned mode;			/* SA_PLAY | SA_REC */
	int started;			/* true if started */
	int nbio;			/* true if non-blocking io */
	int eof;			/* true if error occured */
#ifdef DEBUG
	int debug;			/* debug flag */	
	unsigned long long pollcnt;	/* times sa_revents was called */
	unsigned long long wcnt;	/* bytes written with sa_write() */
	unsigned long long rcnt;	/* bytes read with sa_read() */
	long long realpos;
	struct timeval tv;
	struct sa_par par;
#endif
};

/*
 * operations every device should support
 */
struct sa_ops {
	void (*close)(struct sa_hdl *);
	int (*setpar)(struct sa_hdl *, struct sa_par *);
	int (*getpar)(struct sa_hdl *, struct sa_par *);
	int (*getcap)(struct sa_hdl *, struct sa_cap *);
	size_t (*write)(struct sa_hdl *, void *, size_t);
	size_t (*read)(struct sa_hdl *, void *, size_t);
	int (*start)(struct sa_hdl *);
	int (*stop)(struct sa_hdl *);
	int (*pollfd)(struct sa_hdl *, struct pollfd *, int);
	int (*revents)(struct sa_hdl *, struct pollfd *);
};

void sa_create(struct sa_hdl *, struct sa_ops *, unsigned, int);
void sa_destroy(struct sa_hdl *);
void sa_onmove_cb(struct sa_hdl *, int);

#endif /* !defined(LIBSA_PRIV_H) */
