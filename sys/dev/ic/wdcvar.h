/*      $OpenBSD: wdcvar.h,v 1.7 1999/12/14 18:07:43 csapuntz Exp $     */
/*	$NetBSD: wdcvar.h,v 1.17 1999/04/11 20:50:29 bouyer Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	WAITTIME    (10 * hz)    /* time to wait for a completion */
	/* this is a lot for hard drives, but not for cdroms */

struct channel_queue {  /* per channel queue (may be shared) */
	TAILQ_HEAD(xferhead, wdc_xfer) sc_xfer;
};

struct channel_softc_vtbl;

struct channel_softc { /* Per channel data */
	struct channel_softc_vtbl  *_vtbl;

	/* Our location */
	int channel;
	/* Our controller's softc */
	struct wdc_softc *wdc;
	/* Our registers */
	bus_space_tag_t       cmd_iot;
	bus_space_handle_t    cmd_ioh;
	bus_space_tag_t       ctl_iot;
	bus_space_handle_t    ctl_ioh;
	/* data32{iot,ioh} are only used for 32 bit xfers */
	bus_space_tag_t         data32iot;
	bus_space_handle_t      data32ioh;
	/* Our state */
	int ch_flags;
#define WDCF_ACTIVE   0x01	/* channel is active */
#define WDCF_IRQ_WAIT 0x10	/* controller is waiting for irq */
#define WDCF_ONESLAVE 0x20      /* slave-only channel */
	u_int8_t ch_status;         /* copy of status register */
	u_int8_t ch_error;          /* copy of error register */
	/* per-drive infos */
	struct ata_drive_datas ch_drive[2];
	
	/*
	 * channel queues. May be the same for all channels, if hw channels
	 * are not independants
	 */
	struct channel_queue *ch_queue;
};

/*
 * Disk Controller register definitions.
 */
#define _WDC_REGMASK 7
#define _WDC_AUX 8
#define _WDC_RDONLY  16
#define _WDC_WRONLY  32
enum wdc_regs { 		
	wdr_error = _WDC_RDONLY | 1,
	wdr_precomp = _WDC_WRONLY | 1,
	wdr_features = _WDC_WRONLY | 1,
	wdr_seccnt = 2,
	wdr_ireason = 2,
	wdr_sector = 3,
	wdr_cyl_lo = 4,
	wdr_cyl_hi = 5,
	wdr_sdh = 6,
	wdr_status = _WDC_RDONLY | 7,
	wdr_command = _WDC_WRONLY | 7,
	wdr_altsts = _WDC_RDONLY | _WDC_AUX,
	wdr_ctlr = _WDC_WRONLY | _WDC_AUX
};

struct channel_softc_vtbl {
	u_int8_t (*read_reg)(struct channel_softc *, enum wdc_regs reg);
	void (*write_reg)(struct channel_softc *, enum wdc_regs reg, 
	    u_int8_t var);
	
	void (*read_raw_multi_2)(struct channel_softc *, 
	    void *data, unsigned int nbytes);
	void (*write_raw_multi_2)(struct channel_softc *,
	    void *data, unsigned int nbytes);

	void (*read_raw_multi_4)(struct channel_softc *,
	    void *data, unsigned int nbytes);
	void (*write_raw_multi_4)(struct channel_softc *,
	    void *data, unsigned int nbytes);
};


#define CHP_READ_REG(chp, a)  ((chp)->_vtbl->read_reg)(chp, a)
#define CHP_WRITE_REG(chp, a, b)  ((chp)->_vtbl->write_reg)(chp, a, b)
#define CHP_READ_RAW_MULTI_2(chp, a, b)  \
        ((chp)->_vtbl->read_raw_multi_2)(chp, a, b)
#define CHP_WRITE_RAW_MULTI_2(chp, a, b)  \
        ((chp)->_vtbl->write_raw_multi_2)(chp, a, b)
#define CHP_READ_RAW_MULTI_4(chp, a, b)  \
	((chp)->_vtbl->read_raw_multi_4)(chp, a, b)
#define CHP_WRITE_RAW_MULTI_4(chp, a, b)  \
	((chp)->_vtbl->write_raw_multi_4)(chp, a, b)

struct wdc_softc { /* Per controller state */
	struct device sc_dev;
	/* mandatory fields */
	int           cap;
/* Capabilities supported by the controller */
#define	WDC_CAPABILITY_DATA16 0x0001    /* can do  16-bit data access */
#define	WDC_CAPABILITY_DATA32 0x0002    /* can do 32-bit data access */
#define WDC_CAPABILITY_MODE   0x0004	/* controller knows its PIO/DMA modes */
#define	WDC_CAPABILITY_DMA    0x0008	/* DMA */
#define	WDC_CAPABILITY_UDMA   0x0010	/* Ultra-DMA/33 */
#define	WDC_CAPABILITY_HWLOCK 0x0020	/* Needs to lock HW */
#define	WDC_CAPABILITY_ATA_NOSTREAM 0x0040 /* Don't use stream funcs on ATA */
#define	WDC_CAPABILITY_ATAPI_NOSTREAM 0x0080 /* Don't use stream f on ATAPI */
#define WDC_CAPABILITY_NO_EXTRA_RESETS 0x0100 /* only reset once */
#define WDC_CAPABILITY_PREATA 0x0200 /* ctrl can be a pre-ata one */
	u_int8_t      PIO_cap; /* highest PIO mode supported */
	u_int8_t      DMA_cap; /* highest DMA mode supported */
	u_int8_t      UDMA_cap; /* highest UDMA mode supported */
	int nchannels;	/* Number of channels on this controller */
	struct channel_softc **channels;  /* channels-specific datas (array) */

#if 0
	/*
	 * The reference count here is used for both IDE and ATAPI devices.
	 */
	struct scsipi_adapter sc_atapi_adapter;
#endif

