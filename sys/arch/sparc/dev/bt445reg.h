/*	$OpenBSD: bt445reg.h,v 1.1 2003/06/17 21:21:31 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Actual BT445 register layout
 */

/* Read/write address */
#define	BT445_ADDRESS				0

/*
 * Access to a register is done by programming the address register with
 * the low 8 bits, and then reading or writing at the register pointed out
 * by the high 8 bits.
 */
#define	BT445_REGISTER_OFFSET(x)		((x) & 0xff)
#define	BT445_REGISTER_INDEX(x)			(((x) >> 8) & 0xff)

/* Palette data - 3 r/w cycles par address, and autoincrement */
#define	BT445_PALDATA				1

/* Overlay palette data - 3 r/w cycles par address, and autoincrement */
#define	BT445_OVPALDATA				3

/*
 * Various registers (very incomplete...)
 */
#define	BT445_ID				0x0200
#define	BT445_REVISION				0x0201

