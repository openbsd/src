/*	$OpenBSD: sock.c,v 1.15 2015/02/16 06:35:17 ratchov Exp $	*/
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

void sock_log(struct sock *);
void sock_close(struct sock *);
void sock_slot_fill(void *);
void sock_slot_flush(void *);
void sock_slot_eof(void *);
void sock_slot_onmove(void *);
void sock_slot_onvol(void *, unsigned int);
void sock_midi_imsg(void *, unsigned char *, int);
void sock_midi_omsg(void *, unsigned char *, int);
void sock_midi_fill(void *, int);
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

struct sock *sock_list = NULL;
unsigned int sock_sesrefs = 0;		/* connections to the session */
uint8_t sock_sescookie[AMSG_COOKIELEN];	/* owner of the session */

void
sock_log(struct sock *f)
{
#ifdef DEBUG
	static char *rstates[] = { "ridl", "rmsg", "rdat", "rret" };
	static char *wstates[] = { "widl", "wmsg", "wdat" };
#endif
	if (f->slot)
		slot_log(f->slot);
	else if (f->midi)
		midi_log(f->midi);
	else
		log_puts("sock");
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts(",");
		log_puts(rstates[f->rstate]);
		log_puts(",");
		log_puts(wstates[f->wstate]);
	}
#endif
}

void
sock_close(struct sock *f)
{
	struct sock **pf;

	for (pf = &sock_list; *pf != f; pf = &(*pf)->next) {
#ifdef DEBUG
		if (*pf == NULL) {
			log_puts("sock_close: not on list\n");
			panic();
		}
#endif
	}
	*pf = f->next;

#ifdef DEBUG
	if (log_level >= 3) {
		sock_log(f);
		log_puts(": closing\n");
	}
#endif
	if (f->pstate > SOCK_AUTH)
		sock_sesrefs--;
	if (f->slot) {
		slot_del(f->slot);
		f->slot = NULL;
	}
	if (f->midi) {
		midi_del(f->midi);
		f->midi = NULL;
	}
	if (f->port) {
		port_unref(f->port);
		f->port = NULL;
	}
	file_del(f->file);
	close(f->fd);
	xfree(f);
}

void
sock_slot_fill(void *arg)
{
	struct sock *f = arg;
	struct slot *s = f->slot;

	f->fillpending += s->round;
#ifdef DEBUG
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": fill, rmax -> ");
		log_puti(f->rmax);
		log_puts(", pending -> ");
		log_puti(f->fillpending);
		log_puts("\n");
	}
#endif
}

void
sock_slot_flush(void *arg)
{
	struct sock *f = arg;
	struct slot *s = f->slot;

	f->wmax += s->round * s->sub.bpf;
#ifdef DEBUG
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": flush, wmax -> ");
		log_puti(f->wmax);
		log_puts("\n");
	}
#endif
}

void
sock_slot_eof(void *arg)
{
	struct sock *f = arg;

#ifdef DEBUG
	if (log_level >= 3) {
		sock_log(f);
		log_puts(": stopped\n");
	}
#endif
	f->stoppending = 1;
}

void
sock_slot_onmove(void *arg)
{
	struct sock *f = (struct sock *)arg;
	struct slot *s = f->slot;

#ifdef DEBUG
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": onmove: delta -> ");
		log_puti(s->delta);
		log_puts("\n");
	}
#endif
	if (s->pstate != SOCK_START)
		return;
	f->tickpending++;
}

void
sock_slot_onvol(void *arg, unsigned int delta)
{
	struct sock *f = (struct sock *)arg;
	struct slot *s = f->slot;

#ifdef DEBUG
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": onvol: vol -> ");
		log_puti(s->vol);
		log_puts("\n");
	}
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

struct sock *
sock_new(int fd)
{
	struct sock *f;

	f = xmalloc(sizeof(struct sock));
	f->pstate = SOCK_AUTH;
	f->opt = NULL;
	f->slot = NULL;
	f->port = NULL;
	f->midi = NULL;
	f->tickpending = 0;
	f->fillpending = 0;
	f->stoppending = 0;
	f->wstate = SOCK_WIDLE;
	f->wtodo = 0xdeadbeef;
	f->rstate = SOCK_RMSG;
	f->rtodo = sizeof(struct amsg);
	f->wmax = f->rmax = 0;
	f->lastvol = -1;
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
	if (log_level >= 3) {
		sock_log(f);
		log_puts(": exit\n");
	}
#endif
	sock_close(f);
}

/*
 * write on the socke fd and handle errors
 */
