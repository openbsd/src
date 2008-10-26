/*	$OpenBSD: aucat.c,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "amsg.h"
#include "libsa_priv.h"

struct aucat_hdl {
	struct sa_hdl sa;
	int fd;				/* socket */
	struct amsg rmsg, wmsg;		/* temporary messages */
	size_t wtodo, rtodo;		/* bytes to complete the packet */
#define STATE_IDLE	0		/* nothing to do */
#define STATE_MSG	1		/* message being transferred */
#define STATE_DATA	2		/* data being transfered */
	unsigned rstate, wstate;	/* one of above */
	unsigned rbpf, wbpf;		/* read and write bytes-per-frame */
	int maxwrite;			/* latency constraint */
	int events;			/* events the user requested */
};

void aucat_close(struct sa_hdl *);
int aucat_start(struct sa_hdl *);
int aucat_stop(struct sa_hdl *);
int aucat_setpar(struct sa_hdl *, struct sa_par *);
int aucat_getpar(struct sa_hdl *, struct sa_par *);
int aucat_getcap(struct sa_hdl *, struct sa_cap *);
size_t aucat_read(struct sa_hdl *, void *, size_t);
size_t aucat_write(struct sa_hdl *, void *, size_t);
int aucat_pollfd(struct sa_hdl *, struct pollfd *, int);
int aucat_revents(struct sa_hdl *, struct pollfd *);

struct sa_ops aucat_ops = {
	aucat_close,
	aucat_setpar,
	aucat_getpar,
	aucat_getcap,
	aucat_write,
	aucat_read,
	aucat_start,
	aucat_stop,
	aucat_pollfd,
	aucat_revents
};

struct sa_hdl *
sa_open_aucat(char *path, unsigned mode, int nbio)
{
	int s;
	struct aucat_hdl *hdl;
	struct sockaddr_un ca;	
	socklen_t len = sizeof(struct sockaddr_un);

	if (path == NULL)
		path = SA_AUCAT_PATH;
	hdl = malloc(sizeof(struct aucat_hdl));
	if (hdl == NULL)
		return NULL;
	sa_create(&hdl->sa, &aucat_ops, mode, nbio);	

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		free(hdl);
		return NULL;
	}	
	ca.sun_family = AF_UNIX;
	memcpy(ca.sun_path, path, strlen(path) + 1);
	while (connect(s, (struct sockaddr *)&ca, len) < 0) {
		if (errno == EINTR)
			continue;
		while (close(s) < 0 && errno == EINTR)
			; /* retry */
		free(hdl);
		return NULL;
	}
	hdl->fd = s;
	hdl->rstate = STATE_IDLE;
	hdl->rtodo = 0xdeadbeef;
	hdl->wstate = STATE_IDLE;
	hdl->wtodo = 0xdeadbeef;
	return (struct sa_hdl *)hdl;
}

/*
 * read a message, return 0 if blocked
 */
