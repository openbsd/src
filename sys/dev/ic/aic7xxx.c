/*
 * Generic driver for the aic7xxx based adaptec SCSI controllers
 * Product specific probe and attach routines can be found in:
 * i386/eisa/ahc_eisa.c	27/284X and aic7770 motherboard controllers
 * pci/ahc_pci.c	3985, 3980, 3940, 2940, aic7895, aic7890,
 *			aic7880, aic7870, aic7860, and aic7850 controllers
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * the GNU Public License ("GPL").
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
 * $FreeBSD: src/sys/dev/aic7xxx/aic7xxx.c,v 1.40 2000/01/07 23:08:17 gibbs Exp $
 * $OpenBSD: aic7xxx.c,v 1.19 2000/03/22 02:48:47 smurph Exp $
 */
/*
 * A few notes on features of the driver.
 *
 * SCB paging takes advantage of the fact that devices stay disconnected
 * from the bus a relatively long time and that while they're disconnected,
 * having the SCBs for these transactions down on the host adapter is of
 * little use.  Instead of leaving this idle SCB down on the card we copy
 * it back up into kernel memory and reuse the SCB slot on the card to
 * schedule another transaction.  This can be a real payoff when doing random
 * I/O to tagged queueing devices since there are more transactions active at
 * once for the device to sort for optimal seek reduction. The algorithm goes
 * like this...
 *
 * The sequencer maintains two lists of its hardware SCBs.  The first is the
 * singly linked free list which tracks all SCBs that are not currently in
 * use.  The second is the doubly linked disconnected list which holds the
 * SCBs of transactions that are in the disconnected state sorted most
 * recently disconnected first.  When the kernel queues a transaction to
 * the card, a hardware SCB to "house" this transaction is retrieved from
 * either of these two lists.  If the SCB came from the disconnected list,
 * a check is made to see if any data transfer or SCB linking (more on linking
 * in a bit) information has been changed since it was copied from the host
 * and if so, DMAs the SCB back up before it can be used.  Once a hardware
 * SCB has been obtained, the SCB is DMAed from the host.  Before any work
 * can begin on this SCB, the sequencer must ensure that either the SCB is
 * for a tagged transaction or the target is not already working on another
 * non-tagged transaction.  If a conflict arises in the non-tagged case, the
 * sequencer finds the SCB for the active transactions and sets the SCB_LINKED
 * field in that SCB to this next SCB to execute.  To facilitate finding
 * active non-tagged SCBs, the last four bytes of up to the first four hardware
 * SCBs serve as a storage area for the currently active SCB ID for each
 * target.
 *
 * When a device reconnects, a search is made of the hardware SCBs to find
 * the SCB for this transaction.  If the search fails, a hardware SCB is
 * pulled from either the free or disconnected SCB list and the proper
 * SCB is DMAed from the host.  If the MK_MESSAGE control bit is set
 * in the control byte of the SCB while it was disconnected, the sequencer
 * will assert ATN and attempt to issue a message to the host.
 *
 * When a command completes, a check for non-zero status and residuals is
 * made.  If either of these conditions exists, the SCB is DMAed back up to
 * the host so that it can interpret this information.  Additionally, in the
 * case of bad status, the sequencer generates a special interrupt and pauses
 * itself.  This allows the host to setup a request sense command if it 
 * chooses for this target synchronously with the error so that sense
 * information isn't lost.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxxvar.h>
#include <dev/microcode/aic7xxx/aic7xxx_seq.h>
#include <dev/microcode/aic7xxx/sequencer.h>
#include "pci.h"

/*
 * Some ISA devices (e.g. on a VLB) can perform 32-bit DMA.  This
 * flag is passed to bus_dmamap_create() to indicate that fact.
 */
#ifndef ISABUS_DMA_32BIT
#define ISABUS_DMA_32BIT	BUS_DMA_BUS1
#endif 

#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif
#include <sys/kernel.h>
#define offsetof(s, e) ((char *)&((s *)0)->e - (char *)((s *)0))

#define	IS_SCSIBUS_B(ahc, sc_link)	\
	((sc_link)->scsibus == (ahc)->sc_link_b.scsibus)
#define ALL_CHANNELS '\0'
#define ALL_TARGETS_MASK 0xFFFF
#define INITIATOR_WILDCARD	(~0)

#define	SIM_IS_SCSIBUS_B(ahc, sc_link)	\
	((sc_link)->scsibus == (ahc)->sc_link_b.scsibus)
#define	SIM_CHANNEL(ahc, sc_link)	\
	(SIM_IS_SCSIBUS_B(ahc, sc_link) ? 'B' : 'A')
#define	SIM_SCSI_ID(ahc, sc_link)	\
	(SIM_IS_SCSIBUS_B(ahc, sc_link) ? ahc->our_id_b : ahc->our_id)
#define	SCB_IS_SCSIBUS_B(scb)	\
	(((scb)->hscb->tcl & SELBUSB) != 0)
#define	SCB_TARGET(scb)	\
	(((scb)->hscb->tcl & TID) >> 4)
#define	SCB_CHANNEL(scb) \
	(SCB_IS_SCSIBUS_B(scb) ? 'B' : 'A')
#define	SCB_LUN(scb)	\
	((scb)->hscb->tcl & LID)
#define SCB_TARGET_OFFSET(scb)		\
	(SCB_TARGET(scb) + (SCB_IS_SCSIBUS_B(scb) ? 8 : 0))
#define SCB_TARGET_MASK(scb)		\
	(0x01 << (SCB_TARGET_OFFSET(scb)))
#define TCL_CHANNEL(ahc, tcl)		\
	((((ahc)->features & AHC_TWIN) && ((tcl) & SELBUSB)) ? 'B' : 'A')
#define TCL_SCSI_ID(ahc, tcl)		\
	(TCL_CHANNEL((ahc), (tcl)) == 'B' ? (ahc)->our_id_b : (ahc)->our_id)
#define TCL_TARGET(tcl) (((tcl) & TID) >> TCL_TARGET_SHIFT)
#define TCL_LUN(tcl) ((tcl) & LID)

#define XS_TCL(ahc, xs) \
	((((xs)->sc_link->target << 4) & 0xF0) \
	| (SIM_IS_SCSIBUS_B((ahc), (xs)->sc_link) ? SELBUSB : 0) \
	| ((xs)->sc_link->lun & 0x07))

/*
 * Under normal circumstances, these messages are unnecessary
 * and not terribly cosmetic.
 */
#ifdef DEBUG
#define bootverbose	1
#define STATIC
#define INLINE
#else
#define bootverbose	0
#define STATIC	static
#define INLINE __inline
#endif

typedef enum {
	ROLE_UNKNOWN,
	ROLE_INITIATOR,
	ROLE_TARGET
} role_t;

struct ahc_devinfo {
	int	  our_scsiid;
	int	  target_offset;
	u_int16_t target_mask;
	u_int8_t  target;
	u_int8_t  lun;
	char	  channel;
	role_t	  role;		/*
				 * Only guaranteed to be correct if not
				 * in the busfree state.
				 */
};

typedef enum {
	SEARCH_COMPLETE,
	SEARCH_COUNT,
	SEARCH_REMOVE
} ahc_search_action;

#ifdef AHC_DEBUG
static int     ahc_debug = AHC_DEBUG;
#endif

#if NPCI > 0
void ahc_pci_intr(struct ahc_softc *ahc);
#endif

STATIC int	ahcinitscbdata(struct ahc_softc *ahc);
STATIC void	ahcfiniscbdata(struct ahc_softc *ahc);

STATIC int	ahc_poll __P((struct ahc_softc *ahc, int wait));
STATIC void	ahc_shutdown __P((void *arg));
STATIC int	ahc_execute_scb __P((void *arg, bus_dma_segment_t *dm_segs,
				     int nsegments));
STATIC int	ahc_setup_data __P((struct ahc_softc *ahc,
				    struct scsi_xfer *xs, struct scb *scb));
STATIC void	ahc_freeze_devq __P((struct ahc_softc *ahc,
				     struct scsi_link *sc_link));
STATIC void	ahcallocscbs __P((struct ahc_softc *ahc));
STATIC void	ahc_fetch_devinfo __P((struct ahc_softc *ahc,
				       struct ahc_devinfo *devinfo));
STATIC void	ahc_compile_devinfo __P((struct ahc_devinfo *devinfo,
					 u_int our_id, u_int target,
					 u_int lun, char channel,
					 role_t role));
STATIC u_int	ahc_abort_wscb __P((struct ahc_softc *ahc,
				    u_int scbpos, u_int prev));
STATIC void	ahc_done __P((struct ahc_softc *ahc, struct scb *scbp));
STATIC struct tmode_tstate *
		ahc_alloc_tstate __P((struct ahc_softc *ahc, u_int scsi_id,
				      char channel));
STATIC void	ahc_handle_seqint __P((struct ahc_softc *ahc, u_int intstat));
STATIC void	ahc_handle_scsiint __P((struct ahc_softc *ahc, u_int intstat));
STATIC void	ahc_build_transfer_msg __P((struct ahc_softc *ahc,
					    struct ahc_devinfo *devinfo));
STATIC void	ahc_setup_initiator_msgout __P((struct ahc_softc *ahc,
						struct ahc_devinfo *devinfo,
						struct scb *scb));
STATIC void	ahc_setup_target_msgin __P((struct ahc_softc *ahc,
					    struct ahc_devinfo *devinfo));
STATIC int	ahc_handle_msg_reject __P((struct ahc_softc *ahc,
					   struct ahc_devinfo *devinfo));
STATIC void	ahc_clear_msg_state __P((struct ahc_softc *ahc));
STATIC void	ahc_handle_message_phase __P((struct ahc_softc *ahc,
					      struct scsi_link *sc_link));
STATIC int	ahc_sent_msg __P((struct ahc_softc *ahc, u_int msgtype,
				  int full));

typedef enum {
	MSGLOOP_IN_PROG,
	MSGLOOP_MSGCOMPLETE,
	MSGLOOP_TERMINATED
} msg_loop_stat;

STATIC int	ahc_parse_msg __P((struct ahc_softc *ahc,
				   struct scsi_link *sc_link,
				   struct ahc_devinfo *devinfo));
STATIC void	ahc_handle_ign_wide_residue __P((struct ahc_softc *ahc,
						 struct ahc_devinfo *devinfo));
STATIC void	ahc_handle_devreset __P((struct ahc_softc *ahc,
					 struct ahc_devinfo *devinfo,
					 int status, char *message,
					 int verbose_level));
#ifdef AHC_DUMP_SEQ
STATIC void	ahc_dumpseq __P((struct ahc_softc *ahc));
#endif
STATIC void	ahc_loadseq __P((struct ahc_softc *ahc));
STATIC int	ahc_check_patch __P((struct ahc_softc *ahc,
				     struct patch **start_patch,
				     int start_instr, int *skip_addr));
STATIC void	ahc_download_instr __P((struct ahc_softc *ahc,
					int instrptr, u_int8_t *dconsts));
STATIC int	ahc_match_scb __P((struct scb *scb, int target, char channel,
				   int lun, u_int tag, role_t role));
#ifdef AHC_DEBUG
STATIC void	ahc_print_scb __P((struct scb *scb));
#endif
STATIC int	ahc_search_qinfifo __P((struct ahc_softc *ahc, int target,
					char channel, int lun, u_int tag,
					role_t role, u_int32_t status,
					ahc_search_action action));
STATIC int	ahc_reset_channel __P((struct ahc_softc *ahc, char channel,
				       int initiate_reset));
STATIC int	ahc_abort_scbs __P((struct ahc_softc *ahc, int target,
				    char channel, int lun, u_int tag,
				    role_t role, u_int32_t status));
STATIC int	ahc_search_disc_list __P((struct ahc_softc *ahc, int target,
					  char channel, int lun, u_int tag,
					  int stop_on_first, int remove,
					  int save_state));
STATIC u_int	ahc_rem_scb_from_disc_list __P((struct ahc_softc *ahc,
						u_int prev, u_int scbptr));
STATIC void	ahc_add_curscb_to_free_list __P((struct ahc_softc *ahc));
STATIC void	ahc_clear_intstat __P((struct ahc_softc *ahc));
STATIC void	ahc_reset_current_bus __P((struct ahc_softc *ahc));
STATIC struct ahc_syncrate *
		ahc_devlimited_syncrate __P((struct ahc_softc *ahc, u_int *period));
STATIC struct ahc_syncrate *
		ahc_find_syncrate __P((struct ahc_softc *ahc, u_int *period,
				       u_int maxsync));
STATIC u_int ahc_find_period __P((struct ahc_softc *ahc, u_int scsirate,
				  u_int maxsync));
STATIC void	ahc_validate_offset __P((struct ahc_softc *ahc,
					 struct ahc_syncrate *syncrate,
					 u_int *offset, int wide)); 
STATIC void	ahc_update_target_msg_request __P((struct ahc_softc *ahc,
					      struct ahc_devinfo *devinfo,
					      struct ahc_initiator_tinfo *tinfo,
					      int force, int paused));
STATIC void	ahc_set_syncrate __P((struct ahc_softc *ahc,
				      struct ahc_devinfo *devinfo,
				      struct ahc_syncrate *syncrate,
				      u_int period, u_int offset,
				      u_int type, int paused, int done));
STATIC void	ahc_set_width __P((struct ahc_softc *ahc,
			      struct ahc_devinfo *devinfo,
			      u_int width, u_int type, int paused, int done));
STATIC void	ahc_set_tags __P((struct ahc_softc *ahc,
				  struct ahc_devinfo *devinfo,int enable));
STATIC void	ahc_construct_sdtr __P((struct ahc_softc *ahc,
				   u_int period, u_int offset));
STATIC void	ahc_construct_wdtr __P((struct ahc_softc *ahc, u_int bus_width));

STATIC void	ahc_calc_residual __P((struct scb *scb));

STATIC void	ahc_update_pending_syncrates __P((struct ahc_softc *ahc));

STATIC void	ahc_set_recoveryscb __P((struct ahc_softc *ahc,
					 struct scb *scb));
STATIC void ahc_timeout __P((void *));

static __inline int  sequencer_paused __P((struct ahc_softc *ahc));
static __inline void pause_sequencer __P((struct ahc_softc *ahc));
static __inline void unpause_sequencer __P((struct ahc_softc *ahc));
STATIC void restart_sequencer __P((struct ahc_softc *ahc));
static __inline u_int ahc_index_busy_tcl __P((struct ahc_softc *ahc,
					      u_int tcl, int unbusy));
 
static __inline void	ahc_busy_tcl __P((struct ahc_softc *ahc,
					  struct scb *scb));
static __inline int	ahc_isbusy_tcl __P((struct ahc_softc *ahc,
					    struct scb *scb));
static __inline void ahc_freeze_ccb __P((struct scb* scb));
static __inline void ahcsetccbstatus __P((struct scsi_xfer *xs, int status));
STATIC void ahc_run_qoutfifo __P((struct ahc_softc *ahc));

static __inline struct ahc_initiator_tinfo *
	ahc_fetch_transinfo __P((struct ahc_softc *ahc, char channel,
				 u_int our_id, u_int target,
				 struct tmode_tstate **tstate));
STATIC void ahcfreescb __P((struct ahc_softc *ahc, struct scb *scb));
static __inline struct scb *ahcgetscb __P((struct ahc_softc *ahc));
int    ahc_createdmamem __P((struct ahc_softc *ahc, int size,
			     bus_dmamap_t *mapp, caddr_t *vaddr,
			     bus_addr_t *baddr, bus_dma_segment_t *segs,
			     int *nseg, const char *what));
STATIC void ahc_freedmamem __P((bus_dma_tag_t tag, int size,
				bus_dmamap_t map, caddr_t vaddr,
				bus_dma_segment_t *seg, int nseg));
STATIC void ahcminphys __P((struct buf *bp));

STATIC INLINE	struct scsi_xfer *ahc_first_xs __P((struct ahc_softc *));
STATIC INLINE	void   ahc_list_insert_before __P((struct ahc_softc *ahc,
						   struct scsi_xfer *xs,
						   struct scsi_xfer *next_xs));
STATIC INLINE	void   ahc_list_insert_head __P((struct ahc_softc *ahc,
						 struct scsi_xfer *xs));
STATIC INLINE	void   ahc_list_insert_tail __P((struct ahc_softc *ahc,
						 struct scsi_xfer *xs));
STATIC INLINE	void   ahc_list_remove __P((struct ahc_softc *ahc,
					    struct scsi_xfer *xs));
STATIC INLINE	struct scsi_xfer *ahc_list_next __P((struct ahc_softc *ahc,
						     struct scsi_xfer *xs));

STATIC int32_t ahc_scsi_cmd __P((struct scsi_xfer *xs));

struct cfdriver ahc_cd = {
	NULL, "ahc", DV_DULL
};

static struct scsi_adapter ahc_switch =
{
	ahc_scsi_cmd,
	ahcminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device ahc_dev =
{
	NULL, /* Use default error handler */
	NULL, /* have a queue, served by this */
	NULL, /* have no async handler */
	NULL, /* Use default 'done' routine */
};

STATIC void
ahcminphys(bp)
	struct buf *bp;
{
/*
 * Even though the card can transfer up to 16megs per command
 * we are limited by the number of segments in the dma segment
 * list that we can hold.  The worst case is that all pages are
 * discontinuous physically, hense the "page per segment" limit
 * enforced here.
 */
	if (bp->b_bcount > ((AHC_NSEG - 1) * PAGE_SIZE)) {
		bp->b_bcount = ((AHC_NSEG - 1) * PAGE_SIZE);
	}
	minphys(bp);
}


static __inline u_int32_t
ahc_hscb_busaddr(struct ahc_softc *ahc, u_int index)
{
	return (ahc->scb_data->hscb_busaddr
		+ (sizeof(struct hardware_scb) * index));
}

#define AHC_BUSRESET_DELAY	25	/* Reset delay in us */

static __inline int
sequencer_paused(ahc)
	struct ahc_softc *ahc;
{
	return ((ahc_inb(ahc, HCNTRL) & PAUSE) != 0);
}

static __inline void
pause_sequencer(ahc)
	struct ahc_softc *ahc;
{
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while (sequencer_paused(ahc) == 0)
		;
}

static __inline void
unpause_sequencer(ahc)
	struct ahc_softc *ahc;
{
	if ((ahc_inb(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0)
		ahc_outb(ahc, HCNTRL, ahc->unpause);
}

/*
 * Restart the sequencer program from address zero
 */
STATIC void
restart_sequencer(ahc)
	struct ahc_softc *ahc;
{
	u_int i;

	pause_sequencer(ahc);

	/*
	 * Everytime we restart the sequencer, there
	 * is the possiblitity that we have restarted
	 * within a three instruction window where an
	 * SCB has been marked free but has not made it
	 * onto the free list.  Since SCSI events(bus reset,
	 * unexpected bus free) will always freeze the
	 * sequencer, we cannot close this window.  To
	 * avoid losing an SCB, we reconsitute the free
	 * list every time we restart the sequencer.
	 */
	ahc_outb(ahc, FREE_SCBH, SCB_LIST_NULL);
	for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
		
		ahc_outb(ahc, SCBPTR, i);
		if (ahc_inb(ahc, SCB_TAG) == SCB_LIST_NULL)
			ahc_add_curscb_to_free_list(ahc);
	}
	ahc_outb(ahc, SEQCTL, FASTMODE|SEQRESET);
	unpause_sequencer(ahc);
}

static __inline u_int
ahc_index_busy_tcl(ahc, tcl, unbusy)
	struct ahc_softc *ahc;
	u_int tcl;
	int unbusy;
{
	u_int scbid;

	scbid = ahc->untagged_scbs[tcl];
	if (unbusy) {
		ahc->untagged_scbs[tcl] = SCB_LIST_NULL;
		bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
				BUS_DMASYNC_PREWRITE);
	}

	return (scbid);
}

static __inline void
ahc_busy_tcl(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	ahc->untagged_scbs[scb->hscb->tcl] = scb->hscb->tag;
	bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
			BUS_DMASYNC_PREWRITE);
}

static __inline int
ahc_isbusy_tcl(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	return ahc->untagged_scbs[scb->hscb->tcl] != SCB_LIST_NULL;
}

static __inline void
ahc_freeze_ccb(scb)
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	struct ahc_softc *ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;
	int target;

	target = xs->sc_link->target;
	if (!(scb->flags & SCB_FREEZE_QUEUE)) {
		ahc->devqueue_blocked[target]++;
		scb->flags |= SCB_FREEZE_QUEUE;
	}
}

static __inline void
ahcsetccbstatus(xs, status)
	struct scsi_xfer *xs;
	int status;
{
	xs->error = status;
}

static __inline struct ahc_initiator_tinfo *
ahc_fetch_transinfo(ahc, channel, our_id, remote_id, tstate)
	struct ahc_softc *ahc;
	char channel;
	u_int our_id;
	u_int remote_id;
	struct tmode_tstate **tstate;
{
	/*
	 * Transfer data structures are stored from the perspective
	 * of the target role.  Since the parameters for a connection
	 * in the initiator role to a given target are the same as
	 * when the roles are reversed, we pretend we are the target.
	 */
	if (channel == 'B')
		our_id += 8;
	*tstate = ahc->enabled_targets[our_id];
	return (&(*tstate)->transinfo[remote_id]);
}

STATIC void
ahc_run_qoutfifo(ahc)
	struct ahc_softc *ahc;
{
	struct scb *scb;
	u_int  scb_index;

	bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
			BUS_DMASYNC_POSTREAD);

	while (ahc->qoutfifo[ahc->qoutfifonext] != SCB_LIST_NULL) {
		scb_index = ahc->qoutfifo[ahc->qoutfifonext];
		ahc->qoutfifo[ahc->qoutfifonext++] = SCB_LIST_NULL;

		scb = &ahc->scb_data->scbarray[scb_index];
		if (scb_index >= ahc->scb_data->numscbs
		  || (scb->flags & SCB_ACTIVE) == 0) {
			printf("%s: WARNING no command for scb %d "
			       "(cmdcmplt)\nQOUTPOS = %d\n",
			       ahc_name(ahc), scb_index,
			       ahc->qoutfifonext - 1);
			continue;
		}

		/*
		 * Save off the residual
		 * if there is one.
		 */
		if (scb->hscb->residual_SG_count != 0)
			ahc_calc_residual(scb);
		else
			scb->xs->resid = 0;
		ahc_done(ahc, scb);
	}
}


/*
 * An scb (and hence an scb entry on the board) is put onto the
 * free list.
 */
