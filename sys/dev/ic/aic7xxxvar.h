/*
 * Interface to the generic driver for the aic7xxx based adaptec
 * SCSI controllers.  This is used to implement product specific
 * probe and attach routines.
 *
 * Copyright (c) 1994, 1995, 1996 Justin T. Gibbs.
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
 *	$Id: aic7xxxvar.h,v 1.4 1996/05/05 12:42:31 deraadt Exp $
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#if defined(__FreeBSD__)
#include "ahc.h"                /* for NAHC from config */
#endif

#ifndef STAILQ_ENTRY /* for NetBSD, from FreeBSD <sys/queue.h> */
/*
 * Singly-linked Tail queue definitions.
 */
#define STAILQ_HEAD(name, type)						\
struct name {								\
	struct type *stqh_first;/* first element */			\
	struct type **stqh_last;/* addr of last next element */		\
}

#define STAILQ_ENTRY(type)						\
struct {								\
	struct type *stqe_next;	/* next element */			\
}

/*
 * Singly-linked Tail queue functions.
 */
#define	STAILQ_INIT(head) {						\
	(head)->stqh_first = NULL;					\
	(head)->stqh_last = &(head)->stqh_first;			\
}

#define STAILQ_INSERT_HEAD(head, elm, field) {				\
	if (((elm)->field.stqe_next = (head)->stqh_first) == NULL)	\
		(head)->stqh_last = &(elm)->field.stqe_next;		\
	(head)->stqh_first = (elm);					\
}

#define STAILQ_INSERT_TAIL(head, elm, field) {				\
	(elm)->field.stqe_next = NULL;					\
	*(head)->stqh_last = (elm);					\
	(head)->stqh_last = &(elm)->field.stqe_next;			\
}

#define STAILQ_INSERT_AFTER(head, tqelm, elm, field) {			\
	if (((elm)->field.stqe_next = (tqelm)->field.stqe_next) == NULL)\
		(head)->stqh_last = &(elm)->field.stqe_next;		\
	(tqelm)->field.stqe_next = (elm);				\
}

#define STAILQ_REMOVE_HEAD(head, field) {				\
	if (((head)->stqh_first =					\
	     (head)->stqh_first->field.stqe_next) == NULL)		\
		(head)->stqh_last = &(head)->stqh_first;		\
}

#define STAILQ_REMOVE(head, elm, type, field) {				\
	if ((head)->stqh_first == (elm)) {				\
		STAILQ_REMOVE_HEAD(head, field);			\
	}								\
	else {								\
		struct type *curelm = (head)->stqh_first;		\
		while( curelm->field.stqe_next != (elm) )		\
			curelm = curelm->field.stqe_next;		\
		if((curelm->field.stqe_next =				\
		    curelm->field.stqe_next->field.stqe_next) == NULL)	\
			(head)->stqh_last = &(curelm)->field.stqe_next;	\
	}								\
}

#endif /* STAILQ_ENTRY */

#ifndef NetBSD1_1
#define	NetBSD1_1	0
#endif

#if defined(__FreeBSD__) || NetBSD1_1 < 3
#define	AHC_INB(ahc, port)	\
	inb((ahc)->baseport+(port))
#define	AHC_INSB(ahc, port, valp, size)	\
	insb((ahc)->baseport+(port), valp, size)
#define	AHC_OUTB(ahc, port, val)	\
	outb((ahc)->baseport+(port), val)
#define	AHC_OUTSB(ahc, port, valp, size)	\
	outsb((ahc)->baseport+(port), valp, size)
#define	AHC_OUTSL(ahc, port, valp, size)	\
	outsl((ahc)->baseport+(port), valp, size)
#elif defined(__NetBSD__)
#define	AHC_INB(ahc, port)	\
	bus_io_read_1((ahc)->sc_bc, ahc->baseport, port)
#define	AHC_INSB(ahc, port, valp, size)	\
	bus_io_read_multi_1((ahc)->sc_bc, ahc->baseport, port, valp, size)
#define	AHC_OUTB(ahc, port, val)	\
	bus_io_write_1((ahc)->sc_bc, ahc->baseport, port, val)
#define	AHC_OUTSB(ahc, port, valp, size)	\
	bus_io_write_multi_1((ahc)->sc_bc, ahc->baseport, port, valp, size)
