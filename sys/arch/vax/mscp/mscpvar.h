/*	$OpenBSD: mscpvar.h,v 1.6 2003/06/02 23:27:57 millert Exp $	*/
/*	$NetBSD: mscpvar.h,v 1.7 1999/06/06 19:16:18 ragge Exp $	*/
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mscpvar.h	7.3 (Berkeley) 6/28/90
 */

/*
 * MSCP generic driver configuration
 */

/*
 * Enabling MSCP_PARANOIA makes the response code perform various checks
 * on the hardware.  (Right now it verifies only the buffer pointer in
 * mscp_cmdref.)
 *
 * Enabling AVOID_EMULEX_BUG selects an alternative method of identifying
 * transfers in progress, which gets around a rather peculiar bug in the
 * SC41/MS.  Enabling MSCP_PARANOIA instead should work, but will cause
 * `extra' Unibus resets.
 *
 * Either of these flags can simply be included as an `options' line in
 * your configuration file.
 */

/* #define MSCP_PARANOIA */
/* #define AVOID_EMULEX_BUG */

/*
 * Ring information, per ring (one each for commands and responses).
 */
struct mscp_ri {
	int	mri_size;		/* ring size */
	int	mri_next;		/* next (expected|free) */
	long	*mri_desc;		/* base address of descriptors */
	struct	mscp *mri_ring;		/* base address of packets */
};

/*
 * Transfer info, one per command packet.
 */
struct mscp_xi {
	bus_dmamap_t	mxi_dmam;	/* Allocated DMA map for this entry */
	struct buf *	mxi_bp;		/* Buffer used in this command */
	struct mscp *	mxi_mp;		/* Packet used in this command */
	int		mxi_inuse;
};

struct	mscp_ctlr {
					/* controller operation complete */
	void	(*mc_ctlrdone)(struct device *);
					/* device-specific start routine */
	void	(*mc_go)(struct device *, struct mscp_xi *);
					/* ctlr error handling */
	void	(*mc_saerror)(struct device *, int);
};

struct mscp_softc;

struct	mscp_device {
				/* error datagram */
	void	(*me_dgram)(struct device *, struct mscp *, struct mscp_softc *);
				/* normal I/O is done */
	void	(*me_iodone)(struct device *, struct buf *);
				/* drive on line */
	int	(*me_online)(struct device *, struct mscp *);
				/* got unit status */
	int	(*me_gotstatus)(struct device *, struct mscp *);
				/* replace done */
	void	(*me_replace)(struct device *, struct mscp *);
				/* read or write failed */
	int	(*me_ioerr)(struct device *, struct mscp *, struct buf *);
				/* B_BAD io done */
	void	(*me_bb)(struct device *, struct mscp *, struct buf *);
				/* Fill in mscp info for this drive */
	void	(*me_fillin)(struct buf *,struct mscp *);
				/* Non-data transfer operation is done */
	void	(*me_cmddone)(struct device *, struct mscp *);
};

/*
 * This struct is used when attaching a mscpbus.
 */
struct	mscp_attach_args {
	struct	mscp_ctlr *ma_mc;	/* Pointer to ctlr's mscp_ctlr */
	int	ma_type;		/* disk/tape bus type */
	struct	mscp_pack *ma_uda;	/* comm area virtual */
	struct	mscp_softc **ma_softc;	/* backpointer to bus softc */
	bus_dmamap_t	   ma_dmam;	/* This comm area dma info */
	bus_dma_tag_t	   ma_dmat;
	bus_space_tag_t	   ma_iot;
	bus_space_handle_t ma_iph;	/* initialisation and polling */
	bus_space_handle_t ma_sah;	/* status & address (read part) */
	bus_space_handle_t ma_swh;	/* status & address (write part) */
	short	ma_ivec;		/* Interrupt vector to use */
	char	ma_ctlrnr;		/* Phys ctlr nr */
	char	ma_adapnr;		/* Phys adapter nr */
};
#define MSCPBUS_DISK	001	/* Bus is used for disk mounts */
#define MSCPBUS_TAPE	002	/* Bus is used for tape mounts */
#define MSCPBUS_UDA	004	/* ctlr is disk on unibus/qbus */
#define MSCPBUS_KDB	010	/* ctlr is disk on BI */
#define MSCPBUS_KLE	020	/* ctlr is tape on unibus/qbus */

/*
 * Used when going for child devices.
 */
struct	drive_attach_args {
	struct	mscp *da_mp;	/* this devices response struct */
	int	da_typ;		/* Parent of type */
};

