/*	$OpenBSD: oplreg.h,v 1.1 1999/01/02 00:02:40 niklas Exp $	*/
/*	$NetBSD: oplreg.h,v 1.3 1998/11/25 22:17:06 augustss Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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

/* Offsets from base address */
#define OPL_L				0
#define OPL_R				2

/* Offsets from base+[OPL_L|OPL_R] */
#define OPL_STATUS			0
#define   OPL_STATUS_IRQ		0x80
#define   OPL_STATUS_FT1		0x40
#define   OPL_STATUS_FT2		0x20
#define   OPL_STATUS_MASK		0xE0
#define OPL_ADDR			0
#define OPL_DATA			1

#define OPL_TEST			0x01
#define   OPL_ENABLE_WAVE_SELECT	0x20

#define OPL_TIMER1			0x02
#define OPL_TIMER2			0x03

#define OPL_TIMER_CONTROL		0x04	/* Left */
#define   OPL_TIMER1_START		0x01
#define   OPL_TIMER2_START		0x02
#define   OPL_TIMER2_MASK		0x20
#define   OPL_TIMER1_MASK		0x40
#define   OPL_IRQ_RESET			0x80

#define OPL_CONNECTION_SELECT		0x04	/* Right */
#define   OPL_NOCONNECTION		0x00
#define   OPL_R_4OP_0			0x01
#define   OPL_R_4OP_1			0x02
#define   OPL_R_4OP_2			0x04
#define   OPL_L_4OP_0			0x08
#define   OPL_L_4OP_1			0x10
#define   OPL_L_4OP_2			0x20

#define OPL_MODE			0x05	/* Right */
#define   OPL3_ENABLE			0x01
#define   OPL4_ENABLE			0x02

#define OPL_KBD_SPLIT			0x08	/* Left */
#define   OPL_KEYBOARD_SPLIT		0x40
#define   OPL_COMPOSITE_SINE_WAVE_MODE	0x80

#define OPL_PERCUSSION			0xbd	/* Left */
#define   OPL_NOPERCUSSION		0x00
#define   OPL_HIHAT			0x01
#define   OPL_CYMBAL			0x02
#define   OPL_TOMTOM			0x04
#define   OPL_SNAREDRUM			0x08
#define   OPL_BASSDRUM			0x10
#define	  OPL_PERCUSSION_ENABLE		0x20
#define   OPL_VIBRATO_DEPTH		0x40
#define   OPL_TREMOLO_DEPTH		0x80

/*
 * Offsets to the register banks for operators.
 */
/* AM/VIB/EG/KSR/Multiple (0x20 to 0x35) */
#define OPL_AM_VIB			0x20
#define   OPL_KSR			0x10
#define   OPL_SUSTAIN			0x20
#define   OPL_VIBRATO			0x40
#define   OPL_TREMOLO			0x80
#define   OPL_MULTIPLE_MASK		0x0f

/* KSL/Total level (0x40 to 0x55) */
#define OPL_KSL_LEVEL			0x40
#define   OPL_KSL_MASK			0xc0	/* Envelope scaling bits */
#define   OPL_TOTAL_LEVEL_MASK		0x3f	/* Strength (volume) of OP */

/* Attack / Decay rate (0x60 to 0x75) */
#define OPL_ATTACK_DECAY		0x60
#define   OPL_ATTACK_MASK		0xf0
#define   DECAY_MASK			0x0f

/* Sustain level / Release rate (0x80 to 0x95) */
#define OPL_SUSTAIN_RELEASE		0x80
#define   OPL_SUSTAIN_MASK		0xf0
#define   OPL_RELEASE_MASK		0x0f

/* Wave select (0xE0 to 0xF5) */
#define OPL_WAVE_SELECT			0xe0

#define OPL_MAXREG			0xf5

/*
 * Offsets to the register banks for voices.
 */
/* F-Number low bits (0xA0 to 0xA8). */
#define OPL_FNUM_LOW			0xa0

/* F-number high bits / Key on / Block (octave) (0xB0 to 0xB8) */
#define OPL_KEYON_BLOCK			0xb0
#define	  OPL_KEYON_BIT			0x20
#define	  OPL_BLOCKNUM_MASK		0x1c
#define   OPL_FNUM_HIGH_MASK		0x03

/* Feedback / Connection (0xc0 to 0xc8) */
#define OPL_FEEDBACK_CONNECTION		0xc0
#define   OPL_FEEDBACK_MASK		0x0e
#define   OPL_CONNECTION_BIT		0x01
#define   OPL_STEREO_BITS		0x30	/* OPL-3 only */
#define     OPL_VOICE_TO_LEFT		0x10
#define     OPL_VOICE_TO_RIGHT		0x20

#define OPL2_NVOICE 9
#define OPL3_NVOICE 18