int
sock_fdwrite(struct sock *f, void *data, int count)
{
	int n;

	n = write(f->fd, data, count);
	if (n < 0) {
#ifdef DEBUG
		if (errno == EFAULT) {
			log_puts("sock_fdwrite: fault\n");
			panic();
		}
#endif
		if (errno != EAGAIN) {
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": write filed, errno = ");
				log_puti(errno);
				log_puts("\n");
			}
			sock_close(f);
		} else {
#ifdef DEBUG			
			if (log_level >= 4) {
				sock_log(f);
				log_puts(": write blocked\n");
			}
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
 * read from the socke fd and handle errors
 */
int
sock_fdread(struct sock *f, void *data, int count)
{
	int n;

	n = read(f->fd, data, count);
	if (n < 0) {
#ifdef DEBUG
		if (errno == EFAULT) {
			log_puts("sock_fdread: fault\n");
			panic();
		}
#endif
		if (errno != EAGAIN) {
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": read failed, errno = ");
				log_puti(errno);
				log_puts("\n");
			}
			sock_close(f);
		} else {
#ifdef DEBUG			
			if (log_level >= 4) {
				sock_log(f);
				log_puts(": read blocked\n");
			}
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
		sock_log(f);
		log_puts(": sock_rmsg: nothing to read\n");
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": read full message\n");
	}
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
		sock_log(f);
		log_puts(": sock_wmsg: already written\n");
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": wrote full message\n");
	}
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
		sock_log(f);
		log_puts(": data block already read\n");
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": read complete block\n");
	}
#endif
	if (f->slot)
		slot_write(f->slot);
	return 1;
}

/*
 * read data into the slot/midi ring buffer
 */
int
sock_wdata(struct sock *f)
{
	static unsigned char dummy[AMSG_DATAMAX];
	unsigned char *data = NULL;
	int n, count;

#ifdef DEBUG
	if (f->wtodo == 0) {
		sock_log(f);
		log_puts(": attempted to write zero-sized data block\n");
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
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": zero-filled remaining block\n");
		}
#endif
		return 1;
	}
	while (f->wtodo > 0) {
		if (f->slot)
			data = abuf_rgetblk(&f->slot->sub.buf, &count);
		else if (f->midi)
			data = abuf_rgetblk(&f->midi->obuf, &count);
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": wrote complete block\n");
	}
#endif
	return 1;
}

int
sock_setpar(struct sock *f)
{
	struct slot *s = f->slot;
	struct dev *d = s->dev;
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
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": ");
				log_putu(p->bits);
				log_puts(": bits out of bounds\n");
			}
#endif
			return 0;
		}
		if (AMSG_ISSET(p->bps)) {
			if (p->bps < ((p->bits + 7) / 8) || p->bps > 4) {
#ifdef DEBUG
				if (log_level >= 1) {
					sock_log(f);
					log_puts(": ");
					log_putu(p->bps);
					log_puts(": wrong bytes per sample\n");
				}
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
		if (rchan > NCHAN_MAX)
			rchan = NCHAN_MAX;
		s->sub.slot_cmin = f->opt->rmin;
		s->sub.slot_cmax = f->opt->rmin + rchan - 1;
		s->sub.dev_cmin = f->opt->rmin;
		s->sub.dev_cmax = f->opt->rmax;
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": recording channels ");
			log_putu(s->sub.dev_cmin);
			log_puts(":");
			log_putu(s->sub.dev_cmax);
			log_puts(" -> ");
			log_putu(s->sub.slot_cmin);
			log_puts(":");
			log_putu(s->sub.slot_cmax);
			log_puts("\n");
		}
#endif
	}
	if (AMSG_ISSET(pchan) && (s->mode & MODE_PLAY)) {
		if (pchan < 1)
			pchan = 1;
		if (pchan > NCHAN_MAX)
			pchan = NCHAN_MAX;
		s->mix.slot_cmin = f->opt->pmin;
		s->mix.slot_cmax = f->opt->pmin + pchan - 1;
		s->mix.dev_cmin = f->opt->pmin;
		s->mix.dev_cmax = f->opt->pmax;
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": playback channels ");
			log_putu(s->mix.slot_cmin);
			log_puts(":");
			log_putu(s->mix.slot_cmax);
			log_puts(" -> ");
			log_putu(s->mix.dev_cmin);
			log_puts(":");
			log_putu(s->mix.dev_cmax);
			log_puts("\n");
		}
