/*	$OpenBSD: pdc.c,v 1.1 1998/09/12 02:51:34 mickey Exp $	*/

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

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/pdc.h>
#include <machine/iodc.h>
#include <machine/iomod.h>

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
	/* XXX locore've done it XXX pdc = (pdcio_t)PAGE0->mem_pdc; */
	pz_kbd = &PAGE0->mem_kbd;
	pz_cons = &PAGE0->mem_cons;

	/* following the memory mappings, console iodc is at `mem_free' */
	pdc_cniodc = (iodcio_t)PAGE0->mem_free;
	pdc_kbdiodc = (iodcio_t)PAGE0->mem_free;

	/* XXX make pdc current console */
	cn_tab = &constab[0];
}

int
pdcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	/* struct cfdata *cf = cfdata; */
	struct confargs *ca = aux;

	/* there could be only one */
	if (pdc_cd.cd_ndevs || strcmp(ca->ca_name, pdc_cd.cd_name))
		return 0;

	return 1;
}

void
pdcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf("\n");
	if (!pdc)
		pdc_init();
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

}

int
pdccngetc(dev)
	dev_t dev;
{
	int err;

	if (!pdc)
		return 0;

	if ((err = pdc_call(pdc_kbdiodc, 0, pz_kbd->pz_hpa, IODC_IO_CONSIN,
			    &pz_kbd->pz_spa, pz_kbd->pz_layers, pdcret,
			    0, pdc_consbuf, 1, 0)) < 0) {
#ifdef DEBUG
		printf("pdc_getc: input error: %d\n", err);
#endif
	}

	if (pdcret[0] == 0)
		return (0);
#ifdef DEBUG
	if (pdcret[0] > 1)
		printf("pdc_getc: input got too much (%d)\n", pdcret[0]);
#endif
	return *pdc_consbuf;
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
		printf("pdc_putc: output error: %d\n", err);
#endif
	}

}

void
pdccnpollc(dev, on)
	dev_t dev;
	int on;
{

}

