/* $NetBSD: debugconsole.c,v 1.2 1996/03/18 19:33:04 mark Exp $ */

/*
 * Copyright (c) 1994-1995 Melvyn Tang-Richardson
 * Copyright (c) 1994-1995 RiscBSD kernel team
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
 *	This product includes software developed by the RiscBSD kernel team
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE RISCBSD TEAM ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * debugconsole.c
 *
 * Console functions
 *
 * Created      : 17/09/94
 */

#ifdef DEBUGTERM

#include <sys/types.h>
#include <sys/param.h>
#include <sys/tty.h>
#include <machine/stdarg.h>
#include <machine/vconsole.h>

#define TOTTY 0x02

/*
 * This code allows the kernel developer to have a 'nice' virtual
 * console for debugging information.
 *
 * It is more useful than say, printf since the output is
 *
 * a) isolated
 *
 */

struct vconsole *debug_vc=0;
struct tty *debug_tty=0;

void
dprintf(fmt, va_alist)
	char *fmt;
{
	if ( debug_vc==0 )
		return;

	dumb_putstring ( fmt, strlen(fmt), debug_vc );
/*
	va_list *ap;

	if ( debug_tty == NULL )
		return;

	va_start(ap, fmt);
	kprintf(fmt, TOTTY, debug_tty, ap);
	va_end(ap);
*/
}

#endif

