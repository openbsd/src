/* $NetBSD: podulebus.c,v 1.5 1996/03/27 22:07:26 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * podulebus.c
 *
 * Podule probe and configuration routines
 *
 * Created      : 07/11/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <machine/io.h>
#include <machine/katelib.h>
#include <machine/irqhandler.h>
#include <arm32/podulebus/podulebus.h>

/*
 * Now construct the table of know podules.
 * A podule description array is setup for each manufacturer id
 */
 
struct podule_description podules_0000[] = {
	{ 0x02, "SCSI interface" },
	{ 0x03, "ether 1 interface" },
	{ 0x61, "ether 2 interface" },
	{ 0x00, NULL },
};

struct podule_description podules_0004[] = {
	{ 0x14, "laser direct (Canon LBP-4)" },
	{ 0x00, NULL },
};

struct podule_description podules_0009[] = {
	{ 0x52, "hawk V9 mark2" },
	{ 0xcb, "scanlight Video256" },
	{ 0xcc, "eagle M2" },
	{ 0xce, "lark A16" },
	{ 0x200, "MIDI max" },
	{ 0x00, NULL },
};

struct podule_description podules_0011[] = {
	{ 0xa4, "ether 3 interface" },
	{ 0x00, NULL },
};

struct podule_description podules_001a[] = {
	{ 0x95, "16 bit SCSI interface" },
	{ 0x00, NULL },
};

struct podule_description podules_0021[] = {
	{ 0x58, "16 bit SCSI interface" },
	{ 0x00, NULL },
};

struct podule_description podules_002b[] = {
	{ 0x67, "SCSI interface" },
	{ 0x00, NULL },
};

struct podule_description podules_003a[] = {
	{ 0x3a, "SCSI II interface" },
	{ 0xdd, "CDFS & SLCD expansion card" },
	{ 0x00, NULL },
};

struct podule_description podules_0041[] = {
	{ 0x41, "16 bit SCSI interface" },
	{ 0x00, NULL },
};
  
struct podule_description podules_0042[] = {
	{ 0xea, "PC card" },
	{ 0x00, NULL },
};
  
struct podule_description podules_0046[] = {
	{ 0xec, "etherlan 600 network slot interface" },
	{ 0x00, NULL },
};

struct podule_description podules_0050[] = {
	{ 0x00, "BriniPort intelligent I/O interface" },
	{ 0xdf, "BriniLink transputer link adapter" },
	{ 0x00, NULL },
};

struct podule_description podules_0053[] = {
	{ 0xe4, "ether B network slot interface" },
	{ 0x00, NULL },
};

struct podule_description podules_005b[] = {
	{ 0x107, "SCSI 1 / SCSI 2 host adapter" },
	{ 0x00, NULL },
};

/* A podule list if setup for all the manufacturers */
  
struct podule_list known_podules[] = {
	{ 0x00, "Acorn",	podules_0000 },
	{ 0x04, "CConcepts",	podules_0004 },	/* Two codes ??? */
	{ 0x09, "CConcepts",	podules_0009 },
	{ 0x11, "Atomwide",	podules_0011 },
	{ 0x1a, "Lingenuity",	podules_001a },
	{ 0x21, "Oak",		podules_0021 },
	{ 0x2b, "Morley",	podules_002b },
	{ 0x3a, "Cumana",	podules_003a },
	{ 0x41, "ARXE",		podules_0041 },
	{ 0x42, "Aleph1",	podules_0042 },
	{ 0x46, "I-Cubed",	podules_0046 },
	{ 0x50, "Brini",	podules_0050 },
	{ 0x53, "ANT",		podules_0053 },
	{ 0x5b, "Power-tec",	podules_005b },
};

/* Array of podule structures, one per possible podule */

podule_t podules[MAX_PODULES + MAX_NETSLOTS];
irqhandler_t poduleirq;
extern u_int actual_mask;
extern irqhandler_t *irqhandlers[NIRQS];

/* Declare prototypes */

void map_section __P((vm_offset_t, vm_offset_t, vm_offset_t));
int poduleirqhandler __P((void));
u_int poduleread __P((u_int /*address*/, int /*offset*/, int /*slottype*/));


/*
 * int podulebusmatch(struct device *parent, void *match, void *aux)
 *
 * Probe for the podule bus. Currently all this does is return 1 to
 * indicate that the podule bus was found.
 */
 