STATIC void
ahcfreescb(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{       
	struct hardware_scb *hscb;
	int opri;

	hscb = scb->hscb;
	opri = splbio();

	if ((ahc->flags & AHC_RESOURCE_SHORTAGE) != 0 ||
	    (scb->flags & SCB_RECOVERY_SCB) != 0) {
		ahc->flags &= ~AHC_RESOURCE_SHORTAGE;
		ahc->queue_blocked = 0;
	}

	/* Clean up for the next user */
	scb->flags = SCB_FREE;
	hscb->control = 0;
	hscb->status = 0;

	SLIST_INSERT_HEAD(&ahc->scb_data->free_scbs, scb, links);
	splx(opri);
}

/*
 * Get a free scb, either one already assigned to a hardware slot
 * on the adapter or one that will require an SCB to be paged out before
 * use. If there are none, see if we can allocate a new SCB.  Otherwise
 * either return an error or sleep.
 */
static __inline struct scb *
ahcgetscb(ahc)
	struct ahc_softc *ahc;
{
	struct scb *scbp;
	int opri;

	opri = splbio();
	if ((scbp = SLIST_FIRST(&ahc->scb_data->free_scbs))) {
		SLIST_REMOVE_HEAD(&ahc->scb_data->free_scbs, links);
	} else {
		ahcallocscbs(ahc);
		scbp = SLIST_FIRST(&ahc->scb_data->free_scbs);
		if (scbp != NULL)
			SLIST_REMOVE_HEAD(&ahc->scb_data->free_scbs, links);
	}

	splx(opri);

	return (scbp);
}

int
ahc_createdmamem(ahc, size, mapp, vaddr, baddr, seg, nseg, what)
	struct ahc_softc *ahc;
	int size;
	bus_dmamap_t *mapp;
	caddr_t *vaddr;
	bus_addr_t *baddr;
	bus_dma_segment_t *seg;
	int *nseg;
	const char *what;
{
	int error, rseg, level = 0;
	int dma_flags = BUS_DMA_NOWAIT;
	bus_dma_tag_t tag = ahc->sc_dmat;
	const char *myname = ahc_name(ahc);
	if ((ahc->chip & AHC_VL) !=0)
		dma_flags |= ISABUS_DMA_32BIT;
	
	if ((error = bus_dmamem_alloc(tag, size, NBPG, 0,
			seg, 1, nseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: failed to allocate DMA mem for %s, error = %d\n",
			myname, what, error);
		goto out;
	}
	level++;

	if ((error = bus_dmamem_map(tag, seg, *nseg, size, vaddr,
			BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: failed to map DMA mem for %s, error = %d\n",
			myname, what, error);
		goto out;
	}
	level++;

	if ((error = bus_dmamap_create(tag, size, 1, size, 0,
			dma_flags, mapp)) != 0) {
		printf("%s: failed to create DMA map for %s, error = %d\n",
			myname, what, error);
		goto out;
        }
	level++;

	if ((error = bus_dmamap_load(tag, *mapp, *vaddr, size, NULL,
			BUS_DMA_NOWAIT)) != 0) {
		printf("%s: failed to load DMA map for %s, error = %d\n",
			myname, what, error);
		goto out;
        }

	*baddr = seg[0].ds_addr;

	if (bootverbose)
		printf("%s: dmamem for %s at phys %lx virt %lx nseg %d size %d\n",
		       myname, what, (unsigned long)*baddr,
		       (unsigned long)*vaddr, *nseg, size);
	return 0;
out:
	switch (level) {
	case 3:
		bus_dmamap_destroy(tag, *mapp);
		/* FALLTHROUGH */
	case 2:
		bus_dmamem_unmap(tag, *vaddr, size);
		/* FALLTHROUGH */
	case 1:
		bus_dmamem_free(tag, seg, rseg);
		break;
	default:
		break;
	}

	return error;
}

STATIC void
ahc_freedmamem(tag, size, map, vaddr, seg, nseg)
	bus_dma_tag_t tag;
	int size;
	bus_dmamap_t map;
	caddr_t vaddr;
	bus_dma_segment_t *seg;
	int nseg;
{

	bus_dmamap_unload(tag, map);
	bus_dmamap_destroy(tag, map);
	bus_dmamem_unmap(tag, vaddr, size);
	bus_dmamem_free(tag, seg, nseg);
}

#ifdef  AHC_DEBUG
STATIC void
ahc_print_scb(scb)
	struct scb *scb;
{
	struct hardware_scb *hscb = scb->hscb;

	printf("scb:%p control:0x%x tcl:0x%x cmdlen:%d cmdpointer:0x%lx\n",
		scb,
		hscb->control,
		hscb->tcl,
		hscb->cmdlen,
		hscb->cmdpointer );
	printf("        datlen:%d data:0x%lx segs:0x%x segp:0x%lx\n",
		hscb->datalen,
		hscb->data,
		hscb->SG_count,
		hscb->SG_pointer);
	printf("	sg_addr:%lx sg_len:%ld\n",
		scb->sg_list[0].addr,
		scb->sg_list[0].len);
	printf("	cdb:%x %x %x %x %x %x %x %x %x %x %x %x\n",
		hscb->cmdstore[0], hscb->cmdstore[1], hscb->cmdstore[2],
		hscb->cmdstore[3], hscb->cmdstore[4], hscb->cmdstore[5],
		hscb->cmdstore[6], hscb->cmdstore[7], hscb->cmdstore[8],
		hscb->cmdstore[9], hscb->cmdstore[10], hscb->cmdstore[11]);
}
#endif

static struct {
        u_int8_t errno;
	char *errmesg;
} hard_error[] = {
	{ ILLHADDR,	"Illegal Host Access" },
	{ ILLSADDR,	"Illegal Sequencer Address referrenced" },
	{ ILLOPCODE,	"Illegal Opcode in sequencer program" },
	{ SQPARERR,	"Sequencer Parity Error" },
	{ DPARERR,	"Data-path Parity Error" },
	{ MPARERR,	"Scratch or SCB Memory Parity Error" },
	{ PCIERRSTAT,	"PCI Error detected" },
	{ CIOPARERR,	"CIOBUS Parity Error" },
};
static const int num_errors = sizeof(hard_error)/sizeof(hard_error[0]);

static struct {
        u_int8_t phase;
        u_int8_t mesg_out; /* Message response to parity errors */
	char *phasemsg;
} phase_table[] = {
	{ P_DATAOUT,	MSG_NOOP,		"in Data-out phase"	},
	{ P_DATAIN,	MSG_INITIATOR_DET_ERR,	"in Data-in phase"	},
	{ P_COMMAND,	MSG_NOOP,		"in Command phase"	},
	{ P_MESGOUT,	MSG_NOOP,		"in Message-out phase"	},
	{ P_STATUS,	MSG_INITIATOR_DET_ERR,	"in Status phase"	},
	{ P_MESGIN,	MSG_PARITY_ERROR,	"in Message-in phase"	},
	{ P_BUSFREE,	MSG_NOOP,		"while idle"		},
	{ 0,		MSG_NOOP,		"in unknown phase"	}
};
static const int num_phases = (sizeof(phase_table)/sizeof(phase_table[0])) - 1;

/*
 * Valid SCSIRATE values.  (p. 3-17)
 * Provides a mapping of tranfer periods in ns to the proper value to
 * stick in the scsiscfr reg to use that transfer rate.
 */
#define AHC_SYNCRATE_DT		0
#define AHC_SYNCRATE_ULTRA2	1
#define AHC_SYNCRATE_ULTRA	2
#define AHC_SYNCRATE_FAST	5
static struct ahc_syncrate ahc_syncrates[] = {
      /* ultra2    fast/ultra  period     rate */
	{ 0x42,      0x000,      9,      "80.0" },
	{ 0x03,      0x000,     10,      "40.0" },
	{ 0x04,      0x000,     11,      "33.0" },
	{ 0x05,      0x100,     12,      "20.0" },
	{ 0x06,      0x110,     15,      "16.0" },
	{ 0x07,      0x120,     18,      "13.4" },
	{ 0x08,      0x000,     25,      "10.0" },
	{ 0x19,      0x010,     31,      "8.0"  },
	{ 0x1a,      0x020,     37,      "6.67" },
	{ 0x1b,      0x030,     43,      "5.7"  },
	{ 0x1c,      0x040,     50,      "5.0"  },
	{ 0x00,      0x050,     56,      "4.4"  },
	{ 0x00,      0x060,     62,      "4.0"  },
	{ 0x00,      0x070,     68,      "3.6"  },
	{ 0x00,      0x000,      0,      NULL   }
};

/*
 * Allocate a controller structure for a new device and initialize it.
 * ahc_reset should be called before now since we assume that the card
 * is paused.
 */
void
ahc_construct(ahc, iot, ioh, chip, flags, features, channel)
	struct  ahc_softc *ahc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	ahc_chip chip;
	ahc_flag flags;
	ahc_feature features;
	u_char channel;
{
	/*
	 * find unit and check we have that many defined
	 */
	LIST_INIT(&ahc->pending_scbs);
	ahc->sc_iot = iot;
	ahc->sc_ioh = ioh;
	ahc->chip = chip;
	ahc->flags = flags;
	ahc->features = features;
	ahc->channel = channel;
	ahc->scb_data = NULL;
	ahc->pci_intr_func = NULL;

	ahc->unpause = (ahc_inb(ahc, HCNTRL) & IRQMS) | INTEN;
	/* The IRQMS bit is only valid on VL and EISA chips */
	if ((ahc->chip & AHC_PCI) != 0)
		ahc->unpause &= ~IRQMS;
	ahc->pause = ahc->unpause | PAUSE;
}

void
ahc_free(ahc)
	struct ahc_softc *ahc;
{
	ahcfiniscbdata(ahc);
	if (ahc->init_level != 0)
		ahc_freedmamem(ahc->sc_dmat, ahc->shared_data_size,
		    ahc->shared_data_dmamap, ahc->qoutfifo,
		    &ahc->shared_data_seg, ahc->shared_data_nseg);

	if (ahc->scb_data != NULL)
		free(ahc->scb_data, M_DEVBUF);
	if (ahc->pci_data != NULL)
		free(ahc->pci_data, M_DEVBUF);
	return;
}

STATIC int
ahcinitscbdata(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;
	int i;
	
	scb_data = ahc->scb_data;
	SLIST_INIT(&scb_data->free_scbs);
	SLIST_INIT(&scb_data->sg_maps);

	/* Allocate SCB resources */
	scb_data->scbarray =
	    (struct scb *)malloc(sizeof(struct scb) * AHC_SCB_MAX,
				 M_DEVBUF, M_NOWAIT);
	if (scb_data->scbarray == NULL)
		return (ENOMEM);
	bzero(scb_data->scbarray, sizeof(struct scb) * AHC_SCB_MAX);

	/* Determine the number of hardware SCBs and initialize them */

	scb_data->maxhscbs = ahc_probe_scbs(ahc);
	/* SCB 0 heads the free list */
	ahc_outb(ahc, FREE_SCBH, 0);
	for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
		ahc_outb(ahc, SCBPTR, i);

		/* Clear the control byte. */
		ahc_outb(ahc, SCB_CONTROL, 0);

		/* Set the next pointer */
		ahc_outb(ahc, SCB_NEXT, i+1);

		/* Make the tag number invalid */
		ahc_outb(ahc, SCB_TAG, SCB_LIST_NULL);
	}

	/* Make sure that the last SCB terminates the free list */
	ahc_outb(ahc, SCBPTR, i-1);
	ahc_outb(ahc, SCB_NEXT, SCB_LIST_NULL);

	/* Ensure we clear the 0 SCB's control byte. */
	ahc_outb(ahc, SCBPTR, 0);
	ahc_outb(ahc, SCB_CONTROL, 0);

	scb_data->maxhscbs = i;

	if (ahc->scb_data->maxhscbs == 0)
		panic("%s: No SCB space found", ahc_name(ahc));

	/*
	 * Create our DMA tags.  These tags define the kinds of device
	 * accessable memory allocations and memory mappings we will
	 * need to perform during normal operation.
	 *
	 * Unless we need to further restrict the allocation, we rely
	 * on the restrictions of the parent dmat, hence the common
	 * use of MAXADDR and MAXSIZE.
	 */

	if (ahc_createdmamem(ahc,
	    AHC_SCB_MAX * sizeof(struct hardware_scb), 
	    &scb_data->hscb_dmamap, (caddr_t *)&scb_data->hscbs, 
	    &scb_data->hscb_busaddr, &scb_data->hscb_seg,
	    &scb_data->hscb_nseg, "hardware SCB structures") < 0)
		goto error_exit;

	scb_data->init_level++;

	if (ahc_createdmamem(ahc,
	    AHC_SCB_MAX * sizeof(struct scsi_sense_data),
	    &scb_data->sense_dmamap, (caddr_t *)&scb_data->sense,
	    &scb_data->sense_busaddr, &scb_data->sense_seg,
	    &scb_data->sense_nseg, "sense buffers") < 0)
		goto error_exit;

	scb_data->init_level++;

	/* Perform initial CCB allocation */
	bzero(scb_data->hscbs, AHC_SCB_MAX * sizeof(struct hardware_scb));
	ahcallocscbs(ahc);

	if (scb_data->numscbs == 0) {
		printf("%s: ahc_init_scb_data - "
		       "Unable to allocate initial scbs\n",
		       ahc_name(ahc));
		goto error_exit;
	}

	scb_data->init_level++;

	/*
	 * Note that we were successfull
	 */
	return 0; 

error_exit:

	return ENOMEM;
}

STATIC void
ahcfiniscbdata(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;

	scb_data = ahc->scb_data;

	switch (scb_data->init_level) {
	default:
	case 3:
	{
		struct sg_map_node *sg_map;

		while ((sg_map = SLIST_FIRST(&scb_data->sg_maps))!= NULL) {
			SLIST_REMOVE_HEAD(&scb_data->sg_maps, links);
			ahc_freedmamem(ahc->sc_dmat, PAGE_SIZE,
			    sg_map->sg_dmamap, (caddr_t)sg_map->sg_vaddr,
			    &sg_map->sg_dmasegs, sg_map->sg_nseg);
			free(sg_map, M_DEVBUF);
		}
	}
	/*FALLTHROUGH*/
	case 2:
		ahc_freedmamem(ahc->sc_dmat,
		    AHC_SCB_MAX * sizeof(struct scsi_sense_data),
		    scb_data->sense_dmamap, (caddr_t)scb_data->sense,
		    &scb_data->sense_seg, scb_data->sense_nseg);
	/*FALLTHROUGH*/
	case 1:
		ahc_freedmamem(ahc->sc_dmat,
		    AHC_SCB_MAX * sizeof(struct hardware_scb), 
		    scb_data->hscb_dmamap, (caddr_t)scb_data->hscbs,
		    &scb_data->hscb_seg, scb_data->hscb_nseg);
	/*FALLTHROUGH*/
	}
	if (scb_data->scbarray != NULL)
		free(scb_data->scbarray, M_DEVBUF);
}

void
ahc_xxx_reset(devname, iot, ioh)
	char *devname;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_char hcntrl;
	int wait;

#ifdef AHC_DUMP_SEQ
	ahc_dumpseq(ahc);
#endif
	/* Retain the IRQ type accross the chip reset */
	hcntrl = (bus_space_read_1(iot, ioh, HCNTRL) & IRQMS) | INTEN;

	bus_space_write_1(iot, ioh, HCNTRL, CHIPRST | PAUSE);
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
	while (--wait && !(bus_space_read_1(iot, ioh, HCNTRL) & CHIPRSTACK))
		DELAY(1000);
	if (wait == 0) {
		printf("%s: WARNING - Failed chip reset!  "
				 "Trying to initialize anyway.\n", devname);
	}
	bus_space_write_1(iot, ioh, HCNTRL, hcntrl | PAUSE);
}

int
ahc_reset(ahc)
	struct ahc_softc *ahc;
{
	u_int	sblkctl;
	int	wait;
	
#ifdef AHC_DUMP_SEQ
	if (ahc->init_level == 0)
		ahc_dumpseq(ahc);
#endif
	ahc_outb(ahc, HCNTRL, CHIPRST | ahc->pause);
	/*
	 * Ensure that the reset has finished
	 */
	wait = 1000;
	do {
		DELAY(1000);
	} while (--wait && !(ahc_inb(ahc, HCNTRL) & CHIPRSTACK));

	if (wait == 0) {
		printf("%s: WARNING - Failed chip reset!  "
		       "Trying to initialize anyway.\n", ahc_name(ahc));
	}
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/* Determine channel configuration */
	sblkctl = ahc_inb(ahc, SBLKCTL) & (SELBUSB|SELWIDE);
	/* No Twin Channel PCI cards */
	if ((ahc->chip & AHC_PCI) != 0)
		sblkctl &= ~SELBUSB;
	switch (sblkctl) {
	case 0:
		/* Single Narrow Channel */
		break;
	case 2:
		/* Wide Channel */
		ahc->features |= AHC_WIDE;
		break;
	case 8:
		/* Twin Channel */
		ahc->features |= AHC_TWIN;
		break;
	default:
		printf(" Unsupported adapter type.  Ignoring\n");
		return(-1);
	}
	return (0);
}

/*
 * Called when we have an active connection to a target on the bus,
 * this function finds the nearest syncrate to the input period limited
 * by the capabilities of the bus connectivity of the target.
 */
STATIC struct ahc_syncrate *
ahc_devlimited_syncrate(ahc, period)
	struct ahc_softc *ahc;
	u_int *period;
{
	u_int	maxsync;

	if ((ahc->features & AHC_ULTRA2) != 0) {
		if ((ahc_inb(ahc, SBLKCTL) & ENAB40) != 0
		 && (ahc_inb(ahc, SSTAT2) & EXP_ACTIVE) == 0) {
			maxsync = AHC_SYNCRATE_ULTRA2;
		} else {
			maxsync = AHC_SYNCRATE_ULTRA;
		}
	} else if ((ahc->features & AHC_ULTRA) != 0) {
		maxsync = AHC_SYNCRATE_ULTRA;
	} else {
		maxsync = AHC_SYNCRATE_FAST;
	}
	return (ahc_find_syncrate(ahc, period, maxsync));
}

/*
 * Look up the valid period to SCSIRATE conversion in our table.
 * Return the period and offset that should be sent to the target
 * if this was the beginning of an SDTR.
 */
STATIC struct ahc_syncrate *
ahc_find_syncrate(ahc, period, maxsync)
	struct ahc_softc *ahc;
	u_int *period;
	u_int maxsync;
{
	struct ahc_syncrate *syncrate;

	syncrate = &ahc_syncrates[maxsync];
	while ((syncrate->rate != NULL)
	    && ((ahc->features & AHC_ULTRA2) == 0
	     || (syncrate->sxfr_u2 != 0))) {

		if (*period <= syncrate->period) {
			/*
			 * When responding to a target that requests
			 * sync, the requested rate may fall between
			 * two rates that we can output, but still be
			 * a rate that we can receive.  Because of this,
			 * we want to respond to the target with
			 * the same rate that it sent to us even
			 * if the period we use to send data to it
			 * is lower.  Only lower the response period
			 * if we must.
			 */
			if (syncrate == &ahc_syncrates[maxsync])
				*period = syncrate->period;
			break;
		}
		syncrate++;
	}

	if ((*period == 0)
	 || (syncrate->rate == NULL)
	 || ((ahc->features & AHC_ULTRA2) != 0
	  && (syncrate->sxfr_u2 == 0))) {
		/* Use asynchronous transfers. */
		*period = 0;
		syncrate = NULL;
	}
	return (syncrate);
}

STATIC u_int
ahc_find_period(ahc, scsirate, maxsync)
	struct ahc_softc *ahc;
	u_int scsirate;
	u_int maxsync;
{
	struct ahc_syncrate *syncrate;

	if ((ahc->features & AHC_ULTRA2) != 0)
		scsirate &= SXFR_ULTRA2;
	else
		scsirate &= SXFR;

	syncrate = &ahc_syncrates[maxsync];
	while (syncrate->rate != NULL) {

		if ((ahc->features & AHC_ULTRA2) != 0) {
			if (syncrate->sxfr_u2 == 0)
				break;
			else if (scsirate == (syncrate->sxfr_u2 & SXFR_ULTRA2))
				return (syncrate->period);
		} else if (scsirate == (syncrate->sxfr & SXFR)) {
				return (syncrate->period);
		}
		syncrate++;
	}
	return (0); /* async */
}

STATIC void
ahc_validate_offset(ahc, syncrate, offset, wide)
	struct ahc_softc *ahc;
	struct ahc_syncrate *syncrate;
	u_int *offset;
	int wide;
{
	u_int maxoffset;

	/* Limit offset to what we can do */
	if (syncrate == NULL) {
		maxoffset = 0;
	} else if ((ahc->features & AHC_ULTRA2) != 0) {
		maxoffset = MAX_OFFSET_ULTRA2;
	} else {
		if (wide)
			maxoffset = MAX_OFFSET_16BIT;
		else
			maxoffset = MAX_OFFSET_8BIT;
	}
	*offset = MIN(*offset, maxoffset);
}

STATIC void
ahc_update_target_msg_request(ahc, devinfo, tinfo, force, paused)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	struct ahc_initiator_tinfo *tinfo;
	int force;
	int paused;
{
	u_int targ_msg_req_orig;

	targ_msg_req_orig = ahc->targ_msg_req;
	if (tinfo->current.period != tinfo->goal.period
	    || tinfo->current.width != tinfo->goal.width
	    || tinfo->current.offset != tinfo->goal.offset
	    || (force && (tinfo->goal.period != 0
	    || tinfo->goal.width != MSG_EXT_WDTR_BUS_8_BIT)))
		ahc->targ_msg_req |= devinfo->target_mask;
	else
		ahc->targ_msg_req &= ~devinfo->target_mask;

	if (ahc->targ_msg_req != targ_msg_req_orig) {
		/* Update the message request bit for this target */
		if ((ahc->features & AHC_HS_MAILBOX) != 0) {
			if (paused) {
				ahc_outb(ahc, TARGET_MSG_REQUEST,
					 ahc->targ_msg_req & 0xFF);
				ahc_outb(ahc, TARGET_MSG_REQUEST + 1,
					 (ahc->targ_msg_req >> 8) & 0xFF);
			} else {
				ahc_outb(ahc, HS_MAILBOX,
					 0x01 << HOST_MAILBOX_SHIFT);
			}
		} else {
			if (!paused)
				pause_sequencer(ahc);

			ahc_outb(ahc, TARGET_MSG_REQUEST,
				 ahc->targ_msg_req & 0xFF);
			ahc_outb(ahc, TARGET_MSG_REQUEST + 1,
				 (ahc->targ_msg_req >> 8) & 0xFF);

			if (!paused)
				unpause_sequencer(ahc);
		}
	}
}

STATIC void
ahc_set_syncrate(ahc, devinfo, syncrate, period, offset, type, paused, done)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	struct ahc_syncrate *syncrate;
	u_int period;
	u_int offset;
	u_int type;
	int paused;
	int done;
{
	struct	ahc_initiator_tinfo *tinfo;
	struct	tmode_tstate *tstate;
	u_int	old_period;
	u_int	old_offset;
	int	active = (type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE;

	if (syncrate == NULL) {
		period = 0;
		offset = 0;
	}

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
				    devinfo->our_scsiid,
				    devinfo->target, &tstate);
	old_period = tinfo->current.period;
	old_offset = tinfo->current.offset;

	if ((type & AHC_TRANS_CUR) != 0
	 && (old_period != period || old_offset != offset)) {
		u_int	scsirate;

		scsirate = tinfo->scsirate;
		if ((ahc->features & AHC_ULTRA2) != 0) {

			/* XXX */
			/* Force single edge until DT is fully implemented */
			scsirate &= ~(SXFR_ULTRA2|SINGLE_EDGE|ENABLE_CRC);
			if (syncrate != NULL)
				scsirate |= syncrate->sxfr_u2|SINGLE_EDGE;

			if (active)
				ahc_outb(ahc, SCSIOFFSET, offset);
		} else {

			scsirate &= ~(SXFR|SOFS);
			/*
			 * Ensure Ultra mode is set properly for
			 * this target.
			 */
			tstate->ultraenb &= ~devinfo->target_mask;
			if (syncrate != NULL) {
				if (syncrate->sxfr & ULTRA_SXFR) {
					tstate->ultraenb |=
						devinfo->target_mask;
				}
				scsirate |= syncrate->sxfr & SXFR;
				scsirate |= offset & SOFS;
			}
			if (active) {
				u_int sxfrctl0;

				sxfrctl0 = ahc_inb(ahc, SXFRCTL0);
				sxfrctl0 &= ~FAST20;
				if (tstate->ultraenb & devinfo->target_mask)
					sxfrctl0 |= FAST20;
				ahc_outb(ahc, SXFRCTL0, sxfrctl0);
			}
		}
		if (active)
			ahc_outb(ahc, SCSIRATE, scsirate);

		tinfo->scsirate = scsirate;
		tinfo->current.period = period;
		tinfo->current.offset = offset;

		/* Update the syncrates in any pending scbs */
		ahc_update_pending_syncrates(ahc);
	}

	/*
	 * Print messages if we're verbose and at the end of a negotiation
	 * cycle.
	 */
	if (done && bootverbose) {
		if (offset != 0) {
			printf("%s: target %d synchronous at %sMHz, "
			       "offset = 0x%x\n", ahc_name(ahc),
			       devinfo->target, syncrate->rate, offset);
		} else {
			printf("%s: target %d using "
			       "asynchronous transfers\n",
			       ahc_name(ahc), devinfo->target);
		}
	}

	if ((type & AHC_TRANS_GOAL) != 0) {
		tinfo->goal.period = period;
		tinfo->goal.offset = offset;
	}

	if ((type & AHC_TRANS_USER) != 0) {
		tinfo->user.period = period;
		tinfo->user.offset = offset;
	}

	ahc_update_target_msg_request(ahc, devinfo, tinfo,
				      /*force*/FALSE,
				      paused);
}

STATIC void
ahc_set_width(ahc, devinfo, width, type, paused, done)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	u_int width;
	u_int type;
	int paused;
	int done;
{
	struct ahc_initiator_tinfo *tinfo;
	struct tmode_tstate *tstate;
	u_int  oldwidth;
	int    active = (type & AHC_TRANS_ACTIVE) == AHC_TRANS_ACTIVE;

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
				    devinfo->our_scsiid,
				    devinfo->target,
				    &tstate);
	oldwidth = tinfo->current.width;

	if ((type & AHC_TRANS_CUR) != 0 && oldwidth != width) {
		u_int	scsirate;

		scsirate =  tinfo->scsirate;
		scsirate &= ~WIDEXFER;
		if (width == MSG_EXT_WDTR_BUS_16_BIT)
			scsirate |= WIDEXFER;

		tinfo->scsirate = scsirate;

		if (active)
			ahc_outb(ahc, SCSIRATE, scsirate);

		tinfo->current.width = width;
	}

	if (done) {
		printf("%s: target %d using %dbit transfers\n",
		       ahc_name(ahc), devinfo->target,
		       8 * (0x01 << width));
	}

	if ((type & AHC_TRANS_GOAL) != 0)
		tinfo->goal.width = width;
	if ((type & AHC_TRANS_USER) != 0)
		tinfo->user.width = width;

	ahc_update_target_msg_request(ahc, devinfo, tinfo,
				      /*force*/FALSE, paused);
}

