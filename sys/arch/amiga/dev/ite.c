/*	$NetBSD: ite.c,v 1.32.2.2 1995/10/20 11:01:08 chopps Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Utah Hdr: ite.c 1.1 90/07/09
 *	@(#)ite.c 7.6 (Berkeley) 5/16/91
 */

/*
 * ite - bitmaped terminal.
 * Supports VT200, a few terminal features will be unavailable until
 * the system actually probes the device (i.e. not after consinit())
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <dev/cons.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/kdassert.h>
#include <amiga/amiga/color.h>	/* DEBUG */
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/iteioctl.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/kbdmap.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>

#include "grfcc.h"

/*
 * XXX go ask sys/kern/tty.c:ttselect()
 */

#define ITEUNIT(dev)	(minor(dev))

#define SUBR_INIT(ip)		(ip)->grf->g_iteinit(ip)
#define SUBR_DEINIT(ip)		(ip)->grf->g_itedeinit(ip)
#define SUBR_PUTC(ip,c,dy,dx,m)	(ip)->grf->g_iteputc(ip,c,dy,dx,m)
#define SUBR_CURSOR(ip,flg)	(ip)->grf->g_itecursor(ip,flg)
#define SUBR_CLEAR(ip,sy,sx,h,w)	(ip)->grf->g_iteclear(ip,sy,sx,h,w)
#define SUBR_SCROLL(ip,sy,sx,count,dir)	\
    (ip)->grf->g_itescroll(ip,sy,sx,count,dir)

u_int	ite_confunits;			/* configured units */

int	start_repeat_timeo = 30;	/* first repeat after x s/100 */
int	next_repeat_timeo = 10;		/* next repeat after x s/100 */

int	ite_default_wrap = 1;		/* you want vtxxx-nam, binpatch */

struct	ite_softc con_itesoftc;
u_char	cons_tabs[MAX_TABS];

struct ite_softc *kbd_ite;
int kbd_init;

/* audio bell stuff */
u_int bvolume = 10;
u_int bpitch = 660;
u_int bmsec = 75;
	
static char *bsamplep;
static char sample[20] = {
	0,39,75,103,121,127,121,103,75,39,0,
	-39,-75,-103,-121,-127,-121,-103,-75,-39
};

static char *index __P((const char *, char));
static void ite_sifilter __P((void *, void *));
void iteputchar __P((int c, struct ite_softc *ip));
void ite_putstr __P((const char * s, int len, dev_t dev));
void iteattach __P((struct device *, struct device *, void *));
int itematch __P((struct device *, struct cfdata *, void *));
static void iteprecheckwrap __P((struct ite_softc *));
static void itecheckwrap __P((struct ite_softc *));


struct cfdriver itecd = {
	NULL, "ite", (cfmatch_t)itematch, iteattach, DV_DULL,
	sizeof(struct ite_softc), NULL, 0 };

int
itematch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct grf_softc *gp;
	int maj;
	
	gp = auxp;
	/*
	 * all that our mask allows (more than enough no one 
	 * has > 32 monitors for text consoles on one machine)
	 */
	if (cdp->cf_unit >= sizeof(ite_confunits) * NBBY)
		return(0);
	/*
	 * XXX
	 * normally this would be done in attach, however
	 * during early init we do not have a device pointer
	 * and thus no unit number.
	 */
	for(maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == iteopen)
			break;
	gp->g_itedev = makedev(maj, cdp->cf_unit);
	return(1);
}

void
iteattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct grf_softc *gp;
	struct ite_softc *ip;
	int s;

	gp = (struct grf_softc *)auxp;

	/*
	 * mark unit as attached (XXX see itematch)
	 */
	ite_confunits |= 1 << ITEUNIT(gp->g_itedev);

	if (dp) {
		ip = (struct ite_softc *)dp;

		s = spltty();
		if (con_itesoftc.grf != NULL &&
		    con_itesoftc.grf->g_unit == gp->g_unit) {
			/*
			 * console reinit copy params over.
			 * and console always gets keyboard
			 */
			bcopy(&con_itesoftc.grf, &ip->grf,
			    (char *)&ip[1] - (char *)&ip->grf);
			con_itesoftc.grf = NULL;
			kbd_ite = ip;
		}
		ip->grf = gp;
		splx(s);

		alloc_sicallback();
		iteinit(gp->g_itedev);
		printf(": rows %d cols %d", ip->rows, ip->cols);
		printf(" repeat at (%d/100)s next at (%d/100)s",
		    start_repeat_timeo, next_repeat_timeo);

		if (kbd_ite == NULL)
			kbd_ite = ip;
		if (kbd_ite == ip)
			printf(" has keyboard");
		printf("\n");
	} else {
		if (con_itesoftc.grf != NULL &&
		    con_itesoftc.grf->g_conpri > gp->g_conpri)
			return;
		con_itesoftc.grf = gp;
		con_itesoftc.tabs = cons_tabs;
	}
}

struct ite_softc *
getitesp(dev)
	dev_t dev;
{
	if (amiga_realconfig && con_itesoftc.grf == NULL)
		return(itecd.cd_devs[ITEUNIT(dev)]);

	if (con_itesoftc.grf == NULL)
		panic("no ite_softc for console");
	return(&con_itesoftc);
}

/*
 * cons.c entry points into ite device.
 */

/*
 * Return a priority in consdev->cn_pri field highest wins.  This function
 * is called before any devices have been probed.
 */
void
itecnprobe(cd)
	struct consdev *cd;
{
	/*
	 * bring graphics layer up.
	 */
	config_console();

	/* 
	 * return priority of the best ite (already picked from attach)
	 * or CN_DEAD.
	 */
	if (con_itesoftc.grf == NULL)
		cd->cn_pri = CN_DEAD;
	else {
		cd->cn_pri = con_itesoftc.grf->g_conpri;
		cd->cn_dev = con_itesoftc.grf->g_itedev;
	}
}

/* audio bell stuff */
void
init_bell()
{
	if (bsamplep != NULL)
		return;
	bsamplep = alloc_chipmem(20);
	if (bsamplep == NULL)
		panic("no chipmem for ite_bell"); 

	bcopy(sample, bsamplep, 20);
}

void
ite_bell()
{
	u_int clock;
	u_int period;
	u_int count;

	clock = 3579545; 	/* PAL 3546895 */

	/*
	 * the number of clock ticks per sample byte must be > 124
	 * ergo bpitch must be < clock / 124*20 
	 * i.e. ~1443, 1300 to be safe (PAL etc.). also not zero obviously
	 */
	period = clock / (bpitch * 20);
	count = bmsec * bpitch / 1000;

	play_sample(10, PREP_DMA_MEM(bsamplep), period, bvolume, 0x3, count);
}

void
itecninit(cd)
	struct consdev *cd;
{
	struct ite_softc *ip;

	ip = getitesp(cd->cn_dev);
	iteinit(cd->cn_dev);
	ip->flags |= ITE_ACTIVE | ITE_ISCONS;

	init_bell();
}

/*
 * ite_cnfinish() is called in ite_init() when the device is
 * being probed in the normal fasion, thus we can finish setting
 * up this ite now that the system is more functional.
 */
