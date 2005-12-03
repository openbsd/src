/*	$OpenBSD: aha1742.c,v 1.21 2005/12/03 17:13:22 krw Exp $	*/
/*	$NetBSD: aha1742.c,v 1.61 1996/05/12 23:40:01 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1742.c)")
#endif /* ! DDB */

typedef u_long physaddr;
typedef u_long physlen;

#define KVTOPHYS(x)	kvtop((caddr_t)x)

#define AHB_ECB_MAX	32	/* store up to 32 ECBs at one time */
#define	ECB_HASH_SIZE	32	/* hash table size for phystokv */
#define	ECB_HASH_SHIFT	9
#define ECB_HASH(x)	((((long)(x))>>ECB_HASH_SHIFT) & (ECB_HASH_SIZE - 1))

#define	AHB_NSEG	33	/* number of dma segments supported */

/*
 * EISA registers (offset from slot base)
 */
#define	EISA_VENDOR		0x0c80	/* vendor ID (2 ports) */
#define	EISA_MODEL		0x0c82	/* model number (2 ports) */
#define	EISA_CONTROL		0x0c84
#define	 EISA_RESET		0x04
#define	 EISA_ERROR		0x02
#define	 EISA_ENABLE		0x01

/*
 * AHA1740 EISA board mode registers (Offset from slot base)
 */
#define PORTADDR	0xCC0
#define	 PORTADDR_ENHANCED	0x80
#define BIOSADDR	0xCC1
#define	INTDEF		0xCC2
#define	SCSIDEF		0xCC3
#define	BUSDEF		0xCC4
#define	RESV0		0xCC5
#define	RESV1		0xCC6
#define	RESV2		0xCC7
/**** bit definitions for INTDEF ****/
#define	INT9	0x00
#define	INT10	0x01
#define	INT11	0x02
#define	INT12	0x03
#define	INT14	0x05
#define	INT15	0x06
#define INTHIGH 0x08		/* int high=ACTIVE (else edge) */
#define	INTEN	0x10
/**** bit definitions for SCSIDEF ****/
#define	HSCSIID	0x0F		/* our SCSI ID */
#define	RSTPWR	0x10		/* reset scsi bus on power up or reset */
/**** bit definitions for BUSDEF ****/
#define	B0uS	0x00		/* give up bus immediatly */
#define	B4uS	0x01		/* delay 4uSec. */
#define	B8uS	0x02

/*
 * AHA1740 ENHANCED mode mailbox control regs (Offset from slot base)
 */
#define MBOXOUT0	0xCD0
#define MBOXOUT1	0xCD1
#define MBOXOUT2	0xCD2
#define MBOXOUT3	0xCD3

#define	ATTN		0xCD4
#define	G2CNTRL		0xCD5
#define	G2INTST		0xCD6
#define G2STAT		0xCD7

#define	MBOXIN0		0xCD8
#define	MBOXIN1		0xCD9
#define	MBOXIN2		0xCDA
#define	MBOXIN3		0xCDB

#define G2STAT2		0xCDC

/*
 * Bit definitions for the 5 control/status registers
 */
#define	ATTN_TARGET		0x0F
#define	ATTN_OPCODE		0xF0
#define  OP_IMMED		0x10
#define	  AHB_TARG_RESET	0x80
#define  OP_START_ECB		0x40
#define  OP_ABORT_ECB		0x50

#define	G2CNTRL_SET_HOST_READY	0x20
#define	G2CNTRL_CLEAR_EISA_INT	0x40
#define	G2CNTRL_HARD_RESET	0x80

#define	G2INTST_TARGET		0x0F
#define	G2INTST_INT_STAT	0xF0
#define	 AHB_ECB_OK		0x10
#define	 AHB_ECB_RECOVERED	0x50
#define	 AHB_HW_ERR		0x70
#define	 AHB_IMMED_OK		0xA0
#define	 AHB_ECB_ERR		0xC0
#define	 AHB_ASN		0xD0	/* for target mode */
#define	 AHB_IMMED_ERR		0xE0

