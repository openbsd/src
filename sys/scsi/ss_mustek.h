/*	$NetBSD: ss_mustek.h,v 1.1 1996/02/18 20:32:48 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Joachim Koenig-Baltes.  All rights reserved.
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
 *	This product includes software developed by Joachim Koenig-Baltes.
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

#ifndef	_SS_MUSTEK_H_
#define _SS_MUSTEK_H_ 1

/*
 * support for MUSTEK flatbed SCSI scanners MFS-06000CX and MFS-12000CX
 * (600 and 1200 dpi horizontally resp), not conforming to the SCSI2 spec.
 */

/*
 * Configuration section: describes the mode in which scanner is driven
 * MUSTEK_INCH_SPEC: frame/window sizes are given in inches instead of
 *     pixels, note: unit is 1/8th of an inch
 * MUSTEK_WINDOWS: number of windows in a frame, up to 4 allowed,
 *     not used yet, so set to 0
 */
#define MUSTEK_INCH_SPEC		/* use inches to specify sizes */
#define MUSTEK_WINDOWS		0	/* no window support yet */

/* mustek scsi commands */
#define MUSTEK_SET_WINDOW	0x04	/* set image area and windows */
#define MUSTEK_READ		0x08	/* read command */
#define MUSTEK_GET_STATUS	0x0f	/* image status */
#define MUSTEK_MODE_SELECT	0x15	/* set resolution, paper length, .. */
#define MUSTEK_ADF		0x10	/* ADF and backtracking selection */
#define MUSTEK_START_STOP	0x1b	/* start/stop scan */
#define MUSTEK_LUT		0x55	/* look up table download */

/* the size spec is at the same bit position in different commands */
#define	MUSTEK_UNIT_INCHES		0x00
#define MUSTEK_UNIT_PIXELS		0x08
#ifdef MUSTEK_INCH_SPEC
#define MUSTEK_UNIT_SPEC	MUSTEK_UNIT_INCHES
#else
#define MUSTEK_UNIT_SPEC	MUSTEK_UNIT_PIXELS
#endif

/*
 * SCSI command formats
 */

struct mustek_set_window_cmd {
	u_char	opcode;			/* 0x04 */
	u_char	reserved[3];
	u_char	length;			/* in bytes */
	u_char	control;
};

struct mustek_set_window_data {
#define MUSTEK_LINEART_BACKGROUND	0x00
#define MUSTEK_HALFTONE_BACKGROUND	0x01
	u_char	frame_header;		/* unit-defines also apply */
	u_char	frame_tl_x_0;
	u_char	frame_tl_x_1;
	u_char	frame_tl_y_0;
	u_char	frame_tl_y_1;
	u_char	frame_br_x_0;
	u_char	frame_br_x_1;
	u_char	frame_br_y_0;
	u_char	frame_br_y_1;
#if MUSTEK_WINDOWS >= 1
#define MUSTEK_WINDOW_MASK	0x80
	u_char	window1_header;		/* unit-defines also apply */
	u_char	window1_tl_x_0;
	u_char	window1_tl_x_1;
	u_char	window1_tl_y_0;
	u_char	window1_tl_y_1;
	u_char	window1_br_x_0;
	u_char	window1_br_x_1;
	u_char	window1_br_y_0;
	u_char	window1_br_y_1;
#endif
#if MUSTEK_WINDOWS >= 2
	u_char	window2_header;
	u_char	window2_tl_x_0;
	u_char	window2_tl_x_1;
	u_char	window2_tl_y_0;
	u_char	window2_tl_y_1;
	u_char	window2_br_x_0;
	u_char	window2_br_x_1;
	u_char	window2_br_y_0;
	u_char	window2_br_y_1;
#endif
#if MUSTEK_WINDOWS >= 3
	u_char	window3_header;
	u_char	window3_tl_x_0;
	u_char	window3_tl_x_1;
	u_char	window3_tl_y_0;
	u_char	window3_tl_y_1;
	u_char	window3_br_x_0;
	u_char	window3_br_x_1;
	u_char	window3_br_y_0;
	u_char	window3_br_y_1;
#endif
#if MUSTEK_WINDOWS == 4
	u_char	window4_header;
	u_char	window4_tl_x_0;
	u_char	window4_tl_x_1;
	u_char	window4_tl_y_0;
	u_char	window4_tl_y_1;
	u_char	window4_br_x_0;
	u_char	window4_br_x_1;
	u_char	window4_br_y_0;
	u_char	window4_br_y_1;
#endif
};

