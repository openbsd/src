/*	$OpenBSD: siopvar_common.h,v 1.9 2001/11/05 17:25:58 art Exp $ */
/*	$NetBSD: siopvar_common.h,v 1.10 2001/01/26 21:58:56 bouyer Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer
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
 *
 */

/* common struct and routines used by siop and esiop */

#ifndef SIOP_DEFAULT_TARGET
#define SIOP_DEFAULT_TARGET 7
#endif

/* tables used by SCRIPT */
struct scr_table {
	u_int32_t count;
	u_int32_t addr;
} __attribute__((__packed__));

/* Number of scatter/gather entries */
#define SIOP_NSG	(MAXPHYS/NBPG + 1)	/* XXX NBPG */

/* Number of tags, also number of openings if tags are used */
#define SIOP_NTAG 16

/* Number of openings if tags are not used */
#define SIOP_OPENINGS 2

/*
 * This structure interfaces the SCRIPT with the driver; it describes a full
 * transfer. 
 */
struct siop_xfer_common {
	u_int8_t msg_out[16];	         /*  0 */
	u_int8_t msg_in[16];	         /* 16 */
	u_int32_t status;	         /* 32 */
	u_int32_t pad1;		         /* 36 */
	u_int32_t id;		         /* 40 */
	u_int32_t pad2;		         /* 44 */
	struct scr_table t_msgin;	 /* 48 */
	struct scr_table t_extmsgin;	 /* 56 */
	struct scr_table t_extmsgdata;   /* 64 */
	struct scr_table t_msgout;	 /* 72 */
	struct scr_table cmd;	         /* 80 */
	struct scr_table t_status;	 /* 88 */
	struct scr_table data[SIOP_NSG]; /* 96 */
} __attribute__((__packed__));

/* status can hold the SCSI_* status values, and 2 additional values: */
#define SCSI_SIOP_NOCHECK	0xfe	/* don't check the scsi status */
#define SCSI_SIOP_NOSTATUS	0xff	/* device didn't report status */

/* xfer description of the script: tables and reselect script */
struct siop_xfer {
	struct siop_xfer_common tables;
	/* u_int32_t resel[sizeof(load_dsa) / sizeof(load_dsa[0])]; */
	u_int32_t resel[25];
} __attribute__((__packed__));

/*
 * This describes a command handled by the SCSI controller.
 * These are chained in either a free list or a active list.
 * We have one queue per target.
 */
struct siop_cmd {
	TAILQ_ENTRY (siop_cmd) next;
	struct siop_softc *siop_sc; /* points back to our adapter */
	struct siop_target *siop_target; /* pointer to our target def */
	struct scsi_xfer *xs; /* xfer from the upper level */
	struct siop_xfer *siop_xfer; /* tables dealing with this xfer */
#define siop_tables siop_xfer->tables
	struct siop_cbd *siop_cbdp; /* pointer to our siop_cbd */
	bus_addr_t	dsa; /* DSA value to load */
	bus_dmamap_t	dmamap_cmd;
	bus_dmamap_t	dmamap_data;
	struct scsi_sense rs_cmd; /* request sense command buffer */
	int status;
	int flags;
	int reselslot; /* the reselect slot used */
	int tag;	/* tag used for tagged command queuing */
};

/* command block descriptors: an array of siop_cmd + an array of siop_xfer */

struct siop_cbd {
	TAILQ_ENTRY (siop_cbd) next;
	struct siop_cmd *cmds;
	struct siop_xfer *xfers;
	bus_dmamap_t xferdma; /* DMA map for this block of xfers */
};

/* status defs */
#define CMDST_FREE		0 /* cmd slot is free */
#define CMDST_READY		1 /* cmd slot is waiting for processing */
#define CMDST_ACTIVE		2 /* cmd slot is being processed */
#define CMDST_SENSE		3 /* cmd slot is requesting sense */
#define CMDST_SENSE_ACTIVE	4 /* request sense active */
#define CMDST_SENSE_DONE 	5 /* request sense done */
#define CMDST_DONE		6 /* cmd slot has been processed */
/* flags defs */
#define CMDFL_TIMEOUT	0x0001 /* cmd timed out */
#define CMDFL_TAG	0x0002 /* tagged cmd */

