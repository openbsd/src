/*	$Id: pcmcia_pcic.c,v 1.7 1996/10/16 12:36:04 deraadt Exp $	*/
/*
 *  Copyright (c) 1995, 1996 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
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
 * 
 */
/*
 * Device Driver for Intel 82365 based pcmcia slots
 *
 * Copyright (c) 1994 Stefan Grefen. This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 *
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/ic/i82365reg.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>

#ifdef IBM_WD
#define PCIC_DEBUG 0xf
#endif
#if PCIC_DEBUG
#define PCDMEM  0x01
#define PCDIO   0x02
#define PCDINTR 0x04
#define PCDSERV 0x08
#define PCDRW   0x10
#define PCDCONF 0x20
int             pcic_debug = PCIC_DEBUG;
#define DEBUG(a)	(pcic_debug & (a))
#else
#define DEBUG(a)	(0)
#endif

/*
 *  pcic_softc: per line info and status
 */
#define MAX_IOSECTION 	2
#define MAX_MEMSECTION 	5
struct slot {
	int             status;
#define SLOT_EMPTY    0x01
#define SLOT_PROBING  0x02
#define SLOT_INUSE    0x04
	u_short         io_addr[MAX_IOSECTION], io_len[MAX_IOSECTION];
	u_short         io_used[MAX_IOSECTION];
	caddr_t         mem_caddr[MAX_MEMSECTION];
	u_int           mem_haddr[MAX_MEMSECTION];
	u_int           mem_len[MAX_MEMSECTION], mem_used[MAX_MEMSECTION];
	u_short         region_flag;
	u_short         ioctl_flag;
	u_short         irq;
	u_short         pow;
	u_short         irqt;
	u_short         reg_off;/* 2 chips share an address */
	void		(*handler)(struct slot *, void *);
	void *		handle_arg;
	struct pcmcia_link *link;
	struct pcic_softc *chip;
};

struct pcic_softc {
	struct device sc_dev;
	bus_chipset_tag_t sc_bc;
	struct pcmcia_adapter sc_adapter;
	void *sc_ih;

	int	sc_polltimo;
	int	sc_pcic_irq;
	bus_io_handle_t sc_ioh;
	bus_mem_handle_t sc_memh;
	u_short pcic_base;	/* base port for each board */
	u_char	chip_inf;
	struct slot slot[4];		/* treat up to 4 as on the same pcic */
};
#define pcic_parent(sc) ((struct pcicmaster_softc *)(sc)->sc_dev.dv_parent)

static int      pcic_map_io __P((struct pcmcia_link *, u_int, u_int, int));
static int      pcic_map_mem __P((struct pcmcia_link *, bus_chipset_tag_t,
				  bus_mem_handle_t,
				  u_int, u_int, int));
static int      pcic_map_intr __P((struct pcmcia_link *, int, int));
static int      pcic_service __P((struct pcmcia_link *, int, void *, int));

static struct pcmcia_funcs pcic_funcs = {
	pcic_map_io,
	pcic_map_mem,
	pcic_map_intr,
	pcic_service
};

int pcic_probe __P((struct device *, void *, void *));
void pcic_attach __P((struct device  *, struct device *, void *));
int pcic_print __P((void *, char *));

int pcicmaster_probe __P((struct device *, void *, void *));
void pcicmaster_attach __P((struct device  *, struct device *, void *));
int pcicmaster_print __P((void *, char *));

extern struct pcmciabus_link pcmcia_isa_link;

struct cfattach pcic_ca = {
	sizeof(struct pcic_softc), pcic_probe, pcic_attach,
};

struct cfdriver pcic_cd = {
	NULL, "pcic", DV_DULL
};

struct pcicmaster_softc {
	struct device sc_dev;
	bus_chipset_tag_t sc_bc;
	bus_io_handle_t sc_ioh;
	struct pcic_softc *sc_ctlrs[2];
	char sc_slavestate[2];
#define SLAVE_NOTPRESENT	0
#define SLAVE_FOUND		1
#define SLAVE_CONFIGURED	2
};

struct cfattach pcicmaster_ca = {
	sizeof(struct pcicmaster_softc), pcicmaster_probe, pcicmaster_attach,
};

struct cfdriver pcicmaster_cd = {
	NULL, "pcicmaster", DV_DULL, 1
};

struct pcic_attach_args {
	int	pia_ctlr;		/* pcic ctlr number */
	bus_chipset_tag_t pia_bc;	/* bus chipset tag */
	bus_io_handle_t	pia_ioh;	/* base i/o address */
	int	pia_iosize;		/* span of ports used */
	int	pia_irq;		/* interrupt request */
	int	pia_drq;		/* DMA request */
	int	pia_maddr;		/* physical i/o mem addr */
	u_int	pia_msize;		/* size of i/o memory */
};

