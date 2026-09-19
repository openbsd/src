/*	$OpenBSD: lapic.c,v 1.5 2026/09/19 17:21:52 dv Exp $ */

/*
 * Copyright (c) 2025 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <machine/i82489reg.h>

#include "lapic.h"
#include "i82093aa.h"
#include "mmio.h"
#include "vmd.h"

#ifdef DPRINTF
#undef DPRINTF
#endif
#if LAPIC_DEBUG
#define DPRINTF		log_debug
#else
#define DPRINTF(x...)	do {} while(0)
#endif	/* LAPIC_DEBUG */

extern struct vmd_vm *current_vm;

#ifndef LAPIC_DLMODE_EXTINT
#define LAPIC_DLMODE_EXTINT	0x00000700
#endif

#define LAPIC_MAX_VCPUS		VMM_MAX_VCPUS_PER_VM

/* LVT entry indices */
#define LVT_TIMER	0
#define LVT_THERMAL	1
#define LVT_PCINT	2
#define LVT_LINT0	3
#define LVT_LINT1	4
#define LVT_ERROR	5
#define LVT_CMCI	6
#define LVT_COUNT	7

struct lapic {
	pthread_mutex_t mtx;
	uint64_t	base;
	uint32_t	ver;
	uint32_t	tpr;
	uint32_t	svr;
	uint32_t	id;
	uint32_t	ldr;
	uint32_t	dfr;
	uint32_t	esr;
	uint32_t	icrlo;
	uint32_t	icrhi;
	uint32_t	x2_icr_dest;
	uint32_t	lvt[LVT_COUNT];

	uint32_t	isr[8];
	uint32_t	irr[8];
	uint32_t	tmr[8];

	/* timer state */
	uint32_t	icr_timer;	/* initial count */
	uint32_t	dcr_timer;	/* divisor config */
	struct timespec	timer_start;	/* host time at ICR write */
	int		timer_running;
	int		timer_periodic;

	uint32_t	curvec;
};

/*
 * One LAPIC per vcpu. The mmio handler receives the accessing vcpu id and
 * indexes into this table. Interrupt delivery functions take a vcpu id for
 * the target lapic.
 */
static struct lapic	lapics[LAPIC_MAX_VCPUS];
static int		lapic_ncpus = 0;

static uint32_t	lapic_divisor(uint32_t);
static uint64_t	lapic_timer_ticks(struct lapic *);
static uint32_t	lapic_timer_ccr(struct lapic *);
static void	lapic_timer_reload(struct lapic *);
static int	lapic_highest_in_map(const uint32_t *);
static int	lapic_highest_pending(struct lapic *);
static uint32_t	lapic_ppr(struct lapic *);
static void	lapic_set_map(uint32_t *, int);
static void	lapic_clear_map(uint32_t *, int);
static void	lapic_reset_locked(struct lapic *, uint32_t);
static uint64_t	lapic_icr_targets(uint32_t, uint32_t, uint32_t);
static uint64_t	lapic_x2apic_targets(uint32_t, uint64_t);
static void	lapic_icr_dispatch(uint32_t, uint32_t, uint64_t);
static void	lapic_icr(uint32_t, uint32_t, uint32_t);

uint32_t
lapic_divisor(uint32_t dcr)
{
	switch (dcr & 0xb) {
	case LAPIC_DCRT_DIV1: return 1;
	case LAPIC_DCRT_DIV2: return 2;
	case LAPIC_DCRT_DIV4: return 4;
	case LAPIC_DCRT_DIV8: return 8;
	case LAPIC_DCRT_DIV16: return 16;
	case LAPIC_DCRT_DIV32: return 32;
	case LAPIC_DCRT_DIV64: return 64;
	case LAPIC_DCRT_DIV128: return 128;
	default: return 1;
	}
}

