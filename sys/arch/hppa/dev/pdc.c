/*	$OpenBSD: pdc.c,v 1.11 2000/01/26 20:55:20 mickey Exp $	*/

/*
 * Copyright (c) 1998-2000 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>

#include <dev/cons.h>

#include <machine/conf.h>
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

typedef
struct pdc_softc {
	struct device sc_dv;
} pdcsoftc_t;

pdcio_t pdc;
int pdcret[32] PDC_ALIGNMENT;
char pdc_consbuf[IODC_MINIOSIZ] PDC_ALIGNMENT;
iodcio_t pdc_cniodc, pdc_kbdiodc;
pz_device_t *pz_kbd, *pz_cons;
struct tty *pdc_tty[1];
int CONADDR;

int pdcmatch __P((struct device *, void *, void*));
void pdcattach __P((struct device *, struct device *, void *));

struct cfattach pdc_ca = {
	sizeof(pdcsoftc_t), pdcmatch, pdcattach
};

struct cfdriver pdc_cd = {
	NULL, "pdc", DV_DULL
};

void pdcstart __P((struct tty *tp));
void pdctimeout __P((void *v));
int pdcparam __P((struct tty *tp, struct termios *));
int pdccnlookc __P((dev_t dev, char *cp));

void
pdc_init()
{
	static int kbd_iodc[IODC_MAXSIZE/sizeof(int)];
	static int cn_iodc[IODC_MAXSIZE/sizeof(int)];
	int err;

	/* XXX locore've done it XXX pdc = (pdcio_t)PAGE0->mem_pdc; */
	pz_kbd = &PAGE0->mem_kbd;
	pz_cons = &PAGE0->mem_cons;

	/* XXX should we reset the console/kbd here?
	   well, /boot did that for us anyway */
	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_READ,
			    pdcret, pz_cons->pz_hpa,
			    IODC_IO, cn_iodc, IODC_MAXSIZE)) < 0 ||
	    (err = pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_READ,
			    pdcret, pz_kbd->pz_hpa,
			    IODC_IO, kbd_iodc, IODC_MAXSIZE)) < 0) {
#ifdef DEBUG
		printf("pdc_init: failed reading IODC (%d)\n", err);
#endif
	}

	pdc_cniodc = (iodcio_t)cn_iodc;
	pdc_kbdiodc = (iodcio_t)kbd_iodc;

	/* XXX make pdc current console */
	cn_tab = &constab[0];
	/* TODO: detect that we are on cereal, and set CONADDR */
}

int
pdcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	/* there could be only one */
	if (cf->cf_unit > 0 && !strcmp(ca->ca_name, "pdc"))
		return 0;

	return 1;
}

void
pdcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	if (!pdc)
		pdc_init();

	printf("\n");
}

int
pdcopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp;
	int s;
	int error = 0, setuptimeout;

	if (unit >= 1)
		return ENXIO;

	s = spltty();

	if (!pdc_tty[unit]) {
		tp = pdc_tty[unit] = ttymalloc();
		tty_attach(tp);
	} else
		tp = pdc_tty[unit];

	tp->t_oproc = pdcstart;
	tp->t_param = pdcparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN|TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = 9600;
		ttsetwater(tp);

		setuptimeout = 1;
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error == 0 && setuptimeout)
		timeout(pdctimeout, tp, 1);

	return error;
}

int
pdcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = pdc_tty[unit];

	untimeout(pdctimeout, tp);
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}

int
pdcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = pdc_tty[minor(dev)];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
pdcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = pdc_tty[minor(dev)];
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
 
int
pdcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = pdc_tty[unit];
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	return ENOTTY;
}

int
pdcparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	return 0;
}

void
pdcstart(tp)
	struct tty *tp;
{
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY)) {
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		pdccnputc(tp->t_dev, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;
	splx(s);
}

int
pdcstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
	return 0;
}

void
pdctimeout(v)
	void *v;
{
	struct tty *tp = v;
	u_char c;

	while (pdccnlookc(tp->t_dev, &c)) {
		if (tp->t_state & TS_ISOPEN)
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	timeout(pdctimeout, tp, 1);
}

struct tty *
pdctty(dev)
	dev_t dev;
{
	if (minor(dev) != 0)
		panic("pdctty: bogus");

	return pdc_tty[0];
}

void
pdccnprobe(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(22,0);
	cn->cn_pri = CN_NORMAL;
}

void
pdccninit(cn)
	struct consdev *cn;
{
#ifdef DEBUG
	printf("pdc0: console init\n");
#endif
}

int
pdccnlookc(dev, cp)
	dev_t dev;
	char *cp;
{
	int err, l;

	err = pdc_call(pdc_kbdiodc, 0, pz_kbd->pz_hpa, IODC_IO_CONSIN,
	    pz_kbd->pz_spa, pz_kbd->pz_layers, pdcret, 0,
	    pdc_consbuf, 1, 0);

	l = pdcret[0];
	*cp = pdc_consbuf[0];
#ifdef DEBUG
	if (err < 0)
		printf("pdccnlookc: input error: %d\n", err);
#endif

	return l;
}

int
pdccngetc(dev)
	dev_t dev;
{
	char c;

	if (!pdc)
		return 0;

	while(!pdccnlookc(dev, &c))
		;

	return (c);
}

void
pdccnputc(dev, c)
	dev_t dev;
	int c;
{
	register int err;

	*pdc_consbuf = c;
	if ((err = pdc_call(pdc_cniodc, 0, pz_cons->pz_hpa, IODC_IO_CONSOUT,
			    pz_cons->pz_spa, pz_cons->pz_layers,
			    pdcret, 0, pdc_consbuf, 1, 0)) < 0) {
#ifdef DEBUG
		printf("pdccnputc: output error: %d\n", err);
#endif
	}

}

void
pdccnpollc(dev, on)
	dev_t dev;
	int on;
{

}