#define	G2STAT_BUSY		0x01
#define	G2STAT_INT_PEND		0x02
#define	G2STAT_MBOX_EMPTY	0x04

#define	G2STAT2_HOST_READY	0x01

struct ahb_dma_seg {
	physaddr seg_addr;
	physlen seg_len;
};

struct ahb_ecb_status {
	u_short status;
#define	ST_DON	0x0001
#define	ST_DU	0x0002
#define	ST_QF	0x0008
#define	ST_SC	0x0010
#define	ST_DO	0x0020
#define	ST_CH	0x0040
#define	ST_INT	0x0080
#define	ST_ASA	0x0100
#define	ST_SNS	0x0200
#define	ST_INI	0x0800
#define	ST_ME	0x1000
#define	ST_ECA	0x4000
	u_char  host_stat;
#define	HS_OK			0x00
#define	HS_CMD_ABORTED_HOST	0x04
#define	HS_CMD_ABORTED_ADAPTER	0x05
#define	HS_TIMED_OUT		0x11
#define	HS_HARDWARE_ERR		0x20
#define	HS_SCSI_RESET_ADAPTER	0x22
#define	HS_SCSI_RESET_INCOMING	0x23
	u_char  target_stat;
	u_long  resid_count;
	u_long  resid_addr;
	u_short addit_status;
	u_char  sense_len;
	u_char  unused[9];
	u_char  cdb[6];
};

struct ahb_ecb {
	u_char  opcode;
#define	ECB_SCSI_OP	0x01
		u_char:4;
	u_char  options:3;
		u_char:1;
	short   opt1;
#define	ECB_CNE	0x0001
#define	ECB_DI	0x0080
#define	ECB_SES	0x0400
#define	ECB_S_G	0x1000
#define	ECB_DSB	0x4000
#define	ECB_ARS	0x8000
	short   opt2;
#define	ECB_LUN	0x0007
#define	ECB_TAG	0x0008
#define	ECB_TT	0x0030
#define	ECB_ND	0x0040
#define	ECB_DAT	0x0100
#define	ECB_DIR	0x0200
#define	ECB_ST	0x0400
#define	ECB_CHK	0x0800
#define	ECB_REC	0x4000
#define	ECB_NRB	0x8000
	u_short unused1;
	physaddr data_addr;
	physlen  data_length;
	physaddr status;
	physaddr link_addr;
	short   unused2;
	short   unused3;
	physaddr sense_ptr;
	u_char  req_sense_length;
	u_char  scsi_cmd_length;
	short   cksum;
	struct scsi_generic scsi_cmd;
	/*-----------------end of hardware supported fields----------------*/
	TAILQ_ENTRY(ahb_ecb) chain;
	struct ahb_ecb *nexthash;
	long hashkey;
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int flags;
#define ECB_FREE	0
#define ECB_ACTIVE	1
#define ECB_ABORTED	2
#define ECB_IMMED	4
#define ECB_IMMED_FAIL	8
	struct ahb_dma_seg ahb_dma[AHB_NSEG];
	struct ahb_ecb_status ecb_status;
	struct scsi_sense_data ecb_sense;
};

struct ahb_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	eisa_chipset_tag_t sc_ec;

	bus_space_handle_t sc_ioh;
	int sc_irq;
	void *sc_ih;

	struct ahb_ecb *immed_ecb;	/* an outstanding immediete command */
	struct ahb_ecb *ecbhash[ECB_HASH_SIZE];
	TAILQ_HEAD(, ahb_ecb) free_ecb;
	int numecbs;
	int ahb_scsi_dev;		/* our scsi id */
	struct scsi_link sc_link;
};

