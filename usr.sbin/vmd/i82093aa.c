/*	$OpenBSD: i82093aa.c,v 1.4 2026/09/23 15:35:43 mlarkin Exp $ */

/*
 * Copyright (c) 2024 Mike Larkin <mlarkin@openbsd.org>
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

#include <pthread.h>
#include <machine/i82093reg.h>

#include "i82093aa.h"
#include "lapic.h"
#include "mmio.h"
#include "vmd.h"

#ifdef DPRINTF
#undef DPRINTF
#endif
#if I82093AA_DEBUG
#define DPRINTF		log_debug
#else
#define DPRINTF(x...)	do {} while(0)
#endif	/* I82093AA_DEBUG */

struct i82093aa {
	uint32_t	reg;
	uint32_t	id;
	uint32_t	redtbl[I82093AA_REDTBL_DWORDS];
	int		pin_level[I82093AA_PIN_COUNT];
	int		last_level[I82093AA_PIN_COUNT];
	uint32_t	ncpus;
	uint32_t	arb_next;
	pthread_mutex_t mtx;
};

static struct i82093aa ioapic = {
	.mtx = PTHREAD_MUTEX_INITIALIZER
};

static void	i82093aa_decode_redent(uint32_t);
static void	i82093aa_evaluate_pin(uint8_t);
static int	i82093aa_deliver(uint8_t, int, int, uint8_t, int);
static void	i82093aa_regsel(int, uint64_t *);
static void	i82093aa_winop(int, uint64_t *);

static void
i82093aa_decode_redent(uint32_t reg)
{
#ifdef I82093AA_DEBUG
	uint8_t dest, delmod, pin, vector;
	uint32_t lo, hi;
#endif /* I82093AA_DEBUG */

	if (reg < I82093AA_REDTBL0_LO || reg > I82093AA_REDTBL23_HI) {
		log_warnx("%s: impossible reg 0x%x", __func__, reg);
		return;
	}

#ifdef I82093AA_DEBUG
	pin = (reg - I82093AA_REDTBL0_LO) / 2;
	lo = ioapic.redtbl[pin * 2];
	hi = ioapic.redtbl[pin * 2 + 1];

	dest = (hi & IOAPIC_REDHI_DEST_MASK) >> IOAPIC_REDHI_DEST_SHIFT;
	delmod = (lo & IOAPIC_REDLO_DEL_MASK) >> IOAPIC_REDLO_DEL_SHIFT;
	vector = lo & IOAPIC_REDLO_VECTOR_MASK;
#endif /* I82093AA_DEBUG */

	DPRINTF("%s: pin %u %s write: hi=0x%08x lo=0x%08x "
	    "dest=%u/%s delivery=%u vector=%u %s/%s %s rirr=%u",
	    __func__, pin, reg & 1 ? "high" : "low", hi, lo, dest,
	    lo & IOAPIC_REDLO_DSTMOD ? "logical" : "physical", delmod,
	    vector, lo & IOAPIC_REDLO_LEVEL ? "level" : "edge",
	    lo & IOAPIC_REDLO_ACTLO ? "active-low" : "active-high",
	    lo & IOAPIC_REDLO_MASK ? "masked" : "unmasked",
	    !!(lo & IOAPIC_REDLO_RIRR));
}

