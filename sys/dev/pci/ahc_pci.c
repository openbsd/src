/*	$OpenBSD: ahc_pci.c,v 1.36 2002/11/19 18:40:16 jason Exp $	*/
/*	$NetBSD: ahc_pci.c,v 1.9 1996/10/21 22:56:24 thorpej Exp $	*/

/*
 * Product specific probe and attach routines for:
 *      3940, 2940, aic7880, aic7870, aic7860 and aic7850 SCSI controllers
 *
 * Copyright (c) 1995, 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define AHC_PCI_IOADDR	PCI_MAPREG_START	/* I/O Address */
#define AHC_PCI_MEMADDR	(PCI_MAPREG_START + 4)	/* Mem I/O Address */

#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxx_openbsd.h>
#include <dev/ic/aic7xxx_inline.h>
#include <dev/ic/smc93cx6var.h>

/* 
 * XXX memory-mapped is busted on some i386 on-board chips.
 * for i386, we don't even try it.  Also, suppress the damn 
 * PCI bus errors messages on i386.  They are not fatal, and are 
 * usually caused by some other device on the PCI bus.  But some 
 * ahc cards won't work without ACKing them.  So just ACK and go!  
 * XXX- smurph
 */
#ifndef i386
#define AHC_ALLOW_MEMIO
#define AHC_SHOW_PCI_ERRORS
#endif

/*
 * Under normal circumstances, these messages are unnecessary
 * and not terribly cosmetic.
 */
#ifdef DEBUG
#define bootverbose	1
#else
#define bootverbose	0
#endif

#define PCI_BASEADR0	PCI_MAPREG_START

#define AHC_394X_SLOT_CHANNEL_A	4
#define AHC_394X_SLOT_CHANNEL_B	5

#define AHC_398X_SLOT_CHANNEL_A	4
#define AHC_398X_SLOT_CHANNEL_B	8
#define AHC_398X_SLOT_CHANNEL_C	12

#define	EXROMBADR		0x30
#define 	EXROMEN		0x00000001UL	/* External Rom Enable */

#define	DEVCONFIG		0x40
#define		SCBSIZE32	0x00010000UL	/* aic789X only */
#define		REXTVALID	0x00001000UL	/* ultra cards only */
#define		MPORTMODE	0x00000400UL	/* aic7870+ only */
#define		RAMPSM		0x00000200UL	/* aic7870+ only */
#define		VOLSENSE	0x00000100UL
#define		PCI64BIT	0x00000080UL	/* 64Bit PCI bus (Ultra2 Only)*/
#define		SCBRAMSEL	0x00000080UL
#define		MRDCEN		0x00000040UL
#define		EXTSCBTIME	0x00000020UL	/* aic7870 only */
#define		EXTSCBPEN	0x00000010UL	/* aic7870 only */
#define		BERREN		0x00000008UL
#define		DACEN		0x00000004UL
#define		STPWLEVEL	0x00000002UL
#define		DIFACTNEGEN	0x00000001UL	/* aic7870 only */

#define	CSIZE_LATTIME		0x0c
#define		CACHESIZE	0x0000003fUL	/* only 5 bits */
#define		LATTIME		0x0000ff00UL

static int  ahc_ext_scbram_present(struct ahc_softc *ahc);
static void ahc_scbram_config(struct ahc_softc *ahc, int enable,
				  int pcheck, int fast, int large);
static void ahc_probe_ext_scbram(struct ahc_softc *ahc);
static void check_extport(struct ahc_softc *ahc, u_int *sxfrctl1);
static void configure_termination(struct ahc_softc *ahc,
				  struct seeprom_descriptor *sd,
				  u_int adapter_control,
				  u_int *sxfrctl1);
static void ahc_new_term_detect(struct ahc_softc *ahc,
				int *enableSEC_low,
				int *enableSEC_high,
				int *enablePRI_low,
				int *enablePRI_high,
				int *eeprom_present);
static void aic787X_cable_detect(struct ahc_softc *ahc,
				 int *internal50_present,
				 int *internal68_present,
				 int *externalcable_present,
				 int *eeprom_present);
static void aic785X_cable_detect(struct ahc_softc *ahc,
				 int *internal50_present,
				 int *externalcable_present,
				 int *eeprom_present);
static void write_brdctl(struct ahc_softc *ahc, u_int8_t value);
static u_int8_t read_brdctl(struct ahc_softc *ahc);

int ahc_do_pci_config(struct ahc_softc *ahc);

void load_seeprom(struct ahc_softc *ahc);
static int acquire_seeprom(struct ahc_softc *ahc,
			   struct seeprom_descriptor *sd);
static void release_seeprom(struct seeprom_descriptor *sd);
int ahc_probe_scbs(struct ahc_softc *ahc);

static u_char aic3940_count;

int ahc_pci_probe(struct device *, void *, void *);
void ahc_pci_attach(struct device *, struct device *, void *);

struct cfattach ahc_pci_ca = {
	sizeof(struct ahc_softc), ahc_pci_probe, ahc_pci_attach
};

const struct pci_matchid ahc_pci_devices[] = {
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7810 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7850 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7855 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC5900 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC5905 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7860 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2940AU },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7870 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2930CU },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2940 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_3940 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_3985 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2944 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7880 },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2940U },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_3940U },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_398XU },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2944U },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_2940UWPro },
	{ PCI_VENDOR_ADP, PCI_PRODUCT_ADP_7895 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7890 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_2940U2 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_2930U2 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7892 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_29160 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_19160B },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_3950U2B },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_3950U2D },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7896 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7899B },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7899D },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7899F },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_AIC7899 },
	{ PCI_VENDOR_ADP2, PCI_PRODUCT_ADP2_3960D },
};

int
ahc_pci_probe(parent, match, aux)
struct device *parent;
void *match, *aux;
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ahc_pci_devices,
	    sizeof(ahc_pci_devices)/sizeof(ahc_pci_devices[0])));
}

