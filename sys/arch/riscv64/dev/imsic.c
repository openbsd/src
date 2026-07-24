/*	$OpenBSD: imsic.c,v 1.1 2026/07/24 10:51:00 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/evcount.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/sbi.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include "riscv64/dev/riscv_cpu_intc.h"

#define IMSIC_EIDELIVERY		0x70
#define  IMSIC_EIDELIVERY_ENABLE	(1 << 0)
#define IMSIC_EITHRESHOLD		0x72
#define IMSIC_EIP(x)			(0x80 + (x))
#define IMSIC_EIE(x)			(0xc0 + (x))

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct imsic_intrhand;

struct imsic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	bus_addr_t		sc_msi_addr;

	uint32_t		sc_nmsi;
	uint32_t		sc_guest_index_bits;
	uint16_t		sc_threshold[NIPL];

	struct imsic_intrhand	**sc_ih;

	struct interrupt_controller sc_msi_ic;
};

struct imsic_intrhand {
	struct imsic_softc	*ih_sc;
	int			(*ih_func)(void *);
	void			*ih_arg;
	int			ih_ipl;
	int			ih_flags;
	int			ih_msi;
	struct evcount		ih_count;
	struct cpu_info		*ih_ci;
};

struct imsic_softc *imsic_sc;

int	imsic_match(struct device *, void *, void *);
void	imsic_attach(struct device *, struct device *, void *);

const struct cfattach imsic_ca = {
	sizeof (struct imsic_softc), imsic_match, imsic_attach
};

struct cfdriver imsic_cd = {
	NULL, "imsic", DV_DULL
};

void	imsic_csr_write(uint64_t, uint64_t);
uint64_t imsic_csr_read(uint64_t);

int	imsic_spllower(int);
int	imsic_splraise(int);
void	imsic_splx(int);
void	imsic_setipl(int);
int	imsic_intr(void *);

void	*imsic_intr_establish_msi(void *, uint64_t *, uint64_t *,
	    int, struct cpu_info *, int (*)(void *), void *, char *);
void	imsic_intr_disestablish_msi(void *);
void	imsic_barrier(void *);

int
imsic_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "riscv,imsics");
}

void
imsic_attach(struct device *parent, struct device *self, void *aux)
{
	struct imsic_softc *sc = (struct imsic_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint64_t reg;
	int shift;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	printf("\n");

	sc->sc_nmsi = OF_getpropint(faa->fa_node, "riscv,num-ids", 63) + 1;
	sc->sc_guest_index_bits =
		OF_getpropint(faa->fa_node, "riscv,guest-index-bits", 0);

	/*
	 * Each MSI has its own priority, with lower numbered MSIs
	 * having higher priority.  The architecture guarantees that
	 * we have at least 63 available MSIs.  Divide these up among
	 * IPL_AUDIO, IPL_TTY, IPL_NET and IPL_BIO.  Clock interrupts
	 * and IPIs are handled elsewhere so we don't need to worry
	 * about IPL_CLOCK and IPL_IPI.  If we have more MSIs, divide
	 * them up proportionally by shifting the thresholds.
	 */
	shift = ffs(sc->sc_nmsi / 64) - 1;
	sc->sc_threshold[IPL_NONE] = 0;
	sc->sc_threshold[IPL_SOFTCLOCK] = 0;
	sc->sc_threshold[IPL_SOFTNET] = 0;
	sc->sc_threshold[IPL_SOFTTTY] = 0;
	sc->sc_threshold[IPL_IPI] = 1;
	sc->sc_threshold[IPL_HIGH] = 1;
	sc->sc_threshold[IPL_CLOCK] = 1;
	sc->sc_threshold[IPL_AUDIO] = 1;
	sc->sc_threshold[IPL_VM] = 8 << shift;
	sc->sc_threshold[IPL_TTY] = 8 << shift;
	sc->sc_threshold[IPL_NET] = 16 << shift;
	sc->sc_threshold[IPL_BIO] = 56 << shift;

	sc->sc_msi_addr = faa->fa_reg[0].addr;

	sc->sc_ih = mallocarray(sizeof(*sc->sc_ih), sc->sc_nmsi,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	KASSERT(imsic_sc == NULL);
	imsic_sc = sc;

	riscv_intc_intr_establish(IRQ_EXTERNAL_SUPERVISOR, 0,
	     imsic_intr, sc, sc->sc_dev.dv_xname);
	riscv_set_intr_func(imsic_splraise, imsic_spllower,
	     imsic_splx, imsic_setipl);

	/* Disable all interrupts. */
	for (reg = IMSIC_EIE(0); reg <= IMSIC_EIE((sc->sc_nmsi - 1) / 32);
	     reg += 2)
		imsic_csr_write(reg, 0);

	/* Set threshold as high as possible. */
	imsic_csr_write(IMSIC_EITHRESHOLD, 1);

	/* Enable delivery. */
	imsic_csr_write(IMSIC_EIDELIVERY, IMSIC_EIDELIVERY_ENABLE);

	csr_set(sie, SIE_SEIE);

	sc->sc_msi_ic.ic_node = faa->fa_node;
	sc->sc_msi_ic.ic_cookie = sc;
	sc->sc_msi_ic.ic_establish_msi = imsic_intr_establish_msi;
	sc->sc_msi_ic.ic_disestablish = imsic_intr_disestablish_msi;
	sc->sc_msi_ic.ic_barrier = imsic_barrier;
	fdt_intr_register(&sc->sc_msi_ic);
}