#define	AHC_OUTSL(ahc, port, valp, size)	\
	bus_io_write_multi_4((ahc)->sc_bc, ahc->baseport, port, valp, size)
#endif

#define	AHC_NSEG	256	/* number of dma segments supported */

#define AHC_SCB_MAX	255	/*
				 * Up to 255 SCBs on some types of aic7xxx
				 * based boards.  The aic7870 have 16 internal
				 * SCBs, but external SRAM bumps this to 255.
				 * The aic7770 family have only 4, and the 
				 * aic7850 has only 3.
				 */


typedef unsigned long int physaddr;
extern u_long ahc_unit;

struct ahc_dma_seg {
        physaddr addr;
            long len;
};

typedef enum {
	AHC_NONE	= 0x000,
	AHC_ULTRA	= 0x001,	/* Supports 20MHz Transfers */
	AHC_WIDE  	= 0x002,	/* Wide Channel */
	AHC_TWIN	= 0x008,	/* Twin Channel */
	AHC_AIC7770	= 0x010,
	AHC_AIC7850	= 0x020,
	AHC_AIC7860	= 0x021,	/* ULTRA version of the aic7850 */
	AHC_AIC7870	= 0x040,
	AHC_AIC7880	= 0x041,
	AHC_AIC78X0	= 0x060,	/* PCI Based Controller */
	AHC_274		= 0x110,	/* EISA Based Controller */
	AHC_284		= 0x210,	/* VL/ISA Based Controller */
	AHC_294		= 0x440,	/* PCI Based Controller */
	AHC_294U	= 0x441,	/* ULTRA PCI Based Controller */
	AHC_394		= 0x840,	/* Twin Channel PCI Controller */
	AHC_394U	= 0x841,	/* Twin, ULTRA Channel PCI Controller */
}ahc_type;

typedef enum {
	AHC_FNONE	= 0x00,
	AHC_INIT	= 0x01,
	AHC_RUNNING	= 0x02,
	AHC_PAGESCBS	= 0x04,		/* Enable SCB paging */
	AHC_USEDEFAULTS = 0x10,		/*
					 * For cards without an seeprom
					 * or a BIOS to initialize the chip's
					 * SRAM, we use the default chip and
					 * target settings.
					 */
	AHC_CHNLB	= 0x20,		/* 
					 * Second controller on 3940 
					 * Also encodes the offset in the
					 * SEEPROM for CHNLB info (32)
					 */
}ahc_flag;

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  Only the
 * first 26 bytes of the structure need to be transfered to the card during
 * normal operation.  The fields starting at byte 32 are used for kernel level
 * bookkeeping.  
 */
struct scb {
/* ------------    Begin hardware supported fields    ---------------- */
/*0*/   u_char control;
/*1*/	u_char tcl;		/* 4/1/3 bits */
/*2*/	u_char status;
/*3*/	u_char SG_segment_count;
/*4*/	physaddr SG_list_pointer;
/*8*/	u_char residual_SG_segment_count;
/*9*/	u_char residual_data_count[3];
/*12*/	physaddr data;
/*16*/  u_long datalen;			/* Really only three bits, but its
					 * faster to treat it as a long on
					 * a quad boundary.
					 */
/*20*/	physaddr cmdpointer;
/*24*/	u_char cmdlen;
/*25*/	u_char tag;			/* Index into our kernel SCB array.
					 * Also used as the tag for tagged I/O
					 */
#define SCB_PIO_TRANSFER_SIZE	26 	/* amount we need to upload/download
					 * via PIO to initialize a transaction.
					 */
/*26*/	u_char next;			/* Used for threading SCBs in the
					 * "Waiting for Selection" and
					 * "Disconnected SCB" lists down
					 * in the sequencer.
					 */
/*27*/	u_char prev;
/*-----------------end of hardware supported fields----------------*/
	STAILQ_ENTRY(scb)	links;	/* for chaining */
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int	flags;
#define	SCB_FREE		0x000
#define	SCB_ACTIVE		0x001
#define	SCB_ABORTED		0x002
#define	SCB_DEVICE_RESET	0x004
#define	SCB_IMMED		0x008
#define	SCB_SENSE		0x010
#define	SCB_TIMEDOUT		0x020
#define	SCB_QUEUED_FOR_DONE	0x040
#define	SCB_PAGED_OUT		0x080
#define	SCB_WAITINGQ		0x100
#define	SCB_ASSIGNEDQ		0x200
#define	SCB_SENTORDEREDTAG	0x400
	u_char	position;	/* Position in card's scbarray */
	struct ahc_dma_seg ahc_dma[AHC_NSEG] __attribute__ ((packed));
	struct scsi_sense sense_cmd;	/* SCSI command block */
};

