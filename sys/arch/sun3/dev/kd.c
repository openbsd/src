/*	$NetBSD: kd.c,v 1.14 1996/01/24 22:40:20 gwr Exp $	*/

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
#include <dev/sun/kbd_xlate.h>

#define	KDMAJOR 1
#define PUT_WSIZE	64

cdev_decl(kd);	/* open, close, read, write, ioctl, stop, ... */

struct kd_softc {
	struct	device kd_dev;		/* required first: base device */
	struct  tty *kd_tty;
};

/*
 * There is no point in pretending there might be
 * more than one keyboard/display device.
 */
struct kd_softc kd_softc;

static int kdparam(struct tty *, struct termios *);
static void kdstart(struct tty *);

int kd_is_console;

/*
 * This is called by kbd_attach() 
 * XXX - Make this a proper child of kbd?
 */
void
kd_init(unit)
	int unit;
{
	struct kd_softc *kd;
	struct tty *tp;

	if (unit != 0)
		return;
	kd = &kd_softc; 	/* XXX */
	tp = ttymalloc();

	kd->kd_tty = tp;
	tp->t_oproc = kdstart;
	tp->t_param = kdparam;
	tp->t_dev = makedev(KDMAJOR, unit);

	return;
}

struct tty *
kdtty(dev)
	dev_t dev;
{
	struct kd_softc *kd;

	kd = &kd_softc; 	/* XXX */
	return (kd->kd_tty);
}

int
kdopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct kd_softc *kd;
	int error, s, unit;
	struct tty *tp;
	
	unit = minor(dev);
	if (unit != 0)
		return ENXIO;
	kd = &kd_softc; 	/* XXX */
	tp = kd->kd_tty;

	if ((error = kbd_iopen(unit)) != 0) {
#ifdef	DIAGNOSTIC
		printf("kd: kbd_iopen, error=%d\n", error);
#endif
		return (error);
	}

	/* It's simpler to do this up here. */
	if (((tp->t_state & (TS_ISOPEN | TS_XCLUDE))
	     ==             (TS_ISOPEN | TS_XCLUDE))
	    && (p->p_ucred->cr_uid != 0) )
	{
		return (EBUSY);
	}

	s = spltty();

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/* First open. */
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		(void) kdparam(tp, &tp->t_termios);
		ttsetwater(tp);
		/* Flush pending input?  Clear translator? */
		/* This (pseudo)device always has SOFTCAR */
		tp->t_state |= TS_CARR_ON;
	}

	splx(s);

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
kdclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct kd_softc *kd;
	struct tty *tp;

	kd = &kd_softc; 	/* XXX */
	tp = kd->kd_tty;

	/* XXX This is for cons.c. */
	if ((tp->t_state & TS_ISOPEN) == 0)
		return 0;

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
	struct kd_softc *kd;
	struct tty *tp;

	kd = &kd_softc; 	/* XXX */
	tp = kd->kd_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
kdwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct kd_softc *kd;
	struct tty *tp;

	kd = &kd_softc; 	/* XXX */
	tp = kd->kd_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
kdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct kd_softc *kd;
	struct tty *tp;
	int error;

	kd = &kd_softc; 	/* XXX */
	tp = kd->kd_tty;

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


int
kdstop(tp, flag)
	struct tty *tp;
	int flag;
{

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
	char buf[PUT_WSIZE];
	struct clist *cl = &tp->t_outq;
	char *p, *end;
	int len;

	while ((len = q_to_b(cl, buf, PUT_WSIZE-1)) > 0) {
		/* PROM will barf if high bits are set. */
		p = buf;
		end = buf + len;
		while (p < end)
			*p++ &= 0x7f;
		(romVectorPtr->fbWriteStr)(buf, len);
	}
}

/*
 * Our "interrupt" routine for input.
 */
void
kd_input(c)
	int c;
{
	struct kd_softc *kd = &kd_softc;
	struct tty *tp;

	/* XXX: Make sure the device is open. */
	tp = kd->kd_tty;
	if (tp == NULL)
		return;
    if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	ttyinput(c, kd->kd_tty);
}


/****************************************************************
 * kd console support
 ****************************************************************/

extern void *zs_conschan;
extern int zs_getc();
extern void nullcnprobe();
cons_decl(kd);

/* The debugger gets its own key translation state. */
static struct kbd_state kdcn_state;

void
kdcninit(cn)
	struct consdev *cn;
{
	struct kbd_state *ks = &kdcn_state;

	mon_printf("console on kd0 (keyboard/display)\n");

	/* This prepares kbd_translate() */
	ks->kbd_id = KBD_MIN_TYPE;
	kbd_xlate_init(ks);

	/* Indicate that it is OK to use the PROM fbwrite */
	kd_is_console = 1;
}

int
kdcngetc(dev)
	dev_t dev;
{
	struct kbd_state *ks = &kdcn_state;
	int code, class, data, keysym;

	for (;;) {
		code = zs_getc(zs_conschan);
		keysym = kbd_code_to_keysym(ks, code);
		class = KEYSYM_CLASS(keysym);

		switch (class) {
		case KEYSYM_ASCII:
			goto out;

		case KEYSYM_CLRMOD:
		case KEYSYM_SETMOD:
			data = (keysym & 0x1F);
			/* Only allow ctrl or shift. */
			if (data > KBMOD_SHIFT_R)
				break;
			data = 1 << data;
			if (class == KEYSYM_SETMOD)
				ks->kbd_modbits |= data;
			else
				ks->kbd_modbits &= ~data;
			break;

		case KEYSYM_ALL_UP:
			/* No toggle keys here. */
			ks->kbd_modbits = 0;
			break;

		default:	/* ignore all other keysyms */
			break;
		}
	}
out:
	return (keysym);
}

void
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
	struct kbd_state *ks = &kdcn_state;

	if (on) {
		/* Entering debugger. */
		fb_unblank();
		/* Clear shift keys too. */
		ks->kbd_modbits = 0;
	} else {
		/* Resuming kernel. */
	}
}

struct consdev consdev_kd = {
	nullcnprobe,
	kdcninit,
	kdcngetc,
	kdcnputc,
	kdcnpollc,
	makedev(KDMAJOR, 0),
	CN_INTERNAL
};

