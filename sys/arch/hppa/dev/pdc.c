/*	$OpenBSD: pdc.c,v 1.3 1998/10/29 17:47:15 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
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
int pdcret[32] __attribute__ ((aligned(8)));
char pdc_consbuf[MINIOSIZ] __attribute__ ((aligned(MINIOSIZ)));
iodcio_t pdc_cniodc, pdc_kbdiodc;
pz_device_t *pz_kbd, *pz_cons;
struct tty *pdc_tty[1];

int pdcmatch __P((struct device *, void *, void*));
void pdcattach __P((struct device *, struct device *, void *));

struct cfattach pdc_ca = {
	sizeof(pdcsoftc_t), pdcmatch, pdcattach
};

struct cfdriver pdc_cd = {
	NULL, "pdc", DV_DULL
};

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
	return ENXIO;
}

int
pdcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	return ENXIO;
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
}

struct tty *
pdctty(dev)
	dev_t dev;
{
	if (minor(dev) != 0)
		panic("pdctty: bogus");

	return pdc_tty[0];
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
pdccnprobe(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(28,0);
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
pdccngetc(dev)
	dev_t dev;
{
	static int stash = 0;
	register int err, c, l;

	if (!pdc)
		return 0;

	if (stash) {
		c = stash;
		if (!(dev & 0x80))
			stash = 0;
		return c;
	}

	do {
		err = pdc_call(pdc_kbdiodc, 0, pz_kbd->pz_hpa,
			       IODC_IO_CONSIN, pz_kbd->pz_spa,
			       pz_kbd->pz_layers, pdcret,
			       0, pdc_consbuf, 1, 0);

		l = pdcret[0];
		c = pdc_consbuf[0];
#ifdef DEBUG
		if (err < 0)
			printf("pdccngetc: input error: %d\n", err);
#endif

		/* if we are doing ischar() report immidiatelly */
		if (dev & 0x80 && l == 0)
			return (0);

	} while(!l);

	if (dev & 0x80)
		stash = c;

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