static u_char pcic_rd __P((struct slot *, int));
static void pcic_wr __P((struct slot *, int, int));


static __inline u_char
pcic_rd(slot, reg)
	struct slot    *slot;
	int             reg;
{
	u_char          res;
	bus_chipset_tag_t bc = slot->chip->sc_bc;
	bus_io_handle_t ioh =  slot->chip->sc_ioh;
	if (DEBUG(PCDRW))
		printf("pcic_rd(%x [%x %x]) = ", reg, slot->reg_off, ioh);
	bus_io_write_1(bc, ioh, 0, slot->reg_off + reg);
	delay(1);
	res = bus_io_read_1(bc, ioh, 1);
	if (DEBUG(PCDRW))
		printf("%x\n", res);
	return res;
}

static __inline void
pcic_wr(slot, reg, val)
	struct slot    *slot;
	int             reg, val;
{
	bus_chipset_tag_t bc = slot->chip->sc_bc;
	bus_io_handle_t ioh =  slot->chip->sc_ioh;
	bus_io_write_1(bc, ioh, 0, slot->reg_off + reg);
	delay(1);
	bus_io_write_1(bc, ioh, 1, val);
	if (DEBUG(PCDRW)) {
		int res;
		delay(1);
		bus_io_write_1(bc, ioh, 0, slot->reg_off + reg);
		delay(1);
		res = bus_io_read_1(bc, ioh, 1);
		printf("pcic_wr(%x %x) = %x\n", reg, val, res);
	}
}

static __inline int pcic_wait __P((struct slot *, int));

static __inline int
pcic_wait(slot, i)
	struct slot *slot;
	int i;
{
	while (i-- && ((pcic_rd(slot, PCIC_STATUS) & PCIC_READY) == 0))
		delay(500);
	return i;
}

int
pcic_probe(parent, self, aux)
	struct device	*parent;
	void		*self;
	void            *aux;
{
	struct pcic_softc *pcic = self;
	struct pcicmaster_softc *pcicm = (struct pcicmaster_softc *) parent;
	struct pcic_attach_args *pia = aux;
	bus_mem_handle_t memh;
	u_int           chip_inf = 0, ochip_inf = 0;
	int first = 1;
	int             i, j, maxslot;

	bzero(pcic->slot, sizeof(pcic->slot));

	if (DEBUG(PCDCONF)) {
		printf("pcic_probe controller %d unit %d\n", pia->pia_ctlr,
		       pcic->sc_dev.dv_unit);
		delay(2000000);
	}
	if (pcicm->sc_slavestate[pia->pia_ctlr] != SLAVE_FOUND)
		return 0;
	if (pcic->sc_dev.dv_cfdata->cf_loc[1] == -1 ||
	    pcic->sc_dev.dv_cfdata->cf_loc[2] == 0)
		return 0;

	/*
	 * select register offsets based on which controller we are.
	 * 2 pcic controllers (w/ 2 slots each) possible at each
	 * IO port location, for a total of 8 possible PCMCIA slots.
	 *
	 * for VLSI controllers, we probe up to 4 slots for the same chip type,
	 * and handle them on one controller.  This is slightly
	 * cheating (two separate pcic's are required for 4 slots, according
	 * to the i82365 spec).
	 * 
	 * For other controllers, we only take up to 2 slots.
	 */
	pcic->sc_ioh = pia->pia_ioh;
	pcic->sc_bc = pia->pia_bc;
	pcic->sc_adapter.nslots = 0;
	maxslot = 2;
	for (i = j = 0; i < maxslot; i++) {
	    pcic->slot[j].reg_off = 0x80 * pia->pia_ctlr + 0x40 * i;
	    pcic->slot[j].chip = pcic;

	    chip_inf = pcic_rd(&pcic->slot[j], PCIC_ID_REV);
	    if (DEBUG(PCDCONF)) {
		printf("pcic_probe read info %x\n", chip_inf);
		delay(2000000);
	    }
	    if (!first && ochip_inf != chip_inf)
		continue;		/* don't attach, it's different */
	    ochip_inf = chip_inf;
	    switch (chip_inf) {
	    case PCIC_INTEL0:
		pcic->chip_inf = PCMICA_CHIP_82365_0;
		goto ok;
	    case PCIC_INTEL1:
		pcic->chip_inf = PCMICA_CHIP_82365_1;
		goto ok;
	    case PCIC_IBM1:
		pcic->chip_inf = PCMICA_CHIP_IBM_1;
		goto ok;
	    case PCIC_146FC6:
		pcic->chip_inf = PCMICA_CHIP_146FC6;
		maxslot = 4;
		goto ok;
	    case PCIC_146FC7:
		pcic->chip_inf = PCMICA_CHIP_146FC7;
		maxslot = 4;
		goto ok;
	    case PCIC_IBM2:
		pcic->chip_inf = PCMICA_CHIP_IBM_2;
	ok:
		if (first) {
		    pcic->sc_adapter.adapter_softc = (void *)pcic;
		    pcic->sc_adapter.chip_link = &pcic_funcs;
		    pcic->sc_adapter.bus_link = &pcmcia_isa_link;
		    pcicm->sc_ctlrs[pia->pia_ctlr] = pcic;
		    pcicm->sc_slavestate[pia->pia_ctlr] = SLAVE_CONFIGURED;
		    first = 0;
		}
		pcic->sc_adapter.nslots++;
		j++;
	    default:
		if (DEBUG(PCDCONF)) {
		    printf("found ID %x at pcic%d position\n",
			   chip_inf & 0xff, pcic->sc_dev.dv_unit);
		}
		continue;
	    }
	}
	if (pcic->sc_adapter.nslots != 0) {
		pcic->sc_memh = memh;
		return 1;
	}
	if (DEBUG(PCDCONF)) {
		printf("pcic_probe failed\n");
		delay(2000000);
	}
	bus_mem_unmap(pia->pia_bc, memh, pia->pia_msize);
	return 0;
}

