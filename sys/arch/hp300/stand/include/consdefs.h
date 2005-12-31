/*	$OpenBSD: consdefs.h,v 1.4 2005/12/31 17:59:47 miod Exp $	*/
/*	$NetBSD: consdefs.h,v 1.2 1997/05/12 07:45:41 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

/*
 * Glue for determining console select code.
 */
extern	int curcons_scode;
extern	int cons_scode;
#define	CONSCODE_INTERNAL	(-1)
#define	CONSCODE_INVALID	(-2)

/*
 * Console routine prototypes.
 */
#ifdef ITECONSOLE
void	iteprobe(struct consdev *);
void	iteinit(struct consdev *);
int	itegetchar(dev_t);
void	iteputchar(dev_t, int);
#endif
#ifdef DCACONSOLE
void	dcaprobe(struct consdev *);
void	dcainit(struct consdev *);
int	dcagetchar(dev_t);
void	dcaputchar(dev_t, int);
#endif
#ifdef APCICONSOLE
void	apciprobe(struct consdev *);
void	apciinit(struct consdev *);
int	apcigetchar(dev_t);
void	apciputchar(dev_t, int);
#endif
#ifdef DCMCONSOLE
void	dcmprobe(struct consdev *);
void	dcminit(struct consdev *);
int	dcmgetchar(dev_t);
void	dcmputchar(dev_t, int);
#endif
