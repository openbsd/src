/*	$OpenBSD: pcvt_drv.c,v 1.27 1999/11/25 21:00:35 aaron Exp $	*/
/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore and Scott Turner.
 *
 * Copyright (c) 1993 Charles Hannum.
 *
 * All rights reserved.
 *
 * Parts of this code regarding the NetBSD interface were written
 * by Charles Hannum. Parts regarding OpenBSD written by Aaron Campbell.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore, Joerg Wunsch, Scott Turner
 *	and Charles Hannum.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#)pcvt_drv.c, 3.32, Last Edit-Date: [Tue Oct  3 11:19:47 1995]
 *
 */

/*---------------------------------------------------------------------------*
 *
 *      pcvt_drv.c      VT220 Driver Main Module / OS - Interface
 *      ---------------------------------------------------------
 *      -hm     ------------ Release 3.00 --------------
 *      -hm     integrating NetBSD-current patches
 *      -hm     adding ttrstrt() proto for NetBSD 0.9
 *      -hm     kernel/console output cursor positioning fixed
 *      -hm     kernel/console output switches optional to screen 0
 *      -hm     FreeBSD 1.1 porting
 *      -hm     the NetBSD 0.9 compiler detected a nondeclared var which was
 *               NOT detected by neither the NetBSD-current nor FreeBSD 1.x!
 *      -hm     including Michael's keyboard fifo code
 *      -hm     Joergs patch for FreeBSD tty-malloc code
 *      -hm     adjustments for NetBSD-current
 *      -hm     FreeBSD bugfix from Joerg re timeout/untimeout casts
 *      -jw     including Thomas Gellekum's FreeBSD 1.1.5 patch
 *      -hm     adjusting #if's for NetBSD-current
 *      -hm     applying Joerg's patch for FreeBSD 2.0
 *      -hm     patch from Onno & Martin for NetBSD-current (post 1.0)
 *      -hm     some adjustments for NetBSD 1.0
 *      -hm     NetBSD PR #400: screen size report for new session
 *      -hm     patch from Rafael Boni/Lon Willett for NetBSD-current
 *      -hm     bell patch from Thomas Eberhardt for NetBSD
 *      -hm     multiple X server bugfixes from Lon Willett
 *      -hm     patch from joerg - pcdevtotty for FreeBSD pre-2.1
 *      -hm     delay patch from Martin Husemann after port-i386 ml-discussion
 *      -jw     add some code to provide more FreeBSD pre-2.1 support
 *      -hm     patches from Michael for NetBSD-current (Apr/21/95) support
 *      -hm     merged in changes from FreeBSD 2.0.5-RELEASE
 *      -hm     NetBSD-current patches from John Kohl
 *      -hm     ---------------- Release 3.30 -----------------------
 *      -hm     patch from Joerg in pcopen() to make mouse emulator work again
 *      -hm     patch from Frank van der Linden for keyboard state per VT
 *      -hm     no TS_ASLEEP anymore in FreeBSD 2.1.0 SNAP 950928
 *      -hm     ---------------- Release 3.32 -----------------------
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#define EXTERN			/* allocate mem */

#include "pcvt_hdr.h"		/* global include */

static void vgapelinit(void);	/* read initial VGA DAC palette */

void pccnpollc(Dev_t, int);
int pcprobe(struct device *, void *, void *);
void pcattach(struct device *, struct device *, void *);

int
pcprobe(struct device *parent, void *match, void *aux)
{
	kbd_code_init();

	((struct isa_attach_args *)aux)->ia_iosize = 16;
	return 1;
}