STATIC void
ahc_set_tags(ahc, devinfo, enable)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	int enable;
{
	struct ahc_initiator_tinfo *tinfo;
	struct tmode_tstate *tstate;

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
				    devinfo->our_scsiid,
				    devinfo->target,
				    &tstate);
	if (enable)
		tstate->tagenable |= devinfo->target_mask;
	else
		tstate->tagenable &= ~devinfo->target_mask;
}

/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(ahc)
	struct ahc_softc *ahc;
{
	/*
	 * Initialize the software queue.
	 */
	LIST_INIT(&ahc->sc_xxxq);

#ifdef AHC_BROKEN_CACHE
	if (cpu_class == CPUCLASS_386)	/* doesn't have "wbinvd" instruction */
		ahc_broken_cache = 0;
#endif
	/*
	 * fill in the prototype scsi_links.
	 */
	ahc->sc_link.adapter_target = ahc->our_id;
	if (ahc->features & AHC_WIDE)
		ahc->sc_link.adapter_buswidth = 16;
	ahc->sc_link.adapter_softc = ahc;
	ahc->sc_link.adapter = &ahc_switch;
	ahc->sc_link.openings = 2;
	ahc->sc_link.device = &ahc_dev;
	ahc->sc_link.flags = SCSIDEBUG_LEVEL;
	
	if (ahc->features & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_link_b = ahc->sc_link;
		ahc->sc_link_b.adapter_target = ahc->our_id_b;
		if (ahc->features & AHC_WIDE)
			ahc->sc_link.adapter_buswidth = 16;
	}

/*
	 * ask the adapter what subunits are present
	 */
	if ((ahc->flags & AHC_CHANNEL_B_PRIMARY) == 0) {
		/* make IS_SCSIBUS_B() == false, while probing channel A */
		ahc->sc_link_b.scsibus = 0xff;

		config_found((void *)ahc, &ahc->sc_link, scsiprint);
		if (ahc->features & AHC_TWIN)
			config_found((void *)ahc, &ahc->sc_link_b, scsiprint);
	} else {
		/*
		 * if implementation of IS_SCSIBUS_B() is changed to use
		 * ahc->sc_link.scsibus, then "ahc->sc_link.scsibus = 0xff;"
		 * is needed, here.
		 */

		/* assert(ahc->features & AHC_TWIN); */
		config_found((void *)ahc, &ahc->sc_link_b, scsiprint);
		config_found((void *)ahc, &ahc->sc_link, scsiprint);
	}
	return 1;
}

STATIC void
ahc_fetch_devinfo(ahc, devinfo)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
{
	u_int	saved_tcl;
	role_t	role;
	int	our_id;

	if (ahc_inb(ahc, SSTAT0) & TARGET)
		role = ROLE_TARGET;
	else
		role = ROLE_INITIATOR;

	if (role == ROLE_TARGET
	 && (ahc->features & AHC_MULTI_TID) != 0
	 && (ahc_inb(ahc, SEQ_FLAGS) & CMDPHASE_PENDING) != 0) {
		/* We were selected, so pull our id from TARGIDIN */
		our_id = ahc_inb(ahc, TARGIDIN) & OID;
	} else if ((ahc->features & AHC_ULTRA2) != 0)
		our_id = ahc_inb(ahc, SCSIID_ULTRA2) & OID;
	else
		our_id = ahc_inb(ahc, SCSIID) & OID;

	saved_tcl = ahc_inb(ahc, SAVED_TCL);
	ahc_compile_devinfo(devinfo, our_id, TCL_TARGET(saved_tcl),
			    TCL_LUN(saved_tcl), TCL_CHANNEL(ahc, saved_tcl),
			    role);
}

STATIC void
ahc_compile_devinfo(devinfo, our_id, target, lun, channel, role)
	struct ahc_devinfo *devinfo;
	u_int our_id;
	u_int target;
	u_int lun;
	char channel;
	role_t role;
{
	devinfo->our_scsiid = our_id;
	devinfo->target = target;
	devinfo->lun = lun;
	devinfo->target_offset = target;
	devinfo->channel = channel;
	devinfo->role = role;
	if (channel == 'B')
		devinfo->target_offset += 8;
	devinfo->target_mask = (0x01 << devinfo->target_offset);
}

/*
 * Catch an interrupt from the adapter
 */
int
ahc_intr(void *arg)
{
	struct	ahc_softc *ahc;
	u_int	intstat;

	ahc = (struct ahc_softc *)arg; 

	intstat = ahc_inb(ahc, INTSTAT);

	/*
	 * Any interrupts to process?
	 */
	if ((intstat & INT_PEND) == 0) {
		if (ahc->pci_intr_func && ahc->pci_intr_func(ahc)) {
#ifdef AHC_DEBUG
			printf("%s: bus intr: CCHADDR %x HADDR %x SEQADDR %x\n",
			    ahc_name(ahc),
			    ahc_inb(ahc, CCHADDR) |
			    (ahc_inb(ahc, CCHADDR+1) << 8)
			    | (ahc_inb(ahc, CCHADDR+2) << 16)
			    | (ahc_inb(ahc, CCHADDR+3) << 24),
			    ahc_inb(ahc, HADDR) | (ahc_inb(ahc, HADDR+1) << 8)
			    | (ahc_inb(ahc, HADDR+2) << 16)
			    | (ahc_inb(ahc, HADDR+3) << 24),
			    ahc_inb(ahc, SEQADDR0) |
			    (ahc_inb(ahc, SEQADDR1) << 8));
#endif
			return 1;
		}
		return 0;
	}

	if (intstat & CMDCMPLT) {
		ahc_outb(ahc, CLRINT, CLRCMDINT);
		ahc_run_qoutfifo(ahc);
	}
	if (intstat & BRKADRINT) {
		/*
		 * We upset the sequencer :-(
		 * Lookup the error message
		 */
		int i, error, num_errors;

		error = ahc_inb(ahc, ERROR);
		num_errors =  sizeof(hard_error)/sizeof(hard_error[0]);
		for (i = 0; error != 1 && i < num_errors; i++)
			error >>= 1;
		panic("%s: brkadrint, %s at seqaddr = 0x%x\n",
		      ahc_name(ahc), hard_error[i].errmesg,
		      ahc_inb(ahc, SEQADDR0) |
		      (ahc_inb(ahc, SEQADDR1) << 8));

		/* Tell everyone that this HBA is no longer availible */
		ahc_abort_scbs(ahc, ALL_TARGETS, ALL_CHANNELS,
			       ALL_LUNS, SCB_LIST_NULL, ROLE_UNKNOWN,
			       XS_DRIVER_STUFFUP);
	}
	if (intstat & SEQINT)
		ahc_handle_seqint(ahc, intstat);

	if (intstat & SCSIINT)
		ahc_handle_scsiint(ahc, intstat);
	return(1);
}

STATIC struct tmode_tstate *
ahc_alloc_tstate(ahc, scsi_id, channel)
	struct ahc_softc *ahc;
	u_int scsi_id;
	char channel;
{
	struct tmode_tstate *master_tstate;
	struct tmode_tstate *tstate;
	int i, s;

	master_tstate = ahc->enabled_targets[ahc->our_id];
	if (channel == 'B') {
		scsi_id += 8;
		master_tstate = ahc->enabled_targets[ahc->our_id_b + 8];
	}
	if (ahc->enabled_targets[scsi_id] != NULL
	 && ahc->enabled_targets[scsi_id] != master_tstate)
		panic("%s: ahc_alloc_tstate - Target already allocated",
		      ahc_name(ahc));
	tstate = malloc(sizeof(*tstate), M_DEVBUF, M_NOWAIT);
	if (tstate == NULL)
		return (NULL);

	/*
	 * If we have allocated a master tstate, copy user settings from
	 * the master tstate (taken from SRAM or the EEPROM) for this
	 * channel, but reset our current and goal settings to async/narrow
	 * until an initiator talks to us.
	 */
	if (master_tstate != NULL) {
		bcopy(master_tstate, tstate, sizeof(*tstate));
		tstate->ultraenb = 0;
		for (i = 0; i < 16; i++) {
			bzero(&tstate->transinfo[i].current,
			      sizeof(tstate->transinfo[i].current));
			bzero(&tstate->transinfo[i].goal,
			      sizeof(tstate->transinfo[i].goal));
		}
	} else
		bzero(tstate, sizeof(*tstate));
	s = splbio();
	ahc->enabled_targets[scsi_id] = tstate;
	splx(s);
	return (tstate);
}

STATIC void
ahc_handle_seqint(ahc, intstat)
	struct ahc_softc *ahc;
	u_int intstat;
{
	struct scb *scb;
	struct ahc_devinfo devinfo;
	
	ahc_fetch_devinfo(ahc, &devinfo);

	/*
	 * Clear the upper byte that holds SEQINT status
	 * codes and clear the SEQINT bit. We will unpause
	 * the sequencer, if appropriate, after servicing
	 * the request.
	 */
	ahc_outb(ahc, CLRINT, CLRSEQINT);
	switch (intstat & SEQINT_MASK) {
	case NO_MATCH:
	{
		/* Ensure we don't leave the selection hardware on */
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));

		printf("%s:%c:%d: no active SCB for reconnecting "
		       "target - issuing BUS DEVICE RESET\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target);
		printf("SAVED_TCL == 0x%x, ARG_1 == 0x%x, SEQ_FLAGS == 0x%x\n",
		       ahc_inb(ahc, SAVED_TCL), ahc_inb(ahc, ARG_1),
		       ahc_inb(ahc, SEQ_FLAGS));
		ahc->msgout_buf[0] = MSG_BUS_DEV_RESET;
		ahc->msgout_len = 1;
		ahc->msgout_index = 0;
		ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
		ahc_outb(ahc, MSG_OUT, HOST_MSG);
		ahc_outb(ahc, SCSISIGO, ahc_inb(ahc, LASTPHASE) | ATNO);
		break;
	}
	case UPDATE_TMSG_REQ:
		ahc_outb(ahc, TARGET_MSG_REQUEST, ahc->targ_msg_req & 0xFF);
		ahc_outb(ahc, TARGET_MSG_REQUEST + 1,
			 (ahc->targ_msg_req >> 8) & 0xFF);
		ahc_outb(ahc, HS_MAILBOX, 0);
		break;
	case SEND_REJECT: 
	{
		u_int rejbyte = ahc_inb(ahc, ACCUM);
		printf("%s:%c:%d: Warning - unknown message received from "
		       "target (0x%x).  Rejecting\n", 
		       ahc_name(ahc), devinfo.channel, devinfo.target, rejbyte);
		break; 
	}
	case NO_IDENT: 
	{
		/*
		 * The reconnecting target either did not send an identify
		 * message, or did, but we didn't find and SCB to match and
		 * before it could respond to our ATN/abort, it hit a dataphase.
		 * The only safe thing to do is to blow it away with a bus
		 * reset.
		 */
		int found;

		printf("%s:%c:%d: Target did not send an IDENTIFY message. "
		       "LASTPHASE = 0x%x, SAVED_TCL == 0x%x\n",
		       ahc_name(ahc), devinfo.channel, devinfo.target,
		       ahc_inb(ahc, LASTPHASE), ahc_inb(ahc, SAVED_TCL));
		found = ahc_reset_channel(ahc, devinfo.channel, 
					  /*initiate reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), devinfo.channel,
		       found);
		return;
	}
	case BAD_PHASE:
	{
		u_int lastphase;

		lastphase = ahc_inb(ahc, LASTPHASE);
		if (lastphase == P_BUSFREE) {
			printf("%s:%c:%d: Missed busfree.  Curphase = 0x%x\n",
			       ahc_name(ahc), devinfo.channel, devinfo.target,
			       ahc_inb(ahc, SCSISIGI));
			restart_sequencer(ahc);
			return;
		} else {
			printf("%s:%c:%d: unknown scsi bus phase %x.  "
			       "Attempting to continue\n",
			       ahc_name(ahc), devinfo.channel, devinfo.target,
			       ahc_inb(ahc, SCSISIGI));
		}
		break; 
	}
	case BAD_STATUS:
	{
		u_int  scb_index;
		struct hardware_scb *hscb;
		struct scsi_xfer *xs;
		/*
		 * The sequencer will notify us when a command
		 * has an error that would be of interest to
		 * the kernel.  This allows us to leave the sequencer
		 * running in the common case of command completes
		 * without error.  The sequencer will already have
		 * dma'd the SCB back up to us, so we can reference
		 * the in kernel copy directly.
		 */
		scb_index = ahc_inb(ahc, SCB_TAG);
		scb = &ahc->scb_data->scbarray[scb_index];

		/*
		 * Set the default return value to 0 (don't
		 * send sense).  The sense code will change
		 * this if needed.
		 */
		ahc_outb(ahc, RETURN_1, 0);
		if (!(scb_index < ahc->scb_data->numscbs
		   && (scb->flags & SCB_ACTIVE) != 0)) {
			printf("%s:%c:%d: ahc_intr - referenced scb "
			       "not valid during seqint 0x%x scb(%d)\n",
			       ahc_name(ahc), devinfo.channel,
			       devinfo.target, intstat, scb_index);
			goto unpause;
		}

		hscb = scb->hscb; 
		xs = scb->xs;

		/* Don't want to clobber the original sense code */
		if ((scb->flags & SCB_SENSE) != 0) {
			/*
			 * Clear the SCB_SENSE Flag and have
			 * the sequencer do a normal command
			 * complete.
			 */
			scb->flags &= ~SCB_SENSE;
			ahcsetccbstatus(xs, XS_DRIVER_STUFFUP);
			break;
		}
		/* Freeze the queue unit the client sees the error. */
		ahc_freeze_devq(ahc, xs->sc_link);
		ahc_freeze_ccb(scb);
		xs->status = hscb->status;
		switch (hscb->status) {
		case SCSI_OK:
			printf("%s: Interrupted for staus of 0???\n",
			       ahc_name(ahc));
			break;
		case SCSI_CHECK:
#ifdef AHC_DEBUG
			if (ahc_debug & AHC_SHOWSENSE) {
				sc_print_addr(scb->xs->sc_link);
				printf("SCB %d: requests Check Status\n",
				       scb->hscb->tag);
			}
#endif
				
			if (xs->error == XS_NOERROR &&
			    !(scb->flags & SCB_SENSE)) {
				struct ahc_dma_seg *sg;
				struct scsi_sense *sc;
				struct ahc_initiator_tinfo *tinfo;
				struct tmode_tstate *tstate;

				sg = scb->sg_list;
				sc = (struct scsi_sense *)(&hscb->cmdstore); 
				/*
				 * Save off the residual if there is one.
				 */
				if (hscb->residual_SG_count != 0)
					ahc_calc_residual(scb);
				else
					xs->resid = 0;

#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWSENSE) {
					sc_print_addr(scb->xs->sc_link);
					printf("Sending Sense\n");
				}
#endif
				sg->addr = ahc->scb_data->sense_busaddr +
					(hscb->tag*sizeof(struct scsi_sense_data));
				
				sg->len = sizeof(struct scsi_sense_data);

				sc->opcode = REQUEST_SENSE;
				sc->byte2 =  SCB_LUN(scb) << 5;
				sc->unused[0] = 0;
				sc->unused[1] = 0;
				sc->length = sg->len;
				sc->control = 0;

				/*
				 * Would be nice to preserve DISCENB here,
				 * but due to the way we page SCBs, we can't.
				 */
				hscb->control = 0;

				/*
				 * This request sense could be because the
				 * the device lost power or in some other
				 * way has lost our transfer negotiations.
				 * Renegotiate if appropriate.
				 */
				ahc_calc_residual(scb);
#ifdef AHC_DEBUG
				if (ahc_debug & AHC_SHOWSENSE) {
					sc_print_addr(xs->sc_link);
					printf("Sense: datalen %d resid %d"
					       "chan %d id %d targ %d\n",
					       xs->datalen, xs->resid,
					       devinfo.channel,
					       devinfo.our_scsiid,
					       devinfo.target);
				}
#endif
				if (xs->datalen > 0 &&
				    xs->resid == xs->datalen) {
				tinfo = ahc_fetch_transinfo(ahc,
							    devinfo.channel,
							    devinfo.our_scsiid,
							    devinfo.target,
							    &tstate);
					ahc_update_target_msg_request(ahc,
							      &devinfo,
							      tinfo,
							      /*force*/TRUE,
							      /*paused*/TRUE);
				}
				hscb->status = 0;
				hscb->SG_count = 1;
				hscb->SG_pointer = scb->sg_list_phys;
				hscb->data = sg->addr; 
				hscb->datalen = sg->len;
				hscb->cmdpointer = hscb->cmdstore_busaddr;
				hscb->cmdlen = sizeof(*sc);
				scb->sg_count = hscb->SG_count;
				scb->flags |= SCB_SENSE;
				/*
				 * Ensure the target is busy since this
				 * will be an untagged request.
				 */
				ahc_busy_tcl(ahc, scb);
				ahc_outb(ahc, RETURN_1, SEND_SENSE);

				/*
				 * Ensure we have enough time to actually
				 * retrieve the sense.
				 */
				if (!(scb->xs->flags & SCSI_POLL)) {
				untimeout(ahc_timeout, (caddr_t)scb);
					timeout(ahc_timeout, (caddr_t)scb,
					    5 * hz);
				}
			}
			break;
		case SCSI_BUSY:
			/*
			 * Requeue any transactions that haven't been
			 * sent yet.
			 */
			ahc_freeze_devq(ahc, xs->sc_link);
			ahc_freeze_ccb(scb);
			break;
		}
		break;
	}
	case TRACE_POINT:
	{
		printf("SSTAT2 = 0x%x DFCNTRL = 0x%x\n", ahc_inb(ahc, SSTAT2),
		       ahc_inb(ahc, DFCNTRL));
		printf("SSTAT3 = 0x%x DSTATUS = 0x%x\n", ahc_inb(ahc, SSTAT3),
		       ahc_inb(ahc, DFSTATUS));
		printf("SSTAT0 = 0x%x, SCB_DATACNT = 0x%x\n",
		       ahc_inb(ahc, SSTAT0),
		       ahc_inb(ahc, SCB_DATACNT));
		break;
	}
	case HOST_MSG_LOOP:
	{
		/*
		 * The sequencer has encountered a message phase
		 * that requires host assistance for completion.
		 * While handling the message phase(s), we will be
		 * notified by the sequencer after each byte is
		 * transfered so we can track bus phases.
		 *
		 * If this is the first time we've seen a HOST_MSG_LOOP,
		 * initialize the state of the host message loop.
		 */
		if (ahc->msg_type == MSG_TYPE_NONE) {
			u_int bus_phase;

			bus_phase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
			if (bus_phase != P_MESGIN
			 && bus_phase != P_MESGOUT) {
				printf("ahc_intr: HOST_MSG_LOOP bad "
				       "phase 0x%x\n",
				      bus_phase);
				/*
				 * Probably transitioned to bus free before
				 * we got here.  Just punt the message.
				 */
				ahc_clear_intstat(ahc);
				restart_sequencer(ahc);
			}

			if (devinfo.role == ROLE_INITIATOR) {
				struct scb *scb;
				u_int scb_index;

				scb_index = ahc_inb(ahc, SCB_TAG);
				scb = &ahc->scb_data->scbarray[scb_index];

				if (bus_phase == P_MESGOUT)
					ahc_setup_initiator_msgout(ahc,
								   &devinfo,
								   scb);
				else {
					ahc->msg_type =
					    MSG_TYPE_INITIATOR_MSGIN;
					ahc->msgin_index = 0;
				}
			} else {
				if (bus_phase == P_MESGOUT) {
					ahc->msg_type =
					    MSG_TYPE_TARGET_MSGOUT;
					ahc->msgin_index = 0;
				} else 
					/* XXX Ever executed??? */
					ahc_setup_target_msgin(ahc, &devinfo);
			}
			}

		/* Pass a NULL path so that handlers generate their own */
		ahc_handle_message_phase(ahc, /*path*/NULL);
		break;
		}
	case PERR_DETECTED:
	{
		/*
		 * If we've cleared the parity error interrupt
		 * but the sequencer still believes that SCSIPERR
		 * is true, it must be that the parity error is
		 * for the currently presented byte on the bus,
		 * and we are not in a phase (data-in) where we will
		 * eventually ack this byte.  Ack the byte and
		 * throw it away in the hope that the target will
		 * take us to message out to deliver the appropriate
		 * error message.
		 */
		if ((intstat & SCSIINT) == 0
		 && (ahc_inb(ahc, SSTAT1) & SCSIPERR) != 0) {
			u_int curphase;

			/*
			 * The hardware will only let you ack bytes
			 * if the expected phase in SCSISIGO matches
			 * the current phase.  Make sure this is
			 * currently the case.
			 */
			curphase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
			ahc_outb(ahc, LASTPHASE, curphase);
			ahc_outb(ahc, SCSISIGO, curphase);
			ahc_inb(ahc, SCSIDATL);
		}
		break;
	}
	case DATA_OVERRUN:
	{
		/*
		 * When the sequencer detects an overrun, it
		 * places the controller in "BITBUCKET" mode
		 * and allows the target to complete its transfer.
		 * Unfortunately, none of the counters get updated
		 * when the controller is in this mode, so we have
		 * no way of knowing how large the overrun was.
		 */
		u_int scbindex = ahc_inb(ahc, SCB_TAG);
		u_int lastphase = ahc_inb(ahc, LASTPHASE);
		int i;

		scb = &ahc->scb_data->scbarray[scbindex];
		for (i = 0; i < num_phases; i++) {
			if (lastphase == phase_table[i].phase)
				break;
		}
		sc_print_addr(scb->xs->sc_link);
		printf("data overrun detected %s."
		       "  Tag == 0x%x.\n",
		       phase_table[i].phasemsg,
  		       scb->hscb->tag);
		sc_print_addr(scb->xs->sc_link);
		printf("%s seen Data Phase.  Length = %d.  NumSGs = %d.\n",
		       ahc_inb(ahc, SEQ_FLAGS) & DPHASE ? "Have" : "Haven't",
		       scb->xs->datalen, scb->sg_count);
		if (scb->sg_count > 0) {
			for (i = 0; i < scb->sg_count; i++) {
				printf("sg[%d] - Addr 0x%x : Length %d\n",
				       i,
				       scb->sg_list[i].addr,
				       scb->sg_list[i].len);
			}
		}
		/*
		 * Set this and it will take affect when the
		 * target does a command complete.
		 */
		ahc_freeze_devq(ahc, scb->xs->sc_link);
		ahcsetccbstatus(scb->xs, XS_DRIVER_STUFFUP);
		ahc_freeze_ccb(scb);
		break;
	}
	case TRACEPOINT:
	{
		printf("TRACEPOINT: RETURN_2 = %d\n", ahc_inb(ahc, RETURN_2));
#if 0
		printf("SSTAT1 == 0x%x\n", ahc_inb(ahc, SSTAT1));
		printf("SSTAT0 == 0x%x\n", ahc_inb(ahc, SSTAT0));
		printf(", SCSISIGI == 0x%x\n", ahc_inb(ahc, SCSISIGI));
		printf("TRACEPOINT: CCHCNT = %d, SG_COUNT = %d\n",
		       ahc_inb(ahc, CCHCNT), ahc_inb(ahc, SG_COUNT));
		printf("TRACEPOINT: SCB_TAG = %d\n", ahc_inb(ahc, SCB_TAG));
		printf("TRACEPOINT1: CCHADDR = %d, CCHCNT = %d, SCBPTR = %d\n",
		       ahc_inb(ahc, CCHADDR)
		    | (ahc_inb(ahc, CCHADDR+1) << 8)
		    | (ahc_inb(ahc, CCHADDR+2) << 16)
		    | (ahc_inb(ahc, CCHADDR+3) << 24),
		       ahc_inb(ahc, CCHCNT)
		    | (ahc_inb(ahc, CCHCNT+1) << 8)
		    | (ahc_inb(ahc, CCHCNT+2) << 16),
		       ahc_inb(ahc, SCBPTR));
		printf("TRACEPOINT: WAITING_SCBH = %d\n", 
		       ahc_inb(ahc, WAITING_SCBH));
		printf("TRACEPOINT: SCB_TAG = %d\n", ahc_inb(ahc, SCB_TAG));
#endif
		break;
	}
#if NOT_YET
	/* XXX Fill these in later */
	case MESG_BUFFER_BUSY:
		break;
	case MSGIN_PHASEMIS:
		break;
#endif
	default:
		printf("ahc_intr: seqint, "
		       "intstat == 0x%x, scsisigi = 0x%x\n",
		       intstat, ahc_inb(ahc, SCSISIGI));
		break;
	}
	
