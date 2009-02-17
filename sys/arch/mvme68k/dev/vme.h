/*	$OpenBSD: vme.h,v 1.13 2009/02/17 22:28:41 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 */

struct vmesoftc {
	struct device		sc_dev;
	vaddr_t			sc_vaddr;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_abih;	/* `abort' switch */
};

struct vmessoftc {
	struct device		sc_dev;
	struct vmesoftc		*sc_vme;
};

struct vmelsoftc {
	struct device		sc_dev;
	struct vmesoftc		*sc_vme;
};

/*
 * MVME147 vme configuration registers.
*/
struct vme1reg {
/*01*/	volatile u_short		vme1_scon;
#define VME1_SCON_SWITCH	0x01		/* SCON jumper is set */
#define VME1_SCON_SRESET	0x02		/* assert SRESET on bus */
#define VME1_SCON_SYSFAIL	0x04		/* assert SYSFAIL on bus */
#define VME1_SCON_ROBIN		0x08		/* round robin bus requests */
/*03*/	volatile u_short		vme1_reqconf;
#define VME1_REQ_IPLMASK	0x03		/* interrupt level for requester */
#define VME1_REQ_RNEVER		0x08
#define VME1_REQ_RWD		0x10
#define VME1_REQ_DHB		0x40
#define VME1_REQ_DWB		0x80
/*05*/	volatile u_short		vme1_masconf;
#define VME1_MAS_D16		0x01		/* force d8/16 accesses only */
#define VME1_MAS_MASA24		0x02		/* send address mod for A24 access */
#define VME1_MAS_MASA16		0x04		/* send address mod for A16 access */
#define VME1_MAS_MASUAT		0x08		/* handle unaligned VME cycles */
#define VME1_MAS_CFILL		0x10		/* DO NOT USE */
#define VME1_MAS_MASWP		0x20		/* VME fast mode (DO NOT USE) */
/*07*/	volatile u_short		vme1_slconf;
#define VME1_SLAVE_SLVD16	0x01		/* DO NOT USE */
#define VME1_SLAVE_SLVWP	0x20		/* DO NOT USE */
#define VME1_SLAVE_SLVEN	0x80		/* allow access to onboard DRAM */
/*09*/	volatile u_short		vme1_timerconf;
#define VME1_TIMER_LOCAL_MASK	0x03
#define VME1_TIMER_LOCAL_T0	0x00		/* local timeout 102 microsec */
#define VME1_TIMER_LOCAL_T1	0x01		/* local timeout 205 microsec */
#define VME1_TIMER_LOCAL_T2	0x02		/* local timeout 410 microsec */
#define VME1_TIMER_LOCAL_T3	0x03		/* local timeout disabled */
#define VME1_TIMER_VMEACC_MASK	0x0c
#define VME1_TIMER_VMEACC_T0	0x00		/* VME access timeout 102 microsec */
#define VME1_TIMER_VMEACC_T1	0x04		/* VME access timeout 1.6 millisec */
#define VME1_TIMER_VMEACC_T2	0x08		/* VME access timeout 51 millisec */
#define VME1_TIMER_VMEACC_T3	0x0c		/* VME access timeout disabled */
#define VME1_TIMER_VMEGLO_MASK	0x30
#define VME1_TIMER_VMEGLO_T0	0x00		/* VME glob timeout 102 microsec */
#define VME1_TIMER_VMEGLO_T1	0x10		/* VME glob timeout 205 microsec */
#define VME1_TIMER_VMEGLO_T2	0x20		/* VME glob timeout 410 microsec */
#define VME1_TIMER_VMEGLO_T3	0x30		/* VME glob timeout disabled */
#define VME1_TIMER_ARBTO	0x40		/* enable VME arbitration timer */
/*0b*/	volatile u_short		vme1_sladdrmod;
#define VME1_SLMOD_DATA		0x01
#define VME1_SLMOD_PRGRM	0x02
#define VME1_SLMOD_BLOCK	0x04
#define VME1_SLMOD_SHORT	0x08
#define VME1_SLMOD_STND		0x10
#define VME1_SLMOD_EXTED	0x20
#define VME1_SLMOD_USER		0x40
#define VME1_SLMOD_SUPER	0x80
/*0d*/	volatile u_short		vme1_msaddrmod;
#define VME1_MSMOD_AM_MASK	0x3f
#define VME1_MSMOD_AMSEL	0x80
/*0f*/	volatile u_short		vme1_irqen;
#define VME1_IRQ_VME(x)		(1 << (x))
/*11*/	volatile u_short		vme1_uirqen;
/*13*/	volatile u_short		vme1_uirq;
/*15*/	volatile u_short		vme1_irq;
/*17*/	volatile u_short		vme1_vmeid;
/*19*/	volatile u_short		vme1_buserr;
/*1b*/	volatile u_short		vme1_gcsr;
#define VME1_GCSR_OFF		0x0f
/*1d*/	u_short			:16;
/*1f*/	u_short			:16;
/*21*/	volatile u_short		vme1_gcsr_gr0;
/*23*/	volatile u_short		vme1_gcsr_gr1;
/*25*/	volatile u_short		vme1_gcsr_boardid;
/*27*/	volatile u_short		vme1_gcsr_gpr0;
/*29*/	volatile u_short		vme1_gcsr_gpr1;
/*2b*/	volatile u_short		vme1_gcsr_gpr2;
/*2d*/	volatile u_short		vme1_gcsr_gpr3;
/*2f*/	volatile u_short		vme1_gcsr_gpr4;
};

