/*	$OpenBSD: libsa.h,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
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
#ifndef LIBSA_H
#define LIBSA_H

#include <sys/param.h>

/*
 * private ``handle'' structure
 */
struct sa_hdl;

/*
 * parameters of a full-duplex stream
 */
struct sa_par {
	unsigned bits;	/* bits per sample */
	unsigned bps;	/* bytes per sample */
	unsigned sig;	/* 1 = signed, 0 = unsigned */
	unsigned le;	/* 1 = LE, 0 = BE byte order */
	unsigned msb;	/* 1 = MSB, 0 = LSB aligned */
	unsigned rchan;	/* number channels for recording direction */
	unsigned pchan;	/* number channels for playback direction */
	unsigned rate;	/* frames per second */
	unsigned bufsz;	/* minimum buffer size */
#define SA_IGNORE	0	/* pause during xrun */
#define SA_SYNC		1	/* resync after xrun */
#define SA_ERROR	2	/* terminate on xrun */
	unsigned xrun;	/* what to do on overruns/underruns */
	unsigned round;	/* optimal bufsz divisor */
	int __pad[4];	/* for future use */
	int __magic;	/* for internal/debug purposes only */
};

/*
 * capabilities of a stream
 */
struct sa_cap {
#define SA_NENC		8
#define SA_NCHAN	8
#define SA_NRATE	16
#define SA_NCONF	4
	struct sa_enc {			/* allowed sample encodings */
		unsigned bits;
		unsigned bps;
		unsigned sig;
		unsigned le;
		unsigned msb;
	} enc[SA_NENC];
	unsigned rchan[SA_NCHAN];	/* allowed values for rchan */
	unsigned pchan[SA_NCHAN];	/* allowed values for pchan */
	unsigned rate[SA_NRATE];	/* allowed rates */
	int __pad[7];			/* for future use */
	unsigned nconf;			/* number of elements in confs[] */
	struct sa_conf {
		unsigned enc;		/* mask of enc[] indexes */
		unsigned rchan;		/* mask of chan[] indexes (rec) */
		unsigned pchan;		/* mask of chan[] indexes (play) */
		unsigned rate;		/* mask of rate[] indexes */
	} confs[SA_NCONF];
};

#define SA_XSTRINGS { "ignore", "sync", "error" }

/*
 * mode bitmap
 */
#define SA_PLAY 1
#define SA_REC	2

/*
 * maximum size of the encording string (the longest possible
 * encoding is ``s24le3msb'')
 */
#define SA_ENCMAX	10

/*
 * default bytes per sample for the given bits per sample
 */
#define SA_BPS(bits) (((bits) <= 8) ? 1 : (((bits) <= 16) ? 2 : 4))

/*
 * default value of "sa_par->le" flag
 */
#if BYTE_ORDER == LITTLE_ENDIAN
#define SA_LE_NATIVE 1
#else
#define SA_LE_NATIVE 0
#endif

/*
 * default device for the sun audio(4) back-end
 */
#define SA_SUN_PATH	"/dev/audio"

/*
 * default socket for the aucat(1) back-end
 */
#define SA_AUCAT_PATH	"/tmp/aucat.sock"

int sa_strtoenc(struct sa_par *, char *);
int sa_enctostr(struct sa_par *, char *);
int sa_initpar(struct sa_par *);

struct sa_hdl *sa_open_aucat(char *, unsigned, int);
struct sa_hdl *sa_open_sun(char *, unsigned, int);
struct sa_hdl *sa_open_wav(char *, unsigned, int);
struct sa_hdl *sa_open_raw(char *, unsigned, int);
struct sa_hdl *sa_open(char *, unsigned, int);

void sa_close(struct sa_hdl *);
int sa_setpar(struct sa_hdl *, struct sa_par *);
int sa_getpar(struct sa_hdl *, struct sa_par *);
int sa_getcap(struct sa_hdl *, struct sa_cap *);
void sa_onmove(struct sa_hdl *, void (*)(void *, int), void *);
size_t sa_write(struct sa_hdl *, void *, size_t);
size_t sa_read(struct sa_hdl *, void *, size_t);
int sa_start(struct sa_hdl *);
int sa_stop(struct sa_hdl *);
int sa_nfds(struct sa_hdl *);
int sa_pollfd(struct sa_hdl *, struct pollfd *, int);
int sa_revents(struct sa_hdl *, struct pollfd *);
int sa_eof(struct sa_hdl *);

#endif /* !defined(LIBSA_H) */
