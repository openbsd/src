/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: diodevs,v 1.7 2005/09/27 22:05:36 miod Exp 
 */
/* $NetBSD: diodevs,v 1.7 2003/11/23 01:57:35 tsutsui Exp $ */

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#define DIO_NDEVICES	48

struct dio_devdata dio_devdatas[] = {
	{ 0x02,	0,	1 },
	{ 0x82,	0,	1 },
	{ 0x42,	0,	1 },
	{ 0xc2,	0,	1 },
	{ 0x05,	0,	1 },
	{ 0x85,	0,	1 },
	{ 0x15,	0,	1 },
	{ 0x95,	0,	1 },
	{ 0x08,	0,	1 },
	{ 0x01,	0,	1 },
	{ 0x00,	0,	1 },
	{ 0x07,	0,	1 },
	{ 0x27,	0,	1 },
	{ 0x47,	0,	1 },
	{ 0x67,	0,	1 },
	{ 0x39,	0,	1 },
	{ 0x39,	0x01,	1 },
	{ 0x39,	0x02,	1 },
	{ 0x39,	0x04,	2 },
	{ 0x39,	0x05,	1 },
	{ 0x39,	0x06,	1 },
	{ 0x39,	0x07,	1 },
	{ 0x39,	0x08,	2 },
	{ 0x39,	0x09,	1 },
	{ 0x39,	0x0e,	1 },
	{ 0x39,	0x11,	4 },
	{ 0x39,	0x0b,	1 },
	{ 0x39,	0x0c,	3 },
	{ 0x39,	0x0d,	1 },
	{ 0x03,	0,	1 },
	{ 0x04,	0,	1 },
	{ 0x06,	0,	1 },
	{ 0x09,	0,	1 },
	{ 0x0a,	0,	1 },
	{ 0x0b,	0,	1 },
	{ 0x12,	0,	1 },
	{ 0x13,	0,	1 },
	{ 0x16,	0,	1 },
	{ 0x19,	0,	1 },
	{ 0x1a,	0,	4 },
	{ 0x1b,	0,	1 },
	{ 0x1c,	0,	1 },
	{ 0x1d,	0,	1 },
	{ 0x1e,	0,	1 },
	{ 0x1f,	0,	1 },
	{ 0x31,	0,	2 },
	{ 0x34,	0,	1 },
	{ 0xb4,	0,	1 },
};

#ifdef DIOVERBOSE
struct dio_devdesc dio_devdescs[] = {
	{ 0x02,	0,	DIO_DEVICE_DESC_DCA0 },
	{ 0x82,	0,	DIO_DEVICE_DESC_DCA0REM },
	{ 0x42,	0,	DIO_DEVICE_DESC_DCA1 },
	{ 0xc2,	0,	DIO_DEVICE_DESC_DCA1REM },
	{ 0x05,	0,	DIO_DEVICE_DESC_DCM },
	{ 0x85,	0,	DIO_DEVICE_DESC_DCMREM },
	{ 0x15,	0,	DIO_DEVICE_DESC_LAN },
	{ 0x95,	0,	DIO_DEVICE_DESC_LANREM },
	{ 0x08,	0,	DIO_DEVICE_DESC_FHPIB },
	{ 0x01,	0,	DIO_DEVICE_DESC_NHPIB },
	{ 0x00,	0,	DIO_DEVICE_DESC_IHPIB },
	{ 0x07,	0,	DIO_DEVICE_DESC_SCSI0 },
	{ 0x27,	0,	DIO_DEVICE_DESC_SCSI1 },
	{ 0x47,	0,	DIO_DEVICE_DESC_SCSI2 },
	{ 0x67,	0,	DIO_DEVICE_DESC_SCSI3 },
	{ 0x39,	0,	DIO_DEVICE_DESC_FRAMEBUFFER },
	{ 0x39,	0x01,	DIO_DEVICE_DESC_GATORBOX },
	{ 0x39,	0x02,	DIO_DEVICE_DESC_TOPCAT },
	{ 0x39,	0x04,	DIO_DEVICE_DESC_RENAISSANCE },
	{ 0x39,	0x05,	DIO_DEVICE_DESC_LRCATSEYE },
	{ 0x39,	0x06,	DIO_DEVICE_DESC_HRCCATSEYE },
	{ 0x39,	0x07,	DIO_DEVICE_DESC_HRMCATSEYE },
	{ 0x39,	0x08,	DIO_DEVICE_DESC_DAVINCI },
	{ 0x39,	0x09,	DIO_DEVICE_DESC_XXXCATSEYE },
	{ 0x39,	0x0e,	DIO_DEVICE_DESC_HYPERION },
	{ 0x39,	0x11,	DIO_DEVICE_DESC_FB3x2 },
	{ 0x39,	0x0b,	DIO_DEVICE_DESC_XGENESIS },
	{ 0x39,	0x0c,	DIO_DEVICE_DESC_TIGERSHARK },
	{ 0x39,	0x0d,	DIO_DEVICE_DESC_YGENESIS },
	{ 0x03,	0,	DIO_DEVICE_DESC_MISC0 },
	{ 0x04,	0,	DIO_DEVICE_DESC_MISC1 },
	{ 0x06,	0,	DIO_DEVICE_DESC_PARALLEL },
	{ 0x09,	0,	DIO_DEVICE_DESC_MISC2 },
	{ 0x0a,	0,	DIO_DEVICE_DESC_MISC3 },
	{ 0x0b,	0,	DIO_DEVICE_DESC_MISC4 },
	{ 0x12,	0,	DIO_DEVICE_DESC_MISC5 },
	{ 0x13,	0,	DIO_DEVICE_DESC_AUDIO },
	{ 0x16,	0,	DIO_DEVICE_DESC_MISC6 },
	{ 0x19,	0,	DIO_DEVICE_DESC_MISC7 },
	{ 0x1a,	0,	DIO_DEVICE_DESC_MISC8 },
	{ 0x1b,	0,	DIO_DEVICE_DESC_MISC9 },
	{ 0x1c,	0,	DIO_DEVICE_DESC_MISC10 },
	{ 0x1d,	0,	DIO_DEVICE_DESC_MISC11 },
	{ 0x1e,	0,	DIO_DEVICE_DESC_MISC12 },
	{ 0x1f,	0,	DIO_DEVICE_DESC_MISC13 },
	{ 0x31,	0,	DIO_DEVICE_DESC_VME },
	{ 0x34,	0,	DIO_DEVICE_DESC_DCL },
	{ 0xb4,	0,	DIO_DEVICE_DESC_DCLREM },
};
#endif /* DIOVERBOSE */
