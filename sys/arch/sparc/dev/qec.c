/*	$OpenBSD: qec.c,v 1.23 2015/03/29 10:59:47 mpi Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <sparc/autoconf.h>
#include <sparc/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/qecreg.h>
#include <sparc/dev/qecvar.h>

int	qecprint(void *, const char *);
int	qecmatch(struct device *, void *, void *);
void	qecattach(struct device *, struct device *, void *);
int	qec_fix_range(struct qec_softc *, struct sbus_softc *);
void	qec_translate(struct qec_softc *, struct confargs *);

struct cfattach qec_ca = {
	sizeof(struct qec_softc), qecmatch, qecattach
};

struct cfdriver qec_cd = {
	NULL, "qec", DV_DULL
};

int
qecprint(aux, name)
	void *aux;
	const char *name;
{
	register struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	printf(" offset 0x%x", ca->ca_offset);
	return (UNCONF);
}

/*
 * match a QEC device in a slot capable of DMA
 */
int
qecmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	if (!sbus_testdma((struct sbus_softc *)parent, ca))
		return (0);

	return (1);
}

/*
 * Attach all the sub-devices we can find
 */
void
qecattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	struct qec_softc *sc = (void *)self;
	int node;
	struct confargs oca;
	char *name;
	int sbusburst;

	/*
	 * The first i/o space is the qec global registers, and
	 * the second is a buffer used by the qec channels internally.
	 * (It's not necessary to map the second i/o space, but knowing
	 * its size is necessary).
	 */
	sc->sc_regs = mapiodev(&ca->ca_ra.ra_reg[0], 0,
	    sizeof(struct qecregs));
	sc->sc_bufsiz = ca->ca_ra.ra_reg[1].rr_len;
	sc->sc_paddr = ca->ca_ra.ra_reg[0].rr_paddr;

	/*
	 * On qec+qe, the qec has the interrupt priority, but we
	 * need to pass that down so that the qe's can handle them.
	 */
	if (ca->ca_ra.ra_nintr == 1)
		sc->sc_pri = ca->ca_ra.ra_intr[0].int_pri;

	node = sc->sc_node = ca->ca_ra.ra_node;

	if (qec_fix_range(sc, (struct sbus_softc *)parent) != 0)
		return;

	/*
	 * Get transfer burst size from PROM
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	sc->sc_nchannels = getpropint(ca->ca_ra.ra_node, "#channels", -1);
	if (sc->sc_nchannels == -1) {
		printf(": no channels\n");
		return;
	}
	else if (sc->sc_nchannels < 1 || sc->sc_nchannels > 4) {
		printf(": invalid number of channels: %d\n", sc->sc_nchannels);
		return;
	}

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		/* take SBus burst sizes */
		sc->sc_burst = sbusburst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= sbusburst;

	printf(": %dK memory %d channel%s",
	    sc->sc_bufsiz / 1024, sc->sc_nchannels,
	    (sc->sc_nchannels == 1) ? "" : "s");

	node = sc->sc_node = ca->ca_ra.ra_node;

	/* Propagate bootpath */
	if (ca->ca_ra.ra_bp != NULL)
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	printf("\n");

	qec_reset(sc);

	/* search through children */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		qec_translate(sc, &oca);
		oca.ca_bustype = BUS_SBUS;
		(void) config_found(&sc->sc_dev, (void *)&oca, qecprint);
	}
}

int
qec_fix_range(sc, sbp)
	struct qec_softc *sc;
	struct sbus_softc *sbp;
{
	int rlen, i, j;

	rlen = getproplen(sc->sc_node, "ranges");
	sc->sc_range =
		(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_range == NULL) {
		printf(": PROM ranges too large: %d\n", rlen);
		return EINVAL;
	}
	sc->sc_nrange = rlen / sizeof(struct rom_range);
	(void)getprop(sc->sc_node, "ranges", sc->sc_range, rlen);