void
ahc_pci_attach(parent, self, aux)
struct device *parent, *self;
void *aux;
{
	struct pci_attach_args *pa = aux;
	struct ahc_softc *ahc = (void *)self;
	pcireg_t devconfig;
	pcireg_t command;

	/* setup the PCI stuff */
	ahc->dev_softc = pa;

	/* 
	 * We really don't allocate our softc, but 
	 * we need to do the initialization. And this 
	 * also allocates the platform_data structure.
	 */
	ahc_alloc(ahc, NULL);
	ahc_set_name(ahc, ahc->sc_dev.dv_xname);
	ahc_set_unit(ahc, ahc->sc_dev.dv_unit);
	
	/* set dma tags */
	ahc->parent_dmat = pa->pa_dmat;
	ahc->buffer_dmat = pa->pa_dmat;
        ahc->shared_data_dmat = pa->pa_dmat;
	
	/* card specific setup */
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ADP:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADP_7895:
			ahc->channel = pa->pa_function == 1 ? 'B' : 'A';
			/* The 'C' revision of the aic7895 
			   has a few additional features */
			if (PCI_REVISION(pa->pa_class) >= 4){
				ahc->chip |= AHC_AIC7895C;
			} else {
				ahc->chip |= AHC_AIC7895;
			}
			break;
		case PCI_PRODUCT_ADP_3940U:
		case PCI_PRODUCT_ADP_3940:
			if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_3940U) {
				ahc->chip |= AHC_AIC7880;
			} else {
				ahc->chip |= AHC_AIC7870;
			}
			aic3940_count++;
			if (!(aic3940_count & 0x01))
				/* Even count implies second channel */
				ahc->channel = 'B';
			break;
		case PCI_PRODUCT_ADP_2940UWPro:
			ahc->flags |= AHC_INT50_SPEEDFLEX;
			/* fall through */
		case PCI_PRODUCT_ADP_AIC7880:
		case PCI_PRODUCT_ADP_398XU:	/* XXX */
		case PCI_PRODUCT_ADP_2944U:
		case PCI_PRODUCT_ADP_2940U:
			ahc->chip |= AHC_AIC7880;
			break;
		case PCI_PRODUCT_ADP_AIC7870:
		case PCI_PRODUCT_ADP_2944:
		case PCI_PRODUCT_ADP_2940:
			ahc->chip |= AHC_AIC7870;
			break;
		case PCI_PRODUCT_ADP_AIC7860:
		case PCI_PRODUCT_ADP_2940AU:
			ahc->chip |= AHC_AIC7860;
			break;
		case PCI_PRODUCT_ADP_AIC7855:
		case PCI_PRODUCT_ADP_AIC7850:
			ahc->chip |= AHC_AIC7850;
			break;
		default:
			/* TTT */
			break;
	}
		break;
	case PCI_VENDOR_ADP2:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADP2_AIC7890:
		case PCI_PRODUCT_ADP2_2940U2:
		case PCI_PRODUCT_ADP2_2930U2:
			ahc->chip |= AHC_AIC7890;
			break;
		case PCI_PRODUCT_ADP2_AIC7892:
		case PCI_PRODUCT_ADP2_29160:
		case PCI_PRODUCT_ADP2_19160B:
			ahc->chip |= AHC_AIC7892;
			break;
		case PCI_PRODUCT_ADP2_3950U2B:
		case PCI_PRODUCT_ADP2_3950U2D:
		case PCI_PRODUCT_ADP2_AIC7896:
			ahc->chip |= AHC_AIC7896;
			ahc->channel = pa->pa_function == 1 ? 'B' : 'A';
			devconfig = ahc_pci_read_config(ahc->dev_softc, 
						  DEVCONFIG, 4);
			/* turn off 64 bit for now XXX smurph */
			devconfig &= ~PCI64BIT;
			ahc_pci_write_config(ahc->dev_softc, 
				       DEVCONFIG, devconfig, 4);
			break;
		case PCI_PRODUCT_ADP2_AIC7899:
		case PCI_PRODUCT_ADP2_3960D:
			ahc->chip |= AHC_AIC7899;
			ahc->channel = pa->pa_function == 1 ? 'B' : 'A';
			break;
		default:
			/* TTT */
			break;
		}
	}
	
	/* chip specific setup */
	switch(ahc->chip){
	case AHC_AIC7850:
	case AHC_AIC7855:
	case AHC_AIC7859:
		ahc->features = AHC_AIC7850_FE;
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_CACHETHEN_BUG|AHC_PCI_MWI_BUG;
		if (PCI_REVISION(pa->pa_class) >= 1)
			ahc->bugs |= AHC_PCI_2_1_RETRY_BUG;
		break;
	case AHC_AIC7860:
		ahc->features = AHC_AIC7860_FE;
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_CACHETHEN_BUG|AHC_PCI_MWI_BUG;
		if (PCI_REVISION(pa->pa_class) >= 1)
			ahc->bugs |= AHC_PCI_2_1_RETRY_BUG;
		break;
	case AHC_AIC7870:
		ahc->features = AHC_AIC7870_FE;
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_CACHETHEN_BUG|AHC_PCI_MWI_BUG;
		break;
	case AHC_AIC7880:
		ahc->features = AHC_AIC7880_FE;
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG;
		if (PCI_REVISION(pa->pa_class) >= 1) {
			ahc->bugs |= AHC_PCI_2_1_RETRY_BUG;
		} else {
			ahc->bugs |= AHC_CACHETHEN_BUG|AHC_PCI_MWI_BUG;
		}
		break;
	case AHC_AIC7895:
		ahc->features = AHC_AIC7895_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		/*
		 * The BIOS disables the use of MWI transactions
		 * since it does not have the MWI bug work around
		 * we have.  Disabling MWI reduces performance, so
		 * turn it on again.
		 */
		command = pci_conf_read(pa->pa_pc, pa->pa_tag,
					PCI_COMMAND_STATUS_REG);
		command |= PCI_COMMAND_INVALIDATE_ENABLE;
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			       command);
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_PCI_2_1_RETRY_BUG
			  |  AHC_CACHETHEN_BUG | AHC_PCI_MWI_BUG;
		break;		
	case AHC_AIC7895C:
		ahc->features = AHC_AIC7895C_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		ahc->bugs |= AHC_TMODE_WIDEODD_BUG|AHC_PCI_2_1_RETRY_BUG
			  |  AHC_CACHETHEN_BUG;
		break;
	case AHC_AIC7890:
		ahc->features = AHC_AIC7890_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		if (PCI_REVISION(pa->pa_class) == 0)
			ahc->bugs |= AHC_AUTOFLUSH_BUG|AHC_CACHETHEN_BUG;
		break;
	case AHC_AIC7892:
		ahc->features = AHC_AIC7892_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		ahc->bugs |= AHC_SCBCHAN_UPLOAD_BUG;
		break;
	case AHC_AIC7896:
		ahc->features = AHC_AIC7896_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		ahc->bugs |= AHC_CACHETHEN_DIS_BUG;
		break;
	case AHC_AIC7899:
		ahc->features = AHC_AIC7899_FE;
		ahc->flags |= AHC_NEWEEPROM_FMT;
		ahc->bugs |= AHC_SCBCHAN_UPLOAD_BUG;
		break;
	default:
		break;
	}

	/* setup the PCI interrupt */
	ahc->bus_intr = ahc_pci_intr;
	ahc->unsolicited_ints = 0; 

	if(ahc_do_pci_config(ahc)){
		ahc_free(ahc);
		return;
	}
	
	ahc_attach(ahc);
}

int
ahc_pci_map_registers(ahc)
	struct ahc_softc *ahc;
{
	pcireg_t	command;
	int		ioh_valid;
	bus_space_tag_t  iot;
	bus_space_handle_t ioh;
	struct pci_attach_args *pa = ahc->dev_softc;

