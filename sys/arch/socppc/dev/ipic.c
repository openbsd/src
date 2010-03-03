/*	$OpenBSD: ipic.c,v 1.12 2010/03/03 21:52:13 kettenis Exp $	*/

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

#include <dev/ofw/openfirm.h>

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
struct intrhand *ipic_intrhand[IPIC_NVEC];
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

void	intr_calculatemasks(void);
void	ext_intr(void);
void	ipic_do_pending_int(void);

int
ipic_match(struct device *parent, void *cfdata, void *aux)
{
	struct obio_attach_args *oa = aux;
	char buf[32];

	if (OF_getprop(oa->oa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "ipic") != 0)
		return (0);

	return (1);
}

void
ipic_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipic_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;
	int ivec;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_offset, 128, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	ipic_sc = sc;

	/*
	 * Deal with pre-established interrupts.
	 */
	for (ivec = 0; ivec < IPIC_NVEC; ivec++) {
		if (ipic_intrhand[ivec]) {
			int level = ipic_intrhand[ivec]->ih_level;
			uint32_t mask;

			sc->sc_simsr_h[level] |= ipic_simsr_h(ivec);
			sc->sc_simsr_l[level] |= ipic_simsr_l(ivec);
			sc->sc_semsr[level] |= ipic_semsr(ivec);
			intr_calculatemasks();

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
		}
	}

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
	case 48:
		return 0x80000000;
	}

	return 0;
}

void
intr_calculatemasks(void)
{
	struct ipic_softc *sc = ipic_sc;
	int level;

	for (level = IPL_NONE; level < IPL_NUM; level++)
		imask[level] = SINT_ALLMASK | (1 << level);

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_TTY] |= imask[IPL_NET];
	imask[IPL_VM] |= imask[IPL_TTY];
	imask[IPL_CLOCK] |= imask[IPL_VM] | SPL_CLOCKMASK;

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0x00000000;
	imask[IPL_HIGH] = 0xffffffff;

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
	struct intrhand **p, *q, *ih;
	uint32_t mask;

	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("%s: malloc failed", __func__);

	if (ivec < 0 || ivec >= IPIC_NVEC)
		panic("%s: invalid vector %d", __func__, ivec);

	for (p = &ipic_intrhand[ivec]; (q = *p) != NULL; p = &q->ih_next)
		;

	if (sc) {
		sc->sc_simsr_h[level] |= ipic_simsr_h(ivec);
		sc->sc_simsr_l[level] |= ipic_simsr_l(ivec);
		sc->sc_semsr[level] |= ipic_semsr(ivec);
		intr_calculatemasks();
	}

	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = ivec;
	evcount_attach(&ih->ih_count, name, NULL, &evcount_intr);
	*p = ih;

	if (sc) {
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
	}

	return (ih);
}

void
ext_intr(void)
{
	struct cpu_info *ci = curcpu();
	struct ipic_softc *sc = ipic_sc;
	struct intrhand *ih;
	uint32_t simsr_h, simsr_l, semsr;
	int pcpl, ocpl;
	int ivec;

	pcpl = ci->ci_cpl;
	ivec = ipic_read(sc, IPIC_SIVCR) & 0x7f;

	simsr_h = ipic_read(sc, IPIC_SIMSR_H);
	simsr_l = ipic_read(sc, IPIC_SIMSR_L);
	semsr = ipic_read(sc, IPIC_SEMSR);
	ipic_write(sc, IPIC_SIMSR_H, simsr_h & ~ipic_simsr_h(ivec));
	ipic_write(sc, IPIC_SIMSR_L, simsr_l & ~ipic_simsr_l(ivec));
	ipic_write(sc, IPIC_SEMSR, semsr & ~ipic_semsr(ivec));

	ih = ipic_intrhand[ivec];
	while (ih) {
		if (ci->ci_cpl & (1 << ih->ih_level)) {
			ci->ci_ipending |= (1 << ih->ih_level);
			return;
		}

		ipic_write(sc, IPIC_SIMSR_H, sc->sc_simsr_h[ih->ih_level]);
		ipic_write(sc, IPIC_SIMSR_L, sc->sc_simsr_l[ih->ih_level]);
		ipic_write(sc, IPIC_SEMSR, sc->sc_semsr[ih->ih_level]);
		ocpl = splraise(imask[ih->ih_level]);
		ppc_intr_enable(1);

		KERNEL_LOCK();
		if ((*ih->ih_fun)(ih->ih_arg))
			ih->ih_count.ec_count++;
		KERNEL_UNLOCK();

		ppc_intr_disable();
		ci->ci_cpl = ocpl;
		ih = ih->ih_next;
	}

	ipic_write(sc, IPIC_SIMSR_H, simsr_h);
	ipic_write(sc, IPIC_SIMSR_L, simsr_l);
	ipic_write(sc, IPIC_SEMSR, semsr);
	splx(pcpl);
}

static __inline int
cntlzw(int x)
{
	int a;

	__asm __volatile("cntlzw %0,%1" : "=r"(a) : "r"(x));

	return a;
}

void
ipic_do_pending_int(void)
{
	struct cpu_info *ci = curcpu();
	struct ipic_softc *sc = ipic_sc;
	uint32_t mask;
	int level;

	ci->ci_ipending &= SINT_ALLMASK;
	level = cntlzw(31 - (ci->ci_cpl & ~(SPL_CLOCKMASK|SINT_ALLMASK)));
	mask = sc->sc_simsr_h[IPL_HIGH] & ~sc->sc_simsr_h[level];
	ipic_write(sc, IPIC_SIMSR_H, mask);
	mask = sc->sc_simsr_l[IPL_HIGH] & ~sc->sc_simsr_l[level];
	ipic_write(sc, IPIC_SIMSR_L, mask);
	mask = sc->sc_semsr[IPL_HIGH] & ~sc->sc_semsr[level];
	ipic_write(sc, IPIC_SEMSR, mask);
}