	/* if WDC_CAPABILITY_DMA set in 'cap' */
	void            *dma_arg;
	int            (*dma_init) __P((void *, int, int, void *, size_t,
	                int));
	void           (*dma_start) __P((void *, int, int, int));
	int            (*dma_finish) __P((void *, int, int, int));
/* flags passed to DMA functions */
#define WDC_DMA_READ 0x01
#define WDC_DMA_POLL 0x02

	/* if WDC_CAPABILITY_HWLOCK set in 'cap' */
	int            (*claim_hw) __P((void *, int));
	void            (*free_hw) __P((void *));

	/* if WDC_CAPABILITY_MODE set in 'cap' */
	void 		(*set_modes) __P((struct channel_softc *));
};

 /*
  * Description of a command to be handled by a controller.
  * These commands are queued in a list.
  */
struct wdc_xfer {
	volatile u_int c_flags;    
#define C_INUSE  	0x0001 /* xfer struct is in use */
#define C_ATAPI  	0x0002 /* xfer is ATAPI request */
#define C_TIMEOU  	0x0004 /* xfer processing timed out */
#define C_NEEDDONE  	0x0010 /* need to call upper-level done */
#define C_POLL		0x0020 /* cmd is polled */
#define C_DMA		0x0040 /* cmd uses DMA */
#define C_SENSE		0x0080 /* cmd is a internal command */
#define C_MEDIA_ACCESS  0x0100 /* is a media access command */
#define C_POLL_MACHINE  0x0200 /* machine has a poll hander */

	/* Informations about our location */
	struct channel_softc *chp;
	u_int8_t drive;

	/* Information about the current transfer  */
	void *cmd; /* wdc, ata or scsipi command structure */
	void *databuf;
	int c_bcount;      /* byte count left */
	int c_skip;        /* bytes already transferred */
	TAILQ_ENTRY(wdc_xfer) c_xferchain;
	LIST_ENTRY(wdc_xfer) free_list;
	void (*c_start) __P((struct channel_softc *, struct wdc_xfer *));
	int  (*c_intr)  __P((struct channel_softc *, struct wdc_xfer *, int));
	int (*c_done)  __P((struct channel_softc *, struct wdc_xfer *, int));

	/* Used by ATAPISCSI */
	int timeout;
	int endticks;
	int delay;
	unsigned int expect_irq:1;
	unsigned int claim_irq:1;

	int (*next) __P((struct channel_softc *, struct wdc_xfer *, int));
	
	/* Used for tape devices */
	int  transfer_len;
};

/*
 * Public functions which can be called by ATA or ATAPI specific parts,
 * or bus-specific backends.
 */

int   wdcprobe __P((struct channel_softc *));
void  wdcattach __P((struct channel_softc *));
int   wdcintr __P((void *));
void  wdc_exec_xfer __P((struct channel_softc *, struct wdc_xfer *));
struct wdc_xfer *wdc_get_xfer __P((int)); /* int = WDC_NOSLEEP/CANSLEEP */
#define WDC_CANSLEEP 0x00
#define WDC_NOSLEEP 0x01
void   wdc_free_xfer  __P((struct channel_softc *, struct wdc_xfer *));
void  wdcstart __P((struct channel_softc *));
void  wdcrestart __P((void*));
int   wdcreset	__P((struct channel_softc *, int));
#define VERBOSE 1 
#define SILENT 0 /* wdcreset will not print errors */
int   wdcwait __P((struct channel_softc *, int, int, int));
void  wdcbit_bucket __P(( struct channel_softc *, int));
void  wdccommand __P((struct channel_softc *, u_int8_t, u_int8_t, u_int16_t,
	                  u_int8_t, u_int8_t, u_int8_t, u_int8_t));
void   wdccommandshort __P((struct channel_softc *, int, int));
void  wdctimeout	__P((void *arg));

int	wdc_addref __P((struct channel_softc *));
void	wdc_delref __P((struct channel_softc *));

/*	
 * ST506 spec says that if READY or SEEKCMPLT go off, then the read or write
 * command is aborted.
 */   
#define wait_for_drq(chp, timeout) wdcwait((chp), WDCS_DRQ, WDCS_DRQ, (timeout))
#define wait_for_unbusy(chp, timeout)	wdcwait((chp), 0, 0, (timeout))
#define wait_for_ready(chp, timeout) wdcwait((chp), WDCS_DRDY, \
	WDCS_DRDY, (timeout))
/* ATA/ATAPI specs says a device can take 31s to reset */
#define WDC_RESET_WAIT 31000

void wdc_atapibus_attach __P((struct channel_softc *));
int   atapi_print       __P((void *, const char *));

void wdc_disable_intr __P((struct channel_softc *));
void wdc_enable_intr __P((struct channel_softc *));
int wdc_select_drive __P((struct channel_softc *, int, int));

void wdc_output_bytes __P((struct ata_drive_datas *drvp, void *, unsigned int));
void wdc_input_bytes __P((struct ata_drive_datas *drvp, void *, unsigned int));

