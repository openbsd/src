/*	$OpenBSD: archtype.h,v 1.1 1998/01/28 11:14:36 pefo Exp $	*/
/*
 * Copyright (c) 1997 Per Fogelstrom
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

#ifndef _MACHINE_ARCHTYPE_H_
#define _MACHINE_ARCHTYPE_H_
/*
 * Define architectural identitys for the different Mips machines.
 */
#define	ACER_PICA_61		0x1	/* Acer Labs Pica 61 */
#define	MAGNUM			0x2	/* Mips MAGNUM R4000 */
#define	DESKSTATION_RPC44	0x3	/* Deskstation xxx */
#define	DESKSTATION_TYNE	0x4	/* Deskstation xxx */
#define	NKK_AQUARIUS		0x5	/* NKK R4{67}00 PC */
#define	ALGOR_P4032		0x6	/* ALGORITHMICS P-4032 VR4300 */
#define	SNI_RM200		0x7	/* Siemens Nixdorf RM200 */

#define	SGI_INDY		0x10	/* Silicon Graphics Indy */

extern int system_type;		/* Global system type indicator */

#endif /* _MACHINE_ARCHTYPE_H_ */
