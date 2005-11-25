/*	$OpenBSD: vme.h,v 1.17 2005/11/25 22:14:32 miod Exp $ */

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

#ifndef __MVEME88K_DEV_VME_H__
#define	__MVEME88K_DEV_VME_H__

struct vmesoftc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand 	sc_abih;       /* `abort' switch */
};

/*
 * XXX: this chip has some rather insane access rules!
 */

#define	VME2_BASE		0xfff40000

#define	VME2_SADDR1		0x0000
#define	VME2_SADDR2		0x0004
#define	VME2_SLAVELMOD1		0x0008
#define	VME2_SLAVELMOD2		0x000c
#define	VME2_SLAVECTL		0x0010
#define	VME2_MASTER1		0x0014
#define	VME2_MASTER2		0x0018
#define	VME2_MASTER3		0x001c
#define	VME2_MASTER4		0x0020
#define	VME2_MASTER4MOD		0x0024
#define	VME2_MASTERCTL		0x0028
#define	VME2_GCSRCTL		0x002c
#define	VME2_DMACTL		0x0030
#define	VME2_DMAMODE		0x0034
#define	VME2_DMALADDR		0x0038
#define	VME2_DMAVMEADDR		0x003c
#define	VME2_DMACOUNT		0x0040
#define	VME2_DMATABLE		0x0044
#define	VME2_DMASTAT		0x0048
#define	VME2_TCR		0x004c
#define	VME2_T1CMP		0x0050
#define	VME2_T1COUNT		0x0054
#define	VME2_T2CMP		0x0058
#define	VME2_T2COUNT		0x005c
#define	VME2_TCTL		0x0060
#define	VME2_PRESCALE		0x0064
#define	VME2_IRQSTAT		0x0068
#define	VME2_IRQEN		0x006c
#define	VME2_SETSOFTIRQ		0x0070
#define	VME2_IRQCLR		0x0074
#define	VME2_IRQL1		0x0078
#define	VME2_IRQL2		0x007c
#define	VME2_IRQL3		0x0080
#define	VME2_IRQL4		0x0084
#define	VME2_VBR		0x0088
#define	VME2_MISC		0x008c

#define	VME2_SADDR_END		0xffff0000	/* VME address END & START */
#define	VME2_SADDR_START	0x0000ffff
#define	VME2_SADDR_LADDR	0xffff0000		/* local base address */
#define	VME2_SADDR_SIZE(mem)	(0x1000 - (mem) >> 16)	/* encoding of size */

#define	VME2_SLAVE_CHOOSE(bits, num) ((bits) << (16*((num)-1)))
#define	VME2_SLAVECTL_WP	0x00000100		/* write posting */
#define	VME2_SLAVECTL_SNP_NO	0x00000000		/* no snooping */
#define	VME2_SLAVECTL_SNP_SINK	0x00000200		/* sink data */
#define	VME2_SLAVECTL_SNP_INVAL	0x00000400		/* invalidate */
#define	VME2_SLAVECTL_ADDER	0x00000800		/* use adder */
#define	VME2_SLAVECTL_SUP	0x00000080		/* modifier bit */
#define	VME2_SLAVECTL_USR	0x00000040		/* modifier bit */
#define	VME2_SLAVECTL_A32	0x00000020		/* modifier bit */
#define	VME2_SLAVECTL_A24	0x00000010		/* modifier bit */
#define	VME2_SLAVECTL_D64	0x00000008		/* modifier bit */
#define	VME2_SLAVECTL_BLK	0x00000004		/* modifier bit */
#define	VME2_SLAVECTL_PGM	0x00000002		/* modifier bit */
#define	VME2_SLAVECTL_DAT	0x00000001		/* modifier bit */