void
ite_cnfinish(ip)
	struct ite_softc *ip;
{
	static int done;

	if (done)
		return;
	done = 1;
}

int
itecngetc(dev)
	dev_t dev;
{
	int c;

	/* XXX this should be moved */
	if (!kbd_init) {
		kbd_init = 1;
		kbdenable();
	}
	do {
		c = kbdgetcn();
		c = ite_cnfilter(c, ITEFILT_CONSOLE);
	} while (c == -1);
	return (c);
}

void
itecnputc(dev, c)
	dev_t dev;
	int c;
{
	static int paniced;
	struct ite_softc *ip;
	char ch;

	ip = getitesp(dev);
	ch = c;

	if (panicstr && !paniced &&
	    (ip->flags & (ITE_ACTIVE | ITE_INGRF)) != ITE_ACTIVE) {
		(void)ite_on(dev, 3);
		paniced = 1;
	}
	iteputchar(ch, ip);
}

void
itecnpollc(dev, on)
	dev_t dev;
	int on;
{

}

/*
 * standard entry points to the device.
 */

/* 
 * iteinit() is the standard entry point for initialization of
 * an ite device, it is also called from ite_cninit().
 *
 */
void
iteinit(dev)
	dev_t dev;
{
	struct ite_softc *ip;

	ip = getitesp(dev);
	if (ip->flags & ITE_INITED)
		return;
	bcopy(&ascii_kbdmap, &kbdmap, sizeof(struct kbdmap));

	ip->cursorx = 0;
	ip->cursory = 0;
	SUBR_INIT(ip);
	SUBR_CURSOR(ip, DRAW_CURSOR);
	if (ip->tabs == NULL)
		ip->tabs = malloc(MAX_TABS * sizeof(u_char),M_DEVBUF,M_WAITOK);
	ite_reset(ip);
	ip->flags |= ITE_INITED;
}

int
iteopen(dev, mode, devtype, p)
	dev_t dev;
	int mode, devtype;
	struct proc *p;
{
	struct ite_softc *ip;
	struct tty *tp;
	int error, first, unit;

	unit = ITEUNIT(dev);
	first = 0;
	
	if (((1 << unit) & ite_confunits) == 0)
		return (ENXIO);
	
	ip = getitesp(dev);

	if (ip->tp == NULL)
		tp = ip->tp = ttymalloc();
	else
		tp = ip->tp;
	if ((tp->t_state & (TS_ISOPEN | TS_XCLUDE)) == (TS_ISOPEN | TS_XCLUDE)
	    && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	if ((ip->flags & ITE_ACTIVE) == 0) {
		error = ite_on(dev, 0);
		if (error)
			return (error);
		first = 1;
	}
	tp->t_oproc = itestart;
	tp->t_param = ite_param;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		tp->t_state = TS_WOPEN | TS_CARR_ON;
		ttsetwater(tp);
	}
	error = (*linesw[tp->t_line].l_open) (dev, tp);
	if (error == 0) {
		tp->t_winsize.ws_row = ip->rows;
		tp->t_winsize.ws_col = ip->cols;
		if (!kbd_init) {
			kbd_init = 1;
			kbdenable();
		}
	} else if (first)
		ite_off(dev, 0);
	return (error);
}

int
iteclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;

	tp = getitesp(dev)->tp;

	KDASSERT(tp);
	(*linesw[tp->t_line].l_close) (tp, flag);
	ttyclose(tp);
	ite_off(dev, 0);
	return (0);
}

int
iteread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp;

	tp = getitesp(dev)->tp;

	KDASSERT(tp);
	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

int
itewrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp;

	tp = getitesp(dev)->tp;

	KDASSERT(tp);
	return ((*linesw[tp->t_line].l_write) (tp, uio, flag));
}

struct tty *
itetty(dev)
	dev_t dev;
{
	return (getitesp(dev)->tp);
}

int
itestop(tp, flag)
	struct tty *tp;
	int flag;
{
	return (0);
}

int
iteioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct iterepeat *irp;
	struct ite_softc *ip;
	struct itebell *ib;
	struct tty *tp;
	int error;
	
	ip = getitesp(dev);
	tp = ip->tp;

	KDASSERT(tp);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, addr, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, addr, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case ITEIOCGBELL:
		ib = (struct itebell *)addr;
		ib->volume = bvolume;
		ib->pitch = bpitch;
		ib->msec = bmsec;
		return (0);
	case ITEIOCSBELL:
		ib = (struct itebell *)addr;

		if (ib->pitch > MAXBPITCH || ib->pitch < MINBPITCH ||
		    ib->volume > MAXBVOLUME || ib->msec > MAXBTIME)
			return (EINVAL);
		bvolume = ib->volume;
		bpitch = ib->pitch;
		bmsec = ib->msec;
		return (0);
	case ITEIOCSKMAP:
		if (addr == 0)
			return(EFAULT);
		bcopy(addr, &kbdmap, sizeof(struct kbdmap));
		return(0);
	case ITEIOCGKMAP:
		if (addr == NULL)
			return(EFAULT);
		bcopy(&kbdmap, addr, sizeof(struct kbdmap));
		return(0);
	case ITEIOCGREPT:
		irp = (struct iterepeat *)addr;
		irp->start = start_repeat_timeo;
		irp->next = next_repeat_timeo;
		return (0);
	case ITEIOCSREPT:
		irp = (struct iterepeat *)addr;
		if (irp->start < ITEMINREPEAT && irp->next < ITEMINREPEAT)
			return(EINVAL);
		start_repeat_timeo = irp->start;
		next_repeat_timeo = irp->next;
		return(0);
	}
#if NGRFCC > 0
	/* XXX */
	if (minor(dev) == 0) {
		error = ite_grf_ioctl(ip, cmd, addr, flag, p);
		if (error >= 0)
			return (error);
	}
#endif
	return (ENOTTY);
}

void
itestart(tp)
	struct tty *tp;
{
	struct clist *rbp;
	struct ite_softc *ip;
	u_char buf[ITEBURST];
	int s, len;

	ip = getitesp(tp->t_dev);

	KDASSERT(tp);

	s = spltty(); {
		if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
			goto out;

		tp->t_state |= TS_BUSY;
		rbp = &tp->t_outq;
		
		len = q_to_b(rbp, buf, ITEBURST);
	} splx(s);

	/* Here is a really good place to implement pre/jumpscroll() */
	ite_putstr(buf, len, tp->t_dev);

	s = spltty(); {
		tp->t_state &= ~TS_BUSY;
		/* we have characters remaining. */
		if (rbp->c_cc) {
			tp->t_state |= TS_TIMEOUT;
			timeout(ttrstrt, tp, 1);
		}
		/* wakeup we are below */
		if (rbp->c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t) rbp);
			}
			selwakeup(&tp->t_wsel);
		}
	      out:
	} splx(s);
}

