/*	$NetBSD: sireg.h,v 1.3 1996/01/01 22:40:58 thorpej Exp $	*/

/*
 * Register map for the Sun3 SCSI Interface (si)
 * The first part of this register map is an NCR5380
 * SCSI Bus Interface Controller (SBIC).  The rest is a
 * DMA controller and custom logic in one of two flavors,
 * one for the OBIO interface (3/50,3/60) and one for the
 * VME interface (3/160,3/260,etc.), where some registers
 * are implemented only on one or the other, some on both.
 *
 * Modified for Sun 4 systems by Jason R. Thorpe <thorpej@NetBSD.ORG>.
 */

/*
 * Note that the obio version on the 4/1xx (the so-called "SCSI Weird", or
 * "sw" controller) is laid out a bit differently, and hence the evilness
 * with unions.  Also, the "sw" doesn't appear to have a FIFO.
 */

/*
 * Am5380 Register map (no padding)
 */
struct ncr5380regs {
	volatile u_char sci_r0;
	volatile u_char sci_r1;
	volatile u_char sci_r2;
	volatile u_char sci_r3;
	volatile u_char sci_r4;
	volatile u_char sci_r5;
	volatile u_char sci_r6;
	volatile u_char sci_r7;
};

struct si_regs {
	struct ncr5380regs sci;

	/* DMA controller registers */
	union {
		struct {
			u_short	_Dma_addrh;	/* dma address (VME only) */
			u_short	_Dma_addrl;	/* (high word, low word)  */
		} _si_u1_s;
		u_int		_Dma_addr;	/* dma address (OBIO) */
	} _si_u1;
#define dma_addrh	_si_u1._si_u1_s._Dma_addrh
#define dma_addrl	_si_u1._si_u1_s._Dma_addrl
#define dma_addr	_si_u1._Dma_addr

	union {
		struct {
			u_short	_Dma_counth;	/* dma count   (VME only) */
			u_short	_Dma_countl;	/* (high word, low word)  */
		} _si_u2_s;
		u_int		_Dma_count;	/* dma count (OBIO) */
	} _si_u2;
#define dma_counth	_si_u2._si_u2_s._Dma_counth
#define dma_countl	_si_u2._si_u2_s._Dma_countl
#define dma_count	_si_u2._Dma_count

	u_int		si_pad0;		/* no-existent register */

	union {
		struct {
			u_short	_Fifo_data;	/* fifo data register */
			u_short	_Fifo_count;	/* fifo count register */
		} _si_u4_s;
		u_int		_Sw_csr;	/* sw control/status */
	} _si_u4;
#define fifo_data	_si_u4._si_u4_s._Fifo_data
#define fifo_count	_si_u4._si_u4_s._Fifo_count
#define sw_csr		_si_u4._Sw_csr

	union {
		struct {
			u_short	_Si_csr;	/* si control/status */
			u_short	_Bprh;		/* VME byte pack high */
		} _si_u5_s;
		u_int	_Bpr;			/* sw byte pack */
	} _si_u5;
#define si_csr		_si_u5._si_u5_s._Si_csr
#define si_bprh		_si_u5._si_u5_s._Bprh
#define sw_bpr		_si_u5._Bpr

	/* The rest of these are on the VME interface only: */
	u_short			si_bprl;	/* VME byte pack low */
	u_short			si_iv_am;	/* bits 0-7: intr vector */
				/* bits 8-13: addr modifier (VME only) */
						/* bits 14-15: unused */
	u_short			fifo_cnt_hi;	/* high part of fifo_count (VME only) */

	/* Whole thing repeats after 32 bytes. */
	u_short			_space[3];
};

/* possible values for the address modifier, vme version only */
#define VME_SUPV_DATA_24	0x3d00

/*
 * Status Register.
 * Note:
 *	(r)	indicates bit is read only.
 *	(rw)	indicates bit is read or write.
 *	(v)	vme host adaptor interface only.
 *	(o)	sun3/50 onboard host adaptor interface only.
 *	(b)	both vme and sun3/50 host adaptor interfaces.
 */
#define SI_CSR_DMA_ACTIVE	0x8000	/* (r,o) dma transfer active */
#define SI_CSR_DMA_CONFLICT	0x4000	/* (r,b) reg accessed while dmaing */
#define SI_CSR_DMA_BUS_ERR	0x2000	/* (r,b) bus error during dma */
#define SI_CSR_ID		0x1000	/* (r,b) 0 for 3/50, 1 for SCSI-3, */
					/* 0 if SCSI-3 unmodified */
#define SI_CSR_FIFO_FULL	0x0800	/* (r,b) fifo full */
#define SI_CSR_FIFO_EMPTY	0x0400	/* (r,b) fifo empty */
#define SI_CSR_SBC_IP		0x0200	/* (r,b) sbc interrupt pending */
#define SI_CSR_DMA_IP		0x0100	/* (r,b) dma interrupt pending */
#define SI_CSR_LOB		0x00c0	/* (r,v) number of leftover bytes */
#define SI_CSR_LOB_THREE	0x00c0	/* (r,v) three leftover bytes */
#define SI_CSR_LOB_TWO		0x0080	/* (r,v) two leftover bytes */
#define SI_CSR_LOB_ONE		0x0040	/* (r,v) one leftover byte */
#define SI_CSR_BPCON		0x0020	/* (rw,v) byte packing control */
					/* dma is in 0=longwords, 1=words */
#define SI_CSR_DMA_EN		0x0010	/* (rw,v) dma/interrupt enable */
#define SI_CSR_SEND		0x0008	/* (rw,b) dma dir, 1=to device */
#define SI_CSR_INTR_EN		0x0004	/* (rw,b) interrupts enable */
#define SI_CSR_FIFO_RES		0x0002	/* (rw,b) inits fifo, 0=reset */
#define SI_CSR_SCSI_RES		0x0001	/* (rw,b) reset sbc and udc, 0=reset */
