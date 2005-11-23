/*	$OpenBSD: ts102.c,v 1.16 2005/11/23 11:39:36 mickey Exp $	*/
/*
 * Copyright (c) 2003, 2004, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the PCMCIA controller found in Tadpole SPARCbook 3 series
 * notebooks.
 *
 * Based on the information provided in the SPARCbook 3 Technical Reference
 * Manual (s3gxtrmb.pdf), chapter 7.  A few ramblings against this document
 * and/or the chip itself are scattered across this file.
 *
 * Implementation notes:
 *
 * - The TS102 exports its PCMCIA windows as SBus memory ranges: 64MB for
 *   the common memory window, and 16MB for the attribute and I/O windows.
 *
 *   Mapping the whole windows would consume 192MB of address space, which
 *   is much more that what the iospace can offer.
 *
 *   A best-effort solution would be to map the windows on demand. However,
 *   due to the wap mapdev() works, the va used for the mappings would be
 *   lost after unmapping (although using an extent to register iospace memory
 *   usage would fix this). So, instead, we will do a fixed mapping of a subset
 *   of each window upon attach - this is similar to what the stp4020 driver
 *   does.
 *
 * - IPL for the cards interrupt handlers are not respected. See the stp4020
 *   driver source for comments about this.
 * 
 * Endianness farce:
 *
 * - The documentation pretends that the endianness settings only affect the
 *   common memory window. Gee, thanks a lot. What about other windows, then?
 *   As a result, this driver runs with endianness conversions turned off.
 *
 * - One of the little-endian SBus and big-endian PCMCIA flags has the reverse
 *   meaning, actually. To achieve a ``no endianness conversion'' status,
 *   one has to be set and the other unset. It does not matter which one,
 *   though.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/tctrlvar.h>
#include <sparc/dev/ts102reg.h>

#define	TS102_NUM_SLOTS		2

/*
 * Memory ranges
 */
#define	TS102_RANGE_COMMON	0
#define	TS102_RANGE_ATTR	1
#define	TS102_RANGE_IO		2

#define	TS102_RANGE_CNT		3
#define	TS102_NUM_RANGES	(TS102_RANGE_CNT * TS102_NUM_SLOTS)

#define	TS102_ARBITRARY_MAP_SIZE	(1 * 1024 * 1024)

struct	tslot_softc;

/*
 * Per-slot data
 */
struct	tslot_data {
	struct tslot_softc	*td_parent;
	struct device		*td_pcmcia;

	volatile u_int8_t	*td_regs;
	struct rom_reg		td_rr;
	vaddr_t			td_space[TS102_RANGE_CNT];

	/* Interrupt handler */
	int			(*td_intr)(void *);
	void			*td_intrarg;

	/* Socket status */
	int			td_slot;
	int			td_status;
#define	TS_CARD			0x0001
};

struct	tslot_softc {
	struct device	sc_dev;
	struct sbusdev	sc_sd;

	struct intrhand	sc_ih;
	
	pcmcia_chipset_tag_t sc_pct;

	struct proc	*sc_thread;			/* event thread */
	unsigned int	sc_events;	/* sockets with pending events */

	struct tslot_data sc_slot[TS102_NUM_SLOTS];
};

void	tslot_attach(struct device *, struct device *, void *);
void	tslot_create_event_thread(void *);
void	tslot_event_thread(void *);
int	tslot_intr(void *);
void	tslot_intr_disestablish(pcmcia_chipset_handle_t, void *);
void	*tslot_intr_establish(pcmcia_chipset_handle_t, struct pcmcia_function *,
	    int, int (*)(void *), void *, char *);
const char *tslot_intr_string(pcmcia_chipset_handle_t, void *);
int	tslot_io_alloc(pcmcia_chipset_handle_t, bus_addr_t, bus_size_t,
	    bus_size_t, struct pcmcia_io_handle *);
void	tslot_io_free(pcmcia_chipset_handle_t, struct pcmcia_io_handle *);
int	tslot_io_map(pcmcia_chipset_handle_t, int, bus_addr_t, bus_size_t,
	    struct pcmcia_io_handle *, int *);
void	tslot_io_unmap(pcmcia_chipset_handle_t, int);
int	tslot_match(struct device *, void *, void *);
int	tslot_mem_alloc(pcmcia_chipset_handle_t, bus_size_t,
	    struct pcmcia_mem_handle *);
