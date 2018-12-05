/*	$OpenBSD: pvclock.c,v 1.3 2018/12/05 18:02:51 reyk Exp $	*/

/*
 * Copyright (c) 2018 Reyk Floeter <reyk@openbsd.org>
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

#if !defined(__i386__) && !defined(__amd64__)
#error pvclock(4) is only supported on i386 and amd64
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/atomic.h>

#include <machine/cpu.h>
#include <uvm/uvm_extern.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/pvreg.h>

struct pvclock_softc {
	struct device		 sc_dev;
	void			*sc_time;
	paddr_t			 sc_paddr;
	struct timecounter	*sc_tc;
};

struct pvclock_wall_clock {
	uint32_t		 wc_version;
	uint32_t		 wc_sec;
	uint32_t		 wc_nsec;
} __packed;

struct pvclock_time_info {
	uint32_t		 ti_version;
	uint32_t		 ti_pad0;
	uint64_t		 ti_tsc_timestamp;
	uint64_t		 ti_system_time;
	uint32_t		 ti_tsc_to_system_mul;
	int8_t			 ti_tsc_shift;
	uint8_t			 ti_flags;
	uint8_t			 ti_pad[2];
} __packed;

#define PVCLOCK_FLAG_TSC_STABLE		0x01
#define PVCLOCK_SYSTEM_TIME_ENABLE	0x01
#define DEVNAME(_s)			((_s)->sc_dev.dv_xname)

int	 pvclock_match(struct device *, void *, void *);
void	 pvclock_attach(struct device *, struct device *, void *);
int	 pvclock_activate(struct device *, int);

uint	 pvclock_get_timecount(struct timecounter *);
void	 pvclock_read_time_info(struct pvclock_softc *,
	    struct pvclock_time_info *);

static inline uint32_t
	 pvclock_read_begin(const struct pvclock_time_info *);
static inline int
	 pvclock_read_done(const struct pvclock_time_info *, uint32_t);

struct cfattach pvclock_ca = {
	sizeof(struct pvclock_softc),
	pvclock_match,
	pvclock_attach,
	NULL,
	pvclock_activate
};

struct cfdriver pvclock_cd = {
	NULL,
	"pvclock",
	DV_DULL
};

struct timecounter pvclock_timecounter = {
	pvclock_get_timecount, NULL, ~0u, 0, NULL, -2000, NULL
};

int
pvclock_match(struct device *parent, void *match, void *aux)
{
	struct pv_attach_args	*pva = aux;
	struct pvbus_hv		*hv;

	/*
	 * pvclock is provided by different hypervisors, we currently
	 * only support the "kvmclock".
	 */
	hv = &pva->pva_hv[PVBUS_KVM];
	if (hv->hv_base != 0) {
		/*
		 * We only implement support for the 2nd version of pvclock.
		 * The first version is basically the same but with different
		 * non-standard MSRs and it is deprecated.
		 */
		if ((hv->hv_features & (1 << KVM_FEATURE_CLOCKSOURCE2)) == 0)
			return (0);

		/*
		 * Only the "stable" clock with a sync'ed TSC is supported.
		 * In this case the host guarantees that the TSC is constant
		 * and invariant, either by the underlying TSC or by passing
		 * on a synchronized value.
		 */
		if ((hv->hv_features &
		    (1 << KVM_FEATURE_CLOCKSOURCE_STABLE_BIT)) == 0)
			return (0);

		return (1);
	}

	return (0);
}

void
pvclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct pvclock_softc		*sc = (struct pvclock_softc *)self;
	struct pvclock_time_info	*ti;
	paddr_t			 	 pa;
	uint32_t			 version;
	uint8_t				 flags;

	if ((sc->sc_time = km_alloc(PAGE_SIZE,
	    &kv_any, &kp_zero, &kd_nowait)) == NULL) {
		printf(": time page allocation failed\n");
		return;
	}
	if (!pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_time, &pa)) {
		printf(": time page PA extraction failed\n");
		km_free(sc->sc_time, PAGE_SIZE, &kv_any, &kp_zero);
		return;
	}

	wrmsr(KVM_MSR_SYSTEM_TIME, pa | PVCLOCK_SYSTEM_TIME_ENABLE);
	sc->sc_paddr = pa;

	ti = sc->sc_time;
	do {
		version = pvclock_read_begin(ti);
		flags = ti->ti_flags;
	} while (!pvclock_read_done(ti, version));

	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0) {
		wrmsr(KVM_MSR_SYSTEM_TIME, pa & ~PVCLOCK_SYSTEM_TIME_ENABLE);
		km_free(sc->sc_time, PAGE_SIZE, &kv_any, &kp_zero);
		printf(": unstable clock\n");
		return;
	}

	sc->sc_tc = &pvclock_timecounter;
	sc->sc_tc->tc_name = DEVNAME(sc);
	sc->sc_tc->tc_frequency = 1000000000ULL;
	sc->sc_tc->tc_priv = sc;

	/* Better than HPET but below TSC */
	sc->sc_tc->tc_quality = 1500;

	tc_init(sc->sc_tc);

	printf("\n");
}

int
pvclock_activate(struct device *self, int act)
{
	struct pvclock_softc	*sc = (struct pvclock_softc *)self;
	int			 rv = 0;
	paddr_t			 pa = sc->sc_paddr;

	switch (act) {
	case DVACT_POWERDOWN:
		wrmsr(KVM_MSR_SYSTEM_TIME, pa & ~PVCLOCK_SYSTEM_TIME_ENABLE);
		break;
	case DVACT_RESUME:
		wrmsr(KVM_MSR_SYSTEM_TIME, pa | PVCLOCK_SYSTEM_TIME_ENABLE);
		break;
	}

	return (rv);
}

static inline uint32_t
pvclock_read_begin(const struct pvclock_time_info *ti)
{
	uint32_t version = ti->ti_version & ~0x1;
	virtio_membar_sync();
	return (version);
}

static inline int
pvclock_read_done(const struct pvclock_time_info *ti,
    uint32_t version)
{
	virtio_membar_sync();
	return (ti->ti_version == version);
}

uint
pvclock_get_timecount(struct timecounter *tc)
{
	struct pvclock_softc		*sc = tc->tc_priv;
	struct pvclock_time_info	*ti;
	uint64_t			 tsc_timestamp, system_time, delta, ctr;
	uint32_t			 version, mul_frac;
	int8_t				 shift;
	uint8_t				 flags;

	ti = sc->sc_time;
	do {
		version = pvclock_read_begin(ti);
		system_time = ti->ti_system_time;
		tsc_timestamp = ti->ti_tsc_timestamp;
		mul_frac = ti->ti_tsc_to_system_mul;
		shift = ti->ti_tsc_shift;
		flags = ti->ti_flags;
	} while (!pvclock_read_done(ti, version));

	/* This bit must be set as we attached based on the stable flag */
	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0)
		panic("%s: unstable result on stable clock", DEVNAME(sc));

	/*
	 * The algorithm is described in
	 * linux/Documentation/virtual/kvm/msr.txt
	 */
	delta = rdtsc() - tsc_timestamp;
	if (shift < 0)
		delta >>= -shift;
	else
		delta <<= shift;
	ctr = ((delta * mul_frac) >> 32) + system_time;

	return (ctr);
}