unpause:
	/*
	 *  The sequencer is paused immediately on
	 *  a SEQINT, so we should restart it when
	 *  we're done.
	 */
	unpause_sequencer(ahc);
}

STATIC void
ahc_handle_scsiint(ahc, intstat)
	struct ahc_softc *ahc;
	u_int intstat;
{
	u_int	scb_index;
	u_int	status;
	struct	scb *scb;
	char	cur_channel;
	char	intr_channel;

	if ((ahc->features & AHC_TWIN) != 0
	 && ((ahc_inb(ahc, SBLKCTL) & SELBUSB) != 0))
		cur_channel = 'B';
	else
		cur_channel = 'A';
	intr_channel = cur_channel;

	status = ahc_inb(ahc, SSTAT1);
	if (status == 0) {
		if ((ahc->features & AHC_TWIN) != 0) {
			/* Try the other channel */
		 	ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) ^ SELBUSB);
			status = ahc_inb(ahc, SSTAT1);
		 	ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) ^ SELBUSB);
			intr_channel = (cur_channel == 'A') ? 'B' : 'A';
		}
		if (status == 0) {
			printf("%s: Spurious SCSI interrupt\n", ahc_name(ahc));
			return;
		}
	}

	scb_index = ahc_inb(ahc, SCB_TAG);
	if (scb_index < ahc->scb_data->numscbs) {
		scb = &ahc->scb_data->scbarray[scb_index];
		if ((scb->flags & SCB_ACTIVE) == 0
		 || (ahc_inb(ahc, SEQ_FLAGS) & IDENTIFY_SEEN) == 0)
			scb = NULL;
	} else
		scb = NULL;

	if ((status & SCSIRSTI) != 0) {
		printf("%s: Someone reset channel %c\n",
			ahc_name(ahc), intr_channel);
		ahc_reset_channel(ahc, intr_channel, /* Initiate Reset */FALSE);
	} else if ((status & SCSIPERR) != 0) {
		/*
		 * Determine the bus phase and queue an appropriate message.
		 * SCSIPERR is latched true as soon as a parity error
		 * occurs.  If the sequencer acked the transfer that
		 * caused the parity error and the currently presented
		 * transfer on the bus has correct parity, SCSIPERR will
		 * be cleared by CLRSCSIPERR.  Use this to determine if
		 * we should look at the last phase the sequencer recorded,
		 * or the current phase presented on the bus.
		 */
		u_int mesg_out;
		u_int curphase;
		u_int errorphase;
		u_int lastphase;
		int   i;

		lastphase = ahc_inb(ahc, LASTPHASE);
		curphase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;
		ahc_outb(ahc, CLRSINT1, CLRSCSIPERR);
		/*
		 * For all phases save DATA, the sequencer won't
		 * automatically ack a byte that has a parity error
		 * in it.  So the only way that the current phase
		 * could be 'data-in' is if the parity error is for
		 * an already acked byte in the data phase.  During
		 * synchronous data-in transfers, we may actually
		 * ack bytes before latching the current phase in
		 * LASTPHASE, leading to the discrepancy between
		 * curphase and lastphase.
		 */
		if ((ahc_inb(ahc, SSTAT1) & SCSIPERR) != 0
		 || curphase == P_DATAIN)
			errorphase = curphase;
		else
			errorphase = lastphase;

		for (i = 0; i < num_phases; i++) {
			if (errorphase == phase_table[i].phase)
				break;
		}
		mesg_out = phase_table[i].mesg_out;
		if (scb != NULL)
			sc_print_addr(scb->xs->sc_link);
		else
			printf("%s:%c:%d: ", ahc_name(ahc),
			       intr_channel,
			       TCL_TARGET(ahc_inb(ahc, SAVED_TCL)));
		
		printf("parity error detected %s. "
		       "SEQADDR(0x%x) SCSIRATE(0x%x)\n",
		       phase_table[i].phasemsg,
		       ahc_inb(ahc, SEQADDR0) | (ahc_inb(ahc, SEQADDR1) << 8),
		       ahc_inb(ahc, SCSIRATE));

		/*
		 * We've set the hardware to assert ATN if we   
		 * get a parity error on "in" phases, so all we  
		 * need to do is stuff the message buffer with
		 * the appropriate message.  "In" phases have set
		 * mesg_out to something other than MSG_NOP.
		 */
		if (mesg_out != MSG_NOOP) {
			if (ahc->msg_type != MSG_TYPE_NONE)
				ahc->send_msg_perror = TRUE;
			else
				ahc_outb(ahc, MSG_OUT, mesg_out);
		}
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		unpause_sequencer(ahc);
	} else if ((status & BUSFREE) != 0
		&& (ahc_inb(ahc, SIMODE1) & ENBUSFREE) != 0) {
		/*
		 * First look at what phase we were last in.
		 * If its message out, chances are pretty good
		 * that the busfree was in response to one of
		 * our abort requests.
		 */
		u_int lastphase = ahc_inb(ahc, LASTPHASE);
		u_int saved_tcl = ahc_inb(ahc, SAVED_TCL);
		u_int target = TCL_TARGET(saved_tcl);
		u_int initiator_role_id = TCL_SCSI_ID(ahc, saved_tcl);
		char channel = TCL_CHANNEL(ahc, saved_tcl);
		int printerror = 1;

		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (lastphase == P_MESGOUT) {
			u_int message;
			u_int tag;

			message = ahc->msgout_buf[ahc->msgout_index - 1];
			tag = SCB_LIST_NULL;
			switch (message) {
			case MSG_ABORT_TAG:
				tag = scb->hscb->tag;
				/* FALLTRHOUGH */
			case MSG_ABORT:
				sc_print_addr(scb->xs->sc_link);
				printf("SCB %d - Abort %s Completed.\n",
				       scb->hscb->tag, tag == SCB_LIST_NULL ?
				       "" : "Tag");
				ahc_abort_scbs(ahc, target, channel,
					       TCL_LUN(saved_tcl), tag,
					       ROLE_INITIATOR,
					       XS_DRIVER_STUFFUP);
				printerror = 0;
				break;
			case MSG_BUS_DEV_RESET:
			{
				struct ahc_devinfo devinfo;

				if (scb != NULL &&
				    (scb->xs->flags & SCSI_RESET)
				 && ahc_match_scb(scb, target, channel,
						  TCL_LUN(saved_tcl),
						  SCB_LIST_NULL,
						  ROLE_INITIATOR)) {
					ahcsetccbstatus(scb->xs, XS_NOERROR);
				}
				ahc_compile_devinfo(&devinfo,
						    initiator_role_id,
						    target,
						    TCL_LUN(saved_tcl),
						    channel,
						    ROLE_INITIATOR);
				ahc_handle_devreset(ahc, &devinfo,
						    XS_RESET,
						    "Bus Device Reset",
						    /*verbose_level*/0);
				printerror = 0;
				break;
			}
			default:
				break;
			}
		}
		if (printerror != 0) {
			int i;

			if (scb != NULL) {
				u_int tag;

				if ((scb->hscb->control & TAG_ENB) != 0)
					tag = scb->hscb->tag;
				else
					tag = SCB_LIST_NULL;
				ahc_abort_scbs(ahc, target, channel,
					       SCB_LUN(scb), tag,
					       ROLE_INITIATOR,
					       XS_DRIVER_STUFFUP);
			} else {
				/*
				 * We had not fully identified this connection,
				 * so we cannot abort anything.
				 */
				printf("%s: ", ahc_name(ahc));
			}
			for (i = 0; i < num_phases; i++) {
				if (lastphase == phase_table[i].phase)
					break;
			}
			printf("Unexpected busfree %s\n"
			       "SEQADDR == 0x%x\n",
			       phase_table[i].phasemsg, ahc_inb(ahc, SEQADDR0)
				| (ahc_inb(ahc, SEQADDR1) << 8));
		}
		ahc_clear_msg_state(ahc);
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENBUSFREE);
		ahc_outb(ahc, CLRSINT1, CLRBUSFREE|CLRSCSIPERR);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		restart_sequencer(ahc);
	} else if ((status & SELTO) != 0) {
		u_int scbptr;

		scbptr = ahc_inb(ahc, WAITING_SCBH);
		ahc_outb(ahc, SCBPTR, scbptr);
		scb_index = ahc_inb(ahc, SCB_TAG);

		if (scb_index < ahc->scb_data->numscbs) {
			scb = &ahc->scb_data->scbarray[scb_index];
			if ((scb->flags & SCB_ACTIVE) == 0)
				scb = NULL;
		} else
			scb = NULL;

		if (scb == NULL) {
			printf("%s: ahc_intr - referenced scb not "
			       "valid during SELTO scb(%d, %d)\n",
			       ahc_name(ahc), scbptr, scb_index);
		} else {
			u_int tag;

			tag = SCB_LIST_NULL;
			if ((scb->hscb->control & MSG_SIMPLE_Q_TAG) != 0)
				tag = scb->hscb->tag;

			ahc_abort_scbs(ahc, SCB_TARGET(scb), SCB_CHANNEL(scb),
				       SCB_LUN(scb), tag,
				       ROLE_INITIATOR, XS_SELTIMEOUT);
		}
		/* Stop the selection */
		ahc_outb(ahc, SCSISEQ, 0);

		/* No more pending messages */
		ahc_clear_msg_state(ahc);

		/*
		 * Although the driver does not care about the
		 * 'Selection in Progress' status bit, the busy
		 * LED does.  SELINGO is only cleared by a sucessful
		 * selection, so we must manually clear it to ensure
		 * the LED turns off just incase no future successful
		 * selections occur (e.g. no devices on the bus).
		 */
		ahc_outb(ahc, CLRSINT0, CLRSELINGO);

		/* Clear interrupt state */
		ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRBUSFREE|CLRSCSIPERR);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		restart_sequencer(ahc);
	} else {
		sc_print_addr(scb->xs->sc_link);
		printf("Unknown SCSIINT. Status = 0x%x\n", status);
		ahc_outb(ahc, CLRSINT1, status);
		ahc_outb(ahc, CLRINT, CLRSCSIINT);
		unpause_sequencer(ahc);
	}
}

STATIC void
ahc_build_transfer_msg(ahc, devinfo)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
{
	/*
	 * We need to initiate transfer negotiations.
	 * If our current and goal settings are identical,
	 * we want to renegotiate due to a check condition.
	 */
	struct	ahc_initiator_tinfo *tinfo;
	struct	tmode_tstate *tstate;
	int	dowide;
	int	dosync;

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
				    devinfo->our_scsiid,
				    devinfo->target, &tstate);
	dowide = tinfo->current.width != tinfo->goal.width;
	dosync = tinfo->current.period != tinfo->goal.period;

	if (!dowide && !dosync) {
		dowide = tinfo->goal.width != MSG_EXT_WDTR_BUS_8_BIT;
		dosync = tinfo->goal.period != 0;
	}

	if (dowide) {
		ahc_construct_wdtr(ahc, tinfo->goal.width);
	} else if (dosync) {
		struct	ahc_syncrate *rate;
		u_int	period;
		u_int	offset;

		period = tinfo->goal.period;
		rate = ahc_devlimited_syncrate(ahc, &period);
		offset = tinfo->goal.offset;
		ahc_validate_offset(ahc, rate, &offset,
				    tinfo->current.width);
		ahc_construct_sdtr(ahc, period, offset);
	} else {
		panic("ahc_intr: AWAITING_MSG for negotiation, "
		      "but no negotiation needed\n");	
	}
}

STATIC void
ahc_setup_initiator_msgout(ahc, devinfo, scb)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	struct scb *scb;
{
	/*              
	 * To facilitate adding multiple messages together,
	 * each routine should increment the index and len
	 * variables instead of setting them explicitly.
	 */             
	ahc->msgout_index = 0;
	ahc->msgout_len = 0;

	if ((scb->flags & SCB_DEVICE_RESET) == 0
	 && ahc_inb(ahc, MSG_OUT) == MSG_IDENTIFYFLAG) {
		u_int identify_msg;

		identify_msg = MSG_IDENTIFYFLAG | SCB_LUN(scb);
		if ((scb->hscb->control & DISCENB) != 0)
			identify_msg |= MSG_IDENTIFY_DISCFLAG;
		ahc->msgout_buf[ahc->msgout_index++] = identify_msg;
		ahc->msgout_len++;

		if ((scb->hscb->control & TAG_ENB) != 0) {
			/* XXX fvdl FreeBSD has tag action passed down */
			ahc->msgout_buf[ahc->msgout_index++] = MSG_SIMPLE_Q_TAG;
			ahc->msgout_buf[ahc->msgout_index++] = scb->hscb->tag;
			ahc->msgout_len += 2;
		}
	}

	if (scb->flags & SCB_DEVICE_RESET) {
		ahc->msgout_buf[ahc->msgout_index++] = MSG_BUS_DEV_RESET;
		ahc->msgout_len++;
		
		sc_print_addr(scb->xs->sc_link);
		printf("Bus Device Reset Message Sent\n");
	} else if (scb->flags & SCB_ABORT) {
		if ((scb->hscb->control & TAG_ENB) != 0)
			ahc->msgout_buf[ahc->msgout_index++] = MSG_ABORT_TAG;
		else
			ahc->msgout_buf[ahc->msgout_index++] = MSG_ABORT;
		ahc->msgout_len++;
		sc_print_addr(scb->xs->sc_link);
		printf("Abort Message Sent\n");
	} else if ((ahc->targ_msg_req & devinfo->target_mask) != 0) {
		ahc_build_transfer_msg(ahc, devinfo);
	} else {
		printf("ahc_intr: AWAITING_MSG for an SCB that "
		       "does not have a waiting message");
		panic("SCB = %d, SCB Control = %x, MSG_OUT = %x "
		      "SCB flags = %x", scb->hscb->tag, scb->hscb->control,
		      ahc_inb(ahc, MSG_OUT), scb->flags);
	}

	/*
	 * Clear the MK_MESSAGE flag from the SCB so we aren't
	 * asked to send this message again.
	 */
	ahc_outb(ahc, SCB_CONTROL, ahc_inb(ahc, SCB_CONTROL) & ~MK_MESSAGE);
	ahc->msgout_index = 0;
	ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
}

STATIC void
ahc_setup_target_msgin(ahc, devinfo)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
{
	/*              
	 * To facilitate adding multiple messages together,
	 * each routine should increment the index and len
	 * variables instead of setting them explicitly.
	 */             
	ahc->msgout_index = 0;
	ahc->msgout_len = 0;

	if ((ahc->targ_msg_req & devinfo->target_mask) != 0)
		ahc_build_transfer_msg(ahc, devinfo);
	else
		panic("ahc_intr: AWAITING target message with no message");

	ahc->msgout_index = 0;
	ahc->msg_type = MSG_TYPE_TARGET_MSGIN;
}

STATIC int
ahc_handle_msg_reject(ahc, devinfo)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
{
	/*
	 * What we care about here is if we had an
	 * outstanding SDTR or WDTR message for this
	 * target.  If we did, this is a signal that
	 * the target is refusing negotiation.
	 */
	struct scb *scb;
	u_int scb_index;
	u_int last_msg;
	int   response = 0;

	scb_index = ahc_inb(ahc, SCB_TAG);
	scb = &ahc->scb_data->scbarray[scb_index];

	/* Might be necessary */
	last_msg = ahc_inb(ahc, LAST_MSG);

	if (ahc_sent_msg(ahc, MSG_EXT_WDTR, /*full*/FALSE)) {
		struct ahc_initiator_tinfo *tinfo;
		struct tmode_tstate *tstate;

		/* note 8bit xfers */
		printf("%s:%c:%d: refuses WIDE negotiation.  Using "
		       "8bit transfers\n", ahc_name(ahc),
		       devinfo->channel, devinfo->target);
		ahc_set_width(ahc, devinfo,
			      MSG_EXT_WDTR_BUS_8_BIT,
			      AHC_TRANS_ACTIVE|AHC_TRANS_GOAL,
			      /*paused*/TRUE, /*done*/TRUE);
		/*
		 * No need to clear the sync rate.  If the target
		 * did not accept the command, our syncrate is
		 * unaffected.  If the target started the negotiation,
		 * but rejected our response, we already cleared the
		 * sync rate before sending our WDTR.
		 */
		tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
					    devinfo->our_scsiid,
					    devinfo->target, &tstate);
		if (tinfo->goal.period) {
			u_int period;

			/* Start the sync negotiation */
			period = tinfo->goal.period;
			ahc_devlimited_syncrate(ahc, &period);
			ahc->msgout_index = 0;
			ahc->msgout_len = 0;
			ahc_construct_sdtr(ahc, period, tinfo->goal.offset);
			ahc->msgout_index = 0;
			response = 1;
		}
	} else if (ahc_sent_msg(ahc, MSG_EXT_SDTR, /*full*/FALSE)) {
		/* note asynch xfers and clear flag */
		ahc_set_syncrate(ahc, devinfo, /*syncrate*/NULL, /*period*/0,
				 /*offset*/0,
				 AHC_TRANS_ACTIVE|AHC_TRANS_GOAL,
				 /*paused*/TRUE,
				 /*done*/TRUE);
		printf("%s:%c:%d: refuses synchronous negotiation. "
		       "Using asynchronous transfers\n",
		       ahc_name(ahc),
		       devinfo->channel, devinfo->target);
	} else if ((scb->hscb->control & MSG_SIMPLE_Q_TAG) != 0) {
		printf("%s:%c:%d: refuses tagged commands.  Performing "
		       "non-tagged I/O\n", ahc_name(ahc),
		       devinfo->channel, devinfo->target);
			
		ahc_set_tags(ahc, devinfo, FALSE);

		/*
		 * Resend the identify for this CCB as the target
		 * may believe that the selection is invalid otherwise.
		 */
		ahc_outb(ahc, SCB_CONTROL, ahc_inb(ahc, SCB_CONTROL)
					  & ~MSG_SIMPLE_Q_TAG);
	 	scb->hscb->control &= ~MSG_SIMPLE_Q_TAG;
		ahc_outb(ahc, MSG_OUT, MSG_IDENTIFYFLAG);
		ahc_outb(ahc, SCSISIGO, ahc_inb(ahc, SCSISIGO) | ATNO);

		/*
		 * Requeue all tagged commands for this target
		 * currently in our posession so they can be
		 * converted to untagged commands.
		 */
		ahc_search_qinfifo(ahc, SCB_TARGET(scb), SCB_CHANNEL(scb),
				   SCB_LUN(scb), /*tag*/SCB_LIST_NULL,
				   ROLE_INITIATOR, SCB_REQUEUE,
				   SEARCH_COMPLETE);
	} else {
		/*
		 * Otherwise, we ignore it.
		 */
		printf("%s:%c:%d: Message reject for %x -- ignored\n",
		       ahc_name(ahc), devinfo->channel, devinfo->target,
		       last_msg);
	}
	return (response);
}

STATIC void
ahc_clear_msg_state(ahc)
	struct ahc_softc *ahc;
{
	ahc->msgout_len = 0;
	ahc->msgin_index = 0;
	ahc->msg_type = MSG_TYPE_NONE;
	ahc_outb(ahc, MSG_OUT, MSG_NOOP);
}

STATIC void
ahc_handle_message_phase(ahc, sc_link)
	struct ahc_softc *ahc;
	struct scsi_link *sc_link;
{ 
	struct	ahc_devinfo devinfo;
	u_int	bus_phase;
	int	end_session;