#endif
	}
	if (AMSG_ISSET(rate)) {
		if (rate < RATE_MIN)
			rate = RATE_MIN;
		if (rate > RATE_MAX)
			rate = RATE_MAX;
		s->round = dev_roundof(d, rate);
		s->rate = rate;
		if (!AMSG_ISSET(appbufsz)) {
			appbufsz = d->bufsz / d->round * s->round;
#ifdef DEBUG
			if (log_level >= 3) {
				sock_log(f);
				log_puts(": ");
				log_putu(appbufsz);
				log_puts(" frame buffer\n");
			}
#endif
		}
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": ");
			log_putu(rate);
			log_puts("Hz sample rate, ");
			log_putu(s->round);
			log_puts(" frame blocks\n");
		}
#endif
	}
	if (AMSG_ISSET(p->xrun)) {
		if (p->xrun != XRUN_IGNORE &&
		    p->xrun != XRUN_SYNC &&
		    p->xrun != XRUN_ERROR) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": ");
				log_putx(p->xrun);
				log_puts(": bad xrun policy\n");
			}
#endif
			return 0;
		}
		s->xrun = p->xrun;
		if (f->opt->mmc && s->xrun == XRUN_IGNORE)
			s->xrun = XRUN_SYNC;
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": 0x");
			log_putx(s->xrun);
			log_puts(" xrun policy\n");
		}
#endif
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
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": ");
			log_putu(s->appbufsz);
			log_puts(" frame buffer\n");
		}
#endif
	}
	return 1;
}

int
sock_auth(struct sock *f)
{
	struct amsg_auth *p = &f->rmsg.u.auth;

	if (sock_sesrefs == 0) {
		/* start a new session */
		memcpy(sock_sescookie, p->cookie, AMSG_COOKIELEN);
	} else if (memcmp(sock_sescookie, p->cookie, AMSG_COOKIELEN) != 0) {
		/* another session is active, drop connection */
		return 0;
	}
	sock_sesrefs++;
	f->pstate = SOCK_HELLO;
	return 1;
}

int
sock_hello(struct sock *f)
{
	struct amsg_hello *p = &f->rmsg.u.hello;
	struct slot *s;
	struct port *c;
	struct dev *d;
	unsigned int mode;

	mode = ntohs(p->mode);
#ifdef DEBUG
	if (log_level >= 3) {
		sock_log(f);
		log_puts(": hello from <");
		log_puts(p->who);
		log_puts(">, mode = ");
		log_putx(mode);
		log_puts(", ver ");
		log_putu(p->version);
		log_puts("\n");
	}
#endif
	if (p->version != AMSG_VERSION) {
		if (log_level >= 1) {
			sock_log(f);
			log_puts(": ");
			log_putu(p->version);
			log_puts(": unsupported protocol version\n");
		}
		return 0;
	}
	switch (mode) {
	case MODE_MIDIIN:
	case MODE_MIDIOUT:
	case MODE_MIDIOUT | MODE_MIDIIN:
	case MODE_REC:
	case MODE_PLAY:
	case MODE_PLAY | MODE_REC:
		break;
	default:
#ifdef DEBUG
		if (log_level >= 1) {
			sock_log(f);
			log_puts(": ");
			log_putx(mode);
			log_puts(": unsupported mode\n");
		}
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
		if (p->devnum < 16) {
			d = dev_bynum(p->devnum);
			if (d == NULL)
				return 0;
			midi_tag(f->midi, p->devnum);
		} else if (p->devnum < 32) {
			midi_tag(f->midi, p->devnum);
		} else if (p->devnum < 48) {
			c = port_bynum(p->devnum - 32);
			if (c == NULL || !port_ref(c))
				return 0;
			f->port = c;
			midi_link(f->midi, c->midi);
		} else
			return 0;
		return 1;
	}
	f->opt = opt_byname(p->opt, p->devnum);
	if (f->opt == NULL)
		return 0;
#ifdef DEBUG
	if (log_level >= 3) {
		sock_log(f);
		log_puts(": using ");
		dev_log(f->opt->dev);
		log_puts(".");
		log_puts(f->opt->name);
		log_puts(", mode = ");
		log_putx(mode);
		log_puts("\n");
	}
#endif
	if ((mode & MODE_REC) && (f->opt->mode & MODE_MON)) {
		mode |= MODE_MON;
		mode &= ~MODE_REC;
	}
	if ((mode & f->opt->mode) != mode) {
		if (log_level >= 1) {
			sock_log(f);
			log_puts(": requested mode not allowed\n");
		}
		return 0;
	}
	s = slot_new(f->opt->dev, p->who, &sock_slotops, f, mode);
	if (s == NULL)
		return 0;
	f->midi = NULL;
	if (s->mode & MODE_PLAY) {
		s->mix.slot_cmin = s->mix.dev_cmin = f->opt->pmin;
		s->mix.slot_cmax = s->mix.dev_cmax = f->opt->pmax;
	}
	if (s->mode & MODE_RECMASK) {
		s->sub.slot_cmin = s->sub.dev_cmin = f->opt->rmin;
		s->sub.slot_cmax = s->sub.dev_cmax = f->opt->rmax;
	}
	if (f->opt->mmc) {
		s->xrun = XRUN_SYNC;
		s->tstate = MMC_STOP;
	} else {
		s->xrun = XRUN_IGNORE;
		s->tstate = MMC_OFF;
	}
	s->mix.maxweight = f->opt->maxweight;
	s->dup = f->opt->dup;
	f->slot = s;
	return 1;
}

