/*	$OpenBSD: diofbreg.h,v 1.1 2005/01/14 22:39:25 miod Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: grfreg.h 1.6 92/01/31$
 *
 *	@(#)grfreg.h	8.1 (Berkeley) 6/10/93
 */

/* 300 bitmapped display hardware primary id */
#define GRFHWID		0x39

/* 300 internal bitmapped display address */
#define GRFIADDR	0x560000

/* 300 hardware secondary ids */
#define GID_GATORBOX	1
#define	GID_TOPCAT	2
#define GID_RENAISSANCE	4
#define GID_LRCATSEYE	5
#define GID_HRCCATSEYE	6
#define GID_HRMCATSEYE	7
#define GID_DAVINCI	8
#define GID_XXXCATSEYE	9
#define GID_XGENESIS   11
#define GID_TIGER      12
#define GID_YGENESIS   13
#define GID_HYPERION   14

#ifndef	_LOCORE
struct	diofbreg {
	u_int8_t	:8;
	u_int8_t	id;		/* +0x01 */
	u_int8_t	pad1[0x3];
	u_int8_t	fbwmsb;		/* +0x05 */
	u_int8_t	:8;
	u_int8_t	fbwlsb;		/* +0x07 */
	u_int8_t	:8;
	u_int8_t	fbhmsb;		/* +0x09 */
	u_int8_t	:8;
	u_int8_t	fbhlsb;		/* +0x0B */
	u_int8_t	:8;
	u_int8_t	dwmsb;		/* +0x0D */
	u_int8_t	:8;
	u_int8_t	dwlsb;		/* +0x0F */
	u_int8_t	:8;
	u_int8_t	dhmsb;		/* +0x11 */
	u_int8_t	:8;
	u_int8_t	dhlsb;		/* +0x13 */
	u_int8_t	:8;
	u_int8_t	id2;		/* +0x15 */
	u_int8_t	pad2[0x47];
	u_int8_t	fbomsb;		/* +0x5d */
	u_int8_t	:8;
	u_int8_t	fbolsb;		/* +0x5f */
};
#endif

/*
 * Offsets into the display ROM that is part of the first 4K of each
 * DIO display device.
 */
#define FONTROM		0x3b	/* Offset of font information structure. */
#define FONTADDR	0x4	/* Offset from FONTROM to font address. */
#define FONTHEIGHT	0x0	/* Offset from font address to font height. */
#define FONTWIDTH	0x2	/* Offset from font address to font width. */
#define FONTDATA	0xA	/* Offset from font address to font glyphs. */

#define FBBASE(fb)	((volatile char *)(fb)->fbkva)
