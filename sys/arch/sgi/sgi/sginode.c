/*	$OpenBSD: sginode.c,v 1.25 2011/04/21 18:16:57 miod Exp $	*/
/*
 * Copyright (c) 2008, 2009, 2011 Miodrag Vallat.
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
/*
 * Copyright (c) 2004 Opsycon AB.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/param.h>
#include <machine/autoconf.h>
#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <machine/mnode.h>
#include <sgi/xbow/hub.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

void	kl_add_memory_ip27(int16_t, int16_t *, unsigned int);
void	kl_add_memory_ip35(int16_t, int16_t *, unsigned int);

void	kl_dma_select(paddr_t, psize_t);

int	kl_first_pass_board(lboard_t *, void *);
int	kl_first_pass_comp(klinfo_t *, void *);

#ifdef DEBUG
#define	DB_PRF(x)	bios_printf x
#else
#define	DB_PRF(x)
#endif

/* widget number of the XBow `hub', for each node */
int	kl_hub_widget[GDA_MAXNODES];

int	kl_n_mode = 0;
u_int	kl_n_shift = 32;
klinfo_t *kl_glass_console = NULL;

void
kl_init(int ip35)
{
	kl_config_hdr_t *cfghdr;
	uint64_t val;
	uint64_t nibase = ip35 ? HUBNIBASE_IP35 : HUBNIBASE_IP27;
	size_t gsz;

	/* will be recomputed when processing memory information */
	physmem = 0;

	cfghdr = IP27_KLCONFIG_HDR(0);
	DB_PRF(("config @%p\n", cfghdr));
	DB_PRF(("magic %p version %x\n", cfghdr->magic, cfghdr->version));
	DB_PRF(("console %p baud %d\n", cfghdr->cons_info.uart_base,
	    cfghdr->cons_info.baud));

	val = IP27_LHUB_L(nibase | HUBNI_STATUS);
	kl_n_mode = (val & NI_MORENODES) != 0;
	kl_n_shift = (ip35 ? 33 : 32) - kl_n_mode;
        bios_printf("Machine is in %c mode.\n", kl_n_mode + 'M');

	val = IP27_LHUB_L(HUBPI_REGION_PRESENT);
        DB_PRF(("Region present %p.\n", val));
	val = IP27_LHUB_L(HUBPI_CALIAS_SIZE);
        DB_PRF(("Calias size %p.\n", val));

	/*
	 * Get a grip on the global data area, and figure out how many
	 * theoretical nodes are available.
	 */

	gda = IP27_GDA(0);
	gsz = IP27_GDA_SIZE(0);
	if (gda->magic != GDA_MAGIC || gda->ver < 2) {
		masternasid = 0;
		maxnodes = 0;
	} else {
		masternasid = gda->masternasid;
		maxnodes = (gsz - offsetof(gda_t, nasid)) / sizeof(int16_t);
		if (maxnodes > GDA_MAXNODES)
			maxnodes = GDA_MAXNODES;
		/* in M mode, there can't be more than 64 nodes anyway */
		if (kl_n_mode == 0 && maxnodes > 64)
			maxnodes = 64;
	}

	/*
	 * Scan all nodes configurations to find out CPU and memory
	 * information, starting with the master node.
	 */

	kl_scan_all_nodes(KLBRD_ANY, kl_first_pass_board, &ip35);
}

/*
 * Callback routine for the initial enumeration (boards).
 */
int
kl_first_pass_board(lboard_t *boardinfo, void *arg)
{
	DB_PRF(("%cboard type %x slot %x nasid %x nic %p ncomp %d\n",
	    boardinfo->struct_type & LBOARD ? 'l' : 'r',
	    boardinfo->brd_type, boardinfo->brd_slot,
	    boardinfo->brd_nasid, boardinfo->brd_nic,
	    boardinfo->brd_numcompts));

	/*
	 * Assume the worst case of a restricted XBow, as found on
	 * Origin 200 systems. This value will be overridden should a
	 * full-blown XBow be found during component enumeration.
	 *
	 * On Origin 200, widget 0 reports itself as a Bridge, and
	 * interrupt widget is hardwired to #a (which is another facet
	 * of the bridge).
	 */
	kl_hub_widget[boardinfo->brd_nasid] = 0x0a;

	kl_scan_board(boardinfo, KLSTRUCT_ANY, kl_first_pass_comp, arg);

	return 0;
}