int
pcic_intr __P((void *));

int
pcic_print(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)       
		printf("%s: pcmciabus ", name);
	return UNCONF;
}

void
pcic_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct pcic_softc *pcic = (void *) self;
	struct pcic_attach_args *pia = aux;
	struct pcmciabus_attach_args pba;
	struct slot *slot;
	int i;
	static char    *pcic_names[] = {
		"Intel 82365sl Rev. 0",
		"Intel 82365sl Rev. 1",
		"IBM 82365sl clone Rev. 1",
		"IBM 82365sl clone Rev. 2",
		"VL82146 (82365sl clone) Rev. 6",
		"VL82146 (82365sl clone) Rev. 7" };
	if (DEBUG(PCDCONF)) {
		printf("pcic_attach found\n");
		delay(2000000);
	}
	pia->pia_irq = self->dv_cfdata->cf_loc[0];
	pia->pia_maddr = self->dv_cfdata->cf_loc[1];
	pia->pia_msize = self->dv_cfdata->cf_loc[2];

	printf(": %s slots %d-%d iomem %x-%x",
	       pcic_names[pcic->chip_inf - PCMICA_CHIP_82365_0],
	       pcic->sc_dev.dv_unit * 2,
	       pcic->sc_dev.dv_unit * 2 + pcic->sc_adapter.nslots - 1,
	       pia->pia_maddr, pia->pia_maddr + pia->pia_msize - 1);
	if (pia->pia_irq != IRQUNK)
		printf(" irq %d\n", pia->pia_irq);
	else
		printf("\n");
	if (DEBUG(PCDCONF))
		delay(2000000);

#ifdef PCMCIA_ISA_DEBUG
	printf("pcic %p slots %p,%p\nisaaddr %p ports %x size %d irq %d drq %d maddr %x msize %x\n",
	       pcic, &pcic->slot[0], &pcic->slot[1],
	       pia, pia->pia_ioh, pia->pia_iosize,
	       pia->pia_irq, pia->pia_drq, pia->pia_maddr, pia->pia_msize);
	if (DEBUG(PCDCONF))
		delay(2000000);
