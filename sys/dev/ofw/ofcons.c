/*	$OpenBSD: ofcons.c,v 1.6 2001/08/21 01:55:50 drahn Exp $	*/
/*	$NetBSD: ofcons.c,v 1.3 1996/10/13 01:38:11 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <dev/cons.h>

#include <dev/ofw/openfirm.h>

#include <machine/stdarg.h>

struct ofc_softc {
	struct device of_dev;
	struct tty *of_tty;
	int of_flags;
	struct timeout of_tmo;
};
/* flags: */
#define	OFPOLL		1

#define	OFBURSTLEN	128	/* max number of bytes to write in one chunk */

static int stdin  = 0;
static int stdout = 0;

static int ofcmatch __P((struct device *, void *, void *));
static void ofcattach __P((struct device *, struct device *, void *));

struct cfattach ofcons_ca = {
	sizeof(struct ofc_softc), ofcmatch, ofcattach
};

struct cfdriver ofcons_cd = {
	NULL, "ofcons", DV_TTY
};

static int ofcprobe __P((void));

static int
ofcmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct ofprobe *ofp = aux;
	
	if (!ofcprobe())
		return 0;
	return OF_instance_to_package(stdin) == ofp->phandle
		|| OF_instance_to_package(stdout) == ofp->phandle;
}

static void ofcstart __P((struct tty *));
static int ofcparam __P((struct tty *, struct termios *));
static void ofcpoll __P((void *));

static void
ofcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ofc_softc *sc = (void *)self:

	timeout_set(&sc->of_tmo, ofcpoll, sc);
	printf("\n");
}

void ofcstart __P((struct tty *));
int ofcparam __P((struct tty *, struct termios *));
void ofcpoll __P((void *));
int ofcopen(dev_t dev, int flag, int mode, struct proc *p);
int ofcclose(dev_t dev, int flag, int mode, struct proc *p);
int ofcread(dev_t dev, struct uio *uio, int flag);
int ofcwrite(dev_t dev, struct uio *uio, int flag);
int ofcioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);
struct tty * ofctty(dev_t dev);
void ofcstop(struct tty *tp, int flag);
void ofcstart(struct tty *tp);
int ofcparam(struct tty *tp, struct termios *t);
void ofcpoll(void *aux);
void ofccnprobe(struct consdev *cd);
void ofccninit(struct consdev *cd);
int ofccngetc(dev_t dev);
void ofccnputc(dev_t dev, int c);
void ofccnpollc(dev_t dev, int on);
void ofprintf(char *fmt, ...);

int
ofcopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct ofc_softc *sc;
	int unit = minor(dev);
	struct tty *tp;
	
	if (unit >= ofcons_cd.cd_ndevs)
		return ENXIO;
	sc = ofcons_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;
	if (!(tp = sc->of_tty))
		sc->of_tty = tp = ttymalloc();
	tp->t_oproc = ofcstart;
	tp->t_param = ofcparam;
	tp->t_dev = dev;
	if (!(tp->t_state & TS_ISOPEN)) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ofcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state&TS_XCLUDE) && suser(p->p_ucred, &p->p_acflag))
		return EBUSY;
	tp->t_state |= TS_CARR_ON;
	
	if (!(sc->of_flags & OFPOLL)) {
		sc->of_flags |= OFPOLL;
		timeout_add(&sc->of_tmo, 1);
	}

	return (*linesw[tp->t_line].l_open)(dev, tp);
}

int
ofcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->of_tty;

	timeout_del(&sc->of_tmo);
	sc->of_flags &= ~OFPOLL;
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}

int
ofcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->of_tty;
	
	return (*linesw[tp->t_line].l_read)(tp, uio, flag);
}

int
ofcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->of_tty;
	
	return (*linesw[tp->t_line].l_write)(tp, uio, flag);
}

int
ofcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->of_tty;
	int error;
	
	if ((error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p)) >= 0)
		return error;
	if ((error = ttioctl(tp, cmd, data, flag, p)) >= 0)
		return error;
	return ENOTTY;
}

struct tty *
ofctty(dev)
	dev_t dev;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];

	return sc->of_tty;
}

void
ofcstop(tp, flag)
	struct tty *tp;
	int flag;
{
}

void
ofcstart(tp)
	struct tty *tp;
{
	struct clist *cl;
	int s, len;
	u_char buf[OFBURSTLEN];
	
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, OFBURSTLEN);
	OF_write(stdout, buf, len);
	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (cl->c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout_add(&tp->t_rstrt_to, 1);
	}
	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(cl);
		}
		selwakeup(&tp->t_wsel);
	}
	splx(s);
}

int
ofcparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

void
ofcpoll(aux)
	void *aux;
{
	struct ofc_softc *sc = aux;
	struct tty *tp = sc->of_tty;
	char ch;
	
	while (OF_read(stdin, &ch, 1) > 0) {
		if (tp && (tp->t_state & TS_ISOPEN))
			(*linesw[tp->t_line].l_rint)(ch, tp);
	}
	timeout_add(&of->of_tmo, 1);
}

static int
ofcprobe()
{
	int chosen;

	if (stdin)
		return 1;
	if ((chosen = OF_finddevice("/chosen")) == -1)
		return 0;
	if (OF_getprop(chosen, "stdin", &stdin, sizeof stdin) != sizeof stdin
	    || OF_getprop(chosen, "stdout", &stdout, sizeof stdout) != sizeof stdout)
		return 0;
	return 1;
}

void
ofccnprobe(cd)
	struct consdev *cd;
{
	int maj;

	if (!ofcprobe())
		return;

	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == ofcopen)
			break;
	cd->cn_dev = makedev(maj, 0);
	cd->cn_pri = CN_INTERNAL;
}

void
ofccninit(cd)
	struct consdev *cd;
{
}

int
ofccngetc(dev)
	dev_t dev;
{
	unsigned char ch;
	int l;
	
	while ((l = OF_read(stdin, &ch, 1)) != 1)
		if (l != -2)
			return -1;
	return ch;
}

void
ofccnputc(dev, c)
	dev_t dev;
	int c;
{
	char ch = c;
	
/*#ifdef DEBUG */
#if 1
	if (stdout == 0) {
		ofcprobe();
	}
#endif
	OF_write(stdout, &ch, 1);
}

void
ofccnpollc(dev, on)
	dev_t dev;
	int on;
{
	struct ofc_softc *sc = ofcons_cd.cd_devs[minor(dev)];
	
	if (!sc)
		return;
	if (on) {
		if (sc->of_flags & OFPOLL)
			timeout_del(&of->of_tmo);
		sc->of_flags &= ~OFPOLL;
	} else {
		if (!(sc->of_flags & OFPOLL)) {
			sc->of_flags |= OFPOLL;
			timeout_add(&of->of_tmo, 1);
		}
	}
}
static char buf[1024];

void
ofprintf(char *fmt, ...)
{
	char *c;
	va_list ap;

	va_start(ap, fmt);

	vsprintf(buf, fmt, ap);

	c = buf;
	while (*c != '\0') {
		ofccnputc(0, *c);
	}

	va_end(ap);
}
