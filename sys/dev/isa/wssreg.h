/*	$OpenBSD: wssreg.h,v 1.4 2000/03/28 14:07:43 espie Exp $	*/
/*	$NetBSD: wssreg.h,v 1.3 1995/07/07 02:15:15 brezak Exp $	*/

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
 */
/*
 * Copyright (c) 1993 Analog Devices Inc. All rights reserved
 */

/*
 * Macros to detect valid hardware configuration data.
 */
#define WSS_IRQ_VALID(irq)   ((irq) == 5 || (irq) ==	7 || (irq) ==  9 || \
			     (irq) == 10 || (irq) == 11)
#define WSS_DRQ_VALID(chan)  ((chan) == 0 || (chan) == 1 || (chan) == 3)
#define WSS_BASE_VALID(base) ((base) == 0x0530 || \
			      (base) == 0x0604 || \
			      (base) == 0x0e80 || \
			      (base) == 0x0f40)

/* WSS registers */
#define WSS_CONFIG	0x00	/* write only */
#define WSS_STATUS	0x03	/* read only */
#define WSS_CODEC	0x04	/* ad1848 codec registers (0x04-0x07) */
#define WSS_NPORT	8

/* WSS status register bits */
#define WSS_16SLOT	0x80
#define WSS_IRQACTIVE	0x40
#define WSS_VERS	0x04
#define WSS_VERSMASK	0x3f