	ahc_fetch_devinfo(ahc, &devinfo);
	end_session = FALSE;
	bus_phase = ahc_inb(ahc, SCSISIGI) & PHASE_MASK;

reswitch:
	switch (ahc->msg_type) {
	case MSG_TYPE_INITIATOR_MSGOUT:
	{
		int lastbyte;
		int phasemis;
		int msgdone;

		if (ahc->msgout_len == 0)
			panic("REQINIT interrupt with no active message");

		phasemis = bus_phase != P_MESGOUT;
		if (phasemis) {
			if (bus_phase == P_MESGIN) {
				/*
				 * Change gears and see if
				 * this messages is of interest to
				 * us or should be passed back to
				 * the sequencer.
				 */
				ahc_outb(ahc, CLRSINT1, CLRATNO);
				ahc->send_msg_perror = FALSE;
				ahc->msg_type = MSG_TYPE_INITIATOR_MSGIN;
				ahc->msgin_index = 0;
				goto reswitch;
			}
			end_session = TRUE;
			break;
		}

		if (ahc->send_msg_perror) {
			ahc_outb(ahc, CLRSINT1, CLRATNO);
			ahc_outb(ahc, CLRSINT1, CLRREQINIT);
			ahc_outb(ahc, SCSIDATL, MSG_PARITY_ERROR);
			break;
		}

		msgdone	= ahc->msgout_index == ahc->msgout_len;
		if (msgdone) {
			/*
			 * The target has requested a retry.
			 * Re-assert ATN, reset our message index to
			 * 0, and try again.
			 */
			ahc->msgout_index = 0;
			ahc_outb(ahc, SCSISIGO, ahc_inb(ahc, SCSISIGO) | ATNO);
		}

		lastbyte = ahc->msgout_index == (ahc->msgout_len - 1);
		if (lastbyte) {
			/* Last byte is signified by dropping ATN */
			ahc_outb(ahc, CLRSINT1, CLRATNO);
		}

		/*
		 * Clear our interrupt status and present
		 * the next byte on the bus.
		 */
		ahc_outb(ahc, CLRSINT1, CLRREQINIT);
		ahc_outb(ahc, SCSIDATL, ahc->msgout_buf[ahc->msgout_index++]);
		break;
	}
	case MSG_TYPE_INITIATOR_MSGIN:
	{
		int phasemis;
		int message_done;

		phasemis = bus_phase != P_MESGIN;

		if (phasemis) {
			ahc->msgin_index = 0;
			if (bus_phase == P_MESGOUT
			 && (ahc->send_msg_perror == TRUE
			  || (ahc->msgout_len != 0
			   && ahc->msgout_index == 0))) {
				ahc->msg_type = MSG_TYPE_INITIATOR_MSGOUT;
				goto reswitch;
			}
			end_session = TRUE;
			break;
		}

		/* Pull the byte in without acking it */
		ahc->msgin_buf[ahc->msgin_index] = ahc_inb(ahc, SCSIBUSL);

		message_done = ahc_parse_msg(ahc, sc_link, &devinfo);

		if (message_done) {
			/*
			 * Clear our incoming message buffer in case there
			 * is another message following this one.
			 */
			ahc->msgin_index = 0;

			/*
			 * If this message illicited a response,
			 * assert ATN so the target takes us to the
			 * message out phase.
			 */
			if (ahc->msgout_len != 0)
				ahc_outb(ahc, SCSISIGO,
					 ahc_inb(ahc, SCSISIGO) | ATNO);
		} else 
			ahc->msgin_index++;

		/* Ack the byte */
		ahc_outb(ahc, CLRSINT1, CLRREQINIT);
		ahc_inb(ahc, SCSIDATL);
		break;
	}
	case MSG_TYPE_TARGET_MSGIN:
	{
		int msgdone;
		int msgout_request;

		if (ahc->msgout_len == 0)
			panic("Target MSGIN with no active message");

		/*
		 * If we interrupted a mesgout session, the initiator
		 * will not know this until our first REQ.  So, we
		 * only honor mesgout requests after we've sent our
		 * first byte.
		 */
		if ((ahc_inb(ahc, SCSISIGI) & ATNI) != 0
		 && ahc->msgout_index > 0)
			msgout_request = TRUE;
		else
			msgout_request = FALSE;

		if (msgout_request) {

			/*
			 * Change gears and see if
			 * this messages is of interest to
			 * us or should be passed back to
			 * the sequencer.
			 */
			ahc->msg_type = MSG_TYPE_TARGET_MSGOUT;
			ahc_outb(ahc, SCSISIGO, P_MESGOUT | BSYO);
			ahc->msgin_index = 0;
			/* Dummy read to REQ for first byte */
			ahc_inb(ahc, SCSIDATL);
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
			break;
		}

		msgdone = ahc->msgout_index == ahc->msgout_len;
		if (msgdone) {
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) & ~SPIOEN);
			end_session = TRUE;
			break;
		}

		/*
		 * Present the next byte on the bus.
		 */
		ahc_outb(ahc, SXFRCTL0, ahc_inb(ahc, SXFRCTL0) | SPIOEN);
		ahc_outb(ahc, SCSIDATL, ahc->msgout_buf[ahc->msgout_index++]);
		break;
	}
	case MSG_TYPE_TARGET_MSGOUT:
	{
		int lastbyte;
		int msgdone;

		/*
		 * The initiator signals that this is
		 * the last byte by dropping ATN.
		 */
		lastbyte = (ahc_inb(ahc, SCSISIGI) & ATNI) == 0;

		/*
		 * Read the latched byte, but turn off SPIOEN first
		 * so that we don't inadvertantly cause a REQ for the
		 * next byte.
		 */
		ahc_outb(ahc, SXFRCTL0, ahc_inb(ahc, SXFRCTL0) & ~SPIOEN);
		ahc->msgin_buf[ahc->msgin_index] = ahc_inb(ahc, SCSIDATL);
		msgdone = ahc_parse_msg(ahc, sc_link, &devinfo);
		if (msgdone == MSGLOOP_TERMINATED) {
			/*
			 * The message is *really* done in that it caused
			 * us to go to bus free.  The sequencer has already
			 * been reset at this point, so pull the ejection
			 * handle.
			 */
			return;
		}
		
		ahc->msgin_index++;

		/*
		 * XXX Read spec about initiator dropping ATN too soon
		 *     and use msgdone to detect it.
		 */
		if (msgdone == MSGLOOP_MSGCOMPLETE) {
			ahc->msgin_index = 0;

			/*
			 * If this message illicited a response, transition
			 * to the Message in phase and send it.
			 */
			if (ahc->msgout_len != 0) {
				ahc_outb(ahc, SCSISIGO, P_MESGIN | BSYO);
				ahc_outb(ahc, SXFRCTL0,
					 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
				ahc->msg_type = MSG_TYPE_TARGET_MSGIN;
				ahc->msgin_index = 0;
				break;
			}
		}

		if (lastbyte)
			end_session = TRUE;
		else {
			/* Ask for the next byte. */
			ahc_outb(ahc, SXFRCTL0,
				 ahc_inb(ahc, SXFRCTL0) | SPIOEN);
		}

		break;
	}
	default:
		panic("Unknown REQINIT message type");
	}

	if (end_session) {
		ahc_clear_msg_state(ahc);
		ahc_outb(ahc, RETURN_1, EXIT_MSG_LOOP);
	} else
		ahc_outb(ahc, RETURN_1, CONT_MSG_LOOP);
}

/*
 * See if we sent a particular extended message to the target.
 * If "full" is true, the target saw the full message.
 * If "full" is false, the target saw at least the first
 * byte of the message.
 */
STATIC int
ahc_sent_msg(ahc, msgtype, full)
	struct ahc_softc *ahc;
	u_int msgtype;
	int full;
{
	int found;
	int index;

	found = FALSE;
	index = 0;

	while (index < ahc->msgout_len) {
		if (ahc->msgout_buf[index] == MSG_EXTENDED) {

			/* Found a candidate */
			if (ahc->msgout_buf[index+2] == msgtype) {
				u_int end_index;

				end_index = index + 1
					  + ahc->msgout_buf[index + 1];
				if (full) {
					if (ahc->msgout_index > end_index)
						found = TRUE;
				} else if (ahc->msgout_index > index)
					found = TRUE;
			}
			break;
		} else if (ahc->msgout_buf[index] >= MSG_SIMPLE_Q_TAG
			&& ahc->msgout_buf[index] <= MSG_IGN_WIDE_RESIDUE) {

			/* Skip tag type and tag id or residue param*/
			index += 2;
		} else {
			/* Single byte message */
			index++;
		}
	}
	return (found);
}

STATIC int
ahc_parse_msg(ahc, sc_link, devinfo)
	struct ahc_softc *ahc;
	struct scsi_link *sc_link;
	struct ahc_devinfo *devinfo;
{
	struct	ahc_initiator_tinfo *tinfo;
	struct	tmode_tstate *tstate;
	int	reject;
	int	done;
	int	response;
	u_int	targ_scsirate;

	done = MSGLOOP_IN_PROG;
	response = FALSE;
	reject = FALSE;
	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
				    devinfo->target, &tstate);
	targ_scsirate = tinfo->scsirate;

	/*
	 * Parse as much of the message as is availible,
	 * rejecting it if we don't support it.  When
	 * the entire message is availible and has been
	 * handled, return MSGLOOP_MSGCOMPLETE, indicating
	 * that we have parsed an entire message.
	 *
	 * In the case of extended messages, we accept the length
	 * byte outright and perform more checking once we know the
	 * extended message type.
	 */
	switch (ahc->msgin_buf[0]) {
	case MSG_MESSAGE_REJECT:
		response = ahc_handle_msg_reject(ahc, devinfo);
		/* FALLTHROUGH */
	case MSG_NOOP:
		done = MSGLOOP_MSGCOMPLETE;
		break;
	case MSG_IGN_WIDE_RESIDUE:
	{
		/* Wait for the whole message */
		if (ahc->msgin_index >= 1) {
			if (ahc->msgin_buf[1] != 1
			 || tinfo->current.width == MSG_EXT_WDTR_BUS_8_BIT) {
				reject = TRUE;
				done = MSGLOOP_MSGCOMPLETE;
			} else
				ahc_handle_ign_wide_residue(ahc, devinfo);
		}
		break;
	}
	case MSG_EXTENDED:
	{
		/* Wait for enough of the message to begin validation */
		if (ahc->msgin_index < 2)
			break;
		switch (ahc->msgin_buf[2]) {
		case MSG_EXT_SDTR:
		{
			struct	 ahc_syncrate *syncrate;
			u_int	 period;
			u_int	 offset;
			u_int	 saved_offset;
			
			if (ahc->msgin_buf[1] != MSG_EXT_SDTR_LEN) {
				reject = TRUE;
				break;
			}

			/*
			 * Wait until we have both args before validating
			 * and acting on this message.
			 *
			 * Add one to MSG_EXT_SDTR_LEN to account for
			 * the extended message preamble.
			 */
			if (ahc->msgin_index < (MSG_EXT_SDTR_LEN + 1))
				break;

			period = ahc->msgin_buf[3];
			saved_offset = offset = ahc->msgin_buf[4];
			syncrate = ahc_devlimited_syncrate(ahc, &period);
			ahc_validate_offset(ahc, syncrate, &offset,
					    targ_scsirate & WIDEXFER);
			ahc_set_syncrate(ahc, devinfo,
					 syncrate, period, offset,
					 AHC_TRANS_ACTIVE|AHC_TRANS_GOAL,
					 /*paused*/TRUE, /*done*/TRUE);

			/*
			 * See if we initiated Sync Negotiation
			 * and didn't have to fall down to async
			 * transfers.
			 */
			if (ahc_sent_msg(ahc, MSG_EXT_SDTR, /*full*/TRUE)) {
				/* We started it */
				if (saved_offset != offset) {
					/* Went too low - force async */
					reject = TRUE;
				}
			} else {
				/*
				 * Send our own SDTR in reply
				 */
				if (bootverbose)
					printf("Sending SDTR!\n");
				ahc->msgout_index = 0;
				ahc->msgout_len = 0;
				ahc_construct_sdtr(ahc, period, offset);
				ahc->msgout_index = 0;
				response = TRUE;
			}
			done = MSGLOOP_MSGCOMPLETE;
			break;
		}
		case MSG_EXT_WDTR:
		{
			u_int	bus_width;
			u_int	sending_reply;

			sending_reply = FALSE;
			if (ahc->msgin_buf[1] != MSG_EXT_WDTR_LEN) {
				reject = TRUE;
				break;
			}

			/*
			 * Wait until we have our arg before validating
			 * and acting on this message.
			 *
			 * Add one to MSG_EXT_WDTR_LEN to account for
			 * the extended message preamble.
			 */
			if (ahc->msgin_index < (MSG_EXT_WDTR_LEN + 1))
				break;

			bus_width = ahc->msgin_buf[3];
			if (ahc_sent_msg(ahc, MSG_EXT_WDTR, /*full*/TRUE)) {
				/*
				 * Don't send a WDTR back to the
				 * target, since we asked first.
				 */
				switch (bus_width){
				default:
					/*
					 * How can we do anything greater
					 * than 16bit transfers on a 16bit
					 * bus?
					 */
					reject = TRUE;
					printf("%s: target %d requested %dBit "
					       "transfers.  Rejecting...\n",
					       ahc_name(ahc), devinfo->target,
					       8 * (0x01 << bus_width));
					/* FALLTHROUGH */
				case MSG_EXT_WDTR_BUS_8_BIT:
					bus_width = MSG_EXT_WDTR_BUS_8_BIT;
					break;
				case MSG_EXT_WDTR_BUS_16_BIT:
					break;
				}
			} else {
				/*
				 * Send our own WDTR in reply
				 */
				if (bootverbose)
					printf("Sending WDTR!\n");
				switch (bus_width) {
				default:
					if (ahc->features & AHC_WIDE) {
						/* Respond Wide */
						bus_width =
						    MSG_EXT_WDTR_BUS_16_BIT;
						break;
					}
					/* FALLTHROUGH */
				case MSG_EXT_WDTR_BUS_8_BIT:
					bus_width = MSG_EXT_WDTR_BUS_8_BIT;
					break;
				}
				ahc->msgout_index = 0;
				ahc->msgout_len = 0;
				ahc_construct_wdtr(ahc, bus_width);
				ahc->msgout_index = 0;
				response = TRUE;
				sending_reply = TRUE;
			}
			ahc_set_width(ahc, devinfo, bus_width,
				      AHC_TRANS_ACTIVE|AHC_TRANS_GOAL,
				      /*paused*/TRUE, /*done*/TRUE);

			/* After a wide message, we are async */
			ahc_set_syncrate(ahc, devinfo,
					 /*syncrate*/NULL, /*period*/0,
					 /*offset*/0, AHC_TRANS_ACTIVE,
					 /*paused*/TRUE, /*done*/FALSE);
			if (sending_reply == FALSE && reject == FALSE) {

				if (tinfo->goal.period) {
					struct	ahc_syncrate *rate;
					u_int	period;
					u_int	offset;

					/* Start the sync negotiation */
					period = tinfo->goal.period;
					rate = ahc_devlimited_syncrate(ahc,
								       &period);
					offset = tinfo->goal.offset;
					ahc_validate_offset(ahc, rate, &offset,
							  tinfo->current.width);
					ahc->msgout_index = 0;
					ahc->msgout_len = 0;
					ahc_construct_sdtr(ahc, period, offset);
					ahc->msgout_index = 0;
					response = TRUE;
				}
			}
			done = MSGLOOP_MSGCOMPLETE;
			break;
		}
		default:
			/* Unknown extended message.  Reject it. */
			reject = TRUE;
			break;
		}
		break;
	}
	case MSG_BUS_DEV_RESET:
		ahc_handle_devreset(ahc, devinfo,
				    XS_RESET, "Bus Device Reset Received",
				    /*verbose_level*/0);
		restart_sequencer(ahc);
		done = MSGLOOP_TERMINATED;
		break;
	case MSG_ABORT_TAG:
	case MSG_ABORT:
	case MSG_CLEAR_QUEUE:
		/* Target mode messages */
		if (devinfo->role != ROLE_TARGET) {
			reject = TRUE;
			break;
		}
#if AHC_TARGET_MODE
		ahc_abort_scbs(ahc, devinfo->target, devinfo->channel,
			       devinfo->lun,
			       ahc->msgin_buf[0] == MSG_ABORT_TAG ? SCB_LIST_NULL
			       : ahc_inb(ahc, INITIATOR_TAG),
				ROLE_TARGET, XS_DRIVER_STUFFUP);

		tstate = ahc->enabled_targets[devinfo->our_scsiid];
		if (tstate != NULL) {
			struct tmode_lstate* lstate;

			lstate = tstate->enabled_luns[devinfo->lun];
			if (lstate != NULL) {
				ahc_queue_lstate_event(ahc, lstate,
						       devinfo->our_scsiid,
						       ahc->msgin_buf[0],
						       /*arg*/0);
				ahc_send_lstate_events(ahc, lstate);
			}
		}
		done = MSGLOOP_MSGCOMPLETE;
#else
		panic("ahc: got target mode message");
#endif
		break;
	case MSG_TERM_IO_PROC:
	default:
		reject = TRUE;
		break;
	}

	if (reject) {
		/*
		 * Setup to reject the message.
		 */
		ahc->msgout_index = 0;
		ahc->msgout_len = 1;
		ahc->msgout_buf[0] = MSG_MESSAGE_REJECT;
		done = MSGLOOP_MSGCOMPLETE;
		response = TRUE;
	}

	if (done != MSGLOOP_IN_PROG && !response)
		/* Clear the outgoing message buffer */
		ahc->msgout_len = 0;

	return (done);
}

STATIC void
ahc_handle_ign_wide_residue(ahc, devinfo)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
{
	u_int scb_index;
	struct scb *scb;

	scb_index = ahc_inb(ahc, SCB_TAG);
	scb = &ahc->scb_data->scbarray[scb_index];
	if ((ahc_inb(ahc, SEQ_FLAGS) & DPHASE) == 0
	 || !(scb->xs->flags & SCSI_DATA_IN)) {
		/*
		 * Ignore the message if we haven't
		 * seen an appropriate data phase yet.
		 */
	} else {
		/*
		 * If the residual occurred on the last
		 * transfer and the transfer request was
		 * expected to end on an odd count, do
		 * nothing.  Otherwise, subtract a byte
		 * and update the residual count accordingly.
		 */
		u_int resid_sgcnt;

		resid_sgcnt = ahc_inb(ahc, SCB_RESID_SGCNT);
		if (resid_sgcnt == 0
		 && ahc_inb(ahc, DATA_COUNT_ODD) == 1) {
			/*
			 * If the residual occurred on the last
			 * transfer and the transfer request was
			 * expected to end on an odd count, do
			 * nothing.
			 */
		} else {
			u_int data_cnt;
			u_int data_addr;
			u_int sg_index;

			data_cnt = (ahc_inb(ahc, SCB_RESID_DCNT + 2) << 16)
				 | (ahc_inb(ahc, SCB_RESID_DCNT + 1) << 8)
				 | (ahc_inb(ahc, SCB_RESID_DCNT));

			data_addr = (ahc_inb(ahc, SHADDR + 3) << 24)
				  | (ahc_inb(ahc, SHADDR + 2) << 16)
				  | (ahc_inb(ahc, SHADDR + 1) << 8)
				  | (ahc_inb(ahc, SHADDR));

			data_cnt += 1;
			data_addr -= 1;

			sg_index = scb->sg_count - resid_sgcnt;

			if (sg_index != 0
			 && (scb->sg_list[sg_index].len < data_cnt)) {
				u_int sg_addr;

				sg_index--;
				data_cnt = 1;
				data_addr = scb->sg_list[sg_index].addr
					  + scb->sg_list[sg_index].len - 1;
				
				/*
				 * The physical address base points to the
				 * second entry as it is always used for
				 * calculating the "next S/G pointer".
				 */
				sg_addr = scb->sg_list_phys
					+ (sg_index* sizeof(*scb->sg_list));
				ahc_outb(ahc, SG_NEXT + 3, sg_addr >> 24);
				ahc_outb(ahc, SG_NEXT + 2, sg_addr >> 16);
				ahc_outb(ahc, SG_NEXT + 1, sg_addr >> 8);
				ahc_outb(ahc, SG_NEXT, sg_addr);
			}

			ahc_outb(ahc, SCB_RESID_DCNT + 2, data_cnt >> 16);
			ahc_outb(ahc, SCB_RESID_DCNT + 1, data_cnt >> 8);
			ahc_outb(ahc, SCB_RESID_DCNT, data_cnt);

			ahc_outb(ahc, SHADDR + 3, data_addr >> 24);
			ahc_outb(ahc, SHADDR + 2, data_addr >> 16);
			ahc_outb(ahc, SHADDR + 1, data_addr >> 8);
			ahc_outb(ahc, SHADDR, data_addr);
		}
	}
}

