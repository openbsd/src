/*	$OpenBSD: sio_priv.h,v 1.2 2011/04/16 10:52:22 ratchov Exp $	*/
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
#ifndef SNDIO_PRIV_H
#define SNDIO_PRIV_H

#include <sys/param.h>
#include "sndio.h"

/*
 * private ``handle'' structure
 */
struct sio_hdl {
	struct sio_ops *ops;
	void (*move_cb)(void *, int);	/* call-back for realpos changes */
	void *move_addr;		/* user priv. data for move_cb */
	void (*vol_cb)(void *, unsigned); /* call-back for volume changes */
	void *vol_addr;			/* user priv. data for vol_cb */
	unsigned mode;			/* SIO_PLAY | SIO_REC */
	int started;			/* true if started */
	int nbio;			/* true if non-blocking io */
	int eof;			/* true if error occured */
#ifdef DEBUG
	unsigned long long pollcnt;	/* times sio_revents was called */
	unsigned long long wcnt;	/* bytes written with sio_write() */
	unsigned long long rcnt;	/* bytes read with sio_read() */
	long long realpos;
	struct timeval tv;
	struct sio_par par;
#endif
};

/*
 * operations every device should support
 */
struct sio_ops {
	void (*close)(struct sio_hdl *);
	int (*setpar)(struct sio_hdl *, struct sio_par *);
	int (*getpar)(struct sio_hdl *, struct sio_par *);
	int (*getcap)(struct sio_hdl *, struct sio_cap *);
	size_t (*write)(struct sio_hdl *, const void *, size_t);
	size_t (*read)(struct sio_hdl *, void *, size_t);
	int (*start)(struct sio_hdl *);
	int (*stop)(struct sio_hdl *);
	int (*nfds)(struct sio_hdl *);
	int (*pollfd)(struct sio_hdl *, struct pollfd *, int);
	int (*revents)(struct sio_hdl *, struct pollfd *);
	int (*setvol)(struct sio_hdl *, unsigned);
	void (*getvol)(struct sio_hdl *);
};

struct sio_hdl *sio_aucat_open(const char *, unsigned, int);
struct sio_hdl *sio_sun_open(const char *, unsigned, int);
void sio_create(struct sio_hdl *, struct sio_ops *, unsigned, int);
void sio_destroy(struct sio_hdl *);
void sio_onmove_cb(struct sio_hdl *, int);
void sio_onvol_cb(struct sio_hdl *, unsigned);

#endif /* !defined(SNDIO_PRIV_H) */