int
ite_on(dev, flag)
	dev_t dev;
	int flag;
{
	struct ite_softc *ip;
	int unit;

	unit = ITEUNIT(dev);
	if (((1 << unit) & ite_confunits) == 0)
		return (ENXIO);
	
	ip = getitesp(dev); 

	/* force ite active, overriding graphics mode */
	if (flag & 1) {
		ip->flags |= ITE_ACTIVE;
		ip->flags &= ~(ITE_INGRF | ITE_INITED);
	}
	/* leave graphics mode */
	if (flag & 2) {
		ip->flags &= ~ITE_INGRF;
		if ((ip->flags & ITE_ACTIVE) == 0)
			return (0);
	}
	ip->flags |= ITE_ACTIVE;
	if (ip->flags & ITE_INGRF)
		return (0);
	iteinit(dev);
	return (0);
}

void
ite_off(dev, flag)
	dev_t dev;
	int flag;
{
	struct ite_softc *ip;

	ip = getitesp(dev);
	if (flag & 2)
		ip->flags |= ITE_INGRF;
	if ((ip->flags & ITE_ACTIVE) == 0)
		return;
	if ((flag & 1) ||
	    (ip->flags & (ITE_INGRF | ITE_ISCONS | ITE_INITED)) == ITE_INITED)
		SUBR_DEINIT(ip);
	/* XXX hmm grfon() I think wants this to  go inactive. */
	if ((flag & 2) == 0)
		ip->flags &= ~ITE_ACTIVE;
}

/* XXX called after changes made in underlying grf layer. */
/* I want to nuke this */
void
ite_reinit(dev)
	dev_t dev;
{
	struct ite_softc *ip;

	ip = getitesp(dev);
	ip->flags &= ~ITE_INITED;
	iteinit(dev);
}

int
ite_param(tp, t)
	struct tty *tp;
	struct termios *t;
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return (0);
}

void
ite_reset(ip)
	struct ite_softc *ip;
{
	int i;

	ip->curx = 0;
	ip->cury = 0;
	ip->attribute = ATTR_NOR;
	ip->save_curx = 0;
	ip->save_cury = 0;
	ip->save_attribute = ATTR_NOR;
	ip->ap = ip->argbuf;
	ip->emul_level = 0;
	ip->eightbit_C1 = 0;
	ip->top_margin = 0;
	ip->bottom_margin = ip->rows - 1;
	ip->inside_margins = 0;
	ip->linefeed_newline = 0;
	ip->auto_wrap = ite_default_wrap;
	ip->cursor_appmode = 0;
	ip->keypad_appmode = 0;
	ip->imode = 0;
	ip->key_repeat = 1;
	bzero(ip->tabs, ip->cols);
	for (i = 0; i < ip->cols; i++)
		ip->tabs[i] = ((i & 7) == 0);
}

/*
 * has to be global becuase of the shared filters.
 */
static u_char key_mod;
static u_char last_dead;

/* Used in console at startup only */
int
ite_cnfilter(c, caller)
	u_char c;
	enum caller caller;
{
	struct key key;
	u_char code, up, mask;
	int s, i;

	up = c & 0x80 ? 1 : 0;
	c &= 0x7f;
	code = 0;

	s = spltty();

	i = (int)c - KBD_LEFT_SHIFT;
	if (i >= 0 && i <= (KBD_RIGHT_META - KBD_LEFT_SHIFT)) {
		mask = 1 << i;
		if (up)
			key_mod &= ~mask;
		else
			key_mod |= mask;
		splx(s);
		return -1;
	}	

	if (up) {
		splx(s);
		return -1;
	}
	
	/* translate modifiers */
	if (key_mod & KBD_MOD_SHIFT) {
		if (key_mod & KBD_MOD_ALT)
			key = kbdmap.alt_shift_keys[c];
		else 
			key = kbdmap.shift_keys[c];
	} else if (key_mod & KBD_MOD_ALT)
		key = kbdmap.alt_keys[c];
	else {
		key = kbdmap.keys[c];
		/* if CAPS and key is CAPable (no pun intended) */
		if ((key_mod & KBD_MOD_CAPS) && (key.mode & KBD_MODE_CAPS))
			key = kbdmap.shift_keys[c];
	}
	code = key.code;

	/* if string return */
	if (key.mode & (KBD_MODE_STRING | KBD_MODE_KPAD)) {
		splx(s);
		return -1;
	}
	/* handle dead keys */
	if (key.mode & KBD_MODE_DEAD) {
		/* if entered twice, send accent itself */
		if (last_dead == key.mode & KBD_MODE_ACCMASK)
			last_dead = 0;
		else {
			last_dead = key.mode & KBD_MODE_ACCMASK;
			splx(s);
			return -1;
		}
	}
	if (last_dead) {
		/* can't apply dead flag to string-keys */
		if (code >= '@' && code < 0x7f)
			code =
			    acctable[KBD_MODE_ACCENT(last_dead)][code - '@'];
		last_dead = 0;
	}
	if (key_mod & KBD_MOD_CTRL)
		code &= 0x1f;
	if (key_mod & KBD_MOD_META)
		code |= 0x80;

	/* do console mapping. */
	code = code == '\r' ? '\n' : code;

	splx(s);
	return (code);
}

/* And now the old stuff. */

/* these are used to implement repeating keys.. */
static u_char last_char;
static u_char tout_pending;


static void
ite_sifilter(void *arg1, void *arg2)
{
	ite_filter((u_char)(size_t)arg1, (enum caller)(size_t)arg2);
}


/*ARGSUSED*/
static void
repeat_handler(arg)
	void *arg;
{
	tout_pending = 0;
	if (last_char) 
		add_sicallback(ite_sifilter, (void *)(size_t)last_char,
		    (void *)(size_t)ITEFILT_REPEATER);
}

void
ite_filter(c, caller)
	u_char c;
	enum caller caller;
{
	struct tty *kbd_tty;
	u_char code, *str, up, mask;
	struct key key;
	int s, i;

	if (kbd_ite == NULL || kbd_ite->tp == NULL)
		return;

	kbd_tty = kbd_ite->tp;

	/* have to make sure we're at spltty in here */
	s = spltty();

	/* 
	 * keyboard interrupts come at priority 2, while softint
	 * generated keyboard-repeat interrupts come at level 1.  So,
	 * to not allow a key-up event to get thru before a repeat for
	 * the key-down, we remove any outstanding callout requests..
	 */
	rem_sicallback(ite_sifilter);

	up = c & 0x80 ? 1 : 0;
	c &= 0x7f;
	code = 0;

	i = (int)c - KBD_LEFT_SHIFT;
	if (i >= 0 && i <= (KBD_RIGHT_META - KBD_LEFT_SHIFT)) {
		mask = 1 << i;
		if (up)
			key_mod &= ~mask;
		else
			key_mod |= mask;
		splx(s);
		return;
	}
	/* stop repeating on up event */
	if (up) {
		if (tout_pending) {
			untimeout(repeat_handler, 0);
			tout_pending = 0;
			last_char = 0;
		}
		splx(s);
		return;
	} else if (tout_pending && last_char != c) {
		/* different character, stop also */
		untimeout(repeat_handler, 0);
		tout_pending = 0;
		last_char = 0;
	}
	/* Safety button, switch back to ascii keymap. */
	if (key_mod == (KBD_MOD_LALT | KBD_MOD_LMETA) && c == 0x50) {
		bcopy(&ascii_kbdmap, &kbdmap, sizeof(struct kbdmap));

		splx(s);
		return;
#ifdef DDB
	} else if (key_mod == (KBD_MOD_LALT | KBD_MOD_LMETA) && c == 0x59) {
		extern int Debugger();
		Debugger();
		splx(s);
		return;
#endif
	}
	/* translate modifiers */
	if (key_mod & KBD_MOD_SHIFT) {
		if (key_mod & KBD_MOD_ALT)
			key = kbdmap.alt_shift_keys[c];
		else 
			key = kbdmap.shift_keys[c];
	} else if (key_mod & KBD_MOD_ALT)
		key = kbdmap.alt_keys[c];
	else {
		key = kbdmap.keys[c];
		/* if CAPS and key is CAPable (no pun intended) */
		if ((key_mod & KBD_MOD_CAPS) && (key.mode & KBD_MODE_CAPS))
			key = kbdmap.shift_keys[c];
	}
	code = key.code;

