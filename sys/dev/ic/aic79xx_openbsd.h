/*	$OpenBSD: aic79xx_openbsd.h,v 1.2 2004/05/20 04:35:47 marco Exp $	*/
/*
 * FreeBSD platform specific driver option settings, data structures,
 * function declarations and includes.
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Adaptec Inc.
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
 * $FreeBSD: src/sys/dev/aic7xxx/aic79xx_osm.h,v 1.13 2003/12/17 00:02:09 gibbs Exp $
 *
 * Additional copyrights by:
 * Milos Urbanek
 * Kenneth R. Westerback
 * Marco Peereboom
 *
 */

#ifndef _AIC79XX_OPENBSD_H_
#define _AIC79XX_OPENBSD_H_

#include "pci.h"		/* for config options */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#define AIC_PCI_CONFIG 1
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <uvm/uvm_extern.h>

#ifdef DEBUG
#define bootverbose     1
#else 
#define bootverbose     0
#endif
/****************************** Platform Macros *******************************/
#define	SCSI_IS_SCSIBUS_B(ahd, sc_link)	\
	(0)
#define	SCSI_CHANNEL(ahd, sc_link)	\
	('A')
#define	SCSI_SCSI_ID(ahd, sc_link)	\
	(ahd->our_id)
#define BUILD_SCSIID(ahd, sc_link, target_id, our_id) \
        ((((target_id) << TID_SHIFT) & TID) | (our_id))
        
#ifndef offsetof
#define offsetof(type, member)  ((size_t)(&((type *)0)->member))
#endif

/************************* Forward Declarations *******************************/typedef struct pci_attach_args * ahd_dev_softc_t;

/***************************** Bus Space/DMA **********************************/

/* XXX Need to update Bus DMA for partial map syncs */
#define ahd_dmamap_sync(ahc, dma_tag, dmamap, offset, len, op)		\
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
#define AHD_NSEG (roundup(btoc(MAXPHYS) + 1, 16))

/* This driver supports target mode */
// #define AHD_TARGET_MODE 1

/************************** Softc/SCB Platform Data ***************************/
struct ahd_platform_data {
};

struct scb_platform_data {
};

/********************************* Byte Order *********************************/
#define ahd_htobe16(x) htobe16(x)
#define ahd_htobe32(x) htobe32(x)
#define ahd_htobe64(x) htobe64(x)
#define ahd_htole16(x) htole16(x)
#define ahd_htole32(x) htole32(x)
#define ahd_htole64(x) htole64(x)

#define ahd_be16toh(x) be16toh(x)
#define ahd_be32toh(x) be32toh(x)
#define ahd_be64toh(x) be64toh(x)
#define ahd_le16toh(x) letoh16(x)
#define ahd_le32toh(x) letoh32(x)
#define ahd_le64toh(x) letoh64(x)

/************************** Timer DataStructures ******************************/
typedef struct timeout ahd_timer_t;

/***************************** Core Includes **********************************/

#if AHD_REG_PRETTY_PRINT
#define AIC_DEBUG_REGISTERS 1
#else
#define AIC_DEBUG_REGISTERS 0
#endif
#include <dev/ic/aic79xx.h>

/***************************** Timer Facilities *******************************/
void ahd_timeout(void*);

ahd_callback_t  ahd_reset_poll;
ahd_callback_t  ahd_stat_timer;

#define ahd_timer_init callout_init
#define ahd_timer_stop callout_stop

static __inline void
ahd_timer_reset(ahd_timer_t *timer, u_int usec, ahd_callback_t *func, void *arg)
{
	callout_reset(timer, (usec * hz)/1000000, func, arg);
}

static __inline void
ahd_scb_timer_reset(struct scb *scb, u_int usec)
{
	if (!(scb->xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout,
		    (usec * hz)/1000000, ahd_timeout, scb);
	}
}