/*
 * Basic VME memory layout for the MVME147 follows:
 *    - A32D32 accesses occur at memsize-0xefffffff. This makes it
 *	impossible to do A32D32 accesses before the end of your onboard
 *	memory. If you want to do low address A24D32 accesses, and you
 *	have 16M or more onboard memory you'll find you cannot.
 *    - A32D16 accesses can occur at 0xf0000000-0xff7fffff.
 *    - A16D16 accesses can occur at 0xffff0000-0xffffffff.
 */
#define VME1_A32D32BASE	0x00000000UL
#define VME1_A32D32LEN	0xf0000000UL
#define VME1_A32D16BASE	0xf0000000UL
#define VME1_A32D16LEN	0x0f800000UL
#define VME1_A16D16BASE	0xffff0000UL
#define VME1_A16D16LEN	0x00010000UL
#define VME1_A16BASE	0xffff0000UL

/*
 * XXX: this chip has some rather insane access rules!
 */
struct vme2reg {
/*00*/	volatile u_long		vme2_slaveaddr1;
/*04*/	volatile u_long		vme2_slaveaddr2;
#define VME2_SADDR_END		0xffff0000		/* VME address END & START */
#define VME2_SADDR_START	0x0000ffff
/*08*/	volatile u_long		vme2_slavelmod1;
/*0c*/	volatile u_long		vme2_slavelmod2;
#define VME2_SADDR_LADDR	0xffff0000		/* local base address */
#define VME2_SADDR_SIZE(mem)	(0x1000 - (mem) >> 16)	/* encoding of size */
/*10*/	volatile u_long		vme2_slavectl;
#define VME2_SLAVE_CHOOSE(bits, num) ((bits) << (16*((num)-1)))
#define VME2_SLAVECTL_WP	0x00000100		/* write posting */
#define VME2_SLAVECTL_SNP_NO	0x00000000		/* no snooping */
#define VME2_SLAVECTL_SNP_SINK	0x00000200		/* sink data */
#define VME2_SLAVECTL_SNP_INVAL	0x00000400		/* invalidate */
#define VME2_SLAVECTL_ADDER	0x00000800		/* use adder */
#define VME2_SLAVECTL_SUP	0x00000080		/* modifier bit */
#define VME2_SLAVECTL_USR	0x00000040		/* modifier bit */
#define VME2_SLAVECTL_A32	0x00000020		/* modifier bit */
#define VME2_SLAVECTL_A24	0x00000010		/* modifier bit */
#define VME2_SLAVECTL_D64	0x00000008		/* modifier bit */
#define VME2_SLAVECTL_BLK	0x00000004		/* modifier bit */
#define VME2_SLAVECTL_PGM	0x00000002		/* modifier bit */
#define VME2_SLAVECTL_DAT	0x00000001		/* modifier bit */
/*14*/	volatile u_long		vme2_master1;
/*18*/	volatile u_long		vme2_master2;
/*1c*/	volatile u_long		vme2_master3;
/*20*/	volatile u_long		vme2_master4;
/*24*/	volatile u_long		vme2_master4mod;
/*28*/	volatile u_long		vme2_masterctl;
#define VME2_MASTERCTL_4SHIFT	24
#define VME2_MASTERCTL_3SHIFT	16
#define VME2_MASTERCTL_2SHIFT	8
#define VME2_MASTERCTL_1SHIFT	0
#define VME2_MASTERCTL_D16	0x80
#define VME2_MASTERCTL_WP	0x40
#define VME2_MASTERCTL_AM	0x3f
#define	VME2_MASTERCTL_AM24SB	0x3f	/* A24 Supervisory Block Transfer */
#define	VME2_MASTERCTL_AM24SP	0x3e	/* A24 Supervisory Program Access */
#define	VME2_MASTERCTL_AM24SD	0x3d	/* A24 Supervisory Data Access */
#define	VME2_MASTERCTL_AM24UB	0x3b	/* A24 Non-priv. Block Transfer */
#define	VME2_MASTERCTL_AM24UP	0x3a	/* A24 Non-priv. Program Access */
#define	VME2_MASTERCTL_AM24UD	0x39	/* A24 Non-priv. Data Access */
#define	VME2_MASTERCTL_AM16S	0x2d	/* A16 Supervisory Access */
#define	VME2_MASTERCTL_AM16U	0x29	/* A16 Non-priv. Access */
#define	VME2_MASTERCTL_AM32SB	0x0f	/* A32 Supervisory Block Transfer */
#define	VME2_MASTERCTL_AM32SP	0x0e	/* A32 Supervisory Program Access */
#define	VME2_MASTERCTL_AM32SD	0x0d	/* A32 Supervisory Data Access */
#define	VME2_MASTERCTL_AM32UB	0x0b	/* A32 Non-priv. Block Transfer */
#define	VME2_MASTERCTL_AM32UP	0x0a	/* A32 Non-priv. Program Access */
#define	VME2_MASTERCTL_AM32UD	0x09	/* A32 Non-priv Data Access */

#define VME2_MASTERCTL_ALL	0xff
/*2c*/	volatile u_long		vme2_gcsrctl;
#define VME2_GCSRCTL_OFF	0xf0000000
#define VME2_GCSRCTL_MDEN4	0x00080000
#define VME2_GCSRCTL_MDEN3	0x00040000
#define VME2_GCSRCTL_MDEN2	0x00020000
#define VME2_GCSRCTL_MDEN1	0x00010000
#define VME2_GCSRCTL_I2EN	0x00008000	/* F decode (A24D16/A32D16) on */
#define VME2_GCSRCTL_I2WP	0x00004000	/* F decode write post */
#define VME2_GCSRCTL_I2SU	0x00002000	/* F decode is supervisor */
#define VME2_GCSRCTL_I2PD	0x00001000	/* F decode is program */
#define VME2_GCSRCTL_I1EN	0x00000800	/* short decode (A16Dx) on */
#define VME2_GCSRCTL_I1D16	0x00000400	/* short decode is D16 */
#define VME2_GCSRCTL_I1WP	0x00000200	/* short decode write post */
#define VME2_GCSRCTL_I1SU	0x00000100	/* short decode is supervisor */
#define VME2_GCSRCTL_ROMSIZE	0x000000c0	/* size of ROM */
#define VME2_GCSRCTL_ROMBSPD	0x00000038	/* speed of ROM */
#define VME2_GCSRCTL_ROMASPD	0x00000007	/* speed of ROM */
/*30*/	volatile u_long		vme2_dmactl;
/*34*/	volatile u_long		vme2_dmamode;
/*38*/	volatile u_long		vme2_dmaladdr;
/*3c*/	volatile u_long		vme2_dmavmeaddr;
/*40*/	volatile u_long		vme2_dmacount;
/*44*/	volatile u_long		vme2_dmatable;
/*48*/	volatile u_long		vme2_dmastat;
/*4c*/	volatile u_long		vme2_vmejunk;
/*50*/	volatile u_long		vme2_t1cmp;
/*54*/	volatile u_long		vme2_t1count;
/*58*/	volatile u_long		vme2_t2cmp;
/*5c*/	volatile u_long		vme2_t2count;
/*60*/	volatile u_long		vme2_tctl;
#define VME2_TCTL_CEN		0x01
#define VME2_TCTL_COC		0x02
#define VME2_TCTL_COVF		0x04
#define VME2_TCTL_OVF		0xf0
#define VME2_TCTL_SCON		0x40000000	/* we are SCON */
#define VME2_TCTL_SYSFAIL	0x20000000	/* light SYSFAIL led */
#define VME2_TCTL_SRST		0x00800000	/* system reset */
/*64*/	volatile u_long		vme2_prescale;
/*68*/	volatile u_long		vme2_irqstat;
/*6c*/	volatile u_long		vme2_irqen;
/*70*/	volatile u_long		vme2_setsoftirq;	/* VME2_IRQ_SWx only */
/*74*/	volatile u_long		vme2_irqclr;		/* except VME2_IRQ_VMEx */
#define VME2_IRQ_ACF		0x80000000
#define VME2_IRQ_AB		0x40000000
#define VME2_IRQ_SYSF		0x20000000
#define VME2_IRQ_MWP		0x10000000
#define VME2_IRQ_PE		0x08000000
#define VME2_IRQ_V1IE		0x04000000
#define VME2_IRQ_TIC2		0x02000000
#define VME2_IRQ_TIC1		0x01000000
#define VME2_IRQ_VIA		0x00800000
#define VME2_IRQ_DMA		0x00400000
#define VME2_IRQ_SIG3		0x00200000
#define VME2_IRQ_SIG2		0x00100000
#define VME2_IRQ_SIG1		0x00080000
#define VME2_IRQ_SIG0		0x00040000
#define VME2_IRQ_LM1		0x00020000
#define VME2_IRQ_LM0		0x00010000
#define VME2_IRQ_SW7		0x00008000
#define VME2_IRQ_SW6		0x00004000
#define VME2_IRQ_SW5		0x00002000
#define VME2_IRQ_SW4		0x00001000
#define VME2_IRQ_SW3		0x00000800
#define VME2_IRQ_SW2		0x00000400
#define VME2_IRQ_SW1		0x00000200
#define VME2_IRQ_SW0		0x00000100
#define VME2_IRQ_SPARE		0x00000080
#define VME2_IRQ_VME7		0x00000040
#define VME2_IRQ_VME6		0x00000020
#define VME2_IRQ_VME5		0x00000010
#define VME2_IRQ_VME4		0x00000008
#define VME2_IRQ_VME3		0x00000004
#define VME2_IRQ_VME2		0x00000002
#define VME2_IRQ_VME1		0x00000001
#define VME2_IRQ_VME(x)		(1 << ((x) - 1))
/*78*/	volatile u_long		vme2_irql1;
#define VME2_IRQL1_ACFSHIFT	28
#define VME2_IRQL1_ABSHIFT	24
#define VME2_IRQL1_SYSFSHIFT	20
#define VME2_IRQL1_WPESHIFT	16
#define VME2_IRQL1_PESHIFT	12
#define VME2_IRQL1_V1IESHIFT	8
#define VME2_IRQL1_TIC2SHIFT	4
#define VME2_IRQL1_TIC1SHIFT	0
/*7c*/	volatile u_long		vme2_irql2;
#define VME2_IRQL2_VIASHIFT	28
#define VME2_IRQL2_DMASHIFT	24
#define VME2_IRQL2_SIG3SHIFT	20
#define VME2_IRQL2_SIG2SHIFT	16
#define VME2_IRQL2_SIG1SHIFT	12
#define VME2_IRQL2_SIG0SHIFT	8
#define VME2_IRQL2_LM1SHIFT	4
#define VME2_IRQL2_LM0SHIFT	0
/*80*/	volatile u_long		vme2_irql3;
#define VME2_IRQL3_SW7SHIFT	28
#define VME2_IRQL3_SW6SHIFT	24
#define VME2_IRQL3_SW5SHIFT	20
#define VME2_IRQL3_SW4SHIFT	16
#define VME2_IRQL3_SW3SHIFT	12
#define VME2_IRQL3_SW2SHIFT	8
#define VME2_IRQL3_SW1SHIFT	4
#define VME2_IRQL3_SW0SHIFT	0
/*84*/	volatile u_long		vme2_irql4;
#define VME2_IRQL4_SPARESHIFT	28
#define VME2_IRQL4_VME7SHIFT	24
#define VME2_IRQL4_VME6SHIFT	20
#define VME2_IRQL4_VME5SHIFT	16
#define VME2_IRQL4_VME4SHIFT	12
#define VME2_IRQL4_VME3SHIFT	8
#define VME2_IRQL4_VME2SHIFT	4
#define VME2_IRQL4_VME1SHIFT	0
/*88*/	volatile u_long		vme2_vbr;
#define VME2_VBR_0SHIFT		28
#define VME2_VBR_1SHIFT		24
#define VME2_VBR_GPOXXXX	0x00ffffff
/*8c*/	volatile u_long		vme2_misc;
#define VME2_MISC_MPIRQEN	0x00000080	/* do not set */	
#define VME2_MISC_REVEROM	0x00000040	/* 167: dis eprom. 166: en flash */
#define VME2_MISC_DISSRAM	0x00000020	/* do not set */
#define VME2_MISC_DISMST	0x00000010	
#define VME2_MISC_NOELBBSY	0x00000008	/* do not set */
#define VME2_MISC_DISBSYT	0x00000004	/* do not set */
#define VME2_MISC_ENINT		0x00000002	/* do not set */
#define VME2_MISC_DISBGN	0x00000001	/* do not set */
};

#define VME2_A16D32BASE	0xffff0000UL
#define VME2_A16D32LEN	0x00010000UL
#define VME2_A32D16BASE	0xf1000000UL
#define VME2_A32D16LEN	0x01000000UL
#define VME2_A16D16BASE	0xffff0000UL
#define VME2_A16D16LEN	0x00010000UL
#define VME2_A24D16BASE	0xf0000000UL
#define VME2_A24D16LEN	0x01000000UL
#define VME2_A16BASE	0xffff0000UL
#define VME2_A24BASE	0xff000000UL

paddr_t	vmepmap(struct vmesoftc *sc, paddr_t vmeaddr, int len, int bustype);
vaddr_t	vmemap(struct vmesoftc *sc, paddr_t vmeaddr, int len, int bustype);
int	vmerw(struct vmesoftc *sc, struct uio *uio, int flags, int bus);

int vmeintr_establish(int, struct intrhand *, const char *);
int vmescan(struct device *, void *, void *, int);