#define	VME2_MASTERCTL_4SHIFT	24
#define	VME2_MASTERCTL_3SHIFT	16
#define	VME2_MASTERCTL_2SHIFT	8
#define	VME2_MASTERCTL_1SHIFT	0
#define	VME2_MASTERCTL_D16	0x80
#define	VME2_MASTERCTL_WP	0x40
#define	VME2_MASTERCTL_AM	0x3f
#define	VME2_MASTERCTL_AM24SB	0x3f	/* A24 Supervisory Block Transfer */
#define	VME2_MASTERCTL_AM24SP	0x3e	/* A24 Supervisory Program Access */
#define	VME2_MASTERCTL_AM24SD	0x3d	/* A24 Supervisory Data Access */
#define	VME2_MASTERCTL_AM24UB	0x3b	/* A24 Non-priv. Block Transfer */
#define	VME2_MASTERCTL_AM24UP	0x3a	/* A24 Non-priv. Program Access */
#define	VME2_MASTERCTL_AM24UD	0x39	/* A24 Non-priv. Data Access */
#define	VME2_MASTERCTL_AM16S		0x2d	/* A16 Supervisory Access */
#define	VME2_MASTERCTL_AM16U		0x29	/* A16 Non-priv. Access */
#define	VME2_MASTERCTL_AM32SB	0x0f	/* A32 Supervisory Block Transfer */
#define	VME2_MASTERCTL_AM32SP	0x0e	/* A32 Supervisory Program Access */
#define	VME2_MASTERCTL_AM32SD	0x0d	/* A32 Supervisory Data Access */
#define	VME2_MASTERCTL_AM32UB	0x0b	/* A32 Non-priv. Block Transfer */
#define	VME2_MASTERCTL_AM32UP	0x0a	/* A32 Non-priv. Program Access */
#define	VME2_MASTERCTL_AM32UD	0x09	/* A32 Non-priv Data Access */

#define	VME2_MASTERCTL_ALL	0xff

#define	VME2_GCSRCTL_OFF	0xf0000000
#define	VME2_GCSRCTL_MDEN4	0x00080000
#define	VME2_GCSRCTL_MDEN3	0x00040000
#define	VME2_GCSRCTL_MDEN2	0x00020000
#define	VME2_GCSRCTL_MDEN1	0x00010000
#define	VME2_GCSRCTL_I2EN	0x00008000	/* F decode (A24D16/A32D16) on */
#define	VME2_GCSRCTL_I2WP	0x00004000	/* F decode write post */
#define	VME2_GCSRCTL_I2SU	0x00002000	/* F decode is supervisor */
#define	VME2_GCSRCTL_I2PD	0x00001000	/* F decode is program */
#define	VME2_GCSRCTL_I1EN	0x00000800	/* short decode (A16Dx) on */
#define	VME2_GCSRCTL_I1D16	0x00000400	/* short decode is D16 */
#define	VME2_GCSRCTL_I1WP	0x00000200	/* short decode write post */
#define	VME2_GCSRCTL_I1SU	0x00000100	/* short decode is supervisor */
#define	VME2_GCSRCTL_ROMSIZE	0x000000c0	/* size of ROM */
#define	VME2_GCSRCTL_ROMBSPD	0x00000038	/* speed of ROM */
#define	VME2_GCSRCTL_ROMASPD	0x00000007	/* speed of ROM */

#define	VME2_TCR_1MS		(1 << 8)       	/* Watchdog 1 ms */
#define	VME2_TCR_2MS		(2 << 8)       	/* Watchdog 2 ms */
#define	VME2_TCR_4MS		(3 << 8)       	/* Watchdog 4 ms */
#define	VME2_TCR_8MS		(4 << 8)       	/* Watchdog 8 ms */
#define	VME2_TCR_16MS		(5 << 8)       	/* Watchdog 16 ms */
#define	VME2_TCR_32MS		(6 << 8)       	/* Watchdog 32 ms */
#define	VME2_TCR_64MS		(7 << 8)       	/* Watchdog 64 ms */
#define	VME2_TCR_128MS		(8 << 8)       	/* Watchdog 128 ms */
#define	VME2_TCR_256MS		(9 << 8)       	/* Watchdog 256 ms */
#define	VME2_TCR_512MS		(10 << 8)	/* Watchdog 512 ms */
#define	VME2_TCR_1S		(11 << 8)	/* Watchdog 1 s */
#define	VME2_TCR_4S		(12 << 8)	/* Watchdog 4 s */
#define	VME2_TCR_16S		(13 << 8)	/* Watchdog 16 s */
#define	VME2_TCR_32S		(14 << 8)	/* Watchdog 32 s */
#define	VME2_TCR_64S		(15 << 8)	/* Watchdog 64 s */