STATIC void
ahc_handle_devreset(ahc, devinfo, status, message, verbose_level)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	int status;
	char *message;
	int verbose_level;
{
	int found;

	found = ahc_abort_scbs(ahc, devinfo->target, devinfo->channel,
			       ALL_LUNS, SCB_LIST_NULL, devinfo->role,
			       status);
	
	/*
	 * Go back to async/narrow transfers and renegotiate.
	 * ahc_set_width and ahc_set_syncrate can cope with NULL
	 * paths.
	 */
	ahc_set_width(ahc, devinfo, MSG_EXT_WDTR_BUS_8_BIT,
		      AHC_TRANS_CUR, /*paused*/TRUE, /*done*/FALSE);
	ahc_set_syncrate(ahc, devinfo, /*syncrate*/NULL,
			 /*period*/0, /*offset*/0, AHC_TRANS_CUR,
			 /*paused*/TRUE, /*done*/FALSE);
	
	if (message != NULL
	 && (verbose_level <= bootverbose))
		printf("%s: %s on %c:%d. %d SCBs aborted\n", ahc_name(ahc),
		       message, devinfo->channel, devinfo->target, found);
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
STATIC void
ahc_done(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	int requeue = 0;
	int target;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_done\n"));
	
	LIST_REMOVE(scb, pend_links);

	untimeout(ahc_timeout, (caddr_t)scb);

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWCMDS) {
		sc_print_addr(sc_link);
		printf("ahc_done opcode %d tag %x\n", xs->cmdstore.opcode,
		    scb->hscb->tag);
	}
#endif
	
	target = sc_link->target;
	
	if (xs->datalen) {
		int op;
	
		if ((xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(ahc->sc_dmat, scb->dmamap, op);
		bus_dmamap_unload(ahc->sc_dmat, scb->dmamap);
	}

	/*
	 * Unbusy this target/channel/lun.
	 * XXX if we are holding two commands per lun, 
	 *     send the next command.
	 */
	ahc_index_busy_tcl(ahc, scb->hscb->tcl, /*unbusy*/TRUE);

	/*
	 * If the recovery SCB completes, we have to be
	 * out of our timeout.
	 */
	if ((scb->flags & SCB_RECOVERY_SCB) != 0) {

		struct	scb *scbp;

		/*
		 * We were able to complete the command successfully,
		 * so reinstate the timeouts for all other pending
		 * commands.
		 */
		scbp = ahc->pending_scbs.lh_first;
		while (scbp != NULL) {
			struct scsi_xfer *txs = scbp->xs;

			if (!(txs->flags & SCSI_POLL)) {
				timeout(ahc_timeout, scbp,
				    (scbp->xs->timeout * hz)/1000);
			}
			scbp = LIST_NEXT(scbp, pend_links);
		}

		/*
		 * Ensure that we didn't put a second instance of this
		 * SCB into the QINFIFO.
		 */
		ahc_search_qinfifo(ahc, SCB_TARGET(scb), SCB_CHANNEL(scb),
				   SCB_LUN(scb), scb->hscb->tag,
				   ROLE_INITIATOR, /*status*/0,
				   SEARCH_REMOVE);
		if (xs->error != XS_NOERROR)
			ahcsetccbstatus(xs, XS_TIMEOUT);
		sc_print_addr(xs->sc_link);
		printf("no longer in timeout, status = %x\n", xs->status);
	}

	if (xs->error != XS_NOERROR) {
		/* Don't clobber any existing error state */
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * bzero the sense data before having
		 * the drive fill it.  The SCSI spec mandates
		 * that any untransfered data should be
		 * assumed to be zero.  Complete the 'bounce'
		 * of sense information through buffers accessible
		 * via bus-space by copying it into the clients
		 * csio.
		 */
		bzero(&xs->sense, sizeof(struct scsi_sense));
		bcopy(&ahc->scb_data->sense[scb->hscb->tag],
		      &xs->sense, scb->sg_list->len);
		xs->error = XS_SENSE;
	}
	if (scb->flags & SCB_FREEZE_QUEUE) {
		ahc->devqueue_blocked[target]--;
		scb->flags &= ~SCB_FREEZE_QUEUE;
	}
	
	requeue = scb->flags & SCB_REQUEUE;
	ahcfreescb(ahc, scb);

	if (requeue) {
		/*
		 * Re-insert at the front of the private queue to
		 * preserve order.
		 */
		int s;

		s = splbio();
		/* TAILQ_INSERT_HEAD(&ahc->sc_q, xs, adapter_q); */
		ahc_list_insert_head(ahc, xs);
		splx(s);
	} else {
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}

	/*
	 * If there are entries in the software queue, try to
	 * run the first one.  We should be more or less guaranteed
	 * to succeed, since we just freed an SCB.
	 *
	 * NOTE: ahc_scsi_cmd() relies on our calling it with
	 * the first entry in the queue.
	 */
	if ((xs = ahc->sc_xxxq.lh_first) != NULL)
		(void) ahc_scsi_cmd(xs);
}

/*
 * Determine the number of SCBs available on the controller
 */
int
ahc_probe_scbs(ahc)
	struct ahc_softc *ahc;
{
	int i;

	for (i = 0; i < AHC_SCB_MAX; i++) {
		ahc_outb(ahc, SCBPTR, i);
		ahc_outb(ahc, SCB_CONTROL, i);
		if (ahc_inb(ahc, SCB_CONTROL) != i)
			break;
		ahc_outb(ahc, SCBPTR, 0);
		if (ahc_inb(ahc, SCB_CONTROL) != 0)
			break;
	}
	
	return (i);
}

/*
 * Start the board, ready for normal operation
 */
int
ahc_init(ahc)
	struct ahc_softc *ahc;
{
	int	  max_targ = 15;
	int	  i;
	int	  term;
	u_int	  scsi_conf;
	u_int	  scsiseq_template;
	u_int	  ultraenb;
	u_int	  discenable;
	u_int	  tagenable;
	size_t	  driver_data_size;
	u_int32_t physaddr;
	struct scb_data *scb_data = NULL;

#ifdef AHC_PRINT_SRAM
	printf("Scratch Ram:");
	for (i = 0x20; i < 0x5f; i++) {
		if (((i % 8) == 0) && (i != 0)) {
			printf ("\n              ");
		}
		printf (" 0x%x", ahc_inb(ahc, i));
	}
	if ((ahc->features & AHC_MORE_SRAM) != 0) {
		for (i = 0x70; i < 0x7f; i++) {
			if (((i % 8) == 0) && (i != 0)) {
				printf ("\n              ");
			}
			printf (" 0x%x", ahc_inb(ahc, i));
		}
	}
	printf ("\n");
#endif

	if (ahc->scb_data == NULL) {
		scb_data = malloc(sizeof (struct scb_data), M_DEVBUF, M_NOWAIT);
		if (scb_data == NULL) {
			printf("%s: cannot malloc scb_data!\n", ahc_name(ahc));
			return (ENOMEM);
		}
		bzero(scb_data, sizeof(struct scb_data));
		ahc->scb_data = scb_data;
	}
	/*
	 * Assume we have a board at this stage and it has been reset.
	 */
	if ((ahc->flags & AHC_USEDEFAULTS) != 0)
		ahc->our_id = ahc->our_id_b = 7;
	
	/*
	 * Default to allowing initiator operations.
	 */
	ahc->flags |= AHC_INITIATORMODE;
	
	/*
	 * DMA tag for our command fifos and other data in system memory
	 * the card's sequencer must be able to access.  For initiator
	 * roles, we need to allocate space for the qinfifo, qoutfifo,
	 * and untagged_scb arrays each of which are composed of 256
	 * 1 byte elements.  When providing for the target mode role,
	 * we additionally must provide space for the incoming target
	 * command fifo.
	 */
	driver_data_size = 3 * 256 * sizeof(u_int8_t);

	if (ahc_createdmamem(ahc, driver_data_size, 
	    &ahc->shared_data_dmamap, (caddr_t *)&ahc->qoutfifo,
	    &ahc->shared_data_busaddr, &ahc->shared_data_seg,
	    &ahc->shared_data_nseg, "shared data") < 0)
		return (ENOMEM);

	ahc->init_level++;

	/* Allocate SCB data now that sc_dmat is initialized */
	if (ahc->scb_data->maxhscbs == 0)
		if (ahcinitscbdata(ahc) != 0)
			return (ENOMEM);

	ahc->qinfifo = &ahc->qoutfifo[256];
	ahc->untagged_scbs = &ahc->qinfifo[256];
	/* There are no untagged SCBs active yet. */
	for (i = 0; i < 256; i++)
		ahc->untagged_scbs[i] = SCB_LIST_NULL;

	/* All of our queues are empty */
	for (i = 0; i < 256; i++)
		ahc->qoutfifo[i] = SCB_LIST_NULL;

	/*
	 * Allocate a tstate to house information for our
	 * initiator presence on the bus as well as the user
	 * data for any target mode initiator.
	 */
	if (ahc_alloc_tstate(ahc, ahc->our_id, 'A') == NULL) {
		printf("%s: unable to allocate tmode_tstate.  "
		       "Failing attach\n", ahc_name(ahc));
		return (-1);
	}

	if ((ahc->features & AHC_TWIN) != 0) {
		if (ahc_alloc_tstate(ahc, ahc->our_id_b, 'B') == NULL) {
			printf("%s: unable to allocate tmode_tstate.  "
			       "Failing attach\n", ahc_name(ahc));
			return (-1);
		}
 		printf("Twin Channel, A SCSI Id=%d, B SCSI Id=%d, primary %c, ",
		       ahc->our_id, ahc->our_id_b,
		       ahc->flags & AHC_CHANNEL_B_PRIMARY? 'B': 'A');
	} else {
		if ((ahc->features & AHC_WIDE) != 0) {
			printf("Wide ");
		} else {
			printf("Single ");
		}
		printf("Channel %c, SCSI Id=%d, ", ahc->channel, ahc->our_id);
	}

	ahc_outb(ahc, SEQ_FLAGS, 0);

	if (ahc->scb_data->maxhscbs < AHC_SCB_MAX) {
		ahc->flags |= AHC_PAGESCBS;
		printf("%d/%d SCBs\n", ahc->scb_data->maxhscbs, AHC_SCB_MAX);
	} else {
		ahc->flags &= ~AHC_PAGESCBS;
		printf("%d SCBs\n", ahc->scb_data->maxhscbs);
	}

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC) {
		printf("%s: hardware scb %d bytes; kernel scb %d bytes; "
		       "ahc_dma %d bytes\n",
			ahc_name(ahc),
		        sizeof(struct hardware_scb),
			sizeof(struct scb),
			sizeof(struct ahc_dma_seg));
	}
#endif /* AHC_DEBUG */

	/* Set the SCSI Id,SXFRCTL0,SXFRCTL1, and SIMODE1, for both channels*/
	if (ahc->features & AHC_TWIN) {

		/*
		 * The device is gated to channel B after a chip reset,
		 * so set those values first
		 */
		term = (ahc->flags & AHC_TERM_ENB_B) != 0 ? STPWEN : 0;
		if ((ahc->features & AHC_ULTRA2) != 0)
			ahc_outb(ahc, SCSIID_ULTRA2, ahc->our_id_b);
		else
			ahc_outb(ahc, SCSIID, ahc->our_id_b);
		scsi_conf = ahc_inb(ahc, SCSICONF + 1);
		ahc_outb(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
					|term|ENSTIMER|ACTNEGEN);
		ahc_outb(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
		ahc_outb(ahc, SXFRCTL0, DFON|SPIOEN);

		if ((scsi_conf & RESET_SCSI) != 0
		 && (ahc->flags & AHC_INITIATORMODE) != 0)
			ahc->flags |= AHC_RESET_BUS_B;

		/* Select Channel A */
		ahc_outb(ahc, SBLKCTL, ahc_inb(ahc, SBLKCTL) & ~SELBUSB);
	}
	term = (ahc->flags & AHC_TERM_ENB_A) != 0 ? STPWEN : 0;
	if ((ahc->features & AHC_ULTRA2) != 0)
		ahc_outb(ahc, SCSIID_ULTRA2, ahc->our_id);
	else
		ahc_outb(ahc, SCSIID, ahc->our_id);
	scsi_conf = ahc_inb(ahc, SCSICONF);
	ahc_outb(ahc, SXFRCTL1, (scsi_conf & (ENSPCHK|STIMESEL))
				|term
				|ENSTIMER|ACTNEGEN);
	ahc_outb(ahc, SIMODE1, ENSELTIMO|ENSCSIRST|ENSCSIPERR);
	ahc_outb(ahc, SXFRCTL0, DFON|SPIOEN);

	if ((scsi_conf & RESET_SCSI) != 0
	 && (ahc->flags & AHC_INITIATORMODE) != 0)
		ahc->flags |= AHC_RESET_BUS_A;

	/*
	 * Look at the information that board initialization or
	 * the board bios has left us.
	 */
	ultraenb = 0;	
	tagenable = ALL_TARGETS_MASK;

	/* Grab the disconnection disable table and invert it for our needs */
	if (ahc->flags & AHC_USEDEFAULTS) {
		printf("%s: Host Adapter Bios disabled.  Using default SCSI "
			"device parameters\n", ahc_name(ahc));
		ahc->flags |= AHC_EXTENDED_TRANS_A|AHC_EXTENDED_TRANS_B|
			      AHC_TERM_ENB_A|AHC_TERM_ENB_B;
		discenable = ALL_TARGETS_MASK;
		if ((ahc->features & AHC_ULTRA) != 0)
			ultraenb = ALL_TARGETS_MASK;
	} else {
		discenable = ~((ahc_inb(ahc, DISC_DSB + 1) << 8)
			   | ahc_inb(ahc, DISC_DSB));
		if ((ahc->features & (AHC_ULTRA|AHC_ULTRA2)) != 0)
			ultraenb = (ahc_inb(ahc, ULTRA_ENB + 1) << 8)
				      | ahc_inb(ahc, ULTRA_ENB);
	}

	if ((ahc->features & (AHC_WIDE|AHC_TWIN)) == 0)
		max_targ = 7;

	for (i = 0; i <= max_targ; i++) {
		struct ahc_initiator_tinfo *tinfo;
		struct tmode_tstate *tstate;
		u_int our_id;
		u_int target_id;
		char channel;

		channel = 'A';
		our_id = ahc->our_id;
		target_id = i;
		if (i > 7 && (ahc->features & AHC_TWIN) != 0) {
			channel = 'B';
			our_id = ahc->our_id_b;
			target_id = i % 8;
		}
		tinfo = ahc_fetch_transinfo(ahc, channel, our_id,
					    target_id, &tstate);
		/* Default to async narrow across the board */
		bzero(tinfo, sizeof(*tinfo));
		if (ahc->flags & AHC_USEDEFAULTS) {
			if ((ahc->features & AHC_WIDE) != 0)
				tinfo->user.width = MSG_EXT_WDTR_BUS_16_BIT;

			/*
			 * These will be truncated when we determine the
			 * connection type we have with the target.
			 */
			tinfo->user.period = ahc_syncrates->period;
			tinfo->user.offset = ~0;
		} else {
			u_int scsirate;
			u_int16_t mask;

			/* Take the settings leftover in scratch RAM. */
			scsirate = ahc_inb(ahc, TARG_SCSIRATE + i);
			mask = (0x01 << i);
			if ((ahc->features & AHC_ULTRA2) != 0) {
				u_int offset;
				u_int maxsync;

				if ((scsirate & SOFS) == 0x0F) {
					/*
					 * Haven't negotiated yet,
					 * so the format is different.
					 */
					scsirate = (scsirate & SXFR) >> 4
						 | (ultraenb & mask)
						  ? 0x08 : 0x0
						 | (scsirate & WIDEXFER);
					offset = MAX_OFFSET_ULTRA2;
				} else
					offset = ahc_inb(ahc, TARG_OFFSET + i);
				maxsync = AHC_SYNCRATE_ULTRA2;
				if ((ahc->features & AHC_DT) != 0)
					maxsync = AHC_SYNCRATE_DT;
				tinfo->user.period =
				    ahc_find_period(ahc, scsirate, maxsync);
				if (offset == 0)
					tinfo->user.period = 0;
				else
					tinfo->user.offset = ~0;
			} else if ((scsirate & SOFS) != 0) {
				tinfo->user.period = 
				    ahc_find_period(ahc, scsirate,
						    (ultraenb & mask)
						   ? AHC_SYNCRATE_ULTRA
						   : AHC_SYNCRATE_FAST);
				if (tinfo->user.period != 0)
					tinfo->user.offset = ~0;
			}
			if ((scsirate & WIDEXFER) != 0
			 && (ahc->features & AHC_WIDE) != 0)
				tinfo->user.width = MSG_EXT_WDTR_BUS_16_BIT;
		}
		tinfo->goal = tinfo->user; /* force negotiation */
		tstate->ultraenb = ultraenb;
		tstate->discenable = discenable;
		tstate->tagenable = 0; /* Wait until the XPT says its okay */
	}
	ahc->user_discenable = discenable;
	ahc->user_tagenable = tagenable;

	/*
	 * Tell the sequencer where it can find our arrays in memory.
	 */
	physaddr = ahc->scb_data->hscb_busaddr;
	ahc_outb(ahc, HSCB_ADDR, physaddr & 0xFF);
	ahc_outb(ahc, HSCB_ADDR + 1, (physaddr >> 8) & 0xFF);
	ahc_outb(ahc, HSCB_ADDR + 2, (physaddr >> 16) & 0xFF);
	ahc_outb(ahc, HSCB_ADDR + 3, (physaddr >> 24) & 0xFF);

	physaddr = ahc->shared_data_busaddr;
	ahc_outb(ahc, SCBID_ADDR, physaddr & 0xFF);
	ahc_outb(ahc, SCBID_ADDR + 1, (physaddr >> 8) & 0xFF);
	ahc_outb(ahc, SCBID_ADDR + 2, (physaddr >> 16) & 0xFF);
	ahc_outb(ahc, SCBID_ADDR + 3, (physaddr >> 24) & 0xFF);

	/* Target mode incomding command fifo */
	physaddr += 3 * 256 * sizeof(u_int8_t);
	ahc_outb(ahc, TMODE_CMDADDR, physaddr & 0xFF);
	ahc_outb(ahc, TMODE_CMDADDR + 1, (physaddr >> 8) & 0xFF);
	ahc_outb(ahc, TMODE_CMDADDR + 2, (physaddr >> 16) & 0xFF);
	ahc_outb(ahc, TMODE_CMDADDR + 3, (physaddr >> 24) & 0xFF);

	/*
	 * Initialize the group code to command length table.
	 * This overrides the values in TARG_SCSIRATE, so only
	 * setup the table after we have processed that information.
	 */
	ahc_outb(ahc, CMDSIZE_TABLE, 5);
	ahc_outb(ahc, CMDSIZE_TABLE + 1, 9);
	ahc_outb(ahc, CMDSIZE_TABLE + 2, 9);
	ahc_outb(ahc, CMDSIZE_TABLE + 3, 0);
	ahc_outb(ahc, CMDSIZE_TABLE + 4, 15);
	ahc_outb(ahc, CMDSIZE_TABLE + 5, 11);
	ahc_outb(ahc, CMDSIZE_TABLE + 6, 0);
	ahc_outb(ahc, CMDSIZE_TABLE + 7, 0);
		
	/* Tell the sequencer of our initial queue positions */
	ahc_outb(ahc, KERNEL_QINPOS, 0);
	ahc_outb(ahc, QINPOS, 0);
	ahc_outb(ahc, QOUTPOS, 0);

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC)
		printf("NEEDSDTR == 0x%x\nNEEDWDTR == 0x%x\n"
		       "DISCENABLE == 0x%x\nULTRAENB == 0x%x\n",
		       ahc->needsdtr_orig, ahc->needwdtr_orig,
		       discenable, ultraenb);
#endif

	/* Don't have any special messages to send to targets */
	ahc_outb(ahc, TARGET_MSG_REQUEST, 0);
	ahc_outb(ahc, TARGET_MSG_REQUEST + 1, 0);

	/*
	 * Use the built in queue management registers
	 * if they are available.
	 */
	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, QOFF_CTLSTA, SCB_QSIZE_256);
		ahc_outb(ahc, SDSCB_QOFF, 0);
		ahc_outb(ahc, SNSCB_QOFF, 0);
		ahc_outb(ahc, HNSCB_QOFF, 0);
	}


	/* We don't have any waiting selections */
	ahc_outb(ahc, WAITING_SCBH, SCB_LIST_NULL);

	/* Our disconnection list is empty too */
	ahc_outb(ahc, DISCONNECTED_SCBH, SCB_LIST_NULL);

	/* Message out buffer starts empty */
	ahc_outb(ahc, MSG_OUT, MSG_NOOP);

	/*
	 * Setup the allowed SCSI Sequences based on operational mode.
	 * If we are a target, we'll enalbe select in operations once
	 * we've had a lun enabled.
	 */
	scsiseq_template = ENSELO|ENAUTOATNO|ENAUTOATNP;
	if ((ahc->flags & AHC_INITIATORMODE) != 0)
		scsiseq_template |= ENRSELI;
	ahc_outb(ahc, SCSISEQ_TEMPLATE, scsiseq_template);

	/*
	 * Load the Sequencer program and Enable the adapter
	 * in "fast" mode.
         */
	if (bootverbose)
		printf("%s: Downloading Sequencer Program...",
		       ahc_name(ahc));

	ahc_loadseq(ahc);

	/* We have to wait until after any system dumps... */
	shutdownhook_establish(ahc_shutdown, ahc);
	return (0);
}

/*
 * Routines to manage a scsi_xfer into the software queue.  
 * We overload xs->free_list to to ensure we don't run into a queue 
 * resource shortage, and keep a pointer to the last entry around 
 * to make insertion O(C).
 */
STATIC INLINE void
ahc_list_insert_before(ahc, xs, next_xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	struct scsi_xfer *next_xs;
{
	LIST_INSERT_BEFORE(xs, next_xs, free_list); 

}

STATIC INLINE void
ahc_list_insert_head(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	if (ahc->sc_xxxq.lh_first == NULL)
		ahc->sc_xxxqlast = xs;
	LIST_INSERT_HEAD(&ahc->sc_xxxq, xs, free_list);
	return;
}

STATIC INLINE void
ahc_list_insert_tail(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	if (ahc->sc_xxxq.lh_first == NULL){
		ahc->sc_xxxqlast = xs;
		LIST_INSERT_HEAD(&ahc->sc_xxxq, xs, free_list);
		return;
	}
	LIST_INSERT_AFTER(ahc->sc_xxxqlast, xs, free_list);
	ahc->sc_xxxqlast = xs;
}

STATIC INLINE void
ahc_list_remove(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	struct scsi_xfer *lxs;
	if (xs == ahc->sc_xxxqlast) {
		lxs = ahc->sc_xxxq.lh_first;
		while (lxs != NULL) {
			if (LIST_NEXT(lxs, free_list) == ahc->sc_xxxqlast) {
                                ahc->sc_xxxqlast = lxs;
				break;
			}
			lxs = LIST_NEXT(xs, free_list);
		}
	}
	
	LIST_REMOVE(xs, free_list);
	if (ahc->sc_xxxq.lh_first == NULL)
		ahc->sc_xxxqlast = NULL;
}

STATIC INLINE struct scsi_xfer *
ahc_list_next(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	return(LIST_NEXT(xs, free_list));
}

/*
 * Pick the first xs for a non-blocked target.
 */
STATIC INLINE struct scsi_xfer *
ahc_first_xs(struct ahc_softc *ahc)
{
	int target;
	struct scsi_xfer *xs = ahc->sc_xxxq.lh_first;

	if (ahc->queue_blocked)
        	return NULL;

	while (xs != NULL) {
		target = xs->sc_link->target;
		if (ahc->devqueue_blocked[target] == 0 &&
		    ahc_index_busy_tcl(ahc, XS_TCL(ahc, xs), FALSE) ==
			SCB_LIST_NULL)
			break;
		xs = LIST_NEXT(xs, free_list);
	}

	return xs;
}

STATIC int32_t
ahc_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_xfer *first_xs, *next_xs = NULL;
	struct ahc_softc *ahc;
	struct scb *scb;
	struct hardware_scb *hscb;	
	struct ahc_initiator_tinfo *tinfo;
	struct tmode_tstate *tstate;
	u_int target_id;
	u_int our_id;
	int s, tcl;
	u_int16_t mask;
	int dontqueue = 0, fromqueue = 0;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("ahc_scsi_cmd\n"));
	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;

	/* must protect the queue */
	s = splbio();

	if (xs == ahc->sc_xxxq.lh_first) {
		/*
		 * Called from ahc_done. Calling with the first entry in
		 * the queue is really just a way of seeing where we're
		 * called from. Now, find the first eligible SCB to send,
		 * e.g. one which will be accepted immediately.
		 */

		if (ahc->queue_blocked) {
			splx(s);
			return (TRY_AGAIN_LATER);
		}

		xs = ahc_first_xs(ahc);
		if (xs == NULL) {
			splx(s);
			return (TRY_AGAIN_LATER);
		}

		next_xs = ahc_list_next(ahc, xs);
		ahc_list_remove(ahc, xs);
		fromqueue = 1;
		goto get_scb;
	}

	/*
	 * If no new requests are accepted, just insert into the
	 * private queue to wait for our turn.
	 */
	tcl = XS_TCL(ahc, xs);

	if (ahc->queue_blocked ||
	    ahc->devqueue_blocked[xs->sc_link->target] ||
	    ahc_index_busy_tcl(ahc, tcl, FALSE) != SCB_LIST_NULL) {
		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return TRY_AGAIN_LATER;
		}
		ahc_list_insert_tail(ahc, xs);
		splx(s);
		return SUCCESSFULLY_QUEUED;
	}

	first_xs = ahc_first_xs(ahc);

	/* determine safety of software queueing */
	dontqueue = xs->flags & SCSI_POLL;

	/*
	 * Handle situations where there's already entries in the
	 * queue.
	 */
	if (first_xs != NULL) {
		/*
		 * If we can't queue, we have to abort, since
		 * we have to preserve order.
		 */
		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * Swap with the first queue entry.
		 */
		ahc_list_insert_tail(ahc, xs);
		xs = first_xs;
		next_xs = ahc_list_next(ahc, xs);
		ahc_list_remove(ahc, xs);
		fromqueue = 1;

	}

get_scb:

	target_id = xs->sc_link->target;
	our_id = SIM_SCSI_ID(ahc, xs->sc_link);

	/*
	 * get an scb to use.
	 */
	if ((scb = ahcgetscb(ahc)) == NULL) {

		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * If we were pulled off the queue, put ourselves
		 * back to where we came from, otherwise tack ourselves
		 * onto the end.
		 */
		if (fromqueue && next_xs != NULL)
			ahc_list_insert_before(ahc, xs, next_xs);
		else
			ahc_list_insert_tail(ahc, xs);

		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}

	tcl = XS_TCL(ahc, xs);

#ifdef DIAGNOSTIC
	if (ahc_index_busy_tcl(ahc, tcl, FALSE) != SCB_LIST_NULL)
		panic("ahc: queuing for busy target");
#endif
	
	scb->xs = xs;
	hscb = scb->hscb;
	hscb->tcl = tcl;

	ahc_busy_tcl(ahc, scb);

	splx(s);
 
	/*
	 * Put all the arguments for the xfer in the scb
	 */

	mask = SCB_TARGET_MASK(scb);
	tinfo = ahc_fetch_transinfo(ahc, SIM_CHANNEL(ahc, xs->sc_link), our_id,
				    target_id, &tstate);
	if (ahc->inited_targets[target_id] == 0) {
		struct ahc_devinfo devinfo;

		s = splbio();
		ahc_compile_devinfo(&devinfo, our_id, target_id,
		    xs->sc_link->lun, SIM_CHANNEL(ahc, xs->sc_link),
		    ROLE_INITIATOR);
		ahc_update_target_msg_request(ahc, &devinfo, tinfo, TRUE,
		    FALSE);
		ahc->inited_targets[target_id] = 1;
		splx(s);
	}

	hscb->scsirate = tinfo->scsirate;
	hscb->scsioffset = tinfo->current.offset;
	if ((tstate->ultraenb & mask) != 0)
		hscb->control |= ULTRAENB;
		
	if ((tstate->discenable & mask) != 0)
		hscb->control |= DISCENB;

	if (xs->flags & SCSI_RESET) {
		hscb->cmdpointer = NULL;
		scb->flags |= SCB_DEVICE_RESET;
		hscb->control |= MK_MESSAGE;
		return ahc_execute_scb(scb, NULL, 0);
	}

	return ahc_setup_data(ahc, xs, scb);
}

