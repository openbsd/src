/*	$OpenBSD: aic79xx_openbsd.h,v 1.8 2004/11/18 01:33:28 krw Exp $	*/

/*
 * Copyright (c) 2004 Milos Urbanek, Kenneth R. Westerback & Marco Peereboom
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

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

/************************* Forward Declarations *******************************/
typedef struct pci_attach_args * ahd_dev_softc_t;

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
void ahd_timer_reset(ahd_timer_t *, u_int, ahd_callback_t *, void *);
void ahd_scb_timer_reset(struct scb *, u_int);

#define ahd_timer_init callout_init
#define ahd_timer_stop callout_stop

/*************************** Device Access ************************************/
#define ahd_inb(ahd, port)					\
	bus_space_read_1((ahd)->tags[(port) >> 8],		\
			 (ahd)->bshs[(port) >> 8], (port) & 0xFF)

#define ahd_outb(ahd, port, value)				\
	bus_space_write_1((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8], (port) & 0xFF, value)

#define ahd_inw_atomic(ahd, port)				\
	aic_le16toh(bus_space_read_2((ahd)->tags[(port) >> 8],	\
				     (ahd)->bshs[(port) >> 8], (port) & 0xFF))

#define ahd_outw_atomic(ahd, port, value)			\
	bus_space_write_2((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8],		\
			  (port & 0xFF), aic_htole16(value))

#define ahd_outsb(ahd, port, valp, count)			\
	bus_space_write_multi_1((ahd)->tags[(port) >> 8],	\
				(ahd)->bshs[(port) >> 8],	\
				(port & 0xFF), valp, count)

#define ahd_insb(ahd, port, valp, count)			\
	bus_space_read_multi_1((ahd)->tags[(port) >> 8],	\
			       (ahd)->bshs[(port) >> 8],	\
			       (port & 0xFF), valp, count)

void ahd_flush_device_writes(struct ahd_softc *);

/**************************** Locking Primitives ******************************/
/* Lock protecting internal data structures */
void ahd_lockinit(struct ahd_softc *);
void ahd_lock(struct ahd_softc *, int *flags);
void ahd_unlock(struct ahd_softc *, int *flags);

/* Lock held during command compeletion to the upper layer */
void ahd_done_lockinit(struct ahd_softc *);
void ahd_done_lock(struct ahd_softc *, int *flags);
void ahd_done_unlock(struct ahd_softc *, int *flags);

/* Lock held during ahd_list manipulation and ahd softc frees */
void ahd_list_lockinit(void);
void ahd_list_lock(int *flags);
void ahd_list_unlock(int *flags);

/****************************** OS Primitives *********************************/

/************************** Transaction Operations ****************************/
void ahd_set_transaction_status(struct scb *, uint32_t);
void ahd_set_scsi_status(struct scb *, uint32_t);
uint32_t ahd_get_transaction_status(struct scb *);
uint32_t ahd_get_scsi_status(struct scb *);
void ahd_set_transaction_tag(struct scb *, int, u_int);
u_long ahd_get_transfer_length(struct scb *);
int ahd_get_transfer_dir(struct scb *);
void ahd_set_residual(struct scb *, u_long);
void ahd_set_sense_residual(struct scb *, u_long);
u_long ahd_get_residual(struct scb *);
int ahd_perform_autosense(struct scb *);
uint32_t ahd_get_sense_bufsize(struct ahd_softc*, struct scb*);
void ahd_freeze_simq(struct ahd_softc *);
void ahd_release_simq(struct ahd_softc *);
void ahd_freeze_scb(struct scb *);
void ahd_platform_freeze_devq(struct ahd_softc *, struct scb *);
int  ahd_platform_abort_scbs(struct ahd_softc *, int,
		    char, int, u_int, role_t, uint32_t);
void ahd_platform_scb_free(struct ahd_softc *, struct scb *);

/********************************** PCI ***************************************/
/*#if AHD_PCI_CONFIG > 0*/
int	ahd_get_pci_function(ahd_dev_softc_t);
int	ahd_get_pci_slot(ahd_dev_softc_t);
int	ahd_get_pci_bus(ahd_dev_softc_t);

int			ahd_pci_map_registers(struct ahd_softc *);
int			ahd_pci_map_int(struct ahd_softc *);
/*#endif*/

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
void	ahd_print_path(struct ahd_softc *, struct scb *);
void	ahd_platform_dump_card_state(struct ahd_softc *ahd);

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
void	ahd_platform_flushwork(struct ahd_softc *ahd);
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