int
aucat_rmsg(struct aucat_hdl *hdl)
{
	ssize_t n;
	unsigned char *data;

	while (hdl->rtodo > 0) {
		data = (unsigned char *)&hdl->rmsg;
		data += sizeof(struct amsg) - hdl->rtodo;
		while ((n = read(hdl->fd, data, hdl->rtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				hdl->sa.eof = 1;
				perror("aucat_rmsg: read");
			}
			return 0;
		}
		if (n == 0) {
			fprintf(stderr, "aucat_rmsg: eof\n");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->rtodo -= n;
	}
	return 1;
}

/*
 * write a message, return 0 if blocked
 */
int
aucat_wmsg(struct aucat_hdl *hdl)
{
	ssize_t n;
	unsigned char *data;

	while (hdl->wtodo > 0) {
		data = (unsigned char *)&hdl->wmsg;
		data += sizeof(struct amsg) - hdl->wtodo;
		while ((n = write(hdl->fd, data, hdl->wtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				hdl->sa.eof = 1;
				perror("aucat_wmsg: write");
			}
			return 0;
		}
		hdl->wtodo -= n;
	}
	return 1;
}

/*
 * execute the next message, return 0 if blocked
 */
int
aucat_runmsg(struct aucat_hdl *hdl)
{
	if (!aucat_rmsg(hdl))
		return 0;
	switch (hdl->rmsg.cmd) {
	case AMSG_DATA:
		if (hdl->rmsg.u.data.size == 0 ||
		    hdl->rmsg.u.data.size % hdl->rbpf) {
			fprintf(stderr, "aucat_read: bad data message\n");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->rstate = STATE_DATA;
		hdl->rtodo = hdl->rmsg.u.data.size;
		break;
	case AMSG_MOVE:
		hdl->maxwrite += hdl->rmsg.u.ts.delta * (int)hdl->wbpf;
		sa_onmove_cb(&hdl->sa, hdl->rmsg.u.ts.delta);
		hdl->rstate = STATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
		break;
	case AMSG_GETPAR:
	case AMSG_ACK:
		hdl->rstate = STATE_IDLE;
		hdl->rtodo = 0xdeadbeef;
		break;
	default:
		fprintf(stderr, "aucat_read: unknown mesg\n");
		hdl->sa.eof = 1;
		return 0;
	}
	return 1;
}

void
aucat_close(struct sa_hdl *sh)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* nothing */
	free(hdl);
}

int
aucat_start(struct sa_hdl *sh)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	struct sa_par par;

	/*
	 * save bpf
	 */
	if (!sa_getpar(&hdl->sa, &par))
		return 0;
	hdl->wbpf = par.bps * par.pchan;
	hdl->rbpf = par.bps * par.rchan;
	hdl->maxwrite = hdl->wbpf * par.bufsz;

	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_START;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rstate = STATE_MSG;
	hdl->rtodo = sizeof(struct amsg);
	if (fcntl(hdl->fd, F_SETFL, O_NONBLOCK) < 0) {
		perror("aucat_start: fcntl(0)");
		hdl->sa.eof = 1;
		return 0;
	}
	return 1;
}

int
aucat_stop(struct sa_hdl *sh)
{
#define ZERO_MAX 0x400
	static unsigned char zero[ZERO_MAX];
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	unsigned n, count, todo;

	if (fcntl(hdl->fd, F_SETFL, 0) < 0) {
		perror("aucat_stop: fcntl(0)");
		hdl->sa.eof = 1;
		return 0;
	}

	/*
	 * complete data block in progress
	 */
	if (hdl->wstate != STATE_IDLE) {
		todo = (hdl->wstate == STATE_MSG) ? 
		    hdl->wmsg.u.data.size : hdl->wtodo;
		hdl->maxwrite = todo;
		memset(zero, 0, ZERO_MAX);
		while (todo > 0) {
			count = todo;
			if (count > ZERO_MAX)
				count = ZERO_MAX;
			n = aucat_write(&hdl->sa, zero, count);
			if (n == 0)
				return 0;
			todo -= n;
		}
	}

	/*
	 * send stop message
	 */
	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_STOP;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;

	/*
	 * wait for the STOP ACK
	 */
	while (hdl->rstate != STATE_IDLE) {
		switch (hdl->rstate) {
		case STATE_MSG:
			if (!aucat_runmsg(hdl))
				return 0;
			break;
		case STATE_DATA:
			if (!aucat_read(&hdl->sa, zero, ZERO_MAX))
				return 0;
			break;
		}
	}
	return 1;
}

int
aucat_setpar(struct sa_hdl *sh, struct sa_par *par)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_SETPAR;
	hdl->wmsg.u.par.bits = par->bits;
	hdl->wmsg.u.par.bps = par->bps;
	hdl->wmsg.u.par.sig = par->sig;
	hdl->wmsg.u.par.le = par->le;
	hdl->wmsg.u.par.msb = par->msb;
	hdl->wmsg.u.par.rate = par->rate;
	hdl->wmsg.u.par.bufsz = par->bufsz;
	hdl->wmsg.u.par.xrun = par->xrun;
	hdl->wmsg.u.par.mode = hdl->sa.mode;
	if (hdl->sa.mode & SA_REC)
		hdl->wmsg.u.par.rchan = par->rchan;
	if (hdl->sa.mode & SA_PLAY)
		hdl->wmsg.u.par.pchan = par->pchan;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	return 1;
}

int
aucat_getpar(struct sa_hdl *sh, struct sa_par *par)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->rmsg);
	hdl->wmsg.cmd = AMSG_GETPAR;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rtodo = sizeof(struct amsg);
	if (!aucat_rmsg(hdl))
		return 0;
	if (hdl->rmsg.cmd != AMSG_GETPAR) {
		fprintf(stderr, "aucat_getpar: protocol err\n");
		hdl->sa.eof = 1;
		return 0;
	}
	par->bits = hdl->rmsg.u.par.bits;
	par->bps = hdl->rmsg.u.par.bps;
	par->sig = hdl->rmsg.u.par.sig;
	par->le = hdl->rmsg.u.par.le;
	par->msb = hdl->rmsg.u.par.msb;
	par->rate = hdl->rmsg.u.par.rate;
	par->bufsz = hdl->rmsg.u.par.bufsz;
	par->xrun = hdl->rmsg.u.par.xrun;
	par->round = hdl->rmsg.u.par.round;
	if (hdl->sa.mode & SA_PLAY)
		par->pchan = hdl->rmsg.u.par.pchan;
	if (hdl->sa.mode & SA_REC)
		par->rchan = hdl->rmsg.u.par.rchan;
	return 1;
}