STATIC int
ahc_execute_scb(arg, dm_segs, nsegments)
	void *arg;
	bus_dma_segment_t *dm_segs;
	int nsegments;
{
	struct	 scb *scb;
	struct scsi_xfer *xs;
	struct	 ahc_softc *ahc;
	int	 s;

	scb = (struct scb *)arg;
	xs = scb->xs;
	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;


	if (nsegments != 0) {
		struct	  ahc_dma_seg *sg;
		bus_dma_segment_t *end_seg;
		bus_dmasync_op_t op;

		end_seg = dm_segs + nsegments;

		/* Copy the first SG into the data pointer area */
		scb->hscb->data = dm_segs->ds_addr;
		scb->hscb->datalen = dm_segs->ds_len;

		/* Copy the segments into our SG list */
		sg = scb->sg_list;
		while (dm_segs < end_seg) {
			sg->addr = dm_segs->ds_addr;
			sg->len = dm_segs->ds_len;
			sg++;
			dm_segs++;
		}

		/* Note where to find the SG entries in bus space */
		scb->hscb->SG_pointer = scb->sg_list_phys;
		if ((scb->xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;
		bus_dmamap_sync(ahc->sc_dmat, scb->dmamap, op);
	} else {
		scb->hscb->SG_pointer = 0;
		scb->hscb->data = 0;
		scb->hscb->datalen = 0;
	}
	
	scb->sg_count = scb->hscb->SG_count = nsegments;

	s = splbio();

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (xs->flags & ITSDONE) {
		ahc_index_busy_tcl(ahc, scb->hscb->tcl, TRUE);
		if (nsegments != 0)
			bus_dmamap_unload(ahc->sc_dmat, scb->dmamap);
		ahcfreescb(ahc, scb);
		splx(s);
		return (COMPLETE);
	}

#ifdef DIAGNOSTIC
	if (scb->sg_count > 255)
		panic("ahc bad sg_count");
#endif
		
	LIST_INSERT_HEAD(&ahc->pending_scbs, scb, pend_links);

	scb->flags |= SCB_ACTIVE;

	if (!(xs->flags & SCSI_POLL))
	timeout(ahc_timeout, (caddr_t)scb, 
		    (xs->timeout * hz) / 1000);

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
#if 0
		printf("Continueing Immediate Command %d:%d\n",
		       xs->sc_link->target,
		       xs->sc_link->lun);
#endif
		pause_sequencer(ahc);
		if ((ahc->flags & AHC_PAGESCBS) == 0)
			ahc_outb(ahc, SCBPTR, scb->hscb->tag);
		ahc_outb(ahc, SCB_TAG, scb->hscb->tag);
		ahc_outb(ahc, RETURN_1, CONT_MSG_LOOP);
		unpause_sequencer(ahc);
	} else {

		ahc->qinfifo[ahc->qinfifonext++] = scb->hscb->tag;

		bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
				BUS_DMASYNC_PREWRITE);
		
		if ((ahc->features & AHC_QUEUE_REGS) != 0) {
			ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
		} else {
			pause_sequencer(ahc);
			ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
			unpause_sequencer(ahc);
		}
	}

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWCMDS) {
		printf("opcode %d tag %x len %d flags %x control %x fpos %u"
		    " rate %x\n",
		    xs->cmdstore.opcode, scb->hscb->tag, scb->hscb->datalen,
		    scb->flags, scb->hscb->control, ahc->qinfifonext,
		    scb->hscb->scsirate);
	}
#endif

	if (!(xs->flags & SCSI_POLL)) {
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
	/*
	 * If we can't use interrupts, poll for completion
	 */
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_poll\n"));
	do {
		if (ahc_poll(ahc, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			ahc_timeout(scb);
			break;
		}
	} while (!(xs->flags & ITSDONE));
	splx(s);
	return (COMPLETE);
}

STATIC int
ahc_poll(ahc, wait)
	struct   ahc_softc *ahc;
	int   wait;	/* in msec */
{
	while (--wait) {
		DELAY(1000);
		if (ahc_inb(ahc, INTSTAT) & INT_PEND)
			break;
	}

	if (wait == 0) {
		printf("%s: board is not responding\n", ahc_name(ahc));
		return (EIO);
	}
		
	ahc_intr((void *)ahc);
	return (0);
}

STATIC int
ahc_setup_data(ahc, xs, scb)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	struct scb *scb;
{
	struct hardware_scb *hscb;
	
	hscb = scb->hscb;
	xs->resid = xs->status = 0;
	
	hscb->cmdlen = xs->cmdlen;
	bcopy(xs->cmd, hscb->cmdstore, xs->cmdlen);
	hscb->cmdpointer = hscb->cmdstore_busaddr;

	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

		error = bus_dmamap_load(ahc->sc_dmat,
			    scb->dmamap, xs->data,
			    xs->datalen, NULL,
			    (xs->flags & SCSI_NOSLEEP) ?
			    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		if (error) {
			ahc_index_busy_tcl(ahc, hscb->tcl, TRUE);
			return (TRY_AGAIN_LATER);	/* XXX fvdl */
		}
		error = ahc_execute_scb(scb,
		    scb->dmamap->dm_segs,
		    scb->dmamap->dm_nsegs);
		return error;
	} else {
		return ahc_execute_scb(scb, NULL, 0);
	}
}

STATIC void
ahc_freeze_devq(ahc, sc_link)
	struct ahc_softc *ahc;
	struct scsi_link *sc_link;
{
	int	target;
	char	channel;
	int	lun;

	target = sc_link->target;
	lun = sc_link->lun;
	channel = SIM_CHANNEL(ahc, sc_link);
	
	ahc_search_qinfifo(ahc, target, channel, lun,
			   /*tag*/SCB_LIST_NULL, ROLE_UNKNOWN,
			   SCB_REQUEUE, SEARCH_COMPLETE);
}

STATIC void
ahcallocscbs(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;
	struct scb *next_scb;
	struct sg_map_node *sg_map;
	bus_addr_t physaddr;
	struct ahc_dma_seg *segs;
	int newcount;
	int i;
	int dma_flags = 0;

	scb_data = ahc->scb_data;
	if (scb_data->numscbs >= AHC_SCB_MAX)
		/* Can't allocate any more */
		return;

	next_scb = &scb_data->scbarray[scb_data->numscbs];

	sg_map = malloc(sizeof(*sg_map), M_DEVBUF, M_NOWAIT);

	if (sg_map == NULL)
		return;
	bzero(sg_map, sizeof(struct sg_map_node));
	
	if (ahc_createdmamem(ahc, PAGE_SIZE, &sg_map->sg_dmamap,
	    (caddr_t *)&sg_map->sg_vaddr, &sg_map->sg_physaddr,
	    &sg_map->sg_dmasegs, &sg_map->sg_nseg, "SG space") < 0) {
		free(sg_map, M_DEVBUF);
		return;
	}
	
	SLIST_INSERT_HEAD(&scb_data->sg_maps, sg_map, links);

	segs = sg_map->sg_vaddr;
	physaddr = sg_map->sg_physaddr;

	newcount = (PAGE_SIZE / (AHC_NSEG * sizeof(struct ahc_dma_seg)));

	for (i = 0; scb_data->numscbs < AHC_SCB_MAX && i < newcount; i++) {
		int error;

		next_scb->sg_list = segs;
		/*
		 * The sequencer always starts with the second entry.
		 * The first entry is embedded in the scb.
		 */
		next_scb->sg_list_phys = physaddr + sizeof(struct ahc_dma_seg);
		next_scb->flags = SCB_FREE;
		
		/* set up AHA-284x right. */
		dma_flags = ((ahc->chip & AHC_VL) !=0) ? 
			BUS_DMA_NOWAIT|ISABUS_DMA_32BIT :
			BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW;
		
		error = bus_dmamap_create(ahc->sc_dmat,
				 AHC_MAXTRANSFER_SIZE, AHC_NSEG, MAXBSIZE, 0,
				 dma_flags, &next_scb->dmamap);
		if (error !=0) 
			break;

		next_scb->hscb = &scb_data->hscbs[scb_data->numscbs];
		next_scb->hscb->tag = ahc->scb_data->numscbs;
		next_scb->hscb->cmdstore_busaddr = 
			ahc_hscb_busaddr(ahc, next_scb->hscb->tag) + 
			offsetof(struct hardware_scb, cmdstore);	
		SLIST_INSERT_HEAD(&ahc->scb_data->free_scbs, next_scb, links);
		segs += AHC_NSEG;
		physaddr += (AHC_NSEG * sizeof(struct ahc_dma_seg));
		next_scb++;
		ahc->scb_data->numscbs++;
	}
}

#ifdef AHC_DUMP_SEQ
STATIC void
ahc_dumpseq(ahc)
	struct ahc_softc* ahc;
{
	int i;
	int max_prog;

	if ((ahc->chip & AHC_BUS_MASK) < AHC_PCI)
		max_prog = 448;
	else if ((ahc->features & AHC_ULTRA2) != 0)
		max_prog = 768;
	else
		max_prog = 512;

	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS|FASTMODE|LOADRAM);
	ahc_outb(ahc, SEQADDR0, 0);
	ahc_outb(ahc, SEQADDR1, 0);
	for (i = 0; i < max_prog; i++) {
		u_int8_t ins_bytes[4];

		ahc_insb(ahc, SEQRAM, ins_bytes, 4);
		printf("0x%08x\n", ins_bytes[0] << 24
				 | ins_bytes[1] << 16
				 | ins_bytes[2] << 8
				 | ins_bytes[3]);
	}
}
#endif

STATIC void
ahc_loadseq(ahc)
	struct ahc_softc* ahc;
{
	struct patch *cur_patch;
	int i;
	int downloaded;
	int skip_addr;
	u_int8_t download_consts[4];

	/* Setup downloadable constant table */
#if 0
	/* No downloaded constants are currently defined. */
	download_consts[TMODE_NUMCMDS] = ahc->num_targetcmds;
#endif

	cur_patch = patches;
	downloaded = 0;
	skip_addr = 0;
	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS|FASTMODE|LOADRAM);
	ahc_outb(ahc, SEQADDR0, 0);
	ahc_outb(ahc, SEQADDR1, 0);

	for (i = 0; i < sizeof(seqprog)/4; i++) {
		if (ahc_check_patch(ahc, &cur_patch, i, &skip_addr) == 0) {
			/*
			 * Don't download this instruction as it
			 * is in a patch that was removed.
			 */
                        continue;
		}
		ahc_download_instr(ahc, i, download_consts);
		downloaded++;
	}
	ahc_outb(ahc, SEQCTL, PERRORDIS|FAILDIS|FASTMODE);
	restart_sequencer(ahc);

	if (bootverbose)
		printf(" %d instructions downloaded\n", downloaded);
}

STATIC int
ahc_check_patch(ahc, start_patch, start_instr,skip_addr)
	struct ahc_softc *ahc;
	struct patch **start_patch;
	int start_instr;
	int *skip_addr;
{
	struct	patch *cur_patch;
	struct	patch *last_patch;
	int	num_patches;

	num_patches = sizeof(patches)/sizeof(struct patch);
	last_patch = &patches[num_patches];
	cur_patch = *start_patch;

	while (cur_patch < last_patch && start_instr == cur_patch->begin) {

		if (cur_patch->patch_func(ahc) == 0) {

			/* Start rejecting code */
			*skip_addr = start_instr + cur_patch->skip_instr;
			cur_patch += cur_patch->skip_patch;
		} else {
			/* Accepted this patch.  Advance to the next
			 * one and wait for our intruction pointer to
			 * hit this point.
			 */
			cur_patch++;
		}
	}

	*start_patch = cur_patch;
	if (start_instr < *skip_addr)
		/* Still skipping */
		return (0);

	return (1);
}

STATIC void
ahc_download_instr(ahc, instrptr, dconsts)
	struct ahc_softc *ahc;
	int instrptr;
	u_int8_t *dconsts;
{
	union	ins_formats instr;
	struct	ins_format1 *fmt1_ins;
	struct	ins_format3 *fmt3_ins;
	u_int	opcode;

	/* Structure copy */
	instr = *(union ins_formats*)&seqprog[instrptr * 4];

	fmt1_ins = &instr.format1;
	fmt3_ins = NULL;

	/* Pull the opcode */
	opcode = instr.format1.opcode;
	switch (opcode) {
	case AIC_OP_JMP:
	case AIC_OP_JC:
	case AIC_OP_JNC:
	case AIC_OP_CALL:
	case AIC_OP_JNE:
	case AIC_OP_JNZ:
	case AIC_OP_JE:
	case AIC_OP_JZ:
	{
		struct patch *cur_patch;
		int address_offset;
		u_int address;
		int skip_addr;
		int i;

		fmt3_ins = &instr.format3;
		address_offset = 0;
		address = fmt3_ins->address;
		cur_patch = patches;
		skip_addr = 0;

		for (i = 0; i < address;) {

			ahc_check_patch(ahc, &cur_patch, i, &skip_addr);

			if (skip_addr > i) {
				int end_addr;

				end_addr = MIN(address, skip_addr);
				address_offset += end_addr - i;
				i = skip_addr;
			} else {
				i++;
			}
		}
		address -= address_offset;
		fmt3_ins->address = address;
		/* FALLTHROUGH */
	}
	case AIC_OP_OR:
	case AIC_OP_AND:
	case AIC_OP_XOR:
	case AIC_OP_ADD:
	case AIC_OP_ADC:
	case AIC_OP_BMOV:
		if (fmt1_ins->parity != 0) {
			fmt1_ins->immediate = dconsts[fmt1_ins->immediate];
		}
		fmt1_ins->parity = 0;
		/* FALLTHROUGH */
	case AIC_OP_ROL:
		if ((ahc->features & AHC_ULTRA2) != 0) {
			int i, count;

			/* Calculate odd parity for the instruction */
			for (i = 0, count = 0; i < 31; i++) {
				u_int32_t mask;

				mask = 0x01 << i;
				if ((instr.integer & mask) != 0)
					count++;
			}
			if ((count & 0x01) == 0)
				instr.format1.parity = 1;
		} else {
			/* Compress the instruction for older sequencers */
			if (fmt3_ins != NULL) {
				instr.integer =
					fmt3_ins->immediate
				      | (fmt3_ins->source << 8)
				      | (fmt3_ins->address << 16)
				      |	(fmt3_ins->opcode << 25);
			} else {
				instr.integer =
					fmt1_ins->immediate
				      | (fmt1_ins->source << 8)
				      | (fmt1_ins->destination << 16)
				      |	(fmt1_ins->ret << 24)
				      |	(fmt1_ins->opcode << 25);
			}
		}
		ahc_outsb(ahc, SEQRAM, instr.bytes, 4);
		break;
	default:
		panic("Unknown opcode encountered in seq program");
		break;
	}
}

STATIC void
ahc_set_recoveryscb(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct scb *scbp;

		scb->flags |= SCB_RECOVERY_SCB;

		/*
		 * Take all queued, but not sent SCBs out of the equation.
		 * Also ensure that no new CCBs are queued to us while we
		 * try to fix this problem.
		 */
		ahc->queue_blocked = 1;

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		scbp = ahc->pending_scbs.lh_first;
		while (scbp != NULL) {
			untimeout(ahc_timeout, scbp);
			scbp = scbp->pend_links.le_next;
		}
	}
}

STATIC void
ahc_timeout(void *arg)
{
	struct	scb *scb;
	struct	ahc_softc *ahc;
	int	s, found;
	u_int	last_phase;
	int	target;
	int	lun;
	int	i;
	char	channel;

	scb = (struct scb *)arg; 
	ahc = (struct ahc_softc *)scb->xs->sc_link->adapter_softc;

	s = splbio();

	/*
	 * Ensure that the card doesn't do anything
	 * behind our back.  Also make sure that we
	 * didn't "just" miss an interrupt that would
	 * affect this timeout.
	 */
	do {
		ahc_intr(ahc);
		pause_sequencer(ahc);
	} while (ahc_inb(ahc, INTSTAT) & INT_PEND);

	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("Timedout SCB handled by another timeout\n");
		unpause_sequencer(ahc);
		splx(s);
		return;
	}

	target = SCB_TARGET(scb);
	channel = SCB_CHANNEL(scb);
	lun = SCB_LUN(scb);

	sc_print_addr(scb->xs->sc_link);
	printf("SCB 0x%x - timed out ", scb->hscb->tag);
	/*
	 * Take a snapshot of the bus state and print out
	 * some information so we can track down driver bugs.
	 */
	last_phase = ahc_inb(ahc, LASTPHASE);

	for (i = 0; i < num_phases; i++) {
		if (last_phase == phase_table[i].phase)
			break;
	}
	printf("%s", phase_table[i].phasemsg);
  
	printf(", SEQADDR == 0x%x\n",
	       ahc_inb(ahc, SEQADDR0) | (ahc_inb(ahc, SEQADDR1) << 8));
#if 0
	printf("SSTAT1 == 0x%x\n", ahc_inb(ahc, SSTAT1));
	printf("SSTAT3 == 0x%x\n", ahc_inb(ahc, SSTAT3));
	printf("SCSIPHASE == 0x%x\n", ahc_inb(ahc, SCSIPHASE));
	printf("SCSIRATE == 0x%x\n", ahc_inb(ahc, SCSIRATE));
	printf("SCSIOFFSET == 0x%x\n", ahc_inb(ahc, SCSIOFFSET));
	printf("SEQ_FLAGS == 0x%x\n", ahc_inb(ahc, SEQ_FLAGS));
	printf("SCB_DATAPTR == 0x%x\n", ahc_inb(ahc, SCB_DATAPTR)
				      | ahc_inb(ahc, SCB_DATAPTR + 1) << 8
				      | ahc_inb(ahc, SCB_DATAPTR + 2) << 16
				      | ahc_inb(ahc, SCB_DATAPTR + 3) << 24);
	printf("SCB_DATACNT == 0x%x\n", ahc_inb(ahc, SCB_DATACNT)
				      | ahc_inb(ahc, SCB_DATACNT + 1) << 8
				      | ahc_inb(ahc, SCB_DATACNT + 2) << 16);
	printf("SCB_SGCOUNT == 0x%x\n", ahc_inb(ahc, SCB_SGCOUNT));
	printf("CCSCBCTL == 0x%x\n", ahc_inb(ahc, CCSCBCTL));
	printf("CCSCBCNT == 0x%x\n", ahc_inb(ahc, CCSCBCNT));
	printf("DFCNTRL == 0x%x\n", ahc_inb(ahc, DFCNTRL));
	printf("DFSTATUS == 0x%x\n", ahc_inb(ahc, DFSTATUS));
	printf("CCHCNT == 0x%x\n", ahc_inb(ahc, CCHCNT));
	if (scb->sg_count > 0) {
		for (i = 0; i < scb->sg_count; i++) {
			printf("sg[%d] - Addr 0x%x : Length %d\n",
			       i,
			       scb->sg_list[i].addr,
			       scb->sg_list[i].len);
		}
	}
#endif
	if (scb->flags & (SCB_DEVICE_RESET|SCB_ABORT)) {
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
bus_reset:
		ahcsetccbstatus(scb->xs, XS_TIMEOUT);
		found = ahc_reset_channel(ahc, channel, /*Initiate Reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), channel, found);
	} else {
		/*
		 * If we are a target, transition to bus free and report
		 * the timeout.
		 * 
		 * The target/initiator that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * If the bus is idle and we are actiing as the initiator
		 * for this request, queue a BDR message to the timed out
		 * target.  Otherwise, if the timed out transaction is
		 * active:
		 *   Initiator transaction:
		 *	Stuff the message buffer with a BDR message and assert
		 *	ATN in the hopes that the target will let go of the bus
		 *	and go to the mesgout phase.  If this fails, we'll
		 *	get another timeout 2 seconds later which will attempt
		 *	a bus reset.
		 *
		 *   Target transaction:
		 *	Transition to BUS FREE and report the error.
		 *	It's good to be the target!
		 */
		u_int active_scb_index;

		active_scb_index = ahc_inb(ahc, SCB_TAG);

		if (last_phase != P_BUSFREE 
		  && (active_scb_index < ahc->scb_data->numscbs)) {
			struct scb *active_scb;

			/*
			 * If the active SCB is not from our device,
			 * assume that another device is hogging the bus
			 * and wait for it's timeout to expire before
			 * taking additional action.
			 */ 
			active_scb = &ahc->scb_data->scbarray[active_scb_index];
			if (active_scb->hscb->tcl != scb->hscb->tcl) {
				u_int	newtimeout;

				sc_print_addr(scb->xs->sc_link);
				printf("Other SCB Timeout%s",
			 	       (scb->flags & SCB_OTHERTCL_TIMEOUT) != 0
				       ? " again\n" : "\n");
				scb->flags |= SCB_OTHERTCL_TIMEOUT;
				newtimeout = MAX(active_scb->xs->timeout,
						 scb->xs->timeout);
				timeout(ahc_timeout, scb,
					    (newtimeout * hz) / 1000);
				splx(s);
				return;
			} 

			/* It's us */
			if ((scb->hscb->control & TARGET_SCB) != 0) {

				/*
				 * Send back any queued up transactions
				 * and properly record the error condition.
				 */
				ahc_freeze_devq(ahc, scb->xs->sc_link);
				ahcsetccbstatus(scb->xs, XS_TIMEOUT);
				ahc_freeze_ccb(scb);
				ahc_done(ahc, scb);

				/* Will clear us from the bus */
				restart_sequencer(ahc);
				return;
			} 

			ahc_set_recoveryscb(ahc, active_scb);
			ahc_outb(ahc, MSG_OUT, MSG_BUS_DEV_RESET);
			ahc_outb(ahc, SCSISIGO, last_phase|ATNO);
			sc_print_addr(active_scb->xs->sc_link);
			printf("BDR message in message buffer\n");
			active_scb->flags |=  SCB_DEVICE_RESET;
			    timeout(ahc_timeout, (caddr_t)active_scb, 2 * hz);
		} else {
			int	 disconnected;

			/* XXX Shouldn't panic.  Just punt instead */
			if ((scb->hscb->control & TARGET_SCB) != 0)
				panic("Timed-out target SCB but bus idle");

			if (last_phase != P_BUSFREE
			 && (ahc_inb(ahc, SSTAT0) & TARGET) != 0) {
				/* XXX What happened to the SCB? */
				/* Hung target selection.  Goto busfree */
				printf("%s: Hung target selection\n",
				       ahc_name(ahc));
				restart_sequencer(ahc);
				return;
			}

			if (ahc_search_qinfifo(ahc, target, channel, lun,
					       scb->hscb->tag, ROLE_INITIATOR,
					       /*status*/0, SEARCH_COUNT) > 0) {
				disconnected = FALSE;
			} else {
				disconnected = TRUE;
			}

			if (disconnected) {
				u_int active_scb;

				ahc_set_recoveryscb(ahc, scb);
				/*
				 * Simply set the MK_MESSAGE control bit.
				 */
				scb->hscb->control |= MK_MESSAGE;
				scb->flags |= SCB_QUEUED_MSG
					   |  SCB_DEVICE_RESET;

				/*
				 * Mark the cached copy of this SCB in the
				 * disconnected list too, so that a reconnect
				 * at this point causes a BDR or abort.
				 */
				active_scb = ahc_inb(ahc, SCBPTR);
				if (ahc_search_disc_list(ahc, target,
							 channel, lun,
							 scb->hscb->tag,
							 /*stop_on_first*/TRUE,
							 /*remove*/FALSE,
							 /*save_state*/FALSE)) {
					u_int scb_control;

					scb_control = ahc_inb(ahc, SCB_CONTROL);
					scb_control |= MK_MESSAGE;
					ahc_outb(ahc, SCB_CONTROL, scb_control);
				}
				ahc_outb(ahc, SCBPTR, active_scb);
				ahc_index_busy_tcl(ahc, scb->hscb->tcl,
						   /*unbusy*/TRUE);

				/*
				 * Actually re-queue this SCB in case we can
				 * select the device before it reconnects.
				 * Clear out any entries in the QINFIFO first
				 * so we are the next SCB for this target
				 * to run.
				 */
				ahc_search_qinfifo(ahc, SCB_TARGET(scb),
						   channel, SCB_LUN(scb),
						   SCB_LIST_NULL,
						   ROLE_INITIATOR,
						   SCB_REQUEUE,
						   SEARCH_COMPLETE);
				sc_print_addr(scb->xs->sc_link);
				printf("Queuing a BDR SCB\n");
				ahc->qinfifo[ahc->qinfifonext++] =
				    scb->hscb->tag;
				if ((ahc->features & AHC_QUEUE_REGS) != 0) {
					ahc_outb(ahc, HNSCB_QOFF,
						 ahc->qinfifonext);
				} else {
					ahc_outb(ahc, KERNEL_QINPOS,
						 ahc->qinfifonext);
				}
				timeout(ahc_timeout, (caddr_t)scb, 2 * hz);
				unpause_sequencer(ahc);
			} else {
				/* Go "immediatly" to the bus reset */
				/* This shouldn't happen */
				ahc_set_recoveryscb(ahc, scb);
				sc_print_addr(scb->xs->sc_link);
				printf("SCB %d: Immediate reset.  "
					"Flags = 0x%x\n", scb->hscb->tag,
					scb->flags);
				goto bus_reset;
			}
		}
	}
	splx(s);
}

