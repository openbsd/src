/*-
 * Copyright (c) 1998,1999 Alex Nash
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$OpenBSD: wdt50x.h,v 1.1 1999/04/28 23:21:05 alex Exp $
 */

#ifndef WDT_H
#define WDT_H

#include <sys/ioccom.h>

struct wdt_state {
	int temperature;	/* temperature in fahrenheit	*/
	int status;		/* see WDT_SR_xxx flags		*/
};

#define	WDT_SR_TEMP_GOOD	0x02	/* temperature good (active high) */
#define WDT_SR_ISO_IN0		0x04	/* isolated input #0 */
#define WDT_SR_ISO_IN1		0x08	/* isolated input #1 */
#define WDT_SR_FAN		0x10	/* fan good (active low) */
#define WDT_SR_PS_OVER		0x20	/* power overvoltage (active low) */
#define WDT_SR_PS_UNDER		0x40	/* power undervoltage (active low) */

#define	WIOCSCHED		_IO('w', 1)
#define WIOCGETSTATE		_IOR('r', 2, struct wdt_state)

#endif /* WDT_H */