#if defined(__NetBSD__)
#if NetBSD1_1 < 3 /* NetBSD-1.1 */
typedef int bus_chipset_tag_t;
typedef int bus_io_handle_t;
#endif
#endif

struct ahc_data {
#if defined(__NetBSD__)
	struct device sc_dev;
	void	*sc_ih;
	bus_chipset_tag_t sc_bc;
#endif
	int	unit;
	ahc_type type;
	ahc_flag flags;
	u_long	baseport;
	struct	scb *scbarray[AHC_SCB_MAX]; /* Mirror boards scbarray */
	struct	scb *pagedout_ntscbs[16];/* 
					  * Paged out, non-tagged scbs
					  * indexed by target.
					  */
	STAILQ_HEAD(, scb) free_scbs;	/*
					 * SCBs assigned to free slots
					 * on the card. (no paging required)
					 */
	STAILQ_HEAD(, scb) page_scbs;	/*
					 * SCBs that will require paging
					 * before use (no assigned slot)
					 */
	STAILQ_HEAD(, scb) waiting_scbs;/*
					 * SCBs waiting to be paged in
					 * and started.
					 */
	STAILQ_HEAD(, scb)assigned_scbs;/*
					 * SCBs that were waiting but have
					 * now been assigned a slot by
					 * ahc_free_scb.
					 */
	struct	scsi_link sc_link;
	struct	scsi_link sc_link_b;	/* Second bus for Twin channel cards */
	u_short	needsdtr_orig;		/* Targets we initiate sync neg with */
	u_short	needwdtr_orig;		/* Targets we initiate wide neg with */
	u_short	needsdtr;		/* Current list of negotiated targets */
	u_short needwdtr;		/* Current list of negotiated targets */
	u_short sdtrpending;		/* Pending SDTR to these targets */
	u_short wdtrpending;		/* Pending WDTR to these targets */
	u_short	tagenable;		/* Targets that can handle tagqueing */
	u_short	orderedtag;		/* Targets to use ordered tag on */
	u_short	discenable;		/* Targets allowed to disconnect */
	u_char	our_id;			/* our scsi id */
	u_char	our_id_b;		/* B channel scsi id */
	u_char	numscbs;
	u_char	activescbs;
	u_char  maxhscbs;		/* Number of SCBs on the card */
	u_char	maxscbs;		/*
					 * Max SCBs we allocate total including
					 * any that will force us to page SCBs
					 */
	u_char	qcntmask;
	u_char	unpause;
	u_char	pause;
	u_char	in_timeout;
};

/* #define AHC_DEBUG */
#ifdef AHC_DEBUG
/* Different debugging levels used when AHC_DEBUG is defined */
#define AHC_SHOWMISC	0x0001
#define AHC_SHOWCMDS	0x0002
#define AHC_SHOWSCBS	0x0004
#define AHC_SHOWABORTS	0x0008
#define AHC_SHOWSENSE	0x0010
#define AHC_SHOWSCBCNT	0x0020

extern int ahc_debug; /* Initialized in i386/scsi/aic7xxx.c */
#endif

#if defined(__FreeBSD__)
void ahc_reset __P((u_long iobase));
struct ahc_data *ahc_alloc __P((int unit, u_long io_base, ahc_type type, ahc_flag flags));
#elif defined(__NetBSD__)
void ahc_reset __P((char *devname, bus_chipset_tag_t bc, bus_io_handle_t ioh));
void ahc_construct __P((struct ahc_data *ahc, int unit, bus_chipset_tag_t bc, bus_io_handle_t ioh, ahc_type type, ahc_flag flags));
#endif
void ahc_free __P((struct ahc_data *));
int ahc_init __P((struct ahc_data *));
int ahc_attach __P((struct ahc_data *));
#if defined(__FreeBSD__)
void ahc_intr __P((void *arg));
#elif defined(__NetBSD__)
int ahc_intr __P((void *arg));
#endif

#endif  /* _AIC7XXX_H_ */