	/* 
	 * arrange to repeat the keystroke. By doing this at the level
	 * of scan-codes, we can have function keys, and keys that
	 * send strings, repeat too. This also entitles an additional
	 * overhead, since we have to do the conversion each time, but
	 * I guess that's ok.
	 */
	if (!tout_pending && caller == ITEFILT_TTY && kbd_ite->key_repeat) {
		tout_pending = 1;
		last_char = c;
		timeout(repeat_handler, 0, start_repeat_timeo * hz / 100);
	} else if (!tout_pending && caller == ITEFILT_REPEATER &&
	    kbd_ite->key_repeat) {
		tout_pending = 1;
		last_char = c;
		timeout(repeat_handler, 0, next_repeat_timeo * hz / 100);
	}
	/* handle dead keys */
	if (key.mode & KBD_MODE_DEAD) {
		/* if entered twice, send accent itself */
		if (last_dead == key.mode & KBD_MODE_ACCMASK)
			last_dead = 0;
		else {
			last_dead = key.mode & KBD_MODE_ACCMASK;
			splx(s);
			return;
		}
	}
	if (last_dead) {
		/* can't apply dead flag to string-keys */
		if (!(key.mode & KBD_MODE_STRING) && code >= '@' &&
		    code < 0x7f)
			code = acctable[KBD_MODE_ACCENT(last_dead)][code - '@'];
		last_dead = 0;
	}
	/* if not string, apply META and CTRL modifiers */
	if (!(key.mode & KBD_MODE_STRING)
	    && (!(key.mode & KBD_MODE_KPAD) ||
		(kbd_ite && !kbd_ite->keypad_appmode))) {
		if (key_mod & KBD_MOD_CTRL)
			code &= 0x1f;
		if (key_mod & KBD_MOD_META)
			code |= 0x80;
	} else if ((key.mode & KBD_MODE_KPAD) &&
	    (kbd_ite && kbd_ite->keypad_appmode)) {
		static char in[] = {
		    0x0f /* 0 */, 0x1d /* 1 */, 0x1e /* 2 */, 0x1f /* 3 */,
		    0x2d /* 4 */, 0x2e /* 5 */, 0x2f /* 6 */, 0x3d /* 7 */,
		    0x3e /* 8 */, 0x3f /* 9 */, 0x4a /* - */, 0x5e /* + */,
		    0x3c /* . */, 0x43 /* e */, 0x5a /* ( */, 0x5b /* ) */,
		    0x5c /* / */, 0x5d /* * */
		};
		static char *out = "pqrstuvwxymlnMPQRS";
		char *cp = index (in, c);

		/* 
		 * keypad-appmode sends SS3 followed by the above
		 * translated character
		 */
		(*linesw[kbd_tty->t_line].l_rint) (27, kbd_tty);
		(*linesw[kbd_tty->t_line].l_rint) ('O', kbd_tty);
		(*linesw[kbd_tty->t_line].l_rint) (out[cp - in], kbd_tty);
		splx(s);
		return;
	} else {
		/* *NO* I don't like this.... */
		static u_char app_cursor[] =
		{
		    3, 27, 'O', 'A',
		    3, 27, 'O', 'B',
		    3, 27, 'O', 'C',
		    3, 27, 'O', 'D'};

		str = kbdmap.strings + code;
		/* 
		 * if this is a cursor key, AND it has the default
		 * keymap setting, AND we're in app-cursor mode, switch
		 * to the above table. This is *nasty* !
		 */
		if (c >= 0x4c && c <= 0x4f && kbd_ite->cursor_appmode
		    && !bcmp(str, "\x03\x1b[", 3) &&
		    index("ABCD", str[3]))
			str = app_cursor + 4 * (str[3] - 'A');

		/* 
		 * using a length-byte instead of 0-termination allows
		 * to embed \0 into strings, although this is not used
		 * in the default keymap
		 */
		for (i = *str++; i; i--)
			(*linesw[kbd_tty->t_line].l_rint) (*str++, kbd_tty);
		splx(s);
		return;
	}
	(*linesw[kbd_tty->t_line].l_rint) (code, kbd_tty);

	splx(s);
	return;
}

/* helper functions, makes the code below more readable */
inline static void
ite_sendstr(str)
	char *str;
{
	struct tty *kbd_tty;

	kbd_tty = kbd_ite->tp;
	KDASSERT(kbd_tty);
	while (*str)
		(*linesw[kbd_tty->t_line].l_rint) (*str++, kbd_tty);
}

static void
alignment_display(ip)
	struct ite_softc *ip;
{
  int i, j;

