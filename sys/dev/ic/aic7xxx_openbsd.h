/*
 * OpenBSD platform specific driver option settings, data structures,
 * function declarations and includes. 
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Steve Murphree, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author(s) may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
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
 * $Id: aic7xxx_openbsd.h,v 1.5 2002/06/28 00:34:54 smurph Exp $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic7xxx_freebsd.h,v 1.12 2001/07/18 21:39:47 gibbs Exp $
 * $OpenBSD: aic7xxx_openbsd.h,v 1.5 2002/06/28 00:34:54 smurph Exp $
 */

#ifndef _AIC7XXX_OPENBSD_H_
#define _AIC7XXX_OPENBSD_H_

#include "pci.h"		/* for config options */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcivar.h>

#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <uvm/uvm_extern.h>

#define AHC_SHOWSENSE	0x01
#define AHC_SHOWMISC	0x02
#define AHC_SHOWCMDS	0x04

#if NPCI > 0 
#define AHC_PCI_CONFIG 1
#endif 

#if 0
#define AHC_DEBUG	AHC_SHOWSENSE | AHC_SHOWMISC | AHC_SHOWCMDS
extern int ahc_debug;
#endif 

#ifdef DEBUG
#define bootverbose	1
#else 
#define bootverbose	0
#endif 
/****************************** Platform Macros *******************************/

#define	SCSI_IS_SCSIBUS_B(ahc, sc_link)	\
	((sc_link)->scsibus == (ahc)->platform_data->sc_link_b.scsibus)
#define	SCSI_SCSI_ID(ahc, sc_link)	\
	(SCSI_IS_SCSIBUS_B(ahc, sc_link) ? ahc->our_id_b : ahc->our_id)
#define	SCSI_CHANNEL(ahc, sc_link)	\
	(SCSI_IS_SCSIBUS_B(ahc, sc_link) ? 'B' : 'A')
#define BUILD_SCSIID(ahc, sc_link, target_id, our_id) \
        ((((target_id) << TID_SHIFT) & TID) | (our_id) \
        | (SCSI_IS_SCSIBUS_B(ahc, sc_link) ? TWIN_CHNLB : 0))
#define XS_SCSI_ID(xs) \
	((xs)->sc_link->target)
#define XS_LUN(xs) \
	((xs)->sc_link->lun)
#define XS_TCL(xs) \
	BUILD_TCL(XS_SCSI_ID(xs), XS_LUN(xs))

#ifndef offsetof
#define offsetof(type, member)  ((size_t)(&((type *)0)->member))
#endif

/* COMPAT CAM to XS stuff */
#define CAM_DIR_IN		SCSI_DATA_IN
#define AC_TRANSFER_NEG		0
#define AC_SENT_BDR		0
#define AC_BUS_RESET		0
#define CAM_BUS_WILDCARD	((int)~0)
#define CAM_TARGET_WILDCARD	((int)~0)
#define CAM_LUN_WILDCARD	((int)~0)

/* SPI-3 definitions */
#ifndef MSG_SIMPLE_TASK
#define MSG_SIMPLE_TASK		MSG_SIMPLE_Q_TAG
#endif
#ifndef MSG_ORDERED_TASK
#define MSG_ORDERED_TASK	MSG_ORDERED_Q_TAG
#endif 

/*  FreeBSD to OpenBSD message defs */
#define MSG_EXT_PPR_QAS_REQ	MSG_EXT_PPR_PROT_QAS
#define MSG_EXT_PPR_DT_REQ	MSG_EXT_PPR_PROT_DT	
#define MSG_EXT_PPR_IU_REQ      MSG_EXT_PPR_PROT_IUS

/*  FreeBSD bus_space defines */
#define BUS_SPACE_MAXSIZE_24BIT 0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE       (64 * 1024) /* Maximum supported size */
#define BUS_SPACE_MAXADDR_24BIT 0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR       0xFFFFFFFF

