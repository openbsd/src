/*	$OpenBSD: intreg.h,v 1.1 2012/03/28 20:44:23 miod Exp $	*/
/*	$NetBSD: int2reg.h,v 1.5 2009/02/12 06:33:57 rumble Exp $	*/

/*
 * Copyright (c) 2004 Christopher SEKIYA
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
 * 3. The name of the author may not be used to endorse or promote products
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

/* The INT has known locations on all SGI machines */
#define	INT2_IP20		0x1fb801c0
#define	INT2_IP22		0x1fbd9000
#define	INT2_IP24		0x1fbd9880

/* The following registers are all 8 bit. */
#define INT2_LOCAL0_STATUS	0x03
#define INT2_LOCAL0_STATUS_FIFO	0x01
#define INT2_LOCAL0_MASK	0x07
#define INT2_LOCAL1_STATUS	0x0b
#define INT2_LOCAL1_MASK	0x0f
#define INT2_MAP_STATUS		0x13
#define INT2_MAP_MASK0		0x17
#define INT2_MAP_MASK1		0x1b
#define INT2_MAP_POL		0x1f
#define INT2_TIMER_CLEAR	0x23
#define INT2_ERROR_STATUS	0x27
#define INT2_TIMER_0		0x33
#define	INT2_TIMER_1		0x37
#define	INT2_TIMER_2		0x3b
#define INT2_TIMER_CONTROL	0x3f
