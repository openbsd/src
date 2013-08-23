/*	$OpenBSD: ofw_machdep.h,v 1.5 2013/08/23 08:52:24 mpi Exp $	*/

/*
 * Copyright (c) 2002, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Various console variables...
 */
extern int cons_height, cons_width, cons_linebytes, cons_depth;
extern u_int32_t cons_addr;
extern int cons_backlight_available;

void ofwconprobe(void);
void ofwconsinit(void);

/*
 * For some reason, setting the brightness under 0x29 from OF switches the
 * backlight off, and it won't be switched on again until you set the
 * brightness above 0x33.  All hail hysteresis! -- miod
 */
#define	MIN_BRIGHTNESS		0x34
#define	MAX_BRIGHTNESS		0xff
#define	STEP_BRIGHTNESS		8
#define	DEFAULT_BRIGHTNESS	0x80
extern int cons_brightness;

void of_setbacklight(int);
void of_setbrightness(int);
void of_setcolors(unsigned int, unsigned int, uint8_t *, uint8_t *, uint8_t *);
