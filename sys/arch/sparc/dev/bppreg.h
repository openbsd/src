/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
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
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * based on work by Stephen Williams, Gus Baldauf, and Peter Zaitcev
 */

/*
 * BPP Register Block
 */
struct bppregs {

	volatile u_int32_t	bpp_csr;
	volatile u_int32_t	bpp_addr;
	volatile u_int32_t	bpp_bcnt;
	volatile u_int32_t	bpp_tst_csr;

	volatile u_int16_t	bpp_hcr;
	volatile u_int16_t	bpp_ocr;
	volatile u_int8_t	bpp_dr;
	volatile u_int8_t	bpp_tcr;
	volatile u_int8_t	bpp_or;
	volatile u_int8_t	bpp_ir;
	volatile u_int16_t	bpp_icr;
};


#define BPP_DEV_ID_MASK		0xf0000000
#define BPP_DEV_ID_ZEBRA	0x40000000
#define BPP_DEV_ID_L64854	0xa0000000
#define BPP_NA_LOADED		0x08000000
#define BPP_A_LOADED		0x04000000
#define BPP_DMA_ON		0x02000000
#define BPP_EN_NEXT		0x01000000
#define BPP_TCI_DIS		0x00800000
#define BPP_DIAG		0x00100000

#define BPP_BURST_SIZE		0x000c0000
#define BPP_BURST_8		0x00000000
#define BPP_BURST_4		0x00040000
#define BPP_BURST_1		0x00080000
#define BPP_TC			0x00004000

#define BPP_EN_CNT		0x00002000
#define BPP_EN_DMA		0x00000200
#define BPP_WRITE		0x00000100
#define BPP_RESET		0x00000080
#define BPP_SLAVE_ERR		0x00000040
#define BPP_INVALIDATE		0x00000020
#define BPP_INT_EN		0x00000010
#define BPP_DRAINING		0x0000000c

#define BPP_ERR_PEND		0x00000002
#define BPP_INT_PEND		0x00000001


#define BPP_HCR_TEST		0x8000
#define BPP_HCR_DSW		0x7f00
#define BPP_HCR_DDS		0x007f


#define BPP_OCR_MEM_CLR		0x8000
#define BPP_OCR_DATA_SRC	0x4000
#define BPP_OCR_DS_DSEL		0x2000
#define BPP_OCR_BUSY_DSEL	0x1000
#define BPP_OCR_ACK_DSEL	0x0800
#define BPP_OCR_EN_DIAG		0x0400
#define BPP_OCR_BUSY_OP		0x0200
#define BPP_OCR_ACK_OP		0x0100
#define BPP_OCR_SRST		0x0080
#define BPP_OCR_IDLE		0x0008
#define BPP_OCR_V_ILCK		0x0002
#define BPP_OCR_EN_VER		0x0001


#define BPP_TCR_DIR		0x08
#define BPP_TCR_BUSY		0x04
#define BPP_TCR_ACK		0x02
#define BPP_TCR_DS		0x01


#define BPP_OR_V3		0x20
#define BPP_OR_V2		0x10
#define BPP_OR_V1		0x08
#define BPP_OR_INIT		0x04
#define BPP_OR_AFXN		0x02
#define BPP_OR_SLCT_IN		0x01


#define BPP_IR_PE		0x04
#define BPP_IR_SLCT		0x02
#define BPP_IR_ERR		0x01


#define BPP_DS_IRQ		0x8000
#define BPP_ACK_IRQ		0x4000
#define BPP_BUSY_IRQ		0x2000
#define BPP_PE_IRQ		0x1000
#define BPP_SLCT_IRQ		0x0800
#define BPP_ERR_IRQ		0x0400
#define BPP_DS_IRQ_EN		0x0200
#define BPP_ACK_IRQ_EN		0x0100
#define BPP_BUSY_IRP		0x0080
#define BPP_BUSY_IRQ_EN		0x0040
#define BPP_PE_IRP		0x0020
#define BPP_PE_IRQ_EN		0x0010
#define BPP_SLCT_IRP		0x0008
#define BPP_SLCT_IRQ_EN		0x0004
#define BPP_ERR_IRP		0x0002
#define BPP_ERR_IRQ_EN		0x0001
