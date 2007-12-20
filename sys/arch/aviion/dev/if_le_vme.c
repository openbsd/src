/*	$OpenBSD: if_le_vme.c,v 1.3 2007/12/20 21:16:19 miod Exp $	*/

/*-
 * Copyright (c) 1982, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)if_le.c	8.2 (Berkeley) 10/30/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <aviion/dev/if_le_vmereg.h>
#include <aviion/dev/vmevar.h>

struct	le_softc {
	struct am7990_softc	sc_am7990;	/* glue to MI code */

	bus_space_tag_t		sc_memt;	/* dual-ported memory access */
	bus_space_handle_t	sc_memh;

	bus_space_tag_t		sc_iot;		/* short io access */
	bus_space_handle_t	sc_ioh;

	struct intrhand		sc_ih;		/* interrupt vectoring */
	u_int			sc_csr;		/* CSR image */
	u_int			sc_ipl;
	u_int			sc_vec;
};

void  le_vme_attach(struct device *, struct device *, void *);
int   le_vme_match(struct device *, void *, void *);

struct cfattach le_vme_ca = {
	sizeof(struct le_softc), le_vme_match, le_vme_attach
};

void	le_vme_wrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
u_int16_t le_vme_rdcsr(struct am7990_softc *, u_int16_t);
void	nvram_cmd(struct am7990_softc *, u_int, u_int);
u_int16_t nvram_read(struct am7990_softc *, u_int);
void	le_vme_etheraddr(struct am7990_softc *);
void	le_vme_init(struct am7990_softc *);
void	le_vme_reset(struct am7990_softc *);
int	le_vme_intr(void *);
#if 0
void	le_vme_copyfrombuf_contig(struct am7990_softc *, void *, int, int);
void	le_vme_copytobuf_contig(struct am7990_softc *, void *, int, int);
void	le_vme_zerobuf_contig(struct am7990_softc *, int, int);
#endif

/* send command to the nvram controller */
void
nvram_cmd(sc, cmd, addr)
	struct am7990_softc *sc;
	u_int cmd;
	u_int addr;
{
	struct le_softc *lesc = (void *)sc;
	int i;

	for (i = 0; i < 8; i++) {
		bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, LEREG_EAR,
		    (cmd | (addr << 1)) >> i);
		CDELAY; 
	} 
}

/* read nvram one bit at a time */
u_int16_t
nvram_read(sc, nvram_addr)
	struct am7990_softc *sc;
	u_int nvram_addr;
{
	struct le_softc *lesc = (void *)sc;
	u_int val = 0, mask = 0x04000;
	u_int16_t wbit;

	lesc->sc_csr = HW_RS | NVRAM_EN | 0x07;
	ENABLE_NVRAM;
	nvram_cmd(sc, NVRAM_RCL, 0);
	DISABLE_NVRAM;
	CDELAY;
	ENABLE_NVRAM;
	nvram_cmd(sc, NVRAM_READ, nvram_addr);
	for (wbit = 0; wbit < 15; wbit++) {
		if (bus_space_read_2(lesc->sc_iot, lesc->sc_ioh,
		    LEREG_EAR) & 0x01)
			val |= mask;
		else
			val &= ~mask;
		mask = mask >> 1;
		CDELAY;
	}
	if (bus_space_read_2(lesc->sc_iot, lesc->sc_ioh, LEREG_EAR) & 0x01)
		val |= 0x8000;
	else
		val &= 0x7fff;
	CDELAY;
	DISABLE_NVRAM;
	return (val);
}

void
le_vme_etheraddr(sc)
	struct am7990_softc *sc;
{
	u_char *cp = sc->sc_arpcom.ac_enaddr;
	u_int16_t ival[3];
	int i;

	for (i = 0; i < 3; i++) {
		ival[i] = nvram_read(sc, i);
	}
	memcpy(cp, &ival[0], 6);
}

void
le_vme_wrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct le_softc *lesc = (void *)sc;

	bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, LEREG_RAP, port);
	bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, LEREG_RDP, val);
}

u_int16_t
le_vme_rdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct le_softc *lesc = (void *)sc;

	bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, LEREG_RAP, port);
	return (bus_space_read_2(lesc->sc_iot, lesc->sc_ioh, LEREG_RDP));
}

/* init board, set ipl and vec */
void
le_vme_init(sc)
	struct am7990_softc *sc;
{
	struct le_softc *lesc = (void *)sc;

	lesc->sc_csr = 0x4f;
	WRITE_CSR_AND(lesc->sc_ipl);
	SET_VEC(lesc->sc_vec);
	return;
}

/* board reset */
void
le_vme_reset(sc)
	struct am7990_softc *sc;
{
	struct le_softc *lesc = (void *)sc;

	RESET_HW;
#ifdef LEDEBUG
	if (sc->sc_debug)
		printf("%s: hardware reset\n", sc->sc_dev.dv_xname);
#endif
	SYSFAIL_CL;
}

int
le_vme_intr(sc)
	void *sc;
{
	struct le_softc *lesc = (void *)sc;
	int rc;

	rc = am7990_intr(sc);
	ENABLE_INTR;
	return (rc);
}

