/*	$OpenBSD: if_nireg.h,v 1.2 2003/06/02 23:27:56 millert Exp $ */
/*	$NetBSD: if_nireg.h,v 1.3 2001/08/20 12:20:07 wiz Exp $	*/
/*
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
 *	@(#)nireg.h	7.3 (Berkeley) 6/28/90
 */

/*
 * Registers for the DEBNA and DEBNK Ethernet interfaces
 * (DEC calls these Network Interfaces, hence nireg.h)
 */

/*
 * this seems to be intended to be more general, but I have no details,
 * so it goes here for now
 *
 * BI Vax Port (BVP) stuff first:
 */
#ifdef notdef
struct bvpregs {
	u_long	p_pcr;		/* port control register */
	u_long	p_psr;		/* port status register */
	u_long	p_per;		/* port error register */
	u_long	p_pdr;		/* port data register */
};

/*
 * BI node space registers
 */
struct ni_regs {
	struct	biiregs ni_bi;	/* BIIC registers, except GPRs */
	struct	bvpregs ni_tkp; /* tk50 port control via BIIC GPRs */
	u_long	ni_xxx[64];	/* unused */
	u_long	ni_rxcd;	/* receive console data */
	struct	bvpregs ni_nip; /* NI port control via BCI3 GPRs */
	u_long	ni_pudr;	/* power-up diagnostic register */
};
#endif

#define NI_PCR	0x204
#define NI_PSR	0x208
#define NI_PER	0x20c
#define NI_PDR	0x210
#define NI_PUDR 0x204

/* bits in PCR */
#define PCR_OWN		0x80
#define PCR_MFREEQ	0x000
#define PCR_DFREEQ	0x100
#define PCR_RFREEQ	0x200
#define PCR_IFREEQ	0x300
#define	PCR_CMDQ0	PCR_MFREEQ
#define	PCR_CMDQ1	PCR_DFREEQ
#define	PCR_CMDQ2	PCR_RFREEQ
#define	PCR_CMDQ3	PCR_IFREEQ
#define	PCR_RESTART	11
#define PCR_FREEQNE	7
#define PCR_CMDQNE	6
#define	PCR_SHUTDOWN	4
#define PCR_ENABLE	2
#define PCR_INIT	1

/* bits in PSR */
#define PSR_OWN		0x80000000
#define PSR_STATE	0x00070000
#define PSR_STOPPED	0x00060000
#define PSR_ENABLED	0x00040000
#define PSR_INITED	0x00020000
#define PSR_UNDEF	0x00010000
#define PSR_RSQ		0x00000080
#define	PSR_ERR		0x00000040

/*
 * The DEBNx uses a very weird (set of) structure(s) to communicate
 * with something as simple as an ethernet controller. This is not
 * very different to the way communication is done over CI with disks.
 */

/* Message packet */
struct ni_msg {
	u_int32_t	nm_forw;
	u_int32_t	nm_back;
	u_int32_t	nm_pad1;
	u_int8_t	nm_pad2;
	u_int8_t	nm_status;
	u_int8_t	nm_opcode;
	u_int8_t	nm_pad3;
	u_int16_t	nm_len;
	u_int8_t	nm_opcode2;
	u_int8_t	nm_status2;
	u_int32_t	nm_pad4;
	u_int8_t	nm_text[128];
};

/* Datagram packet */
struct ni_dg {
	u_int32_t	nd_forw;
	u_int32_t	nd_back;
	u_int32_t	nd_pad1;
	u_int8_t	nd_pad2;
	u_int8_t	nd_status;
	u_int8_t	nd_opcode;
	u_int8_t	nd_pad3;
	u_int16_t	nd_len;
	u_int16_t	nd_status2;
	u_int32_t	nd_cmdref;	
	u_int32_t	nd_ptdbidx;	
	struct {
		u_int16_t	_offset;
		u_int16_t	_len;	
		u_int16_t	_index; 
		u_int16_t	_key;	
	} bufs[NTXFRAGS];
};	

#define	NIDG_CHAIN	0x8000

/* NI parameter block */
struct ni_param {	
	u_int8_t	np_dpa[8];
	u_int8_t	np_apa[8];
	u_int8_t	np_lsa[8]; 
	u_int8_t	np_bvc[8];
	u_int16_t	np_curaddr;	
	u_int16_t	np_maxaddr;
	u_int16_t	np_curptt;
	u_int16_t	np_maxptt;
	u_int16_t	np_curfq;
	u_int16_t	np_maxfq;
	u_int32_t	np_sid;
	u_int32_t	np_mop;
	u_int32_t	np_flags;
	u_int32_t	np_rcto;
	u_int32_t	np_xmto;
}; 

#define NP_ECT		0x01
#define NP_PAD		0x02
#define NP_BOO		0x04
#define NP_CAR		0x08
#define NP_ILP		0x10
#define NP_ELP		0x20
#define NP_DCRC		0x40
#define NP_THRU		0x80

/* Protocol type definition block */
struct ni_ptdb {
	u_int16_t	np_type;	/* Protocol type */
	u_int8_t	np_fque;	/* Free queue */
	u_int8_t	np_flags;	/* See below */
	u_int32_t	np_index;	/* protocol type index */
	u_int16_t	np_adrlen;	/* # of multicast addresses */
	u_int16_t	np_802;		/* for IEEE 802 packets */
	u_int8_t	np_mcast[16][8];/* Multicast (direct match) array */
};

