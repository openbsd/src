/*	$NetBSD: sbreg.h,v 1.12 1995/05/08 22:02:29 brezak Exp $	*/

/*
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
 *	From: Header: sbreg.h,v 1.3 93/07/18 14:07:28 mccanne Exp (LBL)
 */

/*
 * SoundBlaster register definitions.
 * See "The Developer Kit for Sound Blaster Series, User's Guide" for more
 * complete information (avialable from Creative Labs, Inc.).  We refer
 * to this documentation as "SBK".
 *
 * We handle two types of cards: the basic SB version 2.0+, and
 * the SB PRO.  There are several distinct pieces of the hardware:
 *
 *   joystick port	(independent of I/O base address)
 *   FM synth		(stereo on PRO)
 *   mixer		(PRO only)
 *   DSP (sic)
 *   CD-ROM		(PRO only)
 *
 * The MIDI capabilities are handled by the DSP unit.
 */

/*
 * Address map.  The SoundBlaster can be configured (via jumpers) for
 * either base I/O address 0x220 or 0x240.  The encodings below give
 * the offsets to specific SB ports.  SBP stands for SB port offset.
 */
#define SBP_LFM_STATUS		0	/* R left FM status port */
#define SBP_LFM_ADDR		0	/* W left FM address register */
#define SBP_LFM_DATA		1	/* RW left FM data port */
#define SBP_RFM_STATUS		2	/* R right FM status port */
#define SBP_RFM_ADDR		2	/* W right FM address register */
#define SBP_RFM_DATA		3	/* RW right FM data port */

#define SBP_FM_STATUS		8	/* R FM status port */
#define SBP_FM_ADDR		8	/* W FM address register */
#define SBP_FM_DATA		9	/* RW FM data port */
#define SBP_MIXER_ADDR		4	/* W mixer address register */
#define SBP_MIXER_DATA		5	/* RW mixer data port */
#define	SBP_MIX_RESET		0	/* mixer reset port, value */
#define	SBP_MASTER_VOL		0x22
#define	SBP_FM_VOL		0x26
#define	SBP_CD_VOL		0x28
#define	SBP_LINE_VOL		0x2E
#define	SBP_DAC_VOL		0x04
#define	SBP_MIC_VOL		0x0A	/* warning: only one channel of
					   volume... */
#define SBP_SPEAKER_VOL		0x42
#define SBP_TREBLE_EQ		0x44
#define SBP_BASS_EQ		0x46

#define SBP_SET_IRQ		0x80	/* Soft-configured irq (SB16-) */
#define SBP_SET_DRQ		0x81	/* Soft-configured drq (SB16-) */

#define	SBP_RECORD_SOURCE	0x0C
#define	SBP_STEREO		0x0E
#define		SBP_PLAYMODE_STEREO	0x2
#define		SBP_PLAYMODE_MONO	0x0
#define		SBP_PLAYMODE_MASK	0x2
#define	SBP_OUTFILTER		0x0E
#define	SBP_INFILTER		0x0C

#define	SBP_RECORD_FROM(src, filteron, high) ((src) | (filteron) | (high))
#define		SBP_FILTER_ON		0x0
#define		SBP_FILTER_OFF		0x20
#define		SBP_FILTER_MASK		0x20
#define		SBP_FILTER_LOW		0
#define		SBP_FILTER_HIGH		0x08
#define		SBP_FROM_MIC		0x00
#define		SBP_FROM_CD		0x02
#define		SBP_FROM_LINE		0x06
#define sbdsp_stereo_vol(left, right) (((left) << 4) | (right))
#define SBP_MAXVOL 0xf			/* per channel */
#define SBP_MINVOL 0x0			/* per channel */
#define SBP_AGAIN_TO_SBGAIN(again)	((again) >> 4) /* per channel */
#define SBP_AGAIN_TO_MICGAIN(again)	((again) >> 5) /* mic has only 3 bits,
							  sorry! */