	command = ahc_pci_read_config(ahc->dev_softc,
				      PCI_COMMAND_STATUS_REG, 4);
#ifdef AHC_ALLOW_MEMIO
	/*
	 * attempt to use memory mapping on hardware that supports it.
	 * e.g powerpc  XXX - smurph
	 *
	 * Note:  If this fails, IO mapping is used.
	 */
	if ((command & PCI_COMMAND_MEM_ENABLE) != 0) {
		pcireg_t memtype;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AHC_PCI_MEMADDR);
		switch (memtype) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			ioh_valid = (pci_mapreg_map(pa, AHC_PCI_MEMADDR,
				memtype, 0, &iot, &ioh, NULL, NULL, 0) == 0);
			break;
		default:
			ioh_valid = 0;
		}
		if (ioh_valid) {
			/*
			 * Do a quick test to see if memory mapped
			 * I/O is functioning correctly.
			 */
			ahc->tag = iot;
			ahc->bsh = ioh;
			if (ahc_inb(ahc, HCNTRL) == 0xFF) {
				/* nope, use I/O mapping */
                                ioh_valid = 0;
			} else {
				/* Using memory mapping, disable I/O mapping */
                                command &= ~PCI_COMMAND_IO_ENABLE;
				ahc_pci_write_config(ahc->dev_softc,
						     PCI_COMMAND_STATUS_REG,
						     command, 4);
			}
		}
	}
	
	if (!ioh_valid) /* try to drop back to IO mapping */
#endif
	{
		ioh_valid = (pci_mapreg_map(pa, AHC_PCI_IOADDR,
		    PCI_MAPREG_TYPE_IO, 0, &iot, &ioh, NULL, NULL, 0) == 0);
		
		/* Using I/O mapping, disable memory mapping */
		command &= ~PCI_COMMAND_MEM_ENABLE;
		ahc_pci_write_config(ahc->dev_softc,
				     PCI_COMMAND_STATUS_REG,
				     command, 4);
	}

	if (!ioh_valid) {
		/* Game Over.  Insert coin... */
		printf(": unable to map registers\n");
		return (1);
	}
	ahc->tag = iot;
	ahc->bsh = ioh;
	return (0);
}

int
ahc_do_pci_config(ahc)
	struct ahc_softc *ahc;
{
	pcireg_t	 command;
	u_int		 our_id = 0;
	u_int		 sxfrctl1;
	u_int		 scsiseq;
	u_int		 dscommand0;
	int		 error;
	int		 opri;
	uint8_t		 sblkctl;


	ahc->chip |= AHC_PCI;
#if 0
	ahc_power_state_change(ahc, AHC_POWER_STATE_D0);
#endif 
	error = ahc_pci_map_registers(ahc);
	if (error != 0)
		return (error);
	/* 
	 * Registers are mapped. Now it is safe to use 
	 * the ahc_inb and ahc_outb macros. 
	 */
	
	/* 
	 * Before we continue probing the card, ensure that
	 * its interrupts are *disabled*.  We don't want
	 * a misstep to hang the machine in an interrupt
	 * storm.
	 */
	ahc_intr_enable(ahc, FALSE);

	/*
	 * If we need to support high memory, enable dual
	 * address cycles.  This bit must be set to enable
	 * high address bit generation even if we are on a
	 * 64bit bus (PCI64BIT set in devconfig).
	 */
	if ((ahc->flags & AHC_39BIT_ADDRESSING) != 0) {
		pcireg_t devconfig;

		if (bootverbose)
			printf("%s: Enabling 39Bit Addressing\n",
			       ahc_name(ahc));
		devconfig = ahc_pci_read_config(ahc->dev_softc, DEVCONFIG, 4);
		devconfig |= DACEN;
		ahc_pci_write_config(ahc->dev_softc, DEVCONFIG, devconfig, 4);
	}
	
	/* Ensure busmastering is enabled */
        command = ahc_pci_read_config(ahc->dev_softc, PCI_COMMAND_STATUS_REG, 4);
	command |= PCI_COMMAND_MASTER_ENABLE; 

	ahc_pci_write_config(ahc->dev_softc, PCI_COMMAND_STATUS_REG, command, 4);
	
	/* On all PCI adapters, we allow SCB paging */
	ahc->flags |= AHC_PAGESCBS;

	error = ahc_softc_init(ahc);
	if (error != 0)
		return (error);

	/* Remember how the card was setup in case there is no SEEPROM */
	if ((ahc_inb(ahc, HCNTRL) & POWRDN) == 0) {
		ahc_pause(ahc);
		if ((ahc->features & AHC_ULTRA2) != 0)
			our_id = ahc_inb(ahc, SCSIID_ULTRA2) & OID;
		else
			our_id = ahc_inb(ahc, SCSIID) & OID;
		sxfrctl1 = ahc_inb(ahc, SXFRCTL1) & STPWEN;
		scsiseq = ahc_inb(ahc, SCSISEQ);
	} else {
		sxfrctl1 = STPWEN;
		our_id = 7;
		scsiseq = 0;
	}

	error = ahc_reset(ahc);
	if (error != 0)
		return (ENXIO);

	if ((ahc->features & AHC_DT) != 0) {
		u_int sfunct;

		/* Perform ALT-Mode Setup */
		sfunct = ahc_inb(ahc, SFUNCT) & ~ALT_MODE;
		ahc_outb(ahc, SFUNCT, sfunct | ALT_MODE);
		ahc_outb(ahc, OPTIONMODE,
			 OPTIONMODE_DEFAULTS|AUTOACKEN|BUSFREEREV|EXPPHASEDIS);
		ahc_outb(ahc, SFUNCT, sfunct);

		/* Normal mode setup */
		ahc_outb(ahc, CRCCONTROL1, CRCVALCHKEN|CRCENDCHKEN|CRCREQCHKEN
					  |TARGCRCENDEN);
	}

	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splbio();
	
	dscommand0 = ahc_inb(ahc, DSCOMMAND0);
	dscommand0 |= MPARCKEN|CACHETHEN;
	if ((ahc->features & AHC_ULTRA2) != 0) {

		/*
		 * DPARCKEN doesn't work correctly on
		 * some MBs so don't use it.
		 */
		dscommand0 &= ~DPARCKEN;
	}

	/*
	 * Handle chips that must have cache line
	 * streaming (dis/en)abled.
	 */
	if ((ahc->bugs & AHC_CACHETHEN_DIS_BUG) != 0)
		dscommand0 |= CACHETHEN;

	if ((ahc->bugs & AHC_CACHETHEN_BUG) != 0)
		dscommand0 &= ~CACHETHEN;

	ahc_outb(ahc, DSCOMMAND0, dscommand0);
	
	ahc->pci_cachesize = ahc_pci_read_config(ahc->dev_softc,
						 CSIZE_LATTIME, 4) & CACHESIZE;
	ahc->pci_cachesize *= 4;

	if ((ahc->bugs & AHC_PCI_2_1_RETRY_BUG) != 0
	 && ahc->pci_cachesize == 4) {
		u_int csl = ahc_pci_read_config(ahc->dev_softc,
						CSIZE_LATTIME, 4);
		csl &= ~CACHESIZE;
		ahc_pci_write_config(ahc->dev_softc, CSIZE_LATTIME, csl, 4);
		ahc->pci_cachesize = 0;
	}