/*************************** Device Access ************************************/
#define ahd_inb(ahd, port)					\
	bus_space_read_1((ahd)->tags[(port) >> 8],		\
			 (ahd)->bshs[(port) >> 8], (port) & 0xFF)

#define ahd_outb(ahd, port, value)				\
	bus_space_write_1((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8], (port) & 0xFF, value)

#define ahd_inw_atomic(ahd, port)				\
	ahd_le16toh(bus_space_read_2((ahd)->tags[(port) >> 8],	\
				     (ahd)->bshs[(port) >> 8], (port) & 0xFF))

#define ahd_outw_atomic(ahd, port, value)			\
	bus_space_write_2((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8],		\
			  (port & 0xFF), ahd_htole16(value))

#define ahd_outsb(ahd, port, valp, count)			\
	bus_space_write_multi_1((ahd)->tags[(port) >> 8],	\
				(ahd)->bshs[(port) >> 8],	\
				(port & 0xFF), valp, count)

#define ahd_insb(ahd, port, valp, count)			\
	bus_space_read_multi_1((ahd)->tags[(port) >> 8],	\
			       (ahd)->bshs[(port) >> 8],	\
			       (port & 0xFF), valp, count)

static __inline void ahd_flush_device_writes(struct ahd_softc *);

static __inline void
ahd_flush_device_writes(struct ahd_softc *ahd)
{
	/* XXX Is this sufficient for all architectures??? */
	ahd_inb(ahd, INTSTAT);
}

/**************************** Locking Primitives ******************************/
/* Lock protecting internal data structures */
static __inline void ahd_lockinit(struct ahd_softc *);
static __inline void ahd_lock(struct ahd_softc *, int *flags);
static __inline void ahd_unlock(struct ahd_softc *, int *flags);

/* Lock held during command compeletion to the upper layer */
static __inline void ahd_done_lockinit(struct ahd_softc *);
static __inline void ahd_done_lock(struct ahd_softc *, int *flags);
static __inline void ahd_done_unlock(struct ahd_softc *, int *flags);

/* Lock held during ahd_list manipulation and ahd softc frees */
static __inline void ahd_list_lockinit(void);
static __inline void ahd_list_lock(int *flags);
static __inline void ahd_list_unlock(int *flags);

static __inline void
ahd_lockinit(struct ahd_softc *ahd)
{
}

static __inline void
ahd_lock(struct ahd_softc *ahd, int *flags)
{
	*flags = splbio();
}

static __inline void
ahd_unlock(struct ahd_softc *ahd, int *flags)
{
	splx(*flags);
}

/* Lock held during command compeletion to the upper layer */
static __inline void
ahd_done_lockinit(struct ahd_softc *ahd)
{
}

static __inline void
ahd_done_lock(struct ahd_softc *ahd, int *flags)
{
}

static __inline void
ahd_done_unlock(struct ahd_softc *ahd, int *flags)
{
}

/* Lock held during ahd_list manipulation and ahd softc frees */
static __inline void
ahd_list_lockinit(void)
{
}

static __inline void
ahd_list_lock(int *flags)
{
}

static __inline void
ahd_list_unlock(int *flags)
{
}

/****************************** OS Primitives *********************************/
#define ahd_delay DELAY

/************************** Transaction Operations ****************************/
static __inline void ahd_set_transaction_status(struct scb *, uint32_t);
static __inline void ahd_set_scsi_status(struct scb *, uint32_t);
static __inline uint32_t ahd_get_transaction_status(struct scb *);
static __inline uint32_t ahd_get_scsi_status(struct scb *);
static __inline void ahd_set_transaction_tag(struct scb *, int, u_int);
static __inline u_long ahd_get_transfer_length(struct scb *);
static __inline int ahd_get_transfer_dir(struct scb *);
static __inline void ahd_set_residual(struct scb *, u_long);
static __inline void ahd_set_sense_residual(struct scb *, u_long);
static __inline u_long ahd_get_residual(struct scb *);
static __inline int ahd_perform_autosense(struct scb *);
static __inline uint32_t ahd_get_sense_bufsize(struct ahd_softc*, struct scb*);
static __inline void ahd_freeze_simq(struct ahd_softc *);
static __inline void ahd_release_simq(struct ahd_softc *);
static __inline void ahd_freeze_scb(struct scb *);
static __inline void ahd_platform_freeze_devq(struct ahd_softc *, struct scb *);
static __inline int  ahd_platform_abort_scbs(struct ahd_softc *, int,
		    char, int, u_int, role_t, uint32_t);

