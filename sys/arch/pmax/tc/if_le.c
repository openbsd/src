/*	$NetBSD: if_le.c,v 1.2.2.1 1995/10/13 03:22:51 jonathan Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>

#ifdef pmax
typedef u_int32_t word_t;
#define wbflush() MachEmptyWriteBuffer()

/* These should be in a header file, but where? */
int	pmax_boardtype;		/* Mother board type */
/*extern u_long le_iomem;*/
extern u_long asic_base;

/* does this machine have an ASIC? */
#define SYSTEM_HAS_ASIC() \
  (pmax_boardtype == DS_MAXINE || pmax_boardtype == DS_3MIN || \
   pmax_boardtype == DS_3MAXPLUS)

#include <pmax/pmax/kn01.h>
#include <machine/machConst.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/tc/tc.h>
#include <pmax/pmax/asic.h>
#include <pmax/tc/if_levar.h>
#else /* Alpha */

typedef  u_int64 word_t;
#define SYSTEM_HAS_ASIC() \
 (hwrpb->rpb_type == ST_DEC_3000_300 || hwrpb->rpb_type == ST_DEC_3000_500)

#include <machine/rpb.h>
#include <alpha/tc/tc.h>
#include <alpha/tc/asic.h>
#include <alpha/tc/if_levar.h>

#endif  /* Alpha */



#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>


/* access LANCE registers */
void lewritereg();
#define	LERDWR(cntl, src, dst)	{ (dst) = (src); wbflush(); }
#define	LEWREG(src, dst)	lewritereg(&(dst), (src))

#define LE_OFFSET_RAM		0x0
#define LE_OFFSET_LANCE		0x100000
#define LE_OFFSET_ROM		0x1c0000

extern caddr_t le_iomem;

#define	LE_SOFTC(unit)	lecd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

/*
 * Routines for accessing the transmit and receive buffers. Unfortunately,
 * CPU addressing of these buffers is done in one of 3 ways:
 * - contiguous (for the 3max and turbochannel option card)
 * - gap2, which means shorts (2 bytes) interspersed with short (2 byte)
 *   spaces (for the kn01)
 * - gap16, which means 16bytes interspersed with 16byte spaces
 *   for buffers which must begin on a 32byte boundary (for 3min,
 *   maxine and 3maxplus).  The gap16 machines still need gap2 for DMA
 *   ring-buffer descriptors.
 * These functions are now defined in the machine-independent code;
 * set the preprocessor macros that say which ones we need.
 */

#define	LE_NEED_BUF_CONTIG
#define	LE_NEED_BUF_GAP2
#define	LE_NEED_BUF_GAP16

int lematch __P((struct device *, void *, void *));
void leattach __P((struct device *, struct device *, void *));

int leintr __P((void *));

struct cfdriver lecd = {
	NULL, "le", lematch, leattach, DV_IFNET, sizeof (struct le_softc)
};

void copytobuf_contig __P((struct le_softc *, void *, int, int));
void copyfrombuf_contig __P((struct le_softc *, void *, int, int));
void zerobuf_contig __P((struct le_softc *, int, int));
integrate void copytobuf_gap16 __P((struct le_softc *, void *, int, int));
integrate void copyfrombuf_gap16 __P((struct le_softc *, void *, int, int));
integrate void zerobuf_gap16 __P((struct le_softc *, int, int));
integrate void copytobuf_gap2 __P((struct le_softc *, void *, int, int));
integrate void copyfrombuf_gap2 __P((struct le_softc *, void *, int, int));
integrate void zerobuf_gap2 __P((struct le_softc *, int, int));

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = sc->sc_r1;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(port, val, ler1->ler1_rdp);
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = sc->sc_r1;
	u_int16_t val;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(0, ler1->ler1_rdp, val);
	return (val);
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
#ifdef notdef /* XXX */
	struct tc_cfloc *tc_locp;
	struct asic_cfloc *asic_locp;
#endif

#ifdef notdef /* XXX */
	tclocp = (struct tc_cfloc *)cf->cf_loc;
#endif

	/* XXX CHECK BUS */
	/* make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "PMAD-BA ") &&	/* untested alpha TC option */
	    !BUS_MATCHNAME(ca, "PMAD-AA ") && /* KN02 baseboard, old option */
	    !BUS_MATCHNAME(ca, "lance"))  /* Our name for baseboard devices */
		return (0);