void
pcattach(struct device *parent, struct device *self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct vt_softc *sc = (void *) self;
	int maj;
	int i;

	if(do_initialization)
		vt_coldinit();

	vt_coldmalloc();		/* allocate memory for screens */

	printf(": ");

	switch(adaptor_type) {
		case MDA_ADAPTOR:
			printf("mda");
			break;
		case CGA_ADAPTOR:
			printf("cga");
			break;
		case EGA_ADAPTOR:
			printf("ega");
			break;
		case VGA_ADAPTOR:
			printf("%s, ", (char *)vga_string(vga_type));
			if(can_do_132col)
				printf("80/132 col");
			else
				printf("80 col");
			vgapelinit();
			break;
		default:
			printf("unknown");
			break;
	}

	if (color == 0)
		printf(", mono");
	else
		printf(", color");

	printf(", %d scr, ", totalscreens);

	switch(keyboard_type) {
		case KB_AT:
			printf("at-");
			break;
		case KB_MFII:
			printf("mf2-");
			break;
		default:
			printf("unknown ");
			break;
	}

	printf("kbd\n");

	for (maj = 0; maj < nchrdev; maj++) {
		if ((u_int)cdevsw[maj].d_open == (u_int)pcopen)
			break;
	}

	for (i = 0; i < totalscreens; i++) {
		vs[i].vs_tty = ttymalloc();
		vs[i].vs_tty->t_dev = makedev(maj, i);
		tty_attach(vs[i].vs_tty);
	}

	pcconsp = vs[0].vs_tty;

	async_update();

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_TTY, pcintr, (void *)0, sc->sc_dev.dv_xname);

	/*
 	 * Look for children of the keyboard controller.
	 * XXX Really should decouple keyboard controller
	 * from the console code.
	 */
	while (config_found(self, ia->ia_ic, NULL) != NULL)
		/* will break when no more children */ ;
}

/* had a look at the friedl driver */

struct tty *
get_pccons(Dev_t dev)
{
	register int i = minor(dev);

	if(i >= PCVT_NSCREENS)
		return(NULL);

	return(vs[i].vs_tty);
}


/*---------------------------------------------------------------------------*
 *		/dev/ttyc0, /dev/ttyc1, etc.
 *---------------------------------------------------------------------------*/
int
pcopen(Dev_t dev, int flag, int mode, struct proc *p)
{
	register struct tty *tp;
	register struct video_state *vsx;
	int s, retval;
	int winsz = 0;
	int i = minor(dev);

	vsx = &vs[i];

  	if((tp = get_pccons(dev)) == NULL)
		return ENXIO;

	vsx->openf++;

	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	}
	else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	tp->t_state |= TS_CARR_ON;
	tp->t_cflag |= CLOCAL;	/* cannot be a modem (:-) */

	if ((tp->t_state & TS_ISOPEN) == 0)	/* is this a "cold" open ? */
		winsz = 1;			/* yes, set winsize later  */

	retval = ((*linesw[tp->t_line].l_open)(dev, tp));

	if (winsz == 1) {
		/*
		 * The line discipline has clobbered t_winsize if TS_ISOPEN
	         * was clear. (NetBSD PR #400 from Bill Sommerfeld)
	         * We have to do this after calling the open routine, because
	         * it does some other things in other/older *BSD releases -hm
		 */

		s = spltty();

		tp->t_winsize.ws_col = vsx->maxcol;
		tp->t_winsize.ws_row = vsx->screen_rows;
		tp->t_winsize.ws_xpixel = (vsx->maxcol == 80)? 720: 1056;
		tp->t_winsize.ws_ypixel = 400;

		splx(s);
	}

	return(retval);
}

int
pcclose(Dev_t dev, int flag, int mode, struct proc *p)
{
	register struct tty *tp;
	register struct video_state *vsx;
	int i = minor(dev);

	vsx = &vs[i];

	if((tp = get_pccons(dev)) == NULL)
		return ENXIO;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	vsx->openf = 0;

	reset_usl_modes(vsx);

	return(0);
}