static __inline
void ahd_set_transaction_status(struct scb *scb, uint32_t status)
{
	scb->xs->error = status;
}

static __inline
void ahd_set_scsi_status(struct scb *scb, uint32_t status)
{
	scb->xs->xs_status = status;
}

static __inline
uint32_t ahd_get_transaction_status(struct scb *scb)
{
	if (scb->xs->flags & ITSDONE)
		return CAM_REQ_CMP;
	else
		return scb->xs->error;
}

static __inline
uint32_t ahd_get_scsi_status(struct scb *scb)
{
	return (scb->xs->status);
}

static __inline
void ahd_set_transaction_tag(struct scb *scb, int enabled, u_int type)
{
}

static __inline
u_long ahd_get_transfer_length(struct scb *scb)
{
	return (scb->xs->datalen);
}

static __inline
int ahd_get_transfer_dir(struct scb *scb)
{
	return (scb->xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT));
}

static __inline
void ahd_set_residual(struct scb *scb, u_long resid)
{
	scb->xs->resid = resid;
}

static __inline
void ahd_set_sense_residual(struct scb *scb, u_long resid)
{
	scb->xs->resid = resid;
}

static __inline
u_long ahd_get_residual(struct scb *scb)
{
	return (scb->xs->resid);
}

static __inline
int ahd_perform_autosense(struct scb *scb)
{
	/* Return true for OpenBSD */
	return (1);
}

static __inline uint32_t
ahd_get_sense_bufsize(struct ahd_softc *ahd, struct scb *scb)
{
	return (sizeof(struct scsi_sense_data));
}

static __inline void
ahd_freeze_simq(struct ahd_softc *ahd)
{
        /* do nothing for now */
}

static __inline void
ahd_release_simq(struct ahd_softc *ahd)
{
        /* do nothing for now */
}

static __inline void
ahd_freeze_scb(struct scb *scb)
{
	struct scsi_xfer *xs = scb->xs;
	int target;

	target = xs->sc_link->target;
	if (!(scb->flags & SCB_FREEZE_QUEUE)) {
		scb->flags |= SCB_FREEZE_QUEUE;
	}
}

static __inline void
ahd_platform_freeze_devq(struct ahd_softc *ahd, struct scb *scb)
{
}

static __inline int
ahd_platform_abort_scbs(struct ahd_softc *ahd, int target,
			char channel, int lun, u_int tag,
			role_t role, uint32_t status)
{
	return (0);
}

static __inline void
ahd_platform_scb_free(struct ahd_softc *ahd, struct scb *scb)
{
	int s;

	ahd_lock(ahd, &s);

	if ((ahd->flags & AHD_RESOURCE_SHORTAGE) != 0 ||
	    (scb->flags & SCB_RECOVERY_SCB) != 0) {
		ahd->flags &= ~AHD_RESOURCE_SHORTAGE;
	}

	timeout_del(&scb->xs->stimeout);

	ahd_unlock(ahd, &s);
}

/********************************** PCI ***************************************/
#if AHD_PCI_CONFIG > 0
static __inline uint32_t ahd_pci_read_config(ahd_dev_softc_t, int, int);
static __inline void	ahd_pci_write_config(ahd_dev_softc_t, int,
		uint32_t, int);
static __inline int	ahd_get_pci_function(ahd_dev_softc_t);
static __inline int	ahd_get_pci_slot(ahd_dev_softc_t);
static __inline int	ahd_get_pci_bus(ahd_dev_softc_t);