/* CAM  Status field values (From FreeBSD cam.h 1.10 */
typedef enum {
	CAM_REQ_INPROG,		/* CCB request is in progress */
	CAM_REQ_CMP,		/* CCB request completed without error */
	CAM_REQ_ABORTED,	/* CCB request aborted by the host */
	CAM_UA_ABORT,		/* Unable to abort CCB request */
	CAM_REQ_CMP_ERR,	/* CCB request completed with an error */
	CAM_BUSY,		/* CAM subsystem is busy */
	CAM_REQ_INVALID,	/* CCB request was invalid */
	CAM_PATH_INVALID,	/* Supplied Path ID is invalid */
	CAM_DEV_NOT_THERE,	/* SCSI Device Not Installed/there */
	CAM_UA_TERMIO,		/* Unable to terminate I/O CCB request */
	CAM_SEL_TIMEOUT,	/* Target Selection Timeout */
	CAM_CMD_TIMEOUT,	/* Command timeout */
	CAM_SCSI_STATUS_ERROR,	/* SCSI error, look at error code in CCB */
	CAM_MSG_REJECT_REC,	/* Message Reject Received */
	CAM_SCSI_BUS_RESET,	/* SCSI Bus Reset Sent/Received */
	CAM_UNCOR_PARITY,	/* Uncorrectable parity error occurred */
	CAM_AUTOSENSE_FAIL = 0x10,/* Autosense: request sense cmd fail */
	CAM_NO_HBA,		/* No HBA Detected error */
	CAM_DATA_RUN_ERR,	/* Data Overrun error */
	CAM_UNEXP_BUSFREE,	/* Unexpected Bus Free */
	CAM_SEQUENCE_FAIL,	/* Target Bus Phase Sequence Failure */
	CAM_CCB_LEN_ERR,	/* CCB length supplied is inadequate */
	CAM_PROVIDE_FAIL,	/* Unable to provide requested capability */
	CAM_BDR_SENT,		/* A SCSI BDR msg was sent to target */
	CAM_REQ_TERMIO,		/* CCB request terminated by the host */
	CAM_UNREC_HBA_ERROR,	/* Unrecoverable Host Bus Adapter Error */
	CAM_REQ_TOO_BIG,	/* The request was too large for this host */
	CAM_REQUEUE_REQ,	/*
				 * This request should be requeued to preserve
				 * transaction ordering.  This typically occurs
				 * when the SIM recognizes an error that should
				 * freeze the queue and must place additional
				 * requests for the target at the sim level
				 * back into the XPT queue.
				 */
	CAM_IDE = 0x33,		/* Initiator Detected Error */
	CAM_RESRC_UNAVAIL,	/* Resource Unavailable */
	CAM_UNACKED_EVENT,	/* Unacknowledged Event by Host */
	CAM_MESSAGE_RECV,	/* Message Received in Host Target Mode */
	CAM_INVALID_CDB,	/* Invalid CDB received in Host Target Mode */
	CAM_LUN_INVALID,	/* Lun supplied is invalid */
	CAM_TID_INVALID,	/* Target ID supplied is invalid */
	CAM_FUNC_NOTAVAIL,	/* The requested function is not available */
	CAM_NO_NEXUS,		/* Nexus is not established */
	CAM_IID_INVALID,	/* The initiator ID is invalid */
	CAM_CDB_RECVD,		/* The SCSI CDB has been received */
	CAM_LUN_ALRDY_ENA,	/* The LUN is already enabled for target mode */
	CAM_SCSI_BUSY,		/* SCSI Bus Busy */

	CAM_DEV_QFRZN = 0x40,	/* The DEV queue is frozen w/this err */

	/* Autosense data valid for target */
	CAM_AUTOSNS_VALID = 0x80,
	CAM_RELEASE_SIMQ = 0x100,/* SIM ready to take more commands */
	CAM_SIM_QUEUED   = 0x200,/* SIM has this command in it's queue */

	CAM_STATUS_MASK = 0x3F,	/* Mask bits for just the status # */

	/* Target Specific Adjunct Status */
	CAM_SENT_SENSE = 0x40000000	/* sent sense with status */
} cam_status;

