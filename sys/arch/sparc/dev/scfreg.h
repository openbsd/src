/*	$OpenBSD: scfreg.h,v 1.2 2003/06/02 18:40:59 jason Exp $	*/

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

struct scf_regs {
	volatile u_int8_t	led1;		/* user led 1 control */
	volatile u_int8_t	led2;		/* user led 2 control */
	volatile u_int8_t	fmpcr1;		/* flash prog control 1 */
	volatile u_int8_t	rssr;		/* rotary switch status */
	volatile u_int8_t	_reserved0[4];	/* unused */
	volatile u_int8_t	brscr;		/* boot ROM size ctrl */
	volatile u_int8_t	fmpcr2;		/* flash prog control reg 2 */
	volatile u_int8_t	fmpvcr;		/* flash prog voltage ctrl */
	volatile u_int8_t	ssldcr;		/* 7-segment led ctrl */
	volatile u_int8_t	fmb0;		/* FMB chan 0 data discard */
	volatile u_int8_t	fmb1;		/* FMB chan 1 data discard */
	volatile u_int8_t	_reserved1[1];	/* unused */
	volatile u_int8_t	lcaid;		/* LCA identification */
};

/* led1/led2: user led 1/led 2 control */
#define	LED_MASK		0x0f
#define	LED_COLOR_MASK		0x03	/* color mask */
#define	LED_COLOR_OFF		0x00	/* led is off */
#define	LED_COLOR_GREEN		0x01	/* led is green */
#define	LED_COLOR_RED		0x02	/* led is red */
#define	LED_COLOR_YELLOW	0x03	/* led is yellow */
#define	LED_BLINK_MASK		0x0c	/* blink mask */
#define	LED_BLINK_NONE		0x00	/* led does not blink */
#define	LED_BLINK_HALF		0x04	/* led blinks at 0.5hz */
#define	LED_BLINK_ONE		0x08	/* led blinks at 1.0hz */
#define	LED_BLINK_TWO		0x0c	/* led blinks at 2.0hz */

/* fmpcr1: flash memory programming control register 1 */
#define	FMPCR1_MASK		0xf0	/* must be or'd with this on write */
#define	FMPCR1_SELADDR		0x0e	/* address select */
#define	FMPCR1_SELROM		0x01	/* 0=first,1=second flash memory */

/* rssr: rotary switch status register */
#define	RSSR_MASK		0x0f	/* value of user rotary switch */

/* brscr: boot ROM size control register */
/* ??? */

/* fmpcr2: flash memory programming control register 2 */
#define	FMPCR2_MASK		0xfe	/* must be or'd with this on write */
#define	FMPCR2_SELBOOT		0x01	/* 0=USER,1=BOOT flash memory */

/* fmpvcr: flash memory programming voltage control register */
#define	FMPVCR_MASK		0xfe	/* must be or'd with this on write */
#define	FMPVCR_VPP		0x01	/* 1=prog voltage on, 0 = off */

/* fmb0/fmb1: FMB channel 0/1 data discard status register */
#define	FMB_MSBVALID	0x01		/* (ro) whether to discard FMB data */

/* ssldcr: seven segment LED display control register */
/*
 * Layout:
 *          AAA
 *         FF BB
 *	    GGG
 *	   EE CC
 *	    DDD  P
 */
#define	SSLDCR_A	0x01
#define	SSLDCR_B	0x02
#define	SSLDCR_C	0x04
#define	SSLDCR_D	0x08
#define	SSLDCR_E	0x10
#define	SSLDCR_F	0x20
#define	SSLDCR_G	0x40
#define	SSLDCR_P	0x80
