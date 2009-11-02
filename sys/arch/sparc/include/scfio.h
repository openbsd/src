/*	$OpenBSD: scfio.h,v 1.3 2009/11/02 22:31:50 sobrado Exp $	*/

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
 * ioctls and flags for sysconfig registers on Force CPU-5V boards.
 */

/* led1/led2 */
#define	SCF_LED_COLOR_MASK	0x03	/* color bits */
#define	SCF_LED_COLOR_OFF	0x00	/* led off */
#define	SCF_LED_COLOR_GREEN	0x01	/* green led */
#define	SCF_LED_COLOR_RED	0x02	/* red led */
#define	SCF_LED_COLOR_YELLOW	0x03	/* yellow led */
#define	SCF_LED_BLINK_MASK	0x0c	/* blink bits */
#define	SCF_LED_BLINK_NONE	0x00	/* steady led */
#define	SCF_LED_BLINK_HALF	0x04	/* blink 1/2 Hz */
#define	SCF_LED_BLINK_ONE	0x08	/* blink 1 Hz */
#define	SCF_LED_BLINK_TWO	0x0c	/* blink 2 Hz */

/* 7 segment led */
#define	SCF_7LED_A		0x01	/* Layout:	*/
#define	SCF_7LED_B		0x02	/*	AAA	*/
#define	SCF_7LED_C		0x04	/*     FF BB	*/
#define	SCF_7LED_D		0x08	/*	GGG	*/
#define	SCF_7LED_E		0x10	/*     EE CC	*/
#define	SCF_7LED_F		0x20	/*      DDD  P	*/
#define	SCF_7LED_G		0x40
#define	SCF_7LED_P		0x80

/* flash memory control */
#define	SCF_FMCTRL_SELROM	0x01	/* select boot/user flash */
#define	SCF_FMCTRL_SELBOOT	0x02	/* select 1st/2nd flash */
#define	SCF_FMCTRL_WRITEV	0x04	/* turn on write voltage */
#define	SCF_FMCTRL_SELADDR	0x38	/* address 21:19 bits */

#define	SCFIOCSLED1	_IOW('S', 0x01, u_int8_t)	/* set led1 */
#define	SCFIOCGLED1	_IOR('S', 0x02, u_int8_t)	/* get led1 */
#define	SCFIOCSLED2	_IOW('S', 0x03, u_int8_t)	/* set led2 */
#define	SCFIOCGLED2	_IOR('S', 0x04, u_int8_t)	/* get led2 */
#define	SCFIOCSLED7	_IOW('S', 0x05, u_int8_t)	/* set 7-segment led */
#define	SCFIOCGLED7	_IOW('S', 0x06, u_int8_t)	/* get 7-segment led */
#define	SCFIOCGROT	_IOR('S', 0x07, u_int8_t)	/* get rotary sw */
#define	SCFIOCSFMCTRL	_IOW('S', 0x08, u_int8_t)	/* set flash ctrl */
#define	SCFIOCGFMCTRL	_IOR('S', 0x09, u_int8_t)	/* get flash ctrl */
