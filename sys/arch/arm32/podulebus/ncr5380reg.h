/*	$NetBSD: ncr5380reg.h,v 1.1 1996/01/31 23:26:04 mark Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY (mach3)
 * Revision 2.3  91/08/24  12:25:10  af
 * 	Moved padding of regmap in impl file.
 * 	[91/08/02  04:22:39  af]
 * 
 * Revision 2.2  91/06/19  16:28:35  rvb
 * 	From the NCR data sheets
 * 	"NCR 5380 Family, SCSI Protocol Controller Data Manual"
 * 	NCR Microelectronics Division, Colorado Spring, 6/98 T01891L
 * 	[91/04/21            af]
 * 
 */

/*
 *	File: scsi_5380.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	5/91
 *
 *	Defines for the NCR 5380 (SCSI chip), aka Am5380
 */

/*
 * Register map:  Note not declared here anymore!
 * All the 5380 registers are accessed through individual
 * pointers initialized by MD code.  This allows the 5380
 * MI functions to be shared between MD drivers that have
 * different padding between the registers (i.e. amiga).
 */
#if 0	/* example only */
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
#endif

/*
 * Machine-independent code uses these names:
 */
#define	sci_data	sci_r0	/* r:  Current data */
#define	sci_odata	sci_r0	/* w:  Out data */

#define	sci_icmd	sci_r1	/* rw: Initiator command */
#define	sci_mode	sci_r2	/* rw: Mode */
#define	sci_tcmd	sci_r3	/* rw: Target command */

#define	sci_bus_csr	sci_r4	/* r:  Bus Status */
#define sci_sel_enb sci_r4	/* w:  Select enable */

#define	sci_csr 	 sci_r5	/* r:  Status */
#define sci_dma_send sci_r5	/* w:  Start dma send data */

#define	sci_idata	sci_r6	/* r:  Input data */
#define	sci_trecv	sci_r6	/* w:  Start dma receive, target */

#define	sci_iack	sci_r7	/* r:  Interrupt Acknowledge  */
#define sci_irecv	sci_r7	/* w:  Start dma receive, initiator */


/*
 * R1: Initiator command register
 */
#define SCI_ICMD_DATA		0x01		/* rw: Assert data bus   */
#define SCI_ICMD_ATN		0x02		/* rw: Assert ATN signal */
#define SCI_ICMD_SEL		0x04		/* rw: Assert SEL signal */
#define SCI_ICMD_BSY		0x08		/* rw: Assert BSY signal */
#define SCI_ICMD_ACK		0x10		/* rw: Assert ACK signal */
#define SCI_ICMD_LST		0x20		/* r:  Lost arbitration */
#define SCI_ICMD_DIFF	SCI_ICMD_LST		/* w:  Differential cable */
#define SCI_ICMD_AIP		0x40		/* r:  Arbitration in progress */
#define SCI_ICMD_TEST	SCI_ICMD_AIP		/* w:  Test mode */
#define SCI_ICMD_RST		0x80		/* rw: Assert RST signal */
/* Bits to keep when doing read/modify/write (leave out RST) */
#define SCI_ICMD_RMASK		0x1F


/*
 * R2: Mode register
 */
#define SCI_MODE_ARB		0x01		/* rw: Start arbitration */
#define SCI_MODE_DMA		0x02		/* rw: Enable DMA xfers */
#define SCI_MODE_MONBSY		0x04		/* rw: Monitor BSY signal */
#define SCI_MODE_DMA_IE		0x08		/* rw: Enable DMA complete interrupt */
#define SCI_MODE_PERR_IE	0x10		/* rw: Interrupt on parity errors */
#define SCI_MODE_PAR_CHK	0x20		/* rw: Check parity */
#define SCI_MODE_TARGET		0x40		/* rw: Target mode (Initiator if 0) */
#define SCI_MODE_BLOCKDMA	0x80		/* rw: Block-mode DMA handshake */


/*
 * R3: Target command register
 */
#define SCI_TCMD_IO		0x01		/* rw: Assert I/O signal */
#define SCI_TCMD_CD		0x02		/* rw: Assert C/D signal */
#define SCI_TCMD_MSG		0x04		/* rw: Assert MSG signal */
#define SCI_TCMD_PHASE_MASK	0x07		/* r:  Mask for current bus phase */
#define SCI_TCMD_REQ		0x08		/* rw: Assert REQ signal */
#define	SCI_TCMD_LAST_SENT	0x80		/* ro: Last byte was xferred
						 *     (not on 5380/1) */

#define	SCI_TCMD_PHASE(x)		((x) & 0x7)

/*
 * R4: Current (SCSI) Bus status (.sci_bus_csr)
 */
#define SCI_BUS_DBP		0x01		/* r:  Data Bus parity */
#define SCI_BUS_SEL		0x02		/* r:  SEL signal */
#define SCI_BUS_IO		0x04		/* r:  I/O signal */
#define SCI_BUS_CD		0x08		/* r:  C/D signal */
#define SCI_BUS_MSG		0x10		/* r:  MSG signal */
#define SCI_BUS_REQ		0x20		/* r:  REQ signal */
#define SCI_BUS_BSY		0x40		/* r:  BSY signal */
#define SCI_BUS_RST		0x80		/* r:  RST signal */

#define	SCI_BUS_PHASE(x)	(((x) >> 2) & 7)

/*
 * R5: Bus and Status register (.sci_csr)
 */
#define SCI_CSR_ACK		0x01		/* r:  ACK signal */
#define SCI_CSR_ATN		0x02		/* r:  ATN signal */
#define SCI_CSR_DISC		0x04		/* r:  Disconnected (BSY==0) */
#define SCI_CSR_PHASE_MATCH	0x08		/* r:  Bus and SCI_TCMD match */
#define SCI_CSR_INT		0x10		/* r:  Interrupt request */
#define SCI_CSR_PERR		0x20		/* r:  Parity error */
#define SCI_CSR_DREQ		0x40		/* r:  DMA request */
#define SCI_CSR_DONE		0x80		/* r:  DMA count is zero */
