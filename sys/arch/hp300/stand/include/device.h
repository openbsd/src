/*	$OpenBSD: device.h,v 1.3 2011/08/18 20:02:58 miod Exp $	*/
/*	$NetBSD: device.h,v 1.1 1997/01/30 10:31:44 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)device.h	8.1 (Berkeley) 6/10/93
 */

struct hp_hw {
	caddr_t	hw_kva;		/* kernel virtual address of control space */
	short	hw_type;	/* type (defined below) */
	short	hw_sc;		/* select code (if applicable) */
	int	hw_ctrl;	/* controller number */
};

#define	MAXCTLRS	16	/* Size of HW table (arbitrary) */

/* controller types */
#define	C_MASK		0xF0
#define	C_HPIB		0x10
#define C_SCSI		0x20
/* device types (controllers with no slaves) */
#define D_MASK		0x0F
#define	D_BITMAP	0x01
#define	D_LAN		0x02
#define	D_COMMDCM	0x03

#define HW_ISHPIB(hw)	(((hw)->hw_type & C_MASK) == C_HPIB)
#define HW_ISSCSI(hw)	(((hw)->hw_type & C_MASK) == C_SCSI)
#define HW_ISDEV(hw,d)	(((hw)->hw_type & D_MASK) == (d))