  for (j = 0; j < ip->rows; j++)
    for (i = 0; i < ip->cols; i++)
      SUBR_PUTC(ip, 'E', j, i, ATTR_NOR);
  attrclr(ip, 0, 0, ip->rows, ip->cols);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
snap_cury(ip)
	struct ite_softc *ip;
{
  if (ip->inside_margins)
    {
      if (ip->cury < ip->top_margin)
	ip->cury = ip->top_margin;
      if (ip->cury > ip->bottom_margin)
	ip->cury = ip->bottom_margin;
    }
}

inline static void
ite_dnchar(ip, n)
     struct ite_softc *ip;
     int n;
{
  n = min(n, ip->cols - ip->curx);
  if (n < ip->cols - ip->curx)
    {
      SUBR_SCROLL(ip, ip->cury, ip->curx + n, n, SCROLL_LEFT);
      attrmov(ip, ip->cury, ip->curx + n, ip->cury, ip->curx,
	      1, ip->cols - ip->curx - n);
      attrclr(ip, ip->cury, ip->cols - n, 1, n);
    }
  while (n-- > 0)
    SUBR_PUTC(ip, ' ', ip->cury, ip->cols - n - 1, ATTR_NOR);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
ite_inchar(ip, n)
     struct ite_softc *ip;
     int n;
{
  n = min(n, ip->cols - ip->curx);
  if (n < ip->cols - ip->curx)
    {
      SUBR_SCROLL(ip, ip->cury, ip->curx, n, SCROLL_RIGHT);
      attrmov(ip, ip->cury, ip->curx, ip->cury, ip->curx + n,
	      1, ip->cols - ip->curx - n);
      attrclr(ip, ip->cury, ip->curx, 1, n);
    }
  while (n--)
    SUBR_PUTC(ip, ' ', ip->cury, ip->curx + n, ATTR_NOR);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
ite_clrtoeol(ip)
     struct ite_softc *ip;
{
  int y = ip->cury, x = ip->curx;
  if (ip->cols - x > 0)
    {
      SUBR_CLEAR(ip, y, x, 1, ip->cols - x);
      attrclr(ip, y, x, 1, ip->cols - x);
      SUBR_CURSOR(ip, DRAW_CURSOR);
    }
}

inline static void
ite_clrtobol(ip)
     struct ite_softc *ip;
{
  int y = ip->cury, x = min(ip->curx + 1, ip->cols);
  SUBR_CLEAR(ip, y, 0, 1, x);
  attrclr(ip, y, 0, 1, x);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
ite_clrline(ip)
     struct ite_softc *ip;
{
  int y = ip->cury;
  SUBR_CLEAR(ip, y, 0, 1, ip->cols);
  attrclr(ip, y, 0, 1, ip->cols);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}



inline static void
ite_clrtoeos(ip)
     struct ite_softc *ip;
{
  ite_clrtoeol(ip);
  if (ip->cury < ip->rows - 1)
    {
      SUBR_CLEAR(ip, ip->cury + 1, 0, ip->rows - 1 - ip->cury, ip->cols);
      attrclr(ip, ip->cury, 0, ip->rows - ip->cury, ip->cols);
      SUBR_CURSOR(ip, DRAW_CURSOR);
    }
}

inline static void
ite_clrtobos(ip)
     struct ite_softc *ip;
{
  ite_clrtobol(ip);
  if (ip->cury > 0)
    {
      SUBR_CLEAR(ip, 0, 0, ip->cury, ip->cols);
      attrclr(ip, 0, 0, ip->cury, ip->cols);
      SUBR_CURSOR(ip, DRAW_CURSOR);
    }
}

inline static void
ite_clrscreen(ip)
     struct ite_softc *ip;
{
  SUBR_CLEAR(ip, 0, 0, ip->rows, ip->cols);
  attrclr(ip, 0, 0, ip->rows, ip->cols);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}



inline static void
ite_dnline(ip, n)
     struct ite_softc *ip;
     int n;
{
  /* interesting.. if the cursor is outside the scrolling
     region, this command is simply ignored.. */
  if (ip->cury < ip->top_margin || ip->cury > ip->bottom_margin)
    return;

  n = min(n, ip->bottom_margin + 1 - ip->cury);
  if (n <= ip->bottom_margin - ip->cury)
    {
      SUBR_SCROLL(ip, ip->cury + n, 0, n, SCROLL_UP);
      attrmov(ip, ip->cury + n, 0, ip->cury, 0,
	      ip->bottom_margin + 1 - ip->cury - n, ip->cols);
    }
  SUBR_CLEAR(ip, ip->bottom_margin - n + 1, 0, n, ip->cols);
  attrclr(ip, ip->bottom_margin - n + 1, 0, n, ip->cols);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
ite_inline(ip, n)
     struct ite_softc *ip;
     int n;
{
  /* interesting.. if the cursor is outside the scrolling
     region, this command is simply ignored.. */
  if (ip->cury < ip->top_margin || ip->cury > ip->bottom_margin)
    return;

  n = min(n, ip->bottom_margin + 1 - ip->cury);
  if (n <= ip->bottom_margin - ip->cury)
    {
      SUBR_SCROLL(ip, ip->cury, 0, n, SCROLL_DOWN);
      attrmov(ip, ip->cury, 0, ip->cury + n, 0,
	      ip->bottom_margin + 1 - ip->cury - n, ip->cols);
    }
  SUBR_CLEAR(ip, ip->cury, 0, n, ip->cols);
  attrclr(ip, ip->cury, 0, n, ip->cols);
  SUBR_CURSOR(ip, DRAW_CURSOR);
}

inline static void
ite_lf (ip)
     struct ite_softc *ip;
{
  ++ip->cury;
  if ((ip->cury == ip->bottom_margin+1) || (ip->cury == ip->rows))
    {
      ip->cury--;
      SUBR_SCROLL(ip, ip->top_margin + 1, 0, 1, SCROLL_UP);
      ite_clrline(ip);
    }
  SUBR_CURSOR(ip, MOVE_CURSOR);
  clr_attr(ip, ATTR_INV);
}

inline static void
ite_crlf (ip)
     struct ite_softc *ip;
{
  ip->curx = 0;
  ite_lf (ip);
}

inline static void
ite_cr (ip)
     struct ite_softc *ip;
{
  if (ip->curx)
    {
      ip->curx = 0;
      SUBR_CURSOR(ip, MOVE_CURSOR);
    }
}

inline static void
ite_rlf (ip)
     struct ite_softc *ip;
{
  ip->cury--;
  if ((ip->cury < 0) || (ip->cury == ip->top_margin - 1))
    {
      ip->cury++;
      SUBR_SCROLL(ip, ip->top_margin, 0, 1, SCROLL_DOWN);
      ite_clrline(ip);
    }
  SUBR_CURSOR(ip, MOVE_CURSOR);
  clr_attr(ip, ATTR_INV);
}

inline static int
atoi (cp)
    const char *cp;
{
  int n;

  for (n = 0; *cp && *cp >= '0' && *cp <= '9'; cp++)
    n = n * 10 + *cp - '0';

  return n;
}

static char *
index (cp, ch)
    const char *cp;
    char ch;
{
  while (*cp && *cp != ch) cp++;
  return *cp ? (char *) cp : 0;
}



inline static int
ite_argnum (ip)
    struct ite_softc *ip;
{
  char ch;
  int n;

  /* convert argument string into number */
  if (ip->ap == ip->argbuf)
    return 1;
  ch = *ip->ap;
  *ip->ap = 0;
  n = atoi (ip->argbuf);
  *ip->ap = ch;
  
  return n;
}

inline static int
ite_zargnum (ip)
    struct ite_softc *ip;
{
  char ch;
  int n;

  /* convert argument string into number */
  if (ip->ap == ip->argbuf)
    return 0;
  ch = *ip->ap;
  *ip->ap = 0;
  n = atoi (ip->argbuf);
  *ip->ap = ch;
  
  return n;	/* don't "n ? n : 1" here, <CSI>0m != <CSI>1m ! */
}

inline static int
strncmp (a, b, l)
    const char *a, *b;
    int l;
{
  for (;l--; a++, b++)
    if (*a != *b)
      return *a - *b;
  return 0;
}

void
ite_putstr(s, len, dev)
	const char *s;
	int len;
	dev_t dev;
{
	struct ite_softc *ip;
	int i;
	
	ip = getitesp(dev);

	/* XXX avoid problems */
	if ((ip->flags & (ITE_ACTIVE|ITE_INGRF)) != ITE_ACTIVE)
	  	return;

	SUBR_CURSOR(ip, START_CURSOROPT);
	for (i = 0; i < len; i++)
		if (s[i])
			iteputchar(s[i], ip);
	SUBR_CURSOR(ip, END_CURSOROPT);
}

static void
iteprecheckwrap(ip)
	struct ite_softc *ip;
{
	if (ip->auto_wrap && ip->curx == ip->cols) {
		ip->curx = 0;
		clr_attr(ip, ATTR_INV);
		if (++ip->cury >= ip->bottom_margin + 1) {
			ip->cury = ip->bottom_margin;
			SUBR_CURSOR(ip, MOVE_CURSOR);
			SUBR_SCROLL(ip, ip->top_margin + 1, 0, 1, SCROLL_UP);
			ite_clrtoeol(ip);
		} else
			SUBR_CURSOR(ip, MOVE_CURSOR);
	}
}

static void
itecheckwrap(ip)
	struct ite_softc *ip;
{
#if 0
	if (++ip->curx == ip->cols) {
		if (ip->auto_wrap) {
			ip->curx = 0;
			clr_attr(ip, ATTR_INV);
			if (++ip->cury >= ip->bottom_margin + 1) {
				ip->cury = ip->bottom_margin;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				SUBR_SCROLL(ip, ip->top_margin + 1, 0, 1, SCROLL_UP);
				ite_clrtoeol(ip);
				return;
			}
		} else
			/* stay there if no autowrap.. */
			ip->curx--;
	}
#else
	if (ip->curx < ip->cols) {
		ip->curx++;
		SUBR_CURSOR(ip, MOVE_CURSOR);
	}
#endif
}

void
iteputchar(c, ip)
	register int c;
	struct ite_softc *ip;
{
	struct tty *kbd_tty;
	int n, x, y;
	char *cp;

	if (kbd_ite == NULL)
		kbd_tty = NULL;
	else
		kbd_tty = kbd_ite->tp;

	if (ip->escape) {
		switch (ip->escape) {
		case ESC:
			switch (c) {
				/*
				 * first 7bit equivalents for the
				 * 8bit control characters
				 */
			case 'D':
				c = IND;
				ip->escape = 0;
				break;
				/*
				 * and fall into the next
				 * switch below (same for all `break')
				 */
			case 'E':
				c = NEL;
				ip->escape = 0;
				break;
			case 'H':
				c = HTS;
				ip->escape = 0;
				break;
			case 'M':
				c = RI;
				ip->escape = 0;
				break;
			case 'N':
				c = SS2;
				ip->escape = 0;
				break;
			case 'O':
				c = SS3;
				ip->escape = 0;
				break;
			case 'P':
				c = DCS;
				ip->escape = 0;
				break;
			case '[':
				c = CSI;
				ip->escape = 0;
				break;
			case '\\':
				c = ST;
				ip->escape = 0;
				break;
			case ']':
				c = OSC;
				ip->escape = 0;
				break;
			case '^':
				c = PM;
				ip->escape = 0;
				break;
			case '_':
				c = APC;
				ip->escape = 0;
				break;
				/* introduces 7/8bit control */
			case ' ':
				/* can be followed by either F or G */
				ip->escape = ' ';
				break;
			/*
			 * a lot of character set selections, not yet
			 * used... 94-character sets:
			 */
			case '(':	/* G0 */
			case ')':	/* G1 */
				ip->escape = c;
				return;
			case '*':	/* G2 */
			case '+':	/* G3 */
			case 'B':	/* ASCII */
			case 'A':	/* ISO latin 1 */
			case '<':	/* user preferred suplemental */
			case '0':	/* dec special graphics */
			/*
			 * 96-character sets:
			 */
			case '-':	/* G1 */
			case '.':	/* G2 */
			case '/':	/* G3 */
			/*
			 * national character sets:
			 */
			case '4':	/* dutch */
			case '5':
			case 'C':	/* finnish */
			case 'R':	/* french */
			case 'Q':	/* french canadian */
			case 'K':	/* german */
			case 'Y':	/* italian */
			case '6':	/* norwegian/danish */
				/*
				 * note: %5 and %6 are not supported (two
				 * chars..)
				 */
				ip->escape = 0;
				/* just ignore for now */
				return;
			/*
			 * locking shift modes (as you might guess, not
			 * yet supported..)
			 */
			case '`':
				ip->GR = ip->G1;
				ip->escape = 0;
				return;
			case 'n':
				ip->GL = ip->G2;
				ip->escape = 0;
				return;
			case '}':
				ip->GR = ip->G2;
				ip->escape = 0;
				return;
			case 'o':
				ip->GL = ip->G3;
				ip->escape = 0;
				return;
			case '|':
				ip->GR = ip->G3;
				ip->escape = 0;
				return;
			case '#':
				/* font width/height control */
				ip->escape = '#';
				return;
			case 'c':
				/* hard terminal reset .. */
				ite_reset(ip);
				SUBR_CURSOR(ip, MOVE_CURSOR);
				ip->escape = 0;
				return;
			case '7':
				ip->save_curx = ip->curx;
				ip->save_cury = ip->cury;
				ip->save_attribute = ip->attribute;
				ip->escape = 0;
				return;
			case '8':
				ip->curx = ip->save_curx;
				ip->cury = ip->save_cury;
				ip->attribute = ip->save_attribute;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				ip->escape = 0;
				return;
			case '=':
				ip->keypad_appmode = 1;
				ip->escape = 0;
				return;
			case '>':
				ip->keypad_appmode = 0;
				ip->escape = 0;
				return;
			case 'Z':	/* request ID */
				/* XXX not clean */
				if (ip->emul_level == EMUL_VT100)
					ite_sendstr("\033[?61;0c");
				else
					ite_sendstr("\033[?63;0c");
				ip->escape = 0;
				return;
			default:
				/*
				 * default catch all for not recognized ESC
				 * sequences
				 */
				ip->escape = 0;
				return;
			}
			break;
		case '(':
		case ')':
			ip->escape = 0;
			return;
		case ' ':
			switch (c) {
			case 'F':
				ip->eightbit_C1 = 0;
				ip->escape = 0;
				return;
			case 'G':
				ip->eightbit_C1 = 1;
				ip->escape = 0;
				return;
			default:
				/* not supported */
				ip->escape = 0;
				return;
			}
			break;
		case '#':
			switch (c) {
			case '5':
				/* single height, single width */
				ip->escape = 0;
				return;
			case '6':
				/* double width, single height */
				ip->escape = 0;
				return;
			case '3':
				/* top half */
				ip->escape = 0;
				return;
			case '4':
				/* bottom half */
				ip->escape = 0;
				return;
			case '8':
				/* screen alignment pattern... */
				alignment_display(ip);
				ip->escape = 0;
				return;
			default:
				ip->escape = 0;
				return;
			}
			break;
		case CSI:
			/* the biggie... */
			switch (c) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case ';':
			case '\"':
			case '$':
			case '>':
				if (ip->ap < ip->argbuf + MAX_ARGSIZE)
					*ip->ap++ = c;
				return;
			case BS:
				/*
				 * you wouldn't believe such perversion is
				 * possible? it is.. BS is allowed in between
				 * cursor sequences (at least), according to
				 * vttest..
				 */
				if (--ip->curx < 0)
					ip->curx = 0;
				else
					SUBR_CURSOR(ip, MOVE_CURSOR);
				break;
			case 'p':
				*ip->ap = 0;
				if (!strncmp(ip->argbuf, "61\"", 3))
					ip->emul_level = EMUL_VT100;
				else if (!strncmp(ip->argbuf, "63;1\"", 5)
				    || !strncmp(ip->argbuf, "62;1\"", 5))
					ip->emul_level = EMUL_VT300_7;
				else
					ip->emul_level = EMUL_VT300_8;
				ip->escape = 0;
				return;
			case '?':
				*ip->ap = 0;
				ip->escape = '?';
				ip->ap = ip->argbuf;
				return;
			case 'c':
				*ip->ap = 0;
				if (ip->argbuf[0] == '>') {
					ite_sendstr("\033[>24;0;0;0c");
				} else
					switch (ite_zargnum(ip)) {
					case 0:
						/*
						 * primary DA request, send
						 * primary DA response
						 */
						if (ip->emul_level
						    == EMUL_VT100)
							ite_sendstr(
							    "\033[?1;1c");
						else
							ite_sendstr(
							    "\033[?63;1c");
						break;
					}
				ip->escape = 0;
				return;
			case 'n':
				switch (ite_zargnum(ip)) {
				case 5:
					/* no malfunction */
					ite_sendstr("\033[0n");
					break;
				case 6:
					/* cursor position report */
					sprintf(ip->argbuf, "\033[%d;%dR",
					    ip->cury + 1, ip->curx + 1);
					ite_sendstr(ip->argbuf);
					break;
				}
				ip->escape = 0;
				return;
			case 'x':
				switch (ite_zargnum(ip)) {
				case 0:
					/* Fake some terminal parameters.  */
					ite_sendstr("\033[2;1;1;112;112;1;0x");
					break;
				case 1:
					ite_sendstr("\033[3;1;1;112;112;1;0x");
					break;
				}
				ip->escape = 0;
				return;
			case 'g':
				switch (ite_zargnum(ip)) {
				case 0:
					if (ip->curx < ip->cols)
						ip->tabs[ip->curx] = 0;
					break;
				case 3:
					for (n = 0; n < ip->cols; n++)
						ip->tabs[n] = 0;
					break;
				}
				ip->escape = 0;
				return;
			case 'h':
			case 'l':
				n = ite_zargnum(ip);
				switch (n) {
				case 4:
					/* insert/replace mode */
					ip->imode = (c == 'h');
					break;
				case 20:
					ip->linefeed_newline = (c == 'h');
					break;
				}
				ip->escape = 0;
				return;
			case 'M':
				ite_dnline(ip, ite_argnum(ip));
				ip->escape = 0;
				return;
			case 'L':
				ite_inline(ip, ite_argnum(ip));
				ip->escape = 0;
				return;
			case 'P':
				ite_dnchar(ip, ite_argnum(ip));
				ip->escape = 0;
				return;
			case '@':
				ite_inchar(ip, ite_argnum(ip));
				ip->escape = 0;
				return;
			case 'G':
				/*
				 * this one was *not* in my vt320 manual but in
				 * a vt320 termcap entry.. who is right? It's
				 * supposed to set the horizontal cursor
				 * position.
				 */
				*ip->ap = 0;
				x = atoi(ip->argbuf);
				if (x)
					x--;
				ip->curx = min(x, ip->cols - 1);
				ip->escape = 0;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'd':
				/*
				 * same thing here, this one's for setting the
				 * absolute vertical cursor position. Not
				 * documented...
				 */
				*ip->ap = 0;
				y = atoi(ip->argbuf);
				if (y)
					y--;
				if (ip->inside_margins)
					y += ip->top_margin;
				ip->cury = min(y, ip->rows - 1);
				ip->escape = 0;
				snap_cury(ip);
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'H':
			case 'f':
				*ip->ap = 0;
				y = atoi(ip->argbuf);
				x = 0;
				cp = index(ip->argbuf, ';');
				if (cp)
					x = atoi(cp + 1);
				if (x)
					x--;
				if (y)
					y--;
				if (ip->inside_margins)
					y += ip->top_margin;
				ip->cury = min(y, ip->rows - 1);
				ip->curx = min(x, ip->cols - 1);
				ip->escape = 0;
				snap_cury(ip);
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'A':
				n = ite_argnum(ip);
				n = ip->cury - (n ? n : 1);
				if (n < 0)
					n = 0;
				if (ip->inside_margins)
					n = max(ip->top_margin, n);
				else if (n == ip->top_margin - 1)
					/*
					 * allow scrolling outside region, but
					 * don't scroll out of active region
					 * without explicit CUP
					 */
					n = ip->top_margin;
				ip->cury = n;
				ip->escape = 0;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'B':
				n = ite_argnum(ip);
				n = ip->cury + (n ? n : 1);
				n = min(ip->rows - 1, n);
				if (ip->inside_margins)
					n = min(ip->bottom_margin, n);
				else if (n == ip->bottom_margin + 1)
					/*
					 * allow scrolling outside region, but
					 * don't scroll out of active region
					 * without explicit CUP
					 */
					n = ip->bottom_margin;
				ip->cury = n;
				ip->escape = 0;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'C':
				n = ite_argnum(ip);
				n = n ? n : 1;
				ip->curx = min(ip->curx + n, ip->cols - 1);
				ip->escape = 0;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'D':
				n = ite_argnum(ip);
				n = n ? n : 1;
				n = ip->curx - n;
				ip->curx = n >= 0 ? n : 0;
				ip->escape = 0;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				clr_attr(ip, ATTR_INV);
				return;
			case 'J':
				*ip->ap = 0;
				n = ite_zargnum(ip);
				if (n == 0)
					ite_clrtoeos(ip);
				else if (n == 1)
					ite_clrtobos(ip);
				else if (n == 2)
					ite_clrscreen(ip);
				ip->escape = 0;
				return;
			case 'K':
				n = ite_zargnum(ip);
				if (n == 0)
					ite_clrtoeol(ip);
				else if (n == 1)
					ite_clrtobol(ip);
				else if (n == 2)
					ite_clrline(ip);
				ip->escape = 0;
				return;
			case 'X':
				n = ite_argnum(ip) - 1;
				n = min(n, ip->cols - 1 - ip->curx);
				for (; n >= 0; n--) {
					attrclr(ip, ip->cury, ip->curx + n, 1, 1);
					SUBR_PUTC(ip, ' ', ip->cury, ip->curx + n, ATTR_NOR);
				}
				ip->escape = 0;
				return;
			case '}':
			case '`':
				/* status line control */
				ip->escape = 0;
				return;
			case 'r':
				*ip->ap = 0;
				x = atoi(ip->argbuf);
				x = x ? x : 1;
				y = ip->rows;
				cp = index(ip->argbuf, ';');
				if (cp) {
					y = atoi(cp + 1);
					y = y ? y : ip->rows;
				}
				if (y - x < 2) {
					/*
					 * if illegal scrolling region, reset
					 * to defaults
					 */
					x = 1;
					y = ip->rows;
				}
				x--;
				y--;
				ip->top_margin = min(x, ip->rows - 1);
				ip->bottom_margin = min(y, ip->rows - 1);
				if (ip->inside_margins) {
					ip->cury = ip->top_margin;
					ip->curx = 0;
					SUBR_CURSOR(ip, MOVE_CURSOR);
				}
				ip->escape = 0;
				return;
			case 'm':
				/* big attribute setter/resetter */
				{ char *cp;
				*ip->ap = 0;
				/* kludge to make CSIm work (== CSI0m) */
				if (ip->ap == ip->argbuf)
					ip->ap++;
				for (cp = ip->argbuf; cp < ip->ap;) {
					switch (*cp) {
					case 0:
					case '0':
						clr_attr(ip, ATTR_ALL);
						cp++;
						break;

					case '1':
						set_attr(ip, ATTR_BOLD);
						cp++;
						break;

					case '2':
						switch (cp[1]) {
						case '2':
							clr_attr(ip, ATTR_BOLD);
							cp += 2;
							break;

						case '4':
							clr_attr(ip, ATTR_UL);
							cp += 2;
							break;

						case '5':
							clr_attr(ip, ATTR_BLINK);
							cp += 2;
							break;

						case '7':
							clr_attr(ip, ATTR_INV);
							cp += 2;
							break;

						default:
							cp++;
							break;
						}
						break;

					case '4':
						set_attr(ip, ATTR_UL);
						cp++;
						break;

					case '5':
						set_attr(ip, ATTR_BLINK);
						cp++;
						break;

					case '7':
						set_attr(ip, ATTR_INV);
						cp++;
						break;

					default:
						cp++;
						break;
					}
				}
				ip->escape = 0;
				return; }
			case 'u':
				/* DECRQTSR */
				ite_sendstr("\033P\033\\");
				ip->escape = 0;
				return;
			default:
				ip->escape = 0;
				return;
			}
			break;
		case '?':	/* CSI ? */
			switch (c) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case ';':
			case '\"':
			case '$':
				/*
				 * Don't fill the last character;
				 * it's needed.
				 * XXX yeah, where ??
				 */
				if (ip->ap < ip->argbuf + MAX_ARGSIZE - 1)
					*ip->ap++ = c;
				return;
			case 'n':
				*ip->ap = 0;
				if (ip->ap == &ip->argbuf[2]) {
					if (!strncmp(ip->argbuf, "15", 2))
						/* printer status: no printer */
						ite_sendstr("\033[13n");

					else if (!strncmp(ip->argbuf, "25", 2))
						/* udk status */
						ite_sendstr("\033[20n");

					else if (!strncmp(ip->argbuf, "26", 2))
						/* keyboard dialect: US */
						ite_sendstr("\033[27;1n");
				}
				ip->escape = 0;
				return;
			case 'h':
			case 'l':
				n = ite_zargnum(ip);
				switch (n) {
				case 1:
					ip->cursor_appmode = (c == 'h');
					break;
				case 3:
					/* 132/80 columns (132 == 'h') */
					break;
				case 4:	/* smooth scroll */
					break;
				case 5:
					/*
					 * light background (=='h') /dark
					 * background(=='l')
					 */
					break;
				case 6:	/* origin mode */
					ip->inside_margins = (c == 'h');
					ip->curx = 0;
					ip->cury = ip->inside_margins ?
					    ip->top_margin : 0;
					SUBR_CURSOR(ip, MOVE_CURSOR);
					break;
				case 7:	/* auto wraparound */
					ip->auto_wrap = (c == 'h');
					break;
				case 8:	/* keyboard repeat */
					ip->key_repeat = (c == 'h');
					break;
				case 20:	/* newline mode */
					ip->linefeed_newline = (c == 'h');
					break;
				case 25:	/* cursor on/off */
					SUBR_CURSOR(ip, (c == 'h') ?
					    DRAW_CURSOR : ERASE_CURSOR);
					break;
				}
				ip->escape = 0;
				return;
			default:
				ip->escape = 0;
				return;
			}
			break;
		default:
			ip->escape = 0;
			return;
		}
	}
	switch (c) {
	case VT:		/* VT is treated like LF */
	case FF:		/* so is FF */
	case LF:
		/*
		 * cr->crlf distinction is done here, on output, not on input!
		 */
		if (ip->linefeed_newline)
			ite_crlf(ip);
		else
			ite_lf(ip);
		break;
	case CR:
		ite_cr(ip);
		break;
	case BS:
		if (--ip->curx < 0)
			ip->curx = 0;
		else
			SUBR_CURSOR(ip, MOVE_CURSOR);
		break;
	case HT:
		for (n = ip->curx + 1; n < ip->cols; n++) {
			if (ip->tabs[n]) {
				ip->curx = n;
				SUBR_CURSOR(ip, MOVE_CURSOR);
				break;
			}
		}
		break;
	case BEL:
		if (kbd_tty && kbd_ite && kbd_ite->tp == kbd_tty)
			ite_bell();
		break;
	case SO:
		ip->GL = ip->G1;
		break;
	case SI:
		ip->GL = ip->G0;
		break;
	case ENQ:
		/* send answer-back message !! */
		break;
	case CAN:
		ip->escape = 0;	/* cancel any escape sequence in progress */
		break;
	case SUB:
		ip->escape = 0;	/* dito, but see below */
		/* should also display a reverse question mark!! */
		break;
	case ESC:
		ip->escape = ESC;
		break;
	/*
	 * now it gets weird.. 8bit control sequences..
	 */
	case IND:
		/* index: move cursor down, scroll */
		ite_lf(ip);
		break;
	case NEL:
		/* next line. next line, first pos. */
		ite_crlf(ip);
		break;
	case HTS:
		/* set horizontal tab */
		if (ip->curx < ip->cols)
			ip->tabs[ip->curx] = 1;
		break;
	case RI:
		/* reverse index */
		ite_rlf(ip);
		break;
	case SS2:
		/* go into G2 for one character */
		/* not yet supported */
		break;
	case SS3:
		/* go into G3 for one character */
		break;
	case DCS:
		/* device control string introducer */
		ip->escape = DCS;
		ip->ap = ip->argbuf;
		break;
	case CSI:
		/* control sequence introducer */
		ip->escape = CSI;
		ip->ap = ip->argbuf;
		break;
	case ST:
		/* string terminator */
		/* ignore, if not used as terminator */
		break;
	case OSC:
		/* 
		 * introduces OS command. Ignore everything
		 * upto ST
		 */
		ip->escape = OSC;
		break;
	case PM:
		/* privacy message, ignore everything upto ST */
		ip->escape = PM;
		break;
	case APC:
		/*
		 * application program command, ignore * everything upto ST
		 */
		ip->escape = APC;
		break;
	default:
		if ((c & 0x7f) < ' ' || c == DEL)
			break;
		if (ip->imode)
			ite_inchar(ip, 1);
		iteprecheckwrap(ip);
#ifdef DO_WEIRD_ATTRIBUTES
		if ((ip->attribute & ATTR_INV) || attrtest(ip, ATTR_INV)) {
			attrset(ip, ATTR_INV);
			SUBR_PUTC(ip, c, ip->cury, ip->curx, ATTR_INV);
		} else
			SUBR_PUTC(ip, c, ip->cury, ip->curx, ATTR_NOR);
#else
		SUBR_PUTC(ip, c, ip->cury, ip->curx, ip->attribute);
#endif
		SUBR_CURSOR(ip, DRAW_CURSOR);
		itecheckwrap(ip);
		break;
	}
}

