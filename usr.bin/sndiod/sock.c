/*	$OpenBSD: sock.c,v 1.52 2024/12/20 07:35:56 ratchov Exp $	*/
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "file.h"
#include "midi.h"
#include "opt.h"
#include "sock.h"
#include "utils.h"

#define SOCK_CTLDESC_SIZE	0x800	/* size of s->ctldesc */

void sock_close(struct sock *);
void sock_slot_fill(void *);
void sock_slot_flush(void *);
void sock_slot_eof(void *);
void sock_slot_onmove(void *);
void sock_slot_onvol(void *);
void sock_midi_imsg(void *, unsigned char *, int);
void sock_midi_omsg(void *, unsigned char *, int);
void sock_midi_fill(void *, int);
void sock_ctl_sync(void *);
struct sock *sock_new(int);
void sock_exit(void *);
int sock_fdwrite(struct sock *, void *, int);
int sock_fdread(struct sock *, void *, int);
int sock_rmsg(struct sock *);
int sock_wmsg(struct sock *);
int sock_rdata(struct sock *);
int sock_wdata(struct sock *);
int sock_setpar(struct sock *);
int sock_auth(struct sock *);
int sock_hello(struct sock *);
int sock_execmsg(struct sock *);
int sock_buildmsg(struct sock *);
int sock_read(struct sock *);
int sock_write(struct sock *);
int sock_pollfd(void *, struct pollfd *);
int sock_revents(void *, struct pollfd *);
void sock_in(void *);
void sock_out(void *);
void sock_hup(void *);

struct fileops sock_fileops = {
	"sock",
	sock_pollfd,
	sock_revents,
	sock_in,
	sock_out,
	sock_hup
};

struct slotops sock_slotops = {
	sock_slot_onmove,
	sock_slot_onvol,
	sock_slot_fill,
	sock_slot_flush,
	sock_slot_eof,
	sock_exit
};

struct midiops sock_midiops = {
	sock_midi_imsg,
	sock_midi_omsg,
	sock_midi_fill,
	sock_exit
};

struct ctlops sock_ctlops = {
	sock_exit,
	sock_ctl_sync
};

struct sock *sock_list = NULL;
unsigned int sock_sesrefs = 0;		/* connections to the session */
uint8_t sock_sescookie[AMSG_COOKIELEN];	/* owner of the session */

/*
 * Old clients used to send dev number and opt name. This routine
 * finds proper opt pointer for the given device.
 */
static struct opt *
legacy_opt(int devnum, char *optname)
{
	struct dev *d;
	struct opt *o;

	d = dev_bynum(devnum);
	if (d == NULL)
		return NULL;
	if (strcmp(optname, "default") == 0) {
		for (o = opt_list; o != NULL; o = o->next) {
			if (strcmp(o->name, d->name) == 0)
				return o;
		}
		return NULL;
	} else {
		o = opt_byname(optname);
		return (o != NULL && o->dev == d) ? o : NULL;
	}
}

/*
 * If control slot is associated to a particular opt, then
 * remove the unused group part of the control name to make mixer
 * look nicer
 */
static char *
ctlgroup(struct sock *f, struct ctl *c)
{
	if (f->ctlslot->opt == NULL)
		return c->group;
	if (strcmp(c->group, f->ctlslot->opt->name) == 0)
		return "";
	if (strcmp(c->group, f->ctlslot->opt->dev->name) == 0)
		return "";
	return c->group;
}

void
sock_close(struct sock *f)
{
	struct opt *o;
	struct sock **pf;
	unsigned int tags, i;

	for (pf = &sock_list; *pf != f; pf = &(*pf)->next) {
#ifdef DEBUG
		if (*pf == NULL) {
			logx(0, "%s: not on list", __func__);
			panic();
		}
#endif
	}
	*pf = f->next;

#ifdef DEBUG
	logx(3, "sock %d: closing", f->fd);
#endif
	if (f->pstate > SOCK_AUTH)
		sock_sesrefs -= f->sesrefs;
	if (f->slot) {
		slot_del(f->slot);
		f->slot = NULL;
	}
	if (f->midi) {
		tags = midi_tags(f->midi);
		for (i = 0; i < DEV_NMAX; i++) {
			if ((tags & (1 << i)) && (o = opt_bynum(i)) != NULL)
				opt_unref(o);
		}
		midi_del(f->midi);
		f->midi = NULL;
	}
	if (f->port) {
		port_unref(f->port);
		f->port = NULL;
	}
	if (f->ctlslot) {
		ctlslot_del(f->ctlslot);
		f->ctlslot = NULL;
		xfree(f->ctldesc);
	}
	file_del(f->file);
	close(f->fd);
	file_slowaccept = 0;
	xfree(f);
}

void
sock_slot_fill(void *arg)
{
	struct sock *f = arg;
	struct slot *s = f->slot;

	f->fillpending += s->round;
#ifdef DEBUG
	logx(4, "%s%u: fill, rmax -> %d, pending -> %d",
	    s->name, s->unit, f->rmax, f->fillpending);
#endif
}