/*  FreeBSD to OpenBSD status defs */
#define SCSI_STATUS_CHECK_COND	SCSI_CHECK
#define SCSI_STATUS_CMD_TERMINATED	SCSI_TERMINATED
#define SCSI_STATUS_OK		SCSI_OK
#define SCSI_REV_2		SC_SCSI_2

/************************* Forward Declarations *******************************/
typedef struct pci_attach_args * ahc_dev_softc_t;
typedef struct scsi_xfer * ahc_io_ctx_t;

/***************************** Bus Space/DMA **********************************/

/* XXX Need to update Bus DMA for partial map syncs */
#define ahc_dmamap_sync(ahc, dma_tag, dmamap, offset, len, op)	\
	bus_dmamap_sync(dma_tag, dmamap, offset, len, op)

/************************ Tunable Driver Parameters  **************************/
/*
 * The number of dma segments supported.  The sequencer can handle any number
 * of physically contiguous S/G entrys.  To reduce the driver's memory
 * consumption, we limit the number supported to be sufficient to handle
 * the largest mapping supported by the kernel, MAXPHYS.  Assuming the
 * transfer is as fragmented as possible and unaligned, this turns out to
 * be the number of paged sized transfers in MAXPHYS plus an extra element
 * to handle any unaligned residual.  The sequencer fetches SG elements
 * in cacheline sized chucks, so make the number per-transaction an even
 * multiple of 16 which should align us on even the largest of cacheline
 * boundaries. 
 */
#define AHC_NSEG (roundup(btoc(MAXPHYS) + 1, 16))

/* This driver does NOT supports target mode */
#ifdef AHC_TARGET_MODE
#undef AHC_TARGET_MODE
#endif 

/***************************** Core Includes **********************************/
#include <dev/ic/aic7xxx.h>

/************************** Softc/SCB Platform Data ***************************/
struct ahc_platform_data {
	bus_dma_segment_t	pshared_data_seg;
	int			pshared_data_nseg;
	int			pshared_data_size;
#define shared_data_seg		platform_data->pshared_data_seg
#define shared_data_nseg	platform_data->pshared_data_nseg
#define shared_data_size	platform_data->pshared_data_size
	/*
	 * Hooks into the XPT.
	 */
	struct	scsi_link	sc_link;
        /* Second bus for Twin channel cards */
	struct	scsi_link 	sc_link_b;
	
	void			*ih;
	int			channel_b_primary;
	
	/* for pci error interrupts  */
	int(*pci_intr_func)(struct ahc_softc *);
        
	/* queue management */
	int			queue_blocked;
	u_int16_t		devqueue_blocked[AHC_NUM_TARGETS];
	LIST_HEAD(, scsi_xfer) sc_xxxq;	/* XXX software request queue */
	struct scsi_xfer *sc_xxxqlast;	/* last entry in queue */
	
	u_int8_t		inited_targets[AHC_NUM_TARGETS];
	u_int8_t		inited_channels[2];
};

typedef enum {
	SCB_FREEZE_QUEUE	= 0x0001,
	SCB_REQUEUE		= 0x0002
} scb_pflag;

struct scb_platform_data {
	scb_pflag	flags;
};

/*
 * Some ISA devices (e.g. on a VLB) can perform 32-bit DMA.  This
 * flag is passed to bus_dmamap_create() to indicate that fact.
 */
#ifndef ISABUS_DMA_32BIT
#define ISABUS_DMA_32BIT	BUS_DMA_BUS1
#endif 

/********************************* Byte Order *********************************/
#define ahc_htobe16(x) htobe16(x)
#define ahc_htobe32(x) htobe32(x)
#define ahc_htobe64(x) htobe64(x)
#define ahc_htole16(x) htole16(x)
#define ahc_htole32(x) htole32(x)
#define ahc_htole64(x) htole64(x)
                       
#define ahc_be16toh(x) betoh16(x)
#define ahc_be32toh(x) betoh32(x)
#define ahc_be64toh(x) betoh64(x)
#define ahc_le16toh(x) letoh16(x)
#define ahc_le32toh(x) letoh32(x)
#define ahc_le64toh(x) letoh64(x)

/*************************** Device Access ************************************/
#define ahc_inb(ahc, port)				\
	bus_space_read_1((ahc)->tag, (ahc)->bsh, port)