#ifdef DEBUG
static const char *klstruct_names[] = {
	"unknown component",
	"cpu",
	"hub",
	"memory",
	"xbow",
	"bridge",
	"ioc3",
	"pci",
	"vme",
	"router",
	"graphics",
	"scsi",
	"fddi",
	"mio",
	"disk",
	"tape",
	"cdrom",
	"hub uart",
	"ioc3 Ethernet",
	"ioc3 uart",
	"component type 20",
	"ioc3 keyboard",
	"rad",
	"hub tty",
	"ioc3 tty",
	"fc",
	"module serialnumber",
	"ioc3 mouse",
	"tpu",
	"gsn main board",
	"gsn aux board",
	"xthd",
	"QLogic fc",
	"firewire",
	"usb",
	"usb keyboard",
	"usb mouse",
	"dual scsi",
	"PE brick",
	"gigabit Ethernet",
	"ide",
	"ioc4",
	"ioc4 uart",
	"ioc4 tty",
	"ioc4 keyboard",
	"ioc4 mouse",
	"ioc4 ATA",
	"pci graphics"
};
#endif

/*
 * Callback routine for the initial enumeration (components).
 * We are interested in cpu and memory information only, but display a few
 * other things if option DEBUG.
 */
int
kl_first_pass_comp(klinfo_t *comp, void *arg)
{
	int ip35 = *(int *)arg;
	klcpu_t *cpucomp;
	klmembnk_m_t *memcomp_m;
	arc_config64_t *arc;
#ifdef DEBUG
	klmembnk_n_t *memcomp_n;
	klhub_t *hubcomp;
	klxbow_t *xbowcomp;
	klscsi_t *scsicomp;
	klscctl_t *scsi2comp;
	int i;
#endif

	arc = (arc_config64_t *)comp->arcs_compt;

#ifdef DEBUG
	if (comp->struct_type < nitems(klstruct_names))
		DB_PRF(("\t%s", klstruct_names[comp->struct_type]));
	else
		DB_PRF(("\tcomponent type %d", comp->struct_type));
	DB_PRF((", widget %x physid 0x%02x virtid %d",
	    comp->widid, comp->physid, comp->virtid));
	if (ip35) {
		DB_PRF((" prt %d bus %d", comp->port, comp->pci_bus_num));
		if (comp->pci_multifunc)
			DB_PRF((" pcifn %d", comp->pci_func_num));
	}
	DB_PRF(("\n"));
#endif

	switch (comp->struct_type) {
	case KLSTRUCT_CPU:
		cpucomp = (klcpu_t *)comp;
		DB_PRF(("\t  type %x/%x %dMHz cache %dMB speed %dMHz\n",
		    cpucomp->cpu_prid, cpucomp->cpu_fpirr,
		    cpucomp->cpu_speed, cpucomp->cpu_scachesz,
		    cpucomp->cpu_scachespeed));
		/*
		 * XXX this assumes the first cpu encountered is the boot
		 * XXX cpu.
		 */
		if (bootcpu_hwinfo.clock == 0) {
			bootcpu_hwinfo.c0prid = cpucomp->cpu_prid;
			bootcpu_hwinfo.c1prid = cpucomp->cpu_prid; /* XXX */
			bootcpu_hwinfo.clock = cpucomp->cpu_speed * 1000000;
			bootcpu_hwinfo.tlbsize = 64;
			bootcpu_hwinfo.type = (cpucomp->cpu_prid >> 8) & 0xff;
		} else
			ncpusfound++;
		break;

	case KLSTRUCT_MEMBNK:
		memcomp_m = (klmembnk_m_t *)comp;
#ifdef DEBUG
		memcomp_n = (klmembnk_n_t *)comp;
		DB_PRF(("\t  %dMB, select %x flags %x\n",
		    memcomp_m->membnk_memsz, memcomp_m->membnk_dimm_select,
		    kl_n_mode ?
		      memcomp_n->membnk_attr : memcomp_m->membnk_attr));

		if (kl_n_mode) {
			for (i = 0; i < MD_MEM_BANKS_N; i++) {
				if (memcomp_n->membnk_bnksz[i] == 0)
					continue;
				DB_PRF(("\t\tbank %d %dMB\n",
				    i + 1, memcomp_n->membnk_bnksz[i]));
			}
		} else {
			for (i = 0; i < MD_MEM_BANKS_M; i++) {
				if (memcomp_m->membnk_bnksz[i] == 0)
					continue;
				DB_PRF(("\t\tbank %d %dMB\n",
				    i + 1, memcomp_m->membnk_bnksz[i]));
			}
		}
#endif
		if (ip35)
			kl_add_memory_ip35(comp->nasid,
			    memcomp_m->membnk_bnksz,
			    kl_n_mode ?  MD_MEM_BANKS_N : MD_MEM_BANKS_M);
		else
			kl_add_memory_ip27(comp->nasid,
			    memcomp_m->membnk_bnksz,
			    kl_n_mode ?  MD_MEM_BANKS_N : MD_MEM_BANKS_M);
		break;

	case KLSTRUCT_GFX:
		/*
		 * We rely upon the PROM setting up a fake ARCBios component
		 * for the graphics console, if there is one.
		 * Of course, the ARCBios structure is only available as long
		 * as we do not tear down the PROM TLB, which is why we check
		 * for this as early as possible and remember the console
		 * component (KL struct are not short-lived).
		 */
		if (arc != NULL &&
		    arc->class != 0 && arc->type == arc_DisplayController &&
		    ISSET(arc->flags, ARCBIOS_DEVFLAGS_CONSOLE_OUTPUT)) {
			DB_PRF(("\t  (console device)\n"));
			/* paranoia */
			if (comp->widid >= WIDGET_MIN &&
			    comp->widid <= WIDGET_MAX)
				kl_glass_console = comp;
		}
		break;

#ifdef DEBUG
	case KLSTRUCT_HUB:
		hubcomp = (klhub_t *)comp;
		DB_PRF(("\t  port %d flag %d speed %dMHz\n",
		    hubcomp->hub_port.port_nasid, hubcomp->hub_port.port_flag,
		    hubcomp->hub_speed / 1000000));
		break;

	case KLSTRUCT_XBOW:
		xbowcomp = (klxbow_t *)comp;
		DB_PRF(("\t hub master link %d\n",
		    xbowcomp->xbow_hub_master_link));
		kl_hub_widget[comp->nasid] = xbowcomp->xbow_hub_master_link;
		for (i = 0; i < MAX_XBOW_LINKS; i++) {
			if (!ISSET(xbowcomp->xbow_port_info[i].port_flag,
			    XBOW_PORT_ENABLE))
				continue;
			DB_PRF(("\t\twidget %d nasid %d flg %u\n", 8 + i,
			    xbowcomp->xbow_port_info[i].port_nasid,
			    xbowcomp->xbow_port_info[i].port_flag));
		}
		break;

	case KLSTRUCT_SCSI2:
		scsi2comp = (klscctl_t *)comp;
		for (i = 0; i < scsi2comp->scsi_buscnt; i++) {
			scsicomp = (klscsi_t *)scsi2comp->scsi_bus[i];
			DB_PRF(("\t\tbus %d, physid 0x%02x virtid %d,"
			    " specific %ld, numdevs %d\n",
			    i, scsicomp->scsi_info.physid,
			    scsicomp->scsi_info.virtid,
			    scsicomp->scsi_specific,
			    scsicomp->scsi_numdevs));
		}
		break;
#endif
	}

#ifdef DEBUG
	if (arc != NULL) {
		DB_PRF(("\t[ARCBios component:"
		    " class %d type %d flags %02x key 0x%lx",
		    arc->class, arc->type, arc->flags, arc->key));
		if (arc->id_len != 0)
			DB_PRF((" %.*s]\n",
			    (int)arc->id_len, (const char *)arc->id));
		else
			DB_PRF((" (no name)]\n"));
	}
#endif

	return 0;
}

