/*	$OpenBSD: scsi_scanner.h,v 1.10 2005/12/10 01:30:13 deraadt Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
 *   modified for configurable scanner support by Joachim Koenig
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
 *	This product includes software developed by Kenneth Stailey.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * SCSI scanner interface description
 */

#ifndef	_SCSI_SCANNER_H_
#define _SCSI_SCANNER_H_

/* SCSI scanner commands */
#define GET_IMAGE_STATUS	0x0f
#define WRITE_BIG		0x2a
#define OBJECT_POSITION		0x31

/* generic scanner command formats */

struct scsi_r_scanner {
#define	READ_BIG		0x28
	u_int8_t opcode;
	u_int    lun:3;
	u_int    res1:5;
	u_int8_t code;
	u_int8_t res2;
	u_int8_t qualifier;
	u_int8_t len[3];
	u_int8_t control;
};

struct scsi_get_buffer_status {
#define GET_BUFFER_STATUS	0x34
	u_int8_t opcode;
	u_int	 lun:3;
	u_int    res1:4;
	u_int    wait:1;
	u_int8_t res2[5];
	u_int8_t len[2];
	u_int8_t control;
};

struct scsi_rw_scanner {
#define	READ			0x08
#define WRITE			0x0a
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRW_FIXED		0x01
	u_int8_t len[3];
	u_int8_t control;
};

struct scsi_start_stop {
#define START_STOP	0x1b
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t how;
#define	SSS_STOP		0x00
#define	SSS_START		0x01
#define	SSS_LOEJ		0x02
	u_int8_t control;
};

struct scsi_set_window {
#define SET_WINDOW		0x24 /* set params of image area and windows */
#define GET_WINDOW		0x25
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t reserved[4];
	u_int8_t len[3];
	u_int8_t control;
};

struct scsi_window_data {
	u_int8_t reserved[6];		/* window header */
	u_int8_t window_desc_len[2];	/* ditto */
	u_int8_t window_id;		/* must be zero */
	u_int    res1:7;
	u_int    auto_bit:1;
	u_int8_t x_res[2];
	u_int8_t y_res[2];
	u_int8_t x_org[4];
	u_int8_t y_org[4];
	u_int8_t width[4];
	u_int8_t length[4];
	u_int8_t brightness;
	u_int8_t threshold;
	u_int8_t contrast;
	u_int8_t image_comp;		/* image composition (data type) */
	u_int8_t bits_per_pixel;
	u_int8_t halftone_pattern[2];
	u_int    rif:1;			/* reverse image format (mono negative) */
	u_int    res2:4;
	u_int    pad_type:3;
	u_int8_t bit_ordering[2];
	u_int8_t compression_type;
	u_int8_t compression_arg;
	u_int8_t res3[6];
};

/* mustek scsi commands */

#define MUSTEK_SET_WINDOW	0x04	/* set image area and windows */
#define MUSTEK_ADF		0x10	/* ADF and backtracking selection */
#define MUSTEK_LUT		0x55	/* look up table download */

#endif /* _SCSI_SCANNER_H_ */
