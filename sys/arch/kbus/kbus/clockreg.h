/*	$OpenBSD: clockreg.h,v 1.2 2000/03/03 00:54:49 todd Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)clockreg.h	8.1 (Berkeley) 6/11/93
 */

/* RTC 58321.  */
#define RTC_STOP	0x80	/* Stop RTC clock.  */
#define RTC_WRITE	0x40	/* Write access.  */
#define RTC_READ	0x20	/* Read access.  */
#define RTC_WRITE_ADDR	0x10	/* Write address.  */

#define RTC_SEC_LOW	0x00	/* RTC seconds (low nibble).  */
#define RTC_SEC_HIGH	0x01	/* RTC seconds (high nibble).  */
#define RTC_MIN_LOW	0x02	/* RTC minutes (low nibble).  */
#define RTC_MIN_HIGH	0x03	/* RTC minutes (high nibble).  */
#define RTC_HOUR_LOW	0x04	/* RTC hours (low nibble).  */
#define RTC_HOUR_HIGH	0x05	/* RTC hours (high nibble).  */
#define RTC_HOUR_24	0x08	/* RTC 24 hours mode bit.  */
#define RTC_HOUR_PM	0x04	/* RTC p.m. bit.  */
#define RTC_WEEK_DAY	0x06	/* RTC day of the week.  */
#define RTC_DAY_LOW	0x07	/* RTC day of the month (low nibble).  */
#define RTC_DAY_HIGH	0x08	/* RTC day of the month (high nibble).  */
#define RTC_MON_LOW	0x09	/* RTC month (low nibble).  */
#define RTC_MON_HIGH	0x0a	/* RTC month (high nibble).  */
#define RTC_YEAR_LOW	0x0b	/* RTC year (low nibble).  */
#define RTC_YEAR_HIGH	0x0c	/* RTC year (high nibble).  */

#define RTC_YEAR_BASE   68