#ifdef notdef /* XXX */
	/* make sure the unit matches the cfdata */
	if ((cf->cf_unit != tap->ta_unit &&
	     tap->ta_unit != TA_ANYUNIT) ||
	    (tclocp->cf_slot != tap->ta_slot &&
	     tclocp->cf_slot != TC_SLOT_WILD) ||
	    (tclocp->cf_offset != tap->ta_offset &&
	     tclocp->cf_offset != TC_OFFSET_WILD))
		return (0);
#endif

	return (1);
}

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *sc = (void *)self;
	struct confargs *ca = aux;
	u_char *cp;	/* pointer to MAC address */
	int i;

	if (sc->sc_dev.dv_unit == 0 && SYSTEM_HAS_ASIC()) {
		/* It's on the system ASIC */
		volatile u_int *ldp;
		word_t dma_mask;

		sc->sc_r1 = (struct lereg1 *)
		    MACH_PHYS_TO_UNCACHED(BUS_CVTADDR(ca));

#ifdef alpha	/* !pmax */
		sc->sc_r1 = TC_DENSE_TO_SPARSE(sc->sc_r1);
#endif	/* !pmax */

		sc->sc_mem = (void *)MACH_PHYS_TO_UNCACHED(le_iomem);
/* XXX */	cp = (u_char *)ASIC_SYS_ETHER_ADDRESS(asic_base);

		sc->sc_copytodesc = copytobuf_gap2;
		sc->sc_copyfromdesc = copyfrombuf_gap2;
		sc->sc_copytobuf = copytobuf_gap16;
		sc->sc_copyfrombuf = copyfrombuf_gap16;
		sc->sc_zerobuf = zerobuf_gap16;

		/*
		 * And enable Lance dma through the asic.
		 */
		ldp = (volatile u_int *) (ASIC_REG_LANCE_DMAPTR(asic_base));
		dma_mask = ((word_t)le_iomem << 3);

#ifdef alpha /* !pmax */
		/* Set upper 64 bits of DMA mask */
		dma_mask  = (dma_mask & ~(word_t)0x1f) |
			(((word_t)le_iomem >> 29) & 0x1f);
#endif /* alpha*/

		*ldp = dma_mask;
		*(volatile u_int *)ASIC_REG_CSR(asic_base) |=
		    ASIC_CSR_DMAEN_LANCE;
		wbflush();
	} else
#ifdef pmax
	if (sc->sc_dev.dv_unit == 0 && (pmax_boardtype == DS_PMAX)) {
		/* It's on the baseboard, attached directly to mainbus. */

		sc->sc_r1 = (struct lereg1 *)BUS_CVTADDR(ca);
/*XXX*/		sc->sc_mem = (void *)MACH_PHYS_TO_UNCACHED(0x19000000);
/*XXX*/		cp = (u_char *)(MACH_PHYS_TO_UNCACHED(KN01_SYS_CLOCK) + 1);

		sc->sc_copytodesc = copytobuf_gap2;
		sc->sc_copyfromdesc = copyfrombuf_gap2;
		sc->sc_copytobuf = copytobuf_gap2;
		sc->sc_copyfrombuf = copyfrombuf_gap2;
		sc->sc_zerobuf = zerobuf_gap2;
	}
#endif	/* pmax */
	else {
		/* It's on the turbochannel proper, or on KN02 baseboard. */
		sc->sc_r1 = (struct lereg1 *)
		    (BUS_CVTADDR(ca) + LE_OFFSET_LANCE);
		sc->sc_mem = (void *)
		    (BUS_CVTADDR(ca) + LE_OFFSET_RAM);
		cp = (u_char *)(BUS_CVTADDR(ca) + LE_OFFSET_ROM + 2);

		sc->sc_copytodesc = copytobuf_contig;
		sc->sc_copyfromdesc = copyfrombuf_contig;
		sc->sc_copytobuf = copytobuf_contig;
		sc->sc_copyfrombuf = copyfrombuf_contig;
		sc->sc_zerobuf = zerobuf_contig;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = 0;
	sc->sc_memsize = 65536;

	/*
	 * Get the ethernet address out of rom
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++) {
		sc->sc_arpcom.ac_enaddr[i] = *cp;
		cp += 4;
	}

	sc->sc_arpcom.ac_if.if_name = lecd.cd_name;
	leconfig(sc);

	BUS_INTR_ESTABLISH(ca, leintr, sc);

	if (SYSTEM_HAS_ASIC()) {
		/* XXX YEECH!!! */
		*(volatile u_int *)ASIC_REG_IMSK(asic_base) |= ASIC_INTR_LANCE;
		wbflush();
	}
}

