/*	$OpenBSD: cacreg.h,v 1.4 2003/11/16 20:30:06 avsm Exp $	*/
/*	$NetBSD: cacreg.h,v 1.5 2001/01/10 16:48:04 ad Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1999 Jonathan Lemon
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _IC_CACREG_H_
#define	_IC_CACREG_H_

/* Board register offsets */
#define	CAC_REG_CMD_FIFO		0x04
#define	CAC_REG_DONE_FIFO		0x08
#define	CAC_REG_INTR_MASK		0x0C
#define	CAC_REG_STATUS			0x10
#define	CAC_REG_INTR_PENDING		0x14

#define	CAC_42REG_CMD_FIFO		0x40
#define	CAC_42REG_DONE_FIFO		0x44
#define	CAC_42REG_INTR_MASK		0x34
#define	CAC_42REG_STATUS		0x30

#define	CAC_42_EXTINT			0x08

#define	CAC_EISAREG_INTR_MASK		0x01
#define	CAC_EISAREG_LOCAL_MASK		0x04
#define	CAC_EISAREG_LOCAL_DOORBELL	0x05
#define	CAC_EISAREG_SYSTEM_MASK		0x06
#define	CAC_EISAREG_SYSTEM_DOORBELL	0x07
#define	CAC_EISAREG_LIST_ADDR		0x08
#define	CAC_EISAREG_LIST_LEN		0x0c
#define	CAC_EISAREG_TAG			0x0f
#define	CAC_EISAREG_COMPLETE_ADDR	0x10
#define	CAC_EISAREG_LIST_STATUS		0x16

/* EISA channel control */
#define	CAC_EISA_CHANNEL_BUSY		0x01
#define	CAC_EISA_CHANNEL_CLEAR		0x02

/* Interrupt mask values */
#define	CAC_INTR_DISABLE		0x00
#define	CAC_INTR_ENABLE			0x01

/* Interrupt status masks */
#define	CAC_INTR_FIFO_NEMPTY		0x01

/* Command types */
#define	CAC_CMD_GET_LOG_DRV_INFO	0x10
#define	CAC_CMD_GET_CTRL_INFO		0x11
#define	CAC_CMD_SENSE_DRV_STATUS	0x12
#define	CAC_CMD_START_RECOVERY		0x13
#define	CAC_CMD_GET_PHYS_DRV_INFO	0x15
#define	CAC_CMD_BLINK_DRV_LEDS		0x16
#define	CAC_CMD_SENSE_DRV_LEDS		0x17
#define	CAC_CMD_GET_LOG_DRV_EXT		0x18
#define	CAC_CMD_GET_CTRL_INFO		0x11
#define	CAC_CMD_READ			0x20
#define	CAC_CMD_WRITE			0x30
#define	CAC_CMD_WRITE_MEDIA		0x31
#define	CAC_CMD_GET_CONFIG		0x50
#define	CAC_CMD_SET_CONFIG		0x51
#define	CAC_CMD_START_FIRMWARE		0x99
#define	CAC_CMD_FLUSH_CACHE		0xc2

/* Return status codes */
#define	CAC_RET_SOFT_ERROR		0x02
#define	CAC_RET_HARD_ERROR		0x04
#define	CAC_RET_CMD_INVALID		0x10
#define	CAC_RET_CMD_REJECTED		0x14

struct cac_drive_info {
	u_int16_t	secsize;
	u_int32_t	secperunit;
	u_int16_t	ncylinders;
	u_int8_t	nheads;
	u_int8_t	signature;
	u_int8_t	psectors;
	u_int16_t	wprecomp;
	u_int8_t	max_acc;
	u_int8_t	control;
	u_int16_t	pcylinders;
	u_int8_t	ptracks;
	u_int16_t	landing_zone;
	u_int8_t	nsectors;
	u_int8_t	checksum;
	u_int8_t	mirror;
} __packed;

struct cac_controller_info {
	u_int8_t	num_drvs;
	u_int32_t	signature;
	u_int8_t	firm_rev[4];
	u_int8_t	rom_rev[4];
	u_int8_t	hw_rev;
	u_int32_t	bb_rev;
	u_int32_t	drv_present_map;
	u_int32_t	ext_drv_map;
	u_int32_t	board_id;
	u_int8_t	cfg_error;
	u_int32_t	non_disk_bits;
	u_int8_t	bad_ram_addr;
	u_int8_t	cpu_rev;
	u_int8_t	pdpi_rev;
	u_int8_t	epic_rev;
	u_int8_t	wcxc_rev;
	u_int8_t	marketing_rev;
	u_int8_t	ctlr_flags;
	u_int8_t	host_flags;
	u_int8_t	expand_dis;
	u_int8_t	scsi_chips;
	u_int32_t	max_req_blocks;
	u_int32_t	ctlr_clock;
	u_int8_t	drvs_per_bus;
	u_int16_t	big_drv_present_map[8];
	u_int16_t	big_ext_drv_map[8];
	u_int16_t	big_non_disk_map[8];
	u_int16_t	task_flags;
	u_int8_t	icl_bus;
	u_int8_t	red_modes;
	u_int8_t	cur_red_mode;
	u_int8_t	red_ctlr_stat;
	u_int8_t	red_fail_reason;
	u_int8_t	reserved[403];
} __packed;

struct cac_hdr {
	u_int8_t	drive;		/* logical drive */
	u_int8_t	priority;	/* block priority */
	u_int16_t	size;		/* size of request, in words */
} __packed;

struct cac_req {
	u_int16_t	next;		/* offset of next request */
	u_int8_t	command;	/* command */
	u_int8_t	error;		/* return error code */
	u_int32_t	blkno;		/* block number */
	u_int16_t	bcount;		/* block count */
	u_int8_t	sgcount;	/* number of scatter/gather entries */
	u_int8_t	reserved;	/* reserved */
} __packed;

struct cac_sgb {
	u_int32_t	length;		/* length of S/G segment */
	u_int32_t	addr;		/* physical address of block */
} __packed;
	
#endif	/* !_IC_CACREG_H_ */