	/*
	 * We cannot perform ULTRA speeds without the presense
	 * of the external precision resistor.
	 */
	if ((ahc->features & AHC_ULTRA) != 0) {
		uint32_t devconfig;

		devconfig = ahc_pci_read_config(ahc->dev_softc, DEVCONFIG, 4);
		if ((devconfig & REXTVALID) == 0)
			ahc->features &= ~AHC_ULTRA;
	}

	/* See if we have a SEEPROM and perform auto-term */
	check_extport(ahc, &sxfrctl1);

	/*
	 * Take the LED out of diagnostic mode
	 */
	sblkctl = ahc_inb(ahc, SBLKCTL);
	ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

	if ((ahc->features & AHC_ULTRA2) != 0) {
		ahc_outb(ahc, DFF_THRSH, RD_DFTHRSH_MAX|WR_DFTHRSH_MAX);
	} else {
		ahc_outb(ahc, DSPCISTATUS, DFTHRSH_100);
	}

	if (ahc->flags & AHC_USEDEFAULTS) {
		/*
		 * PCI Adapter default setup
		 * Should only be used if the adapter does not have
		 * a SEEPROM.
		 */
		/* See if someone else set us up already */
		if (scsiseq != 0) {
			printf("%s: Using left over BIOS settings\n",
				ahc_name(ahc));
			ahc->flags &= ~AHC_USEDEFAULTS;
			ahc->flags |= AHC_BIOS_ENABLED;
		} else {
			/*
			 * Assume only one connector and always turn
			 * on termination.
			 */
 			our_id = 0x07;
			sxfrctl1 = STPWEN;
		}
		ahc_outb(ahc, SCSICONF, our_id|ENSPCHK|RESET_SCSI);

		ahc->our_id = our_id;
	}

	/*
	 * Take a look to see if we have external SRAM.
	 * We currently do not attempt to use SRAM that is
	 * shared among multiple controllers.
	 */
	ahc_probe_ext_scbram(ahc);

	/*
	 * Record our termination setting for the
	 * generic initialization routine.
	 */
	if ((sxfrctl1 & STPWEN) != 0)
		ahc->flags |= AHC_TERM_ENB_A;

	/* Core initialization */
	error = ahc_init(ahc);
	if (error != 0)
		return (error);

	/* Special func to force negotiation */
	ahc_force_neg(ahc);

	/*
	 * Link this softc in with all other ahc instances.
	 */
	ahc_softc_insert(ahc);

	/*
	 * Allow interrupts now that we are completely setup.
	 */
	error = ahc_pci_map_int(ahc);
	if (error != 0)
		return (error);

	ahc_intr_enable(ahc, TRUE);
	splx(opri);

	return (0);
}

int
ahc_pci_map_int(ahc)
	struct ahc_softc *ahc;
{
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	struct pci_attach_args *pa = ahc->dev_softc;
	
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return 1;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	
	ahc->platform_data->ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
						    ahc_platform_intr, ahc,
						    ahc->sc_dev.dv_xname);
	
	if (ahc->platform_data->ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return 1;
	}
	
	if (intrstr != NULL)
		printf(": %s\n", intrstr);
	return 0;
}

/*
 * Test for the presense of external sram in an
 * "unshared" configuration.
 */
static int
ahc_ext_scbram_present(struct ahc_softc *ahc)
{
	u_int chip;
	int ramps;
	int single_user;
	uint32_t devconfig;
	
	chip = ahc->chip & AHC_CHIPID_MASK;
	
	devconfig = ahc_pci_read_config(ahc->dev_softc, DEVCONFIG, 4);

	single_user = (devconfig & MPORTMODE) != 0;

	if ((ahc->features & AHC_ULTRA2) != 0)
		ramps = (ahc_inb(ahc, DSCOMMAND0) & RAMPS) != 0;
	else if (chip >= AHC_AIC7870)
		ramps = (devconfig & RAMPSM) != 0;
	else
		ramps = 0;

	if (ramps && single_user)
		return (1);
	return (0);
}

/*
 * Enable external scbram.
 */
static void
ahc_scbram_config(ahc, enable, pcheck, fast, large)
	struct ahc_softc *ahc;
	int enable;
	int pcheck;
	int fast;
	int large;
{
	pcireg_t devconfig;

	if (ahc->features & AHC_MULTI_FUNC) {
		/*
		 * Set the SCB Base addr (highest address bit)
		 * depending on which channel we are.
		 */
		ahc_outb(ahc, SCBBADDR, ahc_get_pci_function(ahc->dev_softc));
	}

	devconfig = ahc_pci_read_config(ahc->dev_softc, DEVCONFIG, 4);
	if ((ahc->features & AHC_ULTRA2) != 0) {
		u_int dscommand0;

		dscommand0 = ahc_inb(ahc, DSCOMMAND0);
		if (enable)
			dscommand0 &= ~INTSCBRAMSEL;
		else
			dscommand0 |= INTSCBRAMSEL;
		if (large)
			dscommand0 &= ~USCBSIZE32;
		else
			dscommand0 |= USCBSIZE32;
		ahc_outb(ahc, DSCOMMAND0, dscommand0);
	} else {
		if (fast)
			devconfig &= ~EXTSCBTIME;
		else
			devconfig |= EXTSCBTIME;
		if (enable)
			devconfig &= ~SCBRAMSEL;
		else
			devconfig |= SCBRAMSEL;
		if (large)
			devconfig &= ~SCBSIZE32;
		else
			devconfig |= SCBSIZE32;
	}
	if (pcheck)
		devconfig |= EXTSCBPEN;
	else
		devconfig &= ~EXTSCBPEN;

	ahc_pci_write_config(ahc->dev_softc, DEVCONFIG, devconfig, 4);
}

/*
 * Take a look to see if we have external SRAM.
 * We currently do not attempt to use SRAM that is
 * shared among multiple controllers.
 */
static void
ahc_probe_ext_scbram(ahc)
	struct ahc_softc *ahc;
{
	int num_scbs;
	int test_num_scbs;
	int enable;
	int pcheck;
	int fast;
	int large;

	enable = FALSE;
	pcheck = FALSE;
	fast = FALSE;
	large = FALSE;
	num_scbs = 0;
	
	if (ahc_ext_scbram_present(ahc) == 0)
		goto done;

	/*
	 * Probe for the best parameters to use.
	 */
	ahc_scbram_config(ahc, /*enable*/TRUE, pcheck, fast, large);
	num_scbs = ahc_probe_scbs(ahc);
	if (num_scbs == 0) {
		/* The SRAM wasn't really present. */
		goto done;
	}
	enable = TRUE;

	/*
	 * Clear any outstanding parity error
	 * and ensure that parity error reporting
	 * is enabled.
	 */
	ahc_outb(ahc, SEQCTL, 0);
	ahc_outb(ahc, CLRINT, CLRPARERR);
	ahc_outb(ahc, CLRINT, CLRBRKADRINT);

