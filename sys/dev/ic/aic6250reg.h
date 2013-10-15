/*	$OpenBSD: aic6250reg.h,v 1.1 2013/10/15 01:41:46 miod Exp $	*/

/*
 * Copyright (c) 2010, 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Register definitions for the AIC 6250 SCSI controller
 */

#define	AIC_DMA_BYTE_COUNT_L		0x00
#define	AIC_DMA_BYTE_COUNT_M		0x01	
#define	AIC_DMA_BYTE_COUNT_H		0x02
#define	AIC_INT_MSK_REG0		0x03	/* w-only */
#define	AIC_OFFSET_CNTRL		0x04	/* w-only */
#define	AIC_FIFO_STATUS			0x05	/* r-only */
#define	AIC_DMA_CNTRL			0x05	/* w-only */
#define	AIC_REV_CNTRL			0x06	/* r-only */
#define	AIC_INT_MSK_REG1		0x06	/* w-only */
#define	AIC_STATUS_REG0			0x07	/* r-only */
#define	AIC_CONTROL_REG0		0x07	/* w-only */
#define	AIC_STATUS_REG1			0x08	/* r-only */
#define	AIC_CONTROL_REG1		0x08	/* w-only */
#define	AIC_SCSI_SIGNAL_REG		0x09
#define	AIC_SCSI_ID_DATA		0x0a
#define	AIC_SOURCE_DEST_ID		0x0b	/* r-only */
#define	AIC_MEMORY_DATA			0x0c
#define	AIC_PORT_A			0x0d
#define	AIC_PORT_B			0x0e
#define	AIC_SCSI_LATCH_DATA		0x0f	/* r-only */
#define	AIC_SCSI_BSY_RST		0x0f	/* w-only */

#define	AIC_NREG			0x10

/* INT MSK REG0 */
#define	AIC_IMR_ARB_SEL_START		0x40
#define	AIC_IMR_EN_AUTO_ATN		0x20
#define	AIC_IMR_EN_ERROR_INT		0x10
#define	AIC_IMR_EN_CMD_DONE_INT		0x08
#define	AIC_IMR_EN_SEL_OUT_INT		0x04
#define	AIC_IMR_EN_RESEL_INT		0x02
#define	AIC_IMR_EN_SELECT_INT		0x01

/* OFFSET CNTRL */
#define	AIC_OC_SYNC_XFER_MASK		0x70
#define	AIC_OC_SYNC_XFER_SHIFT		4
#define	AIC_OC_OFFSET_MASK		0x0f
#define	AIC_OC_OFFSET_SHIFT		0

/* FIFO STATUS */
#define	AIC_FS_OFFSET_COUNT_ZERO	0x20
#define	AIC_FS_FIFO_EMPTY		0x10
#define	AIC_FS_FIFO_FULL		0x08
#define	AIC_FS_FIFO_COUNTER_MASK	0x07
#define	AIC_FS_FIFO_COUNTER_SHIFT	0

/* DMA CNTRL */
#define	AIC_DC_ODD_XFER_START		0x04
#define	AIC_DC_TRANSFER_DIR		0x02
#define	AIC_DC_DMA_XFER_EN		0x01

/* REV CNTRL */
#define	AIC_RC_MASK			0x03

/* INT MSK REG1 */
#define	AIC_IMR1_EN_SCSI_REQ_ON_INT	0x40
#define	AIC_IMR1_EN_SCSI_RST_INT	0x20
#define	AIC_IMR1_EN_MEM_PARITY_ERR_INT	0x10
#define	AIC_IMR1_EN_PHASE_MISMATCH_INT	0x08
#define	AIC_IMR1_EN_BUS_FREE_DETECT_INT	0x04
#define	AIC_IMR1_EN_SCSI_PARITY_ERR_INT	0x02
#define	AIC_IMR1_EN_PHASE_CHANGE_INT	0x01	/* initiator */
#define	AIC_IMR1_EN_ATTN_ON_INT		0x01	/* target */

/* STATUS REG0 */
#define	AIC_SR0_SCSI_RST_OCCURED	0x80
#define	AIC_SR0_MEMORY_PARITY_ERR	0x40
#define	AIC_SR0_PHASE_MISMATCH_ERR	0x20
#define	AIC_SR0_BUS_FREE_DETECT		0x10
#define	AIC_SR0_SCSI_PARITY_ERR		0x08
#define	AIC_SR0_SCSI_REQ_ON		0x04
#define	AIC_SR0_SCSI_PHASE_CHG_ATTN	0x02
#define	AIC_SR0_DMA_BYTE_CNT_ZERO	0x01

/* CONTROL REG0 */
#define	AIC_CR0_P_MEM_CYCLE_REQ		0x80
#define	AIC_CR0_P_MEM_RW		0x40
#define	AIC_CR0_TARGET_MODE		0x20
#define	AIC_CR0_EN_PORT_A		0x10
#define	AIC_CR0_SCSI_INTERFACE_MODE	0x08
#define	AIC_CR0_SCSI_ID_MASK		0x07

/* STATUS REG1 */
#define	AIC_SR1_MEM_CYCLE_COMPLETE	0x80
#define	AIC_SR1_SCSI_RST_IN		0x20
#define	AIC_SR1_ERROR			0x10
#define	AIC_SR1_CMD_DONE		0x08
#define	AIC_SR1_SEL_OUT			0x04
#define	AIC_SR1_RESELECTED		0x02
#define	AIC_SR1_SELECTED		0x01

/* CONTROL REG1 */
#define	AIC_CR1_AUTO_SCSI_PIO_REQ	0x80
#define	AIC_CR1_ENABLE_16BIT_MEM_BUS	0x40
#define	AIC_CR1_EN_PORT_B		0x10
#define	AIC_CR1_PHASE_CHANGE_MODE	0x08
#define	AIC_CR1_CLK_FREQ_MODE		0x04
#define	AIC_CR1_SCSI_RST_OUT		0x02
#define	AIC_CR1_CHIP_SW_RESET		0x01

/* SCSI SIGNAL REG (read path) */
#define	AIC_SS_CD_IN			0x80
#define	AIC_SS_IO_IN			0x40
#define	AIC_SS_MSG_IN			0x20
#define	AIC_SS_ATN_IN			0x10
#define	AIC_SS_SEL_IN			0x08
#define	AIC_SS_BSY_IN			0x04
#define	AIC_SS_REQ_IN			0x02
#define	AIC_SS_ACK_IN			0x01

/* SCSI SIGNAL REG (write path) */
#define	AIC_SS_CD_OUT			0x80
#define	AIC_SS_IO_OUT			0x40
#define	AIC_SS_MSG_OUT			0x20
#define	AIC_SS_ATN_OUT			0x10
#define	AIC_SS_SEL_OUT			0x08
#define	AIC_SS_BSY_OUT			0x04
#define	AIC_SS_ACK_OUT			0x02	/* initiator */
#define	AIC_SS_REQ_OUT			0x02	/* target */

#define	PH_DATAOUT		0x00
#define	PH_DATAIN		AIC_SS_IO_IN
#define	PH_CMD			AIC_SS_CD_IN
#define	PH_STAT			(AIC_SS_CD_IN | AIC_SS_IO_IN)
#define	PH_MSGOUT		(AIC_SS_CD_OUT | AIC_SS_MSG_OUT)
#define	PH_MSGIN		(AIC_SS_CD_IN | AIC_SS_IO_IN | AIC_SS_MSG_IN)
#define	PH_MASK			(AIC_SS_CD_IN | AIC_SS_IO_IN | AIC_SS_MSG_IN)
#define	PH_INVALID		0xff