int
aucat_getcap(struct sa_hdl *sh, struct sa_cap *cap)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->rmsg);
	hdl->wmsg.cmd = AMSG_GETCAP;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rtodo = sizeof(struct amsg);
	if (!aucat_rmsg(hdl))
		return 0;
	if (hdl->rmsg.cmd != AMSG_GETCAP) {
		fprintf(stderr, "aucat_getcap: protocol err\n");
		hdl->sa.eof = 1;
		return 0;
	}
	cap->enc[0].bits = hdl->rmsg.u.cap.bits;
	cap->enc[0].bps = SA_BPS(hdl->rmsg.u.cap.bits);
	cap->enc[0].sig = 1;
	cap->enc[0].le = SA_LE_NATIVE;
	cap->enc[0].msb = 1;
	cap->rchan[0] = hdl->rmsg.u.cap.rchan;
	cap->pchan[0] = hdl->rmsg.u.cap.pchan;
	cap->rate[0] = hdl->rmsg.u.cap.rate;
	cap->confs[0].enc = 1;
	cap->confs[0].pchan = (hdl->sa.mode & SA_PLAY) ? 1 : 0;
	cap->confs[0].rchan = (hdl->sa.mode & SA_REC) ? 1 : 0;
	cap->confs[0].rate = 1;
	cap->nconf = 1;
	return 1;
}

size_t
aucat_read(struct sa_hdl *sh, void *buf, size_t len)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	ssize_t n;

	while (hdl->rstate != STATE_DATA) {
		switch (hdl->rstate) {
		case STATE_MSG:
			if (!aucat_runmsg(hdl))
				return 0;
			break;
		case STATE_IDLE:
			fprintf(stderr, "aucat_read: unexpected idle\n");
			break;
		}
	}
	if (len > hdl->rtodo)
		len = hdl->rtodo;
	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			hdl->sa.eof = 1;
			perror("aucat_read: read");
		}
		return 0;
	}
	if (n == 0) {
		fprintf(stderr, "aucat_read: eof\n");
		hdl->sa.eof = 1;
		return 0;
	}
	hdl->rtodo -= n;
	if (hdl->rtodo == 0) {
		hdl->rstate = STATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
	}
	return n;
}

size_t
aucat_write(struct sa_hdl *sh, void *buf, size_t len)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	unsigned sz;
	ssize_t n;

	switch (hdl->wstate) {
	case STATE_IDLE:
		sz = (len < AMSG_DATAMAX) ? len : AMSG_DATAMAX;
		sz -= sz % hdl->wbpf;
		if (sz == 0)
			sz = hdl->wbpf;
		hdl->wstate = STATE_MSG;
		hdl->wtodo = sizeof(struct amsg);
		hdl->wmsg.cmd = AMSG_DATA;
		hdl->wmsg.u.data.size = sz;
		/* PASSTHROUGH */
	case STATE_MSG:
		if (!aucat_wmsg(hdl))
			return 0;
		hdl->wstate = STATE_DATA;
		hdl->wtodo = hdl->wmsg.u.data.size;
		/* PASSTHROUGH */
	case STATE_DATA:
		if (hdl->maxwrite <= 0)
			return 0;
		if (len > hdl->maxwrite)
			len = hdl->maxwrite;
		if (len > hdl->wtodo)
			len = hdl->wtodo;
		if (len == 0) {
			fprintf(stderr, "aucat_write: len == 0\n");
			abort();
		}
		while ((n = write(hdl->fd, buf, len)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				hdl->sa.eof = 1;
				perror("aucat_read: read");
			}
			return 0;
		}
		hdl->maxwrite -= n;
		hdl->wtodo -= n;
		if (hdl->wtodo == 0) {
			hdl->wstate = STATE_IDLE;
			hdl->wtodo = 0xdeadbeef;
		}
		return n;
	default:
		fprintf(stderr, "aucat_read: bad state\n");
		abort();
	}
}

int
aucat_pollfd(struct sa_hdl *sh, struct pollfd *pfd, int events)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	hdl->events = events;
	if (hdl->maxwrite <= 0)
		events &= ~POLLOUT;
	if (hdl->rstate != STATE_DATA)
		events |= POLLIN;
	pfd->fd = hdl->fd;
	pfd->events = events;		
	return 1;
}

int
aucat_revents(struct sa_hdl *sh, struct pollfd *pfd)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	int revents = pfd->revents;

	if (revents & POLLIN) {
		while (hdl->rstate == STATE_MSG) {
			if (!aucat_runmsg(hdl)) {
				revents &= ~POLLIN;
				break;
			}
		}
	}
	if (revents & POLLOUT) {
		if (hdl->maxwrite <= 0)
			revents &= ~POLLOUT;
	}
	return revents & hdl->events;
}
