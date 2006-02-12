/*	$OpenBSD: keyboard.h,v 1.3 2006/02/12 21:49:08 miod Exp $	*/
/*	$NetBSD: keyboard.h,v 1.1 1998/05/15 10:15:54 tsubai Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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

#define ADBK_CAPSLOCK	0x39
#define	ADBK_RESET	0x7f

#define ADBK_KEYVAL(key)	((key) & 0x7f)
#define ADBK_PRESS(key)		(((key) & 0x80) == 0)
#define ADBK_KEYDOWN(key)	(key)
#define ADBK_KEYUP(key)		((key) | 0x80)
#define ADBK_MODIFIER(key)	((((key) & 0x7f) == ADBK_SHIFT) || \
				 (((key) & 0x7f) == ADBK_CONTROL) || \
				 (((key) & 0x7f) == ADBK_FLOWER) || \
				 (((key) & 0x7f) == ADBK_OPTION))

#ifndef KEYBOARD_ARRAY
extern unsigned char keyboard[128];
#else
unsigned char keyboard[128] = {
	 30,
	 31,
	 32,
	 33,
	 35,
	 34,
	 44,
	 45,
	 46,
	 47,
#ifdef FIX_SV_X_KBDBUG
	 41,
#else
	 86,
#endif
	 48,
	 16,
	 17,
	 18,
	 19,
	 21,
	 20,
	  2,
	  3,
	  4,
	  5,
	  7,
	  6,
	 13,
	 10,
	  8,
	 12,
	  9,
	 11,
	 27,
	 24,
	 22,
	 26,
	 23,
	 25,
	 28,
	 38,
	 36,
	 40,
	 37,
	 39,
	 43,
	 51,
	 53,
	 49,
	 50,
	 52,
	 15,
	 57,
#ifdef FIX_SV_X_KBDBUG
	 86,
#else
	 41,
#endif
	211, /* Delete */
	105, /* MODE/KP_Enter */
	  1,
	 29,
	219,
	 42,
	 58,
	 56, /* L Alt */
	203,  /* Left */
	205,  /* Right */
	208,  /* Down */
	200,  /* Up */
	  0, /* Fn */
	  0,
	 83,
	  0,
	 55,
	  0,
	 78,
	  0,
	 69,
	  0,
	  0,
	  0,
	181,
	156,
	  0,
	 74,
	  0,
	  0,
	118,
	 82,
	 79,
	 80,
	 81,
	 75,
	 76,
	 77,
	 71,
	  0,
	 72,
	 73,
	  0,
	  0,
	 51,
	 63, /* F5 */
	 64, /* F6 */
	 65, /* F7 */
	 61, /* F3 */
	 66, /* F8 */
	 67, /* F9 */
	  0,
	 87, /* F11 */
	  0,
	  0,
	156,
	  0,
	  0,
	 68, /* F10 */
	  0,
	 88, /* F12 */
	  0,
	  0,
	  0,
	199,
	201,
	  0,
	 62, /* F4 */
	207,
	 60, /* F2 */
	209,
	 59, /* F1 */
	  0,
	  0,
	  0,
	  0,
	  0 /* pwr */
};
#endif /* KEYBOARD_ARRAY */
