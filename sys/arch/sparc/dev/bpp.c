/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
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
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/conf.h>

#include <sparc/dev/bppreg.h>

#define BPP_BSIZE	1024

#define LONG_TIMEOUT	30		/* XXX */
#define SHORT_TIMEOUT	3		/* XXX */

struct bpp_softc {
	struct device sc_dev;

	size_t sc_count;
	struct buf *sc_inbuf;
	u_int8_t *sc_cp;

	char sc_open;

	volatile struct bppregs *sc_regs;
};

static int bppmatch __P((struct device *, void *, void *));
static void bppattach __P((struct device *, struct device *, void *));

#define BPPUNIT(s)	minor(s)

struct cfattach bpp_ca = {
	sizeof(struct bpp_softc), bppmatch, bppattach
};

struct cfdriver bpp_cd = {
	NULL, "bpp", DV_DULL
};

static __inline__ void bpp_outb __P((struct bpp_softc *, u_int8_t));
static __inline__ u_int8_t bpp_inb __P((struct bpp_softc *));
static void bppreset __P((struct bpp_softc *, int));
static void bppresetmode __P((struct bpp_softc *));
static int bpppushbytes __P((struct bpp_softc *));

static int
bppmatch(parent, vcf, aux)
	struct device *parent;
	void *aux, *vcf;
{
	register struct confargs *ca = aux;

	if (strcmp(ca->ca_ra.ra_name, "SUNW,bpp"))
		return (0);

	return (1);
}

static void
bppattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct bpp_softc *bpp = (void *)self;

	bpp->sc_regs = mapiodev(ra->ra_reg, 0, ra->ra_len);

	bppreset(bpp, 0);

	switch (bpp->sc_regs->bpp_csr & BPP_DEV_ID_MASK) {
	case BPP_DEV_ID_ZEBRA:
		printf(": Zebra\n");
		break;
	case BPP_DEV_ID_L64854:
		printf(": DMA2\n");
		break;
	default:
		printf(": Unknown type\n");
		break;
	}

	bppresetmode(bpp);
}

static void
bppreset(bpp, verbose)
	struct bpp_softc *bpp;
	int verbose;
{
	volatile u_int32_t bpp_csr;

	/* Reset hardware. */
	bpp_csr = bpp->sc_regs->bpp_csr;
	if ((bpp_csr & BPP_DRAINING) && !(bpp_csr & BPP_ERR_PEND)) {
		delay(20);

		bpp_csr = bpp->sc_regs->bpp_csr;
		if (verbose && (bpp_csr & BPP_DRAINING) &&
		    !(bpp_csr & BPP_ERR_PEND))
			printf("%s: draining still active (0x%08x)\n",
			    bpp->sc_dev.dv_xname, bpp_csr);
	}
	bpp->sc_regs->bpp_csr = (bpp_csr | BPP_RESET) & ~BPP_INT_EN;
	delay(500);
	bpp->sc_regs->bpp_csr &= ~BPP_RESET;
}

static void
bppresetmode(bpp)
	struct bpp_softc *bpp;
{
	bpp->sc_regs->bpp_or = BPP_OR_AFXN|BPP_OR_SLCT_IN;
	bpp->sc_regs->bpp_tcr = BPP_TCR_DS;
}

static __inline__ void
bpp_outb(bpp, byte)
	struct bpp_softc *bpp;
	u_int8_t byte;
{
	bpp->sc_regs->bpp_dr = byte;
}

static __inline__ u_int8_t
bpp_inb(bpp)
	struct bpp_softc *bpp;
{
	return (bpp->sc_regs->bpp_dr);
}

int
bppopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = BPPUNIT(dev);
	struct bpp_softc *bpp;

	if (unit >= bpp_cd.cd_ndevs)
		return (ENXIO);
	bpp = bpp_cd.cd_devs[unit];
	if (!bpp)
		return (ENXIO);

	if (bpp->sc_open)
		return (EBUSY);

	bpp->sc_inbuf = geteblk(BPP_BSIZE);
	bpp->sc_count = 0;
	bpp->sc_open = 1;

	/* bppreset(bpp, 1); */
	bppresetmode(bpp);

	return (0);
}

int
bppclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct bpp_softc *bpp = bpp_cd.cd_devs[BPPUNIT(dev)];
	int error = 0;

	if (bpp->sc_count)
		(void) bpppushbytes(bpp);

	/* XXX */
	bppresetmode(bpp);
	delay(100);
	bppreset(bpp, 1);
	delay(100);
	bppresetmode(bpp);

	brelse(bpp->sc_inbuf);

	bpp->sc_open = 0;
	return (error);
}

int
bpppushbytes(bpp)
	struct bpp_softc *bpp;
{
	int spin, error;

	while (bpp->sc_count > 0) {
		error = 0;

		/* Wait for BPP_TCR_ACK and BPP_TCR_BUSY to clear. */
		spin = 0;
		while ((bpp->sc_regs->bpp_tcr & BPP_TCR_ACK) ||
		    (bpp->sc_regs->bpp_tcr & BPP_TCR_BUSY)) {
			delay(1000);
			if (++spin >= LONG_TIMEOUT)
				break;
		}
		
		if ((bpp->sc_regs->bpp_tcr & BPP_TCR_ACK) ||
		    (bpp->sc_regs->bpp_tcr & BPP_TCR_BUSY))
			return (EBUSY);

		bpp_outb(bpp, *bpp->sc_cp++);

		/* Clear BPP_TCR_DS. */
		bpp->sc_regs->bpp_tcr &= ~BPP_TCR_DS;

		/* Short wait for BPP_TCR_BUSY. */
		spin = 0;
		while (!(bpp->sc_regs->bpp_tcr & BPP_TCR_BUSY)) {
			delay(1000);
			if (++spin >= SHORT_TIMEOUT)
				break;
		}
		if (!(bpp->sc_regs->bpp_tcr & BPP_TCR_BUSY))
			error = EIO;

		/* Set BPP_TCR_DS. */
		bpp->sc_regs->bpp_tcr |= BPP_TCR_DS;

		if (error)
			return (error);

		bpp->sc_count--;
	}
	return (error);
}

int
bppwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct bpp_softc *bpp = bpp_cd.cd_devs[BPPUNIT(dev)];
	size_t n;
	int error = 0;

	while ((n = min(BPP_BSIZE, uio->uio_resid)) != 0) {
		uiomove(bpp->sc_cp = bpp->sc_inbuf->b_data, n, uio);
		bpp->sc_count = n;
		error = bpppushbytes(bpp);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += bpp->sc_count;
			bpp->sc_count = 0;
			return (error);
		}
	}
	return (0);
}

int
bppioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	return (ENODEV);
}