#define SBP_LEFTGAIN(sbgain)		(sbgain & 0xf0)	/* left channel */
#define SBP_RIGHTGAIN(sbgain)		((sbgain & 0xf) << 4) /* right channel */
#define SBP_SBGAIN_TO_AGAIN(sbgain)	SBP_LEFTGAIN(sbgain)
#define SBP_MICGAIN_TO_AGAIN(micgain)	(micgain << 5)
#define SBP_DSP_RESET		6	/* W reset port */
#define 	SB_MAGIC	0xaa	/* card outputs on successful reset */
#define SBP_DSP_READ		10 	/* R read port */
#define SBP_DSP_WRITE		12	/* W write port */
#define SBP_DSP_WSTAT		12	/* R write status */
#define SBP_DSP_RSTAT		14	/* R read status */
#define 	SB_DSP_BUSY	0x80
#define 	SB_DSP_READY	0x80
#define SBP_CDROM_DATA		16	/* RW send cmds/recv data */
#define SBP_CDROM_STATUS	17	/* R status port */
#define SBP_CDROM_RESET		18	/* W reset register */
#define SBP_CDROM_ENABLE	19	/* W enable register */

#define SBP_NPORT 24
#define SB_NPORT 16

/*
 * DSP commands.  This unit handles MIDI and audio capabilities.
 * The DSP can be reset, data/commands can be read or written to it,
 * and it can generate interrupts.  Interrupts are generated for MIDI
 * input or DMA completion.  They seem to have neglected the fact 
 * that it would be nice to have a MIDI transmission complete interrupt.
 * Worse, the DMA engine is half-duplex.  This means you need to do
 * (timed) programmed I/O to be able to record and play simulataneously.
 */
#define SB_DSP_DACWRITE		0x10	/* programmed I/O write to DAC */
#define SB_DSP_WDMA		0x14	/* begin 8-bit linear DMA output */
#define SB_DSP_WDMA_2		0x16	/* begin 2-bit ADPCM DMA output */
#define SB_DSP_ADCREAD		0x20	/* programmed I/O read from ADC */
#define SB_DSP_RDMA		0x24	/* begin 8-bit linear DMA input */
#define SB_MIDI_POLL		0x30	/* initiate a polling read for MIDI */
#define SB_MIDI_READ		0x31	/* read a MIDI byte on recv intr */
#define SB_MIDI_UART_POLL	0x34	/* enter UART mode w/ read polling */
#define SB_MIDI_UART_INTR	0x35	/* enter UART mode w/ read intrs */
#define SB_MIDI_WRITE		0x38	/* write a MIDI byte (non-UART mode) */
#define SB_DSP_TIMECONST	0x40	/* set ADAC time constant */
#define SB_DSP_BLOCKSIZE	0x48	/* set blk size for high speed xfer */
#define SB_DSP_WDMA_4		0x74	/* begin 4-bit ADPCM DMA output */
#define SB_DSP_WDMA_2_6		0x76	/* begin 2.6-bit ADPCM DMA output */
#define SB_DSP_SILENCE		0x80	/* send a block of silence */
#define SB_DSP_HS_OUTPUT	0x91	/* set high speed mode for wdma */
#define SB_DSP_HS_INPUT		0x99	/* set high speed mode for rdma */
#define SB_DSP_RECORD_MONO	0xA0	/* set mono recording */
#define SB_DSP_RECORD_STEREO	0xA8	/* set stereo recording */
#define SB_DSP_HALT		0xd0	/* temporarilty suspend DMA */
#define SB_DSP_SPKR_ON		0xd1	/* turn speaker on */
#define SB_DSP_SPKR_OFF		0xd3	/* turn speaker off */
#define SB_DSP_CONT		0xd4	/* continue suspended DMA */
#define SB_DSP_RD_SPKR		0xd8	/* get speaker status */
#define 	SB_SPKR_OFF	0x00
#define 	SB_SPKR_ON	0xff
#define SB_DSP_VERSION		0xe1	/* get version number */

/*
 * The ADPCM encodings are differential, meaning each sample represents
 * a difference to add to a running sum.  The inital value is called the
 * reference, or reference byte.  Any of the ADPCM DMA transfers can specify
 * that the given transfer begins with a reference byte by or'ing
 * in the bit below.
 */
#define SB_DSP_REFERENCE	1

/*
 * Macros to detect valid hardware configuration data.
 */
#define SBP_IRQ_VALID(irq)  ((irq) == 5 || (irq) == 7 || (irq) == 9 || (irq) == 10)
#define SB_IRQ_VALID(irq)   ((irq) == 3 || (irq) == 5 || (irq) == 7 || (irq) == 9)

#define SBP_DRQ_VALID(chan) ((chan) == 0 || (chan) == 1 || (chan) == 3)
#define SB_DRQ_VALID(chan)  ((chan) == 1)

#define SB_BASE_VALID(base) ((base) == 0x220 || (base) == 0x240)

#define SB_INPUT_RATE	0
#define SB_OUTPUT_RATE	1

