/*	$OpenBSD: siopvar.h,v 1.4 2001/04/15 06:01:29 krw Exp $ */
/*	$NetBSD: siopvar.h,v 1.13 2000/10/23 23:18:11 bouyer Exp $	*/

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

/* structure and definitions for the siop driver */

TAILQ_HEAD(cmd_list, siop_cmd);
TAILQ_HEAD(cbd_list, siop_cbd);
TAILQ_HEAD(lunsw_list, siop_lunsw);

/* Driver internal state */
struct siop_softc {
	struct device sc_dev;
	struct scsi_link sc_link;	/* link to upper level */
	int features;			/* chip's features */
	int ram_size;
	int maxburst;
	int maxoff;
	int clock_div;			/* async. clock divider (scntl3) */
	int min_dt_sync;		/* minimum acceptable double transition sync */
	int min_st_sync;		/* minimum acceptable single transition sync */
	int scf_index;			/* clock id == index into period_factor[].scf */
	bus_space_tag_t sc_rt;		/* bus_space registers tag */
	bus_space_handle_t sc_rh;	/* bus_space registers handle */
	bus_addr_t sc_raddr;		/* register adresses */
	bus_space_tag_t sc_ramt;	/* bus_space ram tag */
	bus_space_handle_t sc_ramh;	/* bus_space ram handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	void (*sc_reset) __P((struct siop_softc*)); /* reset callback */
	bus_dmamap_t  sc_scriptdma;	/* DMA map for script */
	bus_addr_t sc_scriptaddr;	/* on-board ram or physical adress */
	u_int32_t *sc_script;		/* script location in memory */
	int sc_currschedslot;		/* current scheduler slot */
	struct cbd_list cmds;		/* list of command block descriptors */
	struct cmd_list free_list;	/* cmd descr free list */
	struct cmd_list urgent_list;	/* high priority cmd descr list */
	struct cmd_list ready_list;	/* cmd descr ready list */
	struct lunsw_list lunsw_list;	/* lunsw free list */
	u_int32_t script_free_lo;	/* free ram offset from sc_scriptaddr */
	u_int32_t script_free_hi;	/* free ram offset from sc_scriptaddr */
	struct siop_target *targets[16]; /* per-target states */
	int sc_ntargets;		/* number of known targets */
	u_int32_t sc_flags;
};
/* defs for sc_flags */
/* none for now */

/* features */
#define SF_BUS_WIDE	0x00000001 /* wide bus */
#define SF_BUS_ULTRA	0x00000002 /* Ultra (20Mhz) bus */
#define SF_BUS_ULTRA2	0x00000004 /* Ultra2 (40Mhz) bus */
#define SF_BUS_DIFF	0x00000008 /* differential bus */

#define SF_CHIP_LED0	0x00000100 /* led on GPIO0 */
#define SF_CHIP_DBLR	0x00000200 /* clock doubler */
#define SF_CHIP_QUAD	0x00000400 /* clock quadrupler */
#define SF_CHIP_FIFO	0x00000800 /* large fifo */
#define SF_CHIP_PF	0x00001000 /* Intructions prefetch */
#define SF_CHIP_RAM	0x00002000 /* on-board RAM */
#define SF_CHIP_LS	0x00004000 /* load/store instruction */
#define SF_CHIP_10REGS	0x00008000 /* 10 scratch registers */
#define SF_CHIP_C10	0x00010000 /* 1010 or variant */

#define SF_PCI_RL	0x01000000 /* PCI read line */
#define SF_PCI_RM	0x02000000 /* PCI read multiple */
#define SF_PCI_BOF	0x04000000 /* PCI burst opcode fetch */
#define SF_PCI_CLS	0x08000000 /* PCI cache line size */
#define SF_PCI_WRI	0x10000000 /* PCI write and invalidate */

void    siop_attach __P((struct siop_softc *));
int	siop_intr __P((void *));