static uint64_t
lapic_timer_ticks(struct lapic *lapic)
{
	struct timespec now, delta;
	uint64_t ns;

	if (!lapic->timer_running)
		return (0);

	clock_gettime(CLOCK_MONOTONIC, &now);
	delta.tv_sec = now.tv_sec - lapic->timer_start.tv_sec;
	delta.tv_nsec = now.tv_nsec - lapic->timer_start.tv_nsec;
	if (delta.tv_nsec < 0) {
		delta.tv_sec -= 1;
		delta.tv_nsec += 1000000000L;
	}
	ns = (uint64_t)delta.tv_sec * 1000000000ULL + (uint64_t)delta.tv_nsec;

	/* A 100 MHz source advances once per 10 ns before division. */
	return ns / (10ULL * lapic_divisor(lapic->dcr_timer));
}

/*
 * Compute the current down-count without consuming an expiry.  Expiry and
 * interrupt generation belong to lapic_timer_check(); a guest polling
 * CCR must not be able to lose its timer interrupt.
 */
static uint32_t
lapic_timer_ccr(struct lapic *lapic)
{
	uint64_t ticks, phase;

	if (!lapic->timer_running || lapic->icr_timer == 0)
		return (0);

	ticks = lapic_timer_ticks(lapic);
	if (!lapic->timer_periodic) {
		if (ticks >= lapic->icr_timer)
			return 0;
		return lapic->icr_timer - ticks;
	}

	phase = ticks % lapic->icr_timer;
	return phase == 0 ? lapic->icr_timer : lapic->icr_timer - phase;
}

static void
lapic_timer_reload(struct lapic *lapic)
{
	clock_gettime(CLOCK_MONOTONIC, &lapic->timer_start);
	lapic->timer_running = (lapic->icr_timer != 0);
}

static void
lapic_reset_locked(struct lapic *lapic, uint32_t vcpu_id)
{
	int i;

	lapic->base = LAPIC_BASE;
	lapic->ver = (6U << LAPIC_VERSION_LVT_SHIFT) | 0x10;
	lapic->tpr = 0;
	lapic->svr = 0;
	lapic->id = vcpu_id << LAPIC_ID_SHIFT;
	lapic->ldr = 0;
	lapic->dfr = 0xffffffff;
	lapic->esr = 0;
	lapic->icrlo = 0;
	lapic->icrhi = 0;
	lapic->x2_icr_dest = 0;
	memset(lapic->isr, 0, sizeof(lapic->isr));
	memset(lapic->irr, 0, sizeof(lapic->irr));
	memset(lapic->tmr, 0, sizeof(lapic->tmr));
	for (i = 0; i < LVT_COUNT; i++)
		lapic->lvt[i] = LAPIC_LVT_MASKED;
	lapic->icr_timer = 0;
	lapic->dcr_timer = 0;
	memset(&lapic->timer_start, 0, sizeof(lapic->timer_start));
	lapic->timer_running = 0;
	lapic->timer_periodic = 0;
	lapic->curvec = 0;
}

void
lapic_init(uint32_t curcpu)
{
	struct lapic *lapic = &lapics[curcpu];

	memset(lapic, 0, sizeof(*lapic));
	if (pthread_mutex_init(&lapic->mtx, NULL) != 0)
		fatalx("%s: could not initialize LAPIC mutex", __func__);
	lapic_reset_locked(lapic, curcpu);

	if ((int)curcpu >= lapic_ncpus)
		lapic_ncpus = curcpu + 1;

	/*
	 * Only register the MMIO range once - it dispatches per-vcpu via
	 * the vcpu_id argument.
	 */
	if (curcpu == 0)
		mmio_dev_add(LAPIC_BASE, LAPIC_BASE + 0xFFF,
		    lapic_mmio);
}

void
lapic_reset(uint32_t vcpu_id)
{
	struct lapic *lapic;

	if (vcpu_id >= LAPIC_MAX_VCPUS ||
	    vcpu_id >= (uint32_t)lapic_ncpus)
		return;

	lapic = &lapics[vcpu_id];
	pthread_mutex_lock(&lapic->mtx);
	lapic_reset_locked(lapic, vcpu_id);
	pthread_mutex_unlock(&lapic->mtx);
}