/* per-tag struct */
struct siop_tag {
	struct siop_cmd *active; /* active command */
	u_int reseloff; /* XXX */
};

/* per lun struct */
struct siop_lun {
	struct siop_tag siop_tag[SIOP_NTAG]; /* tag array */
	int lun_flags; /* per-lun flags, see below */
	u_int reseloff; /* XXX */
};

#define SIOP_LUNF_FULL 0x01 /* queue full message */

/* per-target struct */
struct siop_target {
	int status;	/* target status, see below */
	int flags;	/* target flags, see below */
	u_int32_t id;	/* for SELECT FROM
			 * 31-24 == SCNTL3
			 * 23-16 == SCSI id
			 * 15- 8 == SXFER
			 *  7- 0 == SCNTL4
			 */
	struct siop_lun *siop_lun[8]; /* per-lun state */
	u_int reseloff; /* XXX */
	struct siop_lunsw *lunsw; /* XXX */
};

/* target status */
#define TARST_PROBING	0 /* target is being probed */
#define TARST_ASYNC	1 /* target needs sync/wide negotiation */
#define TARST_WIDE_NEG	2 /* target is doing wide negotiation */
#define TARST_SYNC_NEG	3 /* target is doing sync negotiation */
#define TARST_PPR_NEG	4 /* target is doing PPR (Parallel Protocol Request) */
#define TARST_OK	5 /* sync/wide agreement is valid */

/* target flags */
#define TARF_SYNC	0x01 /* target can do sync xfers      */
#define TARF_WIDE	0x02 /* target can do wide xfers      */
#define TARF_TAG	0x04 /* target can do taggged queuing */
#define TARF_PPR	0x08 /* target can do PPR negotiation */

#define TARF_ISWIDE	0x10 /* target is using wide xfers    */
#define TARF_ISIUS	0x20 /* target is using IUS           */
#define TARF_ISDT	0x40 /* target is using DT            */
#define TARF_ISQAS	0x80 /* target is using QAS           */

struct siop_lunsw {
	TAILQ_ENTRY (siop_lunsw) next;
	u_int32_t lunsw_off; /* offset of this lun sw, from sc_scriptaddr*/
	u_int32_t lunsw_size; /* size of this lun sw */
};

static __inline__ void siop_table_sync __P((struct siop_cmd *, int));
static __inline__ void
siop_table_sync(siop_cmd, ops)
	struct siop_cmd *siop_cmd;
	int ops;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
        bus_addr_t offset;
        
        offset = siop_cmd->dsa -
		siop_cmd->siop_cbdp->xferdma->dm_segs[0].ds_addr;
	bus_dmamap_sync(sc->sc_dmat, siop_cmd->siop_cbdp->xferdma, offset,
            sizeof(struct siop_xfer), ops);
}

void	siop_common_reset __P((struct siop_softc *));
void	siop_setuptables __P((struct siop_cmd *));
int	siop_modechange __P((struct siop_softc *));

int	siop_wdtr_neg __P((struct siop_cmd *));
int	siop_sdtr_neg __P((struct siop_cmd *));
int     siop_ppr_neg  __P((struct siop_cmd *));
void	siop_sdtr_msg __P((struct siop_cmd *, int, int, int));
void	siop_wdtr_msg __P((struct siop_cmd *, int, int));
void    siop_ppr_msg  __P((struct siop_cmd *, int, int, int));

/* actions to take at return of siop_<xxx>_neg() */
#define SIOP_NEG_NOP	0x0
#define SIOP_NEG_MSGOUT	0x1
#define SIOP_NEG_ACK	0x2
#define SIOP_NEG_MSGREJ	0x3

void	siop_print_info __P((struct siop_softc *, int));
void	siop_minphys __P((struct buf *));
void 	siop_sdp __P((struct siop_cmd *));
void	siop_clearfifo __P((struct siop_softc *));
void	siop_resetbus __P((struct siop_softc *));

int     siop_period_factor_to_scf __P((struct siop_softc *, int, int));
int     siop_scf_to_period_factor __P((struct siop_softc *, int, int));

/* XXXX should be  callbacks */
void	siop_add_dev __P((struct siop_softc *, int, int));
void	siop_del_dev __P((struct siop_softc *, int, int));
