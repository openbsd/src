/*	$OpenBSD: desktech.h,v 1.6 1999/01/30 22:39:38 imp Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_DESKTECH_H_
#define	_DESKTECH_H_ 1

/*
 * TYNE's Physical address space
 */

#define TYNE_PHYS_MIN		0x00000000	/* 256 Meg */
#define TYNE_PHYS_MAX		0x0fffffff

/*
 * Memory map
 */

#define TYNE_PHYS_MEMORY_START	0x00000000
#define TYNE_PHYS_MEMORY_END	0x0fffffff	/* 256 Meg in 4 slots */

/*
 * I/O map
 */

#define	TYNE_P_ISA_IO		(0x0900000000LL)	/* ISA I/O Control */
#define	TYNE_V_ISA_IO		0xe0000000
#define	TYNE_S_ISA_IO		0x00010000

#define	TYNE_P_ISA_MEM		(0x0100000000LL)	/* ISA Memory control */
#define	TYNE_V_ISA_MEM		0xe1000000
#define	TYNE_S_ISA_MEM		0x00100000

#define	TYNE_P_BOUNCE		(0x0100800000LL)	/* Dma bounce buffer */
#define	TYNE_V_BOUNCE		0xe2000000
#define	TYNE_S_BOUNCE		0x00020000

/*
 * Deskstation rPC44 I/O map.  We map these into one TLB of size 16M.
 * Note: We really have EISA here, but no one has EISA cards yet to 
 * justify implmeneting EISA.
 */
#define RPC44_P_ISA_IO		(0x10000000LL)		/* ISA I/O control */
#define RPC44_V_ISA_IO		(0xb0000000)
#define RPC44_S_ISA_IO		(0x00010000)

#define RPC44_P_ISA_MEM		(0x00000000LL)		/* ISA Memory control */
#define RPC44_V_ISA_MEM		(0xa0000000)
#define RPC44_S_ISA_MEM		(0x01000000)

#endif	/* _DESKTECH_H_ */
