/*	$OpenBSD: ahc_pci.c,v 1.13 2000/03/22 04:01:07 smurph Exp $	*/
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
#include <dev/ic/aic7xxxvar.h>
#include <dev/ic/smc93cx6var.h>

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

#define	DEVCONFIG		0x40
#define		SCBSIZE32	0x00010000UL	/* aic789X only */
#define		MPORTMODE	0x00000400UL	/* aic7870 only */
#define		RAMPSM		0x00000200UL	/* aic7870 only */
#define		VOLSENSE	0x00000100UL
#define		SCBRAMSEL	0x00000080UL
#define		PCI64		0x00000080UL	/* aic7891 & aic7897 only */
#define		MRDCEN		0x00000040UL
#define		EXTSCBTIME	0x00000020UL	/* aic7870 only */
#define		EXTSCBPEN	0x00000010UL	/* aic7870 & aic7890 only */
#define		BERREN		0x00000008UL
#define		DACEN		0x00000004UL
#define		STPWLEVEL	0x00000002UL
#define		DIFACTNEGEN	0x00000001UL	/* aic7870 only */

#define	CSIZE_LATTIME		0x0c
#define		CACHESIZE	0x0000003ful	/* only 5 bits */
#define		LATTIME		0x0000ff00ul

int	    ahc_pci_intr __P((struct ahc_softc *ahc));
static int  ahc_ext_scbram_present __P((struct ahc_softc *ahc));
static void ahc_ext_scbram_config __P((struct ahc_softc *ahc, int enable,
													int pcheck, int fast));
static void ahc_probe_ext_scbram __P((struct ahc_softc *ahc));
static void check_extport __P((struct ahc_softc *ahc, u_int *sxfrctl1));
static void configure_termination __P((struct ahc_softc *ahc,
				       struct seeprom_descriptor *sd,
				       u_int adapter_control,
				       u_int *sxfrctl1));
static void ahc_new_term_detect __P((struct ahc_softc *ahc,
				     int *enableSEC_low,
				     int *enableSEC_high,
				     int *enablePRI_low,
				     int *enablePRI_high,
				     int *eeprom_present));
static void aic787X_cable_detect __P((struct ahc_softc *ahc,
				      int *internal50_present,
				      int *internal68_present,
				      int *externalcable_present,
				      int *eeprom_present));
static void aic785X_cable_detect __P((struct ahc_softc *ahc,
				      int *internal50_present,
				      int *externalcable_present,
				      int *eeprom_present));
static void write_brdctl __P((struct ahc_softc *ahc, u_int8_t value));
static u_int8_t read_brdctl __P((struct ahc_softc *ahc));

void load_seeprom __P((struct ahc_softc *ahc));
static int acquire_seeprom __P((struct ahc_softc *ahc,
				struct seeprom_descriptor *sd));
static void release_seeprom __P((struct seeprom_descriptor *sd));
int ahc_probe_scbs __P((struct ahc_softc *ahc));

static u_char aic3940_count;

int ahc_pci_probe __P((struct device *, void *, void *));
void ahc_pci_attach __P((struct device *, struct device *, void *));

struct cfattach ahc_pci_ca = {
	sizeof(struct ahc_softc), ahc_pci_probe, ahc_pci_attach
};

struct ahc_pci_data {
	pci_chipset_tag_t pc;
	pcitag_t tag;
	u_int function;
};