int
lapic_enabled(int vcpu_id)
{
	int enabled;

	if (vcpu_id < 0 || vcpu_id >= lapic_ncpus)
		return (0);

	pthread_mutex_lock(&lapics[vcpu_id].mtx);
	enabled = (lapics[vcpu_id].svr & LAPIC_SVR_ENABLE) != 0;
	pthread_mutex_unlock(&lapics[vcpu_id].mtx);

	return enabled;
}

int
lapic_extint_enabled(int vcpu_id)
{
	struct lapic *lapic;
	uint32_t lint0;
	int enabled;

	if (vcpu_id < 0 || vcpu_id >= lapic_ncpus)
		return 0;

	lapic = &lapics[vcpu_id];
	pthread_mutex_lock(&lapic->mtx);
	lint0 = lapic->lvt[LVT_LINT0];
	enabled = (lapic->svr & LAPIC_SVR_ENABLE) != 0 &&
	    (lint0 & LAPIC_LVT_MASKED) == 0 &&
	    (lint0 & LAPIC_DLMODE_MASK) == LAPIC_DLMODE_EXTINT;
	pthread_mutex_unlock(&lapic->mtx);

	return enabled;
}

int
lapic_mmio(uint32_t vcpu_id, int dir, paddr_t addr, uint8_t size,
    uint64_t *data)
{
	struct lapic *lapic;
	uint16_t reg;
	uint32_t d, icrlo = 0, icrhi = 0;
	int dispatch_icr = 0, eoi_vector = 0xffff, mapidx;

	(void)size;

	if (vcpu_id >= LAPIC_MAX_VCPUS) {
		log_warnx("%s: invalid vcpu id %u", __func__, vcpu_id);
		return 1;
	}
	lapic = &lapics[vcpu_id];

	reg = addr - lapic->base;
	if (reg > 0xFFF) {
		log_warnx("%s: invalid LAPIC register 0x%x", __func__, reg);
		return 1;
	}

	pthread_mutex_lock(&lapic->mtx);

	if (dir == MMIO_DIR_READ && (reg & 0xf) == 0) {
		if (reg >= LAPIC_ISR && reg < LAPIC_ISR + 0x80) {
			mapidx = (reg - LAPIC_ISR) >> 4;
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->isr[mapidx];
			goto out;
		}
		if (reg >= LAPIC_TMR && reg < LAPIC_TMR + 0x80) {
			mapidx = (reg - LAPIC_TMR) >> 4;
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->tmr[mapidx];
			goto out;
		}
		if (reg >= LAPIC_IRR && reg < LAPIC_IRR + 0x80) {
			mapidx = (reg - LAPIC_IRR) >> 4;
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->irr[mapidx];
			goto out;
		}
	}

	switch (reg) {
	case LAPIC_ID:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->id;
		break;
	case LAPIC_VERS:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->ver;
		break;
	case LAPIC_TPRI:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    (lapic->tpr & LAPIC_TPRI_MASK);
		else
			lapic->tpr = (uint32_t)*data & LAPIC_TPRI_MASK;
		break;
	case LAPIC_APRI:
		if (dir == MMIO_DIR_READ)
			*data &= 0xffffffff00000000ULL;
		break;
	case LAPIC_PPRI:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic_ppr(lapic);
		break;
	case LAPIC_EOI:
		if (dir == MMIO_DIR_WRITE) {
			eoi_vector = lapic_highest_in_map(lapic->isr);
			if (eoi_vector != 0xffff) {
				lapic_clear_map(lapic->isr, eoi_vector);
				lapic_clear_map(lapic->tmr, eoi_vector);
			}
		}
		break;
	case LAPIC_RRR:
		if (dir == MMIO_DIR_READ)
			*data &= 0xFFFFFFFF00000000ULL;
		break;
	case LAPIC_LDR:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->ldr;
		else
			lapic->ldr = (uint32_t)*data;
		break;
	case LAPIC_DFR:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->dfr;
		else
			lapic->dfr = ((uint32_t)*data & 0xF0000000) |
			    0x0fffffff;
		break;
	case LAPIC_SVR:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->svr;
		else {
			d = (uint32_t)*data;
			lapic->svr = d & (LAPIC_SVR_VECTOR_MASK |
			    LAPIC_SVR_ENABLE | LAPIC_SVR_FOCUS);
			DPRINTF("%s: vcpu %u svr=0x%x", __func__, vcpu_id,
			    lapic->svr);
		}
		break;
	case LAPIC_ESR:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) | lapic->esr;
		else
			lapic->esr &= ~(uint32_t)*data;	/* write-1-to-clear */
		break;
	case LAPIC_ICRLO:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->icrlo;
		else {
			d = (uint32_t)*data;
			lapic->icrlo = d & (LAPIC_LVTT_VEC_MASK |
			    LAPIC_DLMODE_MASK | LAPIC_DSTMODE_LOG |
			    LAPIC_LVL_ASSERT | LAPIC_LVL_TRIG |
			    LAPIC_DEST_MASK);
			icrlo = lapic->icrlo;
			icrhi = lapic->icrhi;
			lapic->x2_icr_dest = icrhi >> LAPIC_ID_SHIFT;
			dispatch_icr = 1;
		}
		break;
	case LAPIC_ICRHI:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->icrhi;
		else
			lapic->icrhi = (uint32_t)*data & 0xff000000;
		break;
	case LAPIC_ICR_TIMER:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->icr_timer;
		else {
			lapic->icr_timer = (uint32_t)*data;
			lapic->timer_periodic =
			    (lapic->lvt[LVT_TIMER] & LAPIC_LVTT_TM) ==
			    LAPIC_LVTT_TM_PERIODIC;
			lapic_timer_reload(lapic);
			DPRINTF("%s: vcpu %u lapic timer icr=%u div=%u",
			    __func__, vcpu_id, lapic->icr_timer,
			    lapic_divisor(lapic->dcr_timer));
		}
		break;
	case LAPIC_CCR_TIMER:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic_timer_ccr(lapic);
		break;
	case LAPIC_DCR_TIMER:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->dcr_timer;
		else
			lapic->dcr_timer = (uint32_t)*data & 0x0F;
		break;
	case LAPIC_LVTT:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic->lvt[LVT_TIMER];
		else {
			d = (uint32_t)*data;
			/* TSC deadline mode unsupported: report as masked */
			if ((d & LAPIC_LVTT_TM) == LAPIC_LVTT_TM_TSCDL) {
				log_warnx("%s: vcpu %u tsc-deadline timer "
				    "requested but unsupported", __func__,
				    vcpu_id);
			}
			lapic->lvt[LVT_TIMER] = d;
			lapic->timer_periodic =
			    (d & LAPIC_LVTT_TM) == LAPIC_LVTT_TM_PERIODIC;
		}
		break;
	case LAPIC_PCINT:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic->lvt[LVT_PCINT];
		else
			lapic->lvt[LVT_PCINT] = (uint32_t)*data |
			    LAPIC_LVT_MASKED;	/* always masked */
		break;
	case LAPIC_LVT_THERM:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->lvt[LVT_THERMAL];
		else
			lapic->lvt[LVT_THERMAL] = (uint32_t)*data;
		break;
	case LAPIC_LVT_CMCI:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xffffffff00000000ULL) |
			    lapic->lvt[LVT_CMCI];
		else
			lapic->lvt[LVT_CMCI] = (uint32_t)*data;
		break;
	case LAPIC_LVINT0:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic->lvt[LVT_LINT0];
		else
			lapic->lvt[LVT_LINT0] = (uint32_t)*data;
		break;
	case LAPIC_LVINT1:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic->lvt[LVT_LINT1];
		else
			lapic->lvt[LVT_LINT1] = (uint32_t)*data;
		break;
	case LAPIC_LVERR:
		if (dir == MMIO_DIR_READ)
			*data = (*data & 0xFFFFFFFF00000000ULL) |
			    lapic->lvt[LVT_ERROR];
		else
			lapic->lvt[LVT_ERROR] = (uint32_t)*data;
		break;
	default:
		if (dir == MMIO_DIR_READ) {
			log_debug("%s: unhandled read of LAPIC register 0x%x",
			    __func__, reg);
			*data = 0xFFFFFFFFFFFFFFFF;
		} else {
			log_debug("%s: discarding write to reg 0x%x",
			    __func__, reg);
		}
	}

