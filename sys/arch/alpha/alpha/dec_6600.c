/* $OpenBSD: dec_6600.c,v 1.10 2008/07/16 20:03:20 miod Exp $ */
/* $NetBSD: dec_6600.c,v 1.7 2000/06/20 03:48:54 matt Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <dev/cons.h>

#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/cpuconf.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/tsreg.h>
#include <alpha/pci/tsvar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <dev/ata/atavar.h>

#include "pckbd.h"

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif

#define	DR_VERBOSE(f) while (0)

static int comcnrate __attribute__((unused)) = CONSPEED;

void dec_6600_init(void);
static void dec_6600_cons_init(void);
static void dec_6600_device_register(struct device *, void *);

void
dec_6600_init()
{

	platform.family = "6600";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		/* XXX Don't know the system variations, yet. */
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "tsc";
	platform.cons_init = dec_6600_cons_init;
	platform.device_register = dec_6600_device_register;
	STQP(TS_C_DIM0) = 0UL;
	STQP(TS_C_DIM1) = 0UL;
}

static void
dec_6600_cons_init()
{
	struct ctb *ctb;
	u_int64_t ctbslot;
	struct tsp_config *tsp;

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);
	ctbslot = ctb->ctb_turboslot;

	/* Console hose defaults to hose 0. */
	tsp_console_hose = 0;

	tsp = tsp_init(0, tsp_console_hose);

	switch (ctb->ctb_term_type) {
	case CTB_PRINTERPORT: 
		/* serial console ... */
		/* XXX */
		{
			/*
			 * Delay to allow PROM putchars to complete.
			 * FIFO depth * character time,
			 * character time = (1000000 / (defaultrate / 10))
			 */
			DELAY(160000000 / comcnrate);

			if(comcnattach(&tsp->pc_iot, 0x3f8, comcnrate,
			    COM_FREQ,
			    (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
				panic("can't init serial console");

			break;
		}

	case CTB_GRAPHICS:
#if NPCKBD > 0
		/* display console ... */
		/* XXX */
		(void) pckbc_cnattach(&tsp->pc_iot, IO_KBD, KBCMDP,
		    PCKBC_KBD_SLOT, 0);

		if (CTB_TURBOSLOT_TYPE(ctbslot) ==
		    CTB_TURBOSLOT_TYPE_ISA)
			isa_display_console(&tsp->pc_iot, &tsp->pc_memt);
		else {
			/* The display PCI might be different */
			tsp_console_hose = CTB_TURBOSLOT_HOSE(ctbslot);
			tsp = tsp_init(0, tsp_console_hose);
			pci_display_console(&tsp->pc_iot, &tsp->pc_memt,
			    &tsp->pc_pc, CTB_TURBOSLOT_BUS(ctbslot),
			    CTB_TURBOSLOT_SLOT(ctbslot), 0);
		}
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb_term_type = 0x%lx ctb_turboslot = 0x%lx"
		    " hose = %ld\n", ctb->ctb_term_type, ctbslot,
		    CTB_TURBOSLOT_HOSE(ctbslot));

		panic("consinit: unknown console type %ld",
		    ctb->ctb_term_type);
	}
}

static void
dec_6600_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, diskboot, netboot;
	static struct device *primarydev, *pcidev, *ctrlrdev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	if (found)
		return;

	if (!initted) {
		diskboot = (strncasecmp(b->protocol, "SCSI", 4) == 0) ||
		    (strncasecmp(b->protocol, "IDE", 3) == 0);
		netboot = (strncasecmp(b->protocol, "BOOTP", 5) == 0) ||
		    (strncasecmp(b->protocol, "MOP", 3) == 0);
		DR_VERBOSE(printf("diskboot = %d, netboot = %d\n", diskboot,
		    netboot));
		initted = 1;
	}

	if (primarydev == NULL) {
		if (strcmp(cd->cd_name, "tsp"))
			return;
		else {
			struct tsp_attach_args *tsp = aux;

			if (b->bus != tsp->tsp_slot)
				return;
			primarydev = dev;
			DR_VERBOSE(printf("\nprimarydev = %s\n",
			    dev->dv_xname));
			return;
		}
	}

	if (pcidev == NULL) {
		if (strcmp(cd->cd_name, "pci"))
			return;
		/*
		 * Try to find primarydev anywhere in the ancestry.  This is
		 * necessary if the PCI bus is hidden behind a bridge.
		 */
		while (parent) {
			if (parent == primarydev)
				break;
			parent = parent->dv_parent;
		}
		if (!parent)
			return;
		else {
			struct pcibus_attach_args *pba = aux;

			if ((b->slot / 1000) != pba->pba_bus)
				return;
	
			pcidev = dev;
			DR_VERBOSE(printf("\npcidev = %s\n", dev->dv_xname));
			return;
		}
	}

	if (ctrlrdev == NULL) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;
			int slot;

			slot = pa->pa_bus * 1000 + pa->pa_function * 100 +
			    pa->pa_device;
			if (b->slot != slot)
				return;
	
			if (netboot) {
				booted_device = dev;
				DR_VERBOSE(printf("\nbooted_device = %s\n",
				    dev->dv_xname));
				found = 1;
			} else {
				ctrlrdev = dev;
				DR_VERBOSE(printf("\nctrlrdev = %s\n",
				    dev->dv_xname));
			}
			return;
		}
	}

	if (!diskboot)
		return;

	if (!strcmp(cd->cd_name, "sd") || !strcmp(cd->cd_name, "st") ||
	    !strcmp(cd->cd_name, "cd")) {
		struct scsi_attach_args *sa = aux;
		struct scsi_link *periph = sa->sa_sc_link;
		int unit;

		if (parent->dv_parent != ctrlrdev)
			return;

		unit = periph->target * 100 + periph->lun;
		if (b->unit != unit)
                        return;

		/* we've found it! */
		booted_device = dev;
		DR_VERBOSE(printf("\nbooted_device = %s\n", dev->dv_xname));
		found = 1;
	}

	/*
	 * Support to boot from IDE drives.
	 */
	if (!strcmp(cd->cd_name, "wd")) {
		struct ata_atapi_attach *aa_link = aux;

		if ((strncmp("pciide", parent->dv_xname, 6) != 0))
			return;
		if (parent != ctrlrdev)
			return;

		DR_VERBOSE(printf("\nAtapi info: drive: %d, channel %d\n",
		    aa_link->aa_drv_data->drive, aa_link->aa_channel));
		DR_VERBOSE(printf("Bootdev info: unit: %d, channel: %d\n",
		    b->unit, b->channel));
		if (b->unit != aa_link->aa_drv_data->drive ||
		    b->channel != aa_link->aa_channel)
			return;

		/* we've found it! */
		booted_device = dev;
		DR_VERBOSE(printf("booted_device = %s\n", dev->dv_xname));
		found = 1;
	}
}