/*
 * Enumerate the boards of all nodes, and invoke a callback for those
 * matching the given class.
 */
int
kl_scan_all_nodes(uint cls, int (*cb)(lboard_t *, void *), void *cbarg)
{
	uint node;

	if (kl_scan_node(masternasid, cls, cb, cbarg) != 0)
		return 1;
	for (node = 0; node < maxnodes; node++) {
		if (gda->nasid[node] < 0)
			continue;
		if (gda->nasid[node] == masternasid)
			continue;
		if (kl_scan_node(gda->nasid[node], cls, cb, cbarg) != 0)
			return 1;
	}

	return 0;
}

/*
 * Enumerate the boards of a node, and invoke a callback for those matching
 * the given class.
 */
int
kl_scan_node(int nasid, uint clss, int (*cb)(lboard_t *, void *), void *cbarg)
{
	lboard_t *boardinfo;

	for (boardinfo = IP27_KLFIRST_BOARD(nasid); boardinfo != NULL;
	    boardinfo = IP27_KLNEXT_BOARD(nasid, boardinfo)) {
		if (clss == KLBRD_ANY ||
		    (boardinfo->brd_type & IP27_BC_MASK) == clss) {
			if ((*cb)(boardinfo, cbarg) != 0)
				return 1;
		}
		if (boardinfo->brd_next == 0)
			break;
	}

	return 0;
}

