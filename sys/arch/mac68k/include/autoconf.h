/*	$OpenBSD: autoconf.h,v 1.6 1997/11/30 06:10:29 gene Exp $	*/
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
void	setconf __P((void));
void	configure __P((void));

/* machdep.c */
void	mac68k_set_io_offsets __P((vm_offset_t));
void	dumpconf __P((void));
int	badbaddr __P((register caddr_t addr));
int	badwaddr __P((register caddr_t addr));
int	badladdr __P((register caddr_t addr));

/* clock.h */

void	enablertclock __P((void));
void	cpu_initclocks __P((void));
void	setstatclockrate __P((int));
void	disablertclock __P((void));
u_long	clkread __P((void));
void	inittodr __P((time_t));
void	resettodr __P((void));
void	mac68k_calibrate_delay __P((void));
void	startrtclock __P((void));

/* macrom.c */
void	mrg_init __P((void));
#endif	/* _KERNEL */

#endif	/* _MAC68K_AUTOCONF_H_ */
