/*	$OpenBSD: sndio.c,v 1.5 2008/11/09 15:32:50 ratchov Exp $	*/
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "sndio_priv.h"

#define SIO_PAR_MAGIC	0x83b905a4

void
sio_initpar(struct sio_par *par)
{
	memset(par, 0xff, sizeof(struct sio_par));
	par->__magic = SIO_PAR_MAGIC;	   
}

/*
 * Generate a string corresponding to the encoding in par,
 * return the length of the resulting string
 */
int
sio_enctostr(struct sio_par *par, char *ostr)
{
	char *p = ostr;

	*p++ = par->sig ? 's' : 'u';
	if (par->bits > 9)
		*p++ = '0' + par->bits / 10;
	*p++ = '0' + par->bits % 10;
	if (par->bps > 1) {
		*p++ = par->le ? 'l' : 'b';
		*p++ = 'e';
		if (par->bps != SIO_BPS(par->bits) ||
		    par->bits < par->bps * 8) {
			*p++ = par->bps + '0';
			if (par->bits < par->bps * 8) {
				*p++ = par->msb ? 'm' : 'l';
				*p++ = 's';
				*p++ = 'b';
			}
		}
	}
	*p++ = '\0';
	return p - ostr - 1;
}

/*
 * Parse an encoding string, examples: s8, u8, s16, s16le, s24be ...
 * Retrun the number of bytes consumed
 */
int
sio_strtoenc(struct sio_par *par, char *istr)
{
	char *p = istr;
	int i, sig, bits, le, bps, msb;
	
#define IS_SEP(c)			\
	(((c) < 'a' || (c) > 'z') &&	\
	 ((c) < 'A' || (c) > 'Z') &&	\
	 ((c) < '0' || (c) > '9'))

	/*
	 * get signedness
	 */
	if (*p == 's') {
		sig = 1;
	} else if (*p == 'u') {
		sig = 0;
	} else
		return 0;
	p++;
	
	/*
	 * get number of bits per sample
	 */
	bits = 0;
	for (i = 0; i < 2; i++) {
		if (*p < '0' || *p > '9')
			break;
		bits = (bits * 10) + *p - '0';
		p++;
	}
	if (bits < 1 || bits > 32)
		return 0;
	bps = SIO_BPS(bits);
	le = SIO_LE_NATIVE;
	msb = 1;

	/*
	 * get (optionnal) endianness
	 */
	if (p[0] == 'l' && p[1] == 'e') {
		le = 1;
		p += 2;
	} else if (p[0] == 'b' && p[1] == 'e') {
		le = 0;
		p += 2;
	} else if (IS_SEP(*p)) {
		goto done;
	} else
		return 0;

	/*
	 * get (optionnal) number of bytes
	 */
	if (*p >= '1' && *p <= '4') {
		bps = *p - '0';
		if (bps * 8  < bits)
			return 0;
		p++;

		/*
		 * get (optionnal) alignement
		 */
		if (p[0] == 'm' && p[1] == 's' && p[2] == 'b') {
			msb = 1;
			p += 3;
		} else if (p[0] == 'l' && p[1] == 's' && p[2] == 'b') {
			msb = 0;
			p += 3;
		} else if (IS_SEP(*p)) {
			goto done;
		} else
			return 0;
	} else if (!IS_SEP(*p))
		return 0;

done:
       	par->msb = msb;
	par->sig = sig;
	par->bits = bits;
	par->bps = bps;
	par->le = le;
	return p - istr;
}


struct sio_hdl *
sio_open(char *str, unsigned mode, int nbio)
{
	struct sio_hdl *hdl;

	if ((mode & (SIO_PLAY | SIO_REC)) == 0)
		return NULL;
	hdl = sio_open_aucat(str, mode, nbio);
	if (hdl != NULL)
		return hdl;
	hdl = sio_open_sun(str, mode, nbio);
	if (hdl != NULL)
		return hdl;
	return NULL;
}

void
sio_create(struct sio_hdl *hdl, struct sio_ops *ops, unsigned mode, int nbio)
{
#ifdef DEBUG
	char *dbg;

	dbg = getenv("LIBSIO_DEBUG");
	if (!dbg || sscanf(dbg, "%u", &hdl->debug) != 1)
		hdl->debug = 0;
#endif	
	hdl->ops = ops;
	hdl->mode = mode;
	hdl->nbio = nbio;
	hdl->started = 0;
	hdl->eof = 0;
	hdl->cb_pos = 0;
}

void
sio_close(struct sio_hdl *hdl)
{
	return hdl->ops->close(hdl);
}

int
sio_start(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		fprintf(stderr, "sio_start: eof\n");
		return 0;
	}
	if (hdl->started) {
		fprintf(stderr, "sio_start: already started\n");
		hdl->eof = 1;
		return 0;
	}
#ifdef DEBUG
	if (!sio_getpar(hdl, &hdl->par))
		return 0;
	hdl->pollcnt = hdl->wcnt = hdl->rcnt = hdl->realpos = 0;
	gettimeofday(&hdl->tv, NULL);
#endif
	if (!hdl->ops->start(hdl))
		return 0;
	hdl->started = 1;
	return 1;
}

