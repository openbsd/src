/*
 * Product specific probe and attach routines for:
 * 	27/284X and aic7770 motherboard SCSI controllers
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
 *
 *	$Id: aic7770.c,v 1.1 1996/05/05 12:42:22 deraadt Exp $
 */

#if defined(__FreeBSD__)
#include <eisa.h>
#endif
#if NEISA > 0 || defined(__NetBSD__)

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__FreeBSD__)
#include <sys/devconf.h>
#endif
#include <sys/kernel.h>

#if defined(__NetBSD__)
#include <sys/device.h>
#if NetBSD1_1 < 3 /* NetBSD-1.1 */
#include <machine/pio.h>
#else
#include <machine/bus.h>
#ifdef __alpha__
#include <machine/intr.h>
#endif
#endif
#endif /* defined(__NetBSD__) */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if defined(__FreeBSD__)

#include <machine/clock.h>

#include <i386/eisa/eisaconf.h>
#include <i386/scsi/aic7xxx.h>
#include <dev/aic7xxx/aic7xxx_reg.h>

#define EISA_DEVICE_ID_ADAPTEC_AIC7770	0x04907770
#define EISA_DEVICE_ID_ADAPTEC_274x	0x04907771
#define EISA_DEVICE_ID_ADAPTEC_284xB	0x04907756 /* BIOS enabled */
#define EISA_DEVICE_ID_ADAPTEC_284x	0x04907757 /* BIOS disabled*/

#elif defined(__NetBSD__)

#if NetBSD1_1 < 3 /* NetBSD-1.1 */
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

/* Standard EISA Host ID regs  (Offset from slot base) */
#define HID0		0x80	/* 0,1: msb of ID2, 2-7: ID1      */
#define HID1		0x81	/* 0-4: ID3, 5-7: LSB ID2         */
#define HID2		0x82	/* product                        */
#define HID3		0x83	/* firmware revision              */

#define CHAR1(B1,B2) (((B1>>2) & 0x1F) | '@')
#define CHAR2(B1,B2) (((B1<<3) & 0x18) | ((B2>>5) & 0x7)|'@')
#define CHAR3(B1,B2) ((B2 & 0x1F) | '@')

#define	EISA_MAX_SLOTS	16	/* XXX should be defined in a common header */
static	ahc_slot = 0;		/* slot last board was found in */
#else
#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>
#endif

#include <dev/ic/aic7xxxvar.h>
#include <dev/microcode/aic7xxx/aic7xxx_reg.h>

#endif /* defined(__NetBSD__) */

#define AHC_EISA_SLOT_OFFSET	0xc00
#define AHC_EISA_IOSIZE		0x100
#define INTDEF			0x5cul	/* Interrupt Definition Register */

#if defined(__FreeBSD__)

static int	aic7770probe __P((void));
static int	aic7770_attach __P((struct eisa_device *e_dev));

static struct eisa_driver ahc_eisa_driver = {
					"ahc",
					aic7770probe,
					aic7770_attach,
					/*shutdown*/NULL,
					&ahc_unit
				      };

DATA_SET (eisadriver_set, ahc_eisa_driver);

static struct kern_devconf kdc_aic7770 = {
	0, 0, 0,                /* filled in by dev_attach */
	"ahc", 0, { MDDT_EISA, 0, "bio" },
	eisa_generic_externalize, 0, 0, EISA_EXTERNALLEN,
	&kdc_eisa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start out here */
	NULL,
	DC_CLS_MISC		/* host adapters aren't special */
};


static char	*aic7770_match __P((eisa_id_t type));

static  char*
aic7770_match(type)
	eisa_id_t type;
{
	switch(type) {
		case EISA_DEVICE_ID_ADAPTEC_AIC7770:
			return ("Adaptec aic7770 SCSI host adapter");
			break;
		case EISA_DEVICE_ID_ADAPTEC_274x:
			return ("Adaptec 274X SCSI host adapter");
			break;
		case EISA_DEVICE_ID_ADAPTEC_284xB:
		case EISA_DEVICE_ID_ADAPTEC_284x:
			return ("Adaptec 284X SCSI host adapter");
			break;
		default:
			break;
	}
	return (NULL);
}