static void
i82093aa_winop(int dir, uint64_t *data)
{
	uint32_t d, old, writable;
	uint8_t idx, pin;

	pthread_mutex_lock(&ioapic.mtx);

	if (dir == MMIO_DIR_READ) {
		d = 0;

		switch (ioapic.reg) {
		case IOAPIC_ID:
			d = (ioapic.id << IOAPIC_ID_SHIFT);
			break;
		case IOAPIC_VER:
			d = I82093AA_VERSION |
			    (I82093AA_MAX_PIN << IOAPIC_MAX_SHIFT);
			break;
		case IOAPIC_ARB:
			d = (ioapic.id << IOAPIC_ID_SHIFT);
			break;
		case I82093AA_REDTBL0_LO ... I82093AA_REDTBL23_HI:
			idx = ioapic.reg - I82093AA_REDTBL0_LO;
			d = ioapic.redtbl[idx];
			break;
		default:
			log_warnx("%s: unknown register id 0x%x", __func__,
			    ioapic.reg);
		}

		*data &= 0xFFFFFFFF00000000ULL;
		*data |= d;
	} else if (dir == MMIO_DIR_WRITE) {
		d = (uint32_t)*data;
		switch (ioapic.reg) {
		case IOAPIC_ID:
			ioapic.id = (d & IOAPIC_ID_MASK) >> IOAPIC_ID_SHIFT;
			break;
		case IOAPIC_VER:
		case IOAPIC_ARB:
			/* Read-only. */
			break;
		case I82093AA_REDTBL0_LO ... I82093AA_REDTBL23_HI:
			pin = (ioapic.reg - I82093AA_REDTBL0_LO) / 2;
			idx = ioapic.reg - I82093AA_REDTBL0_LO;
			old = ioapic.redtbl[idx];
			if ((idx & 1) == 0) {
				writable = IOAPIC_REDLO_MASK | IOAPIC_REDLO_LEVEL |
				    IOAPIC_REDLO_ACTLO | IOAPIC_REDLO_DSTMOD |
				    IOAPIC_REDLO_DEL_MASK |
				    IOAPIC_REDLO_VECTOR_MASK;
				d = (d & writable) |
				    (old & (IOAPIC_REDLO_RIRR |
				    IOAPIC_REDLO_DELSTS));
			} else
				d &= IOAPIC_REDHI_DEST_MASK;
			ioapic.redtbl[idx] = d;

			i82093aa_decode_redent(ioapic.reg);
			i82093aa_evaluate_pin(pin);
			break;
		default:
			log_warnx("%s: unknown register id 0x%x", __func__,
			    ioapic.reg);
		}
	} else {
		log_warnx("%s: impossible direction %d", __func__, dir);
	}

	pthread_mutex_unlock(&ioapic.mtx);
}

static void
i82093aa_regsel(int dir, uint64_t *data)
{
	uint64_t d;

	pthread_mutex_lock(&ioapic.mtx);

	if (dir == MMIO_DIR_READ) {
		d = *data & 0xFFFFFFFF00000000ULL;
		d |= ioapic.reg;
		*data = d;
	} else if (dir == MMIO_DIR_WRITE) {
		ioapic.reg = (uint32_t)*data & 0xff;
	} else {
		log_warnx("%s: impossible direction %d", __func__, dir);
	}

	pthread_mutex_unlock(&ioapic.mtx);
}

int
i82093aa_mmio(uint32_t vcpu_id, int dir, paddr_t addr, uint8_t size,
    uint64_t *data)
{
	(void)size;

	DPRINTF("%s: vcpu=%u dir=%d addr=0x%lx data=0x%llx", __func__,
	    vcpu_id, dir, addr,
	    *data);

	if (addr == (IOAPIC_BASE_DEFAULT + IOAPIC_REG)) {
		i82093aa_regsel(dir, data);
	} else if (addr == (IOAPIC_BASE_DEFAULT + IOAPIC_DATA)) {
		i82093aa_winop(dir, data);
	} else {
		log_warnx("%s: invalid i82093aa register @ 0x%lx", __func__,
		    addr);
	}

	return 0;
}

void
i82093aa_init(int numcpus)
{
	uint8_t pin;

	ioapic.reg = 0;
	ioapic.id = (numcpus + 1) & 0xf;
	ioapic.ncpus = numcpus;
	ioapic.arb_next = 0;
	for (pin = 0; pin < I82093AA_PIN_COUNT; pin++) {
		ioapic.redtbl[pin * 2] = IOAPIC_REDLO_MASK;
		ioapic.redtbl[pin * 2 + 1] = 0;
		ioapic.pin_level[pin] = 0;
		ioapic.last_level[pin] = 0;
	}
	mmio_dev_add(IOAPIC_BASE_DEFAULT, IOAPIC_BASE_DEFAULT + 0xFFFF,
	    i82093aa_mmio);
}

void
i82093aa_assert_pin(uint8_t pin)
{
	if (pin >= I82093AA_PIN_COUNT) {
		log_warnx("%s: invalid pin %u", __func__, pin);
		return;
	}
	pthread_mutex_lock(&ioapic.mtx);
	ioapic.pin_level[pin] = 1;
	i82093aa_evaluate_pin(pin);
	pthread_mutex_unlock(&ioapic.mtx);
}