/*
 * execute the message in f->rmsg, return 1 on success
 */
int
sock_execmsg(struct sock *f)
{
	struct slot *s = f->slot;
	struct amsg *m = &f->rmsg;
	unsigned char *data;
	int size, ctl;

	switch (ntohl(m->cmd)) {
	case AMSG_DATA:
#ifdef DEBUG
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": DATA message\n");
		}
#endif
		if (s != NULL && f->pstate != SOCK_START) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": DATA, wrong state\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		if ((f->slot && !(f->slot->mode & MODE_PLAY)) ||
		    (f->midi && !(f->midi->mode & MODE_MIDIOUT))) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": DATA, input-only mode\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		size = ntohl(m->u.data.size);
		if (size <= 0) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": zero size payload\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		if (s != NULL && size % s->mix.bpf != 0) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": not aligned to frame\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		if (s != NULL && size > f->ralign) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": size = ");
				log_puti(size);
				log_puts(": ralign = ");
				log_puti(f->ralign);
				log_puts(": not aligned to block\n");
			}
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
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": unexpected data, size = ");
				log_putu(size);
				log_puts(", rmax = ");
				log_putu(f->rmax);
				log_puts("\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		f->rmax -= f->rtodo;
		if (f->rtodo == 0) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": zero-length data chunk\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		break;
	case AMSG_START:
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": START message\n");
		}
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": START, wrong state\n");
			}
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
		if (log_level >= 2) {
			slot_log(f->slot);
			log_puts(": ");
			log_putu(s->rate);
			log_puts("Hz, ");
			aparams_log(&s->par);
			if (s->mode & MODE_PLAY) {
				log_puts(", play ");
				log_puti(s->mix.slot_cmin);
				log_puts(":");
				log_puti(s->mix.slot_cmax);
			}
			if (s->mode & MODE_RECMASK) {
				log_puts(", rec ");
				log_puti(s->sub.slot_cmin);
				log_puts(":");
				log_puti(s->sub.slot_cmax);
			}
			log_puts(", ");
			log_putu(s->appbufsz / s->round);
			log_puts(" blocks of ");
			log_putu(s->round);
			log_puts(" frames\n");
		}
		break;
	case AMSG_STOP:
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": STOP message\n");
		}
#endif
		if (f->pstate != SOCK_START) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": STOP, wrong state\n");
			}
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
					sock_log(f);
					log_puts(": unaligned stop, size = ");
					log_putu(size);
					log_puts(", ralign = ");
					log_putu(f->ralign);
					log_puts("\n");
					panic();
				}
#endif
				memset(data, 0, f->ralign);
				abuf_wcommit(&s->mix.buf, f->ralign);
				f->ralign = s->round * s->mix.bpf;
			}
		}
		slot_stop(s);		
		break;
	case AMSG_SETPAR:
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": SETPAR message\n");
		}
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": SETPAR, wrong state\n");
			}
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
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": GETPAR message\n");
		}
#endif
		if (f->pstate != SOCK_INIT || s == NULL) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": GETPAR, wrong state\n");
			}
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
		if (s->mode & MODE_PLAY) {
			m->u.par.pchan = htons(s->mix.slot_cmax -
			    s->mix.slot_cmin + 1);
		}
		if (s->mode & MODE_RECMASK) {
			m->u.par.rchan = htons(s->sub.slot_cmax -
			    s->sub.slot_cmin + 1);
		}
		m->u.par.rate = htonl(s->rate);
		m->u.par.appbufsz = htonl(s->appbufsz);
		m->u.par.bufsz = htonl(SLOT_BUFSZ(s));
		m->u.par.round = htonl(s->round);
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETVOL:
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": SETVOL message\n");
		}