static int
aic7770probe(void)
{
	u_long iobase;
	char intdef;
	u_long irq;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, aic7770_match))) {
		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + AHC_EISA_SLOT_OFFSET;
		ahc_reset(iobase);

		eisa_add_iospace(e_dev, iobase, AHC_EISA_IOSIZE, RESVADDR_NONE);
		intdef = inb(INTDEF + iobase);
		switch (intdef & 0xf) {
			case 9: 
				irq = 9;
				break;
			case 10:
				irq = 10;
				break;
			case 11:
				irq = 11;
				break;  
			case 12:
				irq = 12;
				break;
			case 14:
				irq = 14;
				break;
			case 15:
				irq = 15;
				break;
			default:
				printf("aic7770 at slot %d: illegal "
				       "irq setting %d\n", e_dev->ioconf.slot,
					intdef);
				continue;
		}
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahc_eisa_driver, &kdc_aic7770);
		if(e_dev->id == EISA_DEVICE_ID_ADAPTEC_284xB
		   || e_dev->id == EISA_DEVICE_ID_ADAPTEC_284x) {
			/* Our real parent is the isa bus.  Say so. */
			e_dev->kdc->kdc_parent = &kdc_isa0;
		}
		count++;
	}
	return count;
}

#elif defined(__NetBSD__)

#define bootverbose	1

int	ahe_irq __P((bus_chipset_tag_t, bus_io_handle_t));
int	ahematch __P((struct device *, void *, void *));
void	aheattach __P((struct device *, struct device *, void *));

#if NetBSD1_1 < 3 /* NetBSD-1.1 */

int	aheprobe1 __P((struct ahc_data *, struct isa_attach_args *, int));
int	aheprobe __P((struct device *, void *, void *));

struct cfdriver ahecd = {
        NULL, "ahe", aheprobe, aheattach, DV_DULL, 
        sizeof(struct ahc_data)
}; 

int
aheprobe1(ahc, ia, iobase)
	struct ahc_data *ahc;
	struct isa_attach_args *ia;
	int iobase;
{
	int i, irq;
	u_char intdef, sig_id[4];

	static struct {
		ahc_type type;
		u_int8_t id;
	} valid_ids[] = {
	/* Entries of other tested adaptors should be added here */
		{ AHC_AIC7770,	0x70 }, /*aic7770 on Motherboard*/
		{ AHC_274,	0x71 }, /*274x*/
		{ AHC_284,	0x56 }, /*284x, BIOS enabled*/
		{ AHC_284,	0x57 }  /*284x, BIOS disabled*/
	};

	for (i = 0; i < sizeof(sig_id); i++) {
		/*
		 * An outb is required to prime these
		 * registers on VL cards
		 */
		outb(iobase + HID0, HID0 + i);
		sig_id[i] = inb(iobase + HID0 + i);
	}
	if (sig_id[0] == 0xff)
		return (0);
	/* Check manufacturer's ID. */
	if (CHAR1(sig_id[0], sig_id[1]) != 'A' ||
	    CHAR2(sig_id[0], sig_id[1]) != 'D' ||
	    CHAR3(sig_id[0], sig_id[1]) != 'P' ||
	    sig_id[2] != 0x77)
		return (0);
	for (i = 0; i < sizeof(valid_ids)/sizeof(valid_ids[0]); i++) {
		if (sig_id[3] != valid_ids[i].id)
			continue;

		ahc_reset("ahe", 0, iobase);
		intdef = inb(INTDEF + iobase);
		switch (irq = (intdef & 0xf)) {
		case 9: 
		case 10:
		case 11:
		case 12:
		case 14:
		case 15:
			break;
		default:
			printf("%s: illegal irq setting %d\n",
			       ahc->sc_dev.dv_xname, intdef);
			return (0);
		}

		if (ia->ia_irq == IRQUNK) {
			ia->ia_irq = irq;
		} else if (ia->ia_irq != irq) {
			printf("%s: irq mismatch; kernel configured %d"
			       "!= board configured %d\n",
			       ahc->sc_dev.dv_xname, ia->ia_irq, irq);
			return (0);
		}

		ahc->type = valid_ids[i].type;

		ia->ia_msize = 0;
		ia->ia_iobase =	iobase;
		ia->ia_iosize = AHC_EISA_IOSIZE;
		return (1);
	}
	return (0);
}