void
sock_slot_flush(void *arg)
{
	struct sock *f = arg;
	struct slot *s = f->slot;

	f->wmax += s->round * s->sub.bpf;
#ifdef DEBUG
	logx(4, "%s%u: flush, wmax -> %d", s->name, s->unit, f->wmax);
#endif
}

void
sock_slot_eof(void *arg)
{
	struct sock *f = arg;
#ifdef DEBUG
	struct slot *s = f->slot;

	logx(3, "%s%u: eof", s->name, s->unit);
#endif
	f->stoppending = 1;
}

void
sock_slot_onmove(void *arg)
{
	struct sock *f = (struct sock *)arg;
	struct slot *s = f->slot;

#ifdef DEBUG
	logx(4, "%s%u: onmove: delta -> %d", s->name, s->unit, s->delta);
#endif
	if (s->pstate != SOCK_START)
		return;
	f->tickpending++;
}

void
sock_slot_onvol(void *arg)
{
	struct sock *f = (struct sock *)arg;
	struct slot *s = f->slot;

#ifdef DEBUG
	logx(4, "%s%u: onvol: vol -> %d", s->name, s->unit, s->vol);
#endif
	if (s->pstate != SOCK_START)
		return;
}

void
sock_midi_imsg(void *arg, unsigned char *msg, int size)
{
	struct sock *f = arg;

	midi_send(f->midi, msg, size);
}

void
sock_midi_omsg(void *arg, unsigned char *msg, int size)
{
	struct sock *f = arg;

	midi_out(f->midi, msg, size);
}

void
sock_midi_fill(void *arg, int count)
{
	struct sock *f = arg;

	f->fillpending += count;
}

void
sock_ctl_sync(void *arg)
{
	struct sock *f = arg;

	if (f->ctlops & SOCK_CTLDESC)
		f->ctlsyncpending = 1;
}

struct sock *
sock_new(int fd)
{
	struct sock *f;

	f = xmalloc(sizeof(struct sock));
	f->pstate = SOCK_AUTH;
	f->slot = NULL;
	f->port = NULL;
	f->midi = NULL;
	f->ctlslot = NULL;
	f->tickpending = 0;
	f->fillpending = 0;
	f->stoppending = 0;
	f->wstate = SOCK_WIDLE;
	f->wtodo = 0xdeadbeef;
	f->rstate = SOCK_RMSG;
	f->rtodo = sizeof(struct amsg);
	f->wmax = f->rmax = 0;
	f->lastvol = -1;
	f->ctlops = 0;
	f->ctlsyncpending = 0;
	f->file = file_new(&sock_fileops, f, "sock", 1);
	f->fd = fd;
	if (f->file == NULL) {
		xfree(f);
		return NULL;
	}
	f->next = sock_list;
	sock_list = f;
	return f;
}

void
sock_exit(void *arg)
{
	struct sock *f = (struct sock *)arg;

#ifdef DEBUG
	logx(3, "sock %d: exit", f->fd);
#endif
	sock_close(f);
}

/*
 * write on the socket fd and handle errors
 */
int
sock_fdwrite(struct sock *f, void *data, int count)
{
	int n;

	n = write(f->fd, data, count);
	if (n == -1) {
#ifdef DEBUG
		if (errno == EFAULT) {
			logx(0, "%s: fault", __func__);
			panic();
		}
#endif
		if (errno != EAGAIN) {
			logx(1, "sock %d: write failed, errno = %d", f->fd, errno);
			sock_close(f);
		} else {
#ifdef DEBUG
			logx(4, "sock %d: write blocked", f->fd);
#endif
		}
		return 0;
	}
	if (n == 0) {
		sock_close(f);
		return 0;
	}
	return n;
}

/*
 * read from the socket fd and handle errors
 */
int
sock_fdread(struct sock *f, void *data, int count)
{
	int n;

	n = read(f->fd, data, count);
	if (n == -1) {
#ifdef DEBUG
		if (errno == EFAULT) {
			logx(0, "%s: fault", __func__);
			panic();
		}
#endif
		if (errno != EAGAIN) {
			logx(1, "sock %d: read failed, errno = %d", f->fd, errno);
			sock_close(f);
		} else {
#ifdef DEBUG
			logx(4, "sock %d: read blocked", f->fd);
#endif
		}
		return 0;
	}
	if (n == 0) {
		sock_close(f);
		return 0;
	}
	return n;
}

/*
 * read the next message into f->rmsg, return 1 on success
 */
int
sock_rmsg(struct sock *f)
{
	int n;
	char *data;

#ifdef DEBUG
	if (f->rtodo == 0) {
		logx(0, "%s: sock %d: nothing to read", __func__, f->fd);
		panic();
	}
#endif
	data = (char *)&f->rmsg + sizeof(struct amsg) - f->rtodo;
	n = sock_fdread(f, data, f->rtodo);
	if (n == 0)
		return 0;
	if (n < f->rtodo) {
		f->rtodo -= n;
		return 0;
	}
	f->rtodo = 0;
#ifdef DEBUG
	logx(4, "sock %d: read full message", f->fd);
#endif
	return 1;
}

/*
 * write the message in f->rmsg, return 1 on success
 */