	/* Now see if we can do parity */
	ahc_scbram_config(ahc, enable, /*pcheck*/TRUE, fast, large);
	num_scbs = ahc_probe_scbs(ahc);
	if ((ahc_inb(ahc, INTSTAT) & BRKADRINT) == 0
	 || (ahc_inb(ahc, ERROR) & MPARERR) == 0)
		pcheck = TRUE;

	/* Clear any resulting parity error */
	ahc_outb(ahc, CLRINT, CLRPARERR);
	ahc_outb(ahc, CLRINT, CLRBRKADRINT);

	/* Now see if we can do fast timing */
	ahc_scbram_config(ahc, enable, pcheck, /*fast*/TRUE, large);
	test_num_scbs = ahc_probe_scbs(ahc);
	if (test_num_scbs == num_scbs
	 && ((ahc_inb(ahc, INTSTAT) & BRKADRINT) == 0
	  || (ahc_inb(ahc, ERROR) & MPARERR) == 0))
		fast = TRUE;

	/*
	 * See if we can use large SCBs and still maintain
	 * the same overall count of SCBs.
	 */
	if ((ahc->features & AHC_LARGE_SCBS) != 0) {
		ahc_scbram_config(ahc, enable, pcheck, fast, /*large*/TRUE);
		test_num_scbs = ahc_probe_scbs(ahc);
		if (test_num_scbs >= num_scbs) {
			large = TRUE;
			num_scbs = test_num_scbs;
	 		if (num_scbs >= 64) {
				/*
				 * We have enough space to move the
				 * "busy targets table" into SCB space
				 * and make it qualify all the way to the
				 * lun level.
				 */
				ahc->flags |= AHC_SCB_BTT;
			}
		}
	}
done:
	/*
	 * Disable parity error reporting until we
	 * can load instruction ram.
	 */
	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS);
	/* Clear any latched parity error */
	ahc_outb(ahc, CLRINT, CLRPARERR);
	ahc_outb(ahc, CLRINT, CLRBRKADRINT);
	if (bootverbose && enable) {
		printf("%s: External SRAM, %s access%s, %dbytes/SCB\n",
		       ahc_name(ahc), fast ? "fast" : "slow", 
		       pcheck ? ", parity checking enabled" : "",
		       large ? 64 : 32);
	}
	ahc_scbram_config(ahc, enable, pcheck, fast, large);
}

/*
 * Check the external port logic for a serial eeprom
 * and termination/cable detection contrls.
 */