#endif

	/* enable interrupts on events */
	if (pia->pia_irq != IRQUNK)
	    pcic->sc_pcic_irq = pia->pia_irq;
	else
	    pcic->sc_pcic_irq = 0;

	for (i = 0; i < pcic->sc_adapter.nslots; i++) {
		slot = &pcic->slot[i];
		/*
		 * Arrange for card status change interrupts
		 * to be steered to specified IRQ.
		 * Treat all cards as I/O cards for the moment so we get
		 * sensible card change interrupt codes (besides, we don't
		 * support memory cards :)
		 */
		pcic_wr(slot, PCIC_STAT_INT,
			(pcic->sc_pcic_irq << 4) |
			PCIC_CDTCH | PCIC_IOCARD);
		slot->irq = pcic_rd(slot, PCIC_INT_GEN) & ~PCIC_INTR_ENA;
		pcic_wr(slot, PCIC_INT_GEN, slot->irq);
		(void) pcic_rd(slot, PCIC_STAT_CHG);
	}
	if (pia->pia_irq == IRQUNK) {
	    pcic->sc_polltimo = hz/2;
	    timeout((void (*)(void *))pcic_intr, pcic, pcic->sc_polltimo);
	} else {
	    pcic->sc_ih = isa_intr_establish(pia->pia_bc,
					     pia->pia_irq, IST_EDGE,
					     IPL_PCMCIA, pcic_intr, pcic, pcic->sc_dev.dv_xname);
	    pcic->sc_polltimo = 0;
	}
	/*
	 * Probe the pcmciabus at this controller.
	 */
	pba.pba_bc = pia->pia_bc;
	pba.pba_maddr = pia->pia_maddr;
	pba.pba_msize = pia->pia_msize;
	pba.pba_aux = &pcic->sc_adapter;
#ifdef PCMCIA_DEBUG
	printf("config_found(%p, %p, %p)\n",
	       self, &pba, pcic_print);
#endif
	config_found(self, (void *)&pba, pcic_print);
}

int
pcic_intr(arg)
void *arg;
{
	struct pcic_softc *pcic = arg;
	u_char statchg, intgen;
	register int i;

#ifdef PCMCIA_PCIC_DEBUG
	if (pcic->sc_polltimo == 0)
		printf("%s: interrupt:", pcic->sc_dev.dv_xname);
#endif
	for (i = 0; i < pcic->sc_adapter.nslots; i++) {
		struct pcmcia_link *link = pcic->slot[i].link;
		statchg = pcic_rd(&pcic->slot[i], PCIC_STAT_CHG);
		if (statchg == 0)
			continue;
		intgen = pcic_rd(&pcic->slot[i], PCIC_INT_GEN);
#ifdef PCMCIA_PCIC_DEBUG
		if (intgen & PCIC_IOCARD) {
			printf("%s: slot %d iocard status %s%s\n", 
			       pcic->sc_dev.dv_xname, i,
			       statchg & PCIC_STCH ? "statchange " : "",
			       statchg & PCIC_CDTCH ? "cardchange" : "");
		} else {
			printf("%s: slot %d memcard status %x\n",
			       pcic->sc_dev.dv_xname, i, statchg);
		} 
#endif
		if ((statchg & PCIC_CDTCH) &&
		    (link->flags & PCMCIA_SLOT_OPEN) == 0) {
#if 0
			if (pcic->slot[i].status & SLOT_INUSE) {
				pcmcia_unconfigure(link);
			} else {
				if (link) {
					link->fordriver = NULL;
					pcmcia_probe_bus(link, 0, link->slot,
							 NULL);
				}
			}
#endif
		}
		if (link && (link->flags & PCMCIA_SLOT_OPEN)) {
			link->flags |= PCMCIA_SLOT_EVENT;
			selwakeup(&link->pcmcialink_sel);
		}
		if (pcic->slot[i].handler == NULL)
			continue;
		(*pcic->slot[i].handler)(&pcic->slot[i],
					 pcic->slot[i].handle_arg);
	}
	if (pcic->sc_polltimo)
		timeout((void (*)(void *))pcic_intr, pcic, pcic->sc_polltimo);
	return 1;
}