int
ahc_pci_probe(parent, match, aux)
struct device *parent;
void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ADP:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADP_AIC7810:
		case PCI_PRODUCT_ADP_AIC7850:
		case PCI_PRODUCT_ADP_AIC7855:
		case PCI_PRODUCT_ADP_AIC5900:
		case PCI_PRODUCT_ADP_AIC5905:
		case PCI_PRODUCT_ADP_AIC7860:
		case PCI_PRODUCT_ADP_2940AU:
		case PCI_PRODUCT_ADP_AIC7870:
		case PCI_PRODUCT_ADP_2940:
		case PCI_PRODUCT_ADP_3940:
		case PCI_PRODUCT_ADP_3985:
		case PCI_PRODUCT_ADP_2944:
		case PCI_PRODUCT_ADP_AIC7880:
		case PCI_PRODUCT_ADP_2940U:
		case PCI_PRODUCT_ADP_3940U:
		case PCI_PRODUCT_ADP_398XU:
		case PCI_PRODUCT_ADP_2944U:
		case PCI_PRODUCT_ADP_2940UWPro:
		case PCI_PRODUCT_ADP_AIC6915:
		case PCI_PRODUCT_ADP_7895:
			return (1);
		}
			break;
	case PCI_VENDOR_ADP2:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADP2_2940U2:
		case PCI_PRODUCT_ADP2_AIC7890:
		case PCI_PRODUCT_ADP2_AIC7892:
		case PCI_PRODUCT_ADP2_29160:
		case PCI_PRODUCT_ADP2_19160B:
		case PCI_PRODUCT_ADP2_3950U2B:
		case PCI_PRODUCT_ADP2_3950U2D:
		case PCI_PRODUCT_ADP2_AIC7896:
		case PCI_PRODUCT_ADP2_AIC7899:
		case PCI_PRODUCT_ADP2_3960D:
			return (1);
		}
		break;
	}
	return (0);
}