/*
 * Write a lance register port, reading it back to ensure success. This seems
 * to be necessary during initialization, since the chip appears to be a bit
 * pokey sometimes.
 */
void
lewritereg(regptr, val)
	register volatile u_short *regptr;
	register u_short val;
{
	register int i = 0;

	while (*regptr != val) {
		*regptr = val;
		wbflush();
		if (++i > 10000) {
			printf("le: Reg did not settle (to x%x): x%x\n", val,
			    *regptr);
			return;
		}
		DELAY(100);
	}
}

#include <dev/ic/am7990.c>

/**
 ** XXX These could arguably go in dev/ic/am7990.c, or at least somewhere
 **     sharable with other Lance drivers that need them.
 **/

/*
 * Routines for accessing the transmit and receive buffers.
 * The various CPU and adapter configurations supported by this
 * driver require three different access methods for buffers
 * and descriptors:
 *	(1) contig (contiguous data; no padding),
 *	(2) gap2 (two bytes of data followed by two bytes of padding),
 *	(3) gap16 (16 bytes of data followed by 16 bytes of padding).
 */

#ifdef LE_NEED_BUF_CONTIG
/*
 * contig: contiguous data with no padding.
 *
 * Buffers may have any alignment.
 */

/*integrate*/ void
copytobuf_contig(sc, from, boff, len)
	struct le_softc *sc;
	void *from;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(from, buf + boff, len);
}

/*integrate*/ void
copyfrombuf_contig(sc, to, boff, len)
	struct le_softc *sc;
	void *to;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(buf + boff, to, len);
}

/*integrate*/ void
zerobuf_contig(sc, boff, len)
	struct le_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just let bzero() do the work
	 */
	bzero(buf + boff, len);
}
#endif /* LE_NEED_BUF_CONTIG */

#ifdef LE_NEED_BUF_GAP2
/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

integrate void
copytobuf_gap2(sc, fromv, boff, len)
	struct le_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register volatile u_int16_t *bptr;
	register int xfer;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr = (*from++ << 8) | (*bptr & 0xff);
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		*bptr = (from[1] << 8) | (from[0] & 0xff);
		bptr += 2;
		from += 2;
		len -= 2;
	}
	if (len == 1)
		*bptr = (u_int16_t)*from;
}

integrate void
copyfrombuf_gap2(sc, tov, boff, len)
	struct le_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register volatile u_int16_t *bptr;
	register u_int16_t tmp;
	register int xfer;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*to++ = (*bptr >> 8) & 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		tmp = *bptr;
		*to++ = tmp & 0xff;
		*to++ = (tmp >> 8) & 0xff;
		bptr += 2;
		len -= 2;
	}
	if (len == 1)
		*to = *bptr & 0xff;
}

integrate void
zerobuf_gap2(sc, boff, len)
	struct le_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register volatile u_int16_t *bptr;

	if ((unsigned)boff & 0x1) {
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr &= 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 0) {
		*bptr = 0;
		bptr += 2;
		len -= 2;
	}
}
#endif /* LE_NEED_BUF_GAP2 */

#ifdef LE_NEED_BUF_GAP16
/*
 * gap16: 16 bytes of data followed by 16 bytes of pad.
 *
 * Buffers must be 32-byte aligned.
 */

integrate void
copytobuf_gap16(sc, fromv, boff, len)
	struct le_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(from, bptr + boff, xfer);
		from += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

integrate void
copyfrombuf_gap16(sc, tov, boff, len)
	struct le_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(bptr + boff, to, xfer);
		to += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

integrate void
zerobuf_gap16(sc, boff, len)
	struct le_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bzero(bptr + boff, xfer);
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}
#endif /* LE_NEED_BUF_GAP16 */