#define	PTDB_PROMISC	0x08
#define	PTDB_802	0x10
#define	PTDB_BDC	0x20
#define	PTDB_UNKN	0x40
#define	PTDB_AMC	0x80

/* Buffer descriptor */
struct ni_bbd {
	u_int16_t	nb_status;	/* Offset, valid etc */
	u_int16_t	nb_key;
	u_int32_t	nb_len;		/* Buffer length */
	u_int32_t	nb_pte;		/* start (vax) PTE for this buffer */
	u_int32_t	nb_pad;
};	
#define NIBD_OFFSET	0x1ff
#define NIBD_VALID	0x8000


/* Free Queue Block */
struct ni_fqb { 
	u_int32_t	nf_mlen;
	u_int32_t	nf_mpad;
	u_int32_t	nf_mforw;
	u_int32_t	nf_mback;
	u_int32_t	nf_dlen;
	u_int32_t	nf_dpad;
	u_int32_t	nf_dforw;	
	u_int32_t	nf_dback;
	u_int32_t	nf_rlen;
	u_int32_t	nf_rpad;
	u_int32_t	nf_rforw;
	u_int32_t	nf_rback;
	u_int32_t	nf_ilen;
	u_int32_t	nf_ipad;
	u_int32_t	nf_iforw;
	u_int32_t	nf_iback;
};	

/* DEBNx specific part of Generic VAX Port */
struct ni_pqb {
	u_int16_t	np_veclvl;	/* Interrupt vector + level */
	u_int16_t	np_node;	/* Where to interrupt */
	u_int32_t	np_freeq;
	u_int32_t	np_vfqb;	/* Free queue block pointer */	
	u_int32_t	np_pad1[39];
	u_int32_t	np_bvplvl;
	u_int32_t	np_vpqb;	/* Virtual address of Generic PQB */
	u_int32_t	np_vbdt;	/* Virtual address of descriptors */
	u_int32_t	np_nbdr;	/* Number of descriptors */	
	u_int32_t	np_spt;		/* System Page Table */
	u_int32_t	np_sptlen;	/* System Page Table length */
	u_int32_t	np_gpt;		/* Global Page Table */
	u_int32_t	np_gptlen;	/* Global Page Table length */	
	u_int32_t	np_mask;
	u_int32_t	np_pad2[67];
};

/* "Generic VAX Port Control Block" whatever it means */
struct ni_gvppqb {
	u_int32_t	nc_forw0;
	u_int32_t	nc_back0;
	u_int32_t	nc_forw1;
	u_int32_t	nc_back1;
	u_int32_t	nc_forw2;
	u_int32_t	nc_back2;
	u_int32_t	nc_forw3;
	u_int32_t	nc_back3;
	u_int32_t	nc_forwr;
	u_int32_t	nc_backr;
	struct ni_pqb	nc_pqb;		/* DEBNx specific part of struct */
};


/* BVP opcodes, should be somewhere else */
#define BVP_DGRAM	1
#define BVP_MSG		2
#define BVP_DGRAMI	3
#define BVP_DGRAMRX	33
#define BVP_MSGRX	34
#define BVP_DGRAMIRX	35

/* NI-specific sub-opcodes */
#define NI_WSYSID	1
#define NI_RSYSID	2
#define NI_WPARAM	3
#define NI_RPARAM	4
#define NI_RCCNTR	5
#define NI_RDCNTR	6
#define NI_STPTDB	7
#define NI_CLPTDB	8

/* bits in ni_pudr */
#define PUDR_TAPE	0x40000000	/* tk50 & assoc logic ok */
#define PUDR_PATCH	0x20000000	/* patch logic ok */
#define PUDR_VRAM	0x10000000	/* DEBNx onboard RAM ok */
#define PUDR_VROM1	0x08000000	/* uVax ROM 1 ok */ /* ? */
#define PUDR_VROM2	0x04000000	/* uVax ROM 2 ok */
#define PUDR_VROM3	0x02000000	/* uVax ROM 3 ok */
#define PUDR_VROM4	0x01000000	/* uVax ROM 4 ok */
#define PUDR_UVAX	0x00800000	/* uVax passes self test */
#define PUDR_BI		0x00400000	/* BIIC and BCI3 chips ok */
#define PUDR_TMR	0x00200000	/* interval timer ok */
#define PUDR_IRQ	0x00100000	/* no IRQ lines stuck */
#define PUDR_NI		0x00080000	/* Ethernet ctlr ok */
#define PUDR_TK50	0x00040000	/* tk50 present */
#define PUDR_PRES	0x00001000	/* tk50 present (again?!) */
#define PUDR_UVINT	0x00000800	/* uVax-to-80186 intr logic ok */
#define PUDR_BUSHD	0x00000400	/* no bus hold errors */
#define PUDR_II32	0x00000200	/* II32 transceivers ok */
#define PUDR_MPSC	0x00000100	/* MPSC logic ok */
#define PUDR_GAP	0x00000080	/* gap-detect logic ok */
#define PUDR_MISC	0x00000040	/* misc. registers ok */
#define PUDR_UNEXP	0x00000020	/* unexpected interrupt trapped */
#define PUDR_80186	0x00000010	/* 80186 ok */
#define PUDR_PATCH2	0x00000008	/* patch logic ok (again) */
#define PUDR_8RAM	0x00000004	/* 80186 RAM ok */
#define PUDR_8ROM2	0x00000002	/* 80186 ROM1 ok */
#define PUDR_8ROM1	0x00000001	/* 80186 ROM2 ok */