#endif
		if (f->pstate < SOCK_INIT || s == NULL) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": SETVOL, wrong state\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		ctl = ntohl(m->u.vol.ctl);
		if (ctl > MIDI_MAXCTL) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": SETVOL, volume out of range\n");
			}
#endif
			sock_close(f);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		f->lastvol = ctl; /* dont trigger feedback message */
		slot_setvol(s, ctl);
		dev_midi_vol(s->dev, s);
		break;
	case AMSG_AUTH:
#ifdef DEBUG
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": AUTH message\n");
		}
#endif
		if (f->pstate != SOCK_AUTH) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": AUTH, wrong state\n");
			}
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
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": HELLO message\n");
		}
#endif
		if (f->pstate != SOCK_HELLO) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": HELLO, wrong state\n");
			}
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
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": BYE message\n");
		}
#endif
		if (s != NULL && f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (log_level >= 1) {
				sock_log(f);
				log_puts(": BYE, wrong state\n");
			}
#endif
		}
		sock_close(f);
		return 0;
	default:
#ifdef DEBUG
		if (log_level >= 1) {
			sock_log(f);
			log_puts(": unknown command in message\n");
		}
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
	unsigned int size;	

	/*
	 * If pos changed (or initial tick), build a MOVE message.
	 */
	if (f->tickpending) {
#ifdef DEBUG
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": building MOVE message, delta = ");
			log_puti(f->slot->delta);
			log_puts("\n");
		}
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
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": building FLOWCTL message, count = ");
			log_puti(f->fillpending);
			log_puts(", rmax -> ");
			log_puti(f->rmax);
			log_puts("\n");
		}
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
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": building SETVOL message, vol = ");
			log_puti(f->slot->vol);
			log_puts("\n");
		}
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
			sock_log(f);
			log_puts(": sock_buildmsg size == 0\n");
			panic();
		}
#endif
		f->walign -= size;
		f->wmax -= size;
		if (f->walign == 0)
			f->walign = f->slot->round * f->slot->sub.bpf;
#ifdef DEBUG
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": building audio DATA message, size = ");
			log_puti(size);
			log_puts("\n");
		}
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
		if (log_level >= 3) {
			sock_log(f);
			log_puts(": building STOP message\n");
		}
#endif
		f->stoppending = 0;
		f->pstate = SOCK_INIT;
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = htonl(AMSG_STOP);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}
#ifdef DEBUG
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": no messages to build anymore, idling...\n");
	}
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": reading ");
		log_putu(f->rtodo);
		log_puts(" todo\n");
	}
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
			if (log_level >= 4) {
				sock_log(f);
				log_puts(": can't reply, write-end blocked\n");
			}
#endif
			return 0;
		}
		f->wmsg = f->rmsg;
		f->wstate = SOCK_WMSG;
		f->wtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
#ifdef DEBUG
		if (log_level >= 4) {
			sock_log(f);
			log_puts(": copied RRET message\n");
		}
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
	if (log_level >= 4) {
		sock_log(f);
		log_puts(": writing");
		if (f->wstate != SOCK_WIDLE) {
			log_puts(" todo = ");
			log_putu(f->wtodo);
		}
		log_puts("\n");
	}
#endif
	switch (f->wstate) {
	case SOCK_WMSG:
		if (!sock_wmsg(f))
			return 0;
		if (ntohl(f->wmsg.cmd) != AMSG_DATA) {
			f->wstate = SOCK_WIDLE;
			f->wtodo = 0xdeadbeef;
			break;
		}
		f->wstate = SOCK_WDATA;
		f->wsize = f->wtodo = ntohl(f->wmsg.u.data.size);
		/* PASSTHROUGH */
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
			if (log_level >= 4) {
				sock_log(f);
				log_puts(": drained, moved to INIT state\n");
			}
#endif
		}
		/* PASSTHROUGH */
	case SOCK_WIDLE:
		if (f->rstate == SOCK_RRET) {
			f->wmsg = f->rmsg;
			f->wstate = SOCK_WMSG;
			f->wtodo = sizeof(struct amsg);
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
#ifdef DEBUG
			if (log_level >= 4) {
				sock_log(f);
				log_puts(": copied RRET message\n");
			}
#endif
		} else {
			if (!sock_buildmsg(f))
				return 0;
		}
		break;
#ifdef DEBUG
	default:
		sock_log(f);
		log_puts(": bad writing end state\n");
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