int
sock_wmsg(struct sock *f)
{
	int n;
	char *data;

#ifdef DEBUG
	if (f->wtodo == 0) {
		logx(0, "%s: sock %d: already written", __func__, f->fd);
		/* XXX: this is fatal and we should exit here */
	}
#endif
	data = (char *)&f->wmsg + sizeof(struct amsg) - f->wtodo;
	n = sock_fdwrite(f, data, f->wtodo);
	if (n == 0)
		return 0;
	if (n < f->wtodo) {
		f->wtodo -= n;
		return 0;
	}
	f->wtodo = 0;
#ifdef DEBUG
	logx(4, "sock %d: wrote full message", f->fd);
#endif
	return 1;
}

/*
 * read data into the slot/midi ring buffer
 */
int
sock_rdata(struct sock *f)
{
	unsigned char midibuf[MIDI_BUFSZ];
	unsigned char *data;
	int n, count;

#ifdef DEBUG
	if (f->rtodo == 0) {
		logx(0, "%s: sock %d: data block already read", __func__, f->fd);
		panic();
	}
#endif
	while (f->rtodo > 0) {
		if (f->slot)
			data = abuf_wgetblk(&f->slot->mix.buf, &count);
		else {
			data = midibuf;
			count = MIDI_BUFSZ;
		}
		if (count > f->rtodo)
			count = f->rtodo;
		n = sock_fdread(f, data, count);
		if (n == 0)
			return 0;
		f->rtodo -= n;
		if (f->slot)
			abuf_wcommit(&f->slot->mix.buf, n);
		else
			midi_in(f->midi, midibuf, n);
	}
#ifdef DEBUG
	logx(4, "sock %d: read complete block", f->fd);
#endif
	if (f->slot)
		slot_write(f->slot);
	return 1;
}

/*
 * write data to the slot/midi ring buffer
 */
int
sock_wdata(struct sock *f)
{
	static unsigned char dummy[AMSG_DATAMAX];
	unsigned char *data = NULL;
	int n, count;

#ifdef DEBUG
	if (f->wtodo == 0) {
		logx(0, "%s: sock %d: zero-sized data block", __func__, f->fd);
		panic();
	}
#endif
	if (f->pstate == SOCK_STOP) {
		while (f->wtodo > 0) {
			n = sock_fdwrite(f, dummy, f->wtodo);
			if (n == 0)
				return 0;
			f->wtodo -= n;
		}
#ifdef DEBUG
		logx(4, "sock %d: zero-filled remaining block", f->fd);
#endif
		return 1;
	}
	while (f->wtodo > 0) {
		/*
		 * f->slot and f->midi are set by sock_hello(), so
		 * count is always properly initialized
		 */
		if (f->slot)
			data = abuf_rgetblk(&f->slot->sub.buf, &count);
		else if (f->midi)
			data = abuf_rgetblk(&f->midi->obuf, &count);
		else {
			data = f->ctldesc + (f->wsize - f->wtodo);
			count = f->wtodo;
		}
		if (count > f->wtodo)
			count = f->wtodo;
		n = sock_fdwrite(f, data, count);
		if (n == 0)
			return 0;
		f->wtodo -= n;
		if (f->slot)
			abuf_rdiscard(&f->slot->sub.buf, n);
		else if (f->midi)
			abuf_rdiscard(&f->midi->obuf, n);
	}
	if (f->slot)
		slot_read(f->slot);
	if (f->midi)
		midi_fill(f->midi);
#ifdef DEBUG
	logx(4, "sock %d: wrote complete block", f->fd);
#endif
	return 1;
}