#define ahc_outb(ahc, port, value)			\
	bus_space_write_1((ahc)->tag, (ahc)->bsh, port, value)

#define ahc_outsb(ahc, port, valp, count)		\
	bus_space_write_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

#define ahc_insb(ahc, port, valp, count)		\
	bus_space_read_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

static __inline void ahc_flush_device_writes(struct ahc_softc *);

static __inline void
ahc_flush_device_writes(ahc)
	struct ahc_softc *ahc;
{
	/* XXX Is this sufficient for all architectures??? */
	ahc_inb(ahc, INTSTAT);
}

/**************************** Locking Primitives ******************************/
/* Lock protecting internal data structures */
static __inline void ahc_lockinit(struct ahc_softc *);
static __inline void ahc_lock(struct ahc_softc *, int *flags);
static __inline void ahc_unlock(struct ahc_softc *, int *flags);

/* Lock held during command compeletion to the upper layer */
static __inline void ahc_done_lockinit(struct ahc_softc *);
static __inline void ahc_done_lock(struct ahc_softc *, int *flags);
static __inline void ahc_done_unlock(struct ahc_softc *, int *flags);

static __inline void
ahc_lockinit(ahc)
	struct ahc_softc *ahc;
{
	/* Nothing to do here for OpenBSD */
}

static __inline void
ahc_lock(ahc, flags)
	struct ahc_softc *ahc;
	int *flags;
{
	*flags = splbio();
}

static __inline void
ahc_unlock(ahc, flags)
	struct ahc_softc *ahc;
	int *flags;
{
	splx(*flags);
}

/* Lock held during command compeletion to the upper layer */
static __inline void
ahc_done_lockinit(ahc)
	struct ahc_softc *ahc;
{
	/* Nothing to do here for OpenBSD */
}

static __inline void
ahc_done_lock(ahc, flags)
	struct ahc_softc *ahc;
	int *flags;
{
	/* Nothing to do here for OpenBSD */
}

static __inline void
ahc_done_unlock(ahc, flags)
	struct ahc_softc *ahc;
	int *flags;
{
	/* Nothing to do here for OpenBSD */
}

/****************************** OS Primitives *********************************/
#define ahc_delay delay

/************************** Transaction Operations ****************************/
static __inline void ahc_set_transaction_status(struct scb *, uint32_t);
static __inline void ahc_set_scsi_status(struct scb *, uint32_t);
static __inline uint32_t ahc_get_transaction_status(struct scb *);
static __inline uint32_t ahc_get_scsi_status(struct scb *);
static __inline void ahc_set_transaction_tag(struct scb *, int, u_int);
static __inline u_long ahc_get_transfer_length(struct scb *);
static __inline int ahc_get_transfer_dir(struct scb *);
static __inline void ahc_set_residual(struct scb *, u_long);
static __inline void ahc_set_sense_residual(struct scb *, u_long);
static __inline u_long ahc_get_residual(struct scb *);
static __inline int ahc_perform_autosense(struct scb *);
static __inline uint32_t ahc_get_sense_bufsize(struct ahc_softc*, struct scb*);
static __inline void ahc_freeze_scb(struct scb *scb);
static __inline void ahc_platform_freeze_devq(struct ahc_softc *, struct scb *);
static __inline int  ahc_platform_abort_scbs(struct ahc_softc *ahc, int target,
						  char channel, int lun, u_int tag,
					     role_t role, uint32_t status);
static __inline void ahc_platform_scb_free(struct ahc_softc *ahc,
					   struct scb *scb);

/* 
 * This is a hack to keep from modifying the main
 * driver code as much as possible.  This function
 * does CAM to SCSI api stuff.
 */