#if 0
void
le_vme_copytobuf_contig(sc, from, boff, len)
	struct am7990_softc *sc;
	void *from;
	int boff, len;
{
	struct le_softc *lesc = (void *)sc;

	bus_space_write_region_1(lesc->sc_memt, lesc->sc_memh, boff, from, len);
}

void
le_vme_copyfrombuf_contig(sc, to, boff, len)
	struct am7990_softc *sc;
	void *to;
	int boff, len;
{
	struct le_softc *lesc = (void *)sc;

	bus_space_read_region_1(lesc->sc_memt, lesc->sc_memh, boff, to, len);
}

void
le_vme_zerobuf_contig(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	struct le_softc *lesc = (void *)sc;

	bus_space_set_region_1(lesc->sc_memt, lesc->sc_memh, boff, 0, len);
}
#endif

int
le_vme_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct vme_attach_args *vaa = args;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh;
	int rc;

	/* we expect a32 and a16 locators */
	if (vaa->vaa_addr_a16 == (vme_addr_t)-1 ||
	    vaa->vaa_addr_a32 == (vme_addr_t)-1)
		return (0);

	/* check the dual ported memory */
	if (vmebus_get_bst(parent, VME_A32, VME_D32, &memt) != 0)
		return (0);
	if (bus_space_map(memt, vaa->vaa_addr_a32, PAGE_SIZE, 0, &ioh) != 0)
		return (0);
	rc = badaddr((vaddr_t)bus_space_vaddr(memt, ioh), 2);
	bus_space_unmap(memt, ioh, PAGE_SIZE);
	vmebus_release_bst(parent, memt);

	/* check the control space */
	if (vmebus_get_bst(parent, VME_A16, VME_D16, &iot) != 0)
		return (0);
	if (bus_space_map(iot, vaa->vaa_addr_a16, PAGE_SIZE, 0, &ioh) != 0)
		return (0);
	rc |= badaddr((vaddr_t)bus_space_vaddr(iot, ioh), 2);
	bus_space_unmap(iot, ioh, PAGE_SIZE);
	vmebus_release_bst(parent, iot);

	return (rc == 0);
}

void
le_vme_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct vme_attach_args *vaa = aux;

	/*
	 * Allocate an interrupt vector.
	 */
	lesc->sc_ipl = vaa->vaa_ipl == 0 ? IPL_NET : vaa->vaa_ipl;
	if (vmeintr_allocate(1, VMEINTR_ANY | VMEINTR_SHARED, lesc->sc_ipl,
	    &lesc->sc_vec) != 0) {
		printf(": no more interrupts!\n");
		return;
	}
	printf(" vec %x", lesc->sc_vec);

	/*
	 * Map the dual-ported memory.
	 */
	if (vmebus_get_bst(parent, VME_A32, VME_D32, &lesc->sc_memt) != 0) {
		printf(": can't map memory\n");
		return;
	}
	if (bus_space_map(lesc->sc_memt, vaa->vaa_addr_a32, VLEMEMSIZE,
	    BUS_SPACE_MAP_LINEAR, &lesc->sc_memh) != 0) {
		printf(": can't map memory\n");
		goto fail3;
	}

	/*
	 * Map the control space.
	 */
	if (vmebus_get_bst(parent, VME_A16, VME_D16, &lesc->sc_iot) != 0) {
		printf(": can't map registers\n");
		goto fail2;
	}
	if (bus_space_map(lesc->sc_iot, vaa->vaa_addr_a16, PAGE_SIZE,
	    0, &lesc->sc_ioh) != 0) {
		printf(": can't map registers\n");
		goto fail1;
	}

	sc->sc_mem = (void *)bus_space_vaddr(lesc->sc_memt, lesc->sc_memh);
	sc->sc_memsize = VLEMEMSIZE;
	sc->sc_addr = vaa->vaa_addr_a32 & 0x00ffffff;
	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_hwreset = le_vme_reset;
	sc->sc_rdcsr = le_vme_rdcsr;
	sc->sc_wrcsr = le_vme_wrcsr;
	sc->sc_hwinit = le_vme_init;
#if 0
	sc->sc_copytodesc = le_vme_copytobuf_contig;
	sc->sc_copyfromdesc = le_vme_copyfrombuf_contig;
	sc->sc_copytobuf = le_vme_copytobuf_contig;
	sc->sc_copyfrombuf = le_vme_copyfrombuf_contig;
	sc->sc_zerobuf = le_vme_zerobuf_contig;
#else
	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;
#endif

	/* get Ethernet address */
	le_vme_etheraddr(sc);

	am7990_config(sc);

	/* connect the interrupt */
	lesc->sc_ih.ih_fn = le_vme_intr;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_flags = 0;
	lesc->sc_ih.ih_ipl = lesc->sc_ipl;
	vmeintr_establish(lesc->sc_vec, &lesc->sc_ih, self->dv_xname);

	return;

fail1:
	vmebus_release_bst(parent, lesc->sc_iot);
fail2:
	bus_space_unmap(lesc->sc_memt, lesc->sc_memh, VLEMEMSIZE);
fail3:
	vmebus_release_bst(parent, lesc->sc_memt);
}