out:
	pthread_mutex_unlock(&lapic->mtx);
	/* Never acquire the IOAPIC lock while holding a LAPIC lock. */
	if (eoi_vector != 0xffff)
		i82093aa_eoi(eoi_vector);
	if (dispatch_icr)
		lapic_icr(vcpu_id, icrhi, icrlo);

	return 0;
}

/* Resolve xAPIC ICR destinations without holding the source LAPIC lock. */
static uint64_t
lapic_icr_targets(uint32_t source, uint32_t hi, uint32_t lo)
{
	uint64_t targets = 0;
	uint32_t shorthand, dest;
	int i;

	shorthand = lo & LAPIC_DEST_MASK;
	dest = (hi >> LAPIC_ID_SHIFT) & 0xff;

	switch (shorthand) {
	case 0:
		if (lo & LAPIC_DSTMODE_LOG) {
			log_debug("%s: logical destination IPI from vcpu %u "
			    "ignored", __func__, source);
			break;
		}
		if (dest == 0xff) {
			for (i = 0; i < lapic_ncpus; i++)
				targets |= 1ULL << i;
		} else if (dest < (uint32_t)lapic_ncpus)
			targets = 1ULL << dest;
		break;
	case LAPIC_DEST_SELF:
		targets = 1ULL << source;
		break;
	case LAPIC_DEST_ALLINCL:
		for (i = 0; i < lapic_ncpus; i++)
			targets |= 1ULL << i;
		break;
	case LAPIC_DEST_ALLEXCL:
		for (i = 0; i < lapic_ncpus; i++) {
			if ((uint32_t)i != source)
				targets |= 1ULL << i;
		}
		break;
	}

	return (targets);
}

