/*	$OpenBSD: ravenvar.h,v 1.3 2004/01/29 10:58:06 miod Exp $ */

/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 *	This product includes software developed under OpenBSD for RTMX Inc
 *	by Per Fogelstrom, Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * Motorola 'Raven' PowerPC to PCI bridge controller
 */

#ifndef _DEV_RAVENVAR_H_
#define _DEV_RAVENVAR_H_

#define MPCIC_BASE	0xFC000000
#define	MPCIC_SIZE	0x00022000

#define MPCIC_FEATURE	0x01000
#define MPCIC_GCR	0x01020
#define MPCIC_VID	0x01080
#define MPCIC_PINIT	0x01090
#define MPCIC_IPI0	0x010A0
#define MPCIC_IPI1	0x010B0
#define MPCIC_IPI2	0x010C0
#define MPCIC_IPI3	0x010D0
#define MPCIC_SP	0x010E0
#define MPCIC_TFR	0x010F0
#define MPCIC_T0CC	0x01100
#define MPCIC_T0BC	0x01110
#define MPCIC_T0VP	0x01120
#define MPCIC_T0D	0x01130
#define MPCIC_T1CC	0x01140
#define MPCIC_T1BC	0x01150
#define MPCIC_T1VP	0x01160
#define MPCIC_T1D	0x01170
#define MPCIC_T2CC	0x01180
#define MPCIC_T2BC	0x01190
#define MPCIC_T2VP	0x011A0
#define MPCIC_T2D	0x011B0
#define MPCIC_T3CC	0x011C0
#define MPCIC_T3BC	0x011D0
#define MPCIC_T3VP	0x011E0
#define MPCIC_T3D	0x011F0
#define MPCIC_INT0VP	0x10000
#define MPCIC_INT0D	0x10010
#define MPCIC_INT1VP	0x10020
#define MPCIC_INT1D	0x10030
#define MPCIC_INT2VP	0x10040
#define MPCIC_INT2D	0x10050
#define MPCIC_INT3VP	0x10060
#define MPCIC_INT3D	0x10070
#define MPCIC_INT4VP	0x10080
#define MPCIC_INT4D	0x10090
#define MPCIC_INT5VP	0x100A0
#define MPCIC_INT5D	0x100B0
#define MPCIC_INT6VP	0x100C0
#define MPCIC_INT6D	0x100D0
#define MPCIC_INT7VP	0x100E0
#define MPCIC_INT7D	0x100F0
#define MPCIC_INT8VP	0x10100
#define MPCIC_INT8D	0x10110
#define MPCIC_INT9VP	0x10120
#define MPCIC_INT9D	0x10130
#define MPCIC_INT10VP	0x10140
#define MPCIC_INT10D	0x10150
#define MPCIC_INT11VP	0x10160
#define MPCIC_INT11D	0x10170
#define MPCIC_INT12VP	0x10180
#define MPCIC_INT12D	0x10190
#define MPCIC_INT13VP	0x101A0
#define MPCIC_INT13D	0x101B0
#define MPCIC_INT14VP	0x101C0
#define MPCIC_INT14D	0x101D0
#define MPCIC_INT15VP	0x101E0
#define MPCIC_INT15D	0x101F0
#define MPCIC_EVP	0x10200
#define MPCIC_ED	0x10210
#define MPCIC_P0_IPI0_D	0x20040
#define MPCIC_P0_IPI1_D	0x20050
#define MPCIC_P0_IPI2_D	0x20060
#define MPCIC_P0_IPI3_D	0x20070
#define MPCIC_P0_TP	0x20080
#define MPCIC_P0_IACK	0x200A0
#define MPCIC_P0_EOI	0x200B0
#define MPCIC_P1_IPI0_D	0x21040
#define MPCIC_P1_IPI1_D	0x21050
#define MPCIC_P1_IPI2_D	0x21060
#define MPCIC_P1_IPI3_D	0x21070
#define MPCIC_P1_TP	0x21080
#define MPCIC_P1_IACK	0x210A0
#define MPCIC_P1_EOI	0x210B0

#define PROC0	0x01
#define PROC1	0x02

#define GCR_M		0x04
#define VP_MASKED	0x00000080
#define VP_LEVEL	0x00004000
#define VP_POL		0x00008000
#define VP_VEC(x)	((x) << 24)
#define VP_PRI(x)	((x) << 8)

struct mpic_feature {
	unsigned int	res1 : 4,
	                nirq : 12,
			res2 : 3,
	                ncpu : 5,
			vid  : 8;
};