int
aheprobe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
	struct isa_attach_args *ia = aux;
	struct ahc_data *ahc = match;

	if (ia->ia_iobase != IOBASEUNK)
		return aheprobe1(ahc, ia, ia->ia_iobase);

	ahc_slot++;
	for (; ahc_slot < EISA_MAX_SLOTS; ahc_slot++) {
		if (aheprobe1(ahc, ia,
			      0x1000 * ahc_slot + AHC_EISA_SLOT_OFFSET))
			return 1;
	}
	return (0);
}

#else

struct cfattach ahe_ca = {
	sizeof(struct ahc_data), ahematch, aheattach
};

struct cfdriver ahe_cd = {
	NULL, "ahe", DV_DULL
};

/*
 * Return irq setting of the board, otherwise -1.
 */
int
ahe_irq(bc, ioh)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
{
	int irq;
	u_char intdef;

	ahc_reset("ahe", bc, ioh);
	intdef = bus_io_read_1(bc, ioh, INTDEF);
	switch (irq = (intdef & 0xf)) {
	case 9:
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
		break;
	default:
		printf("ahe_irq: illegal irq setting %d\n", intdef);
		return -1;
	}

	/* Note that we are going and return (to probe) */
	return irq;
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
ahematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct eisa_attach_args *ea = aux;
	bus_io_handle_t ioh;
	int irq;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "ADP7770") &&
	    strcmp(ea->ea_idstring, "ADP7771") &&
	    strcmp(ea->ea_idstring, "ADP7756") && /* XXX - not EISA, but VL */
	    strcmp(ea->ea_idstring, "ADP7757"))	  /* XXX - not EISA, but VL */
		return (0);

#ifdef notyet
	if (bus_io_map(ea->ea_bc, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE,
		       &ioh))
		return (0);
	/* This won't compile as-is, anyway. */
	bus_io_write_1(ea->ea_bc, ioh, EISA_CONTROL, EISA_ENABLE | EISA_RESET);
	delay(10);
	bus_io_write_1(ea->ea_bc, ioh, EISA_CONTROL, EISA_ENABLE);
	/* Wait for reset? */
	delay(1000);
	bus_io_unmap(ea->ea_bc, ioh, EISA_SLOT_SIZE);
#endif

	if (bus_io_map(ea->ea_bc, 
		       EISA_SLOT_ADDR(ea->ea_slot) + AHC_EISA_SLOT_OFFSET, 
		       AHC_EISA_IOSIZE, &ioh))
		return (0);
	irq = ahe_irq(ea->ea_bc, ioh);
	bus_io_unmap(ea->ea_bc, ioh, EISA_SLOT_SIZE);
	return (irq >= 0);
}

#endif

#endif /* defined(__NetBSD__) */

#if defined(__FreeBSD__)
static int
aic7770_attach(e_dev)
	struct eisa_device *e_dev;
#elif defined(__NetBSD__)
void
aheattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
#endif
{
	ahc_type type;

#if defined(__FreeBSD__)
	struct ahc_data *ahc;
	resvaddr_t *iospace;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;

	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if(!iospace)
		return -1;

	switch(e_dev->id) {
		case EISA_DEVICE_ID_ADAPTEC_AIC7770:
			type = AHC_AIC7770;
			break;
		case EISA_DEVICE_ID_ADAPTEC_274x:
			type = AHC_274;
			break;          
		case EISA_DEVICE_ID_ADAPTEC_284xB:
		case EISA_DEVICE_ID_ADAPTEC_284x:
			type = AHC_284;
			break;
		default: 
			printf("aic7770_attach: Unknown device type!\n");
			return -1;
			break;
	}

	if(!(ahc = ahc_alloc(unit, iospace->addr, type, AHC_FNONE)))
		return -1;

	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, iospace)) {
		ahc_free(ahc);
		return -1;
	}

	/*
	 * The IRQMS bit enables level sensitive interrupts only allow
	 * IRQ sharing if its set.
	 */
	if(eisa_reg_intr(e_dev, irq, ahc_intr, (void *)ahc, &bio_imask,
			 /*shared ==*/ahc->pause & IRQMS)) {
		ahc_free(ahc);
		return -1;
	}
	eisa_reg_end(e_dev);