int
sock_setpar(struct sock *f)
{
	struct slot *s = f->slot;
	struct dev *d = s->opt->dev;
	struct amsg_par *p = &f->rmsg.u.par;
	unsigned int min, max;
	uint32_t rate, appbufsz;
	uint16_t pchan, rchan;

	rchan = ntohs(p->rchan);
	pchan = ntohs(p->pchan);
	appbufsz = ntohl(p->appbufsz);
	rate = ntohl(p->rate);

	if (AMSG_ISSET(p->bits)) {
		if (p->bits < BITS_MIN || p->bits > BITS_MAX) {
#ifdef DEBUG
			logx(1, "sock %d: %d: bits out of bounds", f->fd, p->bits);
#endif
			return 0;
		}
		if (AMSG_ISSET(p->bps)) {
			if (p->bps < ((p->bits + 7) / 8) || p->bps > 4) {
#ifdef DEBUG
				logx(1, "sock %d: %d: wrong bytes per sample",
				    f->fd, p->bps);
#endif
				return 0;
			}
		} else
			p->bps = APARAMS_BPS(p->bits);
		s->par.bits = p->bits;
		s->par.bps = p->bps;
	}
	if (AMSG_ISSET(p->sig))
		s->par.sig = p->sig ? 1 : 0;
	if (AMSG_ISSET(p->le))
		s->par.le = p->le ? 1 : 0;
	if (AMSG_ISSET(p->msb))
		s->par.msb = p->msb ? 1 : 0;
	if (AMSG_ISSET(rchan) && (s->mode & MODE_RECMASK)) {
		if (rchan < 1)
			rchan = 1;
		else if (rchan > NCHAN_MAX)
			rchan = NCHAN_MAX;
		s->sub.nch = rchan;
	}
	if (AMSG_ISSET(pchan) && (s->mode & MODE_PLAY)) {
		if (pchan < 1)
			pchan = 1;
		else if (pchan > NCHAN_MAX)
			pchan = NCHAN_MAX;
		s->mix.nch = pchan;
	}
	if (AMSG_ISSET(rate)) {
		if (rate < RATE_MIN)
			rate = RATE_MIN;
		else if (rate > RATE_MAX)
			rate = RATE_MAX;
		s->round = dev_roundof(d, rate);
		s->rate = rate;
		if (!AMSG_ISSET(appbufsz))
			appbufsz = d->bufsz / d->round * s->round;
	}
	if (AMSG_ISSET(p->xrun)) {
		if (p->xrun != XRUN_IGNORE &&
		    p->xrun != XRUN_SYNC &&
		    p->xrun != XRUN_ERROR) {
#ifdef DEBUG
			logx(1, "sock %d: %u: bad xrun policy", f->fd, p->xrun);
#endif
			return 0;
		}
		s->xrun = p->xrun;
		if (s->opt->mtc != NULL && s->xrun == XRUN_IGNORE)
			s->xrun = XRUN_SYNC;
	}
	if (AMSG_ISSET(appbufsz)) {
		rate = s->rate;
		min = 1;
		max = 1 + rate / d->round;
		min *= s->round;
		max *= s->round;
		appbufsz += s->round / 2;
		appbufsz -= appbufsz % s->round;
		if (appbufsz < min)
			appbufsz = min;
		if (appbufsz > max)
			appbufsz = max;
		s->appbufsz = appbufsz;
	}
	return 1;
}

int
sock_auth(struct sock *f)
{
	struct amsg_auth *p = &f->rmsg.u.auth;
	uid_t euid;
	gid_t egid;

	/*
	 * root bypasses any authentication checks and has no session
	 */
	if (getpeereid(f->fd, &euid, &egid) == 0 && euid == 0) {
		f->pstate = SOCK_HELLO;
		f->sesrefs = 0;
		return 1;
	}

	if (sock_sesrefs == 0) {
		/* start a new session */
		memcpy(sock_sescookie, p->cookie, AMSG_COOKIELEN);
		f->sesrefs = 1;
	} else if (memcmp(sock_sescookie, p->cookie, AMSG_COOKIELEN) != 0) {
		/* another session is active, drop connection */
		return 0;
	}
	sock_sesrefs += f->sesrefs;
	f->pstate = SOCK_HELLO;
	return 1;
}

int
sock_hello(struct sock *f)
{
	struct amsg_hello *p = &f->rmsg.u.hello;
	struct port *c;
	struct opt *opt;
	unsigned int mode;
	unsigned int id;

	mode = ntohs(p->mode);
	id = ntohl(p->id);
#ifdef DEBUG
	logx(3, "sock %d: hello from <%s>, mode %x, ver %d",
	    f->fd, p->who, mode, p->version);
#endif
	if (p->version != AMSG_VERSION) {
		logx(1, "sock %d: %u: unsupported version", f->fd, p->version);
		return 0;
	}
	switch (mode) {
	case MODE_MIDIIN:
	case MODE_MIDIOUT:
	case MODE_MIDIOUT | MODE_MIDIIN:
	case MODE_REC:
	case MODE_PLAY:
	case MODE_PLAY | MODE_REC:
	case MODE_CTLREAD:
	case MODE_CTLWRITE:
	case MODE_CTLREAD | MODE_CTLWRITE:
		break;
	default:
#ifdef DEBUG
		logx(1, "sock %d: %u: unsupported mode", f->fd, mode);
#endif
		return 0;
	}
	f->pstate = SOCK_INIT;
	f->port = NULL;
	if (mode & MODE_MIDIMASK) {
		f->slot = NULL;
		f->midi = midi_new(&sock_midiops, f, mode);
		if (f->midi == NULL)
			return 0;
		/* XXX: add 'devtype' to libsndio */
		if (p->devnum == AMSG_NODEV) {
			opt = opt_byname(p->opt);
			if (opt == NULL)
				return 0;
			if (!opt_ref(opt))
				return 0;
			midi_tag(f->midi, opt->num);
		} else if (p->devnum < 16) {
			opt = legacy_opt(p->devnum, p->opt);
			if (opt == NULL)
				return 0;
			if (!opt_ref(opt))
				return 0;
			midi_tag(f->midi, opt->num);
		} else if (p->devnum < 32) {
			midi_tag(f->midi, p->devnum);
		} else if (p->devnum < 48) {
			c = port_alt_ref(p->devnum - 32);
			if (c == NULL)
				return 0;
			f->port = c;
			midi_link(f->midi, c->midi);
		} else
			return 0;
		return 1;
	}
	if (mode & MODE_CTLMASK) {
		if (p->devnum == AMSG_NODEV) {
			opt = opt_byname(p->opt);
			if (opt == NULL)
				return 0;
		} else {
			opt = legacy_opt(p->devnum, p->opt);
			if (opt == NULL)
				return 0;
		}
		f->ctlslot = ctlslot_new(opt, &sock_ctlops, f);
		if (f->ctlslot == NULL) {
			logx(2, "sock %d: couldn't get ctlslot", f->fd);
			return 0;
		}
		f->ctldesc = xmalloc(SOCK_CTLDESC_SIZE);
		f->ctlops = 0;
		f->ctlsyncpending = 0;
		return 1;
	}
	opt = (p->devnum == AMSG_NODEV) ?
	    opt_byname(p->opt) : legacy_opt(p->devnum, p->opt);
	if (opt == NULL)
		return 0;
	f->slot = slot_new(opt, id, p->who, &sock_slotops, f, mode);
	if (f->slot == NULL)
		return 0;
	f->midi = NULL;
	return 1;
}

