/*	$OpenBSD: daadioreg.h,v 1.2 2003/06/02 18:40:59 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * This software was developed by Jason L. Wright under contract with
 * RTMX Incorporated (http://www.rtmx.com).
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
 * Register definitions for Matrix Corporation MD-DAADIO VME
 * digital/analog, analog/digitial, parallel i/o board.
 * Definitions from "MD-DAADIO User's Manual" revision B.3
 */

struct daadioregs {
	volatile u_int8_t	_unused0[32];	/* reserved */
	/* PIO module: 0x20-0x27 */
	volatile u_int8_t	pio_portb;	/* port b */
	volatile u_int8_t	pio_porta;	/* port a */
	volatile u_int8_t	pio_portd;	/* port d */
	volatile u_int8_t	pio_portc;	/* port c */
	volatile u_int8_t	pio_portf;	/* port f */
	volatile u_int8_t	pio_porte;	/* port e */
	volatile u_int8_t	_unused1[1];	/* reserved */
	volatile u_int8_t	pio_oc;		/* output control */

	volatile u_int8_t	_unused2[24];	/* reserved */

	/* DAC module: 0x40-0x4f */
	volatile u_int16_t	dac_channel[8];	/* dac channels 0-7 */

	volatile u_int8_t	_unused3[16];	/* reserved */

	/* Miscellaneous: 0x60-0x69 */
	volatile u_int8_t	_unused4;	/* reserved */
	volatile u_int8_t	gvrilr;		/* gain value/irq level */
	volatile u_int8_t	_unused5;	/* reserved */
	volatile u_int8_t	ier;		/* irq enable (wo) */
	volatile u_int8_t	_unused6;	/* reserved */
	volatile u_int8_t	pio_pattern;	/* pio pattern */
	volatile u_int8_t	_unused7;	/* reserved */
	volatile u_int8_t	sid;		/* status/id */
	volatile u_int8_t	_unused8;	/* reserved */
	volatile u_int8_t	isr;		/* interrupt status */

	volatile u_int8_t	_unused9[22];	/* reserved */

	/* ADC module: 0x80-0xff */
	volatile u_int16_t	adc12bit[32];	/* adc 12 bit channels 0-31 */
	volatile u_int16_t	adc8bit[32];	/* adc 8 bit channels 0-31 */
};

/*
 * The board occupies the space from 0 - 3ff (from some board configured
 * offset). There are four register mappings (the last three are redundant
 * mappings of the first)
 */
struct daadioboard {
	struct daadioregs	regs0;
	struct daadioregs	regs1;
	struct daadioregs	regs2;
	struct daadioregs	regs3;
};

/* gain value register/irq level register (gvr/ilr) */
#define ILR_TRIGGER		0x80		/* 0=internal,1=external */
#define	ILR_IRQ_MASK		0x70		/* IRQ level */
#define	ILR_IRQ_SHIFT		4		/* irq shift to/from lsbits */
#define	ILR_ADC_GAINMASK	0x07		/* adc gain select bits */
#define	ILR_ADC_GAIN1		0x00		/* 1x adc gain */
#define	ILR_ADC_GAIN2		0x01		/* 2x adc gain */
#define	ILR_ADC_GAIN4		0x02		/* 4x adc gain */
#define	ILR_ADC_GAIN8		0x03		/* 8x adc gain */
#define	ILR_ADC_GAIN16		0x04		/* 16x adc gain */

/* interrupt enable register (ier): WRITE ONLY */
#define	IER_MASK		0x07		/* interrupt bits */
#define	IER_PIPELINE		0x04		/* adc pipeline empty */
#define	IER_CONVERSION		0x02		/* adc conversion done */
#define	IER_PIOEVENT		0x01		/* pio event triggered */

/* interrupt status register (isr) */
#define	ISR_MASK		0x07		/* interrupt bits */
#define	ISR_PIPELINE		0x04		/* adc pipeline empty */
#define	ISR_CONVERSION		0x02		/* adc conversion done */
#define	ISR_PIOEVENT		0x01		/* pio event triggered */

/* analog/digital data register */
#define	ADC_IV			0x1000		/* invalid (out of range) */
#define	ADC_PR			0x2000		/* pipeline empty */
#define	ADC_DR			0x4000		/* data ready (valid) */
#define	ADC_OW			0x8000		/* data overwritten */
#define	ADC_DATAMASK		0x0fff		/* the actual data */

/* output control register (pio_oc) */
#define	PIOC_OCA		0x01		/* enable port A output */
#define	PIOC_OCB		0x02		/* enable port B output */
#define	PIOC_OCC		0x03		/* enable port C output */
#define	PIOC_OCD		0x04		/* enable port D output */
#define	PIOC_OCE		0x05		/* enable port E output */
#define	PIOC_OCF		0x06		/* enable port F output */