STATIC int
ahc_search_qinfifo(ahc, target, channel, lun, tag, role, status, action)
	struct ahc_softc *ahc;
	int target;
	char channel;
	int lun;
	u_int tag;
	role_t role;
	u_int32_t status;
	ahc_search_action action;
{
	struct	 scb *scbp;
	u_int8_t qinpos;
	u_int8_t qintail;
	int	 found;

	qinpos = ahc_inb(ahc, QINPOS);
	qintail = ahc->qinfifonext;
	found = 0;

	/*
	 * Start with an empty queue.  Entries that are not chosen
	 * for removal will be re-added to the queue as we go.
	 */
	ahc->qinfifonext = qinpos;
	bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
			BUS_DMASYNC_POSTREAD);

	while (qinpos != qintail) {
		scbp = &ahc->scb_data->scbarray[ahc->qinfifo[qinpos]];
		if (ahc_match_scb(scbp, target, channel, lun, tag, role)) {
			/*
			 * We found an scb that needs to be removed.
			 */
			switch (action) {
			case SEARCH_COMPLETE:
				if (!(scbp->xs->flags & ITSDONE)) {
					scbp->flags |= status;
					scbp->xs->error = XS_NOERROR;
				}
				ahc_freeze_ccb(scbp);
				ahc_done(ahc, scbp);
				break;
			case SEARCH_COUNT:
				ahc->qinfifo[ahc->qinfifonext++] =
				    scbp->hscb->tag;
				break;
			case SEARCH_REMOVE:
				break;
			}
			found++;
		} else {
			ahc->qinfifo[ahc->qinfifonext++] = scbp->hscb->tag;
		}
		qinpos++;
	}
	bus_dmamap_sync(ahc->sc_dmat, ahc->shared_data_dmamap, 
			BUS_DMASYNC_PREWRITE);
	
	if ((ahc->features & AHC_QUEUE_REGS) != 0) {
		ahc_outb(ahc, HNSCB_QOFF, ahc->qinfifonext);
	} else {
		ahc_outb(ahc, KERNEL_QINPOS, ahc->qinfifonext);
	}

	return (found);
}

/*
 * Abort all SCBs that match the given description (target/channel/lun/tag),
 * setting their status to the passed in status if the status has not already
 * been modified from CAM_REQ_INPROG.  This routine assumes that the sequencer
 * is paused before it is called.
 */
STATIC int
ahc_abort_scbs(ahc, target, channel, lun, tag, role, status)
	struct ahc_softc *ahc;
	int target;
	char channel;
	int lun;
	u_int tag;
	role_t role;
	u_int32_t status;
{
	struct	scb *scbp;
	u_int	active_scb;
	int	i;
	int	found;

	/* restore this when we're done */
	active_scb = ahc_inb(ahc, SCBPTR);

	found = ahc_search_qinfifo(ahc, target, channel, lun, SCB_LIST_NULL,
				   role, SCB_REQUEUE, SEARCH_COMPLETE);

	/*
	 * Search waiting for selection list.
	 */
	{
		u_int8_t next, prev;
                /* Start at head of list. */
		next = ahc_inb(ahc, WAITING_SCBH);
		prev = SCB_LIST_NULL;

		while (next != SCB_LIST_NULL) {
			u_int8_t scb_index;

			ahc_outb(ahc, SCBPTR, next);
			scb_index = ahc_inb(ahc, SCB_TAG);
			if (scb_index >= ahc->scb_data->numscbs) {
				panic("Waiting List inconsistency. "
				      "SCB index == %d, yet numscbs == %d.",
				      scb_index, ahc->scb_data->numscbs);
			}
			scbp = &ahc->scb_data->scbarray[scb_index];
			if (ahc_match_scb(scbp, target, channel,
					  lun, SCB_LIST_NULL, role)) {

				next = ahc_abort_wscb(ahc, next, prev);
			} else {
				
				prev = next;
				next = ahc_inb(ahc, SCB_NEXT);
			}
		}
	}
	/*
	 * Go through the disconnected list and remove any entries we
	 * have queued for completion, 0'ing their control byte too.
	 * We save the active SCB and restore it ourselves, so there
	 * is no reason for this search to restore it too.
	 */
	ahc_search_disc_list(ahc, target, channel, lun, tag,
			     /*stop_on_first*/FALSE, /*remove*/TRUE,
			     /*save_state*/FALSE);

	/*
	 * Go through the hardware SCB array looking for commands that
	 * were active but not on any list.
	 */
	for(i = 0; i < ahc->scb_data->maxhscbs; i++) {
		u_int scbid;

		ahc_outb(ahc, SCBPTR, i);
		scbid = ahc_inb(ahc, SCB_TAG);
		scbp = &ahc->scb_data->scbarray[scbid];
		if (scbid < ahc->scb_data->numscbs && 
			 ahc_match_scb(scbp, target, channel, lun, tag, role))
				ahc_add_curscb_to_free_list(ahc);
	}

	/*
	 * Go through the pending CCB list and look for
	 * commands for this target that are still active.
	 * These are other tagged commands that were
	 * disconnected when the reset occured.
	 */
	{
		struct scb *scb;

		scb = ahc->pending_scbs.lh_first;
		while (scb != NULL) {
			scbp = scb;
			scb = scb->pend_links.le_next;
			if (ahc_match_scb(scbp, target, channel,
					  lun, tag, role)) {
				if (!(scbp->xs->flags & ITSDONE))
					ahcsetccbstatus(scbp->xs, status);
				ahc_freeze_ccb(scbp);
				ahc_done(ahc, scbp);
				found++;
			}
		}
	}
	ahc_outb(ahc, SCBPTR, active_scb);
	return found;
}

STATIC int
ahc_search_disc_list(ahc, target, channel, lun, tag, stop_on_first, 
		     remove, save_state)
	struct ahc_softc *ahc;
	int target;
	char channel;
	int lun;
	u_int tag;
	int stop_on_first;
	int remove;
	int save_state;
{
	struct	scb *scbp;
	u_int	next;
	u_int	prev;
	u_int	count;
	u_int	active_scb;

	count = 0;
	next = ahc_inb(ahc, DISCONNECTED_SCBH);
	prev = SCB_LIST_NULL;

	if (save_state) {
	/* restore this when we're done */
	active_scb = ahc_inb(ahc, SCBPTR);
	} else
		/* Silence compiler */
		active_scb = SCB_LIST_NULL;

	while (next != SCB_LIST_NULL) {
		u_int scb_index;

		ahc_outb(ahc, SCBPTR, next);
		scb_index = ahc_inb(ahc, SCB_TAG);
		if (scb_index >= ahc->scb_data->numscbs) {
			panic("Disconnected List inconsistency. "
			      "SCB index == %d, yet numscbs == %d.",
			      scb_index, ahc->scb_data->numscbs);
		}
		scbp = &ahc->scb_data->scbarray[scb_index];
		if (ahc_match_scb(scbp, target, channel, lun,
				  tag, ROLE_INITIATOR)) {
			count++;
			if (remove) {
				next =
				    ahc_rem_scb_from_disc_list(ahc, prev, next);
			} else {
				prev = next;
				next = ahc_inb(ahc, SCB_NEXT);
			}
			if (stop_on_first)
				break;
		} else {
			prev = next;
			next = ahc_inb(ahc, SCB_NEXT);
		}
	}
	if (save_state)
	ahc_outb(ahc, SCBPTR, active_scb);
	return (count);
}

STATIC u_int
ahc_rem_scb_from_disc_list(ahc, prev, scbptr)
	struct ahc_softc *ahc;
	u_int prev;
	u_int scbptr;
{
	u_int next;

	ahc_outb(ahc, SCBPTR, scbptr);
	next = ahc_inb(ahc, SCB_NEXT);

	ahc_outb(ahc, SCB_CONTROL, 0);

	ahc_add_curscb_to_free_list(ahc);

	if (prev != SCB_LIST_NULL) {
		ahc_outb(ahc, SCBPTR, prev);
		ahc_outb(ahc, SCB_NEXT, next);
	} else
		ahc_outb(ahc, DISCONNECTED_SCBH, next);

	return (next);
}

STATIC void
ahc_add_curscb_to_free_list(ahc)
	struct ahc_softc *ahc;
{
	/* Invalidate the tag so that ahc_find_scb doesn't think it's active */
	ahc_outb(ahc, SCB_TAG, SCB_LIST_NULL);

	ahc_outb(ahc, SCB_NEXT, ahc_inb(ahc, FREE_SCBH));
	ahc_outb(ahc, FREE_SCBH, ahc_inb(ahc, SCBPTR));
}

/*
 * Manipulate the waiting for selection list and return the
 * scb that follows the one that we remove.
 */
STATIC u_int
ahc_abort_wscb(ahc, scbpos, prev)
	struct ahc_softc *ahc;
	u_int scbpos;
        u_int prev;
{       
	u_int curscb, next;

	/*
	 * Select the SCB we want to abort and
	 * pull the next pointer out of it.
	 */
	curscb = ahc_inb(ahc, SCBPTR);
	ahc_outb(ahc, SCBPTR, scbpos);
	next = ahc_inb(ahc, SCB_NEXT);

	/* Clear the necessary fields */
	ahc_outb(ahc, SCB_CONTROL, 0);

	ahc_add_curscb_to_free_list(ahc);

	/* update the waiting list */
	if (prev == SCB_LIST_NULL) {
		/* First in the list */
		ahc_outb(ahc, WAITING_SCBH, next); 

		/*
		 * Ensure we aren't attempting to perform
		 * selection for this entry.
		 */
		ahc_outb(ahc, SCSISEQ, (ahc_inb(ahc, SCSISEQ) & ~ENSELO));
	} else {
		/*
		 * Select the scb that pointed to us 
		 * and update its next pointer.
		 */
		ahc_outb(ahc, SCBPTR, prev);
		ahc_outb(ahc, SCB_NEXT, next);
	}

	/*
	 * Point us back at the original scb position.
	 */
	ahc_outb(ahc, SCBPTR, curscb);
	return next;
}

STATIC void
ahc_clear_intstat(ahc)
	struct ahc_softc *ahc;
{
	/* Clear any interrupt conditions this may have caused */
	ahc_outb(ahc, CLRSINT0, CLRSELDO|CLRSELDI|CLRSELINGO);
	ahc_outb(ahc, CLRSINT1, CLRSELTIMEO|CLRATNO|CLRSCSIRSTI
				|CLRBUSFREE|CLRSCSIPERR|CLRPHASECHG|
				CLRREQINIT);
	ahc_outb(ahc, CLRINT, CLRSCSIINT);
}

STATIC void
ahc_reset_current_bus(ahc)
	struct ahc_softc *ahc;
{
	u_int8_t scsiseq;

	ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENSCSIRST);
	scsiseq = ahc_inb(ahc, SCSISEQ);
	ahc_outb(ahc, SCSISEQ, scsiseq | SCSIRSTO);
	DELAY(AHC_BUSRESET_DELAY);
	/* Turn off the bus reset */
	ahc_outb(ahc, SCSISEQ, scsiseq & ~SCSIRSTO);

	ahc_clear_intstat(ahc);

	/* Re-enable reset interrupts */
	ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) | ENSCSIRST);
}

STATIC int
ahc_reset_channel(ahc, channel, initiate_reset)
	struct ahc_softc *ahc;
	char channel;
	int initiate_reset;
{
	u_int	initiator, target, max_scsiid;
	u_int	sblkctl;
	u_int	our_id;
	int	found;
	int	restart_needed;
	char	cur_channel;

	ahc->pending_device = NULL;

	pause_sequencer(ahc);

	/*
	 * Run our command complete fifos to ensure that we perform
	 * completion processing on any commands that 'completed'
	 * before the reset occurred.
	 */
	ahc_run_qoutfifo(ahc);

	/*
	 * Reset the bus if we are initiating this reset
	 */
	sblkctl = ahc_inb(ahc, SBLKCTL);
	cur_channel = 'A';
	if ((ahc->features & AHC_TWIN) != 0
	 && ((sblkctl & SELBUSB) != 0))
	    cur_channel = 'B';
	if (cur_channel != channel) {
		/* Case 1: Command for another bus is active
		 * Stealthily reset the other bus without
		 * upsetting the current bus.
		 */
		ahc_outb(ahc, SBLKCTL, sblkctl ^ SELBUSB);
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENBUSFREE);
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		ahc_clear_intstat(ahc);
		ahc_outb(ahc, SBLKCTL, sblkctl);
		restart_needed = FALSE;
	} else {
		/* Case 2: A command from this bus is active or we're idle */
		ahc_clear_msg_state(ahc);
		ahc_outb(ahc, SIMODE1, ahc_inb(ahc, SIMODE1) & ~ENBUSFREE);
		ahc_outb(ahc, SCSISEQ,
			 ahc_inb(ahc, SCSISEQ) & (ENSELI|ENRSELI|ENAUTOATNP));
		if (initiate_reset)
			ahc_reset_current_bus(ahc);
		ahc_clear_intstat(ahc);

		/*
		 * Since we are going to restart the sequencer, avoid
		 * a race in the sequencer that could cause corruption
		 * of our Q pointers by starting over from index 0.
		 */
		ahc->qoutfifonext = 0;
		if ((ahc->features & AHC_QUEUE_REGS) != 0)
			ahc_outb(ahc, SDSCB_QOFF, 0);
		else
			ahc_outb(ahc, QOUTPOS, 0);
		restart_needed = TRUE;
	}

	/*
	 * Clean up all the state information for the
	 * pending transactions on this bus.
	 */
	found = ahc_abort_scbs(ahc, ALL_TARGETS, channel,
			       ALL_LUNS, SCB_LIST_NULL,
			       ROLE_UNKNOWN, XS_RESET);
	if (channel == 'B') {
		our_id = ahc->our_id_b;
	} else {
		our_id = ahc->our_id;
	}

	max_scsiid = (ahc->features & AHC_WIDE) ? 15 : 7;
	
	/*
	 * Revert to async/narrow transfers until we renegotiate.
	 */
	for (target = 0; target <= max_scsiid; target++) {

		if (ahc->enabled_targets[target] == NULL)
			continue;
		for (initiator = 0; initiator <= max_scsiid; initiator++) {
			struct ahc_devinfo devinfo;

			ahc_compile_devinfo(&devinfo, target, initiator,
					    ALL_LUNS,
					    channel, ROLE_UNKNOWN);
			ahc_set_width(ahc, &devinfo,
				      MSG_EXT_WDTR_BUS_8_BIT,
				      AHC_TRANS_CUR,
				      /*paused*/TRUE,
				      /*done*/FALSE);
			ahc_set_syncrate(ahc, &devinfo,
					 /*syncrate*/NULL, /*period*/0,
					 /*offset*/0, AHC_TRANS_CUR,
					 /*paused*/TRUE,
					 /*done*/FALSE);
		}
	}

	if (restart_needed)
		restart_sequencer(ahc);
	else
		unpause_sequencer(ahc);
	return found;
}

STATIC int
ahc_match_scb(scb, target, channel, lun, role, tag)
	struct scb *scb;
	int target;
	char channel;
	int lun;
	role_t role;
	u_int tag;
{
	int targ = SCB_TARGET(scb);
	char chan = SCB_CHANNEL(scb);
	int slun = SCB_LUN(scb);
	int match;

	match = ((chan == channel) || (channel == ALL_CHANNELS));
	if (match != 0)
		match = ((targ == target) || (target == ALL_TARGETS));
	if (match != 0)
		match = ((lun == slun) || (lun == ALL_LUNS));
	return match;
}

STATIC void
ahc_construct_sdtr(ahc, period, offset)
	struct ahc_softc *ahc;
	u_int period;
	u_int offset;
{
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXTENDED;
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXT_SDTR_LEN;
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXT_SDTR;
	ahc->msgout_buf[ahc->msgout_index++] = period;
	ahc->msgout_buf[ahc->msgout_index++] = offset;
	ahc->msgout_len += 5;
}

STATIC void
ahc_construct_wdtr(ahc, bus_width)
	struct ahc_softc *ahc;
	u_int bus_width;
{
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXTENDED;
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXT_WDTR_LEN;
	ahc->msgout_buf[ahc->msgout_index++] = MSG_EXT_WDTR;
	ahc->msgout_buf[ahc->msgout_index++] = bus_width;
	ahc->msgout_len += 4;
}

STATIC void
ahc_calc_residual(scb)
	struct scb *scb;
{
	struct	hardware_scb *hscb;

	hscb = scb->hscb;

	/*
	 * If the disconnected flag is still set, this is bogus
	 * residual information left over from a sequencer
	 * pagin/pageout, so ignore this case.
	 */
	if ((scb->hscb->control & DISCONNECTED) == 0) {
		u_int32_t resid;
		int	  resid_sgs;
		int	  sg;
		
		/*
		 * Remainder of the SG where the transfer
		 * stopped.
		 */
		resid = (hscb->residual_data_count[2] << 16)
		      |	(hscb->residual_data_count[1] <<8)
		      |	(hscb->residual_data_count[0]);

		/*
		 * Add up the contents of all residual
		 * SG segments that are after the SG where
		 * the transfer stopped.
		 */
		resid_sgs = scb->hscb->residual_SG_count - 1/*current*/;
		sg = scb->sg_count - resid_sgs;
		while (resid_sgs > 0) {

			resid += scb->sg_list[sg].len;
			sg++;
			resid_sgs--;
		}
		scb->xs->resid = resid;
	}

	/*
	 * Clean out the residual information in this SCB for its
	 * next consumer.
	 */
	hscb->residual_SG_count = 0;

#ifdef AHC_DEBUG
	if (ahc_debug & AHC_SHOWMISC) {
		sc_print_addr(scb->xs->sc_link);
		printf("Handled Residual of %ld bytes\n" ,scb->xs->resid);
	}
#endif
}

STATIC void
ahc_update_pending_syncrates(ahc)
	struct ahc_softc *ahc;
{
	struct	scb *scb;
	int	pending_scb_count;
	int	i;
	u_int	saved_scbptr;

	/*
	 * Traverse the pending SCB list and ensure that all of the
	 * SCBs there have the proper settings.
	 */
	scb = LIST_FIRST(&ahc->pending_scbs);
	pending_scb_count = 0;
	while (scb != NULL) {
		struct ahc_devinfo devinfo;
		struct scsi_xfer *xs;
		struct scb *pending_scb;
		struct hardware_scb *pending_hscb;
		struct ahc_initiator_tinfo *tinfo;
		struct tmode_tstate *tstate;
		u_int  our_id, remote_id;
		
		xs = scb->xs;
		pending_scb = scb;
		pending_hscb = pending_scb->hscb;
		our_id = SCB_IS_SCSIBUS_B(pending_scb)
		       ? ahc->our_id_b : ahc->our_id;
		remote_id = xs->sc_link->target;
		ahc_compile_devinfo(&devinfo, our_id, remote_id,
				    SCB_LUN(pending_scb),
				    SCB_CHANNEL(pending_scb),
				    ROLE_UNKNOWN);
		tinfo = ahc_fetch_transinfo(ahc, devinfo.channel,
					    our_id, remote_id, &tstate);
		pending_hscb->control &= ~ULTRAENB;
		if ((tstate->ultraenb & devinfo.target_mask) != 0)
			pending_hscb->control |= ULTRAENB;
		pending_hscb->scsirate = tinfo->scsirate;
		pending_hscb->scsioffset = tinfo->current.offset;
		pending_scb_count++;
		scb = LIST_NEXT(scb, pend_links);
	}

	if (pending_scb_count == 0)
		return;

	saved_scbptr = ahc_inb(ahc, SCBPTR);
	/* Ensure that the hscbs down on the card match the new information */
	for (i = 0; i < ahc->scb_data->maxhscbs; i++) {
		u_int scb_tag;

		ahc_outb(ahc, SCBPTR, i);
		scb_tag = ahc_inb(ahc, SCB_TAG);
		if (scb_tag != SCB_LIST_NULL) {
			struct	ahc_devinfo devinfo;
			struct	scb *pending_scb;
			struct scsi_xfer *xs;
			struct	hardware_scb *pending_hscb;
			struct	ahc_initiator_tinfo *tinfo;
			struct	tmode_tstate *tstate;
			u_int	our_id, remote_id;
			u_int	control;

			pending_scb = &ahc->scb_data->scbarray[scb_tag];
			if (pending_scb->flags == SCB_FREE)
				continue;
			pending_hscb = pending_scb->hscb;
			xs = pending_scb->xs;
			our_id = SCB_IS_SCSIBUS_B(pending_scb)
			       ? ahc->our_id_b : ahc->our_id;
			remote_id = xs->sc_link->target;
			ahc_compile_devinfo(&devinfo, our_id, remote_id,
					    SCB_LUN(pending_scb),
					    SCB_CHANNEL(pending_scb),
					    ROLE_UNKNOWN);
			tinfo = ahc_fetch_transinfo(ahc, devinfo.channel,
						    our_id, remote_id, &tstate);
			control = ahc_inb(ahc, SCB_CONTROL);
			control &= ~ULTRAENB;
			if ((tstate->ultraenb & devinfo.target_mask) != 0)
				control |= ULTRAENB;
			ahc_outb(ahc, SCB_CONTROL, control);
			ahc_outb(ahc, SCB_SCSIRATE, tinfo->scsirate);
			ahc_outb(ahc, SCB_SCSIOFFSET, tinfo->current.offset);
		}
	}
	ahc_outb(ahc, SCBPTR, saved_scbptr);
}

STATIC void
ahc_shutdown(void *arg)
{
	struct	ahc_softc *ahc;
	int	i;
	u_int	sxfrctl1_a, sxfrctl1_b;

	ahc = (struct ahc_softc *)arg;

	pause_sequencer(ahc);

	/*
	 * Preserve the value of the SXFRCTL1 register for all channels.
	 * It contains settings that affect termination and we don't want
	 * to disturb the integrity of the bus during shutdown in case
	 * we are in a multi-initiator setup.
	 */
	sxfrctl1_b = 0;
	if ((ahc->features & AHC_TWIN) != 0) {
		u_int sblkctl;

		sblkctl = ahc_inb(ahc, SBLKCTL);
		ahc_outb(ahc, SBLKCTL, sblkctl | SELBUSB);
		sxfrctl1_b = ahc_inb(ahc, SXFRCTL1);
		ahc_outb(ahc, SBLKCTL, sblkctl & ~SELBUSB);
	}

	sxfrctl1_a = ahc_inb(ahc, SXFRCTL1);

	/* This will reset most registers to 0, but not all */
	ahc_reset(ahc);

	if ((ahc->features & AHC_TWIN) != 0) {
		u_int sblkctl;

		sblkctl = ahc_inb(ahc, SBLKCTL);
		ahc_outb(ahc, SBLKCTL, sblkctl | SELBUSB);
		ahc_outb(ahc, SXFRCTL1, sxfrctl1_b);
		ahc_outb(ahc, SBLKCTL, sblkctl & ~SELBUSB);
	}
	ahc_outb(ahc, SXFRCTL1, sxfrctl1_a);

	ahc_outb(ahc, SCSISEQ, 0);
	ahc_outb(ahc, SXFRCTL0, 0);
	ahc_outb(ahc, DSPCISTATUS, 0);

	for (i = TARG_SCSIRATE; i < HA_274_BIOSCTRL; i++)
		ahc_outb(ahc, i, 0);
}
