/*	$NetBSD: ncr_sireg.h,v 1.2 1995/11/17 23:27:55 gwr Exp $	*/

/*
 * Register map for the Sun3 SCSI Interface (si)
 * The first part of this register map is an NCR5380
 * SCSI Bus Interface Controller (SBIC).  The rest is a
 * DMA controller and custom logic in one of two flavors,
 * one for the OBIO interface (3/50,3/60) and one for the
 * VME interface (3/160,3/260,etc.), where some registers
 * are implemented only on one or the other, some on both.
 */

/*
 * Some of these registers apply to only one interface and some
 * apply to both. The registers which apply to the Sun3/50 onboard 
 * version only are udc_rdata and udc_raddr. The registers which
 * apply to the Sun3 vme version only are dma_addr, dma_count, bpr,
 * iv_am, and bcrh. Thus, the sbc registers, fifo_data, bcr, and csr 
 * apply to both interfaces.
 * One other feature of the vme interface: a write to the dma count 
 * register also causes a write to the fifo byte count register and
 * vis versa.
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
	u_short			dma_addrh;	/* dma address (VME only) */
	u_short			dma_addrl;	/* (high word, low word)  */
	u_short			dma_counth;	/* dma count   (VME only) */
	u_short			dma_countl;	/* (high word, low word)  */

	/* AMD 9516 regs (OBIO only) see am9516.h */
	u_short			udc_data;	/* Am9516, reg data (OBIO only) */
	u_short			udc_addr;	/* Am9516, reg addr (OBIO only) */

	/* These three registers are on both OBIO and VME versions. */
	u_short			fifo_data;	/* fifo data register */
						/* holds extra byte on odd */
						/* byte dma read */
	u_short			fifo_count;		/* fifo byte count */
	u_short			si_csr;		/* control/status register */

	/* The rest of these are on the VME interface only: */
	u_short			si_bprh;		/* byte pack, high (VME only) */
	u_short			si_bprl;		/* byte pack, low  (VME only) */
	u_short			si_iv_am;		/* bits 0-7: intr vector */
						/* bits 8-13: addr modifier (VME only) */
						/* bits 14-15: unused */
	u_short			fifo_cnt_hi;	/* high part of fifo_count (VME only) */

	/* Whole thing repeats after 32 bytes. */
	u_short			_space[3];
};

/* possible values for the address modifier, sun3 vme version only */
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