int
podulebusmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}


int
podulebusprint(aux, podulebus)
	void *aux;
	const char *podulebus;
{
	struct podule_attach_args *pa = aux;

	if (pa->pa_podule != NULL) {
		if (pa->pa_podule->slottype == SLOT_POD)
			printf(" [ podule %d ]:", pa->pa_podule_number);
		else if (pa->pa_podule->slottype == SLOT_NET)
			printf(" [ netslot %d ]:", pa->pa_podule_number - MAX_PODULES);
		else
			panic("Invalid slot type");
	}

/* XXXX print flags */
	return (QUIET);
}


void
podulebusscan(parent, match)
	struct device *parent;
	void *match;
{
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct podule_attach_args pa;

	if (cf->cf_fstate == FSTATE_STAR)
		panic("nope cannot handle this");

	pa.pa_podule = NULL;
	pa.pa_podule_number = -1;

	if (cf->cf_loc[0] != -1) {
		pa.pa_podule_number = cf->cf_loc[0];
	}

	if ((*cf->cf_attach->ca_match)(parent, dev, &pa) > 0)
		config_attach(parent, dev, &pa, podulebusprint);
	else
		free(dev, M_DEVBUF);
}


void
dump_podule(podule)
	podule_t *podule;
{
	printf("podule%d: ", podule->podulenum);
	printf("flags0=%02x ", podule->flags0);
	printf("flags1=%02x ", podule->flags1);
	printf("reserved=%02x ", podule->reserved);
	printf("product=%02x ", podule->product);
	printf("manufacturer=%02x ", podule->manufacturer);
	printf("country=%02x ", podule->country);
	printf("irq_addr=%08x ", podule->irq_addr);
	printf("irq_mask=%02x ", podule->irq_mask);
	printf("fiq_addr=%08x ", podule->fiq_addr);
	printf("fiq_mask=%02x ", podule->fiq_mask);
	printf("fast_base=%08x ", podule->fast_base);
	printf("medium_base=%08x ", podule->medium_base);
	printf("slow_base=%08x ", podule->slow_base);
	printf("sync_base=%08x ", podule->sync_base);
	printf("mod_base=%08x ", podule->mod_base);
	printf("easi_base=%08x ", podule->easi_base);
	printf("attached=%d ", podule->attached);
	printf("slottype=%d ", podule->slottype);
	printf("podulenum=%d ", podule->podulenum);
	printf("\n");
}


void
podulechunkdirectory(podule)
	podule_t *podule;
{
	u_int address;
	u_int id;
	u_int size;
	u_int addr;
	int loop;
	
	address = 0x40;

	do {
		id = poduleread(podule->slow_base, address, podule->slottype);
		size = poduleread(podule->slow_base, address + 4, podule->slottype);
		size |= (poduleread(podule->slow_base, address + 8, podule->slottype) << 8);
		size |= (poduleread(podule->slow_base, address + 12, podule->slottype) << 16);
		if (id == 0xf5) {
			addr = poduleread(podule->slow_base, address + 16, podule->slottype);
			addr |= (poduleread(podule->slow_base, address + 20, podule->slottype) << 8);
			addr |= (poduleread(podule->slow_base, address + 24, podule->slottype) << 16);
			addr |= (poduleread(podule->slow_base, address + 28, podule->slottype) << 24);
			if (addr < 0x800) {
				for (loop = 0; loop < size; ++loop)
					printf("%c", poduleread(podule->slow_base, (addr + loop)*4, podule->slottype));
				printf("\n");
			}
		}
		address += 32;
	} while (id != 0 && address < 0x800);
}


