/*	$OpenBSD: eisa.c,v 1.2 1996/04/21 22:20:23 deraadt Exp $	*/
/*	$NetBSD: eisa.c,v 1.11 1996/04/09 22:46:11 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * EISA Bus device
 *
 * Makes sure an EISA bus is present, and finds and attaches devices
 * living on it.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

int	eisamatch __P((struct device *, void *, void *));
void	eisaattach __P((struct device *, struct device *, void *));

struct cfattach eisa_ca = {
	sizeof(struct device), eisamatch, eisaattach
};

struct cfdriver eisa_cd = {
	NULL, "eisa", DV_DULL
};

int	eisasubmatch __P((struct device *, void *, void *));
int	eisaprint __P((void *, char *));
void	eisa_devinfo __P((const char *, char *));

int
eisamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct eisabus_attach_args *eba = aux;

	if (strcmp(eba->eba_busname, cf->cf_driver->cd_name))
		return (0);

	/* XXX check other indicators */

	return (1);
}

int
eisaprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct eisa_attach_args *ea = aux;
	char devinfo[256]; 

	if (pnp) {
		eisa_devinfo(ea->ea_idstring, devinfo);
		printf("%s at %s", devinfo, pnp);
	}
	printf(" slot %d", ea->ea_slot);
	return (UNCONF);
}

int
eisasubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct eisa_attach_args *ea = aux;

	if (cf->eisacf_slot != EISA_UNKNOWN_SLOT &&
	    cf->eisacf_slot != ea->ea_slot)
		return 0;
	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

void
eisaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eisabus_attach_args *eba = aux;
	bus_chipset_tag_t bc;
	eisa_chipset_tag_t ec;
	int slot, maxnslots;

	eisa_attach_hook(parent, self, eba);
	printf("\n");

	bc = eba->eba_bc;
	ec = eba->eba_ec;

	/*
	 * Search for and attach subdevices.
	 *
	 * Slot 0 is the "motherboard" slot, and the code attaching
	 * the EISA bus should have already attached an ISA bus there.
	 */
	maxnslots = eisa_maxslots(ec);
	for (slot = 1; slot < maxnslots; slot++) {
		struct eisa_attach_args ea;
		u_int slotaddr;
		bus_io_handle_t slotioh;
		int i;

		ea.ea_bc = bc;
		ea.ea_ec = ec;
		ea.ea_slot = slot;
		slotaddr = EISA_SLOT_ADDR(slot);

		/*
		 * Get a mapping for the whole slot-specific address
		 * space.  If we can't, assume nothing's there but warn
		 * about it.
		 */
		if (bus_io_map(bc, slotaddr, EISA_SLOT_SIZE, &slotioh)) {
			printf("%s: can't map I/O space for slot %d\n",
			    self->dv_xname, slot);
			continue;
		}

		/* Get the vendor ID bytes */
		for (i = 0; i < EISA_NVIDREGS; i++)
			ea.ea_vid[i] = bus_io_read_1(bc, slotioh,
			    EISA_SLOTOFF_VID + i);

		/* Check for device existence */
		if (EISA_VENDID_NODEV(ea.ea_vid)) {
#if 0
			printf("no device at %s slot %d\n", self->dv_xname,
			    slot);
			printf("\t(0x%x, 0x%x)\n", ea.ea_vid[0],
			    ea.ea_vid[1]);
#endif
			bus_io_unmap(bc, slotioh, EISA_SLOT_SIZE);
			continue;
		}

		/* And check that the firmware didn't biff something badly */
		if (EISA_VENDID_IDDELAY(ea.ea_vid)) {
			printf("%s slot %d not configured by BIOS?\n",
			    self->dv_xname, slot);
			bus_io_unmap(bc, slotioh, EISA_SLOT_SIZE);
			continue;
		}

		/* Get the product ID bytes */
		for (i = 0; i < EISA_NPIDREGS; i++)
			ea.ea_pid[i] = bus_io_read_1(bc, slotioh,
			    EISA_SLOTOFF_PID + i);

		/* Create the ID string from the vendor and product IDs */
		ea.ea_idstring[0] = EISA_VENDID_0(ea.ea_vid);
		ea.ea_idstring[1] = EISA_VENDID_1(ea.ea_vid);
		ea.ea_idstring[2] = EISA_VENDID_2(ea.ea_vid);
		ea.ea_idstring[3] = EISA_PRODID_0(ea.ea_pid);
		ea.ea_idstring[4] = EISA_PRODID_1(ea.ea_pid);
		ea.ea_idstring[5] = EISA_PRODID_2(ea.ea_pid);
		ea.ea_idstring[6] = EISA_PRODID_3(ea.ea_pid);
		ea.ea_idstring[7] = '\0';		/* sanity */

		/* We no longer need the I/O handle; free it. */
		bus_io_unmap(bc, slotioh, EISA_SLOT_SIZE);

		/* Attach matching device. */
		config_found_sm(self, &ea, eisaprint, eisasubmatch);
	}
}

#ifdef EISAVERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct eisa_knowndev {
	int	flags;
	const char *id, *name;
};
#define EISA_KNOWNDEV_NOPROD	0x01		/* match on vendor only */

#include <dev/eisa/eisadevs_data.h>
#endif /* EISAVERBOSE */

void
eisa_devinfo(id, cp)
	const char *id;
	char *cp;
{
	const char *name;
	int onlyvendor;
#ifdef EISAVERBOSE
	struct eisa_knowndev *edp;
	int match;
	const char *unmatched = "unknown ";
#else
	const char *unmatched = "";
#endif

	onlyvendor = 0;
	name = NULL;

#ifdef EISAVERBOSE
	/* find the device in the table, if possible. */
	edp = eisa_knowndevs;
	while (edp->id != NULL) {
		/* check this entry for a match */
		if ((edp->flags & EISA_KNOWNDEV_NOPROD) != 0)
			match = !strncmp(edp->id, id, 3);
		else
			match = !strcmp(edp->id, id);
		if (match) {
			name = edp->name;
			onlyvendor = (edp->flags & EISA_KNOWNDEV_NOPROD) != 0;
			break;
		}
		edp++;
	}
#endif

	if (name == NULL)
		cp += sprintf(cp, "%sdevice %s", unmatched, id);
	else if (onlyvendor)			/* never if not EISAVERBOSE */
		cp += sprintf(cp, "unknown %s device %s", name, id);
	else
		cp += sprintf(cp, "%s", name);
}
