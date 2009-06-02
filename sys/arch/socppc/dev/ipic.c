/*	$OpenBSD: ipic.c,v 1.7 2009/06/02 21:38:10 drahn Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/intr.h>

#define IPIC_SICFR	0x00
#define IPIC_SIVCR	0x04
#define IPIC_SIPNR_H	0x08
#define IPIC_SIPNR_L	0x0c
#define IPIC_SIPRR_A	0x10
#define IPIC_SIPRR_D	0x1c
#define IPIC_SIMSR_H	0x20
#define IPIC_SIMSR_L	0x24
#define IPIC_SICNR	0x28
#define IPIC_SEPNR	0x2c
#define IPIC_SMPRR_A	0x30
#define IPIC_SMPRR_B	0x34
#define IPIC_SEMSR	0x38
#define IPIC_SECNR	0x3c
#define IPIC_SERSR	0x40
#define IPIC_SERMR	0x44
#define IPIC_SERCR	0x48
#define IPIC_SIFCR_H	0x50
#define IPIC_SIFCR_L	0x54
#define IPIC_SEFCR	0x58
#define IPIC_SERFR	0x5c
#define IPIC_SCVCR	0x60
#define IPIC_SMVCR	0x64

#define IPIC_NVEC	128

#define IPIC_EXTERNAL(ivec) ((ivec) >= 17 && (ivec) <= 23)

struct ipic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	uint32_t		sc_simsr_h[IPL_NUM];
	uint32_t		sc_simsr_l[IPL_NUM];
	uint32_t		sc_semsr[IPL_NUM];
};

uint32_t ipic_imask;
struct intrq ipic_handler[IPIC_NVEC];
struct ipic_softc *ipic_sc;

int	ipic_match(struct device *, void *, void *);
void	ipic_attach(struct device *, struct device *, void *);

struct cfattach ipic_ca = {
	sizeof(struct ipic_softc), ipic_match, ipic_attach
};

struct cfdriver ipic_cd = {
	NULL, "ipic", DV_DULL
};

uint32_t ipic_read(struct ipic_softc *, bus_addr_t);
void	ipic_write(struct ipic_softc *, bus_addr_t, uint32_t);
uint32_t ipic_simsr_h(int);
uint32_t ipic_simsr_l(int);
uint32_t ipic_semsr(int);
void	ipic_calc_masks(void);

void	ext_intr(void);

ppc_splraise_t ipic_splraise;
ppc_spllower_t ipic_spllower;
ppc_splx_t ipic_splx;

void	ipic_setipl(int);
void	ipic_do_pending(int);


int
ipic_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
ipic_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipic_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;
	struct intrq *iq;
	int i;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_offset, 128, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	ipic_sc = sc;

	for (i = 0; i < IPIC_NVEC; i++) {
		iq = &ipic_handler[i];
		TAILQ_INIT(&iq->iq_list);
	}

	ppc_smask_init();
	ppc_intr_func.raise = ipic_splraise;
	ppc_intr_func.lower = ipic_spllower;
	ppc_intr_func.x = ipic_splx;

	printf("\n");
}

uint32_t
ipic_read(struct ipic_softc *sc, bus_addr_t addr)
{
	return (letoh32(bus_space_read_4(sc->sc_iot, sc->sc_ioh, addr)));
}

void
ipic_write(struct ipic_softc *sc, bus_addr_t addr, uint32_t data)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, addr, htole32(data));
}

uint32_t
ipic_simsr_h(int ivec)
{
	switch (ivec) {
	case 9:
		return 0x00000080;
	case 10:
		return 0x00000040;
	case 32:
		return 0x80000000;
	case 33:
		return 0x40000000;
	case 34:
		return 0x20000000;
	case 35:
		return 0x10000000;
	case 36:
		return 0x08000000;
	case 37:
		return 0x04000000;
	case 39:
		return 0x01000000;
	}

	return 0;
}

uint32_t
ipic_simsr_l(int ivec)
{
	return 0;
}

uint32_t
ipic_semsr(int ivec)
{
	switch (ivec) {
	case 17:
		return 0x40000000;
	case 18:
		return 0x20000000;
	case 19:
		return 0x10000000;
	case 20:
		return 0x08000000;
	case 21:
		return 0x04000000;
	case 22:
		return 0x02000000;
	case 23:
		return 0x01000000;
	}

	return 0;
}

void
ipic_calc_masks(void)
{
	struct ipic_softc *sc = ipic_sc;

	sc->sc_simsr_h[IPL_NET] |= sc->sc_simsr_h[IPL_BIO];
	sc->sc_simsr_h[IPL_TTY] |= sc->sc_simsr_h[IPL_NET];
	sc->sc_simsr_h[IPL_VM] |= sc->sc_simsr_h[IPL_TTY];
	sc->sc_simsr_h[IPL_CLOCK] |= sc->sc_simsr_h[IPL_VM];
	sc->sc_simsr_h[IPL_HIGH] |= sc->sc_simsr_h[IPL_CLOCK];

	sc->sc_simsr_l[IPL_NET] |= sc->sc_simsr_l[IPL_BIO];
	sc->sc_simsr_l[IPL_TTY] |= sc->sc_simsr_l[IPL_NET];
	sc->sc_simsr_l[IPL_VM] |= sc->sc_simsr_l[IPL_TTY];
	sc->sc_simsr_l[IPL_CLOCK] |= sc->sc_simsr_l[IPL_VM];
	sc->sc_simsr_l[IPL_HIGH] |= sc->sc_simsr_l[IPL_CLOCK];

	sc->sc_semsr[IPL_NET] |= sc->sc_semsr[IPL_BIO];
	sc->sc_semsr[IPL_TTY] |= sc->sc_semsr[IPL_NET];
	sc->sc_semsr[IPL_VM] |= sc->sc_semsr[IPL_TTY];
	sc->sc_semsr[IPL_CLOCK] |= sc->sc_semsr[IPL_VM];
	sc->sc_semsr[IPL_HIGH] |= sc->sc_semsr[IPL_CLOCK];
}

void *
intr_establish(int ivec, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *name)
{
	struct ipic_softc *sc = ipic_sc;
	struct intrhand *ih;
	struct intrq *iq;
	uint32_t mask;
	int s;

	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("%s: malloc failed", __func__);
	iq = &ipic_handler[ivec];

	if (ivec < 0 || ivec >= IPIC_NVEC)
		panic("%s: invalid vector %d", __func__, ivec);

	sc->sc_simsr_h[level] |= ipic_simsr_h(ivec);
	sc->sc_simsr_l[level] |= ipic_simsr_l(ivec);
	sc->sc_semsr[level] |= ipic_semsr(ivec);

	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_level = level;
	ih->ih_irq = ivec;

	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_irq,
	    &evcount_intr);

	/*
	 * Append handler to end of list
	 */
	s = ppc_intr_disable();

	TAILQ_INSERT_TAIL(&iq->iq_list, ih, ih_list);
	ipic_calc_masks();

	ppc_intr_enable(s);

	/* Unmask the interrupt. */
	mask = ipic_read(sc, IPIC_SIMSR_H);
	mask |= ipic_simsr_h(ivec);
	ipic_write(sc, IPIC_SIMSR_H, mask);
	mask = ipic_read(sc, IPIC_SIMSR_L);
	mask |= ipic_simsr_l(ivec);
	ipic_write(sc, IPIC_SIMSR_L, mask);
	mask = ipic_read(sc, IPIC_SEMSR);
	mask |= ipic_semsr(ivec);
	ipic_write(sc, IPIC_SEMSR, mask);

	return (ih);
}

