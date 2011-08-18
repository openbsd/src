/*	$OpenBSD: dioreg.h,v 1.7 2011/08/18 19:55:43 miod Exp $	*/
/*	$NetBSD: dioreg.h,v 1.3 1997/01/30 09:18:40 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Portions of this file are derived from software contributed to Berkeley
 * by the Systems Programming Group of the University of Utah Computer
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register definitions for the DIO and DIO-II bus.
 */

/*
 * DIO/DIO-II device registers, offsets from base of device.  All
 * registers are 8-bit.
 */
#define	DIO_IDOFF		0x01	/* primary device id */
#define	DIO_IPLOFF		0x03	/* interrupt level */
#define DIO_SECIDOFF		0x15	/* secondary device id */
#define	DIOII_SIZEOFF		0x101	/* device size */

/*
 * System physical addresses of some `special' DIO devices.
 */
#define	DIO_IHPIBADDR		0x478000 /* internal HP-IB; select code 7 */

/*
 * DIO ranges from select codes 0-31 at physical addresses given by:
 *	0x600000 + sc * 0x10000
 * DIO cards are addressed in the range 0-31 [0x600000-0x800000) for
 * their control space and the remaining areas, [0x200000-0x400000) and
 * [0x800000-0x1000000), are for additional space required by a card;
 * e.g. a display framebuffer.
 *
 * DIO-II ranges from select codes 132-255 at physical addresses given by:
 *	0x1000000 + (sc - 132) * 0x400000
 * The address range of DIO-II space is thus [0x1000000-0x20000000).
 *
 * DIO/DIO-II space is too large to map in its entirety, instead devices
 * are mapped into kernel virtual address space allocated from a range
 * of EIOMAPSIZE pages (vmparam.h) starting at ``extiobase''.
 */
#define	DIO_BASE		0x00600000	/* start of DIO space */
#define	DIO_END			0x01000000	/* end of DIO space */
#define	DIO_DEVSIZE		0x00010000	/* size of a DIO device */

#define	DIOII_BASE		0x01000000	/* start of DIO-II space */
#define	DIOII_END		0x20000000	/* end of DIO-II space */
#define	DIOII_DEVSIZE		0x00400000	/* size of a DIO-II scode */
#define	DIOII_DEVSIZE_UNIT	0x00100000	/* unit of DIO-II size */

/*
 * Find the highest select code for a given machine; HP320 doesn't
 * have DIO-II.
 */
#if defined(HP320)
#define	DIO_SCMAX(machineid)	((machineid) == HP_320 ? 32 : 256)
#else
#define	DIO_SCMAX(machineid)	256
#endif

/*
 * Base of DIO-II select codes.
 */
#define	DIOII_SCBASE		132

/*
 * Macros to determine if device is DIO or DIO-II.
 */
#define	DIO_ISDIO(scode)	((scode) >= 0 && (scode) < 32)
#define	DIO_ISDIOII(scode)	((scode) >= DIOII_SCBASE && (scode) < 256)

/*
 * Macro to determine if device is a framebuffer, given the
 * primary id of the device.  We key off this to determine if
 * we should look at secondary id and ignore interrupt level.
 */
#define	DIO_ISFRAMEBUFFER(id)		\
	((id) == DIO_DEVICE_ID_FRAMEBUFFER)

/*
 * Macro to extract primary and secondary device ids, given
 * the base address of the device.
 */
#define	DIO_ID(base)			\
	(*((u_int8_t *)((u_long)(base) + DIO_IDOFF)))
#define	DIO_SECID(base)			\
	(*((u_int8_t *)((u_long)(base) + DIO_SECIDOFF)))

/*
 * Macro to extract the interrupt level, given the
 * base address of the device.
 */
#define	DIO_IPL(base)			\
	((((*((u_int8_t *)((u_long)(base) + DIO_IPLOFF))) >> 4) & 0x03) + 3)

/*
 * Macro to compute the size of a DIO-II device's address
 * space, given the base address of the device.
 */
#define DIOII_SIZE(base)		\
	((int)((*((u_int8_t *)((u_long)(base) + DIOII_SIZEOFF)) + 1)	\
	    * DIOII_DEVSIZE_UNIT))

/*
 * Given a select code and device base address, compute
 * the size of the DIO/DIO-II device.
 */
#define	DIO_SIZE(scode, base)		\
	(DIO_ISDIOII((scode)) ? DIOII_SIZE((base)) : DIO_DEVSIZE)