void	tslot_mem_free(pcmcia_chipset_handle_t, struct pcmcia_mem_handle *);
int	tslot_mem_map(pcmcia_chipset_handle_t, int, bus_addr_t, bus_size_t,
	    struct pcmcia_mem_handle *, bus_size_t *, int *);
void	tslot_mem_unmap(pcmcia_chipset_handle_t, int);
int	tslot_print(void *, const char *);
void	tslot_queue_event(struct tslot_softc *, int);
void	tslot_reset(struct tslot_data *, u_int32_t);
void	tslot_slot_disable(pcmcia_chipset_handle_t);
void	tslot_slot_enable(pcmcia_chipset_handle_t);
void	tslot_slot_intr(struct tslot_data *, int);

struct	cfattach tslot_ca = {
	sizeof(struct tslot_softc), tslot_match, tslot_attach
};

struct	cfdriver tslot_cd = {
	NULL, "tslot", DV_DULL
};

/*
 * PCMCIA chipset methods
 */
struct	pcmcia_chip_functions tslot_functions = {
	tslot_mem_alloc,
	tslot_mem_free,
	tslot_mem_map,
	tslot_mem_unmap,

	tslot_io_alloc,
	tslot_io_free,
	tslot_io_map,
	tslot_io_unmap,

	tslot_intr_establish,
	tslot_intr_disestablish,
	tslot_intr_string,

	tslot_slot_enable,
	tslot_slot_disable
};

#define	TSLOT_READ(slot, offset) \
	*(volatile u_int16_t *)((slot)->td_regs + (offset))
#define	TSLOT_WRITE(slot, offset, value) \
	*(volatile u_int16_t *)((slot)->td_regs + (offset)) = (value)

/*
 * Attachment and initialization
 */

int
tslot_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;

	return (strcmp("ts102", ca->ca_ra.ra_name) == 0);
}

void
tslot_attach(struct device *parent, struct device *self, void *args)
{
	struct confargs *ca = args;
	struct tslot_softc *sc = (struct tslot_softc *)self;
	struct romaux *ra;
	struct rom_range ranges[TS102_NUM_RANGES], *range;
	struct tslot_data *td;
	volatile u_int8_t *regs;
	int node, nranges, slot, rnum;

	ra = &ca->ca_ra;
	node = ra->ra_node;
	regs = mapiodev(&ra->ra_reg[0], 0, ra->ra_len);

	/*
	 * Find memory ranges
	 */
	nranges = getproplen(node, "ranges") / sizeof(struct rom_range);
	if (nranges < TS102_NUM_RANGES) {
		printf(": expected %d memory ranges, got %d\n",
		    TS102_NUM_RANGES, nranges);
		return;
	}
	getprop(node, "ranges", ranges, sizeof ranges);

	/*
	 * Ranges being relative to this sbus slot, turn them into absolute
	 * addresses.
	 */
	for (rnum = 0; rnum < TS102_NUM_RANGES; rnum++) {
		ranges[rnum].poffset -= TS102_OFFSET_REGISTERS;
	}

	sc->sc_ih.ih_fun = tslot_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ra->ra_intr[0].int_pri, &sc->sc_ih, -1, self->dv_xname);
	printf(" pri %d", ra->ra_intr[0].int_pri);

	sbus_establish(&sc->sc_sd, self);

	printf(": %d slots\n", TS102_NUM_SLOTS);

	/*
	 * Setup asynchronous event handler
	 */
	sc->sc_events = 0;
	kthread_create_deferred(tslot_create_event_thread, sc);

	sc->sc_pct = (pcmcia_chipset_tag_t)&tslot_functions;

	/*
	 * Setup slots
	 */
	for (slot = 0; slot < TS102_NUM_SLOTS; slot++) {
		td = &sc->sc_slot[slot];
		for (rnum = 0; rnum < TS102_RANGE_CNT; rnum++) {
			range = ranges + (slot * TS102_RANGE_CNT + rnum);
			td->td_rr = ra->ra_reg[0];
			td->td_rr.rr_iospace = range->pspace;
			td->td_rr.rr_paddr = (void *)
			   ((u_int32_t)td->td_rr.rr_paddr + range->poffset);
			td->td_space[rnum] = (vaddr_t)mapiodev(&td->td_rr, 0,
			    TS102_ARBITRARY_MAP_SIZE);
		}
		td->td_parent = sc;
		td->td_regs = regs +
		    slot * (TS102_REG_CARD_B_INT - TS102_REG_CARD_A_INT);
		td->td_slot = slot;
		SET_TAG_LITTLE_ENDIAN(&td->td_rr);
		tslot_reset(td, TS102_ARBITRARY_MAP_SIZE);
	}
}