static __inline
void ahc_set_transaction_status(scb, status)
	struct scb *scb;
	uint32_t status;
{
	/* don't wipe the error */
	if (scb->io_ctx->error == XS_NOERROR){
		switch (status) {
		case CAM_CMD_TIMEOUT:
			status = XS_TIMEOUT;
			break;
		case CAM_BDR_SENT:
		case CAM_SCSI_BUS_RESET:
			status = XS_RESET;
			break;
		case CAM_UNEXP_BUSFREE:
		case CAM_REQ_TOO_BIG:
		case CAM_REQ_ABORTED:
		case CAM_AUTOSENSE_FAIL:
		case CAM_NO_HBA:
			status = XS_DRIVER_STUFFUP;
			break;
		case CAM_SEL_TIMEOUT:
			status = XS_SELTIMEOUT;
			break;
		case CAM_REQUEUE_REQ:
			scb->platform_data->flags |= SCB_REQUEUE;
			scb->io_ctx->error = XS_NOERROR;
			break;
		case CAM_SCSI_STATUS_ERROR:
		default:
			status = scb->io_ctx->error;
			break;
		}
	} else {
		status = scb->io_ctx->error;
	}
	scb->io_ctx->error = status;
}

static __inline
void ahc_set_scsi_status(scb, status)
	struct scb *scb;
	uint32_t status;
{
	scb->io_ctx->status = status;
}

/* 
 * This is a hack to keep from modifying the main
 * driver code as much as possible.
 * This function ONLY needs to return weather 
 * a scsi_xfer is in progress or not. XXX smurph
 */
static __inline
uint32_t ahc_get_transaction_status(scb)
	struct scb *scb;
{
	return (scb->io_ctx->flags & ITSDONE ? CAM_REQ_CMP : CAM_REQ_INPROG);
}

static __inline
uint32_t ahc_get_scsi_status(scb)
	struct scb *scb;
{
	return (scb->io_ctx->status);
}

static __inline
void ahc_set_transaction_tag(scb, enabled, type)
	struct scb *scb;
	int enabled;
	u_int type;
{
	struct scsi_xfer *xs = scb->io_ctx;
	switch (type) {
	case MSG_SIMPLE_TASK:
		if (enabled)
			xs->sc_link->quirks &= ~SDEV_NOTAGS;
		else
			xs->sc_link->quirks |= SDEV_NOTAGS;
		break;
	}
}

static __inline
u_long ahc_get_transfer_length(scb)
	struct scb *scb;
{
	return (scb->io_ctx->datalen);
}

static __inline
int ahc_get_transfer_dir(scb)
	struct scb *scb;
{
	return (scb->io_ctx->flags & (SCSI_DATA_IN | SCSI_DATA_OUT));
}

static __inline
void ahc_set_residual(scb, resid)
	struct scb *scb;
	u_long resid;
{
	scb->io_ctx->resid = resid;
}

static __inline
void ahc_set_sense_residual(scb, resid)
	struct scb *scb;
	u_long resid;
{
	scb->io_ctx->resid = resid;
}

static __inline
u_long ahc_get_residual(scb)
	struct scb *scb;
{
	return (scb->io_ctx->resid);
}

static __inline
int ahc_perform_autosense(scb)
	struct scb *scb;
{
	/* Return true for OpenBSD */
	return (1);
}

static __inline uint32_t
ahc_get_sense_bufsize(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	return (sizeof(struct scsi_sense_data));
}

static __inline void
ahc_freeze_scb(scb)
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->io_ctx;
	struct ahc_softc *ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;
	int target;

	target = xs->sc_link->target;
	if (!(scb->platform_data->flags & SCB_FREEZE_QUEUE)) {
		ahc->platform_data->devqueue_blocked[target]++;
		scb->platform_data->flags |= SCB_FREEZE_QUEUE;
	}
}

static __inline void
ahc_platform_freeze_devq(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	/* Nothing to do here for OpenBSD */
}

static __inline int
ahc_platform_abort_scbs(ahc, target, channel, lun, tag, role, status)
	struct ahc_softc *ahc;
	int target, lun;
	char channel;
	u_int tag;
	role_t role;
	uint32_t status;
{
	/* Nothing to do here for OpenBSD */
	return (0);
}