/*
 * Enumerate the components of a board, and invoke a callback for those
 * matching the given type.
 */
int
kl_scan_board(lboard_t *boardinfo, uint type, int (*cb)(klinfo_t *, void *),
    void *cbarg)
{
	klinfo_t *comp;
	int i;

	if (!ISSET(boardinfo->struct_type, LBOARD))
		return 0;

	for (i = 0; i < boardinfo->brd_numcompts; i++) {
		comp = IP27_UNCAC_ADDR(klinfo_t *, boardinfo->brd_nasid,
		    boardinfo->brd_compts[i]);

		if (!ISSET(comp->flags, KLINFO_ENABLED) ||
		    ISSET(comp->flags, KLINFO_FAILED))
			continue;

		if (type != KLSTRUCT_ANY && comp->struct_type != type)
			continue;

		if ((*cb)(comp, cbarg) != 0)
			return 1;
	}

	return 0;
}

/*
 * Return the console device information.
 */
console_t *
kl_get_console()
{
	kl_config_hdr_t *cfghdr = IP27_KLCONFIG_HDR(0);

	return &cfghdr->cons_info;
}

/*
 * Pick the given window as the DMAable memory window, if it contains more
 * memory than our former choice. Expects the base address to be aligned on
 * a 2GB boundary.
 */
void
kl_dma_select(paddr_t base, psize_t size)
{
	static psize_t maxsize = 0;

	if (size > maxsize) {
		maxsize = size;
		dma_constraint.ucr_low = base;
		dma_constraint.ucr_high = base + ((1UL << 31) - 1);
	}
}

/*
 * Process memory bank information.
 * There are two different routines, because IP27 and IP35 do not
 * layout memory the same way.
 *
 * This is the opportunity to pick the largest populated 2GB window on a
 * 2GB boundary, as our DMA window.
 */

/*
 * On IP27, access to each DIMM is interleaved, which cause it to map to
 * four banks on 128MB boundaries.
 * DIMMs of 128MB or smaller map everything in the first bank, though --
 * interleaving would be horribly non-optimal.
 */
void
kl_add_memory_ip27(int16_t nasid, int16_t *sizes, unsigned int cnt)
{
	paddr_t basepa;
	uint64_t fp, lp, np;
	unsigned int seg, nmeg;
	paddr_t twogseg;
	psize_t twogcnt;

	basepa = (paddr_t)nasid << kl_n_shift;

	/* note we know kl_n_shift > 31, so 2GB windows can not span nodes */
	twogseg = basepa >> 31;
	twogcnt = 0;

	while (cnt-- != 0) {
		nmeg = *sizes++;
		for (seg = 0; seg < 4; basepa += (1 << 27), seg++) {
			/* did we cross a 2GB boundary? */
			if ((basepa >> 31) != twogseg) {
				kl_dma_select(twogseg << 31, twogcnt);
				twogseg = basepa >> 31;
				twogcnt = 0;
			}

			if (nmeg <= 128)
				np = seg == 0 ? nmeg : 0;
			else
				np = nmeg / 4;
			if (np == 0)
				continue;

			DB_PRF(("IP27 memory from %p to %p (%u MB)\n",
			    basepa, basepa + (np << 20), np));

			np = atop(np << 20);	/* MB to pages */
			fp = atop(basepa);
			lp = fp + np;

			/*
			 * We do not manage the first 32MB, so skip them here
			 * if necessary.
			 */
			if (fp < atop(32 << 20)) {
				fp = atop(32 << 20);
				if (fp >= lp)
					continue;
				np = lp - fp;
				physmem += atop(32 << 20);
			}

			twogcnt += ptoa(lp - fp);

			if (memrange_register(fp, lp,
			    ~(atop(1UL << kl_n_shift) - 1),
			    VM_FREELIST_DEFAULT) != 0) {
				/*
				 * We could hijack the smallest segment here.
				 * But is it really worth doing?
				 */
				bios_printf("%u MB of memory could not be "
				    "managed, increase MAXMEMSEGS\n",
				    ptoa(np) >> 20);
			}
		}
	}

	kl_dma_select(twogseg << 31, twogcnt);
}

/*
 * On IP35, the smallest memory DIMMs are 256MB, and the largest is 1GB.
 * Memory is reported at 1GB intervals.
 */