void
ahc_pci_attach(parent, self, aux)
struct device *parent, *self;
void *aux;
{
	struct pci_attach_args *pa = aux;
	struct ahc_softc *ahc = (void *)self;
	bus_space_tag_t  iot;
	bus_space_handle_t ioh;
#ifdef AHC_ALLOW_MEMIO
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	int	memh_valid;
#endif
	pci_intr_handle_t ih;
	pcireg_t	   command;
	const char *intrstr;
	unsigned opri = 0;
	ahc_chip ahc_c = AHC_PCI; /* we are a PCI controller */
	ahc_flag ahc_flags = AHC_FNONE;
	ahc_feature ahc_f = AHC_FENONE;
	int	ioh_valid;

	u_char ultra_enb = 0;
	u_char our_id = 0;
	u_char channel = 'A';
	u_int    sxfrctl1;
	u_int    scsiseq;
	/* So we can access PCI configuration space after init */
	struct ahc_pci_data *pd;

	ahc->sc_dmat = pa->pa_dmat;
	
	command = pci_conf_read(pa->pa_pc, pa->pa_tag,
				PCI_COMMAND_STATUS_REG);

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ADP:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADP_7895:
			{
				pcireg_t devconfig;
				channel = pa->pa_function == 1 ? 'B' : 'A';
				ahc_c |= AHC_AIC7895;
				/* The 'C' revision of the aic7895 
				   has a few additional features */
				if (PCI_REVISION(pa->pa_class) >= 4)
					ahc_f = AHC_AIC7895C_FE;
				else
					ahc_f = AHC_AIC7895_FE;
				ahc_flags |= AHC_NEWEEPROM_FMT;
				devconfig = pci_conf_read(pa->pa_pc, 
							  pa->pa_tag,
							  DEVCONFIG);
				devconfig &= ~SCBSIZE32;
				pci_conf_write(pa->pa_pc, pa->pa_tag,
					       DEVCONFIG, devconfig);
			}
			break;
	   case PCI_PRODUCT_ADP_3940U:
		case PCI_PRODUCT_ADP_3940:
			if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_3940U) {
				ahc_c |= AHC_AIC7880;
				ahc_f = AHC_AIC7880_FE;
			} else {
				ahc_c |= AHC_AIC7870;
				ahc_f = AHC_AIC7870_FE;
			}
			aic3940_count++;
			if (!(aic3940_count & 0x01))
				/* Even count implies second channel */
				channel = 'B';
			break;
		case PCI_PRODUCT_ADP_2940UWPro:
			ahc_c |= AHC_AIC7880;
			ahc_f = AHC_AIC7880_FE;
			ahc_f |= AHC_INT50_SPEEDFLEX;
		case PCI_PRODUCT_ADP_2944U:
		case PCI_PRODUCT_ADP_2940U:
			ahc_c |= AHC_AIC7880;
			ahc_f = AHC_AIC7880_FE;
			break;
		case PCI_PRODUCT_ADP_2944:
		case PCI_PRODUCT_ADP_2940:
			ahc_c |= AHC_AIC7870;
			ahc_f = AHC_AIC7870_FE;
			break;
		case PCI_PRODUCT_ADP_2940AU:
			ahc_c |= AHC_AIC7860;
			ahc_f = AHC_AIC7860_FE;
			break;
		case PCI_PRODUCT_ADP_398XU:	/* XXX */
		case PCI_PRODUCT_ADP_AIC7880:
			ahc_c |= AHC_AIC7880;
			ahc_f = AHC_AIC7880_FE;
			break;
		case PCI_PRODUCT_ADP_AIC7870:
			ahc_c |= AHC_AIC7870;
			ahc_f = AHC_AIC7870_FE;
			break;
		case PCI_PRODUCT_ADP_AIC7860:
			ahc_c |= AHC_AIC7860;
			ahc_f = AHC_AIC7860_FE;
			break;
		case PCI_PRODUCT_ADP_AIC7855:
		case PCI_PRODUCT_ADP_AIC7850:
			ahc_c |= AHC_AIC7850;
			ahc_f = AHC_AIC7850_FE;
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
			ahc_c |= AHC_AIC7890;
			ahc_f = AHC_AIC7890_FE;
			ahc_flags |= AHC_NEWEEPROM_FMT;
			break;
		case PCI_PRODUCT_ADP2_AIC7892:
		case PCI_PRODUCT_ADP2_29160:
		case PCI_PRODUCT_ADP2_19160B:
			ahc_c |= AHC_AIC7892;
			ahc_f = AHC_AIC7892_FE;
			ahc_flags |= AHC_NEWEEPROM_FMT;
			break;
		case PCI_PRODUCT_ADP2_3950U2B:
		case PCI_PRODUCT_ADP2_3950U2D:
		case PCI_PRODUCT_ADP2_AIC7896:
			{
				pcireg_t devconfig;
				channel = pa->pa_function == 1 ? 'B' : 'A';
				ahc_c |= AHC_AIC7896;
				ahc_f = AHC_AIC7896_FE;
				ahc_flags |= AHC_NEWEEPROM_FMT;
				devconfig = pci_conf_read(pa->pa_pc, 
							  pa->pa_tag, 
							  DEVCONFIG);
				/* turn off 64 bit for now XXX smurph */
				devconfig &= ~PCI64;
				pci_conf_write(pa->pa_pc, pa->pa_tag, 
					       DEVCONFIG, devconfig);
			}
			break;
		case PCI_PRODUCT_ADP2_AIC7899:
		case PCI_PRODUCT_ADP2_3960D:
			ahc_c |= AHC_AIC7899;
			ahc_f = AHC_AIC7899_FE;
			ahc_flags |= AHC_NEWEEPROM_FMT;
			break;
		default:
			/* TTT */
		}
	}
	
#ifdef AHC_ALLOW_MEMIO
	memh_valid = (pci_mapreg_map(pa, AHC_PCI_MEMADDR,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL) == 0);