void ahb_send_mbox(struct ahb_softc *, int, struct ahb_ecb *);
int ahb_poll(struct ahb_softc *, struct scsi_xfer *, int);
void ahb_send_immed(struct ahb_softc *, int, u_long);
int ahbintr(void *);
void ahb_done(struct ahb_softc *, struct ahb_ecb *);
void ahb_free_ecb(struct ahb_softc *, struct ahb_ecb *, int);
struct ahb_ecb *ahb_get_ecb(struct ahb_softc *, int);
struct ahb_ecb *ahb_ecb_phys_kv(struct ahb_softc *, physaddr);
int ahb_find(bus_space_tag_t, bus_space_handle_t, struct ahb_softc *);
void ahb_init(struct ahb_softc *);
void ahbminphys(struct buf *);
int ahb_scsi_cmd(struct scsi_xfer *);
void ahb_timeout(void *);
void ahb_print_ecb(struct ahb_ecb *);
void ahb_print_active_ecb(struct ahb_softc *);
int ahbprint(void *, const char *);

#define	MAX_SLOTS	15
int     ahb_debug = 0;
#define AHB_SHOWECBS 0x01
#define AHB_SHOWINTS 0x02
#define AHB_SHOWCMDS 0x04
#define AHB_SHOWMISC 0x08

struct scsi_adapter ahb_switch = {
	ahb_scsi_cmd,
	ahbminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device ahb_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int	ahbmatch(struct device *, void *, void *);
void	ahbattach(struct device *, struct device *, void *);

struct cfattach ahb_ca = {
	sizeof(struct ahb_softc), ahbmatch, ahbattach
};

struct cfdriver ahb_cd = {
	NULL, "ahb", DV_DULL
};

/*
 * Function to send a command out through a mailbox
 */
void
ahb_send_mbox(sc, opcode, ecb)
	struct ahb_softc *sc;
	int opcode;
	struct ahb_ecb *ecb;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wait = 300;	/* 1ms should be enough */

	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) &
		    (G2STAT_BUSY | G2STAT_MBOX_EMPTY)) == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", sc->sc_dev.dv_xname);
		Debugger();
	}

	/* don't know this will work */
	bus_space_write_4(iot, ioh, MBOXOUT0, KVTOPHYS(ecb));
	bus_space_write_1(iot, ioh, ATTN, opcode | ecb->xs->sc_link->target);
}

/*
 * Function to poll for command completion when in poll mode
 */
int
ahb_poll(sc, xs, count)
	struct ahb_softc *sc;
	struct scsi_xfer *xs;
	int count;
{				/* in msec  */
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND)
			ahbintr(sc);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

/*
 * Function to  send an immediate type command to the adapter
 */
void
ahb_send_immed(sc, target, cmd)
	struct ahb_softc *sc;
	int target;
	u_long cmd;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wait = 100;	/* 1 ms enough? */

	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) &
		    (G2STAT_BUSY | G2STAT_MBOX_EMPTY)) == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", sc->sc_dev.dv_xname);
		Debugger();
	}

	/* don't know this will work */
	bus_space_write_4(iot, ioh, MBOXOUT0, cmd);
	bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_SET_HOST_READY);
	bus_space_write_1(iot, ioh, ATTN, OP_IMMED | target);
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
ahbmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	int rv;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "ADP0000") &&
	    strcmp(ea->ea_idstring, "ADP0001") &&
	    strcmp(ea->ea_idstring, "ADP0002") &&
	    strcmp(ea->ea_idstring, "ADP0400"))
		return (0);

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, 0,
	    &ioh))
		return (0);

#ifdef notyet
	/* This won't compile as-is, anyway. */
	bus_space_write_1(iot, ioh, EISA_CONTROL, EISA_ENABLE | EISA_RESET);
	delay(10);
	bus_space_write_1(iot, ioh, EISA_CONTROL, EISA_ENABLE);
	/* Wait for reset? */
	delay(1000);
#endif

	rv = !ahb_find(iot, ioh, NULL);

	bus_space_unmap(ea->ea_iot, ioh, EISA_SLOT_SIZE);

	return (rv);
}

int
ahbprint(aux, name)
	void *aux;
	const char *name;
{
	return UNCONF;
}

/*
 * Attach all the sub-devices we can find
 */
