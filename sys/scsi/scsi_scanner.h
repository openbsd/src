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
#define READ_BIG		0x28
#define WRITE_BIG		0x2a
#define OBJECT_POSITION		0x31
#define GET_BUFFER_STATUS	0x34

/* generic scanner command formats */

struct scsi_rw_scanner {
#define	READ			0x08
#define WRITE			0x0a
	u_char	opcode;
	u_char	byte2;
#define	SRW_FIXED		0x01
	u_char	len[3];
	u_char	control;
};

struct scsi_start_stop {
	u_char	opcode;
	u_char	byte2;
	u_char	unused[2];
	u_char  how;
#define	SSS_STOP		0x00
#define	SSS_START		0x01
#define	SSS_LOEJ		0x02
	u_char	control;
};

struct scsi_set_window {
#define SET_WINDOW		0x24 /* set params of image area and windows */
#define GET_WINDOW		0x25
	u_char	opcode;
	u_char	byte2;
	u_char	reserved[4];
	u_char  len[3];
	u_char	control;
};

struct scsi_window_header {
	u_char	reserved[6];
	u_char  len[2];		    /* MSB-LSB */
};

struct scsi_window_data {
	u_char	window_id;	    /* must be zero */
	u_char	res1:7;
	u_char	auto_bit:1;
	u_char	x_res[2];
	u_char	y_res[2];
	u_char	x_org[4];
	u_char	y_org[4];
	u_char	width[4];
	u_char	length[4];
	u_char	brightness;
	u_char	threshold;
	u_char	contrast;
	u_char	image_comp;	    /* image composition (data type) */
	u_char	bits_per_pixel;
	u_char	halftone_pattern[2];
	u_char	rif:1;		    /* reverse image format (mono negative) */
	u_char  res2:4;
	u_char	pad_type:3;
	u_char	bit_ordering[2];
	u_char	compression_type;
	u_char	compression_arg;
	u_char	res3[6];
};

/* mustek scsi commands */

#define MUSTEK_SET_WINDOW	0x04	/* set image area and windows */
#define MUSTEK_ADF		0x10	/* ADF and backtracking selection */
#define MUSTEK_LUT		0x55	/* look up table download */

#endif /* _SCSI_SCANNER_H_ */