#elif defined(__NetBSD__)

	struct ahc_data *ahc = (void *)self;
	const char *model;
#if NetBSD1_1 < 3 /* NetBSD-1.1 */
	struct isa_attach_args *ia = aux;

	switch (ahc->type) {
	case AHC_AIC7770:
		model = "AIC-7770 SCSI (on motherboard)";
		break;
	case AHC_274:
		model = "AHA-274x SCSI";
		break;
	case AHC_284:
		model = "AHA-284x SCSI";
		break;
	default:
		panic("aheattach: Unknown device type 0x%x\n", ahc->type);
	}
	printf(": %s\n", model);

	ahc_construct(ahc, ahc->sc_dev.dv_unit, 0,
		      ia->ia_iobase, ahc->type, AHC_FNONE);
#else
	struct eisa_attach_args *ea = aux;
	bus_io_handle_t ioh;			/* XXX - ioh */
	eisa_intr_handle_t ih;
	const char *intrstr;
	int irq;

	if (bus_io_map(ea->ea_bc, 
		       EISA_SLOT_ADDR(ea->ea_slot) + AHC_EISA_SLOT_OFFSET, 
		       AHC_EISA_IOSIZE, &ioh))
		panic("aheattach: could not map I/O addresses");
	if ((irq = ahe_irq(ea->ea_bc, ioh)) < 0)
		panic("aheattach: ahe_irq failed!");

	if (strcmp(ea->ea_idstring, "ADP7770") == 0) {
		model = EISA_PRODUCT_ADP7770;
		type = AHC_AIC7770;
	} else if (strcmp(ea->ea_idstring, "ADP7771") == 0) {
		model = EISA_PRODUCT_ADP7771;
		type = AHC_274;
	} else if (strcmp(ea->ea_idstring, "ADP7756") == 0) {
		model = EISA_PRODUCT_ADP7756;
		type = AHC_284;
	} else if (strcmp(ea->ea_idstring, "ADP7757") == 0) {
		model = EISA_PRODUCT_ADP7757;
		type = AHC_284;
	} else {
		panic("aheattach: Unknown device type %s\n",
		      ea->ea_idstring);
	}
	printf(": %s\n", model);

	ahc_construct(ahc,
		      ahc->sc_dev.dv_unit, ea->ea_bc, ioh, type, AHC_FNONE);
	if (eisa_intr_map(ea->ea_ec, irq, &ih)) {
		printf("%s: couldn't map interrupt (%d)\n",
		       ahc->sc_dev.dv_xname, irq);
		return;
	}
