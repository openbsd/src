/*	$OpenBSD: autoconf.h,v 1.10 2006/01/02 18:10:05 miod Exp $	*/
/*	$NetBSD: autoconf.h,v 1.5 1996/12/17 06:47:40 scottr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 */

#ifndef _MAC68K_AUTOCONF_H_
#define _MAC68K_AUTOCONF_H_

/*
 * Autoconfiguration information.
 * From sun3 port--adapted for mac68k platform by Allen Briggs.
 */

#ifdef _KERNEL
/* autoconf.c */
void	setconf(void);

/* machdep.c */
void	mac68k_set_io_offsets(vaddr_t);
void	dumpconf(void);
int	badbaddr(register caddr_t addr);
int	badwaddr(register caddr_t addr);
int	badladdr(register caddr_t addr);

/* clock.h */

void	cpu_initclocks(void);
void	setstatclockrate(int);
u_long	clkread(void);
void	inittodr(time_t);
void	resettodr(void);
void	mac68k_calibrate_delay(void);
void	startrtclock(void);

/* macrom.c */
void	mrg_init(void);
#endif	/* _KERNEL */

#endif	/* _MAC68K_AUTOCONF_H_ */