/*
 * Dispatch an ICR after dropping the source LAPIC mutex. INIT resets a
 * target LAPIC and fixed IPIs acquire the target LAPIC and vCPU run locks.
 */
static void
lapic_icr_dispatch(uint32_t source, uint32_t lo, uint64_t targets)
{
	uint32_t mode;
	uint8_t vector;
	int i;

	mode = lo & LAPIC_DLMODE_MASK;
	vector = lo & LAPIC_LVTT_VEC_MASK;

	DPRINTF("%s: vcpu %u mode=0x%x vector=0x%x targets=0x%llx",
	    __func__, source, mode, vector, (unsigned long long)targets);
	switch (mode) {
	case LAPIC_DLMODE_FIXED:
		for (i = 0; i < lapic_ncpus; i++) {
			if (targets & (1ULL << i))
				vcpu_assert_vector(current_vm->vm_fd, i, vector);
		}
		break;
	case LAPIC_DLMODE_INIT:
		/* A level-triggered deassert completes the INIT handshake. */
		if ((lo & LAPIC_LVL_TRIG) && !(lo & LAPIC_LVL_ASSERT))
			break;
		for (i = 0; i < lapic_ncpus; i++) {
			if (targets & (1ULL << i))
				vcpu_assert_init(i);
		}
		break;
	case LAPIC_DLMODE_STARTUP:
		for (i = 0; i < lapic_ncpus; i++) {
			if (targets & (1ULL << i))
				vcpu_start_sipi(i, vector);
		}
		break;
	default:
		log_debug("%s: unsupported delivery mode 0x%x from vcpu %u",
		    __func__, mode, source);
		break;
	}
}