int			ahd_pci_map_registers(struct ahd_softc *);
int			ahd_pci_map_int(struct ahd_softc *);

static __inline uint32_t
ahd_pci_read_config(ahd_dev_softc_t pci, int reg, int width)
{
	return (pci_conf_read(pci->pa_pc, pci->pa_tag, reg));
}

static __inline void
ahd_pci_write_config(ahd_dev_softc_t pci, int reg, uint32_t value, int width)
{
	pci_conf_write(pci->pa_pc, pci->pa_tag, reg, value);
}

static __inline int
ahd_get_pci_function(ahd_dev_softc_t pci)
{
	return (pci->pa_function);
}

static __inline int
ahd_get_pci_slot(ahd_dev_softc_t pci)
{
	return (pci->pa_device);
}


static __inline int
ahd_get_pci_bus(ahd_dev_softc_t pci)
{
	return (pci->pa_bus);
}
#endif

typedef enum
{
	AHD_POWER_STATE_D0,
	AHD_POWER_STATE_D1,
	AHD_POWER_STATE_D2,
	AHD_POWER_STATE_D3
} ahd_power_state;

void ahd_power_state_change(struct ahd_softc *, ahd_power_state);

/******************************** VL/EISA *************************************/
int aic7770_map_registers(struct ahd_softc *);
int aic7770_map_int(struct ahd_softc *, int);

/********************************* Debug **************************************/
static __inline void	ahd_print_path(struct ahd_softc *, struct scb *);
static __inline void	ahd_platform_dump_card_state(struct ahd_softc *ahd);

static __inline void
ahd_print_path(struct ahd_softc *ahd, struct scb *scb)
{
	sc_print_addr(scb->xs->sc_link);
}

static __inline void
ahd_platform_dump_card_state(struct ahd_softc *ahd)
{
	/* Nothing to do here for OpenBSD */
	printf("FEATURES = 0x%x, FLAGS = 0x%x, CHIP = 0x%x BUGS =0x%x\n",
		ahd->features, ahd->flags, ahd->chip, ahd->bugs);
}
/**************************** Transfer Settings *******************************/
void	  ahd_notify_xfer_settings_change(struct ahd_softc *,
					  struct ahd_devinfo *);
void	  ahd_platform_set_tags(struct ahd_softc *, struct ahd_devinfo *,
				ahd_queue_alg);

/************************* Initialization/Teardown ****************************/
int	  ahd_platform_alloc(struct ahd_softc *, void *);
void	  ahd_platform_free(struct ahd_softc *);
int	  ahd_attach(struct ahd_softc *);
int	  ahd_softc_comp(struct ahd_softc *lahd, struct ahd_softc *rahd);
int	  ahd_detach(struct device *, int);
#define	ahd_platform_init

/****************************** Interrupts ************************************/
int			ahd_platform_intr(void *);
static __inline void	ahd_platform_flushwork(struct ahd_softc *ahd);
static __inline void
ahd_platform_flushwork(struct ahd_softc *ahd)
{
}

/************************ Misc Function Declarations **************************/
void	  ahd_done(struct ahd_softc *, struct scb *);
void	  ahd_send_async(struct ahd_softc *, char /*channel*/,
			 u_int /*target*/, u_int /*lun*/, ac_code, void *arg);
/************************ SCSI task management **************************/
#define SIU_TASKMGMT_NONE               0x00
#define SIU_TASKMGMT_ABORT_TASK         0x01
#define SIU_TASKMGMT_ABORT_TASK_SET     0x02
#define SIU_TASKMGMT_CLEAR_TASK_SET     0x04
#define SIU_TASKMGMT_LUN_RESET          0x08
#define SIU_TASKMGMT_TARGET_RESET       0x20
#define SIU_TASKMGMT_CLEAR_ACA          0x40
#endif  /* _AIC79XX_OPENBSD_H_ */