static int
pcic_map_io(link, start, len, flags)
	struct pcmcia_link *link;
	u_int           start, len;
	int		flags;
{
	struct pcic_softc *sc = link->adapter->adapter_softc;
	struct slot    *slot;
	if (link->slot >= sc->sc_adapter.nslots)
		return ENXIO;
	slot = &sc->slot[link->slot];

	len--;
	if (DEBUG(PCDIO)) {
		printf("pcic_map_io %x %x %x\n", start, len, flags);
	}
	if (!(flags & PCMCIA_UNMAP)) {
		u_int           stop;
		int             window;
		int             winid;
		int             ioflags;

		if (flags & PCMCIA_LAST_WIN) {
			window = MAX_IOSECTION - 1;
		} else if (flags & PCMCIA_FIRST_WIN) {
			window = 0;
		} else if (flags & PCMCIA_ANY_WIN) {
			for (window = 0; window < MAX_IOSECTION; window++) {
				if (slot->io_used[window] == 0)
					break;
				if (window >= MAX_IOSECTION)
					return EBUSY;
			}
		} else {
			window = flags & 0xf;
			if (window >= MAX_IOSECTION)
				return EINVAL;
		}
		slot->status |= SLOT_INUSE;
		slot->io_used[window] = 1;
		winid = window * 0x4 + 0x08;
		stop = start + len;

		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_LOW,
			(u_long) start & 0xff);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_HIGH,
			((u_long) start >> 8) & 0xff);

		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_LOW,
			stop & 0xff);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_HIGH,
			(stop >> 8) & 0xff);
		flags &= (PCMCIA_MAP_8 | PCMCIA_MAP_16);
		switch (flags) {
		case PCMCIA_MAP_8:
			ioflags = PCIC_IO0_0WS;
			break;
		case PCMCIA_MAP_16:
			ioflags = PCIC_IO0_16BIT;
			break;
		default:
			ioflags = PCIC_IO0_CS16;
			break;
		}

		if (window == 1) {
			ioflags <<= 4;
			slot->ioctl_flag &= ~(3 << 4);
		}
		else {
			slot->ioctl_flag &= ~3;
		}

		delay(1000);

		pcic_wr(slot, PCIC_IOCTL, slot->ioctl_flag |= ioflags);
		pcic_wr(slot, PCIC_ADDRWINE,
			slot->region_flag |= (0x40 << window));
		slot->io_addr[window] = start;
		slot->io_len[window] = len;
		delay(1000);
		return 0;
	} else {
		int             window;
		int             winid;
		if (flags & PCMCIA_LAST_WIN) {
			window = MAX_IOSECTION - 1;
		} else if (flags & PCMCIA_FIRST_WIN) {
			window = 0;
		} else if (flags & PCMCIA_ANY_WIN) {
			for (window = 0; window < MAX_IOSECTION; window++) {
				if (slot->io_addr[window] == start)
					if (len == -1 ||
					    slot->io_len[window] == len)
						break;
			}
			if (window >= MAX_IOSECTION)
				return EINVAL;
		} else {
			window = flags & 0xf;
			if (window >= MAX_IOSECTION)
				return EINVAL;
		}
		slot->status &= ~SLOT_INUSE;
		winid = window * 0x4 + 0x08;

		pcic_wr(slot, PCIC_ADDRWINE,
			slot->region_flag &= ~(0x40 << window));
		delay(1000);
		pcic_wr(slot, PCIC_IOCTL,
			slot->ioctl_flag &= ~(0xf << window));
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_LOW, 0);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_HIGH, 0);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_LOW, 0);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_HIGH, 0);

		slot->io_addr[window] = start;
		slot->io_len[window] = len;
		return 0;
	}
}