void
poduleexamine(podule, dev, slottype)
	podule_t *podule;
	struct device *dev;
	int slottype;
{
	struct podule_list *pod_list;
	struct podule_description *pod_desc;

/* Test to see if the podule is present */

	if ((podule->flags0 & 0x02) == 0x00) {
		podule->slottype = slottype;
		if (slottype == SLOT_NET)
			printf("netslot%d at %s : ", podule->podulenum - MAX_PODULES,
			    dev->dv_xname);
		else
			printf("podule%d  at %s : ", podule->podulenum,
			    dev->dv_xname);

/* Is it Acorn conformant ? */

		if (podule->flags0 & 0x80)
			printf("Non-Acorn conformant expansion card\n");
		else {
			int id;

/* Is it a simple podule ? */

			id = (podule->flags0 >> 3) & 0x0f;
			if (id != 0)
				printf("Simple expansion card <%x>\n", id);
			else {
/* Do we know this manufacturer ? */
				pod_list = known_podules;
				while (pod_list->description) {
					if (pod_list->manufacturer_id == podule->manufacturer)
						break;
					++pod_list;
				}
				if (!pod_list->description)
					printf("man=%04x   : ", podule->manufacturer);
				else
					printf("%10s : ", pod_list->description);

/* Do we know this product ? */

				pod_desc = pod_list->products;
				while (pod_desc->description) {
					if (pod_desc->product_id == podule->product)
						break;
					++pod_desc;
				}
				if (!pod_desc->description)
					printf("prod=%04x\n", podule->product);
				else
					printf("%s\n", pod_desc->description);

				if (pod_desc->description == NULL
				    && podule->flags1 & PODULE_FLAGS_CD)
					podulechunkdirectory(podule);
			}
		}
	}
}


u_int
poduleread(address, offset, slottype)
	u_int address;
	int offset;
	int slottype;
{
	static u_int netslotoffset;

	if (slottype == SLOT_NET) {
		offset = offset >> 2;
		if (offset < netslotoffset) {
			WriteWord(address, 0);
			netslotoffset = 0;
		}
		while (netslotoffset < offset) {
			slottype = ReadWord(address);
			++netslotoffset;
		}
		++netslotoffset;
		return(ReadByte(address));
	}
	return(ReadByte(address + offset));
}


void
podulescan(dev)
	struct device *dev;
{
	int loop;
	podule_t *podule;
	u_char *address;
	u_int offset = 0;

/* Loop round all the podules */

	for (loop = 0; loop < MAX_PODULES; ++loop, offset += SIMPLE_PODULE_SIZE) {
		if (loop == 4) offset += PODULE_GAP;
		address = ((u_char *)SLOW_PODULE_BASE) + offset;
        
		podule = &podules[loop];

/* Get information from the podule header */

		podule->flags0 = address[0];
		podule->flags1 = address[4];
		podule->reserved = address[8];
		podule->product = address[12] + (address[16] << 8);
		podule->manufacturer = address[20] + (address[24] << 8);
		podule->country = address[28];
		if (podule->flags1 & PODULE_FLAGS_IS) {
			podule->irq_addr = address[52] + (address[56] << 8) + (address[60] << 16);
			podule->irq_mask = address[48];
			podule->fiq_addr = address[36] + (address[40] << 8) + (address[44] << 16);
			podule->fiq_mask = address[32];
		} else {
			podule->irq_addr = 0;
			podule->irq_mask = 0;
			podule->fiq_addr = 0;
			podule->fiq_mask = 0;
		}
		podule->fast_base = FAST_PODULE_BASE + offset;
		podule->medium_base = MEDIUM_PODULE_BASE + offset;
		podule->slow_base = SLOW_PODULE_BASE + offset;
		podule->sync_base = SYNC_PODULE_BASE + offset;
		podule->mod_base = MOD_PODULE_BASE + offset;
		podule->easi_base = EASI_BASE + loop * EASI_SIZE;
		podule->attached = 0;
		podule->slottype = SLOT_NONE;
		podule->podulenum = loop;

		poduleexamine(podule, dev, SLOT_POD);
	}
}


void
netslotscan(dev)
	struct device *dev;
{
	podule_t *podule;
	volatile u_char *address;

/* Only one netslot atm */

/* Reset the address counter */

	WriteByte(NETSLOT_BASE, 0x00);

	address = (u_char *)NETSLOT_BASE;

	podule = &podules[MAX_PODULES];

/* Get information from the podule header */