#define	VME2_TCTL1_CEN		0x01
#define	VME2_TCTL1_COC		0x02
#define	VME2_TCTL1_COVF		0x04
#define	VME2_TCTL1_OVF		0xf0
#define	VME2_TCTL2_CEN		(0x01 << 8)
#define	VME2_TCTL2_COC		(0x02 << 8)
#define	VME2_TCTL2_COVF		(0x04 << 8)
#define	VME2_TCTL2_OVF		(0xf0 << 8)
#define	VME2_TCTL_WDEN		0x00010000	/* Watchdog Enable */
#define	VME2_TCTL_WDRSE		0x00020000	/* Watchdog Reset Enable */
#define	VME2_TCTL_WDSL		0x00040000	/* local or system reset */
#define	VME2_TCTL_WDBFE		0x00080000	/* Watchdog Board Fail Enable */
#define	VME2_TCTL_WDTO		0x00100000	/* Watchdog Timeout Status */
#define	VME2_TCTL_WDCC		0x00200000	/* Watchdog Clear Counter */
#define	VME2_TCTL_WDCS		0x00400000	/* Watchdog Clear Timeout */
#define	VME2_TCTL_SRST		0x00800000	/* system reset */
#define	VME2_TCTL_RSWE		0x01000000	/* Reset Switch Enable */
#define	VME2_TCTL_BDFLO		0x02000000	/* Assert Board Fail */
#define	VME2_TCTL_CPURS		0x04000000	/* Clear Power-up Reset bit */
#define	VME2_TCTL_PURS		0x08000000	/* Power-up Reset bit */
#define	VME2_TCTL_BDFLI		0x10000000	/* Board Fail Status*/
#define	VME2_TCTL_SYSFAIL	0x20000000	/* light SYSFAIL led */
#define	VME2_TCTL_SCON		0x40000000	/* we are SCON */

#define	VME2_IRQ_ACF		0x80000000
#define	VME2_IRQ_AB		0x40000000
#define	VME2_IRQ_SYSF		0x20000000
#define	VME2_IRQ_MWP		0x10000000
#define	VME2_IRQ_PE		0x08000000
#define	VME2_IRQ_V1IE		0x04000000
#define	VME2_IRQ_TIC2		0x02000000
#define	VME2_IRQ_TIC1		0x01000000
#define	VME2_IRQ_VIA		0x00800000
#define	VME2_IRQ_DMA		0x00400000
#define	VME2_IRQ_SIG3		0x00200000
#define	VME2_IRQ_SIG2		0x00100000
#define	VME2_IRQ_SIG1		0x00080000
#define	VME2_IRQ_SIG0		0x00040000
#define	VME2_IRQ_LM1		0x00020000
#define	VME2_IRQ_LM0		0x00010000
#define	VME2_IRQ_SW7		0x00008000
#define	VME2_IRQ_SW6		0x00004000
#define	VME2_IRQ_SW5		0x00002000
#define	VME2_IRQ_SW4		0x00001000
#define	VME2_IRQ_SW3		0x00000800
#define	VME2_IRQ_SW2		0x00000400
#define	VME2_IRQ_SW1		0x00000200
#define	VME2_IRQ_SW0		0x00000100
#define	VME2_IRQ_SW(x)		((1 << (x))) << 8)
#define	VME2_IRQ_SPARE		0x00000080
#define	VME2_IRQ_VME7		0x00000040
#define	VME2_IRQ_VME6		0x00000020
#define	VME2_IRQ_VME5		0x00000010
#define	VME2_IRQ_VME4		0x00000008
#define	VME2_IRQ_VME3		0x00000004
#define	VME2_IRQ_VME2		0x00000002
#define	VME2_IRQ_VME1		0x00000001
#define	VME2_IRQ_VME(x)		(1 << ((x) - 1))