#endif
#endif /* defined(__NetBSD__) */

	/*
	 * Tell the user what type of interrupts we're using.
	 * usefull for debugging irq problems
	 */
	if(bootverbose) {
		printf(
#if defined(__FreeBSD__)
		       "ahc%d: Using %s Interrupts\n",
		       unit,
#elif defined(__NetBSD__)
		       "%s: Using %s Interrupts\n",
		       ahc->sc_dev.dv_xname,
#endif
		       ahc->pause & IRQMS ?
				"Level Sensitive" : "Edge Triggered");
	}

	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 *
	 * First, the aic7770 card specific setup.
	 */
	switch( ahc->type ) {
	    case AHC_AIC7770:
	    {
		/* XXX
		 * It would be really nice to know if the BIOS
		 * was installed for the motherboard controllers,
		 * but I don't know how to yet.  Assume its enabled
		 * for now.
		 */
		break;
	    }
	    case AHC_274:
	    {
		if((AHC_INB(ahc, HA_274_BIOSCTRL) & BIOSMODE) == BIOSDISABLED)
			ahc->flags |= AHC_USEDEFAULTS;
		break;
	    }
	    case AHC_284:
	    {
		/* XXX
		 * All values are automagically intialized at
		 * POST for these cards, so we can always rely
		 * on the Scratch Ram values.  However, we should
		 * read the SEEPROM here (Dan has the code to do
		 * it) so we can say what kind of translation the
		 * BIOS is using.  Printing out the geometry could
		 * save a lot of users the grief of failed installs.
		 */
		break;
	    }
	    default:
		break;
	}

	/*      
	 * See if we have a Rev E or higher aic7770. Anything below a
	 * Rev E will have a R/O autoflush disable configuration bit.
	 * Its still not clear exactly what is differenent about the Rev E.
	 * We think it allows 8 bit entries in the QOUTFIFO to support
	 * "paging" SCBs so you can have more than 4 commands active at
	 * once.
	 */     
	{
		char *id_string;
		u_char sblkctl;
		u_char sblkctl_orig;

		sblkctl_orig = AHC_INB(ahc, SBLKCTL);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		AHC_OUTB(ahc, SBLKCTL, sblkctl);
		sblkctl = AHC_INB(ahc, SBLKCTL);
		if(sblkctl != sblkctl_orig)
		{
			id_string = "aic7770 >= Rev E, ";
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			AHC_OUTB(ahc, SBLKCTL, sblkctl);

			/* Allow paging on this adapter */
			ahc->flags |= AHC_PAGESCBS;
		}
		else
			id_string = "aic7770 <= Rev C, ";

#if defined(__FreeBSD__)
		printf("ahc%d: %s", unit, id_string);
#elif defined(__NetBSD__)
		printf("%s: %s", ahc->sc_dev.dv_xname, id_string);
#endif
	}

	/* Setup the FIFO threshold and the bus off time */
	if(ahc->flags & AHC_USEDEFAULTS) {
		AHC_OUTB(ahc, BUSSPD, DFTHRSH_100);
		AHC_OUTB(ahc, BUSTIME, BOFF_60BCLKS);
	}
	else {
		u_char hostconf = AHC_INB(ahc, HOSTCONF);
		AHC_OUTB(ahc, BUSSPD, hostconf & DFTHRSH);
		AHC_OUTB(ahc, BUSTIME, (hostconf << 2) & BOFF);
	}

	/*
	 * Generic aic7xxx initialization.
	 */
	if(ahc_init(ahc)){
#if defined(__FreeBSD__)
		ahc_free(ahc);
		/*
		 * The board's IRQ line is not yet enabled so its safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
#elif defined(__NetBSD__)
		ahc_free(ahc);
		return;
#endif
	}

	/*
	 * Enable the board's BUS drivers
	 */
	AHC_OUTB(ahc, BCTL, ENABLE);

#if defined(__FreeBSD__)
	/*
	 * Enable our interrupt handler.
	 */
	if(eisa_enable_intr(e_dev, irq)) {
		ahc_free(ahc);
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
	}

	e_dev->kdc->kdc_state = DC_BUSY; /* host adapters always busy */
#elif defined(__NetBSD__)
#if NetBSD1_1 < 3 /* NetBSD-1.1 */
	ahc->sc_ih = isa_intr_establish(ia->ia_irq,
		ahc->pause & IRQMS ? IST_LEVEL : IST_EDGE, ISA_IPL_BIO,
		ahc_intr, ahc);
#else
	intrstr = eisa_intr_string(ea->ea_ec, ih);
	/*
	 * The IRQMS bit enables level sensitive interrupts only allow
	 * IRQ sharing if its set.
	 */
#ifdef __OpenBSD__
	ahc->sc_ih = eisa_intr_establish(ea->ea_ec, ih,
		ahc->pause & IRQMS ? IST_LEVEL : IST_EDGE, IPL_BIO,
		ahc_intr, ahc, ahc->sc_dev.dv_xname);
#else
	ahc->sc_ih = eisa_intr_establish(ea->ea_ec, ih,
		ahc->pause & IRQMS ? IST_LEVEL : IST_EDGE, IPL_BIO,
		ahc_intr, ahc);
#endif
	if (ahc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		       ahc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		ahc_free(ahc);
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", ahc->sc_dev.dv_xname,
		       intrstr);
#endif
#endif /* defined(__NetBSD__) */

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);

#if defined(__FreeBSD__)
	return 0;
#endif
}

#endif /* NEISA > 0 */