static void
lapic_icr(uint32_t source, uint32_t hi, uint32_t lo)
{
	lapic_icr_dispatch(source, lo, lapic_icr_targets(source, hi, lo));
}

static uint64_t
lapic_x2apic_targets(uint32_t source, uint64_t icr)
{
	uint64_t targets = 0;
	uint32_t dest, lo, cluster, logical;
	int i;

	lo = (uint32_t)icr;
	if ((lo & LAPIC_DEST_MASK) != 0)
		return (lapic_icr_targets(source, 0, lo));

	dest = icr >> 32;
	if (lo & LAPIC_DSTMODE_LOG) {
		cluster = dest >> 16;
		logical = dest & 0xffff;
		for (i = 0; i < lapic_ncpus; i++) {
			if (((uint32_t)i >> 4) == cluster &&
			    (logical & (1U << (i & 0xf))))
				targets |= 1ULL << i;
		}
	} else if (dest == 0xffffffff) {
		for (i = 0; i < lapic_ncpus; i++)
			targets |= 1ULL << i;
	} else if (dest < (uint32_t)lapic_ncpus)
		targets = 1ULL << dest;

	return (targets);
}

int
lapic_x2apic(uint32_t vcpu_id, int dir, uint32_t msr, uint64_t *data)
{
	struct lapic *lapic;
	uint32_t lo;
	uint64_t icr, targets;

	if (vcpu_id >= LAPIC_MAX_VCPUS ||
	    vcpu_id >= (uint32_t)lapic_ncpus ||
	    msr < MSR_X2APIC_BASE || msr > MSR_X2APIC_END)
		return (EINVAL);

	lapic = &lapics[vcpu_id];
	switch (msr) {
	case MSR_X2APIC_ID:	/* x2APIC ID is not shifted. */
		if (dir != MMIO_DIR_READ)
			return (EINVAL);
		*data = vcpu_id;
		return (0);
	case MSR_X2APIC_LDR:	/* Cluster ID and logical ID. */
		if (dir != MMIO_DIR_READ)
			return (EINVAL);
		*data = ((uint64_t)(vcpu_id >> 4) << 16) |
		    (1U << (vcpu_id & 0xf));
		return (0);
	case MSR_X2APIC_ICR:	/* Destination and command share one 64-bit ICR. */
		if (dir == MMIO_DIR_READ) {
			pthread_mutex_lock(&lapic->mtx);
			*data = ((uint64_t)lapic->x2_icr_dest << 32) |
			    lapic->icrlo;
			pthread_mutex_unlock(&lapic->mtx);
			return (0);
		}
		icr = *data;
		lo = (uint32_t)icr & (LAPIC_LVTT_VEC_MASK |
		    LAPIC_DLMODE_MASK | LAPIC_DSTMODE_LOG | LAPIC_LVL_ASSERT |
		    LAPIC_LVL_TRIG | LAPIC_DEST_MASK);
		pthread_mutex_lock(&lapic->mtx);
		lapic->icrlo = lo;
		lapic->x2_icr_dest = icr >> 32;
		lapic->icrhi = (lapic->x2_icr_dest & 0xff) <<
		    LAPIC_ID_SHIFT;
		pthread_mutex_unlock(&lapic->mtx);
		targets = lapic_x2apic_targets(vcpu_id, icr);
		lapic_icr_dispatch(vcpu_id, lo, targets);
		return (0);
	case MSR_X2APIC_SELF_IPI:	/* Fixed, edge-triggered delivery. */
		if (dir != MMIO_DIR_WRITE)
			return (EINVAL);
		lo = (uint8_t)*data | LAPIC_DEST_SELF;
		lapic_icr(vcpu_id, 0, lo);
		return (0);
	default:
		return (lapic_mmio(vcpu_id, dir, LAPIC_BASE +
		    ((msr - MSR_X2APIC_BASE) << 4), sizeof(uint32_t), data));
	}
}