int
pcread(Dev_t dev, struct uio *uio, int flag)
{
	register struct tty *tp;

	if((tp = get_pccons(dev)) == NULL)
		return ENXIO;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
pcwrite(Dev_t dev, struct uio *uio, int flag)
{
	register struct tty *tp;

	if((tp = get_pccons(dev)) == NULL)
		return ENXIO;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
pctty(Dev_t dev)
{
	register struct tty *tp;

	if((tp = get_pccons(dev)) == NULL)
		return 0;

	return tp;
}

int
pcioctl(Dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	register int error;
	register struct tty *tp;

	if((tp = get_pccons(dev)) == NULL)
		return(ENXIO);

	/* note that some ioctl's are global, e.g.  KBSTPMAT: There is
	 * only one keyboard and different repeat rates for instance between
	 * sessions are a suspicious wish. If you really need this make the
	 * appropriate variables arrays
	 */

	if((error = usl_vt_ioctl(dev, cmd, data, flag, p)) >= 0)
		return (error == PCVT_ERESTART) ? ERESTART : error;

	if((error = kbdioctl(dev,cmd,data,flag)) >= 0)
		return error;

	if((error = vgaioctl(dev,cmd,data,flag)) >= 0)
		return error;

	if((error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p)) >= 0)
		return (error);

	if((error = ttioctl(tp, cmd, data, flag, p)) >= 0)
		return (error);

	return (ENOTTY);
}

int
pcmmap(Dev_t dev, int offset, int nprot)
{
	if ((u_int)offset > 0x20000)
		return -1;

	return i386_btop((0xa0000 + offset));
}


/*---------------------------------------------------------------------------*
 *
 *	handle a keyboard receive interrupt
 *
 *	NOTE: the keyboard is multiplexed by means of "pcconsp"
 *	between virtual screens. pcconsp - switching is done in
 *	the vgapage() routine
 *
 *---------------------------------------------------------------------------*/

#if PCVT_KBD_FIFO

u_char pcvt_kbd_fifo[PCVT_KBD_FIFO_SZ];
int pcvt_kbd_wptr = 0;
int pcvt_kbd_rptr = 0;
short pcvt_kbd_count= 0;
static u_char pcvt_timeout_scheduled;

static void
pcvt_timeout(void *arg)
{
	int s;
	u_char *cp;

	pcvt_timeout_scheduled = 0;

#if PCVT_SCREENSAVER
	pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

	while (pcvt_kbd_count) {
		if ((cp = sgetc(1)) && (vs[current_video_screen].openf)) {
#if PCVT_NULLCHARS
			if(*cp == '\0') {
				/* pass a NULL character */
				(*linesw[pcconsp->t_line].l_rint)('\0',
				    pcconsp);
			}
/* XXX */		else
#endif /* PCVT_NULLCHARS */

			while (*cp)
				(*linesw[pcconsp->t_line].l_rint)(*cp++ & 0xff,
				    pcconsp);
		}

		s = spltty();

		if (!pcvt_kbd_count)
			pcvt_timeout_scheduled = 0;

		splx(s);
	}

	return;
}
#endif

int
pcintr(void *arg)
{

#if PCVT_KBD_FIFO
	u_char	dt;
	u_char	ret = -1;
	int	s;

#else /* !PCVT_KBD_FIFO */
	u_char	*cp;
#endif /* PCVT_KBD_FIFO */

#if PCVT_SCREENSAVER
	pcvt_scrnsv_reset();
#endif /* PCVT_SCREENSAVER */

#if PCVT_KBD_FIFO
	if (kbd_polling) {
		if(sgetc(1) == 0)
			return -1;
		else
			return 1;
	}

	while (inb(CONTROLLER_CTRL) & STATUS_OUTPBF) {	/* check 8042 buffer */
		ret = 1;				/* got something */

		PCVT_KBD_DELAY();			/* 7 us delay */

		dt = inb(CONTROLLER_DATA);		/* get it 8042 data */

		if (pcvt_kbd_count >= PCVT_KBD_FIFO_SZ)	/* fifo overflow ? */
			log (LOG_WARNING, "pcvt: keyboard buffer overflow\n");
		else {
			pcvt_kbd_fifo[pcvt_kbd_wptr++] = dt; /* data -> fifo */

			s = spltty();	/* XXX necessary ? */
			pcvt_kbd_count++;		/* update fifo count */
			splx(s);

			if (pcvt_kbd_wptr >= PCVT_KBD_FIFO_SZ)
				pcvt_kbd_wptr = 0;	/* wraparound pointer */
		}
	}

	if (ret == 1) {	/* got data from keyboard ? */
		if (!pcvt_timeout_scheduled) {	/* if not already active .. */
			s = spltty();
			pcvt_timeout_scheduled = 1;	/* flag active */
			timeout((TIMEOUT_FUNC_T)pcvt_timeout, (caddr_t) 0, 1);
			splx(s);
		}
	}
	return (ret);

#else /* !PCVT_KBD_FIFO */

	if((cp = sgetc(1)) == 0)
		return -1;

	if (kbd_polling)
		return 1;

	if(!(vs[current_video_screen].openf))	/* XXX was vs[minor(dev)] */
		return 1;

#if PCVT_NULLCHARS
	if(*cp == '\0') {
		/* pass a NULL character */
		(*linesw[pcconsp->t_line].l_rint)('\0', pcconsp);
		return 1;
	}
#endif /* PCVT_NULLCHARS */

	while (*cp)
		(*linesw[pcconsp->t_line].l_rint)(*cp++ & 0xff, pcconsp);
	return 1;

#endif /* PCVT_KBD_FIFO */
}


void
pcstart(register struct tty *tp)
{
	int s, len;
	u_char buf[PCVT_PCBURST];

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;

	if (tp->t_outq.c_cc == 0 && tp->t_wsel.si_selpid == 0) {
		async_update();
		goto low;
	}

	tp->t_state |= TS_BUSY;

	splx(s);

	/*
	 * We need to do this outside spl since it could be fairly
	 * expensive and we don't want our serial ports to overflow.
	 */

	while ((len = q_to_b(&tp->t_outq, buf, PCVT_PCBURST)) != 0) {
		if (vs[minor(tp->t_dev)].scrolling)
			sgetc(31337);
		sput(&buf[0], 0, len, minor(tp->t_dev));
	}

	s = spltty();

	tp->t_state &= ~TS_BUSY;

	tp->t_state |= TS_TIMEOUT;
	timeout(ttrstrt, tp, 1);

	if (tp->t_outq.c_cc <= tp->t_lowat) {
low:
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}

out:
	splx(s);
}

void
pcstop(struct tty *tp, int flag)
{
}


/*---------------------------------------------------------------------------*
 *		/dev/console
 *---------------------------------------------------------------------------*/
void
pccnprobe(struct consdev *cp)
{
	int maj;

	/* locate the major number */

	for (maj = 0; maj < nchrdev; maj++) {
		if ((u_int)cdevsw[maj].d_open == (u_int)pcopen)
			break;
	}

	if (maj == nchrdev) {
		/* we are not in cdevsw[], give up */
		panic("pcvt is not in cdevsw[]");
	}

	/* initialize required fields */

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;
}

void
pccninit(struct consdev *cp)
{

	pcvt_is_console = 1;
}

void
pccnputc(Dev_t dev, U_char c)
{

#if PCVT_SW0CNOUTP

	if(current_video_screen != 0)
		switch_screen(0, 0);

#endif /* PCVT_SW0CNOUTP */

	if (c == '\n')
		sput("\r", 1, 1, 0);

	sput((char *) &c, 1, 1, 0);

 	async_update();
}

int
pccngetc(Dev_t dev)
{
	register int s;
	register u_char *cp;

#ifdef XSERVER
 	if (dev != NODEV && vs[minor(dev)].kbd_state == K_RAW)
		return 0;
#endif /* XSERVER */

	s = spltty();		/* block pcrint while we poll */
	cp = sgetc(0);
	splx(s);
	async_update();

	/* this belongs to cons.c */
	if (*cp == '\r')
		return('\n');

	return (*cp);
}

void
pccnpollc(Dev_t dev, int on)
{
	kbd_polling = on;
	if (!on) {
		register int s;

		/*
		 * If disabling polling, make sure there are no bytes left in
		 * the FIFO, holding up the interrupt line.  Otherwise we
		 * won't get any further interrupts.
		 */
		s = spltty();
		pcintr(NULL);
		splx(s);
	}
}

/*---------------------------------------------------------------------------*
 *	Set line parameters
 *---------------------------------------------------------------------------*/
int
pcparam(struct tty *tp, struct termios *t)
{
	register int cflag = t->c_cflag;

        /* and copy to tty */

        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	return(0);
}

/*----------------------------------------------------------------------*
 *	read initial VGA palette (as stored by VGA ROM BIOS) into
 *	palette save area
 *----------------------------------------------------------------------*/
void
vgapelinit(void)
{
	register unsigned idx;
	register struct rgb *val;

	/* first, read all and store to first screen's save buffer */
	for(idx = 0, val = vs[0].palette; idx < NVGAPEL; idx++, val++)
		vgapaletteio(idx, val, 0 /* read it */);

	/* now, duplicate for remaining screens */
	for(idx = 1; idx < PCVT_NSCREENS; idx++)
		bcopy(vs[0].palette, vs[idx].palette,
		      NVGAPEL * sizeof(struct rgb));
}

#endif	/* NVT > 0 */