void
ext_intr(void)
{
	struct cpu_info *ci = curcpu();
	struct ipic_softc *sc = ipic_sc;
	struct intrhand *ih;
	struct intrq *iq;
	int pcpl;
	int ivec;

	pcpl = ci->ci_cpl;
	ivec = ipic_read(sc, IPIC_SIVCR) & 0x7f;

	iq = &ipic_handler[ivec];
	TAILQ_FOREACH(ih, &iq->iq_list, ih_list) {
		if (ih->ih_level < pcpl)
			continue;

		ipic_splraise(ih->ih_level);
		ppc_intr_enable(1);

		KERNEL_LOCK();
		if ((*ih->ih_fun)(ih->ih_arg))
			ih->ih_count.ec_count++;
		KERNEL_UNLOCK();

		ppc_intr_disable();
	}

	splx(pcpl);
}

int
ipic_splraise(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int ocpl = ci->ci_cpl;

	if (ocpl > newcpl)
		newcpl = ocpl;

	ipic_setipl(newcpl);

	return (ocpl);
}

int
ipic_spllower(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int ocpl = ci->ci_cpl;

	ipic_splx(newcpl);

	return (ocpl);
}

void
ipic_splx(int newcpl)
{
	struct cpu_info *ci = curcpu();

	ipic_setipl(newcpl);
	if (ci->ci_ipending & ppc_smask[newcpl])
		ipic_do_pending(newcpl);
}