void
tslot_reset(struct tslot_data *td, u_int32_t iosize)
{
	struct pcmciabus_attach_args paa;
	int ctl, status;

	paa.paa_busname = "pcmcia";
	paa.pct = (pcmcia_chipset_tag_t)td->td_parent->sc_pct;
	paa.pch = (pcmcia_chipset_handle_t)td;
	paa.iobase = 0;
	paa.iosize = iosize;

	td->td_pcmcia = config_found(&td->td_parent->sc_dev, &paa, tslot_print);

	if (td->td_pcmcia == NULL) {
		/*
		 * If no pcmcia attachment, power down the slot.
		 */
		tslot_slot_disable((pcmcia_chipset_handle_t)td);
		return;
	}

	/*
	 * Initialize the slot
	 */

	ctl = TSLOT_READ(td, TS102_REG_CARD_A_CTL);
	/* force low addresses */
	ctl &= ~(TS102_CARD_CTL_AA_MASK | TS102_CARD_CTL_IA_MASK);
	/* Put SBus and PCMCIA in their respective endian mode */
	ctl |= TS102_CARD_CTL_SBLE;	/* this is not what it looks like! */
	ctl &= ~TS102_CARD_CTL_PCMBE;
	/* disable read ahead and address increment */
	ctl &= ~TS102_CARD_CTL_RAHD;
	ctl |= TS102_CARD_CTL_INCDIS;
	/* power on */
	ctl &= ~TS102_CARD_CTL_PWRD;
	TSLOT_WRITE(td, TS102_REG_CARD_A_CTL, ctl);

	/*
	 * Enable interrupt upon insertion/removal
	 */

	TSLOT_WRITE(td, TS102_REG_CARD_A_INT,
	    TS102_CARD_INT_MASK_CARDDETECT_STATUS);

	status = TSLOT_READ(td, TS102_REG_CARD_A_STS);
	if (status & TS102_CARD_STS_PRES) {
		tadpole_set_pcmcia(td->td_slot, 1);
		td->td_status = TS_CARD;
		pcmcia_card_attach(td->td_pcmcia);
	} else {
		tadpole_set_pcmcia(td->td_slot, 0);
		td->td_status = 0;
	}
}

/* XXX there ought to be a common function for this... */
int
tslot_print(void *aux, const char *description)
{
	struct pcmciabus_attach_args *paa = aux;
	struct tslot_data *td = (struct tslot_data *)paa->pch;

	printf(" socket %d", td->td_slot);
	return (UNCONF);
}

/*
 * PCMCIA Helpers
 */

int
tslot_io_alloc(pcmcia_chipset_handle_t pch, bus_addr_t start, bus_size_t size,
    bus_size_t align, struct pcmcia_io_handle *pih)
{
	struct tslot_data *td = (struct tslot_data *)pch;

#ifdef TSLOT_DEBUG
	printf("[io alloc %x-%x]", start, size);
#endif

	pih->iot = &td->td_rr;
	pih->ioh = (bus_space_handle_t)(td->td_space[TS102_RANGE_IO]);
	pih->addr = start;
	pih->size = size;
	pih->flags = 0;

	return (0);
}

void
tslot_io_free(pcmcia_chipset_handle_t pch, struct pcmcia_io_handle *pih)
{
#ifdef TSLOT_DEBUG
	printf("[io free %x-%x]", pih->start, pih->size);
#endif
}

int
tslot_io_map(pcmcia_chipset_handle_t pch, int width, bus_addr_t offset,
    bus_size_t size, struct pcmcia_io_handle *pih, int *windowp)
{
	struct tslot_data *td = (struct tslot_data *)pch;

#ifdef TSLOT_DEBUG
	printf("[io map %x-%x", offset, size);
#endif

	pih->iot = &td->td_rr;
	bus_space_subregion(&td->td_rr, td->td_space[TS102_RANGE_IO],
	    offset, size, &pih->ioh);
	*windowp = TS102_RANGE_IO;

#ifdef TSLOT_DEBUG
	printf("->%p/%x]", pih->ioh, size);
#endif

	return (0);
}

void
tslot_io_unmap(pcmcia_chipset_handle_t pch, int win)
{
#ifdef TSLOT_DEBUG
	printf("[io unmap]");
#endif
}

