/*	$OpenBSD: ariareg.h,v 1.2 2007/05/25 21:27:15 krw Exp $ */

/*
 * Copyright (c) 1995, 1996 Roland C. Dowdeswell.  All rights reserved.
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
 *      This product includes software developed by Roland C. Dowdeswell.
 * 4. The name of the authors may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Macros to detect valid hardware configuration data.
 */
#define ARIA_IRQ_VALID(irq)   ((irq) == 10 || (irq) == 11 || (irq) == 12)
#define ARIA_DRQ_VALID(chan)  ((chan) == 5 || (chan) == 6)
#define ARIA_BASE_VALID(base) ((base) == 0x290 || (base) == 0x280 || (base) == 0x2a0 || (base) == 0x2b0)

/*
 * Aria DSP ports
 *  (abrieviated ARIADSP_)
 */

#define	ARIADSP_NPORT		8

#define	ARIADSP_DSPDATA		0
#define ARIADSP_WRITE		0
#define	ARIADSP_STATUS		2
#define	ARIADSP_CONTROL		2
#define	ARIADSP_DMAADDRESS	4
#define	ARIADSP_DMADATA		6

/*
 * Aria DSP Addresses and the like...
 *  (abrieviated ARIAA_)
 */

#define ARIAA_HARDWARE_A	0x6050
#define	ARIAA_MODEL_A		0x60c3
#define ARIAA_PLAY_FIFO_A	0x6100
#define ARIAA_REC_FIFO_A	0x6101
#define ARIAA_TASK_A		0x6102

/*
 * DSP random values
 *  (abrieviated ARIAR_)
 */

#define ARIAR_PROMETHEUS_KLUDGE 0x0001
#define ARIAR_NPOLL		30000
#define ARIAR_OPEN_PLAY		0x0002
#define ARIAR_OPEN_RECORD	0x0001
#define ARIAR_PLAY_CHAN         1
#define ARIAR_RECORD_CHAN       0
#define ARIAR_BUSY		0x8000
#define ARIAR_ARIA_SYNTH	0x0080
#define ARIAR_SR22K             0x0040
#define ARIAR_DSPINTWR		0x0008
#define ARIAR_PCINTWR		0x0002

/*
 * Aria DSP Commands
 *  (abrieviated ARIADSPC_)
 */

#define ARIADSPC_SYSINIT	0x0000	/* Initialise system */
#define	ARIADSPC_FORMAT		0x0003	/* format (pcm8, pcm16, etc) */
#define ARIADSPC_MASTERVOLUME	0x0004
#define	ARIADSPC_BLOCKSIZE	0x0005
#define	ARIADSPC_MODE		0x0006
#define	ARIADSPC_CDVOLUME	0x0007
#define	ARIADSPC_MICVOLUME	0x0008
#define	ARIADSPC_MIXERCONFIG	0x0009
#define ARIADSPC_FORCEINTR	0x000a	/* Force an Interrupt */
#define ARIADSPC_TRANSCOMPLETE	0x0010	/* Transfer Complete */
#define ARIADSPC_START_PLAY	0x0011
#define ARIADSPC_STOP_PLAY	0x0012
#define ARIADSPC_CHAN_VOL	0x0013
#define ARIADSPC_CHAN_PAN	0x0014
#define ARIADSPC_START_REC	0x0015
#define ARIADSPC_STOP_REC	0x0016
#define ARIADSPC_DAPVOL		0x0017  /* Digital Audio Playback Vol */
#define ARIADSPC_ADCSOURCE	0x0030
#define ARIADSPC_ADCCONTROL	0x0031  /* Turn ADC off/on */
#define ARIADSPC_INPMONMODE	0x0032  /* Input Monitor Mode */
#define ARIADSPC_MASMONMODE	0x0033  /* Master Monitor Mode */
#define ARIADSPC_MIXERVOL	0x0034  /* Mixer Volumes */
#define ARIADSPC_TONE		0x0035  /* Tone controls */
#define	ARIADSPC_TERM		0xffff  /* End of Command */

/*
 * DSP values (for commands)
 *  (abrieviated ARIAV_)
 */

#define ARIAV_MODE_NO_SYNTH	0x0000	/* No synthesizer mode */

#define ARIAMIX_MIC_LVL		0
#define ARIAMIX_LINE_IN_LVL	1
#define ARIAMIX_CD_LVL		2
#define ARIAMIX_DAC_LVL		3
#define ARIAMIX_TEL_LVL		4
#define ARIAMIX_AUX_LVL		5
#define ARIAMIX_MASTER_LVL	6
#define ARIAMIX_MASTER_TREBLE	7
#define ARIAMIX_MASTER_BASS	8
#define ARIAMIX_RECORD_SOURCE	9
#define ARIAMIX_MIC_MUTE	10
#define ARIAMIX_LINE_IN_MUTE	11
#define ARIAMIX_CD_MUTE		12
#define ARIAMIX_DAC_MUTE	13
#define ARIAMIX_TEL_MUTE	14
#define ARIAMIX_AUX_MUTE	15
#define ARIAMIX_OUT_LVL		16
#define ARIAMIX_OUTPUT_CLASS	17
#define ARIAMIX_INPUT_CLASS	18
#define ARIAMIX_RECORD_CLASS	19
#define ARIAMIX_EQ_CLASS        20