#endif
	ioh_valid = (pci_mapreg_map(pa, AHC_PCI_IOADDR,
	    PCI_MAPREG_TYPE_IO, 0, &iot, &ioh, NULL, NULL) == 0);

	if (ioh_valid) {
		/* do nothing */
#ifdef AHC_ALLOW_MEMIO
	} else if (memh_valid) {
		/* do nothing */
#endif
	} else {
		/* error out */
		printf(": unable to map registers\n");
		return;
	}
	
	/* Ensure busmastering is enabled */
	command |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);

	pd = malloc(sizeof (struct ahc_pci_data), M_DEVBUF, M_NOWAIT);
	if (pd == NULL) {
		printf(": error allocating pci data\n");
		return;
	}

	pd->pc = pa->pa_pc;
	pd->tag = pa->pa_tag;
	pd->function = pa->pa_function;

	/* setup the PCI stuff */
	ahc->pci_data = pd;
	ahc->pci_intr_func = ahc_pci_intr;


	/* On all PCI adapters, we allow SCB paging */
	ahc_flags |= AHC_PAGESCBS;

	ahc_construct(ahc, pa->pa_iot, ioh, ahc_c, ahc_flags, ahc_f, channel);
	/* Now we can use the ahc_inb and ahc_outb macros */

	/* setup the PCI error interrupt handler */
	ahc->pci_intr_func = &ahc_pci_intr;
	
	/* Remeber how the card was setup in case there is no SEEPROM */
	ahc_outb(ahc, HCNTRL, ahc->pause);
	if ((ahc->features & AHC_ULTRA2) != 0)
		our_id = ahc_inb(ahc, SCSIID_ULTRA2) & OID;
	else
		our_id = ahc_inb(ahc, SCSIID) & OID;
	sxfrctl1 = ahc_inb(ahc, SXFRCTL1) & STPWEN;
	scsiseq = ahc_inb(ahc, SCSISEQ);
	
	if (ahc_reset(ahc) != 0) {
		/* Failed */
		ahc_free(ahc);
		return;
	}
	
	if (ahc->features & AHC_ULTRA)
		ultra_enb = bus_space_read_1(pa->pa_iot, ioh, SXFRCTL0) &
						FAST20;
	
	if ((ahc->features & AHC_DT) != 0) {
		u_int optionmode;
		u_int sfunct;

		/* Perform ALT-Mode Setup */
		sfunct = ahc_inb(ahc, SFUNCT) & ~ALT_MODE;
		ahc_outb(ahc, SFUNCT, sfunct | ALT_MODE);
		optionmode = ahc_inb(ahc, OPTIONMODE);
		printf("OptionMode = %x\n", optionmode);
		ahc_outb(ahc, OPTIONMODE, OPTIONMODE_DEFAULTS);
		/* Send CRC info in target mode every 4K */
		ahc_outb(ahc, TARGCRCCNT, 0);
		ahc_outb(ahc, TARGCRCCNT + 1, 0x10);
		ahc_outb(ahc, SFUNCT, sfunct);

		/* Normal mode setup */
		ahc_outb(ahc, CRCCONTROL1, CRCVALCHKEN|CRCENDCHKEN|CRCREQCHKEN
					|TARGCRCENDEN|TARGCRCCNTEN);
	}

	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", ahc->sc_dev.dv_xname);
		ahc_free(ahc);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	ahc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ahc_intr, ahc,
                                        ahc->sc_dev.dv_xname);

	if (ahc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		ahc_free(ahc);
		return;
	}
	if (intrstr != NULL)
		printf(": %s\n", intrstr);

	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splbio();

	/*
	 * Do aic7880/aic7870/aic7860/aic7850 specific initialization
	 */
	{
		u_int8_t sblkctl;
		u_int dscommand0;

		dscommand0 = ahc_inb(ahc, DSCOMMAND0);
		dscommand0 |= MPARCKEN;
		if ((ahc->features & AHC_ULTRA2) != 0) {

			/*
			 * DPARCKEN doesn't work correctly on
			 * some MBs so don't use it.
			 */
			dscommand0 &= ~(USCBSIZE32|DPARCKEN);
			dscommand0 |= CACHETHEN;
		}

		ahc_outb(ahc, DSCOMMAND0, dscommand0);

		/* See if we have an SEEPROM and perform auto-term */
		check_extport(ahc, &sxfrctl1);

		/*
		 * Take the LED out of diagnostic mode
		 */
		sblkctl = ahc_inb(ahc, SBLKCTL);
		ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

		/*
		 * I don't know where this is set in the SEEPROM or by the
		 * BIOS, so we default to 100% on Ultra or slower controllers
		 * and 75% on ULTRA2 controllers.
		 */
		if ((ahc->features & AHC_ULTRA2) != 0) {
			ahc_outb(ahc, DFF_THRSH, RD_DFTHRSH_75|WR_DFTHRSH_75);
		} else {
			ahc_outb(ahc, DSPCISTATUS, DFTHRSH_100);
		}

		if (ahc->flags & AHC_USEDEFAULTS) {
			/*
			 * PCI Adapter default setup
			 * Should only be used if the adapter does not have
			 * an SEEPROM.
			 */
			/* See if someone else set us up already */
			if (scsiseq != 0) {
				printf("%s: Using left over BIOS settings\n",
						 ahc_name(ahc));
				ahc->flags &= ~AHC_USEDEFAULTS;
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
	}

	/*
	 * Take a look to see if we have external SRAM.
	 * We currently do not attempt to use SRAM that is
	 * shared among multiple controllers.
	 */
	ahc_probe_ext_scbram(ahc);

	printf("%s: ", ahc_name(ahc));

	/*
	 * Record our termination setting for the
	 * generic initialization routine.
	 */
	if ((sxfrctl1 & STPWEN) != 0)
		ahc->flags |= AHC_TERM_ENB_A;

	if (ahc_init(ahc)) {
		ahc_free(ahc);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}
	splx(opri);

	ahc_attach(ahc);
}

/*
 * Test for the presense of external sram in an
 * "unshared" configuration.
 */
static int
ahc_ext_scbram_present(ahc)
struct ahc_softc *ahc;
{
	int ramps;
	int single_user;
	pcireg_t devconfig;
	struct ahc_pci_data *pd = ahc->pci_data;

	devconfig = pci_conf_read(pd->pc, pd->tag, DEVCONFIG);
	single_user = (devconfig & MPORTMODE) != 0;

	if ((ahc->features & AHC_ULTRA2) != 0)
		ramps = (ahc_inb(ahc, DSCOMMAND0) & RAMPS) != 0;
	else if ((ahc->chip & AHC_CHIPID_MASK) >= AHC_AIC7870)
		ramps = (devconfig & RAMPSM) != 0;
	else
		ramps	= 0;

	if (ramps && single_user)
		return (1);
	return (0);
}

/*
 * Enable external scbram.
 */
static void
ahc_ext_scbram_config(ahc, enable, pcheck, fast)
struct ahc_softc *ahc;
int enable;
int pcheck;
int fast;
{
	pcireg_t devconfig;
	struct ahc_pci_data *pd = ahc->pci_data;

	if (ahc->features & AHC_MULTI_FUNC) {
		u_char channel; 
		/*
		 * Set the SCB Base addr (highest address bit)
		 * depending on which channel we are.
		 */
		channel = pd->function == 1 ? 1 : 0;
		ahc_outb(ahc, SCBBADDR, channel);
	}

	devconfig = pci_conf_read(pd->pc, pd->tag, DEVCONFIG);
	
	if ((ahc->features & AHC_ULTRA2) != 0) {
		u_int dscommand0;

		dscommand0 = ahc_inb(ahc, DSCOMMAND0);
		if (enable)
			dscommand0 &= ~INTSCBRAMSEL;
		else
			dscommand0 |= INTSCBRAMSEL;
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
	}
	if (pcheck)
		devconfig |= EXTSCBPEN;
	else
		devconfig &= ~EXTSCBPEN;

	pci_conf_write(pd->pc, pd->tag, DEVCONFIG, devconfig);
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

	if (ahc_ext_scbram_present(ahc) == 0)
		return;

	/*
	 * Probe for the best parameters to use.
	 */
	enable = 0;
	pcheck = 0;
	fast = 0;
	ahc_ext_scbram_config(ahc,	/*enable*/1, pcheck, fast);
	num_scbs = ahc_probe_scbs(ahc);
	if (num_scbs == 0) {
		/* The SRAM wasn't really present. */
		goto done;
	}
	enable = 1;

	/* Now see if we can do parity */
	ahc_ext_scbram_config(ahc, enable, /*pcheck*/1, fast);
	num_scbs = ahc_probe_scbs(ahc);
	if ((ahc_inb(ahc, INTSTAT) & BRKADRINT) == 0
		 || (ahc_inb(ahc, ERROR) & MPARERR) == 0)
		pcheck = 1;

	/* Clear any resulting parity error */
	ahc_outb(ahc, CLRINT, CLRPARERR);
	ahc_outb(ahc, CLRINT, CLRBRKADRINT);

	/* Now see if we can do fast timing */
	ahc_ext_scbram_config(ahc, enable, pcheck, /*fast*/1);
	test_num_scbs = ahc_probe_scbs(ahc);
	if (test_num_scbs == num_scbs
		 && ((ahc_inb(ahc, INTSTAT) & BRKADRINT) == 0
			  || (ahc_inb(ahc, ERROR) & MPARERR) == 0))
		fast = 1;

	done:
	/* Clear any resulting parity error */
	ahc_outb(ahc, CLRINT, CLRPARERR);
	ahc_outb(ahc, CLRINT, CLRBRKADRINT);
	if (bootverbose && enable) {
		printf("%s: External SRAM, %dns access%s\n",
				 ahc_name(ahc), fast ? 10 : 20,
				 pcheck ? ", parity checking enabled" : "");

	}
	ahc_ext_scbram_config(ahc, enable, pcheck, fast);
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
	struct     seeprom_descriptor sd;
	struct     seeprom_config sc;
	u_int   scsi_conf;
	u_int   adapter_control;
	int     have_seeprom;
	int     have_autoterm;

	sd.sd_tag = ahc->sc_iot;
	sd.sd_bsh = ahc->sc_ioh;
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
			bus_size_t start_addr;

			start_addr = 32 * (ahc->channel - 'A');

			have_seeprom = read_seeprom(&sd,
						    (u_int16_t *)&sc,
						    start_addr,
						    sizeof(sc)/2);

			if (have_seeprom) {
				/* Check checksum */
				int i;
				int maxaddr;
				u_int32_t checksum;
				u_int16_t *scarray;

				maxaddr = (sizeof(sc)/2) - 1;
				checksum = 0;
				scarray = (u_int16_t *)&sc;

				for (i = 0; i < maxaddr; i++)
					checksum = checksum + scarray[i];
				if (checksum == 0
				    || (checksum & 0xFFFF) != sc.checksum) {
					if (bootverbose && sd.sd_chip == C56_66)
						printf ("checksum error\n");
					have_seeprom = 0;
				} else {
					if (bootverbose)
						printf("done.\n");
					break;
				}
			}

			if (sd.sd_chip == C56_66)
				break;
			sd.sd_chip = C56_66;
		}
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
		u_int16_t discenable;
		u_int16_t ultraenb;

		discenable = 0;
		ultraenb = 0;
		if ((sc.adapter_control & CFULTRAEN) != 0) {
			/*
			 * Determine if this adapter has a "newstyle"
			 * SEEPROM format.
			 */
			for (i = 0; i < max_targ; i++) {
				if ((sc.device_flags[i] & CFSYNCHISULTRA) != 0) {
					ahc->flags |= AHC_NEWEEPROM_FMT;
					break;
				}
			}
		}

		for (i = 0; i < max_targ; i++) {
			u_int     scsirate;
			u_int16_t target_mask;

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

				scsirate = (sc.device_flags[i] & CFXFER)
				| ((ultraenb & target_mask) ? 0x8 : 0x0);
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

		if (sc.bios_control & CFEXTEND)
			ahc->flags |= AHC_EXTENDED_TRANS_A;
		if (ahc->features & AHC_ULTRA
			 && (ahc->flags & AHC_NEWEEPROM_FMT) == 0) {
			/* Should we enable Ultra mode? */
			if (!(sc.adapter_control & CFULTRAEN))
				/* Treat us as a non-ultra card */
				ultraenb = 0;
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
			have_autoterm = 1;
		else
			have_autoterm = 0;
	}

	if (have_autoterm)
		configure_termination(ahc, &sd, adapter_control, sxfrctl1);

	release_seeprom(&sd);
}

static void
configure_termination(ahc, sd, adapter_control, sxfrctl1)
struct ahc_softc *ahc;
struct seeprom_descriptor *sd;
u_int adapter_control;
u_int *sxfrctl1;
{
	u_int8_t brddat;

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
				enableSEC_low = (adapter_control & CFSTERM);
				enableSEC_high = (adapter_control & CFWSTERM);
			}
			if ((adapter_control & CFAUTOTERM) == 0) {
				if (bootverbose)
					printf("%s: Manual LVD Termination\n",
					       ahc_name(ahc));
				enablePRI_low = enablePRI_high =
					(adapter_control & CFLVDSTERM);
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

		if (bootverbose) {
			if ((ahc->features & AHC_ULTRA2) == 0) {
				printf("%s: internal 50 cable %s present, "
				       "internal 68 cable %s present\n",
				       ahc_name(ahc),
				       internal50_present ? "is":"not",
				       internal68_present ? "is":"not");

				printf("%s: external cable %s present\n",
				       ahc_name(ahc),
				       externalcable_present ? "is":"not");
			}
			printf("%s: BIOS eeprom %s present\n",
			       ahc_name(ahc), eeprom_present ? "is" : "not");
		}

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

		if (((internal50_present ? 1 : 0)
			  + (internal68_present ? 1 : 0)
			  + (externalcable_present ? 1 : 0)) <= 1
			 || (enableSEC_low != 0)) {
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
				       (ahc->features & AHC_ULTRA2)
				       ? "Primary " : "");
		}

		if ((adapter_control & CFWSTERM) != 0) {
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
	*internal50_present = !(brdctl & BRDDAT6);
	*internal68_present = !(brdctl & BRDDAT7);

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
	*externalcable_present = !(brdctl & BRDDAT6);
	*eeprom_present = brdctl & BRDDAT7;
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
	*internal50_present = !(brdctl & BRDDAT5);
	*externalcable_present = !(brdctl & BRDDAT6);

	*eeprom_present = (ahc_inb(ahc, SPIOCAP) & EEPROM) != 0;
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
	DELAY(20);
	brdctl |= value;
	ahc_outb(ahc, BRDCTL, brdctl);
	DELAY(20);
	if ((ahc->features & AHC_ULTRA2) != 0)
		brdctl |= BRDSTB_ULTRA2;
	else
		brdctl &= ~BRDSTB;
	ahc_outb(ahc, BRDCTL, brdctl);
	DELAY(20);
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
	DELAY(20);
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

int
ahc_pci_intr(ahc)
struct ahc_softc *ahc;
{
	pcireg_t status1;
	struct ahc_pci_data *pd = ahc->pci_data;
        
	if ((ahc_inb(ahc, ERROR) & PCIERRSTAT) == 0)
                return 0;

	status1 = pci_conf_read(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG);

	if (status1 & DPE) {
		printf("%s: Data Parity Error Detected during address "
				 "or write data phase\n", ahc_name(ahc));
	}
	if (status1 & SSE) {
		printf("%s: Signal System Error Detected\n", ahc_name(ahc));
	}
	if (status1 & RMA) {
		printf("%s: Received a Master Abort\n", ahc_name(ahc));
	}
	if (status1 & RTA) {
		printf("%s: Received a Target Abort\n", ahc_name(ahc));
	}
	if (status1 & STA) {
		printf("%s: Signaled a Target Abort\n", ahc_name(ahc));
	}
	if (status1 & DPR) {
		printf("%s: Data Parity Error has been reported via PERR#\n",
				 ahc_name(ahc));
	}
	if ((status1 & (DPE|SSE|RMA|RTA|STA|DPR)) == 0) {
		printf("%s: Latched PCIERR interrupt with "
				 "no status bits set\n", ahc_name(ahc)); 
	}
	pci_conf_write(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG, status1);

	if (status1 & (DPR|RMA|RTA)) {
		ahc_outb(ahc, CLRINT, CLRPARERR);
	}

        return 1;
}