/*
 * Return highest priority pending vector within a class-scan order.
 * Within a priority class, higher vectors are delivered first, so scan
 * bits from MSB downward. Returns 0xFFFF if nothing pending in the map.
 */
static int
lapic_highest_in_map(const uint32_t *map)
{
	int base, bit;

	for (base = 7; base >= 0; base--) {
		for (bit = 31; bit >= 0; bit--) {
			if (map[base] & (1U << bit))
				return base * 32 + bit;
		}
	}

	return 0xFFFF;
}

static void
lapic_set_map(uint32_t *map, int vector)
{
	map[vector / 32] |= 1U << (vector % 32);
}

static void
lapic_clear_map(uint32_t *map, int vector)
{
	map[vector / 32] &= ~(1U << (vector % 32));
}

/* Intel PPR is TPR when its class wins, otherwise the ISR class. */
static uint32_t
lapic_ppr(struct lapic *lapic)
{
	int isr;

	isr = lapic_highest_in_map(lapic->isr);
	if (isr != 0xffff &&
	    (isr & LAPIC_TPRI_INT_MASK) >
	    (lapic->tpr & LAPIC_TPRI_INT_MASK))
		return isr & LAPIC_TPRI_INT_MASK;

	return lapic->tpr & LAPIC_TPRI_MASK;
}

static int
lapic_highest_pending(struct lapic *lapic)
{
	int vector;

	if (!(lapic->svr & LAPIC_SVR_ENABLE))
		return 0xffff;

	vector = lapic_highest_in_map(lapic->irr);
	if (vector == 0xffff ||
	    (vector & LAPIC_TPRI_INT_MASK) <=
	    (lapic_ppr(lapic) & LAPIC_TPRI_INT_MASK))
		return 0xffff;

	return vector;
}

/*
 * Check the vcpu's LAPIC timer for expiry. On expiry, deliver the timer
 * LVT vector (if unmasked) into the IRR and advance periodic timers without
 * accumulating host scheduling drift.
 *
 * Returns 1 if a newly queued interrupt is immediately deliverable.
 */
int
lapic_timer_check(uint32_t vcpu_id)
{
	struct lapic *lapic;
	uint64_t elapsed_ticks, elapsed_periods, interval_ns;
	uint8_t vector;
	int pending = 0;

	if (vcpu_id >= LAPIC_MAX_VCPUS || vcpu_id >= (uint32_t)lapic_ncpus)
		return 0;

	lapic = &lapics[vcpu_id];
	pthread_mutex_lock(&lapic->mtx);
	if (!lapic->timer_running || lapic->icr_timer == 0)
		goto out;

	elapsed_ticks = lapic_timer_ticks(lapic);
	if (elapsed_ticks < lapic->icr_timer)
		goto out;

	vector = lapic->lvt[LVT_TIMER] & LAPIC_LVTT_VEC_MASK;
	if (lapic->timer_periodic) {
		elapsed_periods = elapsed_ticks / lapic->icr_timer;
		interval_ns = (uint64_t)lapic->icr_timer * 10ULL *
		    lapic_divisor(lapic->dcr_timer);
		timespecadd(&lapic->timer_start,
		    (&(struct timespec) {
		    .tv_sec = elapsed_periods * interval_ns / 1000000000ULL,
		    .tv_nsec = elapsed_periods * interval_ns % 1000000000ULL
		    }), &lapic->timer_start);
	} else
		lapic->timer_running = 0;

	if (!(lapic->svr & LAPIC_SVR_ENABLE) ||
	    (lapic->lvt[LVT_TIMER] & LAPIC_LVT_MASKED))
		goto out;

	if (vector < 32) {
		log_debug("%s: vcpu %u timer vector %u too low, dropped",
		    __func__, vcpu_id, vector);
		goto out;
	}

	DPRINTF("%s: vcpu %u lapic timer expired, vector %u", __func__,
	    vcpu_id, vector);
	lapic_set_map(lapic->irr, vector);
	lapic_clear_map(lapic->tmr, vector);
	pending = lapic_highest_pending(lapic) != 0xffff;

out:
	pthread_mutex_unlock(&lapic->mtx);
	return pending;
}