void
ipic_setipl(int ipl)
{
	struct cpu_info *ci = curcpu();
	struct ipic_softc *sc = ipic_sc;
	uint32_t mask;
	int s;

	s = ppc_intr_disable();
	ci->ci_cpl = ipl;
	mask = sc->sc_simsr_h[IPL_HIGH] & ~sc->sc_simsr_h[ipl];
	ipic_write(sc, IPIC_SIMSR_H, mask);
	mask = sc->sc_simsr_l[IPL_HIGH] & ~sc->sc_simsr_l[ipl];
	ipic_write(sc, IPIC_SIMSR_L, mask);
	mask = sc->sc_semsr[IPL_HIGH] & ~sc->sc_semsr[ipl];
	ipic_write(sc, IPIC_SEMSR, mask);
	ppc_intr_enable(s);
}

void
ipic_do_pending(int pcpl)
{
	struct cpu_info *ci = curcpu();
	int s;

	s = ppc_intr_disable();
	if (ci->ci_iactive & CI_IACTIVE_PROCESSING_SOFT) {
		ppc_intr_enable(s);
		return;
	}

	atomic_setbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_SOFT);

	do {
		if ((ci->ci_ipending & SI_TO_IRQBIT(SI_SOFTNET)) &&
		    (pcpl < IPL_SOFTNET)) {
			extern int netisr;
			int pisr;
		       
			ci->ci_ipending &= ~SI_TO_IRQBIT(SI_SOFTNET);
			ci->ci_cpl = IPL_SOFTNET;
			ppc_intr_enable(s);
			KERNEL_LOCK();
			while ((pisr = netisr) != 0) {
				atomic_clearbits_int(&netisr, pisr);
				softnet(pisr);
			}
			KERNEL_UNLOCK();
			ppc_intr_disable();
			continue;
		}
		if ((ci->ci_ipending & SI_TO_IRQBIT(SI_SOFTCLOCK)) &&
		    (pcpl < IPL_SOFTCLOCK)) {
			ci->ci_ipending &= ~SI_TO_IRQBIT(SI_SOFTCLOCK);
			ci->ci_cpl = IPL_SOFTCLOCK;
			ppc_intr_enable(s);
			KERNEL_LOCK();
			softclock();
			KERNEL_UNLOCK();
			ppc_intr_disable();
			continue;
		}
	} while (ci->ci_ipending & ppc_smask[pcpl]);
	ipic_setipl(pcpl);	/* Don't use splx... we are here already! */

	atomic_clearbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_SOFT);
	ppc_intr_enable(s);
}