int
sio_stop(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		fprintf(stderr, "sio_stop: eof\n");
		return 0;
	}
	if (!hdl->started) {
		fprintf(stderr, "sio_stop: not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->stop(hdl))
		return 0;
#ifdef DEBUG
	if (hdl->debug)
		fprintf(stderr,
		    "libsio: polls: %llu, written = %llu, read: %llu\n",
		    hdl->pollcnt, hdl->wcnt, hdl->rcnt);
#endif
	hdl->started = 0;
	return 1;
}

int
sio_setpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		fprintf(stderr, "sio_setpar: eof\n");
		return 0;
	}
	if (par->__magic != SIO_PAR_MAGIC) {
		fprintf(stderr, 
		    "sio_setpar: use of uninitialized sio_par structure\n");
		hdl->eof = 1;
		return 0;
	}
	if (hdl->started) {
		fprintf(stderr, "sio_setpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (par->rate != (unsigned)~0 && par->bufsz == (unsigned)~0)
		par->bufsz = par->rate * 200 / 1000;
	return hdl->ops->setpar(hdl, par);
}

int
sio_getpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		fprintf(stderr, "sio_getpar: eof\n");
		return 0;
	}
	if (hdl->started) {
		fprintf(stderr, "sio_getpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->getpar(hdl, par)) {
		par->__magic = 0;
		return 0;
	}
	par->__magic = 0;
	return 1;
}

int
sio_getcap(struct sio_hdl *hdl, struct sio_cap *cap)
{
	if (hdl->eof) {
		fprintf(stderr, "sio_getcap: eof\n");
		return 0;
	}
	if (hdl->started) {
		fprintf(stderr, "sio_getcap: already started\n");
		hdl->eof = 1;
		return 0;
	}
	return hdl->ops->getcap(hdl, cap);
}

int
sio_psleep(struct sio_hdl *hdl, int event)
{
	struct pollfd pfd;
	int revents;

	for (;;) {
		sio_pollfd(hdl, &pfd, event);
		while (poll(&pfd, 1, -1) < 0) {
			if (errno == EINTR)
				continue;
			perror("sio_psleep: poll");
			hdl->eof = 1;
			return 0;
		}
		revents = sio_revents(hdl, &pfd);
		if (revents & POLLHUP) {
			fprintf(stderr, "sio_psleep: hang-up\n");
			return 0;
		}
		if (revents & event)
			break;
	}
	return 1;
}

size_t
sio_read(struct sio_hdl *hdl, void *buf, size_t len)
{
	unsigned n;
	char *data = buf;
	size_t todo = len;

	if (hdl->eof) {
		fprintf(stderr, "sio_read: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_REC)) {
		fprintf(stderr, "sio_read: recording not stared\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		fprintf(stderr, "sio_read: zero length read ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->read(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof || todo < len)
				break;
			if (!sio_psleep(hdl, POLLIN))
				break;
			continue;
		}
		data += n;
		todo -= n;
		hdl->rcnt += n;
	}
	return len - todo;
}

size_t
sio_write(struct sio_hdl *hdl, void *buf, size_t len)
{
	unsigned n;
	unsigned char *data = buf;
	size_t todo = len;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (hdl->debug >= 2)
		gettimeofday(&tv0, NULL);
#endif

	if (hdl->eof) {
		fprintf(stderr, "sio_write: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_PLAY)) {
		fprintf(stderr, "sio_write: playback not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		fprintf(stderr, "sio_write: zero length write ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->write(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof)
				break;
			if (!sio_psleep(hdl, POLLOUT))
				break;
			continue;
		}
		data += n;
		todo -= n;
		hdl->wcnt += n;
	}
#ifdef DEBUG
	if (hdl->debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		fprintf(stderr, "%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
		fprintf(stderr, 
		    "sio_write: wrote %d bytes of %d in %uus\n",
		    (int)(len - todo), (int)len, us);
	}
#endif
	return len - todo;
}

int
sio_nfds(struct sio_hdl *hdl)
{
	/*
	 * in the futur we might use larger values
	 */
	return 1;
}

int
sio_pollfd(struct sio_hdl *hdl, struct pollfd *pfd, int events)
{
	if (hdl->eof)
		return 0;
	if (!hdl->started)
		events = 0;
	return hdl->ops->pollfd(hdl, pfd, events);
}

int
sio_revents(struct sio_hdl *hdl, struct pollfd *pfd)
{	
	int revents;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (hdl->debug >= 2)
		gettimeofday(&tv0, NULL);
#endif
	if (hdl->eof)
		return POLLHUP;
	if (!hdl->started)
		return 0;
	hdl->pollcnt++;
	revents = hdl->ops->revents(hdl, pfd);
#ifdef DEBUG
	if (hdl->debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		fprintf(stderr, "%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
		fprintf(stderr, 
		    "sio_revents: revents = 0x%x, complete in %uus\n",
		    revents, us);
	}
#endif
	return revents;
}

int
sio_eof(struct sio_hdl *hdl)
{
	return hdl->eof;
}

void
sio_onmove(struct sio_hdl *hdl, void (*cb)(void *, int), void *addr)
{
	if (hdl->started) {
		fprintf(stderr, "sio_onmove: already started\n");
		hdl->eof = 1;
		return;
	}
	hdl->cb_pos = cb;
	hdl->cb_addr = addr;
}

void
sio_onmove_cb(struct sio_hdl *hdl, int delta)
{
#ifdef DEBUG
	struct timeval tv0, dtv;
	long long playpos;

	if (hdl->debug >= 2 && (hdl->mode & SIO_PLAY)) {
		gettimeofday(&tv0, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		fprintf(stderr, "%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);
		hdl->realpos += delta;
		playpos = hdl->wcnt / (hdl->par.bps * hdl->par.pchan);
		fprintf(stderr,
		    "sio_onmove_cb: delta = %+7d, "
		    "plat = %+7lld, "
		    "realpos = %+7lld, "
		    "bufused = %+7lld\n",
		    delta,
		    playpos - hdl->realpos,
		    hdl->realpos,
		    hdl->realpos < 0 ? playpos : playpos - hdl->realpos);
	}
#endif
	if (hdl->cb_pos)
		hdl->cb_pos(hdl->cb_addr, delta);
}
