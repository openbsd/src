/*	$OpenBSD: meshreg.h,v 1.1 2001/09/01 15:50:00 drahn Exp $	*/
/*	$NetBSD: meshreg.h,v 1.1 1999/02/19 13:06:03 tsubai Exp $	*/

/*-
 * Copyright (C) 1999	Internet Research Institute, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* MESH register offsets */
#define MESH_XFER_COUNT0	0x00	/* transfer count (low)  */
#define MESH_XFER_COUNT1	0x10	/* transfer count (high) */
#define MESH_FIFO		0x20	/* FIFO (16byte depth) */
#define MESH_SEQUENCE		0x30	/* command register */
#define MESH_BUS_STATUS0	0x40
#define MESH_BUS_STATUS1	0x50
#define MESH_FIFO_COUNT		0x60
#define MESH_EXCEPTION		0x70
#define MESH_ERROR		0x80
#define MESH_INTR_MASK		0x90
#define MESH_INTERRUPT		0xa0
#define MESH_SOURCE_ID		0xb0
#define MESH_DEST_ID		0xc0
#define MESH_SYNC_PARAM		0xd0
#define MESH_MESH_ID		0xe0	/* MESH version */
#define MESH_SEL_TIMEOUT	0xf0	/* selection timeout delay */

#define MESH_SIGNATURE		0xe2	/* XXX wrong! */

/* MESH commands */
#define MESH_CMD_ARBITRATE	0x01
#define MESH_CMD_SELECT		0x02
#define MESH_CMD_COMMAND	0x03
#define MESH_CMD_STATUS		0x04
#define MESH_CMD_DATAOUT	0x05
#define MESH_CMD_DATAIN		0x06
#define MESH_CMD_MSGOUT		0x07
#define MESH_CMD_MSGIN		0x08
#define MESH_CMD_BUSFREE	0x09
#define MESH_CMD_ENABLE_PARITY	0x0A
#define MESH_CMD_DISABLE_PARITY	0x0B
#define MESH_CMD_ENABLE_RESEL	0x0C
#define MESH_CMD_DISABLE_RESEL	0x0D
#define MESH_CMD_RESET_MESH	0x0E
#define MESH_CMD_FLUSH_FIFO	0x0F
#define MESH_SEQ_DMA		0x80
#define MESH_SEQ_TARGET		0x40
#define MESH_SEQ_ATN		0x20
#define MESH_SEQ_ACTNEG		0x10

/* INTERRUPT/INTR_MASK register bits */
#define MESH_INTR_ERROR		0x04
#define MESH_INTR_EXCEPTION	0x02
#define MESH_INTR_CMDDONE	0x01

/* EXCEPTION register bits */
#define MESH_EXC_SELATN		0x20	/* selected and ATN asserted (T) */
#define MESH_EXC_SELECTED	0x10	/* selected (T) */
#define MESH_EXC_RESEL		0x08	/* reselected */
#define MESH_EXC_ARBLOST	0x04	/* arbitration lost */
#define MESH_EXC_PHASEMM	0x02	/* phase mismatch */
#define MESH_EXC_SELTO		0x01	/* selection timeout */

/* ERROR register bits */
#define MESH_ERR_DISCONNECT	0x40	/* unexpected disconnect */
#define MESH_ERR_SCSI_RESET	0x20	/* Rst signal asserted */
#define MESH_ERR_SEQERR		0x10	/* sequence error */
#define MESH_ERR_PARITY_ERR3	0x08	/* parity error */
#define MESH_ERR_PARITY_ERR2	0x04
#define MESH_ERR_PARITY_ERR1	0x02
#define MESH_ERR_PARITY_ERR0	0x01

/* BUS_STATUS0 status bits */
#define MESH_STATUS0_REQ32	0x80
#define MESH_STATUS0_ACK32	0x40
#define MESH_STATUS0_REQ	0x20
#define MESH_STATUS0_ACK	0x10
#define MESH_STATUS0_ATN	0x08
#define MESH_STATUS0_MSG	0x04
#define MESH_STATUS0_CD		0x02
#define MESH_STATUS0_IO		0x01

/* BUS_STATUS1 status bits */
#define MESH_STATUS1_RST	0x80
#define MESH_STATUS1_BSY	0x40
#define MESH_STATUS1_SEL	0x20