static int
pcic_map_mem(link, bc, ioh, start, len, flags)
	struct pcmcia_link *link;
	bus_chipset_tag_t bc;
	bus_mem_handle_t ioh;
	u_int           start, len;
	int		flags;
{
	vm_offset_t     physaddr;
	struct pcic_softc *sc = link->adapter->adapter_softc;
	struct slot    *slot;
	caddr_t haddr = ioh;		/* XXX */
	if (link->slot >= sc->sc_adapter.nslots)
		return ENXIO;

	slot = &sc->slot[link->slot];

	if (flags & PCMCIA_PHYSICAL_ADDR)
		physaddr = (vm_offset_t) haddr;
	else
		physaddr = pmap_extract(pmap_kernel(), (vm_offset_t) haddr);
	if (DEBUG(PCDMEM))
		printf("pcic_map_mem %p %lx %x %x %x\n", haddr, physaddr,
		       start, len, flags);

	(u_long) physaddr >>= 12;
	start >>= 12;
	len = (len - 1) >> 12;

	if (!(flags & PCMCIA_UNMAP)) {
		u_int           offs;
		u_int           stop;
		int             window;
		int             winid;
		if (flags & PCMCIA_LAST_WIN) {
			window = MAX_MEMSECTION - 1;
		} else if (flags & PCMCIA_FIRST_WIN) {
			window = 0;
		} else if (flags & PCMCIA_ANY_WIN) {
			for (window = 0; window < MAX_MEMSECTION; window++) {
				if (slot->mem_used[window] == 0)
					break;
				if (window >= MAX_MEMSECTION)
					return EBUSY;
			}
		} else {
			window = flags & 0xf;
			if (window >= MAX_MEMSECTION)
				return EINVAL;
		}
		slot->mem_used[window] = 1;

		offs = (start - (u_long) physaddr) & 0x3fff;
		if (DEBUG(PCDMEM))
			printf("mapmem 2:%x %lx %x\n", offs, physaddr + offs,
			       start);

		stop = (u_long) physaddr + len;

		winid = window * 0x8 + 0x10;


		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_LOW,
			(u_long) physaddr & 0xff);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_HIGH,
			(((u_long) physaddr >> 8) & 0x3f) |
		/* PCIC_ZEROWS| */
			((flags & PCMCIA_MAP_16) ? PCIC_DATA16 : 0));

		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_LOW,
			stop & 0xff);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_HIGH,
			PCIC_MW1 | ((stop >> 8) & 0x3f));


		pcic_wr(slot, winid | PCIC_MOFF | PCIC_ADDR_LOW,
			offs & 0xff);
		pcic_wr(slot, winid | PCIC_MOFF | PCIC_ADDR_HIGH,
			((offs >> 8) & 0x3f) |
			((flags & PCMCIA_MAP_ATTR) ? PCIC_REG : 0));
		delay(1000);

		pcic_wr(slot, PCIC_ADDRWINE,
			slot->region_flag |= ((1 << window) | PCIC_MEMCS16));
		slot->mem_caddr[window] = (caddr_t) physaddr;
		slot->mem_haddr[window] = start;
		slot->mem_len[window] = len;
		delay(1000);
		return 0;
	} else {
		int             window;
		int             winid;

		if (flags & PCMCIA_LAST_WIN) {
			window = MAX_MEMSECTION - 1;
		} else if (flags & PCMCIA_FIRST_WIN) {
			window = 0;
		} else if (flags & PCMCIA_ANY_WIN) {
			for (window = 0; window < MAX_MEMSECTION; window++) {
				if ((slot->mem_caddr[window] ==
				     (caddr_t) physaddr) &&
				    ((start == -1) ||
				     (slot->mem_haddr[window] == start)) &&
				    ((len == -1) ||
				     (slot->mem_len[window] == len)))
					break;
			}
			if (window >= MAX_MEMSECTION)
				return EINVAL;
		} else {
			window = flags & 0xf;
			if (window >= MAX_MEMSECTION)
				return EINVAL;
		}
		winid = window * 0x8 + 0x10;

		slot->region_flag &= (~(1 << window));
		pcic_wr(slot, PCIC_ADDRWINE, slot->region_flag);
		delay(1000);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_LOW, 0);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_HIGH, 0);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_LOW, 0);
		pcic_wr(slot, winid | PCIC_END | PCIC_ADDR_HIGH, 0);
		pcic_wr(slot, winid | PCIC_MOFF | PCIC_ADDR_LOW, 0);
		pcic_wr(slot, winid | PCIC_MOFF | PCIC_ADDR_HIGH, 0);
		slot->mem_caddr[window] = 0;
		slot->mem_haddr[window] = 0;
		slot->mem_len[window] = 0;
		slot->mem_used[window] = 0;
		return 0;
	}
}

static int
pcic_map_intr(link, irq, flags)
	struct pcmcia_link *link;
	int             irq, flags;
{
	struct pcic_softc *sc = link->adapter->adapter_softc;
	struct slot    *slot;
	if (link->slot >= sc->sc_adapter.nslots)
		return ENXIO;

	slot = &sc->slot[link->slot];

	if (DEBUG(PCDINTR))
		printf("pcic_map_intr %x %x\n", irq, flags);

	if (flags & PCMCIA_UNMAP) {
		slot->irq &= ~(PCIC_INT_MASK|PCIC_INTR_ENA);
		pcic_wr(slot, PCIC_INT_GEN, slot->irq);
	}
	else {
		if (irq < 2 || irq > 15 || irq == 6 || irq == 8 || irq == 13)
			return EINVAL;
		if(irq==2)
		    irq=9;
		slot->irq &= ~(PCIC_INTR_ENA|PCIC_INT_MASK);
		slot->irq |= irq | PCIC_CARDRESET;	/* reset is inverted */
		pcic_wr(slot, PCIC_INT_GEN, slot->irq);
	}
	return 0;
}