void
kl_add_memory_ip35(int16_t nasid, int16_t *sizes, unsigned int cnt)
{
	paddr_t basepa;
	uint64_t fp, lp, np;
	paddr_t twogseg;
	psize_t twogcnt;

	basepa = (paddr_t)nasid << kl_n_shift;

	/* note we know kl_n_shift > 31, so 2GB windows can not span nodes */
	twogseg = basepa >> 31;
	twogcnt = 0;

	while (cnt-- != 0) {
		/* did we cross a 2GB boundary? */
		if ((basepa >> 31) != twogseg) {
			kl_dma_select(twogseg << 31, twogcnt);
			twogseg = basepa >> 31;
			twogcnt = 0;
		}

		np = *sizes++;
		if (np != 0) {
			DB_PRF(("IP35 memory from %p to %p (%u MB)\n",
			    basepa, basepa + (np << 20), np));

			fp = atop(basepa);
			np = atop(np << 20);	/* MB to pages */
			lp = fp + np;

			/*
			 * We do not manage the first 64MB, so skip them here
			 * if necessary.
			 */
			if (fp < atop(64 << 20)) {
				fp = atop(64 << 20);
				if (fp >= lp)
					continue;
				np = lp - fp;
				physmem += atop(64 << 20);
			}

			twogcnt += ptoa(lp - fp);

			if (memrange_register(fp, lp,
			    ~(atop(1UL << kl_n_shift) - 1),
			    VM_FREELIST_DEFAULT) != 0) {
				/*
				 * We could hijack the smallest segment here.
				 * But is it really worth doing?
				 */
				bios_printf("%u MB of memory could not be "
				    "managed, increase MAXMEMSEGS\n",
				    ptoa(np) >> 20);
			}
		}
		basepa += 1UL << 30;	/* 1 GB */
	}

	kl_dma_select(twogseg << 31, twogcnt);
}

/*
 * Extract unique device location from a klinfo structure.
 */
void
kl_get_location(klinfo_t *cmp, struct sgi_device_location *sdl)
{
	uint32_t wid;
	int device = cmp->physid;

	/*
	 * If the widget is actually a PIC, we need to compensate
	 * for PCI device numbering.
	 */
	if (xbow_widget_id(cmp->nasid, cmp->widid, &wid) == 0) {
		if (WIDGET_ID_VENDOR(wid) == XBOW_VENDOR_SGI3 &&
		    WIDGET_ID_PRODUCT(wid) == XBOW_PRODUCT_SGI3_PIC)
			device--;
	}

	sdl->nasid = cmp->nasid;
	sdl->widget = cmp->widid;
	if (sys_config.system_type == SGI_IP35) {
		/*
		 * IP35: need to be aware of secondary buses on PIC, and
		 * multifunction PCI cards.
		 */
		sdl->bus = cmp->pci_bus_num;
		sdl->device = device;
		if (cmp->pci_multifunc)
			sdl->fn = cmp->pci_func_num;
		else
			sdl->fn = -1;
		sdl->specific = cmp->port;
	} else {
		/*
		 * IP27: secondary buses and multifunction PCI devices are
		 * not recognized.
		 */
		sdl->bus = 0;
		sdl->device = device;
		sdl->fn = -1;
		sdl->specific = 0;
	}
}

/*
 * Similar to the above, but for the input console device, which information
 * does not come from a klinfo structure.
 */
void
kl_get_console_location(console_t *cons, struct sgi_device_location *sdl)
{
	uint32_t wid;
	int device = cons->npci;

	/*
	 * If the widget is actually a PIC, we need to compensate
	 * for PCI device numbering.
	 */
	if (xbow_widget_id(cons->nasid, cons->wid, &wid) == 0) {
		if (WIDGET_ID_VENDOR(wid) == XBOW_VENDOR_SGI3 &&
		    WIDGET_ID_PRODUCT(wid) == XBOW_PRODUCT_SGI3_PIC)
			device--;
	}

	sdl->nasid = cons->nasid;
	sdl->widget = cons->wid;
	sdl->fn = -1;
	sdl->specific = cons->type;

	/*
	 * This is a disgusting hack. The console structure does not
	 * contain a precise PCI device identification, and will also
	 * not point to the relevant klinfo structure.
	 *
	 * We assume that if the console `type' is not the IOC3 PCI
	 * identifier, then it is an IOC4 device connected to a PIC
	 * bus, therefore we can compute proper PCI location from the
	 * `npci' field.
	 */

	if (cons->type == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3) ||
	    sys_config.system_type != SGI_IP35) {
		sdl->bus = 0;
		sdl->device = device;
	} else {
		sdl->bus = cons->npci & KLINFO_PHYSID_PIC_BUS1 ? 1 : 0;
		sdl->device = device & KLINFO_PHYSID_WIDGET_MASK;
	}
}