static void
check_extport(ahc, sxfrctl1)
	struct ahc_softc *ahc;
	u_int *sxfrctl1;
{
	struct	seeprom_descriptor sd;
	struct	seeprom_config sc;
	u_int	scsi_conf;
	u_int	adapter_control;
	int	have_seeprom;
	int	have_autoterm;
	
	sd.sd_ahc = ahc;
	sd.sd_control_offset = SEECTL;		
	sd.sd_status_offset = SEECTL;		
	sd.sd_dataout_offset = SEECTL;		

	/*
	 * For some multi-channel devices, the c46 is simply too
	 * small to work.  For the other controller types, we can
	 * get our information from either SEEPROM type.  Set the
	 * type to start our probe with accordingly.
	 */
	if (ahc->flags & AHC_LARGE_SEEPROM)
		sd.sd_chip = C56_66;
	else
		sd.sd_chip = C46;

	sd.sd_MS = SEEMS;
	sd.sd_RDY = SEERDY;
	sd.sd_CS = SEECS;
	sd.sd_CK = SEECK;
	sd.sd_DO = SEEDO;
	sd.sd_DI = SEEDI;

	have_seeprom = acquire_seeprom(ahc, &sd);
	if (have_seeprom) {

		if (bootverbose) 
			printf("%s: Reading SEEPROM...", ahc_name(ahc));

		for (;;) {
			u_int start_addr;

			start_addr = 32 * (ahc->channel - 'A');

			have_seeprom = read_seeprom(&sd, (uint16_t *)&sc,
						    start_addr, sizeof(sc)/2);

			if (have_seeprom)
				have_seeprom = verify_cksum(&sc);

			if (have_seeprom != 0 || sd.sd_chip == C56_66) {
				if (bootverbose) {
					if (have_seeprom == 0)
						printf ("checksum error\n");
					else
						printf ("done.\n");
				}
				break;
			}
			sd.sd_chip = C56_66;
		}
		release_seeprom(&sd);
	}

	if (!have_seeprom) {
		/*
		 * Pull scratch ram settings and treat them as
		 * if they are the contents of an seeprom if
		 * the 'ADPT' signature is found in SCB2.
		 * We manually compose the data as 16bit values
		 * to avoid endian issues.
		 */
		ahc_outb(ahc, SCBPTR, 2);
		if (ahc_inb(ahc, SCB_BASE) == 'A'
		 && ahc_inb(ahc, SCB_BASE + 1) == 'D'
		 && ahc_inb(ahc, SCB_BASE + 2) == 'P'
		 && ahc_inb(ahc, SCB_BASE + 3) == 'T') {
			uint16_t *sc_data;
			int	  i;

			sc_data = (uint16_t *)&sc;
			for (i = 0; i < 32; i++) {
				uint16_t val;
				int	 j;

				j = i * 2;
				val = ahc_inb(ahc, SRAM_BASE + j)
				    | ahc_inb(ahc, SRAM_BASE + j + 1) << 8;
			}
			have_seeprom = verify_cksum(&sc);
		}
		/*
		 * Clear any SCB parity errors in case this data and
		 * its associated parity was not initialized by the BIOS
		 */
		ahc_outb(ahc, CLRINT, CLRPARERR);
		ahc_outb(ahc, CLRINT, CLRBRKADRINT);
	}

	if (!have_seeprom) {
		if (bootverbose)
			printf("%s: No SEEPROM available.\n", ahc_name(ahc));
		ahc->flags |= AHC_USEDEFAULTS;
	} else {
		/*
		 * Put the data we've collected down into SRAM
		 * where ahc_init will find it.
		 */
		int i;
		int max_targ = sc.max_targets & CFMAXTARG;
		uint16_t discenable;
		uint16_t ultraenb;

		discenable = 0;
		ultraenb = 0;
		if ((sc.adapter_control & CFULTRAEN) != 0) {
			/*
			 * Determine if this adapter has a "newstyle"
			 * SEEPROM format.
			 */
			for (i = 0; i < max_targ; i++) {
				if ((sc.device_flags[i] & CFSYNCHISULTRA) != 0){
					ahc->flags |= AHC_NEWEEPROM_FMT;
					break;
				}
			}
		}

		for (i = 0; i < max_targ; i++) {
			u_int     scsirate;
			uint16_t target_mask;

			target_mask = 0x01 << i;
			if (sc.device_flags[i] & CFDISC)
				discenable |= target_mask;
			if ((ahc->flags & AHC_NEWEEPROM_FMT) != 0) {
				if ((sc.device_flags[i] & CFSYNCHISULTRA) != 0)
					ultraenb |= target_mask;
			} else if ((sc.adapter_control & CFULTRAEN) != 0) {
				ultraenb |= target_mask;
			}
			if ((sc.device_flags[i] & CFXFER) == 0x04
			 && (ultraenb & target_mask) != 0) {
				/* Treat 10MHz as a non-ultra speed */
				sc.device_flags[i] &= ~CFXFER;
			 	ultraenb &= ~target_mask;
			}
			if ((ahc->features & AHC_ULTRA2) != 0) {
				u_int offset;

				if (sc.device_flags[i] & CFSYNCH)
					offset = MAX_OFFSET_ULTRA2;
				else 
					offset = 0;
				ahc_outb(ahc, TARG_OFFSET + i, offset);

				/*
				 * The ultra enable bits contain the
				 * high bit of the ultra2 sync rate
				 * field.
				 */
				scsirate = (sc.device_flags[i] & CFXFER)
					 | ((ultraenb & target_mask)
					    ? 0x8 : 0x0);
				if (sc.device_flags[i] & CFWIDEB)
					scsirate |= WIDEXFER;
			} else {
				scsirate = (sc.device_flags[i] & CFXFER) << 4;
				if (sc.device_flags[i] & CFSYNCH)
					scsirate |= SOFS;
				if (sc.device_flags[i] & CFWIDEB)
					scsirate |= WIDEXFER;
			}
			ahc_outb(ahc, TARG_SCSIRATE + i, scsirate);
		}
		ahc->our_id = sc.brtime_id & CFSCSIID;

		scsi_conf = (ahc->our_id & 0x7);
		if (sc.adapter_control & CFSPARITY)
			scsi_conf |= ENSPCHK;
		if (sc.adapter_control & CFRESETB)
			scsi_conf |= RESET_SCSI;

		ahc->flags |=
		    (sc.adapter_control & CFBOOTCHAN) >> CFBOOTCHANSHIFT;

		if (sc.bios_control & CFEXTEND)
			ahc->flags |= AHC_EXTENDED_TRANS_A;

		if (sc.bios_control & CFBIOSEN)
			ahc->flags |= AHC_BIOS_ENABLED;
		if (ahc->features & AHC_ULTRA
		 && (ahc->flags & AHC_NEWEEPROM_FMT) == 0) {
			/* Should we enable Ultra mode? */
			if (!(sc.adapter_control & CFULTRAEN))
				/* Treat us as a non-ultra card */
				ultraenb = 0;
		}

		if (sc.signature == CFSIGNATURE
		 || sc.signature == CFSIGNATURE2) {
			pcireg_t devconfig;

			/* Honor the STPWLEVEL settings */
			devconfig = ahc_pci_read_config(ahc->dev_softc, 
							DEVCONFIG, 4);
			devconfig &= ~STPWLEVEL;
			if ((sc.bios_control & CFSTPWLEVEL) != 0)
				devconfig |= STPWLEVEL;
			ahc_pci_write_config(ahc->dev_softc,
					     DEVCONFIG, devconfig, 4);
		}
		/* Set SCSICONF info */
		ahc_outb(ahc, SCSICONF, scsi_conf);
		ahc_outb(ahc, DISC_DSB, ~(discenable & 0xff));
		ahc_outb(ahc, DISC_DSB + 1, ~((discenable >> 8) & 0xff));
		ahc_outb(ahc, ULTRA_ENB, ultraenb & 0xff);
		ahc_outb(ahc, ULTRA_ENB + 1, (ultraenb >> 8) & 0xff);
	}

	/*
	 * Cards that have the external logic necessary to talk to
	 * a SEEPROM, are almost certain to have the remaining logic
	 * necessary for auto-termination control.  This assumption
	 * hasn't failed yet...
	 */
	have_autoterm = have_seeprom;
	if (have_seeprom)
		adapter_control = sc.adapter_control;
	else
		adapter_control = CFAUTOTERM;

	/*
	 * Some low-cost chips have SEEPROM and auto-term control built
	 * in, instead of using a GAL.  They can tell us directly
	 * if the termination logic is enabled.
	 */
	if ((ahc->features & AHC_SPIOCAP) != 0) {
		if ((ahc_inb(ahc, SPIOCAP) & SSPIOCPS) != 0)
			have_autoterm = TRUE;
		else
			have_autoterm = FALSE;
	}

	if (have_autoterm) {
		acquire_seeprom(ahc, &sd);
		configure_termination(ahc, &sd, adapter_control, sxfrctl1);
		release_seeprom(&sd);
	}
}