void
imsic_csr_write(uint64_t reg, uint64_t val)
{
	csr_write(siselect, reg);
	csr_write(sireg, val);
}

uint64_t
imsic_csr_read(uint64_t reg)
{
	csr_write(siselect, reg);
	return csr_read(sireg);
}

void
imsic_splx(int new)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & riscv_smask[new])
		riscv_do_pending_intr(new);

	imsic_setipl(new);
}

int
imsic_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	imsic_splx(new);
	return old;
}

int
imsic_splraise(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	if (old > new)
		new = old;

	imsic_setipl(new);
	return old;
}

void
imsic_setipl(int new)
{
	struct imsic_softc *sc = imsic_sc;
	struct cpu_info *ci = curcpu();
	u_long sie;

	sie = intr_disable();
	ci->ci_cpl = new;
	imsic_csr_write(IMSIC_EITHRESHOLD, sc->sc_threshold[new]);

	/* trigger deferred timer interrupt if cpl is now low enough */
	if (ci->ci_timer_deferred && new < IPL_CLOCK)
		sbi_set_timer(0);

	intr_restore(sie);
}

int
imsic_intr(void *arg)
{
	struct imsic_softc *sc = arg;
	struct imsic_intrhand *ih;
	uint64_t stopei;
	u_int msi;
	int need_lock;
	int handled;
	int s;

	stopei = csr_swap(stopei, 0);
	msi = (stopei & STOPEI_ID_MASK) >> STOPEI_ID_SHIFT;
	if (msi >= sc->sc_nmsi)
		return 0;

	ih = sc->sc_ih[msi];
	if (ih == NULL)
		return 0;

	s = imsic_splraise(ih->ih_ipl);

	if (ih->ih_flags & IPL_MPSAFE)
		need_lock = 0;
	else
		need_lock = s < IPL_SCHED;

	if (need_lock)
		KERNEL_LOCK();

	intr_enable();
	handled = ih->ih_func(ih->ih_arg);
	intr_disable();
	if (handled)
		ih->ih_count.ec_count++;
	
	if (need_lock)
		KERNEL_UNLOCK();

	imsic_splx(s);
	return handled;
}

uint64_t
imsic_msi_addr(struct imsic_softc *sc, struct cpu_info *ci)
{
	uint64_t hart_offset;

	/* XXX This assumes hart ID and hart index are the same. */
	hart_offset = ci->ci_hartid << (PAGE_SHIFT + sc->sc_guest_index_bits);
	return sc->sc_msi_addr | hart_offset;
}

void *
imsic_intr_establish_msi(void *cookie, uint64_t *addr, uint64_t *data,
    int level, struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct imsic_softc *sc = cookie;
	struct imsic_intrhand *ih;
	uint64_t eie, reg;
	int msi;

	if (ci == NULL)
		ci = &cpu_info_primary;

	msi = sc->sc_threshold[level & IPL_IRQMASK];
	for (; msi < sc->sc_nmsi; msi++) {
		if (sc->sc_ih[msi] == NULL)
			break;
	}
	if (msi == sc->sc_nmsi)
		return NULL;

	ih = malloc(sizeof(struct imsic_intrhand), M_DEVBUF, M_WAITOK);
	ih->ih_sc = sc;
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level & IPL_IRQMASK;
	ih->ih_flags = level & IPL_FLAGMASK;
	ih->ih_msi = msi;
	ih->ih_ci = ci;
	sc->sc_ih[msi] = ih;

	if (name)
		evcount_attach(&ih->ih_count, name, &ih->ih_msi);

	/* Enable interrupt. */
	reg = IMSIC_EIE((msi / 64) * 2);
	eie = imsic_csr_read(reg);
	imsic_csr_write(reg, eie | (1ULL << (msi % 64)));

	*addr = imsic_msi_addr(sc, ci);
	*data = msi;
	return ih;
}

void
imsic_intr_disestablish_msi(void *cookie)
{
	struct imsic_intrhand *ih = cookie;
	struct imsic_softc *sc = ih->ih_sc;
	uint64_t eie, reg;

	/* Disable interrupt. */
	reg = IMSIC_EIE((ih->ih_msi / 64) * 2);
	eie = imsic_csr_read(reg);
	imsic_csr_write(reg, eie & ~(1ULL << (ih->ih_msi % 64)));

	sc->sc_ih[ih->ih_msi] = NULL;
	free(ih, M_DEVBUF, sizeof(*ih));
}

void
imsic_barrier(void *cookie)
{
	struct imsic_intrhand *ih = cookie;

	sched_barrier(ih->ih_ci);
}
