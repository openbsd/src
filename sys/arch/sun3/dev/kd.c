/*	$NetBSD: kd.c,v 1.13 1995/04/26 23:20:15 gwr Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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

/*
 * Keyboard/Display device.
 *
 * This driver exists simply to provide a tty device that
 * the indirect console driver can point to.
 * The kbd driver sends its input here.
 * Output goes to the screen via PROM printf.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/mon.h>
#include <machine/psl.h>

#include <dev/cons.h>

#define BURST	64

/*
 * There is no point in pretending there might be
 * more than one keyboard/display device.
 */
struct tty *kd_tty[1];

int kdopen(dev_t, int, int, struct proc *);
int kdclose(dev_t, int, int, struct proc *);
int kdread(dev_t, struct uio *, int);
int kdwrite(dev_t, struct uio *, int);
int kdioctl(dev_t, int, caddr_t, int, struct proc *);

static int kdparam(struct tty *, struct termios *);
static void kdstart(struct tty *);

int kd_is_console;

/* This is called by kbd_serial() like a pseudo-device. */
void
kd_attach(n)
	int n;
{
	kd_tty[0] = ttymalloc();

	/* Tell keyboard module where to send read data. */
	kbd_ascii(kd_tty[0]);
}

struct tty *
kdtty(dev)
	dev_t dev;
{
	return kd_tty[0];
}

int
kdopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int error, unit;
	struct tty *tp;
	
	unit = minor(dev);
	if (unit) return ENXIO;

	tp = kd_tty[unit];
	if (tp == NULL)
		return ENXIO;

	if ((error = kbd_iopen()) != 0) {
#ifdef	DIAGNOSTIC
		printf("kd: kbd_iopen, error=%d\n", error);
#endif
		return (error);
	}

	tp->t_oproc = kdstart;
	tp->t_param = kdparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		kdparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
kdclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = kd_tty[unit];

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return (0);
}

int
kdread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct tty *tp = kd_tty[unit];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
kdwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct tty *tp = kd_tty[unit];

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
kdstop(tp, flag)
	struct tty *tp;
	int flag;
{

}

int
kdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;
	int unit = minor(dev);
	struct tty *tp = kd_tty[unit];

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	/* Handle any ioctl commands specific to kbd/display. */
	/* XXX - Send KB* ioctls to kbd module? */
	/* XXX - Send FB* ioctls to fb module?  */

	return ENOTTY;
}

static void kd_later(void*);
static void kd_putfb(struct tty *);

void
kdstart(tp)
	struct tty *tp;
{
	struct clist *cl;
	register int s;

	s = spltty();
	if (tp->t_state & (TS_BUSY|TS_TTSTOP|TS_TIMEOUT))
		goto out;

	cl = &tp->t_outq;
	if (cl->c_cc) {
		if (kd_is_console) {
			tp->t_state |= TS_BUSY;
			if ((s & PSL_IPL) == 0) {
				/* called at level zero - update screen now. */
				(void) splsoftclock();
				kd_putfb(tp);
				(void) spltty();
				tp->t_state &= ~TS_BUSY;
			} else {
				/* called at interrupt level - do it later */
				timeout(kd_later, (void*)tp, 0);
			}
		} else {
			/*
			 * This driver uses the PROM for writing the screen,
			 * and that only works if this is the console device.
			 * If this is not the console, just flush the output.
			 * Sorry.  (In that case, use xdm instead of getty.)
			 */
			ndflush(cl, cl->c_cc);
		}
	}
	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)cl);
		}
		selwakeup(&tp->t_wsel);
	}
out:
	splx(s);
}

/*
 * Timeout function to do delayed writes to the screen.
 * Called at splsoftclock when requested by kdstart.
 */
static void
kd_later(tpaddr)
	void *tpaddr;
{
	struct tty *tp = tpaddr;
	register int s;

	kd_putfb(tp);

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		kdstart(tp);
	splx(s);
}

/*
 * Put text on the screen using the PROM monitor.
 * This can take a while, so to avoid missing
 * interrupts, this is called at splsoftclock.
 */
static void kd_putfb(tp)
	struct tty *tp;
{
	char buf[BURST];
	struct clist *cl = &tp->t_outq;
	char *p, *end;
	int len;

	while ((len = q_to_b(cl, buf, BURST-1)) > 0) {
		/* PROM will barf if high bits are set. */
		p = buf;
		end = buf + len;
		while (p < end)
			*p++ &= 0x7f;
		(romVectorPtr->fbWriteStr)(buf, len);
	}
}


static int
kdparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	/* XXX - These are ignored... */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}


/*
 * kd console support
 */

extern int zscnprobe_kbd(), zscngetc(), kbd_translate();

kdcnprobe(cp)
	struct consdev *cp;
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == (void*)kdopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = zscnprobe_kbd();
}

kdcninit(cp)
	struct consdev *cp;
{

	/* This prepares zscngetc() */
	zs_set_conschan(1, 0);

	/* This prepares kbd_translate() */
	kbd_init_tables();

	/* Indicate that it is OK to use the PROM fbwrite */
	kd_is_console = 1;

	mon_printf("console on kd0 (keyboard/display)\n");
}

kdcngetc(dev)
	dev_t dev;
{
	int c;

	do {
		c = zscngetc(0);
		c = kbd_translate(c);
	} while (c == -1);

	return (c);
}

kdcnputc(dev, c)
	dev_t dev;
	int c;
{
	(romVectorPtr->fbWriteChar)(c & 0x7f);
}

extern void fb_unblank();
void kdcnpollc(dev, on)
	dev_t dev;
	int on;
{
	if (on)
		fb_unblank();
}
