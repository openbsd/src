/*	$OpenBSD: siopvar.h,v 1.11 2004/10/13 23:33:03 krw Exp $ */
/*	$NetBSD: siopvar.h,v 1.18 2002/04/23 20:41:15 bouyer Exp $ */

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
 *	This product includes software developed by Manuel Bouyer.
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

/* structure and definitions for the siop driver */

/* Number of tag */
#define SIOP_NTAG 16

/*
 * xfer description of the script: tables and reselect script
 * In struct siop_common_cmd siop_xfer will point to this.
 */
struct siop_xfer {
	struct siop_common_xfer siop_tables;
	/* u_int32_t resel[sizeof(load_dsa) / sizeof(load_dsa[0])]; */
	/* Add some entries to make size 384 bytes (256+128) */
	u_int32_t resel[36];
} __packed;

/*
 * This describes a command handled by the SCSI controller
 * These are chained in either a free list or an active list
 * We have one queue per target
 */

struct siop_cmd {
	TAILQ_ENTRY (siop_cmd) next;
	struct siop_common_cmd cmd_c;
	struct siop_cbd *siop_cbdp; /* pointer to our siop_cbd */
	int reselslot;
};
#define cmd_tables cmd_c.siop_tables

/* command block descriptors: an array of siop_cmd + an array of siop_xfer */
struct siop_cbd {
	TAILQ_ENTRY (siop_cbd) next;
	struct siop_cmd *cmds;
	struct siop_xfer *xfers;
	bus_dmamap_t xferdma; /* DMA map for this block of xfers */
};

/* per-tag struct */
struct siop_tag {
	struct siop_cmd *active; /* active command */
	u_int reseloff;
};

/* per lun struct */
struct siop_lun {
	struct siop_tag siop_tag[SIOP_NTAG]; /* tag array */
	int lun_flags;
#define SIOP_LUNF_FULL 0x01 /* queue full message */
	u_int reseloff;
};

/*
 * per target struct; siop_common_cmd->target and siop_common_softc->targets[]
 * will point to this
 */
struct siop_target {
	struct siop_common_target target_c;
	struct siop_lun *siop_lun[8]; /* per-lun state */
	u_int reseloff;
	struct siop_lunsw *lunsw;
};

struct siop_lunsw {
	TAILQ_ENTRY (siop_lunsw) next;
	u_int32_t lunsw_off; /* offset of this lun sw, from sc_scriptaddr*/
	u_int32_t lunsw_size; /* size of this lun sw */
};


TAILQ_HEAD(cmd_list, siop_cmd);
TAILQ_HEAD(cbd_list, siop_cbd);
TAILQ_HEAD(lunsw_list, siop_lunsw);


/* Driver internal state */
struct siop_softc {
	struct siop_common_softc sc_c;
	int sc_currschedslot;		/* current scheduler slot */
	struct cbd_list cmds;		/* list of command block descriptors */
	struct cmd_list free_list;	/* cmd descr free list */
	struct cmd_list urgent_list;	/* high priority cmd descr list */
	struct cmd_list ready_list;	/* cmd descr ready list */
	struct lunsw_list lunsw_list;	/* lunsw free list */
	u_int32_t script_free_lo;	/* free ram offset from sc_scriptaddr */
	u_int32_t script_free_hi;	/* free ram offset from sc_scriptaddr */
	int sc_ntargets;		/* number of known targets */
	u_int32_t sc_flags;
};

/* defs for sc_flags */
#define SCF_CHAN_NOSLOT	0x0001		/* channel out of scheduler slot */

void    siop_attach(struct siop_softc *);
int	siop_intr(void *);
void	siop_add_dev(struct siop_softc *, int, int);
void	siop_del_dev(struct siop_softc *, int, int);