	podule->flags0 = *address;
	podule->flags1 = *address;
	podule->reserved = *address;
	podule->product = *address + (*address << 8);
	podule->manufacturer = *address + (*address << 8);
	podule->country = *address;
	if (podule->flags1 & PODULE_FLAGS_IS) {
		podule->irq_mask = *address;
		podule->irq_addr = *address + (*address << 8) + (*address << 16);
		podule->fiq_mask = *address;
		podule->fiq_addr = *address + (*address << 8) + (*address << 16);
	} else {
		podule->irq_addr = 0;
		podule->irq_mask = 0;
		podule->fiq_addr = 0;
		podule->fiq_mask = 0;
	}
	podule->fast_base = NETSLOT_BASE;
	podule->medium_base = NETSLOT_BASE;
	podule->slow_base = NETSLOT_BASE;
	podule->sync_base = NETSLOT_BASE;
	podule->mod_base = NETSLOT_BASE;
	podule->easi_base = 0;
	podule->attached = 0;
	podule->slottype = SLOT_NONE;
	podule->podulenum = MAX_PODULES;

	poduleexamine(podule, dev, SLOT_NET);
}


/*
 * void podulebusattach(struct device *parent, struct device *dev, void *aux)
 *
 * Attach podulebus.
 * This probes all the podules and sets up the podules array with
 * information found in the podule headers.
 * After identifing all the podules, all the children of the podulebus
 * are probed and attached.
 */
  
void
podulebusattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	int loop;
    
	printf("\n");

/* Ok we need to map in the podulebus */

/* Map the FAST and SYNC simple podules */

	map_section(PAGE_DIRS_BASE, SYNC_PODULE_BASE & 0xfff00000,
	    SYNC_PODULE_HW_BASE & 0xfff00000);

/* Now map the EASI space */

	for (loop = 0; loop < MAX_PODULES; ++loop) {
		int loop1;
        
		for (loop1 = loop * EASI_SIZE; loop1 < ((loop + 1) * EASI_SIZE); loop1 += (1 << 20))
		map_section(PAGE_DIRS_BASE, EASI_BASE + loop1, EASI_HW_BASE + loop1);
	}

/*
 * The MEDIUM and SLOW simple podules and the module space will have been
 * mapped when the IOMD and COMBO we mapped in for the RPC
 */

/* Install an podule IRQ handler */

	poduleirq.ih_func = poduleirqhandler;
	poduleirq.ih_arg = NULL;
	poduleirq.ih_level = IPL_NONE;
	poduleirq.ih_name = "podulebus";

	if (irq_claim(IRQ_PODULE, &poduleirq))
		panic("Cannot claim IRQ for podulebus%d", IRQ_PODULE, parent->dv_unit);

/* Find out what hardware is bolted on */

	podulescan(self); 
	netslotscan(self);

/* Look for drivers */

	config_scan(podulebusscan, self);
}


/*
 * int podule_irqhandler(void *arg)
 *
 * text irq handler to service expansion card IRQ's
 *
 * There is currently a problem here.
 * The spl_mask may mask out certain expansion card IRQ's e.g. SCSI
 * but allow others e.g. Ethernet.
 */

int
poduleirqhandler()
{
	int loop;
	irqhandler_t *handler;

	printf("eek ! Unknown podule IRQ received - Blocking all podule interrupts\n");
	disable_irq(IRQ_PODULE);
	return(1);

/* Loop round the expansion card handlers */

	for (loop = IRQ_EXPCARD0; loop <= IRQ_EXPCARD7; ++loop) {

/* Is the IRQ currently allowable */

		if (actual_mask & (1 << loop)) {
			handler = irqhandlers[loop];
        
			if (handler && handler->ih_irqmask) {
				if ((*handler->ih_irqmask) & handler->ih_irqbit)
					handler->ih_func(handler->ih_arg);
			}
		}
	}
	return(1);      
}

struct cfattach podulebus_ca = {
	sizeof(struct device), podulebusmatch, podulebusattach
};

struct cfdriver podulebus_cd = {
	NULL, "podulebus", DV_DULL, 1
};


/* Useful functions that drivers may share */

/*
 * Search the podule list for the specified manufacturer and product.
 * Return the podule number if the podule is not already attach and
 * the podule was in the required slot.
 * A required slot of -1 means any slot.
 */

int
findpodule(manufacturer, product, required_slot)
	int manufacturer;
	int product;
	int required_slot;
{
	int loop;

	for (loop = 0; loop < MAX_PODULES+MAX_NETSLOTS; ++loop) {
		if (podules[loop].slottype != SLOT_NONE
		    && !podules[loop].attached
		    && podules[loop].manufacturer == manufacturer
		    && podules[loop].product == product
		    && (required_slot == -1 || required_slot == loop))
			return(loop);
	}
      
	return(-1);
}

/* End of podulebus.c */
