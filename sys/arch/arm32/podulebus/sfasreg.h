/* $NetBSD: sfasreg.h,v 1.1 1996/01/31 23:26:45 mark Exp $ */

/*
 * Copyright (c) 1995 Daniel Widenfalk
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
 *      This product includes software developed by Daniel Widenfalk
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _SFASREG_H_
#define _SFASREG_H_

/*
 * Emulex FAS216 SCSI interface hardware description.
 */

typedef volatile unsigned char vu_char;

typedef struct {
	vu_char		*sfas_tc_low;	/* rw: Transfer count low */
	vu_char		*sfas_tc_mid;	/* rw: Transfer count mid */
	vu_char		*sfas_fifo;	/* rw: Data FIFO */
	vu_char		*sfas_command;	/* rw: Chip command reg */
	vu_char		*sfas_dest_id;	/* w: (Re)select bus ID */
#define sfas_status sfas_dest_id	/* r: Status */
	vu_char		*sfas_timeout;	/* w: (Re)select timeout */
#define sfas_interrupt sfas_timeout	/* r: Interrupt */
	vu_char		*sfas_syncper;	/* w: Synch. transfer period */
#define sfas_seqstep sfas_syncper	/* r: Sequence step */
	vu_char		*sfas_syncoff;	/* w: Synch. transfer offset */
#define sfas_fifo_flags sfas_syncoff	/* r: FIFO flags */
	vu_char		*sfas_config1;	/* rw: Config register #1 */
	vu_char		*sfas_clkconv;	/* w: Clock conv. factor */
	vu_char		*sfas_test;	/* w: Test register */
	vu_char		*sfas_config2;	/* rw: Config register #2 */
	vu_char		*sfas_config3;	/* rw: Config register #3 */
	vu_char		*sfas_tc_high;	/* rw: Transfer count high */
	vu_char		*sfas_fifo_bot;	/* w: FIFO bottom register */
} sfas_regmap_t;
typedef sfas_regmap_t *sfas_regmap_p;

/* Commands for the FAS216 */
#define SFAS_CMD_DMA			0x80

#define SFAS_CMD_SEL_NO_ATN		0x41
#define SFAS_CMD_SEL_ATN		0x42
#define SFAS_CMD_SEL_ATN3		0x46
#define SFAS_CMD_SEL_ATN_STOP		0x43

#define SFAS_CMD_ENABLE_RESEL		0x44
#define SFAS_CMD_DISABLE_RESEL		0x45

#define SFAS_CMD_TRANSFER_INFO		0x10
#define SFAS_CMD_TRANSFER_PAD		0x98

#define SFAS_CMD_COMMAND_COMPLETE	0x11
#define SFAS_CMD_MESSAGE_ACCEPTED	0x12

#define SFAS_CMD_SET_ATN		0x1A
#define SFAS_CMD_RESET_ATN		0x1B

#define SFAS_CMD_NOP			0x00
#define SFAS_CMD_FLUSH_FIFO		0x01
#define SFAS_CMD_RESET_CHIP		0x02
#define SFAS_CMD_RESET_SCSI_BUS		0x03

#define SFAS_STAT_PHASE_MASK		0x07
#define SFAS_STAT_PHASE_TRANS_CPLT	0x08
#define SFAS_STAT_TRANSFER_COUNT_ZERO	0x10
#define SFAS_STAT_PARITY_ERROR		0x20
#define SFAS_STAT_GROSS_ERROR		0x40
#define SFAS_STAT_INTERRUPT_PENDING	0x80

#define SFAS_PHASE_DATA_OUT		0
#define SFAS_PHASE_DATA_IN		1
#define SFAS_PHASE_COMMAND		2
#define SFAS_PHASE_STATUS		3
#define SFAS_PHASE_MESSAGE_OUT		6
#define SFAS_PHASE_MESSAGE_IN		7

#define SFAS_DEST_ID_MASK		0x07

#define SFAS_INT_SELECTED		0x01
#define SFAS_INT_SELECTED_WITH_ATN	0x02
#define SFAS_INT_RESELECTED		0x04
#define SFAS_INT_FUNCTION_COMPLETE	0x08
#define SFAS_INT_BUS_SERVICE		0x10
#define SFAS_INT_DISCONNECT		0x20
#define SFAS_INT_ILLEGAL_COMMAND	0x40
#define SFAS_INT_SCSI_RESET_DETECTED	0x80

#define SFAS_SYNCHRON_PERIOD_MASK	0x1F

#define SFAS_FIFO_COUNT_MASK		0x1F
#define SFAS_FIFO_SEQUENCE_STEP_MASK	0xE0
#define SFAS_FIFO_SEQUENCE_SHIFT	5

#define SFAS_SYNCHRON_OFFSET_MASK	0x0F
#define SFAS_SYNC_ASSERT_MASK		0x30
#define SFAS_SYNC_ASSERT_SHIFT		4
#define SFAS_SYNC_DEASSERT_MASK		0x30
#define SFAS_SYNC_DEASSERT_SHIFT	6

#define SFAS_CFG1_BUS_ID_MASK		0x07
#define SFAS_CFG1_CHIP_TEST_MODE	0x08
#define SFAS_CFG1_SCSI_PARITY_ENABLE	0x10
#define SFAS_CFG1_PARITY_TEST_MODE	0x20
#define SFAS_CFG1_SCSI_RES_INT_DIS	0x40
#define SFAS_CFG1_SLOW_CABLE_MODE	0x80

#define SFAS_CLOCK_CONVERSION_MASK	0x07

#define SFAS_TEST_TARGET_TEST_MODE	0x01
#define SFAS_TEST_INITIATOR_TEST_MODE	0x02
#define SFAS_TEST_TRISTATE_TEST_MODE	0x04

#define SFAS_CFG2_DMA_PARITY_ENABLE	0x01
#define SFAS_CFG2_REG_PARITY_ENABLE	0x02
#define SFAS_CFG2_TARG_BAD_PARITY_ABORT	0x04
#define SFAS_CFG2_SCSI_2_MODE		0x08
#define SFAS_CFG2_TRISTATE_DMA_REQ	0x10
#define SFAS_CFG2_BYTE_CONTROL_MODE	0x20
#define SFAS_CFG2_FEATURES_ENABLE	0x40
#define SFAS_CFG2_RESERVE_FIFO_BYTE	0x80

#define SFAS_CFG3_THRESHOLD_8_MODE	0x01
#define SFAS_CFG3_ALTERNATE_DMA_MODE	0x02
#define SFAS_CFG3_SAVE_RESIDUAL_BYTE	0x04
#define SFAS_CFG3_FASTCLK		0x08
#define SFAS_CFG3_FASTSCSI		0x10
#define SFAS_CFG3_CDB10			0x20
#define SFAS_CFG3_QENB			0x40
#define SFAS_CFG3_IDRESCHK		0x80

#endif
