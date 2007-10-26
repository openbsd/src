/*	$OpenBSD: pssreg.h,v 1.3 2007/10/26 15:00:49 martin Exp $	*/
/*	$NetBSD: pssreg.h,v 1.2 1995/05/08 22:02:09 brezak Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */
/*
 * Copyright (c) 1993 Analog Devices Inc. All rights reserved
 */

/*
 * Macros to detect valid hardware configuration data.
 */
#define PSS_BASE_VALID(base) ((base) == 0x220 || (base) == 0x240)

/*
 * ESC614 Interface chip
 */
#define ADDR_MASK	0x003f

#define INT_MASK	0xffc7
#define INT_3_BITS	0x0008
#define INT_5_BITS	0x0010
#define INT_7_BITS	0x0018
#define INT_9_BITS	0x0020
#define INT_10_BITS	0x0028
#define INT_11_BITS	0x0030
#define INT_12_BITS	0x0038

#define INT_TEST_BIT	0x0200
#define INT_TEST_PASS	0x0100
#define INT_TEST_BIT_MASK 0xFDFF

#define DMA_MASK	0xfff8
#define DMA_0_BITS	0x0001
#define DMA_1_BITS	0x0002
#define DMA_3_BITS	0x0003
#define DMA_5_BITS	0x0004
#define DMA_6_BITS	0x0005
#define DMA_7_BITS	0x0006

#define DMA_TEST_BIT	0x0080
#define DMA_TEST_PASS	0x0040
#define DMA_TEST_BIT_MASK 0xFF7F

/* Echo DSP Flags */
#define DSP_FLAG3	0x10
#define DSP_FLAG2	0x08
#define DSP_FLAG1	0x80
#define DSP_FLAG0	0x40

/* ESC614 register offsets */
#define PSS_NPORT		32

#define PSS_DATA	0x00
#define PSS_STATUS	0x02
#define PSS_CONTROL	0x02
#define PSS_ID_VERS	0x04
#define PSS_IRQ_ACK	0x04

#define PSS_CONFIG	0x10
#define PSS_WSS_CONFIG	0x12
#define SB_CONFIG	0x14
#define CD_CONFIG	0x16
#define MIDI_CONFIG	0x18
#define UART_CONFIG	0x1a

/* PSS control register */
#define PSS_WEIE	0x8000
#define PSS_RFIE	0x4000
#define PSS_RESET	0x2000
#define PSS_FLAG1	0x1000
#define PSS_FLAG0	0x0800

/* PSS status register */
#define PSS_WRITE_EMPTY	0x8000
#define PSS_READ_FULL	0x4000
#define PSS_IRQ		0x2000
#define PSS_DMQ_TC	0x1000
#define PSS_FLAG3	0x0800
#define PSS_FLAG2	0x0400

/* Game control register */
#define GAME_BIT	0x0400
#define GAME_BIT_MASK	0xfbff

/* MPU registers */
#define MIDI_NPORT	8

#define MIDI_DATA_REG	0x00
#define MIDI_STATUS_REG	0x01
#define MIDI_COMMAND_REG 0x01

#define MIDI_SR_RF	0x80
#define MIDI_SR_TE	0x40

/* CD Interface registers */
#define CD_NPORT	16

#define CD_POL_MASK	0xFFBF
#define CD_POL_BIT	0x0040

/* Philips amplifier controls: only via DSP */
/* DSP commands */
#define SET_MASTER_COMMAND	0x0010
#define MASTER_VOLUME_LEFT	0x0000
#define MASTER_VOLUME_RIGHT	0x0100
#define MASTER_BASS		0x0200
#define MASTER_TREBLE		0x0300
#define MASTER_SWITCH		0x0800

#define PSS_STEREO		0x00ce
#define PSS_PSEUDO		0x00d6
#define PSS_SPATIAL		0x00de
#define PSS_MONO		0x00c6

#define PHILLIPS_VOL_MIN	-64
#define PHILLIPS_VOL_MAX	6
#define PHILLIPS_VOL_DELTA	70
#define PHILLIPS_VOL_INITIAL	-20
#define PHILLIPS_VOL_CONSTANT	252
#define PHILLIPS_VOL_STEP	2
#define PHILLIPS_BASS_MIN	-12
#define PHILLIPS_BASS_MAX	15
#define PHILLIPS_BASS_DELTA	27
#define PHILLIPS_BASS_INITIAL	0
#define PHILLIPS_BASS_CONSTANT	246
#define PHILLIPS_BASS_STEP	2
#define PHILLIPS_TREBLE_MIN	-12
#define PHILLIPS_TREBLE_MAX	12
#define PHILLIPS_TREBLE_DELTA	24
#define PHILLIPS_TREBLE_INITIAL	0
#define PHILLIPS_TREBLE_CONSTANT 246
#define PHILLIPS_TREBLE_STEP	2