static void
configure_termination(struct ahc_softc *ahc,
		      struct seeprom_descriptor *sd,
		      u_int adapter_control,
		      u_int *sxfrctl1)
{
	uint8_t brddat;
	
	brddat = 0;

	/*
	 * Update the settings in sxfrctl1 to match the
	 * termination settings 
	 */
	*sxfrctl1 = 0;
	
	/*
	 * SEECS must be on for the GALS to latch
	 * the data properly.  Be sure to leave MS
	 * on or we will release the seeprom.
	 */
	SEEPROM_OUTB(sd, sd->sd_MS | sd->sd_CS);
	if ((adapter_control & CFAUTOTERM) != 0
	 || (ahc->features & AHC_NEW_TERMCTL) != 0) {
		int internal50_present;
		int internal68_present;
		int externalcable_present;
		int eeprom_present;
		int enableSEC_low;
		int enableSEC_high;
		int enablePRI_low;
		int enablePRI_high;
		int sum;

		enableSEC_low = 0;
		enableSEC_high = 0;
		enablePRI_low = 0;
		enablePRI_high = 0;
		if ((ahc->features & AHC_NEW_TERMCTL) != 0) {
			ahc_new_term_detect(ahc, &enableSEC_low,
					       &enableSEC_high,
					       &enablePRI_low,
					       &enablePRI_high,
					       &eeprom_present);
			if ((adapter_control & CFSEAUTOTERM) == 0) {
				if (bootverbose)
					printf("%s: Manual SE Termination\n",
					       ahc_name(ahc));
				enableSEC_low = (adapter_control & CFSELOWTERM);
				enableSEC_high =
				    (adapter_control & CFSEHIGHTERM);
			}
			if ((adapter_control & CFAUTOTERM) == 0) {
				if (bootverbose)
					printf("%s: Manual LVD Termination\n",
					       ahc_name(ahc));
				enablePRI_low = (adapter_control & CFSTERM);
				enablePRI_high = (adapter_control & CFWSTERM);
			}
			/* Make the table calculations below happy */
			internal50_present = 0;
			internal68_present = 1;
			externalcable_present = 1;
		} else if ((ahc->features & AHC_SPIOCAP) != 0) {
			aic785X_cable_detect(ahc, &internal50_present,
					     &externalcable_present,
					     &eeprom_present);
		} else {
			aic787X_cable_detect(ahc, &internal50_present,
					     &internal68_present,
					     &externalcable_present,
					     &eeprom_present);
		}

		if ((ahc->features & AHC_WIDE) == 0)
			internal68_present = 0;

		if (bootverbose
		 && (ahc->features & AHC_ULTRA2) == 0) {
			printf("%s: internal 50 cable %s present",
			       ahc_name(ahc),
			       internal50_present ? "is":"not");

			if ((ahc->features & AHC_WIDE) != 0)
				printf(", internal 68 cable %s present",
				       internal68_present ? "is":"not");
			printf("\n%s: external cable %s present\n",
			       ahc_name(ahc),
			       externalcable_present ? "is":"not");
		}
		if (bootverbose)
			printf("%s: BIOS eeprom %s present\n",
			       ahc_name(ahc), eeprom_present ? "is" : "not");

		if ((ahc->flags & AHC_INT50_SPEEDFLEX) != 0) {
			/*
			 * The 50 pin connector is a separate bus,
			 * so force it to always be terminated.
			 * In the future, perform current sensing
			 * to determine if we are in the middle of
			 * a properly terminated bus.
			 */
			internal50_present = 0;
		}

		/*
		 * Now set the termination based on what
		 * we found.
		 * Flash Enable = BRDDAT7
		 * Secondary High Term Enable = BRDDAT6
		 * Secondary Low Term Enable = BRDDAT5 (7890)
		 * Primary High Term Enable = BRDDAT4 (7890)
		 */
		if ((ahc->features & AHC_ULTRA2) == 0
		 && (internal50_present != 0)
		 && (internal68_present != 0)
		 && (externalcable_present != 0)) {
			printf("%s: Illegal cable configuration!!. "
			       "Only two connectors on the "
			       "adapter may be used at a "
			       "time!\n", ahc_name(ahc));

			/*
			 * Pretend there are no cables in the hope
			 * that having all of the termination on
			 * gives us a more stable bus.
			 */
		 	internal50_present = 0;
			internal68_present = 0;
			externalcable_present = 0;
		}

		if ((ahc->features & AHC_WIDE) != 0
		 && ((externalcable_present == 0)
		  || (internal68_present == 0)
		  || (enableSEC_high != 0))) {
			brddat |= BRDDAT6;
			if (bootverbose) {
				if ((ahc->flags & AHC_INT50_SPEEDFLEX) != 0)
					printf("%s: 68 pin termination "
					       "Enabled\n", ahc_name(ahc));
				else
					printf("%s: %sHigh byte termination "
					       "Enabled\n", ahc_name(ahc),
					       enableSEC_high ? "Secondary "
							      : "");
			}
		}

		sum = internal50_present + internal68_present
		    + externalcable_present;
		if (sum < 2 || (enableSEC_low != 0)) {
			if ((ahc->features & AHC_ULTRA2) != 0)
				brddat |= BRDDAT5;
			else
				*sxfrctl1 |= STPWEN;
			if (bootverbose) {
				if ((ahc->flags & AHC_INT50_SPEEDFLEX) != 0)
					printf("%s: 50 pin termination "
					       "Enabled\n", ahc_name(ahc));
				else
					printf("%s: %sLow byte termination "
					       "Enabled\n", ahc_name(ahc),
					       enableSEC_low ? "Secondary "
							     : "");
			}
		}

		if (enablePRI_low != 0) {
			*sxfrctl1 |= STPWEN;
			if (bootverbose)
				printf("%s: Primary Low Byte termination "
				       "Enabled\n", ahc_name(ahc));
		}

		/*
		 * Setup STPWEN before setting up the rest of
		 * the termination per the tech note on the U160 cards.
		 */
		ahc_outb(ahc, SXFRCTL1, *sxfrctl1);

		if (enablePRI_high != 0) {
			brddat |= BRDDAT4;
			if (bootverbose)
				printf("%s: Primary High Byte "
				       "termination Enabled\n",
				       ahc_name(ahc));
		}
		
		write_brdctl(ahc, brddat);

	} else {
		if ((adapter_control & CFSTERM) != 0) {
			*sxfrctl1 |= STPWEN;

			if (bootverbose)
				printf("%s: %sLow byte termination Enabled\n",
				       ahc_name(ahc),
				       (ahc->features & AHC_ULTRA2) ? "Primary "
								    : "");
		}

		if ((adapter_control & CFWSTERM) != 0
		 && (ahc->features & AHC_WIDE) != 0) {
			brddat |= BRDDAT6;
			if (bootverbose)
				printf("%s: %sHigh byte termination Enabled\n",
				       ahc_name(ahc),
				       (ahc->features & AHC_ULTRA2)
				     ? "Secondary " : "");
		}

		/*
		 * Setup STPWEN before setting up the rest of
		 * the termination per the tech note on the U160 cards.
		 */
		ahc_outb(ahc, SXFRCTL1, *sxfrctl1);

		if ((ahc->features & AHC_WIDE) != 0)
			write_brdctl(ahc, brddat);
	}
	SEEPROM_OUTB(sd, sd->sd_MS); /* Clear CS */
}

static void
ahc_new_term_detect(ahc, enableSEC_low, enableSEC_high, enablePRI_low, 
		    enablePRI_high, eeprom_present)
	struct ahc_softc *ahc;
	int *enableSEC_low;
	int *enableSEC_high;
	int *enablePRI_low;
	int *enablePRI_high;
	int *eeprom_present;
{
	u_int8_t brdctl;

	/*
	 * BRDDAT7 = Eeprom
	 * BRDDAT6 = Enable Secondary High Byte termination
	 * BRDDAT5 = Enable Secondary Low Byte termination
	 * BRDDAT4 = Enable Primary high byte termination
	 * BRDDAT3 = Enable Primary low byte termination
	 */
	brdctl = read_brdctl(ahc);
	*eeprom_present = brdctl & BRDDAT7;
	*enableSEC_high = (brdctl & BRDDAT6);
	*enableSEC_low = (brdctl & BRDDAT5);
	*enablePRI_high = (brdctl & BRDDAT4);
	*enablePRI_low = (brdctl & BRDDAT3);
}

static void
aic787X_cable_detect(ahc, internal50_present, internal68_present,
		     externalcable_present, eeprom_present)
	struct ahc_softc *ahc;
	int *internal50_present;
	int *internal68_present;
	int *externalcable_present;
	int *eeprom_present;
{
	u_int8_t brdctl;

	/*
	 * First read the status of our cables.
	 * Set the rom bank to 0 since the
	 * bank setting serves as a multiplexor
	 * for the cable detection logic.
	 * BRDDAT5 controls the bank switch.
	 */
	write_brdctl(ahc, 0);

	/*
	 * Now read the state of the internal
	 * connectors.  BRDDAT6 is INT50 and
	 * BRDDAT7 is INT68.
	 */
	brdctl = read_brdctl(ahc);
	*internal50_present = (brdctl & BRDDAT6) ? 0 : 1;
	*internal68_present = (brdctl & BRDDAT7) ? 0 : 1;

	/*
	 * Set the rom bank to 1 and determine
	 * the other signals.
	 */
	write_brdctl(ahc, BRDDAT5);

	/*
	 * Now read the state of the external
	 * connectors.  BRDDAT6 is EXT68 and
	 * BRDDAT7 is EPROMPS.
	 */
	brdctl = read_brdctl(ahc);
	*externalcable_present = (brdctl & BRDDAT6) ? 0 : 1;
	*eeprom_present = (brdctl & BRDDAT7) ? 1 : 0;
}

