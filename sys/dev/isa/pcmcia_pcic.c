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

#include <dev/pcmcia/pcmciabus.h>
#ifdef IBM_WD
#define PCIC_DEBUG 0xf
#endif
#if PCIC_DEBUG
#define PCDMEM  0x01
#define PCDIO   0x02
#define PCDINTR 0x04
#define PCDSERV 0x08
#define PCDRW   0x10
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
	struct device   sc_dev;
	void *sc_ih;

	int	sc_polltimo;
	int	sc_pcic_irq;
	u_short         pcic_base;	/* base port for each board */
	u_char          slot_id;
	u_char          chip_inf;
	struct slot     slot[2];
} pcic_softc[4];

static int      pcic_map_io __P((struct pcmcia_link *, u_int, u_int, int));
static int      pcic_map_mem __P((struct pcmcia_link *, caddr_t,
				  u_int, u_int, int));
static int      pcic_map_intr __P((struct pcmcia_link *, int, int));
static int      pcic_service __P((struct pcmcia_link *, int, void *, int));

static struct pcmcia_funcs pcic_funcs = {
	pcic_map_io,
	pcic_map_mem,
	pcic_map_intr,
	pcic_service
};

int             pcicprobe __P((struct device *, void *, void *));
void            pcicattach __P((struct device  *, struct device *, void *));

extern struct pcmciabus_link pcmcia_isa_link;

struct cfdriver pciccd = {
	NULL, "pcic", pcicprobe, pcicattach, DV_DULL, sizeof(struct pcic_softc)
};


static u_char pcic_rd __P((struct slot *, int));
static void pcic_wr __P((struct slot *, int, int));


static __inline u_char
pcic_rd(slot, reg)
	struct slot    *slot;
	int             reg;
{
	u_char          res;
	if (DEBUG(PCDRW))
		printf("pcic_rd(%x [%x %x]) = ", reg, slot->reg_off,
		       slot->chip->pcic_base);
	outb(slot->chip->pcic_base, slot->reg_off + reg);
	delay(1);
	res = inb(slot->chip->pcic_base + 1);
	if (DEBUG(PCDRW))
		printf("%x\n", res);
	return res;
}

static __inline void
pcic_wr(slot, reg, val)
	struct slot    *slot;
	int             reg, val;
{
	outb(slot->chip->pcic_base, slot->reg_off + reg);
	delay(1);
	outb(slot->chip->pcic_base + 1, val);
	if (DEBUG(PCDRW)) {
		int res;
		delay(1);
		outb(slot->chip->pcic_base, slot->reg_off + reg);
		delay(1);
		res = inb(slot->chip->pcic_base + 1);
		printf("pcic_wr(%x %x) = %x\n", reg, val, res);
	}
}

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
pcicprobe(parent, self, aux)
	struct device	*parent;
	void		*self;
	void            *aux;
{
	struct pcic_softc *pcic = (void *) self;
	struct isa_attach_args *ia = aux;
	struct cfdata *cf = pcic->sc_dev.dv_cfdata;
	u_int           chip_inf = 0;
	int             i;

	pcic->pcic_base = ia->ia_iobase;
	pcic->slot_id = 0; /* XXX */
	bzero(pcic->slot, sizeof(pcic->slot));
	pcic->slot[0].chip = pcic;
	pcic->slot[0].reg_off = (pcic->slot_id & 1) * 0x80;
	pcic->slot[1].chip = pcic;
	pcic->slot[1].reg_off = ((pcic->slot_id & 1) * 0x80) + 0x40;
	chip_inf = pcic_rd(&pcic->slot[0], PCIC_ID_REV);
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
	case PCIC_IBM2:
		pcic->chip_inf = PCMICA_CHIP_IBM_2;
ok:
		ia->ia_msize = 0;
		ia->ia_iosize = 2;
		pcmcia_register(pcic, &pcmcia_isa_link, &pcic_funcs,
				pcic->slot_id);
		return 1;
	default:
		printf("found ID %x at pcic position\n", chip_inf & 0xff);
		break;
	}
	/* reset mappings .... */
	pcic_wr(&pcic->slot[0], PCIC_POWER, pcic->slot[0].pow=PCIC_DISRST);
	pcic_wr(&pcic->slot[1], PCIC_POWER, pcic->slot[1].pow=PCIC_DISRST);

	delay(1000);

	for (i = PCIC_INT_GEN; i < 0x40; i++) {
		pcic_wr(&pcic->slot[0], i, 0);
		pcic_wr(&pcic->slot[1], i, 0);
	}
	delay(10000);
	return 0;
}

int
pcic_intr __P((void *));


