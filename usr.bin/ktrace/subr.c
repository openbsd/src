/*	$OpenBSD: subr.c,v 1.13 2016/07/18 09:36:50 guenther Exp $	*/
/*	$NetBSD: subr.c,v 1.6 1995/08/31 23:01:45 jtc Exp $	*/

/*-
 * Copyright (c) 1988, 1993
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

#include <sys/param.h>	/* MAXCOMLEN */
#include <sys/file.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/ktrace.h>

#include <stdio.h>

#include "ktrace.h"
#include "extern.h"

/*
 * If you change the trace point letters, then update to match:
 * ktrace/ktrace.1, ktrace/ltrace.1, kdump/kdump.1, and
 * usage() in kdump/kdump.c
 */
int
getpoints(const char *s, int defpoints)
{
	int facs = 0;

	while (*s) {
		switch(*s) {
		case 'c':
			facs |= KTRFAC_SYSCALL | KTRFAC_SYSRET;
			break;
		case 'i':
			facs |= KTRFAC_GENIO;
			break;
		case 'n':
			facs |= KTRFAC_NAMEI;
			break;
		case 'p':
			facs |= KTRFAC_PLEDGE;
			break;
		case 's':
			facs |= KTRFAC_PSIG;
			break;
		case 't':
			facs |= KTRFAC_STRUCT;
			break;
		case 'u':
			facs |= KTRFAC_USER;
			break;
		case 'x':
			facs |= KTRFAC_EXECARGS;
			break;
		case 'X':
			facs |= KTRFAC_EXECENV;
			break;
		case '+':
			facs |= defpoints;
			break;
		default:
			return (-1);
		}
		s++;
	}
	return (facs);
}