static void
aic785X_cable_detect(ahc, internal50_present, externalcable_present,
		     eeprom_present)
	struct ahc_softc *ahc;
	int *internal50_present;
	int *externalcable_present;
	int *eeprom_present;
{
	u_int8_t brdctl;

	ahc_outb(ahc, BRDCTL, BRDRW|BRDCS);
	ahc_outb(ahc, BRDCTL, 0);
	brdctl = ahc_inb(ahc, BRDCTL);
	*internal50_present = (brdctl & BRDDAT5) ? 0 : 1;
	*externalcable_present = (brdctl & BRDDAT6) ? 0 : 1;

	*eeprom_present = (ahc_inb(ahc, SPIOCAP) & EEPROM) ? 1 : 0;
}

static void
write_brdctl(ahc, value)
	struct   ahc_softc *ahc;
	u_int8_t value;
{
	u_int8_t brdctl;

	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7895) {
		brdctl = BRDSTB;
		if (ahc->channel == 'B')
			brdctl |= BRDCS;
	} else if ((ahc->features & AHC_ULTRA2) != 0) {
		brdctl = 0;
	} else {
		brdctl = BRDSTB|BRDCS;
	}
	ahc_outb(ahc, BRDCTL, brdctl);
	ahc_flush_device_writes(ahc);
	brdctl |= value;
	ahc_outb(ahc, BRDCTL, brdctl);
	ahc_flush_device_writes(ahc);
	if ((ahc->features & AHC_ULTRA2) != 0)
		brdctl |= BRDSTB_ULTRA2;
	else
		brdctl &= ~BRDSTB;
	ahc_outb(ahc, BRDCTL, brdctl);
	ahc_flush_device_writes(ahc);
	if ((ahc->features & AHC_ULTRA2) != 0)
		brdctl = 0;
	else
		brdctl &= ~BRDCS;
	ahc_outb(ahc, BRDCTL, brdctl);
}

static u_int8_t
read_brdctl(ahc)
	struct   ahc_softc *ahc;
{
	u_int8_t brdctl;
	u_int8_t value;

	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7895) {
		brdctl = BRDRW;
		if (ahc->channel == 'B')
			brdctl |= BRDCS;
	} else if ((ahc->features & AHC_ULTRA2) != 0) {
		brdctl = BRDRW_ULTRA2;
	} else {
		brdctl = BRDRW|BRDCS;
	}
	ahc_outb(ahc, BRDCTL, brdctl);
	ahc_flush_device_writes(ahc);
	value = ahc_inb(ahc, BRDCTL);
	ahc_outb(ahc, BRDCTL, 0);
	return (value);
}

static int
acquire_seeprom(ahc, sd)
	struct ahc_softc *ahc;
	struct seeprom_descriptor *sd;
{
	int wait;

	if ((ahc->features & AHC_SPIOCAP) != 0
		 && (ahc_inb(ahc, SPIOCAP) & SEEPROM) == 0)
		return (0);

	/*
	 * Request access of the memory port.  When access is
	 * granted, SEERDY will go high.  We use a 1 second
	 * timeout which should be near 1 second more than
	 * is needed.  Reason: after the chip reset, there
	 * should be no contention.
	 */
	SEEPROM_OUTB(sd, sd->sd_MS);
	wait = 1000;  /* 1 second timeout in msec */
	while (--wait && ((SEEPROM_STATUS_INB(sd) & sd->sd_RDY) == 0)) {
		DELAY(1000);  /* delay 1 msec */
	}
	if ((SEEPROM_STATUS_INB(sd) & sd->sd_RDY) == 0) {
		SEEPROM_OUTB(sd, 0); 
		return (0);
	}
	return (1);
}

static void
release_seeprom(sd)
	struct seeprom_descriptor *sd;
{
	/* Release access to the memory port and the serial EEPROM. */
	SEEPROM_OUTB(sd, 0);
}

#define DPE	PCI_STATUS_PARITY_DETECT
#define SSE	PCI_STATUS_SPECIAL_ERROR
#define RMA	PCI_STATUS_MASTER_ABORT
#define RTA	PCI_STATUS_MASTER_TARGET_ABORT
#define STA	PCI_STATUS_TARGET_TARGET_ABORT
#define DPR	PCI_STATUS_PARITY_ERROR

#define PCIDEBUG
#ifdef	PCIDEBUG
#define	PCI_PRINT(Printstuff) printf Printstuff
#else
#define	PCI_PRINT(Printstuff)
#endif

void
ahc_pci_intr(ahc)
	struct ahc_softc *ahc;
{
	pcireg_t status1;

	if ((ahc_inb(ahc, ERROR) & PCIERRSTAT) == 0)
		return;
  	PCI_PRINT(("%s: PCI error Interrupt at seqaddr = 0x%x\n",
		   ahc_name(ahc), 
		   ahc_inb(ahc, SEQADDR0) | (ahc_inb(ahc, SEQADDR1) << 8)));
 
	status1 = ahc_pci_read_config(ahc->dev_softc, PCI_COMMAND_STATUS_REG, 4);

/* define AHC_SHOW_PCI_ERRORS to get painful errors on your i386 console */
#ifdef AHC_SHOW_PCI_ERRORS
	if (status1 & DPE) {
		PCI_PRINT(("%s: Data Parity Error Detected during address "
			   "or write data phase\n", ahc_name(ahc)));
	}
#endif
	if (status1 & SSE) {
		PCI_PRINT(("%s: Signal System Error Detected\n", ahc_name(ahc)));
	}
	if (status1 & RMA) {
		PCI_PRINT(("%s: Received a Master Abort\n", ahc_name(ahc)));
	}
	if (status1 & RTA) {
		PCI_PRINT(("%s: Received a Target Abort\n", ahc_name(ahc)));
	}
	if (status1 & STA) {
		PCI_PRINT(("%s: Signaled a Target Abort\n", ahc_name(ahc)));
	}
	if (status1 & DPR) {
		PCI_PRINT(("%s: Data Parity Error has been reported via PERR#\n",
			   ahc_name(ahc)));
	}
	
	ahc_pci_write_config(ahc->dev_softc, PCI_COMMAND_STATUS_REG, status1, 4);

	if ((status1 & (DPE|SSE|RMA|RTA|STA|DPR)) == 0) {
		printf("%s: Latched PCIERR interrupt with "
		       "no status bits set\n", ahc_name(ahc)); 
	} else {
		ahc_outb(ahc, CLRINT, CLRPARERR);
	}
	
	ahc_unpause(ahc);

	return;
}