int
tslot_mem_alloc(pcmcia_chipset_handle_t pch, bus_size_t size,
    struct pcmcia_mem_handle *pmh)
{
	struct tslot_data *td = (struct tslot_data *)pch;

#ifdef TSLOT_DEBUG
	printf("[mem alloc %x]", size);
#endif
	pmh->memt = &td->td_rr;
	pmh->size = round_page(size);
	pmh->addr = 0;
	pmh->mhandle = 0;
	pmh->realsize = 0;	/* nothing so far! */

	return (0);
}

void
tslot_mem_free(pcmcia_chipset_handle_t pch, struct pcmcia_mem_handle *pmh)
{
#ifdef TSLOT_DEBUG
	printf("[mem free %x]", pmh->size);
#endif
}

int
tslot_mem_map(pcmcia_chipset_handle_t pch, int kind, bus_addr_t addr,
    bus_size_t size, struct pcmcia_mem_handle *pmh, bus_size_t *offsetp,
    int *windowp)
{
	struct tslot_data *td = (struct tslot_data *)pch;
	int slot;

	slot = kind & PCMCIA_MEM_ATTR ? TS102_RANGE_ATTR : TS102_RANGE_COMMON;
#ifdef TSLOT_DEBUG
	printf("[mem map %d %x-%x", slot, addr, size);
#endif

	addr += pmh->addr;

	pmh->memt = &td->td_rr;
	bus_space_subregion(&td->td_rr, td->td_space[slot],
	    addr, size, &pmh->memh);
	pmh->realsize = TS102_ARBITRARY_MAP_SIZE - addr;
	*offsetp = 0;
	*windowp = slot;

#ifdef TSLOT_DEBUG
	printf("->%p/%x]", pmh->memh, size);
#endif

	return (0);
}

void
tslot_mem_unmap(pcmcia_chipset_handle_t pch, int win)
{
#ifdef TSLOT_DEBUG
	printf("[mem unmap %d]", win);
#endif
}

void
tslot_slot_disable(pcmcia_chipset_handle_t pch)
{
	struct tslot_data *td = (struct tslot_data *)pch;

#ifdef TSLOT_DEBUG
	printf("%s: disable slot %d\n",
	    td->td_parent->sc_dev.dv_xname, td->td_slot);
#endif

	/*
	 * Disable card access.
	 */
	TSLOT_WRITE(td, TS102_REG_CARD_A_STS,
	    TSLOT_READ(td, TS102_REG_CARD_A_STS) & ~TS102_CARD_STS_ACEN);

	/*
	 * Disable interrupts, except for insertion.
	 */
	TSLOT_WRITE(td, TS102_REG_CARD_A_INT,
	    TS102_CARD_INT_MASK_CARDDETECT_STATUS);
}