/*
 * execute the message in f->rmsg, return 1 on success
 */
int
sock_execmsg(struct sock *f)
{
	struct ctl *c;
	struct slot *s = f->slot;
	struct amsg *m = &f->rmsg;
	unsigned char *data;
	unsigned int size, ctl;
	int cmd;

	cmd = ntohl(m->cmd);
	switch (cmd) {
	case AMSG_DATA:
#ifdef DEBUG
		logx(4, "sock %d: DATA message", f->fd);
#endif
		if (s != NULL && f->pstate != SOCK_START) {
#ifdef DEBUG
			logx(1, "sock %d: DATA, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if ((f->slot && !(f->slot->mode & MODE_PLAY)) ||
		    (f->midi && !(f->midi->mode & MODE_MIDIOUT))) {
#ifdef DEBUG
			logx(1, "sock %d: DATA, input-only mode", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		size = ntohl(m->u.data.size);
		if (size == 0) {
#ifdef DEBUG
			logx(1, "sock %d: zero size payload", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (s != NULL && size % s->mix.bpf != 0) {
#ifdef DEBUG
			logx(1, "sock %d: not aligned to frame", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (s != NULL && size > f->ralign) {
#ifdef DEBUG
			logx(1, "sock %d: size = %d, ralign = %d: "
			   "not aligned to block", f->fd, size, f->ralign);
#endif
			sock_close(f);
			return 0;
		}
		f->rstate = SOCK_RDATA;
		f->rsize = f->rtodo = size;
		if (s != NULL) {
			f->ralign -= size;
			if (f->ralign == 0)
				f->ralign = s->round * s->mix.bpf;
		}
		if (f->rtodo > f->rmax) {
#ifdef DEBUG
			logx(1, "sock %d: unexpected data, size = %u, rmax = %d",
			    f->fd, size, f->rmax);
#endif
			sock_close(f);
			return 0;
		}
		f->rmax -= f->rtodo;
		if (f->rtodo == 0) {
#ifdef DEBUG
			logx(1, "sock %d: zero-length data chunk", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		break;
	case AMSG_START:
#ifdef DEBUG
		logx(3, "sock %d: START message", f->fd);
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: START, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		f->tickpending = 0;
		f->stoppending = 0;
		slot_start(s);
		if (s->mode & MODE_PLAY) {
			f->fillpending = s->appbufsz;
			f->ralign = s->round * s->mix.bpf;
			f->rmax = 0;
		}
		if (s->mode & MODE_RECMASK) {
			f->walign = s->round * s->sub.bpf;
			f->wmax = 0;
		}
		f->pstate = SOCK_START;
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_STOP:
#ifdef DEBUG
		logx(3, "sock %d: STOP message", f->fd);
#endif
		if (f->pstate != SOCK_START) {
#ifdef DEBUG
			logx(1, "sock %d: STOP, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		f->rmax = 0;
		if (!(s->mode & MODE_PLAY))
			f->stoppending = 1;
		f->pstate = SOCK_STOP;
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		if (s->mode & MODE_PLAY) {
			if (f->ralign < s->round * s->mix.bpf) {
				data = abuf_wgetblk(&s->mix.buf, &size);
#ifdef DEBUG
				if (size < f->ralign) {
					logx(0, "sock %d: unaligned stop, "
					    "size = %u, ralign = %u",
					    f->fd, size, f->ralign);
					panic();
				}
#endif
				memset(data, 0, f->ralign);
				abuf_wcommit(&s->mix.buf, f->ralign);
				f->ralign = s->round * s->mix.bpf;
			}
		}
		slot_stop(s, AMSG_ISSET(m->u.stop.drain) ? m->u.stop.drain : 1);
		break;
	case AMSG_SETPAR:
#ifdef DEBUG
		logx(3, "sock %d: SETPAR message", f->fd);
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: SETPAR, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (!sock_setpar(f)) {
			sock_close(f);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_GETPAR:
#ifdef DEBUG
		logx(3, "sock %d: GETPAR message", f->fd);
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: GETPAR, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = htonl(AMSG_GETPAR);
		m->u.par.legacy_mode = s->mode;
		m->u.par.xrun = s->xrun;
		m->u.par.bits = s->par.bits;
		m->u.par.bps = s->par.bps;
		m->u.par.sig = s->par.sig;
		m->u.par.le = s->par.le;
		m->u.par.msb = s->par.msb;
		if (s->mode & MODE_PLAY)
			m->u.par.pchan = htons(s->mix.nch);
		if (s->mode & MODE_RECMASK)
			m->u.par.rchan = htons(s->sub.nch);
		m->u.par.rate = htonl(s->rate);
		m->u.par.appbufsz = htonl(s->appbufsz);
		m->u.par.bufsz = htonl(SLOT_BUFSZ(s));
		m->u.par.round = htonl(s->round);
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETVOL:
#ifdef DEBUG
		logx(3, "sock %d: SETVOL message", f->fd);
#endif
		if (f->pstate < SOCK_INIT || s == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: SETVOL, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		ctl = ntohl(m->u.vol.ctl);
		if (ctl > MIDI_MAXCTL) {
#ifdef DEBUG
			logx(1, "sock %d: SETVOL, volume out of range", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		f->lastvol = ctl; /* dont trigger feedback message */
		slot_setvol(s, ctl);
		dev_midi_vol(s->opt->dev, s);
		ctl_onval(CTL_SLOT_LEVEL, s, NULL, ctl);
		break;
	case AMSG_CTLSUB_OLD:
	case AMSG_CTLSUB:
#ifdef DEBUG
		logx(3, "sock %d: CTLSUB message, desc = 0x%x, val = 0x%x",
		    f->fd, m->u.ctlsub.desc, m->u.ctlsub.val);
#endif
		if (f->pstate != SOCK_INIT || f->ctlslot == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: CTLSUB, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (m->u.ctlsub.desc) {
			if (!(f->ctlops & SOCK_CTLDESC)) {
				ctl = f->ctlslot->self;
				c = ctl_list;
				while (c != NULL) {
					if (ctlslot_visible(f->ctlslot, c))
						c->desc_mask |= ctl;
					c = c->next;
				}
				f->ctlops |= SOCK_CTLDESC;
				f->ctlsyncpending = 1;
				f->ctl_desc_size = (cmd == AMSG_CTLSUB) ?
				    sizeof(struct amsg_ctl_desc) :
				    AMSG_OLD_DESC_SIZE;
			}
		} else
			f->ctlops &= ~SOCK_CTLDESC;
		if (m->u.ctlsub.val) {
			f->ctlops |= SOCK_CTLVAL;
		} else
			f->ctlops &= ~SOCK_CTLVAL;
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_CTLSET:
#ifdef DEBUG
		logx(3, "sock %d: CTLSET message", f->fd);
#endif
		if (f->pstate < SOCK_INIT || f->ctlslot == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: CTLSET, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}

		c = ctlslot_lookup(f->ctlslot, ntohs(m->u.ctlset.addr));
		if (c == NULL) {
#ifdef DEBUG
			logx(1, "sock %d: CTLSET, wrong addr", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (!ctl_setval(c, ntohs(m->u.ctlset.val))) {
#ifdef DEBUG
			logx(1, "sock %d: CTLSET, bad value", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_AUTH:
#ifdef DEBUG
		logx(3, "sock %d: AUTH message", f->fd);
#endif
		if (f->pstate != SOCK_AUTH) {
#ifdef DEBUG
			logx(1, "sock %d: AUTH, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (!sock_auth(f)) {
			sock_close(f);
			return 0;
		}
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_HELLO:
#ifdef DEBUG
		logx(3, "sock %d: HELLO message", f->fd);
#endif
		if (f->pstate != SOCK_HELLO) {
#ifdef DEBUG
			logx(1, "sock %d: HELLO, wrong state", f->fd);
#endif
			sock_close(f);
			return 0;
		}
		if (!sock_hello(f)) {
			sock_close(f);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = htonl(AMSG_ACK);
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_BYE:
#ifdef DEBUG
		logx(3, "sock %d: BYE message", f->fd);
#endif
		if (s != NULL && f->pstate != SOCK_INIT) {
#ifdef DEBUG
			logx(1, "sock %d: BYE, wrong state", f->fd);
#endif
		}
		sock_close(f);
		return 0;
	default:
#ifdef DEBUG
		logx(1, "sock %d: unknown command in message", f->fd);
#endif
		sock_close(f);
		return 0;
	}
	return 1;
}

/*
 * build a message in f->wmsg, return 1 on success and 0 if
 * there's nothing to do. Assume f->wstate is SOCK_WIDLE
 */
int
sock_buildmsg(struct sock *f)
{
	unsigned int size, type, mask;
	struct amsg_ctl_desc *desc;
	struct ctl *c, **pc;

	/*
	 * If pos changed (or initial tick), build a MOVE message.
	 */
	if (f->tickpending) {
#ifdef DEBUG
		logx(4, "sock %d: building MOVE message, delta = %d", f->fd, f->slot->delta);
#endif
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_MOVE);
		f->wmsg.u.ts.delta = htonl(f->slot->delta);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->tickpending = 0;
		/*
		 * XXX: use tickpending as accumulator rather than
		 * slot->delta
		 */
		f->slot->delta = 0;
		return 1;
	}

	if (f->fillpending > 0) {
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_FLOWCTL);
		f->wmsg.u.ts.delta = htonl(f->fillpending);
		size = f->fillpending;
		if (f->slot)
			size *= f->slot->mix.bpf;
		f->rmax += size;
#ifdef DEBUG
		logx(4, "sock %d: building FLOWCTL message, "
		    "count = %d, rmax -> %d", f->fd, f->fillpending, f->rmax);
#endif
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->fillpending = 0;
		return 1;
	}

	/*
	 * if volume changed build a SETVOL message
	 */
	if (f->pstate >= SOCK_START && f->slot->vol != f->lastvol) {
#ifdef DEBUG
		logx(3, "sock %d: building SETVOL message, vol = %d", f->fd,
		    f->slot->vol);
#endif
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_SETVOL);
		f->wmsg.u.vol.ctl = htonl(f->slot->vol);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->lastvol = f->slot->vol;
		return 1;
	}

	if (f->midi != NULL && f->midi->obuf.used > 0) {
		size = f->midi->obuf.used;
		if (size > AMSG_DATAMAX)
			size = AMSG_DATAMAX;
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_DATA);
		f->wmsg.u.data.size = htonl(size);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}

	/*
	 * If data available, build a DATA message.
	 */
	if (f->slot != NULL && f->wmax > 0 && f->slot->sub.buf.used > 0) {
		size = f->slot->sub.buf.used;
		if (size > AMSG_DATAMAX)
			size = AMSG_DATAMAX;
		if (size > f->walign)
			size = f->walign;
		if (size > f->wmax)
			size = f->wmax;
		size -= size % f->slot->sub.bpf;
#ifdef DEBUG
		if (size == 0) {
			logx(0, "sock %d: sock_buildmsg size == 0", f->fd);
			panic();
		}
#endif
		f->walign -= size;
		f->wmax -= size;
		if (f->walign == 0)
			f->walign = f->slot->round * f->slot->sub.bpf;
#ifdef DEBUG
		logx(4, "sock %d: building audio DATA message, size = %d", f->fd, size);
#endif
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_DATA);
		f->wmsg.u.data.size = htonl(size);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}

	if (f->stoppending) {
#ifdef DEBUG
		logx(3, "sock %d: building STOP message", f->fd);
#endif
		f->stoppending = 0;
		f->pstate = SOCK_INIT;
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_STOP);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}

	/*
	 * XXX: add a flag indicating if there are changes
	 * in controls not seen by this client, rather
	 * than walking through the full list of control
	 * searching for the {desc,val}_mask bits
	 */
	if (f->ctlslot && (f->ctlops & SOCK_CTLDESC)) {
		mask = f->ctlslot->self;
		size = 0;
		pc = &ctl_list;
		while ((c = *pc) != NULL) {
			if ((c->desc_mask & mask) == 0 ||
			    (c->refs_mask & mask) == 0) {
				pc = &c->next;
				continue;
			}
			if (size + f->ctl_desc_size > SOCK_CTLDESC_SIZE)
				break;
			desc = (struct amsg_ctl_desc *)(f->ctldesc + size);
			c->desc_mask &= ~mask;
			c->val_mask &= ~mask;
			type = ctlslot_visible(f->ctlslot, c) ?
			    c->type : CTL_NONE;
			strlcpy(desc->group, ctlgroup(f, c), AMSG_CTL_NAMEMAX);
			strlcpy(desc->node0.name, c->node0.name,
			    AMSG_CTL_NAMEMAX);
			desc->node0.unit = ntohs(c->node0.unit);
			strlcpy(desc->node1.name, c->node1.name,
			    AMSG_CTL_NAMEMAX);
			desc->node1.unit = ntohs(c->node1.unit);
			desc->type = type;
			strlcpy(desc->func, c->func, AMSG_CTL_NAMEMAX);
			desc->addr = htons(c->addr);
			desc->maxval = htons(c->maxval);
			desc->curval = htons(c->curval);

			/* old clients don't have the 'display' member */
			if (f->ctl_desc_size >= offsetof(struct amsg_ctl_desc,
				display) + AMSG_CTL_DISPLAYMAX) {
				strlcpy(desc->display, c->display, AMSG_CTL_DISPLAYMAX);
			}

			size += f->ctl_desc_size;

			/* if this is a deleted entry unref it */
			if (type == CTL_NONE) {
				c->refs_mask &= ~mask;
				if (c->refs_mask == 0) {
					*pc = c->next;
					xfree(c);
					continue;
				}
			}

			pc = &c->next;
		}
		if (size > 0) {
			AMSG_INIT(&f->wmsg);
			f->wmsg.cmd = htonl(AMSG_DATA);
			f->wmsg.u.data.size = htonl(size);
			f->wtodo = sizeof(struct amsg);
			f->wstate = SOCK_WMSG;
#ifdef DEBUG
			logx(3, "sock %d: building control DATA message", f->fd);
#endif
			return 1;
		}
	}
	if (f->ctlslot && (f->ctlops & SOCK_CTLVAL)) {
		mask = f->ctlslot->self;
		for (c = ctl_list; c != NULL; c = c->next) {
			if (!ctlslot_visible(f->ctlslot, c))
				continue;
			if ((c->val_mask & mask) == 0)
				continue;
			c->val_mask &= ~mask;
			AMSG_INIT(&f->wmsg);
			f->wmsg.cmd = htonl(AMSG_CTLSET);
			f->wmsg.u.ctlset.addr = htons(c->addr);
			f->wmsg.u.ctlset.val = htons(c->curval);
			f->wtodo = sizeof(struct amsg);
			f->wstate = SOCK_WMSG;
#ifdef DEBUG
			logx(3, "sock %d: building CTLSET message", f->fd);
#endif
			return 1;
		}
	}
	if (f->ctlslot && f->ctlsyncpending) {
		f->ctlsyncpending = 0;
		f->wmsg.cmd = htonl(AMSG_CTLSYNC);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
#ifdef DEBUG
		logx(3, "sock %d: building CTLSYNC message", f->fd);
#endif
		return 1;
	}
#ifdef DEBUG
	logx(4, "sock %d: no messages to build anymore, idling...", f->fd);
#endif
	f->wstate = SOCK_WIDLE;
	return 0;
}

/*
 * iteration of the socket reader loop, return 1 on success
 */
int
sock_read(struct sock *f)
{
#ifdef DEBUG
	logx(4, "sock %d: reading %u todo", f->fd, f->rtodo);
#endif
	switch (f->rstate) {
	case SOCK_RIDLE:
		return 0;
	case SOCK_RMSG:
		if (!sock_rmsg(f))
			return 0;
		if (!sock_execmsg(f))
			return 0;
		break;
	case SOCK_RDATA:
		if (!sock_rdata(f))
			return 0;
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case SOCK_RRET:
		if (f->wstate != SOCK_WIDLE) {
#ifdef DEBUG
			logx(4, "sock %d: can't reply, write-end blocked", f->fd);
#endif
			return 0;
		}
		f->wmsg = f->rmsg;
		f->wstate = SOCK_WMSG;
		f->wtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
#ifdef DEBUG
		logx(4, "sock %d: copied RRET message", f->fd);
#endif
	}
	return 1;
}

/*
 * iteration of the socket writer loop, return 1 on success
 */
int
sock_write(struct sock *f)
{
#ifdef DEBUG
	logx(4, "sock %d: writing", f->fd);
#endif
	switch (f->wstate) {
	case SOCK_WMSG:
		if (!sock_wmsg(f))
			return 0;
		/*
		 * f->wmsg is either build by sock_buildmsg() or
		 * copied from f->rmsg (in the SOCK_RRET state), so
		 * it's safe.
		 */
		if (ntohl(f->wmsg.cmd) != AMSG_DATA) {
			f->wstate = SOCK_WIDLE;
			f->wtodo = 0xdeadbeef;
			break;
		}
		f->wstate = SOCK_WDATA;
		f->wsize = f->wtodo = ntohl(f->wmsg.u.data.size);
		/* FALLTHROUGH */
	case SOCK_WDATA:
		if (!sock_wdata(f))
			return 0;
		if (f->wtodo > 0)
			break;
		f->wstate = SOCK_WIDLE;
		f->wtodo = 0xdeadbeef;
		if (f->pstate == SOCK_STOP) {
			f->pstate = SOCK_INIT;
			f->wmax = 0;
#ifdef DEBUG
			logx(4, "sock %d: drained, moved to INIT state", f->fd);
#endif
		}
		/* FALLTHROUGH */
	case SOCK_WIDLE:
		if (f->rstate == SOCK_RRET) {
			f->wmsg = f->rmsg;
			f->wstate = SOCK_WMSG;
			f->wtodo = sizeof(struct amsg);
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
#ifdef DEBUG
			logx(4, "sock %d: copied RRET message", f->fd);
#endif
		} else {
			if (!sock_buildmsg(f))
				return 0;
		}
		break;
#ifdef DEBUG
	default:
		logx(0, "sock %d: bad writing end state", f->fd);
		panic();
#endif
	}
	return 1;
}

int
sock_pollfd(void *arg, struct pollfd *pfd)
{
	struct sock *f = arg;
	int events = 0;

	/*
	 * feedback counters, clock ticks and alike may have changed,
	 * prepare a message to trigger writes
	 *
	 * XXX: doing this at the beginning of the cycle is not optimal,
	 * because state is changed at the end of the read cycle, and
	 * thus counters, ret message and alike are generated then.
	 */
	if (f->wstate == SOCK_WIDLE && f->rstate != SOCK_RRET)
		sock_buildmsg(f);

	if (f->rstate == SOCK_RMSG ||
	    f->rstate == SOCK_RDATA)
		events |= POLLIN;
	if (f->rstate == SOCK_RRET ||
	    f->wstate == SOCK_WMSG ||
	    f->wstate == SOCK_WDATA)
		events |= POLLOUT;
	pfd->fd = f->fd;
	pfd->events = events;
	return 1;
}

int
sock_revents(void *arg, struct pollfd *pfd)
{
	return pfd->revents;
}

void
sock_in(void *arg)
{
	struct sock *f = arg;

	while (sock_read(f))
		;
}

void
sock_out(void *arg)
{
	struct sock *f = arg;

	while (sock_write(f))
		;
}

void
sock_hup(void *arg)
{
	struct sock *f = arg;

	sock_close(f);
}