int
lapic_vector_irq(uint32_t dest_vcpu, int destmode, uint8_t vector,
    int level)
{
	struct lapic *lapic;
	int error;

	if (dest_vcpu >= LAPIC_MAX_VCPUS ||
	    dest_vcpu >= (uint32_t)lapic_ncpus) {
		log_warnx("%s: invalid destination vcpu %u", __func__,
		    dest_vcpu);
		return (0);
	}
	if (vector < 32) {
		log_debug("%s: skipping low vector %d", __func__, vector);
		return (0);
	}

	lapic = &lapics[dest_vcpu];
	pthread_mutex_lock(&lapic->mtx);
	if (!(lapic->svr & LAPIC_SVR_ENABLE)) {
		log_debug("%s: vector irq %d but vcpu %u lapic disabled",
		    __func__, vector, dest_vcpu);
		pthread_mutex_unlock(&lapic->mtx);
		return (0);
	}

	DPRINTF("%s: delivering vec=%d level=%d to vcpu %u (%s dest)",
	    __func__, vector, level, dest_vcpu,
	    destmode ? "logical" : "physical");
	lapic_set_map(lapic->irr, vector);
	if (level)
		lapic_set_map(lapic->tmr, vector);
	else
		lapic_clear_map(lapic->tmr, vector);
	pthread_mutex_unlock(&lapic->mtx);

	/*
	 * Wake the LAPIC destination, rather than relying on the device which
	 * asserted an IOAPIC input to guess where the IOAPIC routed it.  The
	 * device-side vcpu id is normally zero, while the IOAPIC may target any
	 * vCPU.  Without this kick, a halted target can retain a deliverable
	 * vector in its IRR until an unrelated exit or timeout occurs.
	 *
	 * Always kick the target after queuing a vector.  The cached TPR in the
	 * software LAPIC can lag a MOV CR8 completed in vmm, so intr_pending()
	 * cannot reliably decide here whether the new vector is deliverable.
	 * The resulting exit synchronizes the state before vmd checks the IRR.
	 */
	error = vcpu_intr(current_vm->vm_fd, dest_vcpu, 1);
	if (error != 0)
		fatalx("%s: can't assert vector %u on vcpu %u: %s",
		    __func__, vector, dest_vcpu, strerror(error));
	vcpu_unhalt(dest_vcpu);
	vcpu_signal_run(dest_vcpu);

	return (1);
}

int
lapic_is_pending(int vcpu_id)
{
	struct lapic *lapic;
	int pending;

	if (vcpu_id < 0 || vcpu_id >= lapic_ncpus)
		return 0;

	lapic = &lapics[vcpu_id];
	pthread_mutex_lock(&lapic->mtx);
	pending = lapic_highest_pending(lapic) != 0xffff;
	pthread_mutex_unlock(&lapic->mtx);

	return pending;
}

int
lapic_ack(int vcpu_id)
{
	struct lapic *lapic;
	int vector;

	if (vcpu_id < 0 || vcpu_id >= lapic_ncpus)
		return 0xFFFF;

	lapic = &lapics[vcpu_id];
	pthread_mutex_lock(&lapic->mtx);
	vector = lapic_highest_pending(lapic);
	if (vector != 0xffff) {
		lapic_set_map(lapic->isr, vector);
		lapic_clear_map(lapic->irr, vector);
	}
	pthread_mutex_unlock(&lapic->mtx);

	return vector;
}