void
tslot_slot_enable(pcmcia_chipset_handle_t pch)
{
	struct tslot_data *td = (struct tslot_data *)pch;
	int status, intr, i;

#ifdef TSLOT_DEBUG
	printf("%s: enable slot %d\n",
	    td->td_parent->sc_dev.dv_xname, td->td_slot);
#endif

	/* Power down the socket to reset it */
	status = TSLOT_READ(td, TS102_REG_CARD_A_STS);
	TSLOT_WRITE(td, TS102_REG_CARD_A_STS, status | TS102_CARD_STS_VCCEN);

	/*
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since we
	 * are changing Vcc (Toff).
	 */
	DELAY((300 + 100) * 1000);

	/*
	 * Power on the card if not already done, and enable card access
	 */
	status |= TS102_CARD_STS_ACEN;
	status &= ~TS102_CARD_STS_VCCEN;
	TSLOT_WRITE(td, TS102_REG_CARD_A_STS, status);

	/*
	 * wait 100ms until power raise (Tpr) and 20ms to become
	 * stable (Tsu(Vcc)).
	 */
	DELAY((100 + 20) * 1000);

	status &= ~TS102_CARD_STS_VPP1_MASK;
	status |= TS102_CARD_STS_VPP1_VCC;
	TSLOT_WRITE(td, TS102_REG_CARD_A_STS, status);

	/*
	 * hold RESET at least 20us.
	 */
	intr = TSLOT_READ(td, TS102_REG_CARD_A_INT);
	TSLOT_WRITE(td, TS102_REG_CARD_A_INT, TS102_CARD_INT_SOFT_RESET);
	DELAY(20);
	TSLOT_WRITE(td, TS102_REG_CARD_A_INT, intr);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	DELAY(20 * 1000);

	/* We need level-triggered interrupts for PC Card hardware */
	TSLOT_WRITE(td, TS102_REG_CARD_A_STS,
		TSLOT_READ(td, TS102_REG_CARD_A_STS) | TS102_CARD_STS_LVL);

	/*
	 * Wait until the card is unbusy. If it is still busy after 3 seconds,
	 * give up. We could enable card interrupts and wait for the interrupt
	 * to happen when BUSY is released, but the interrupt could also be
	 * triggered by the card itself if it's an I/O card, so better poll
	 * here.
	 */
	for (i = 30000; i != 0; i--) {
		status = TSLOT_READ(td, TS102_REG_CARD_A_STS);
		/* If the card has been removed, abort */
		if ((status & TS102_CARD_STS_PRES) == 0) {
			tslot_slot_disable(pch);
			return;
		}
		if (status & TS102_CARD_STS_RDY)
			break;
		else
			DELAY(100);
	}

	if (i == 0) {
		printf("%s: slot %d still busy after 3 seconds, status 0x%x\n",
		    td->td_parent->sc_dev.dv_xname, td->td_slot,
		    TSLOT_READ(td, TS102_REG_CARD_A_STS));
		return;
	}

	/*
	 * Enable the card interrupts if this is an I/O card.
	 * Note that the TS102_CARD_STS_IO bit in the status register will
	 * never get set, despite what the documentation says!
	 */
	if (pcmcia_card_gettype(td->td_pcmcia) == PCMCIA_IFTYPE_IO) {
		TSLOT_WRITE(td, TS102_REG_CARD_A_STS,
		    TSLOT_READ(td, TS102_REG_CARD_A_STS) | TS102_CARD_STS_IO);
		TSLOT_WRITE(td, TS102_REG_CARD_A_INT,
		    TS102_CARD_INT_MASK_CARDDETECT_STATUS |
		    TS102_CARD_INT_MASK_IRQ);
	}
}

/*
 * Event management
 */
void
tslot_create_event_thread(void *v)
{
	struct tslot_softc *sc = v;
	const char *name = sc->sc_dev.dv_xname;

	if (kthread_create(tslot_event_thread, sc, &sc->sc_thread, "%s",
	    name) != 0) {
		panic("%s: unable to create event kthread", name);
	}
}

void
tslot_event_thread(void *v)
{
	struct tslot_softc *sc = v;
	struct tslot_data *td;
	int s, status;
	unsigned int socket;

	for (;;) {
		s = splhigh();

		if ((socket = ffs(sc->sc_events)) == 0) {
			splx(s);
			tsleep(&sc->sc_events, PWAIT, "tslot_event", 0);
			continue;
		}
		socket--;
		sc->sc_events &= ~(1 << socket);
		splx(s);

		if (socket >= TS102_NUM_SLOTS) {
#ifdef DEBUG
			printf("%s: invalid slot number %d\n",
			    sc->sc_dev.dv_xname, te->te_slot);
#endif
			continue;
		}

		td = &sc->sc_slot[socket];
		status = TSLOT_READ(td, TS102_REG_CARD_A_STS);

		if (status & TS102_CARD_STS_PRES) {
			/* Card insertion */
			if ((td->td_status & TS_CARD) == 0) {
				td->td_status |= TS_CARD;
				tadpole_set_pcmcia(td->td_slot, 1);
				pcmcia_card_attach(td->td_pcmcia);
			}
		} else {
			/* Card removal */
			if ((td->td_status & TS_CARD) != 0) {
				td->td_status &= ~TS_CARD;
				tadpole_set_pcmcia(td->td_slot, 0);
				pcmcia_card_detach(td->td_pcmcia,
				    DETACH_FORCE);
			}
		}
	}
}

/*
 * Interrupt handling
 */

