/*	$NetBSD: scireg.h,v 1.3 1994/10/26 02:04:46 cgd Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)scireg.h	7.3 (Berkeley) 2/5/91
 */

/*
 * NCR 5380 SCSI interface hardware description.
 *
 */

#if 0	/* for reference */
typedef struct {
	unsigned char pad0[1];
	volatile unsigned char sci_data;	/* r:  Current data */
#define	sci_odata sci_data			/* w:  Out data */

	unsigned char pad1[1];
	volatile unsigned char sci_icmd;	/* rw: Initiator command */

	unsigned char pad2[1];
	volatile unsigned char sci_mode;	/* rw: Mode */

	unsigned char pad3[1];
	volatile unsigned char sci_tcmd;	/* rw: Target command */

	unsigned char pad4[1];
	volatile unsigned char sci_bus_csr;	/* r:  Bus Status */
#define	sci_sel_enb sci_bus_csr			/* w:  Select enable */

	unsigned char pad5[1];
	volatile unsigned char sci_csr;		/* r:  Status */
#define	sci_dma_send sci_csr			/* w:  Start dma send data */

	unsigned char pad6[1];
	volatile unsigned char sci_idata;	/* r:  Input data */
#define	sci_trecv sci_idata			/* w:  Start dma receive, target */

	unsigned char pad7[1];
	volatile unsigned char sci_iack;	/* r:  Interrupt Acknowledge  */
#define	sci_irecv sci_iack			/* w:  Start dma receive, initiator */
} sci_regmap_t;
#endif

/*
 * Initiator command register
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


/*
 * Mode register
 */

#define SCI_MODE_ARB		0x01		/* rw: Start arbitration */
#define SCI_MODE_DMA		0x02		/* rw: Enable DMA xfers */
#define SCI_MODE_MONBSY		0x04		/* rw: Monitor BSY signal */
#define SCI_MODE_DMA_IE		0x08		/* rw: Enable DMA complete interrupt */
#define SCI_MODE_PERR_IE	0x10		/* rw: Interrupt on parity errors */
#define SCI_MODE_PAR_CHK	0x20		/* rw: Check parity */
#define SCI_MODE_TARGET		0x40		/* rw: Target mode (Initiator if 0) */
#define SCI_MODE_BLOCKDMA	0x80		/* rw: Block-mode DMA handshake (MBZ) */


/*
 * Target command register
 */

#define SCI_TCMD_IO		0x01		/* rw: Assert I/O signal */
#define SCI_TCMD_CD		0x02		/* rw: Assert C/D signal */
#define SCI_TCMD_MSG		0x04		/* rw: Assert MSG signal */
#define SCI_TCMD_PHASE_MASK	0x07		/* r:  Mask for current bus phase */
#define SCI_TCMD_REQ		0x08		/* rw: Assert REQ signal */
#define	SCI_TCMD_LAST_SENT	0x80		/* ro: Last byte was xferred
						 *     (not on 5380/1) */

#define	SCI_PHASE(x)		((x>>2) & 7)

/*
 * Current (SCSI) Bus status
 */

#define SCI_BUS_DBP		0x01		/* r:  Data Bus parity */
#define SCI_BUS_SEL		0x02		/* r:  SEL signal */
#define SCI_BUS_IO		0x04		/* r:  I/O signal */
#define SCI_BUS_CD		0x08		/* r:  C/D signal */
#define SCI_BUS_MSG		0x10		/* r:  MSG signal */
#define SCI_BUS_REQ		0x20		/* r:  REQ signal */
#define SCI_BUS_BSY		0x40		/* r:  BSY signal */
#define SCI_BUS_RST		0x80		/* r:  RST signal */

#define	SCI_CUR_PHASE(x)	SCSI_PHASE((x)>>2)

/*
 * Bus and Status register
 */

#define SCI_CSR_ACK		0x01		/* r:  ACK signal */
#define SCI_CSR_ATN		0x02		/* r:  ATN signal */
#define SCI_CSR_DISC		0x04		/* r:  Disconnected (BSY==0) */
#define SCI_CSR_PHASE_MATCH	0x08		/* r:  Bus and SCI_TCMD match */
#define SCI_CSR_INT		0x10		/* r:  Interrupt request */
#define SCI_CSR_PERR		0x20		/* r:  Parity error */
#define SCI_CSR_DREQ		0x40		/* r:  DMA request */
#define SCI_CSR_DONE		0x80		/* r:  DMA count is zero */

