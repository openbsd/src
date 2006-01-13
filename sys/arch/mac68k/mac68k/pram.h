/*	$OpenBSD: pram.h,v 1.8 2006/01/13 21:02:38 miod Exp $	*/
/*	$NetBSD: pram.h,v 1.3 1996/05/05 06:18:53 briggs Exp $	*/

/*
 * RTC toolkit version 1.08b, copyright 1995, erik vogan
 *
 * All rights and privledges to this code hereby donated
 * to the ALICE group for use in NetBSD.  see the copyright
 * below for more info...
 */
/*
 * Copyright (c) 1995 Erik Vogan
 * All rights reserved.
 *
 * This code is derived from software contributed to the Alice Group
 * by Erik Vogan.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The following routines are used to get/set the PRAM time
 * (which is stored as seconds since 1904).
 */

unsigned long	pram_readtime(void);
void		pram_settime(unsigned long);

unsigned long	getPramTimeII(void);
void		setPramTimeII(unsigned long);