void
ahbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eisa_attach_args *ea = aux;
	struct ahb_softc *sc = (void *)self;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;

	sc->sc_iot = iot;
	sc->sc_ec = ec;

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, 0,
	    &ioh))
		panic("ahbattach: could not map I/O addresses");
	sc->sc_ioh = ioh;
	if (ahb_find(iot, ioh, sc))
		panic("ahbattach: ahb_find failed!");

	ahb_init(sc);
	TAILQ_INIT(&sc->free_ecb);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->ahb_scsi_dev;
	sc->sc_link.adapter = &ahb_switch;
	sc->sc_link.device = &ahb_dev;
	sc->sc_link.openings = 2;

	if (!strcmp(ea->ea_idstring, "ADP0000"))
		model = EISA_PRODUCT_ADP0000;
	else if (!strcmp(ea->ea_idstring, "ADP0001"))
		model = EISA_PRODUCT_ADP0001;
	else if (!strcmp(ea->ea_idstring, "ADP0002"))
		model = EISA_PRODUCT_ADP0002;
	else if (!strcmp(ea->ea_idstring, "ADP0400"))
		model = EISA_PRODUCT_ADP0400;
	else
		model = "unknown model!";
	printf(": <%s> ", model);

	if (eisa_intr_map(ec, sc->sc_irq, &ih)) {
		printf("%s: couldn't map interrupt (%d)\n",
		    sc->sc_dev.dv_xname, sc->sc_irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih);
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    ahbintr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s\n", intrstr);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_link, ahbprint);
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahbintr(arg)
	void *arg;
{
	struct ahb_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ahb_ecb *ecb;
	u_char ahbstat;
	u_long mboxval;

#ifdef	AHBDEBUG
	printf("%s: ahbintr ", sc->sc_dev.dv_xname);
#endif /* AHBDEBUG */

	if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) == 0)
		return 0;

	for (;;) {
		/*
		 * First get all the information and then
		 * acknowledge the interrupt
		 */
		ahbstat = bus_space_read_1(iot, ioh, G2INTST);
		mboxval = bus_space_read_4(iot, ioh, MBOXIN0);
		bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);

#ifdef	AHBDEBUG
		printf("status = 0x%x ", ahbstat);
#endif /*AHBDEBUG */

		/*
		 * Process the completed operation
		 */
		switch (ahbstat & G2INTST_INT_STAT) {
		case AHB_ECB_OK:
		case AHB_ECB_RECOVERED:
		case AHB_ECB_ERR:
			ecb = ahb_ecb_phys_kv(sc, mboxval);
			if (!ecb) {
				printf("%s: BAD ECB RETURNED!\n",
				    sc->sc_dev.dv_xname);
				continue;	/* whatever it was, it'll timeout */
			}
			break;

		case AHB_IMMED_ERR:
			ecb->flags |= ECB_IMMED_FAIL;
		case AHB_IMMED_OK:
			ecb = sc->immed_ecb;
			sc->immed_ecb = 0;
			break;

		default:
			printf("%s: unexpected interrupt %x\n",
			    sc->sc_dev.dv_xname, ahbstat);
			ecb = 0;
			break;
		}
		if (ecb) {
#ifdef	AHBDEBUG
			if (ahb_debug & AHB_SHOWCMDS)
				show_scsi_cmd(ecb->xs);
			if ((ahb_debug & AHB_SHOWECBS) && ecb)
				printf("<int ecb(%x)>", ecb);
#endif /*AHBDEBUG */
			timeout_del(&ecb->xs->stimeout);
			ahb_done(sc, ecb);
		}

		if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) ==
		    0)
			return 1;
	}
}