static __inline void
ahc_platform_scb_free(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	int s;

	ahc_lock(ahc, &s);
	
	if ((ahc->flags & AHC_RESOURCE_SHORTAGE) != 0 ||
	    (scb->flags & SCB_RECOVERY_SCB) != 0) {
		ahc->flags &= ~AHC_RESOURCE_SHORTAGE;
		ahc->platform_data->queue_blocked = 0;
	}
	
	timeout_del(&scb->io_ctx->stimeout);
	
	ahc_unlock(ahc, &s);
}

/********************************** PCI ***************************************/
#ifdef AHC_PCI_CONFIG
int                      ahc_pci_map_registers(struct ahc_softc *ahc);
int                      ahc_pci_map_int(struct ahc_softc *ahc);

typedef enum
{
	AHC_POWER_STATE_D0,
	AHC_POWER_STATE_D1,
	AHC_POWER_STATE_D2,
	AHC_POWER_STATE_D3
} ahc_power_state;

void                    ahc_power_state_change(struct ahc_softc *ahc,
						    ahc_power_state new_state);

static __inline uint32_t ahc_pci_read_config(ahc_dev_softc_t pci,
						  int reg, int width);
static __inline void    ahc_pci_write_config(ahc_dev_softc_t pci,
						  int reg, uint32_t value,
						  int width);
static __inline u_int   ahc_get_pci_function(ahc_dev_softc_t);
static __inline u_int   ahc_get_pci_slot(ahc_dev_softc_t);
static __inline u_int   ahc_get_pci_bus(ahc_dev_softc_t);


static __inline uint32_t
ahc_pci_read_config(pa, reg, width)
	ahc_dev_softc_t pa;
	int reg, width;
{
	return (pci_conf_read(pa->pa_pc, pa->pa_tag, reg));
}

static __inline void
ahc_pci_write_config(pa, reg, value, width)
	ahc_dev_softc_t pa;
	uint32_t value;
	int reg, width;
{
	pci_conf_write(pa->pa_pc, pa->pa_tag, reg, value);
}

static __inline u_int
ahc_get_pci_function(pa)
	ahc_dev_softc_t pa;
{
	return (pa->pa_function);
}

static __inline u_int
ahc_get_pci_slot(pa)
	ahc_dev_softc_t pa;
{
	return (pa->pa_device);
}

static __inline u_int
ahc_get_pci_bus(pa)
	ahc_dev_softc_t pa;
{
	return (pa->pa_bus);
}
#endif

/******************************** VL/EISA *************************************/
int aic7770_map_registers(struct ahc_softc *ahc);
int aic7770_map_int(struct ahc_softc *ahc, int irq);

/********************************* Debug **************************************/
static __inline void    ahc_print_path(struct ahc_softc *, struct scb *);
static __inline void    ahc_platform_dump_card_state(struct ahc_softc *ahc);

static __inline void
ahc_print_path(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	sc_print_addr(scb->io_ctx->sc_link);
}

static __inline void
ahc_platform_dump_card_state(ahc)
	struct ahc_softc *ahc;
{
	/* Nothing to do here for OpenBSD */
}
/**************************** Transfer Settings *******************************/
void      ahc_notify_xfer_settings_change(struct ahc_softc *,
					       struct ahc_devinfo *);
void      ahc_platform_set_tags(struct ahc_softc *, struct ahc_devinfo *,
				     ahc_queue_alg);

/************************* Initialization/Teardown ****************************/
int       ahc_platform_alloc(struct ahc_softc *ahc, void *platform_arg);
void      ahc_platform_free(struct ahc_softc *ahc);
int       ahc_attach(struct ahc_softc *);
int       ahc_softc_comp(struct ahc_softc *lahc, struct ahc_softc *rahc);

/****************************** Interrupts ************************************/
int                     ahc_platform_intr(void *);
static __inline void    ahc_platform_flushwork(struct ahc_softc *ahc);

static __inline void
ahc_platform_flushwork(ahc)
	struct ahc_softc *ahc;
{
	/* Nothing to do here for OpenBSD */
}

/************************ Misc Function Declarations **************************/
void    ahc_done(struct ahc_softc *ahc, struct scb *scb);
void    ahc_send_async(struct ahc_softc *, char /*channel*/,
			    u_int /*target*/, u_int /*lun*/, u_int, void *arg);