static int
pcic_service(link, opcode, arg, flags)
	struct pcmcia_link *link;
	int             opcode;
	void           *arg;
	int		flags;
{
	struct pcic_softc *sc = link->adapter->adapter_softc;
	struct slot    *slot;
	if (link->slot >= sc->sc_adapter.nslots)
		return ENXIO;

	slot = &sc->slot[link->slot];

	slot->link = link;		/* save it for later :) */
	switch (opcode) {
	case PCMCIA_OP_STATUS:{
			u_char          cp;
			int            *iarg = arg;

			if (DEBUG(PCDSERV))
				printf("pcic_service(status)\n");
			cp = pcic_rd(slot, PCIC_STATUS);
			if (DEBUG(PCDSERV))
				printf("status for slot %d %b\n",
				       link->slot, cp, PCIC_STATUSBITS);
			*iarg = 0;
#define DO_STATUS(cp, val, map)	((cp & val) == val ? map : 0)
			*iarg |= DO_STATUS(cp, PCIC_CD, PCMCIA_CARD_PRESENT);
			*iarg |= DO_STATUS(cp, PCIC_BVD, PCMCIA_BATTERY);
			*iarg |= DO_STATUS(cp, PCIC_MWP, PCMCIA_WRITE_PROT);
			*iarg |= DO_STATUS(cp, PCIC_READY, PCMCIA_READY);
			*iarg |= DO_STATUS(cp, PCIC_POW, PCMCIA_POWER);
			*iarg |= DO_STATUS(cp, PCIC_VPPV, PCMCIA_POWER_PP);
			return 0;

		}
	case PCMCIA_OP_WAIT:{
			int             iarg = (int) arg;
			int             i = iarg * 4;

			if (DEBUG(PCDSERV))
				printf("pcic_service(wait)\n");
			i = pcic_wait(slot, i);
			if (DEBUG(PCDSERV))
				printf("op99 %b %d\n", 
				       pcic_rd(slot, PCIC_STATUS),
				       PCIC_STATUSBITS, i);
			if (i <= 0)
				return EIO;
			else
				return 0;
		}
	case PCMCIA_OP_RESET:{
			int             force = ((int) arg) < 0;
			int             iarg = abs((int) arg);
			int             i = iarg * 4;

			if (DEBUG(PCDSERV))
				printf("pcic_service(reset)\n");
			if (flags)
				slot->irq |= PCIC_IOCARD;
			else
				slot->irq &= ~PCIC_IOCARD; /* XXX? */
			pcic_wr(slot, PCIC_POWER, slot->pow &= ~PCIC_DISRST);
			slot->irq &= ~PCIC_CARDRESET;
			pcic_wr(slot, PCIC_INT_GEN, slot->irq);
			if (iarg == 0)
				return 0;
			delay(iarg);
			pcic_wr(slot, PCIC_POWER, slot->pow |= PCIC_DISRST);
			slot->irq |= PCIC_CARDRESET;
			pcic_wr(slot, PCIC_INT_GEN, slot->irq);
			delay(iarg);
			i = pcic_wait(slot, i);
			if (DEBUG(PCDSERV))
				printf("opreset %d %b %d\n", force, 
				       pcic_rd(slot, PCIC_STATUS),
				       PCIC_STATUSBITS, i);
			if (i <= 0)
				return EIO;
			else
				return 0;
		}
	case PCMCIA_OP_POWER:{
			int             iarg = (int) arg;
			if (DEBUG(PCDSERV))
				printf("pcic_service(power): ");
			if (flags & PCMCIA_POWER_ON) {
				int nv = (PCIC_DISRST|PCIC_OUTENA);
				pcic_wr(slot, PCIC_INT_GEN,
					slot->irq = PCIC_IOCARD);
				if(flags & PCMCIA_POWER_3V)
					nv |= PCIC_VCC3V;
				if(flags & PCMCIA_POWER_5V)
					nv |= PCIC_VCC5V;
				if(flags & PCMCIA_POWER_AUTO)
					nv |= PCIC_APSENA|
					      PCIC_VCC5V|PCIC_VCC3V;
				slot->pow &= ~(PCIC_APSENA|PCIC_VCC5V|
					       PCIC_VCC3V|PCIC_VPP12V|
					       PCIC_VPP5V);
				slot->pow |= nv;
				pcic_wr(slot, PCIC_POWER, slot->pow);
#if 0
				delay(iarg);
				slot->pow |= PCIC_OUTENA;
				pcic_wr(slot, PCIC_POWER, slot->pow);
#endif
				delay(iarg);
				if (DEBUG(PCDSERV))
					printf("on\n");
			} else {
				slot->pow &= ~(PCIC_APSENA|PCIC_VCC5V|
					       PCIC_VCC3V);
				slot->pow &= ~(PCIC_DISRST|PCIC_OUTENA);
				pcic_wr(slot,PCIC_POWER, slot->pow);
				if (DEBUG(PCDSERV))
					printf("off\n");
			}
			return 0;
		}
	case PCMCIA_OP_GETREGS:{
			struct pcic_regs *pi = arg;
			int             i;
			if (DEBUG(PCDSERV))
				printf("pcic_service(getregs)\n");
			pi->chip_vers = sc->chip_inf;
			for (i = 0; i < pi->cnt; i++)
				pi->reg[i].val =
				    pcic_rd(slot, pi->reg[i].addr);
			return 0;
		}
	default:
		if (DEBUG(PCDSERV))
			printf("pcic_service(%x)\n", opcode);
		return EINVAL;
	}
}