struct mpic_gcr {
	unsigned int	reset : 1,
			res1 : 1,
			cmode : 1,
                        res2 : 29;
};

struct mpic_vid {
	unsigned int	res1 : 8,
			stp : 8,
			res2 : 16;
};


struct mpic_ipivp {
	unsigned int	masked : 1,
			act : 1,
			res1 : 10,
			pri : 4,
			res2 : 8,
			vec : 8;
};

struct mpic_timer_count {
	unsigned int	toggle : 1,
			count : 31;
};

struct mpic_timer_bcount {
	unsigned int	inhib : 1,
			count : 31;
};

struct mpic_timer_vp {
	unsigned int	masked : 1,
			act : 1,
			res1 : 10,
			pri : 4,
			res2 : 8,
			vec : 8;
};

struct mpic_timer {
	struct mpic_timer_count *cr;
        struct mpic_timer_bcount *bcr;
	struct mpic_timer_vp *vp;
	unsigned char *dest;
};

struct mpic_ext_vp {
	unsigned int	masked : 1,
			act : 1,
			res1 : 6,
			polarity : 1,
			sense : 1,
			res2 : 2,
			pri : 4,
			res3 : 8,
			vec : 8;
};

struct mpic_ext_intr {
	volatile unsigned int *vp;
	volatile unsigned char *dest;
};

struct mpic_err_vp {
	unsigned int	masked : 1,
			act : 1,
			res3 : 7,
			sense : 1,
			res2 : 2,
			pri : 4,
			res1 : 8,
			vec : 8;
};

#if 1
struct raven_reg {
	struct mpic_feature *feature;
	unsigned int *gcr;
	struct mpic_vid *vid;
	char *p_init;
	struct mpic_ipivp *ipi[4];
	char *sp;
	unsigned int *timer_freq;
	struct mpic_timer timer[4];
	/* external interrupt configuration registers */
        struct mpic_ext_intr extint[16];
	unsigned int *p0_ipi0d;
        unsigned int *p0_ipi1d;
        unsigned int *p0_ipi2d;
        unsigned int *p0_ipi3d;
        unsigned int *p1_ipi0d;
        unsigned int *p1_ipi1d;
        unsigned int *p1_ipi2d;
        unsigned int *p1_ipi3d;
	/* task priority registers (IPL) */
	unsigned char *tp[2];
	/* interrupt acknowledge registers */
	volatile unsigned char *iack[2];
	/* end of interrupt registers */
	volatile unsigned char *eio[2];
};