	if (sbp->sc_nrange == 0) {
		/*
		 * Old-style SBus configuration: we need to compute the
		 * physical address of the board's base, in order to be
		 * able to compute proper physical addresses in
		 * qec_translate() below.
		 */
		struct rom_reg rr[RA_MAXREG];
		int reglen;

		reglen = getprop(sc->sc_node, "reg", rr, sizeof rr);
		/* shouldn't happen */
		if (reglen <= 0 || reglen % sizeof(struct rom_reg) != 0) {
			printf(": unexpected \"reg\" property layout\n");
			return EINVAL;
		}
		for (i = 0; i < sc->sc_nrange; i++) {
			sc->sc_range[i].poffset +=
			    sc->sc_paddr - rr[0].rr_paddr;
		}
	} else {
		for (i = 0; i < sc->sc_nrange; i++) {
			for (j = 0; j < sbp->sc_nrange; j++) {
				if (sc->sc_range[i].pspace ==
				    sbp->sc_range[j].cspace) {
					sc->sc_range[i].poffset +=
					    sbp->sc_range[j].poffset;
					sc->sc_range[i].pspace =
					    sbp->sc_range[j].pspace;
					break;
				}
			}
		}
	}

	return 0;
}

/*
 * Translate the register addresses of our children 
 */
void
qec_translate(sc, ca)
	struct qec_softc *sc;
	struct confargs *ca;
{
	register int i;

	ca->ca_slot = ca->ca_ra.ra_iospace;
	ca->ca_offset = sc->sc_range[ca->ca_slot].poffset - (long)sc->sc_paddr;

	/* Translate into parent address spaces */
	for (i = 0; i < ca->ca_ra.ra_nreg; i++) {
		int j, cspace = ca->ca_ra.ra_reg[i].rr_iospace;

		for (j = 0; j < sc->sc_nrange; j++) {
			if (sc->sc_range[j].cspace == cspace) {
				ca->ca_ra.ra_reg[i].rr_paddr +=
					sc->sc_range[j].poffset;
				ca->ca_ra.ra_reg[i].rr_iospace =
					sc->sc_range[j].pspace;
				break;
			}
		}
	}
}

/*
 * Reset the QEC and initialize its global registers.
 */
void
qec_reset(sc)
	struct qec_softc *sc;
{
	struct qecregs *qr = sc->sc_regs;
	int i = 200;

	qr->ctrl = QEC_CTRL_RESET;
	while (--i) {
		if ((qr->ctrl & QEC_CTRL_RESET) == 0)
			break;
		DELAY(20);
	}
	if (i == 0) {
		printf("%s: reset failed.\n", sc->sc_dev.dv_xname);
		return;
	}

	qr->msize = sc->sc_bufsiz / sc->sc_nchannels;
	sc->sc_msize = qr->msize;

	qr->rsize = sc->sc_bufsiz / (sc->sc_nchannels * 2);
	sc->sc_rsize = qr->rsize;

	qr->tsize = sc->sc_bufsiz / (sc->sc_nchannels * 2);

	qr->psize = QEC_PSIZE_2048;

        if (sc->sc_burst & SBUS_BURST_64)
		i = QEC_CTRL_B64;
	else if (sc->sc_burst & SBUS_BURST_32)
		i = QEC_CTRL_B32;
	else
		i = QEC_CTRL_B16;

	qr->ctrl = (qr->ctrl & QEC_CTRL_MODEMASK) | i;
}

/*
 * Routine to copy from mbuf chain to transmit buffer in
 * network buffer memory.
 */
int
qec_put(buf, m0)
	u_int8_t *buf;
	struct mbuf *m0;
{
	struct mbuf *m;
	int len, tlen = 0;

	for (m = m0; m != NULL; m = m->m_next) {
		len = m->m_len;
		bcopy(mtod(m, caddr_t), buf, len);
		buf += len;
		tlen += len;
	}
	m_freem(m0);
	return (tlen);
}

/*
 * Pull data off an interface.
 * Len is the length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present,
 * we copy into clusters.
 */
struct mbuf *
qec_get(buf, totlen)
	u_int8_t *buf;
	int totlen;
{
	struct mbuf *m, *top, **mp;
	int len, pad;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);
	m->m_pkthdr.len = totlen;
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	len = MHLEN;
	if (totlen >= MINCLSIZE) {
		MCLGET(m, M_DONTWAIT);
		if (m->m_flags & M_EXT)
			len = MCLBYTES;
	}
	m->m_data += pad;
	len -= pad;
	top = NULL;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return NULL;
			}
			len = MLEN;
		}
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy(buf, mtod(m, caddr_t), len);
		buf += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}