/*
 * Return values from functions.
 * MSCP_RESTARTED is peculiar to I/O errors.
 */
#define MSCP_DONE	0		/* all ok */
#define MSCP_FAILED	1		/* no go */
#define MSCP_RESTARTED	2		/* transfer restarted */

/*
 * Per device information.
 *
 * mi_ip is a pointer to the inverting pointers (things that get `ui's
 * given unit numbers) FOR THIS CONTROLLER (NOT the whole set!).
 *
 * b_actf holds a queue of those transfers that were started but have
 * not yet finished.  Other Unibus drivers do not need this as they hand
 * out requests one at a time.	MSCP devices, however, take a slew of
 * requests and pick their own order to execute them.  This means that
 * we have to have a place to move transfers that were given to the
 * controller, so we can tell those apart from those that have not yet
 * been handed out; b_actf is that place.
 */
struct mscp_softc {
	struct	device mi_dev;		/* Autoconf stuff */
	struct	mscp_ri mi_cmd;		/* MSCP command ring info */
	struct	mscp_ri mi_rsp;		/* MSCP response ring info */
	bus_dma_tag_t	mi_dmat;
	bus_dmamap_t	mi_dmam;
	struct	mscp_xi mi_xi[NCMD];
	int	mi_mxiuse;		/* Bitfield of inuse mxi packets */
	short	mi_credits;		/* transfer credits */
	char	mi_wantcmd;		/* waiting for command packet */
	char	mi_wantcredits;		/* waiting for transfer credits */
	struct	mscp_ctlr *mi_mc;	/* Pointer to parent's mscp_ctlr */
	struct	mscp_device *mi_me;	/* Pointer to child's mscp_device */
	struct	device **mi_dp;		/* array of backpointers */
	int	mi_driveno;		/* Max physical drive number found */
	char	mi_ctlrnr;		/* Phys ctlr nr */
	char	mi_adapnr;		/* Phys adapter nr */
	int	mi_flags;
	struct	mscp_pack *mi_uda;	/* virtual address */
	int	mi_type;
	short	mi_ivec;		/* Interrupt vector to use */
	short	mi_ierr;		/* Init err counter */
	bus_space_tag_t	   mi_iot;
	bus_space_handle_t mi_iph;	/* initialisation and polling */
	bus_space_handle_t mi_sah;	/* status & address (read part) */
	bus_space_handle_t mi_swh;	/* status & address (write part) */
	SIMPLEQ_HEAD(, buf) mi_resq;	/* While waiting for packets */
};

/* mi_flags */
#define MSC_STARTPOLL	1
#define MSC_INSTART	2
#define MSC_IGNOREINTR	4
#define MSC_READY	8

/*
 * We have run out of credits when mi_credits is <= MSCP_MINCREDITS.
 * It is still possible to issue one command in this case, but it must
 * not be a data transfer.  E.g., `get command status' or `abort command'
 * is legal, while `read' is not.
 */
#define MSCP_MINCREDITS 1

/*
 * Flags for mscp_getcp().
 */
#define MSCP_WAIT	1
#define MSCP_DONTWAIT	0

	/* get a command packet */

/*
 * Unit flags
 */
#define UNIT_ONLINE	0x01	/* drive is on line */
#define UNIT_HAVESTATUS 0x02	/* got unit status */
#define UNIT_REQUEUE	0x04	/* requeue after response */

/*
 * Handle a command ring transition: wake up sleepers for command packets.
 * This is too simple to bother with a function call.
 */
#define MSCP_DOCMD(mi) { \
	if ((mi)->mi_wantcmd) { \
		(mi)->mi_wantcmd = 0; \
		wakeup((caddr_t) &(mi)->mi_wantcmd); \
	} \
}

/* Prototypes */
struct	mscp *mscp_getcp(struct mscp_softc *, int);
void	mscp_printevent(struct mscp *);
void	mscp_go(struct mscp_softc *, struct mscp *, int);
void	mscp_requeue(struct mscp_softc *);
void	mscp_dorsp(struct mscp_softc *);
int	mscp_decodeerror(char *, struct mscp *, struct mscp_softc *);
int	mscp_print(void *, const char *);
void	mscp_hexdump(struct mscp *);
void	mscp_strategy(struct buf *, struct device *);
void	mscp_printtype(int, int);
int	mscp_waitstep(struct mscp_softc *, int, int);
void	mscp_dgo(struct mscp_softc *, struct mscp_xi *);
void	mscp_intr(struct mscp_softc *);