#define	VME2_IRQL1_ACFSHIFT	28
#define	VME2_IRQL1_ABSHIFT	24
#define	VME2_IRQL1_SYSFSHIFT	20
#define	VME2_IRQL1_WPESHIFT	16
#define	VME2_IRQL1_PESHIFT	12
#define	VME2_IRQL1_V1IESHIFT	8
#define	VME2_IRQL1_TIC2SHIFT	4
#define	VME2_IRQL1_TIC1SHIFT	0

#define	VME2_IRQL2_VIASHIFT	28
#define	VME2_IRQL2_DMASHIFT	24
#define	VME2_IRQL2_SIG3SHIFT	20
#define	VME2_IRQL2_SIG2SHIFT	16
#define	VME2_IRQL2_SIG1SHIFT	12
#define	VME2_IRQL2_SIG0SHIFT	8
#define	VME2_IRQL2_LM1SHIFT	4
#define	VME2_IRQL2_LM0SHIFT	0

#define	VME2_IRQL3_SW7SHIFT	28
#define	VME2_IRQL3_SW6SHIFT	24
#define	VME2_IRQL3_SW5SHIFT	20
#define	VME2_IRQL3_SW4SHIFT	16
#define	VME2_IRQL3_SW3SHIFT	12
#define	VME2_IRQL3_SW2SHIFT	8
#define	VME2_IRQL3_SW1SHIFT	4
#define	VME2_IRQL3_SW0SHIFT	0

#define	VME2_IRQL4_SPARESHIFT	28
#define	VME2_IRQL4_VME7SHIFT	24
#define	VME2_IRQL4_VME6SHIFT	20
#define	VME2_IRQL4_VME5SHIFT	16
#define	VME2_IRQL4_VME4SHIFT	12
#define	VME2_IRQL4_VME3SHIFT	8
#define	VME2_IRQL4_VME2SHIFT	4
#define	VME2_IRQL4_VME1SHIFT	0

#define	VME2_SYSFAIL		(1 << 22)
#define	VME2_IOCTL1_MIEN	(1 << 23)
#define	VME2_VBR_0SHIFT		28
#define	VME2_VBR_1SHIFT		24
#define	VME2_SET_VBR0(x)	((x) << VME2_VBR_0SHIFT)
#define	VME2_SET_VBR1(x)	((x) << VME2_VBR_1SHIFT)
#define	VME2_GET_VBR0(x)	((((x) >> 28) & 0xf) << 4)
#define	VME2_GET_VBR1(x)	((((x) >> 24) & 0xf) << 4)
#define	VME2_VBR_GPOXXXX	0x00ffffff

#define	VME2_MISC_MPIRQEN	0x00000080	/* do not set */
#define	VME2_MISC_REVEROM	0x00000040	/* 167: dis eprom. 166: en flash */
#define	VME2_MISC_DISSRAM	0x00000020	/* do not set */
#define	VME2_MISC_DISMST	0x00000010
#define	VME2_MISC_NOELBBSY	0x00000008	/* do not set */
#define	VME2_MISC_DISBSYT	0x00000004	/* do not set */
#define	VME2_MISC_ENINT		0x00000002	/* do not set */
#define	VME2_MISC_DISBGN	0x00000001	/* do not set */

#define	VME2_A16D32BASE	0xffff0000UL
#define	VME2_A16D32LEN	0x00010000UL
#define	VME2_A32D16BASE	0xf1000000UL
#define	VME2_A32D16LEN	0x01000000UL
#define	VME2_A16D16BASE	0xffff0000UL
#define	VME2_A16D16LEN	0x00010000UL
#define	VME2_A24D16BASE	0xf0000000UL
#define	VME2_A24D16LEN	0x01000000UL
#define	VME2_A16BASE	0xffff0000UL
#define	VME2_A24BASE	0xff000000UL

paddr_t vmepmap(struct device *sc, off_t vmeaddr, int bustype);
int vmerw(struct device *sc, struct uio *uio, int flags, int bus);
int vmeintr_establish(int, struct intrhand *, const char *);
int vme_findvec(int);
int vmescan(struct device *, void *, void *, int);

#endif /* __MVEME88K_DEV_VME_H__ */
