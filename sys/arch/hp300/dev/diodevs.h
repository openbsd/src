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


#define	DIO_DEVICE_ID_DCA0	0x02
#define	DIO_DEVICE_DESC_DCA0	"98644A serial"

#define	DIO_DEVICE_ID_DCA0REM	0x82
#define	DIO_DEVICE_DESC_DCA0REM	"98644A serial"

#define	DIO_DEVICE_ID_DCA1	0x42
#define	DIO_DEVICE_DESC_DCA1	"98644A serial"

#define	DIO_DEVICE_ID_DCA1REM	0xc2
#define	DIO_DEVICE_DESC_DCA1REM	"98644A serial"


#define	DIO_DEVICE_ID_DCM	0x05
#define	DIO_DEVICE_DESC_DCM	"98642A serial MUX"

#define	DIO_DEVICE_ID_DCMREM	0x85
#define	DIO_DEVICE_DESC_DCMREM	"98642A serial MUX"


#define	DIO_DEVICE_ID_LAN	0x15
#define	DIO_DEVICE_DESC_LAN	"98643A LAN"

#define	DIO_DEVICE_ID_LANREM	0x95
#define	DIO_DEVICE_DESC_LANREM	"98643A LAN"


#define	DIO_DEVICE_ID_FHPIB	0x08
#define	DIO_DEVICE_DESC_FHPIB	"98625A/98625B HP-IB"

#define	DIO_DEVICE_ID_NHPIB	0x01
#define	DIO_DEVICE_DESC_NHPIB	"98624A HP-IB"

#define	DIO_DEVICE_ID_IHPIB	0x00
#define	DIO_DEVICE_DESC_IHPIB	"internal HP-IB"


#define	DIO_DEVICE_ID_SCSI0	0x07
#define	DIO_DEVICE_DESC_SCSI0	"98265A SCSI"

#define	DIO_DEVICE_ID_SCSI1	0x27
#define	DIO_DEVICE_DESC_SCSI1	"98265A SCSI"

#define	DIO_DEVICE_ID_SCSI2	0x47
#define	DIO_DEVICE_DESC_SCSI2	"98265A SCSI"

#define	DIO_DEVICE_ID_SCSI3	0x67
#define	DIO_DEVICE_DESC_SCSI3	"98265A SCSI"

/* Framebuffer devices; same primary ID, different secondary IDs. */


#define	DIO_DEVICE_ID_FRAMEBUFFER	0x39
#define	DIO_DEVICE_DESC_FRAMEBUFFER	"bitmapped display"


#define	DIO_DEVICE_SECID_GATORBOX	0x01
#define	DIO_DEVICE_DESC_GATORBOX	"98700/98710 (\"gatorbox\") display"

#define	DIO_DEVICE_SECID_TOPCAT	0x02
#define	DIO_DEVICE_DESC_TOPCAT	"98544/98545/98547 (\"topcat\") display"

#define	DIO_DEVICE_SECID_RENAISSANCE	0x04
#define	DIO_DEVICE_DESC_RENAISSANCE	"98720/98721 (\"renaissance\") display"

#define	DIO_DEVICE_SECID_LRCATSEYE	0x05
#define	DIO_DEVICE_DESC_LRCATSEYE	"low-res catseye display"

#define	DIO_DEVICE_SECID_HRCCATSEYE	0x06
#define	DIO_DEVICE_DESC_HRCCATSEYE	"high-res color catseye display"

#define	DIO_DEVICE_SECID_HRMCATSEYE	0x07
#define	DIO_DEVICE_DESC_HRMCATSEYE	"high-res mono catseye display"

#define	DIO_DEVICE_SECID_DAVINCI	0x08
#define	DIO_DEVICE_DESC_DAVINCI	"98730/98731 (\"davinci\") display"

#define	DIO_DEVICE_SECID_XXXCATSEYE	0x09
#define	DIO_DEVICE_DESC_XXXCATSEYE	"catseye display"

#define	DIO_DEVICE_SECID_HYPERION	0x0e
#define	DIO_DEVICE_DESC_HYPERION	"A1096A (\"hyperion\") display"

#define	DIO_DEVICE_SECID_FB3x2	0x11
#define	DIO_DEVICE_DESC_FB3x2	"362/382 internal display"

/* Unsupported framebuffers. */


#define	DIO_DEVICE_SECID_XGENESIS	0x0b
#define	DIO_DEVICE_DESC_XGENESIS	"x-genesis display"

#define	DIO_DEVICE_SECID_TIGERSHARK	0x0c
#define	DIO_DEVICE_DESC_TIGERSHARK	"TurboVRX (\"tigershark\") display"

#define	DIO_DEVICE_SECID_YGENESIS	0x0d
#define	DIO_DEVICE_DESC_YGENESIS	"y-genesis display"

/* Devices not yet supported.  Descriptions are lacking. */


#define	DIO_DEVICE_ID_MISC0	0x03
#define	DIO_DEVICE_DESC_MISC0	"98622A"

#define	DIO_DEVICE_ID_MISC1	0x04
#define	DIO_DEVICE_DESC_MISC1	"98623A"

#define	DIO_DEVICE_ID_PARALLEL	0x06
#define	DIO_DEVICE_DESC_PARALLEL	"internal parallel"

#define	DIO_DEVICE_ID_MISC2	0x09
#define	DIO_DEVICE_DESC_MISC2	"98287A keyboard"

#define	DIO_DEVICE_ID_MISC3	0x0a
#define	DIO_DEVICE_DESC_MISC3	"HP98635A floating point accelerator"

#define	DIO_DEVICE_ID_MISC4	0x0b
#define	DIO_DEVICE_DESC_MISC4	"timer"

#define	DIO_DEVICE_ID_MISC5	0x12
#define	DIO_DEVICE_DESC_MISC5	"98640A"

#define	DIO_DEVICE_ID_AUDIO	0x13
#define	DIO_DEVICE_DESC_AUDIO	"digital audio"

#define	DIO_DEVICE_ID_MISC6	0x16
#define	DIO_DEVICE_DESC_MISC6	"98659A"

#define	DIO_DEVICE_ID_MISC7	0x19
#define	DIO_DEVICE_DESC_MISC7	"237 display"

#define	DIO_DEVICE_ID_MISC8	0x1a
#define	DIO_DEVICE_DESC_MISC8	"quad-wide card"

#define	DIO_DEVICE_ID_MISC9	0x1b
#define	DIO_DEVICE_DESC_MISC9	"98253A"

#define	DIO_DEVICE_ID_MISC10	0x1c
#define	DIO_DEVICE_DESC_MISC10	"98627A"

#define	DIO_DEVICE_ID_MISC11	0x1d
#define	DIO_DEVICE_DESC_MISC11	"98633A"

#define	DIO_DEVICE_ID_MISC12	0x1e
#define	DIO_DEVICE_DESC_MISC12	"98259A"

#define	DIO_DEVICE_ID_MISC13	0x1f
#define	DIO_DEVICE_DESC_MISC13	"8741"

#define	DIO_DEVICE_ID_VME	0x31
#define	DIO_DEVICE_DESC_VME	"98577A VME adapter"

#define	DIO_DEVICE_ID_DCL	0x34
#define	DIO_DEVICE_DESC_DCL	"98628A serial"

#define	DIO_DEVICE_ID_DCLREM	0xb4
#define	DIO_DEVICE_DESC_DCLREM	"98628A serial"
