/* $NetBSD: io.h,v 1.3 1996/03/28 21:28:21 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * io.h
 *
 * IO registers
 *
 * Created      : 10/10/94
 */

/* Some of these addresses are frightening and need cleaning up */

/*
 * The podule addresses should be removed and localised for the podules.
 * This is difficuly as the podule addresses are interleaved with the
 * other IO devices thus making it difficult to separate them.
 */
 
#define IO_CONF_BASE			0xf6000000

#define IO_HW_BASE			0x03000000

#define IO_BASE				0xf6200000

#define COMBO_BASE			0xf6210000

#define IDE_CONTROLLER_BASE		0xf62107c0

#define FLOPPY_CONTROLLER_BASE		0xf6210fc0

#define FLOPPY_DACK			0x00002000

#define SERIAL0_CONTROLLER_BASE		0xf6210fe0

#define SERIAL1_CONTROLLER_BASE		0xf6210be0

#define PARALLEL_CONTROLLER_BASE	0xf62109e0

#define IO_MOUSE_BUTTONS		0xf6010000

#ifdef RC7500

#define IDE_CONTROLLER_BASE2		0xf622B000

/*
 * a bit low turns attached LED on
 */
#define LEDPORT	(IO_BASE + 0x0002B060)
#define LED0	0x01
#define LED1	0x02
#define LED2	0x04
#define LED3	0x08
#define LED4	0x10
#define LED5	0x20
#define LED6	0x40
#define LED7	0x80
#define LEDOFF	0x00
#define LEDALL	0xFF
#endif


#define EASI_HW_BASE		0x08000000
#define EASI_BASE		0xf8000000
#define EASI_SIZE		0x01000000

#define SIMPLE_PODULE_SIZE	0x00004000

#define MOD_PODULE_BASE		0xf6200000
#define SYNC_PODULE_BASE	0xf63c0000
#define SYNC_PODULE_HW_BASE	0x033c0000
#define FAST_PODULE_BASE	0xf6340000
#define MEDIUM_PODULE_BASE	0xf60c0000
#define SLOW_PODULE_BASE	0xf6040000

#define PODULE_GAP		0x00020000
#define MAX_PODULES		8

#define NETSLOT_BASE		0xf622b000
#define MAX_NETSLOTS		1

#define MOUSE_BUTTON_RIGHT	0x10
#define MOUSE_BUTTON_MIDDLE	0x20
#define MOUSE_BUTTON_LEFT	0x40

/* End of io.h */