int
tslot_intr(void *v)
{
	struct tslot_softc *sc = v;
	struct tslot_data *td;
	int intregs[TS102_NUM_SLOTS], *intreg;
	int i, rc = 0;

	/*
	 * Scan slots, and acknowledge the interrupt if necessary first
	 */
	for (i = 0; i < TS102_NUM_SLOTS; i++) {
		td = &sc->sc_slot[i];
		intreg = &intregs[i];
		*intreg = TSLOT_READ(td, TS102_REG_CARD_A_INT);

		/*
		 * Acknowledge all interrupt situations at once, even if they
		 * did not occur.
		 */
		if ((*intreg & (TS102_CARD_INT_STATUS_IRQ |
		    TS102_CARD_INT_STATUS_WP_STATUS_CHANGED |
		    TS102_CARD_INT_STATUS_BATTERY_STATUS_CHANGED |
		    TS102_CARD_INT_STATUS_CARDDETECT_STATUS_CHANGED)) != 0) {
			rc = 1;
			TSLOT_WRITE(td, TS102_REG_CARD_A_INT, *intreg |
			    TS102_CARD_INT_RQST_IRQ |
			    TS102_CARD_INT_RQST_WP_STATUS_CHANGED |
			    TS102_CARD_INT_RQST_BATTERY_STATUS_CHANGED |
			    TS102_CARD_INT_RQST_CARDDETECT_STATUS_CHANGED);
		}
	}

	/*
	 * Invoke the interrupt handler for each slot
	 */
	for (i = 0; i < TS102_NUM_SLOTS; i++) {
		td = &sc->sc_slot[i];
		intreg = &intregs[i];

		if ((*intreg & (TS102_CARD_INT_STATUS_IRQ |
		    TS102_CARD_INT_STATUS_WP_STATUS_CHANGED |
		    TS102_CARD_INT_STATUS_BATTERY_STATUS_CHANGED |
		    TS102_CARD_INT_STATUS_CARDDETECT_STATUS_CHANGED)) != 0)
			tslot_slot_intr(td, *intreg);
	}

	return (rc);
}

void
tslot_queue_event(struct tslot_softc *sc, int slot)
{
	int s;

	s = splhigh();
	sc->sc_events |= (1 << slot);
	splx(s);
	wakeup(&sc->sc_events);
}

void
tslot_slot_intr(struct tslot_data *td, int intreg)
{
	struct tslot_softc *sc = td->td_parent;
	int status, sockstat;

	status = TSLOT_READ(td, TS102_REG_CARD_A_STS);
#ifdef TSLOT_DEBUG
	printf("%s: interrupt on socket %d ir %x sts %x\n",
	    sc->sc_dev.dv_xname, td->td_slot, intreg, status);
#endif

	sockstat = td->td_status;

	/*
	 * The TS102 queues interrupt requests, and may trigger an interrupt
	 * for a condition the driver does not want to receive anymore (for
	 * example, after a card gets removed).
	 * Thus, only proceed if the driver is currently allowing a particular
	 * condition.
	 */

	if ((intreg & TS102_CARD_INT_STATUS_CARDDETECT_STATUS_CHANGED) != 0 &&
	    (intreg & TS102_CARD_INT_MASK_CARDDETECT_STATUS) != 0) {
		tslot_queue_event(sc, td->td_slot);
#ifdef TSLOT_DEBUG
		printf("%s: slot %d status changed from %d to %d\n",
		    sc->sc_dev.dv_xname, td->td_slot, sockstat, td->td_status);
#endif
		/*
		 * Ignore extra interrupt bits, they are part of the change.
		 */
		return;
	}

	if ((intreg & TS102_CARD_INT_STATUS_IRQ) != 0 &&
	    (intreg & TS102_CARD_INT_MASK_IRQ) != 0) {
		/* ignore interrupts if we have a pending state change */
		if (sc->sc_events & (1 << td->td_slot))
			return;

		if ((sockstat & TS_CARD) == 0) {
			printf("%s: spurious interrupt on slot %d isr %x\n",
			    sc->sc_dev.dv_xname, td->td_slot, intreg);
			return;
		}

		if (td->td_intr != NULL) {
			/*
			 * XXX There is no way to honour the interrupt handler
			 * requested IPL level...
			 */
			(*td->td_intr)(td->td_intrarg);
		}
	}
}

void
tslot_intr_disestablish(pcmcia_chipset_handle_t pch, void *ih)
{
	struct tslot_data *td = (struct tslot_data *)pch;

	td->td_intr = NULL;
	td->td_intrarg = NULL;
}

const char *
tslot_intr_string(pcmcia_chipset_handle_t pch, void *ih)
{
	if (ih == NULL)
		return ("couldn't establish interrupt");
	else
		return ("");	/* nothing for now */
}


void *
tslot_intr_establish(pcmcia_chipset_handle_t pch, struct pcmcia_function *pf,
    int ipl, int (*handler)(void *), void *arg, char *xname)
{
	struct tslot_data *td = (struct tslot_data *)pch;

	td->td_intr = handler;
	td->td_intrarg = arg;

	return (td);
}
