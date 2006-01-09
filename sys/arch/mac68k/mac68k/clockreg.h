/*	$OpenBSD: clockreg.h,v 1.7 2006/01/09 22:59:35 miod Exp $	*/
/*	$NetBSD: clockreg.h,v 1.5 1996/04/01 05:16:52 scottr Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 *  Clock defines and things.
 *  MacII clock characteristics used.
 */

/*
 * Calculate clocks needed to hit HZ ticks/sec.
 *
 * The VIA clock speed is 1.2766us, so the timer value needed is:
 *
 *                    1       1,000,000us     1
 *  CLK_INTERVAL = -------- * ----------- * ------
 *                 1.2766us       1s          HZ
 *
 * While it may be tempting to simplify the following further, we can run
 * into integer overflow problems.  Also note:  do *not* define HZ to be
 * less than 12; overflow will occur, yielding invalid results.
 */
#define CLK_INTERVAL	((int)((((100000000L / HZ) * 100) / 12766)))

#define CLK_INTH	((CLK_INTERVAL >> 8) & 0xff)	/* high byte */
#define CLK_INTL	(CLK_INTERVAL & 0xff)		/* low byte */