struct mustek_read_cmd {
	u_char	opcode;			/* 0x08 */
	u_char	reserved;
	u_char	length_2;		/* number of LINES to be read (MSB) */
	u_char	length_1;		/* number of LINES to be read */
	u_char	length_0;		/* number of LINES to be read (LSB) */
	u_char	control;
};

struct mustek_get_status_cmd {
	u_char	opcode;			/* 0x0f */
	u_char	reserved[3];
	u_char	length;			/* 0x06 */
	u_char	control;
};

struct mustek_get_status_data {
#define MUSTEK_READY 0
#define MUSTEK_BUSY  -1	
	u_char	ready_busy;		/* 0 = ready */
	u_char	bytes_per_line_0;	/* LSB */
	u_char	bytes_per_line_1;	/* MSB */
	u_char	lines_0;		/* LSB */
	u_char	lines_1;
	u_char	lines_2;		/* MSB */
};

struct mustek_mode_select_cmd {
	u_char	opcode;			/* 0x15 */
	u_char	reserved[2];
	u_char	length_1;		/* MSB */
	u_char	length_0;		/* LSB */
	u_char	control;
};

/*
 * resolution settings:
 *   MFS06000CX:
 *   1% : 0x01 0x02 ... 0x64
 *           3    6 ...  300 dpi
 *   10%: 0x1e 0x3c 0x5a 0x14 0x32 0x50 0x0a 0x28 0x46 0x64
 *         330  360  390  420  450  480  510  540  570  600 dpi
 *   MFS12000CX:
 *   1% : 0x01 0x02 ... 0x64
 *           6   12 ...  600 dpi
 *   10%: 0x1e 0x3c 0x5a 0x14 0x32 0x50 0x0a 0x28 0x46 0x64
 *         660  720  780  840  900  960 1020 1080 1140 1200 dpi
 */
struct mustek_mode_select_data {
#define MUSTEK_MODE_MASK		0x83
#define MUSTEK_HT_PATTERN_BUILTIN	0x00
#define MUSTEK_HT_PATTERN_DOWNLOADED	0x10
	u_char	mode;
	u_char	resolution;
	u_char	brightness;
	u_char	contrast;
	u_char	grain;			/* 0 = 8x8, .....  5 = 2x2  */
	u_char	velocity;		/* 0 = fast, ...., 4 = slow */
	u_char	reserved[2];
	u_char	paperlength_0;		/* LSB */
	u_char	paperlength_1;		/* MSB */
};

struct mustek_start_scan_cmd {
	u_char	opcode;			/* 0x1b */
	u_char	reserved[3];
#define MUSTEK_SCAN_STOP	0x00
#define MUSTEK_SCAN_START	0x01
#define MUSTEK_GRAY_FILTER	0x00
#define MUSTEK_RED_FILTER	0x08
#define MUSTEK_GREEN_FILTER	0x10
#define MUSTEK_BLUE_FILTER	0x18
#define MUSTEK_GRAY_MODE	0x40
#define MUSTEK_BIT_MODE		0x00
#define MUSTEK_RES_STEP_1	0x00
#define MUSTEK_RES_STEP_10	0x80
	u_char	mode;
	u_char	control;
};

#endif /* _SS_MUSTEK_H_ */