/*
 * Handle I/O space mapping for children.  Thin layer.
 */
int
pcicmaster_probe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct pcicmaster_softc *pcicm = self;
	struct isa_attach_args *ia = aux;
	struct cfdata *cf = pcicm->sc_dev.dv_cfdata;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;

	u_int           chip_inf = 0;
	int             i, j;
	int rval = 0;
	struct pcic_softc pcic;		/* faked up for probing only */

	if (DEBUG(PCDCONF)) {
		printf("pcicmaster_probe\n");
		delay(2000000);
	}
	bc = ia->ia_bc;
	if (bus_io_map(bc, ia->ia_iobase, PCIC_NPORTS, &ioh))
		return (0);
	/*
	 * Probe the slots for each of the possible child controllers,
	 * and if any are there we return a positive indication.
	 */
	pcic.sc_ioh = ioh;
	for (i = 0; i < 2; i++) {
		bzero(pcic.slot, sizeof(pcic.slot));
		pcic.slot[0].chip = &pcic;
		pcic.slot[0].reg_off = i * 0x80;
		chip_inf = pcic_rd(&pcic.slot[0], PCIC_ID_REV);
		switch (chip_inf) {
		case PCIC_INTEL0:
		case PCIC_INTEL1:
		case PCIC_IBM1:
		case PCIC_146FC6:
		case PCIC_146FC7:
		case PCIC_IBM2:
			if (DEBUG(PCDCONF)) {
				printf("pcicmaster_probe found, cf=%p\n", cf);
				delay(2000000);
			}
			pcicm->sc_slavestate[i] = SLAVE_FOUND;
			rval++;
			break;
		default:
			pcicm->sc_slavestate[i] = SLAVE_NOTPRESENT;
			if (DEBUG(PCDCONF)) {
			    printf("found ID %x at slave %d\n",
				   chip_inf & 0xff, i);
			}
			break;
		}
		if (pcicm->sc_slavestate[i] != SLAVE_FOUND) {
			/* reset mappings .... */
			pcic_wr(&pcic.slot[0], PCIC_POWER,
				pcic.slot[0].pow=PCIC_DISRST);
			delay(1000);
			for (j = PCIC_INT_GEN; j < 0x40; j++) {
				pcic_wr(&pcic.slot[0], j, 0);
			}
			delay(10000);
		}
	}
	if (rval) {
		ia->ia_iosize = 2;
		pcicm->sc_bc = bc;
		pcicm->sc_ioh = ioh;
	} else
	    bus_io_unmap(bc, ioh, PCIC_NPORTS);
	return rval;
}

void
pcicmaster_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct pcicmaster_softc *pcicm = (void *) self;
	struct isa_attach_args *ia = aux;
	struct pcic_attach_args pia;
	int i;
	printf("\n");
	if (DEBUG(PCDCONF)) {
		printf("pcicmaster_attach\n");
		delay(2000000);
	}
#ifdef PCMCIA_ISA_DEBUG
	printf("pcicm %p isaaddr %p ports %x size %d irq %d drq %d maddr %x msize %x\n",
	       pcicm, ia, ia->ia_iobase, ia->ia_iosize,
	       ia->ia_irq, ia->ia_drq, ia->ia_maddr, ia->ia_msize);
	if (DEBUG(PCDCONF))
		delay(2000000);
#endif
	/* attach up to two PCICs at this I/O address */
	for (i = 0; i < 2; i++) {
		if (pcicm->sc_slavestate[i] == SLAVE_FOUND) {
			pia.pia_ctlr = i;
			/*
			 * share the I/O space and memory mapping space.
			 */
			pia.pia_bc = pcicm->sc_bc;
			pia.pia_ioh = pcicm->sc_ioh;
			pia.pia_iosize = ia->ia_iosize;
			pia.pia_drq = ia->ia_drq;
#if 0
			pia.pia_irq = ia->ia_irq;
			pia.pia_irq = cf->cf_loc[0]; /* irq from master attach */
			pia.pia_maddr = ia->ia_maddr + (ia->ia_msize / 2) * i;
			pia.pia_msize = ia->ia_msize / 2;
#endif

			config_found(self, &pia, pcicmaster_print);
		}
	}
}

int
pcicmaster_print(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)       
		printf("%s: master controller ", name);
	return UNCONF;
}