int     ahc_createdmamem(struct ahc_softc *ahc, bus_dma_tag_t dmat,
			      int size, bus_dmamap_t *mapp, caddr_t *vaddr,
			      bus_addr_t *baddr, bus_dma_segment_t *segs,
			      int *nseg, const char *what);
void    ahc_freedmamem(bus_dma_tag_t tag, int size,
			    bus_dmamap_t map, caddr_t vaddr,
			    bus_dma_segment_t *seg, int nseg);
void    ahc_force_neg(struct ahc_softc *ahc);

/*
 * Routines to manage a scsi_xfer into the software queue.  
 * We overload xs->free_list to to ensure we don't run into a queue 
 * resource shortage, and keep a pointer to the last entry around 
 * to make insertion O(C).
 */
static __inline void   ahc_list_insert_before(struct ahc_softc *ahc,
						   struct scsi_xfer *xs,
						   struct scsi_xfer *next_xs);
static __inline void   ahc_list_insert_head(struct ahc_softc *ahc,
						 struct scsi_xfer *xs);
static __inline void   ahc_list_insert_tail(struct ahc_softc *ahc,
						 struct scsi_xfer *xs);
static __inline void   ahc_list_remove(struct ahc_softc *ahc,
					    struct scsi_xfer *xs);
static __inline struct scsi_xfer *ahc_list_next(struct ahc_softc *ahc,
						     struct scsi_xfer *xs);
static __inline struct scsi_xfer *ahc_first_xs(struct ahc_softc *);

static __inline void
ahc_list_insert_before(ahc, xs, next_xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	struct scsi_xfer *next_xs;
{
	LIST_INSERT_BEFORE(xs, next_xs, free_list); 

}

static __inline void
ahc_list_insert_head(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	if (ahc->platform_data->sc_xxxq.lh_first == NULL)
		ahc->platform_data->sc_xxxqlast = xs;
	LIST_INSERT_HEAD(&ahc->platform_data->sc_xxxq, xs, free_list);
	return;
}

static __inline void
ahc_list_insert_tail(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	if (ahc->platform_data->sc_xxxq.lh_first == NULL){
		ahc->platform_data->sc_xxxqlast = xs;
		LIST_INSERT_HEAD(&ahc->platform_data->sc_xxxq, xs, free_list);
		return;
	}
	LIST_INSERT_AFTER(ahc->platform_data->sc_xxxqlast, xs, free_list);
	ahc->platform_data->sc_xxxqlast = xs;
}

static __inline void
ahc_list_remove(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	struct scsi_xfer *lxs;
	if (xs == ahc->platform_data->sc_xxxqlast) {
		lxs = ahc->platform_data->sc_xxxq.lh_first;
		while (lxs != NULL) {
			if (LIST_NEXT(lxs, free_list) == ahc->platform_data->sc_xxxqlast) {
                                ahc->platform_data->sc_xxxqlast = lxs;
				break;
			}
			lxs = LIST_NEXT(xs, free_list);
		}
	}
	
	LIST_REMOVE(xs, free_list);
	if (ahc->platform_data->sc_xxxq.lh_first == NULL)
		ahc->platform_data->sc_xxxqlast = NULL;
}

static __inline struct scsi_xfer *
ahc_list_next(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	return(LIST_NEXT(xs, free_list));
}

/*
 * Pick the first xs for a non-blocked target.
 */
static __inline struct scsi_xfer *
ahc_first_xs(ahc)
	struct ahc_softc *ahc;
{
	int target;
	struct scsi_xfer *xs = ahc->platform_data->sc_xxxq.lh_first;

	if (ahc->platform_data->queue_blocked)
        	return NULL;

	while (xs != NULL) {
		target = xs->sc_link->target;
		if (ahc->platform_data->devqueue_blocked[target] == 0 &&
		    ahc_index_busy_tcl(ahc, XS_TCL(xs)) == SCB_LIST_NULL)
			break;
		xs = LIST_NEXT(xs, free_list);
	}

	return xs;
}
#endif  /* _AIC7XXX_OPENBSD_H_ */

