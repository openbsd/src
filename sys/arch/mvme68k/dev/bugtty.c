/*	$OpenBSD: bugtty.c,v 1.14 2006/01/01 11:59:39 miod Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/prom.h>

#include "bugtty.h"

int bugttymatch(struct device *parent, void *self, void *aux);
void bugttyattach(struct device *parent, struct device *self, void *aux);

struct cfattach bugtty_ca = {
	sizeof(struct device), bugttymatch, bugttyattach
};

struct cfdriver bugtty_cd = {
	NULL, "bugtty", DV_TTY
};

/* prototypes */
cons_decl(bugtty);

struct tty *bugttytty(dev_t);
int bugttymctl(dev_t, int, int);
int bugttyparam(struct tty*, struct termios *);
void bugtty_chkinput(void);

#define DIALOUT(x) ((x) & 0x80)
#define SWFLAGS(dev) (bugttyswflags | (DIALOUT(dev) ? TIOCFLAG_SOFTCAR : 0))
#define BUGTTYUNIT(x) ((x) & 0x7f)

#define BUGBUF 80
char bugtty_ibuffer[BUGBUF+1];
volatile char *pinchar = bugtty_ibuffer;
char bug_obuffer[BUGBUF+1];

#define	BUGTTYS	4
struct tty *bugtty_tty[BUGTTYS];

struct tty *
bugttytty(dev)
	dev_t dev;
{
	int unit;

	unit = BUGTTYUNIT(dev);
	if (unit >= BUGTTYS)
		return (NULL);
	return (bugtty_tty[unit]);
}

int
bugttymatch(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	extern int needprom;
	
	if (needprom == 0)
		return (0);
	return (1);
}

void
bugttyattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf(": fallback console\n");
}

void bugttyoutput(struct tty *tp);

int bugttydefaultrate = TTYDEF_SPEED;
int bugttyswflags;

int
bugttymctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	/*static int settings = TIOCM_DTR | TIOCM_RTS |
	    TIOCM_CTS | TIOCM_CD | TIOCM_DSR;*/
	int s;

	/*printf("mctl: dev %x, bits %x, how %x,",dev, bits, how);*/

	/* settings are currently ignored */
	s = spltty();
	switch (how) {
	case DMSET:
		break;
	case DMBIC:
		break;
	case DMBIS:
		break;
	case DMGET:
		break;
	}
	splx(s);

	bits = 0;
	/* proper defaults? */
	bits |= TIOCM_DTR;
	bits |= TIOCM_RTS;
	bits |= TIOCM_CTS;
	bits |= TIOCM_CD;
	/* bits |= TIOCM_RI; */
	bits |= TIOCM_DSR;

	/* printf("retbits %x\n", bits); */
	return (bits);
}

int
bugttyopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int s, unit = BUGTTYUNIT(dev);
	struct tty *tp;
	extern int needprom;

	if (needprom == 0)
		return (ENODEV);

	s = spltty();
	if (bugtty_tty[unit]) {
		tp = bugtty_tty[unit];
	} else {
		tp = bugtty_tty[unit] = ttymalloc();
	}
	tp->t_oproc = bugttyoutput;
	tp->t_param = NULL;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			/*
			 * only when cleared do we reset to defaults.
			 */
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = bugttydefaultrate;
		}
		/* bugtty does not have carrier */
		tp->t_cflag |= CLOCAL;
		/*
		 * do these all the time
		 */
		if (bugttyswflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (bugttyswflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (bugttyswflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
		bugttyparam(tp, &tp->t_termios);
		ttsetwater(tp);

		(void)bugttymctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);
		/*
		if ((SWFLAGS(dev) & TIOCFLAG_SOFTCAR) ||
		    (bugttymctl(dev, 0, DMGET) & TIOCM_CD))
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &= ~TS_CARR_ON;
		*/
		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}

	/*
	 * if NONBLOCK requested, ignore carrier
	 */
/*
	if (flag & O_NONBLOCK)
		goto done;
*/

	splx(s);
	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
bugttyparam(tp, tm)
	struct tty*tp;
	struct termios *tm;
{
	return (0);
}

void
bugttyoutput(tp)
	struct tty *tp;
{
	int cc, s, cnt;

	/* only supports one unit */

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = spltty();
	cc = tp->t_outq.c_cc;
	while (cc > 0) {
		cnt = min(BUGBUF, cc);
		cnt = q_to_b(&tp->t_outq, bug_obuffer, cnt);
		bug_outstr(bug_obuffer, &bug_obuffer[cnt]);
		cc -= cnt;
	}
	splx(s);
}

int
bugttyclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = BUGTTYUNIT(dev);
	struct tty *tp = bugtty_tty[unit];

	(*linesw[tp->t_line].l_close)(tp, flag);

	ttyclose(tp);
#if 0
	bugtty_tty[unit] = NULL;
#endif
	return (0);
}

int
bugttyread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp;

	if ((tp = bugtty_tty[BUGTTYUNIT(dev)]) == NULL)
		return (ENXIO); 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

#if 1
/* only to be called at splclk() */
void
bugtty_chkinput()
{
	struct tty *tp;

	tp = bugtty_tty[0]; /* Kinda ugly hack */
	if (tp == NULL )
		return;

	if (bug_instat()) {
		while (bug_instat()) {
			u_char c = bug_inchr() & 0xff;
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
		/*
		wakeup(tp);
		*/
	}
}
#endif

int
bugttywrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
#if 0
	/* bypass tty output routines. */
	int i, cnt, s;
	int oldoff;

	s = spltty();
	oldoff = uio->uio_offset;
	do  {
		uiomove(bug_obuffer, BUGBUF, uio);
		bug_outstr(bug_obuffer, &bug_obuffer[uio->uio_offset - oldoff]);
		oldoff = uio->uio_offset;
	} while (uio->uio_resid != 0);
	splx(s);

	return (0);
#else
	struct tty *tp;
	if((tp = bugtty_tty[BUGTTYUNIT(dev)]) == NULL)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
#endif
}

int
bugttyioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = BUGTTYUNIT(dev);
	struct tty *tp = bugtty_tty[unit];
	int error;

	if (!tp)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0) 
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		/* */
		break;

	case TIOCCBRK:
		/* */
		break;

	case TIOCSDTR:
		(void) bugttymctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) bugttymctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) bugttymctl(dev, *(int *) data, DMSET);
		break;

	case TIOCMBIS:
		(void) bugttymctl(dev, *(int *) data, DMBIS);
		break;

	case TIOCMBIC:
		(void) bugttymctl(dev, *(int *) data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = bugttymctl(dev, 0, DMGET);
		break;
	case TIOCGFLAGS:
		*(int *)data = SWFLAGS(dev);
		break;
	case TIOCSFLAGS:
		error = suser(p, 0); 
		if (error != 0)
			return (EPERM); 

		bugttyswflags = *(int *)data;
		bugttyswflags &= /* only allow valid flags */
		    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
bugttystop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return (0);
}

/*
 * bugtty is the last possible choice for a console device.
 */
void
bugttycnprobe(cp)
	struct consdev *cp;
{
	int maj;
	extern int needprom;

	if (needprom == 0)
		return;
		
	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == bugttyopen)
			break;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_NORMAL;
}

void
bugttycninit(cp)
	struct consdev *cp;
{
	/* Nothing to do */
}

int
bugttycngetc(dev)
	dev_t dev;
{
	return (bug_inchr());
}

void
bugttycnputc(dev, c)
	dev_t dev;
	char c;
{
	bug_outchr(c);
}