/*
 * We have a ecb which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
void
ahb_done(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{
	struct ahb_ecb_status *stat = &ecb->ecb_status;
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ecb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahb_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if (ecb->flags & ECB_IMMED) {
		if (ecb->flags & ECB_IMMED_FAIL)
			xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
	if (xs->error == XS_NOERROR) {
		if (stat->host_stat != HS_OK) {
			switch (stat->host_stat) {
			case HS_SCSI_RESET_ADAPTER:
				break;
			case HS_SCSI_RESET_INCOMING:
				break;
			case HS_CMD_ABORTED_HOST:
			case HS_CMD_ABORTED_ADAPTER:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case HS_TIMED_OUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    sc->sc_dev.dv_xname, stat->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (stat->target_stat != SCSI_OK) {
			switch (stat->target_stat) {
			case SCSI_CHECK:
				s1 = &ecb->ecb_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    sc->sc_dev.dv_xname, stat->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
done:
	xs->flags |= ITSDONE;
	ahb_free_ecb(sc, ecb, xs->flags);
	scsi_done(xs);
}

/*
 * A ecb (and hence a mbx-out is put onto the
 * free list.
 */
void
ahb_free_ecb(sc, ecb, flags)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
	int flags;
{
	int s;

	s = splbio();

	ecb->flags = ECB_FREE;
	TAILQ_INSERT_HEAD(&sc->free_ecb, ecb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (TAILQ_NEXT(ecb, chain) == NULL)
		wakeup(&sc->free_ecb);

	splx(s);
}

static inline void ahb_init_ecb(struct ahb_softc *, struct ahb_ecb *);

static inline void
ahb_init_ecb(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{
	int hashnum;

	bzero(ecb, sizeof(struct ahb_ecb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ecb->hashkey = KVTOPHYS(ecb);
	hashnum = ECB_HASH(ecb->hashkey);
	ecb->nexthash = sc->ecbhash[hashnum];
	sc->ecbhash[hashnum] = ecb;
}

static inline void ahb_reset_ecb(struct ahb_softc *, struct ahb_ecb *);

static inline void
ahb_reset_ecb(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{

}

/*
 * Get a free ecb
 *
 * If there are none, see if we can allocate a new one. If so, put it in the
 * hash table too otherwise either return an error or sleep.
 */
struct ahb_ecb *
ahb_get_ecb(sc, flags)
	struct ahb_softc *sc;
	int flags;
{
	struct ahb_ecb *ecb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		ecb = TAILQ_FIRST(&sc->free_ecb);
		if (ecb) {
			TAILQ_REMOVE(&sc->free_ecb, ecb, chain);
			break;
		}
		if (sc->numecbs < AHB_ECB_MAX) {
			ecb = (struct ahb_ecb *) malloc(sizeof(struct ahb_ecb),
			    M_TEMP, M_NOWAIT);
			if (ecb) {
				ahb_init_ecb(sc, ecb);
				sc->numecbs++;
			} else {
				printf("%s: can't malloc ecb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->free_ecb, PRIBIO, "ahbecb", 0);
	}

	ahb_reset_ecb(sc, ecb);
	ecb->flags = ECB_ACTIVE;

out:
	splx(s);
	return ecb;
}

/*
 * given a physical address, find the ecb that it corresponds to.
 */
struct ahb_ecb *
ahb_ecb_phys_kv(sc, ecb_phys)
	struct ahb_softc *sc;
	physaddr ecb_phys;
{
	int hashnum = ECB_HASH(ecb_phys);
	struct ahb_ecb *ecb = sc->ecbhash[hashnum];

	while (ecb) {
		if (ecb->hashkey == ecb_phys)
			break;
		ecb = ecb->nexthash;
	}
	return ecb;
}

/*
 * Start the board, ready for normal operation
 */
int
ahb_find(iot, ioh, sc)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ahb_softc *sc;
{
	u_char intdef;
	int i, irq, busid;
	int wait = 1000;	/* 1 sec enough? */

	bus_space_write_1(iot, ioh, PORTADDR, PORTADDR_ENHANCED);

#define	NO_NO 1
#ifdef NO_NO
	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */
	bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_HARD_RESET);
	delay(1000);
	bus_space_write_1(iot, ioh, G2CNTRL, 0);
	delay(10000);
	while (--wait) {
		if ((bus_space_read_1(iot, ioh, G2STAT) & G2STAT_BUSY) == 0)
			break;
		delay(1000);
	}
	if (!wait) {
#ifdef AHBDEBUG
		if (ahb_debug & AHB_SHOWMISC)
			printf("ahb_find: No answer from aha1742 board\n");
#endif /*AHBDEBUG */
		return ENXIO;
	}
	i = bus_space_read_1(iot, ioh, MBOXIN0);
	if (i) {
		printf("self test failed, val = 0x%x\n", i);
		return EIO;
	}

	/* Set it again, just to be sure. */
	bus_space_write_1(iot, ioh, PORTADDR, PORTADDR_ENHANCED);
#endif

	while (bus_space_read_1(iot, ioh, G2STAT) & G2STAT_INT_PEND) {
		printf(".");
		bus_space_write_1(iot, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		delay(10000);
	}

	intdef = bus_space_read_1(iot, ioh, INTDEF);
	switch (intdef & 0x07) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("illegal int setting %x\n", intdef);
		return EIO;
	}

	/* make sure we can interrupt */
	bus_space_write_1(iot, ioh, INTDEF, (intdef | INTEN));

	/* who are we on the scsi bus? */
	busid = (bus_space_read_1(iot, ioh, SCSIDEF) & HSCSIID);

	/* if we want to fill in softc, do so now */
	if (sc != NULL) {
		sc->sc_irq = irq;
		sc->ahb_scsi_dev = busid;
	}

	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

void
ahb_init(sc)
	struct ahb_softc *sc;
{

}

void
ahbminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((AHB_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((AHB_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
int
ahb_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct ahb_softc *sc = sc_link->adapter_softc;
	struct ahb_ecb *ecb;
	struct ahb_dma_seg *sg;
	int seg;		/* scatter gather seg being worked on */
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
#ifdef TFS
	struct iovec *iovp;
#endif
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("ahb_scsi_cmd\n"));
	/*
	 * get a ecb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (flags & ITSDONE) {
		printf("%s: done?\n", sc->sc_dev.dv_xname);
		xs->flags &= ~ITSDONE;
	}
	if ((ecb = ahb_get_ecb(sc, flags)) == NULL) {
		return TRY_AGAIN_LATER;
	}
	ecb->xs = xs;
	timeout_set(&ecb->xs->stimeout, ahb_timeout, ecb);

	/*
	 * If it's a reset, we need to do an 'immediate'
	 * command, and store its ecb for later
	 * if there is already an immediate waiting,
	 * then WE must wait
	 */
	if (flags & SCSI_RESET) {
		ecb->flags |= ECB_IMMED;
		if (sc->immed_ecb)
			return TRY_AGAIN_LATER;
		sc->immed_ecb = ecb;

		s = splbio();

		ahb_send_immed(sc, sc_link->target, AHB_TARG_RESET);

		if ((flags & SCSI_POLL) == 0) {
			splx(s);
			timeout_add(&ecb->xs->stimeout, (xs->timeout * hz) / 1000);
			return SUCCESSFULLY_QUEUED;
		}

		splx(s);

		/*
		 * If we can't use interrupts, poll on completion
		 */
		if (ahb_poll(sc, xs, xs->timeout))
			ahb_timeout(ecb);
		return COMPLETE;
	}

	/*
	 * Put all the arguments for the xfer in the ecb
	 */
	ecb->opcode = ECB_SCSI_OP;
	ecb->opt1 = ECB_SES | ECB_DSB | ECB_ARS;
	if (xs->datalen)
		ecb->opt1 |= ECB_S_G;
	ecb->opt2 = sc_link->lun | ECB_NRB;
	ecb->scsi_cmd_length = xs->cmdlen;
	ecb->sense_ptr = KVTOPHYS(&ecb->ecb_sense);
	ecb->req_sense_length = sizeof(ecb->ecb_sense);
	ecb->status = KVTOPHYS(&ecb->ecb_status);
	ecb->ecb_status.host_stat = 0x00;
	ecb->ecb_status.target_stat = 0x00;

	if (xs->datalen && (flags & SCSI_RESET) == 0) {
		ecb->data_addr = KVTOPHYS(ecb->ahb_dma);
		sg = ecb->ahb_dma;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < AHB_NSEG) {
				sg->seg_addr = (physaddr)iovp->iov_base;
				sg->seg_len = iovp->iov_len;
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		}
		else
#endif /*TFS */
		{
			/*
			 * Set up the scatter gather block
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (long) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < AHB_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

				SC_DEBUGN(sc_link, SDEV_DB4, ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the last, just extend the
					 * length
					 */
					/* how far to the end of the page */
					nextphys = (thisphys & ~PGOFSET) + NBPG;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page,
							      datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & ~PGOFSET) + NBPG;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				sg->seg_len = bytes_this_seg;
				sg++;
				seg++;
			}
		}
		/*end of iov/kv decision */
		ecb->data_length = seg * sizeof(struct ahb_dma_seg);
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: ahb_scsi_cmd, more than %d dma segs\n",
			    sc->sc_dev.dv_xname, AHB_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahb_free_ecb(sc, ecb, flags);
			return COMPLETE;
		}
	} else {	/* No data xfer, use non S/G values */
		ecb->data_addr = (physaddr)0;
		ecb->data_length = 0;
	}
	ecb->link_addr = (physaddr)0;

	/*
	 * Put the scsi command in the ecb and start it
	 */
	if ((flags & SCSI_RESET) == 0)
		bcopy(xs->cmd, &ecb->scsi_cmd, ecb->scsi_cmd_length);

	s = splbio();

	ahb_send_mbox(sc, OP_START_ECB, ecb);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if ((flags & SCSI_POLL) == 0) {
		splx(s);
		timeout_add(&ecb->xs->stimeout, (xs->timeout * hz) / 1000);
		return SUCCESSFULLY_QUEUED;
	}

	splx(s);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (ahb_poll(sc, xs, xs->timeout)) {
		ahb_timeout(ecb);
		if (ahb_poll(sc, xs, 2000))
			ahb_timeout(ecb);
	}
	return COMPLETE;
}

void
ahb_timeout(arg)
	void *arg;
{
	struct ahb_ecb *ecb = arg;
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ahb_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	if (ecb->flags & ECB_IMMED) {
		printf("\n");
		ecb->xs->retries = 0;	/* I MEAN IT ! */
		ecb->flags |= ECB_IMMED_FAIL;
		ahb_done(sc, ecb);
		splx(s);
		return;
	}

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ecb->flags == ECB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		ecb->xs->retries = 0;	/* I MEAN IT ! */
		ahb_done(sc, ecb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ecb->xs->error = XS_TIMEOUT;
		ecb->flags = ECB_ABORTED;
		ahb_send_mbox(sc, OP_ABORT_ECB, ecb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout_add(&ecb->xs->stimeout, 2 * hz);
	}

	splx(s);
}

#ifdef	AHBDEBUG
void
ahb_print_ecb(ecb)
	struct ahb_ecb *ecb;
{
	printf("ecb:%x op:%x cmdlen:%d senlen:%d\n",
		ecb, ecb->opcode, ecb->cdblen, ecb->senselen);
	printf("	datlen:%d hstat:%x tstat:%x flags:%x\n",
		ecb->datalen, ecb->ecb_status.host_stat,
		ecb->ecb_status.target_stat, ecb->flags);
	show_scsi_cmd(ecb->xs);
}

void
ahb_print_active_ecb(sc)
	struct ahb_softc *sc;
{
	struct ahb_ecb *ecb;
	int i = 0;

	while (i++ < ECB_HASH_SIZE) {
		ecb = sc->ecb_hash_list[i];
		while (ecb) {
			if (ecb->flags != ECB_FREE)
				ahb_print_ecb(ecb);
			ecb = ecb->hash_list;
		}
	}
}
#endif /* AHBDEBUG */