#else
struct raven_reg {
	struct mpic_feature *feature = (struct mpic_feature *)MPCIC_FEATURE;
	struct mpic_gcr *gcr = (struct mpic_gcr *)MPCIC_GCR;
	struct mpic_vid *vid = (struct mpic_vid *)MPCIC_VID;
	char *p_init = (char *)MPCIC_PINIT;
#if 0
	struct mpic_ipivp *ipi0 = (struct mpic_ipivp *)MPCIC_IPI0;
	struct mpic_ipivp *ipi1 = (struct mpic_ipivp *)MPCIC_IPI1;
	struct mpic_ipivp *ipi2 = (struct mpic_ipivp *)MPCIC_IPI2;
	struct mpic_ipivp *ipi3 = (struct mpic_ipivp *)MPCIC_IPI3;
#else 
	struct mpic_ipivp *ipi[4] = {
		(struct mpic_ipivp *)MPCIC_IPI0,
		(struct mpic_ipivp *)MPCIC_IPI1,
		(struct mpic_ipivp *)MPCIC_IPI2,
		(struct mpic_ipivp *)MPCIC_IPI3,
	};
#endif 
	char *sp = (char *)MPCIC_SP;
	unsigned int *timer_freq = (unsigned int *)MPCIC_TFR;
#if 1
	struct mpic_timer timer[4] = {
		{(struct mpic_timer_count *)MPCIC_T0CC, 
		(struct mpic_timer_bcount *)MPCIC_T0BC,
		(struct mpic_timer_vp *)MPCIC_T0VP,
		(unsigned char *)MPCIC_T0D},
		{(struct mpic_timer_count *)MPCIC_T1CC, 
		(struct mpic_timer_bcount *)MPCIC_T1BC,
		(struct mpic_timer_vp *)MPCIC_T1VP,
		(unsigned char *)MPCIC_T1D},
		{(struct mpic_timer_count *)MPCIC_T2CC, 
		(struct mpic_timer_bcount *)MPCIC_T2BC,
		(struct mpic_timer_vp *)MPCIC_T2VP,
		(unsigned char *)MPCIC_T2D},
		{(struct mpic_timer_count *)MPCIC_T3CC, 
		(struct mpic_timer_bcount *)MPCIC_T3BC,
		(struct mpic_timer_vp *)MPCIC_T3VP,
		(unsigned char *)MPCIC_T3D}
	};
#else
	struct mpic_timer_count *t0c = (struct mpic_timer_count *)MPCIC_T0CC;
        struct mpic_timer_bcount *t0bc = (struct mpic_timer_bcount *)MPCIC_T0BC;
	struct mpic_timer_vp *t0vp = (struct mpic_timer_vp *)MPCIC_T0VP;
	unsigned int *t0d = (unsigned int *)MPCIC_T0D;
	struct mpic_timer_count *t1c = (struct mpic_timer_count *)MPCIC_T1CC;
        struct mpic_timer_bcount *t1bc = (struct mpic_timer_bcount *)MPCIC_T1BC;
	struct mpic_timer_vp *t1vp = (struct mpic_timer_vp *)MPCIC_T1VP;
	unsigned int *t1d = (unsigned int *)MPCIC_T1D;
	struct mpic_timer_count *t2c = (struct mpic_timer_count *)MPCIC_T2CC;
        struct mpic_timer_bcount *t2bc = (struct mpic_timer_bcount *)MPCIC_T2BC;
	struct mpic_timer_vp *t2vp = (struct mpic_timer_vp *)MPCIC_T2VP;
	unsigned int *t2d = (unsigned int *)MPCIC_T2D;
	struct mpic_timer_count *t3c = (struct mpic_timer_count *)MPCIC_T3CC;
        struct mpic_timer_bcount *t3bc = (struct mpic_timer_bcount *)MPCIC_T3BC;
	struct mpic_timer_vp *t3vp = (struct mpic_timer_vp *)MPCIC_T3VP;
	unsigned int *t3d = (unsigned int *)MPCIC_T3D;
#endif 

#if 1
	/* external interrupt configuration registers */
        struct mpic_ext_intr extint[16] = {
		{(struct mpic_ext_vp *)MPCIC_INT0VP, (unsigned char *)MPCIC_INT0D}, 
		{(struct mpic_ext_vp *)MPCIC_INT1VP, (unsigned char *)MPCIC_INT1D}, 
		{(struct mpic_ext_vp *)MPCIC_INT2VP, (unsigned char *)MPCIC_INT2D}, 
		{(struct mpic_ext_vp *)MPCIC_INT3VP, (unsigned char *)MPCIC_INT3D}, 
		{(struct mpic_ext_vp *)MPCIC_INT4VP, (unsigned char *)MPCIC_INT4D}, 
		{(struct mpic_ext_vp *)MPCIC_INT5VP, (unsigned char *)MPCIC_INT5D}, 
		{(struct mpic_ext_vp *)MPCIC_INT6VP, (unsigned char *)MPCIC_INT6D}, 
		{(struct mpic_ext_vp *)MPCIC_INT7VP, (unsigned char *)MPCIC_INT7D}, 
		{(struct mpic_ext_vp *)MPCIC_INT8VP, (unsigned char *)MPCIC_INT8D}, 
		{(struct mpic_ext_vp *)MPCIC_INT9VP, (unsigned char *)MPCIC_INT9D}, 
		{(struct mpic_ext_vp *)MPCIC_INT1VP, (unsigned char *)MPCIC_INT10D}, 
		{(struct mpic_ext_vp *)MPCIC_INT11VP, (unsigned char *)MPCIC_INT11D}, 
		{(struct mpic_ext_vp *)MPCIC_INT12VP, (unsigned char *)MPCIC_INT12D}, 
		{(struct mpic_ext_vp *)MPCIC_INT13VP, (unsigned char *)MPCIC_INT13D}, 
		{(struct mpic_ext_vp *)MPCIC_INT14VP, (unsigned char *)MPCIC_INT14D}, 
		{(struct mpic_ext_vp *)MPCIC_INT16VP, (unsigned char *)MPCIC_INT15D} 
	};
#else
	struct mpic_ext_vp *ext0vp = (struct mpic_ext_vp *)MPCIC_INT0VP;
	unsigned int *ext0d = (unsigned int *)MPCIC_INT0D;
        struct mpic_ext_vp *ext1vp = (struct mpic_ext_vp *)MPCIC_INT1VP;
	unsigned int *ext1d = (unsigned int *)MPCIC_INT1D;
        struct mpic_ext_vp *ext2vp = (struct mpic_ext_vp *)MPCIC_INT2VP;
	unsigned int *ext2d = (unsigned int *)MPCIC_INT2D;
        struct mpic_ext_vp *ext3vp = (struct mpic_ext_vp *)MPCIC_INT3VP;
	unsigned int *ext3d = (unsigned int *)MPCIC_INT3D;
        struct mpic_ext_vp *ext4vp = (struct mpic_ext_vp *)MPCIC_INT4VP;
	unsigned int *ext4d = (unsigned int *)MPCIC_INT4D;
        struct mpic_ext_vp *ext5vp = (struct mpic_ext_vp *)MPCIC_INT5VP;
	unsigned int *ext5d = (unsigned int *)MPCIC_INT5D;
        struct mpic_ext_vp *ext6vp = (struct mpic_ext_vp *)MPCIC_INT6VP;
	unsigned int *ext6d = (unsigned int *)MPCIC_INT6D;
        struct mpic_ext_vp *ext7vp = (struct mpic_ext_vp *)MPCIC_INT7VP;
	unsigned int *ext7d = (unsigned int *)MPCIC_INT7D;
        struct mpic_ext_vp *ext8vp = (struct mpic_ext_vp *)MPCIC_INT8VP;
	unsigned int *ext8d = (unsigned int *)MPCIC_INT8D;
        struct mpic_ext_vp *ext9vp = (struct mpic_ext_vp *)MPCIC_INT9VP;
	unsigned int *ext9d = (unsigned int *)MPCIC_INT9D;
        struct mpic_ext_vp *ext10vp = (struct mpic_ext_vp *)MPCIC_INT10VP;
	unsigned int *ext10d = (unsigned int *)MPCIC_INT10D;
        struct mpic_ext_vp *ext11vp = (struct mpic_ext_vp *)MPCIC_INT11VP;
	unsigned int *ext11d = (unsigned int *)MPCIC_INT11D;
        struct mpic_ext_vp *ext12vp = (struct mpic_ext_vp *)MPCIC_INT12VP;
	unsigned int *ext12d = (unsigned int *)MPCIC_INT12D;
        struct mpic_ext_vp *ext13vp = (struct mpic_ext_vp *)MPCIC_INT13VP;
	unsigned int *ext13d = (unsigned int *)MPCIC_INT13D;
        struct mpic_ext_vp *ext14vp = (struct mpic_ext_vp *)MPCIC_INT14VP;
	unsigned int *ext14d = (unsigned int *)MPCIC_INT14D;
        struct mpic_ext_vp *ext15vp = (struct mpic_ext_vp *)MPCIC_INT15VP;
	unsigned int *ext15d = (unsigned int *)MPCIC_INT15D;
        struct mpic_err_vp *errvp = (struct mpic_err_vp *)MPCIC_EVP;
	unsigned int *errd = (unsigned int *)MPCIC_ED;
#endif
	unsigned int *p0_ipi0d = (unsigned int *)MPCIC_P0_IPI0_D;
        unsigned int *p0_ipi1d = (unsigned int *)MPCIC_P0_IPI1_D;
        unsigned int *p0_ipi2d = (unsigned int *)MPCIC_P0_IPI2_D;
        unsigned int *p0_ipi3d = (unsigned int *)MPCIC_P0_IPI3_D;
        unsigned int *p1_ipi0d = (unsigned int *)MPCIC_P1_IPI0_D;
        unsigned int *p1_ipi1d = (unsigned int *)MPCIC_P1_IPI1_D;
        unsigned int *p1_ipi2d = (unsigned int *)MPCIC_P1_IPI2_D;
        unsigned int *p1_ipi3d = (unsigned int *)MPCIC_P1_IPI3_D;
        
	/* task priority registers (IPL) */
	unsigned char *tp[2] = {
		(unsigned char *)MPCIC_P0_TP,
		(unsigned char *)MPCIC_P1_TP
	};
	/* interrupt acknowledge registers */
	volatile unsigned char *iack[2] = {
		(volatile unsigned char *)MPCIC_P0_IACK,
		(volatile unsigned char *)MPCIC_P1_IACK
	};
	/* end of interrupt registers */
	volatile unsigned char *eio[2] = {
		(volatile unsigned char *)MPCIC_P0_EOI,
		(volatile unsigned char *)MPCIC_P1_EOI,
	};
}
#endif

struct raven_softc {
	struct device	sc_dev;
	u_int8_t	*sc_regs;
};

#endif /* _DEV_RAVENVAR_H_ */
