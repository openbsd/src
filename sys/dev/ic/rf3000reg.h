/* $NetBSD */

/*
 * Copyright (c) 2004 David Young.  All rights reserved.
 *
 * This code was written by David Young.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_RF3000REG_H_
#define	_DEV_IC_RF3000REG_H_

/*
 * Registers for RF Microdevices RF3000 spread-spectrum baseband modem.
 */
#define RF3000_CTL		0x01		/* modem control */
#define RF3000_RXSTAT		RF3000_CTL	/* RX status */
#define		RF3000_CTL_MODE_MASK		BITS(7, 4)
#define		RF3000_RXSTAT_ACQ		BIT(2)
#define		RF3000_RXSTAT_SFD		BIT(1)
#define		RF3000_RXSTAT_CRC		BIT(0)
#define RF3000_CCACTL		0x02		/* CCA control */
/* CCA mode */
#define		RF3000_CCACTL_MODE_MASK		BITS(7, 6)
#define		RF3000_CCACTL_MODE_RSSIT	0	/* RSSI threshold */
#define		RF3000_CCACTL_MODE_ACQ		1	/* acquisition */
#define		RF3000_CCACTL_MODE_BOTH		2	/* threshold or acq. */
/* RSSI threshold for CCA */
#define		RF3000_CCACTL_RSSIT_MASK	BITS(5, 0)
#define RF3000_DIVCTL		0x03		/* diversity control */
#define		RF3000_DIVCTL_ENABLE		BIT(7)	/* enable diversity */
#define		RF3000_DIVCTL_ANTSEL		BIT(6)	/* if ENABLE = 0, set
							 * ANT SEL
							 */
#define RF3000_RSSI		RF3000_DIVCTL	/* RSSI value */
#define		RF3000_RSSI_MASK		BITS(5, 0)
#define RF3000_GAINCTL		0x11		/* TX variable gain control */
#define		RF3000_GAINCTL_TXVGC_MASK	BITS(7, 2)
#define		RF3000_GAINCTL_SCRAMBLER	BIT(1)
#define	RF3000_LOGAINCAL	0x14		/* low gain calibration */
#define		RF3000_LOGAINCAL_CAL_MASK	BITS(5, 0)
#define	RF3000_HIGAINCAL	0x15		/* high gain calibration */
#define		RF3000_HIGAINCAL_CAL_MASK	BITS(5, 0)
#define		RF3000_HIGAINCAL_DSSSPAD	BIT(6)	/* 6dB gain pad for DSSS
							 * modes (meaning?)
							 */
#define RF3000_OPTIONS1		0x1C		/* Options Register 1 */
/* Saturation threshold is 4 + offset, where -3 <= offset <= 3.
 * SAT_THRESH is the absolute value, SAT_THRESH_SIGN is the sign.
 */
#define		RF3000_OPTIONS1_SAT_THRESH_SIGN	BIT(7)
#define		RF3000_OPTIONS1_SAT_THRESH	BITS(6,5)
#define		RF3000_OPTIONS1_ALTAGC		BIT(4)	/* 1: retrigger AGC
 							 * algorithm on ADC
 							 * saturation
							 */
#define		RF3000_OPTIONS1_ALTBUS		BIT(3)	/* 1: enable alternate
							 * Tx/Rx data bus
							 * interface.
							 */
#define		RF3000_OPTIONS1_RESERVED0_MASK	BITS(2,0)/* 0 */

#define RF3000_OPTIONS2		0x1D		/* Options Register 2 */
/* 1: delay next AGC 2us instead of 1us after a 1->0 LNAGS-pin transition. */
#define		RF3000_OPTIONS2_LNAGS_DELAY	BIT(7)
#define		RF3000_OPTIONS2_RESERVED0_MASK	BITS(6,3)	/* 0 */
/* Threshold for AGC re-trigger. 0: high count, 1: low count. */
#define		RF3000_OPTIONS2_RTG_THRESH	BIT(2)
#define		RF3000_OPTIONS2_RESERVED1_MASK	BITS(1,0)	/* 0 */

#endif /* _DEV_IC_RF3000REG_H_ */