void
i82093aa_deassert_pin(uint8_t pin)
{
	if (pin >= I82093AA_PIN_COUNT) {
		log_warnx("%s: invalid pin %u", __func__, pin);
		return;
	}
	pthread_mutex_lock(&ioapic.mtx);
	ioapic.pin_level[pin] = 0;
	i82093aa_evaluate_pin(pin);
	pthread_mutex_unlock(&ioapic.mtx);
}

static void
i82093aa_evaluate_pin(uint8_t pin)
{
	uint64_t ent;
	uint8_t delivery_mode, vector, dest;
	int dest_mode, masked, level, active, rising;

	if (pin >= I82093AA_PIN_COUNT) {
		log_warnx("%s: invalid pin %u", __func__, pin);
		return;
	}

	ent = ioapic.redtbl[pin * 2] |
	    ((uint64_t)ioapic.redtbl[pin * 2 + 1] << 32);
	masked = (ent & IOAPIC_REDLO_MASK) != 0;
	level = (ent & IOAPIC_REDLO_LEVEL) != 0;
	/* Device backends report logical assertion, not electrical level. */
	active = ioapic.pin_level[pin] != 0;
	rising = active && !ioapic.last_level[pin];

	dest_mode = (ent & IOAPIC_REDLO_DSTMOD) != 0;
	delivery_mode = (ent & IOAPIC_REDLO_DEL_MASK) >>
	    IOAPIC_REDLO_DEL_SHIFT;
	vector = ent & IOAPIC_REDLO_VECTOR_MASK;
	dest = (ent >> I82093AA_REDIR_SHIFT) & 0xff;

	ioapic.last_level[pin] = active;

	if (masked)
		return;

	if (level) {
		if (active && !(ent & IOAPIC_REDLO_RIRR) &&
		    i82093aa_deliver(dest, dest_mode, delivery_mode, vector, 1))
			ioapic.redtbl[pin * 2] |= IOAPIC_REDLO_RIRR;
	} else {
		if (rising)
			i82093aa_deliver(dest, dest_mode, delivery_mode,
			    vector, 0);
	}
}

static int
i82093aa_deliver(uint8_t dest, int dest_mode, int delivery_mode,
    uint8_t vector, int level)
{
	uint64_t targets;
	uint32_t i;
	int delivered = 0, target;

	if (delivery_mode != IOAPIC_REDLO_DEL_FIXED &&
	    delivery_mode != IOAPIC_REDLO_DEL_LOPRI) {
		log_debug("%s: delivery mode %d unsupported", __func__,
		    delivery_mode);
		return 0;
	}
	if (vector < 32)
		return 0;

	targets = lapic_targets(dest, dest_mode);
	if (delivery_mode == IOAPIC_REDLO_DEL_LOPRI) {
		target = lapic_lowest_priority(targets, ioapic.arb_next);
		if (target == -1)
			return 0;
		targets = 1ULL << target;
		ioapic.arb_next = target + 1;
		if (ioapic.arb_next >= ioapic.ncpus)
			ioapic.arb_next = 0;
	}

	for (i = 0; i < ioapic.ncpus; i++) {
		if ((targets & (1ULL << i)) == 0 || !lapic_enabled(i))
			continue;
		lapic_vector_irq(i, dest_mode, vector, level);
		delivered = 1;
	}

	return delivered;
}

void
i82093aa_eoi(int vector)
{
	uint8_t pin;
	uint64_t ent;

	DPRINTF("%s: EOI for vector %d", __func__, vector);
	pthread_mutex_lock(&ioapic.mtx);

	for (pin = 0; pin < I82093AA_PIN_COUNT; pin++) {
		ent = ioapic.redtbl[pin * 2] |
		    ((uint64_t)(ioapic.redtbl[pin * 2 + 1]) << 32);
		if ((ent & 0xFF) != vector)
			continue;

		/*
		 * Only level-triggered entries with RIRR set participate
		 * in EOI; clear RIRR and re-evaluate in case the line is
		 * still asserted.
		 */
		if ((ent & IOAPIC_REDLO_LEVEL) &&
		    (ioapic.redtbl[pin * 2] & IOAPIC_REDLO_RIRR)) {
			ioapic.redtbl[pin * 2] &= ~IOAPIC_REDLO_RIRR;
			i82093aa_evaluate_pin(pin);
		}
	}

	pthread_mutex_unlock(&ioapic.mtx);
}
