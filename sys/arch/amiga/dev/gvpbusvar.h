/*	$NetBSD: gvpbusvar.h,v 1.8 1995/08/18 15:27:55 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
#ifndef _GVPBUSVAR_H_
#define _GVPBUSVAR_H_

enum gvpbusprod {
	GVP_GFORCE_040 = 0x20,
	GVP_GFORCE_040_SCSI = 0x30,
	GVP_A1291_SCSI = 0x40,
	GVP_GFORCE_030 = 0xa0,
	GVP_GFORCE_030_SCSI = 0xb0,
	GVP_COMBO_R4 = 0x60,
	GVP_COMBO_R4_SCSI = 0x70,
	GVP_COMBO_R3 = 0xe0,
	GVP_COMBO_R3_SCSI = 0xf0,
	GVP_SERIESII = 0xf8,
	GVP_A530 = 0xc0,
	GVP_A530_SCSI = 0xd0,
	GVP_IOEXTEND = 0x98,
	GVP_PHONEPAK = 0x78,
};

enum gvpbusflags {
	GVP_IO = 0x1,
	GVP_ACCEL = 0x2,
	GVP_SCSI = 0x4,
	GVP_24BITDMA = 0x8,
	GVP_25BITDMA = 0x10,
	GVP_NOBANK = 0x20,
	GVP_14MHZ = 0x40,
};

struct gvpbus_args {
	struct zbus_args zargs;
	enum gvpbusprod prod;
	int  flags;
};

#endif