void
pcicattach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct pcic_softc *pcic = (void *) self;
	struct isa_attach_args *ia = aux;
	struct slot *slot;
	int i;
	static char    *pcic_names[] = {
		"Intel 82365sl Rev. 0",
		"Intel 82365sl Rev. 1",
		"IBM 82365sl clone Rev. 1",
	"IBM 82365sl clone Rev. 2"};
	printf(": %s slots %d-%d (%x %x)\n", pcic_names[pcic->chip_inf -
	    PCMICA_CHIP_82365_0], pcic->slot_id * 2, pcic->slot_id * 2 + 1,
	       &pcic->slot[0], &pcic->slot[1]);
	/* enable interrupts on events */
	if (ia->ia_irq != IRQUNK)
	    pcic->sc_pcic_irq = ia->ia_irq;
	else
	    pcic->sc_pcic_irq = 0;

	for (i = 0; i < 2; i++) {
	    slot = &pcic->slot[i];
	    slot->irq = pcic->sc_pcic_irq |  PCIC_INTR_ENA;
	    pcic_wr(slot, PCIC_STAT_INT,
		    (pcic->sc_pcic_irq << 4) |PCIC_CDTCH | PCIC_STCH);
	    pcic_wr(&pcic->slot[i], PCIC_INT_GEN, slot->irq);
	    (void) pcic_rd(&pcic->slot[i], PCIC_STAT_CHG);
	}
	if (ia->ia_irq == IRQUNK) {
	    pcic->sc_polltimo = hz/2;
	    timeout((void (*)(void *))pcic_intr, pcic, pcic->sc_polltimo);
	} else {
	    pcic->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE,
					     IPL_NET, pcic_intr, pcic, pcic->sc_dev.dv_xname);
	    pcic->sc_polltimo = 0;
	}
}

#ifdef DDB
int pcic_intr_test(slot)
struct slot *slot;
{
    printf("CSC interrupt state: %x\n", pcic_rd(slot, PCIC_STAT_INT));
    printf("General interrupt state: %x\n", pcic_rd(slot, PCIC_INT_GEN));
}

int pcic_intr_set(slot)
struct slot *slot;
{
    pcic_wr(slot, PCIC_INT_GEN, pcic_rd(slot, PCIC_INT_GEN)|PCIC_INTR_ENA);
    pcic_intr_test(slot);
}
#endif

int
pcic_intr(arg)
void *arg;
{
	struct pcic_softc *pcic = arg;
	u_char statchg, intgen;
	register int i;

	if (pcic->sc_polltimo == 0)
		printf("%s: interrupt:", pcic->sc_dev.dv_xname);
	for (i = 0; i < 2; i++) {
		struct pcmcia_link *link = pcic->slot[i].link;
		statchg = pcic_rd(&pcic->slot[i], PCIC_STAT_CHG);
		if (statchg == 0)
			continue;
		intgen = pcic_rd(&pcic->slot[i], PCIC_INT_GEN);
		if (intgen & PCIC_IOCARD) {
			printf("%s: slot %d iocard status %s%s\n", 
			       pcic->sc_dev.dv_xname, i,
			       statchg & PCIC_STCH ? "statchange " : "",
			       statchg & PCIC_CDTCH ? "cardchange" : "");
		} else {
			printf("%s: slot %d memcard status %x\n",
			       pcic->sc_dev.dv_xname, i, statchg);
		} 
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
	struct slot    *slot = &sc->slot[link->slot & 1];

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
	}

}
static int
pcic_map_mem(link, haddr, start, len, flags)
	struct pcmcia_link *link;
	caddr_t         haddr;
	u_int           start, len;
	int		flags;
{
	struct pcic_softc *sc = link->adapter->adapter_softc;
	struct slot    *slot = &sc->slot[link->slot & 1];
	vm_offset_t     physaddr;

	if (flags & PCMCIA_PHYSICAL_ADDR)
		physaddr = (vm_offset_t) haddr;
	else
		physaddr = pmap_extract(pmap_kernel(), (vm_offset_t) haddr);
	if (DEBUG(PCDMEM))
		printf("pcic_map_mem %x %x %x %x %x\n", haddr, physaddr,
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
			printf("mapmem 2:%x %x %x\n", offs, physaddr + offs,
			       start);

		stop = (u_long) physaddr + len;

		winid = window * 0x8 + 0x10;


		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_LOW,
			(u_long) physaddr & 0xff);
		pcic_wr(slot, winid | PCIC_START | PCIC_ADDR_HIGH,
			(((u_long) physaddr >> 8) & 0x3f) |
		/* PCIC_ZEROWS|/* */
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
	struct slot    *slot = &sc->slot[link->slot & 1];

	if (DEBUG(PCDINTR))
		printf("pcic_map_intr %x %x\n", irq, flags);

	if (flags & PCMCIA_UNMAP) {
		slot->irq &= ~PCIC_INT_MASK;
		slot->irq |= sc->sc_pcic_irq | PCIC_INTR_ENA;
		pcic_wr(slot, PCIC_INT_GEN, slot->irq);
	}
	else {
		if (irq < 2 || irq > 15 || irq == 6 || irq == 8 || irq == 13)
			return EINVAL;
		if(irq==2)
		    irq=9;
		slot->irq = (slot->irq & PCIC_INT_FLAGMASK) |
		    irq | PCIC_INTR_ENA;
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
	struct slot    *slot = &sc->slot[link->slot & 1];

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
			slot->irq |= flags ? PCIC_IOCARD : 0;
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
				pcic_wr(slot, PCIC_INT_GEN, slot->irq = 0);
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
