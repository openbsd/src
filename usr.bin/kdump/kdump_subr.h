/*	$OpenBSD: kdump_subr.h,v 1.22 2018/11/05 17:05:50 anton Exp $	*/
/*
 * Copyright(c) 2006 2006 David Kirchner <dpk@dpk.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $FreeBSD: src/usr.bin/kdump/kdump_subr.h,v 1.3 2007/04/09 22:04:27 emaste Exp $ */


/*
 * These are simple support macros. print_or utilizes a variable
 * defined in the calling function to track whether or not it should
 * print a logical-OR character ('|') before a string. if_print_or
 * simply handles the necessary "if" statement used in many lines
 * of this file.
 */
#define print_or(str,orflag) do {                  \
	if (orflag) putchar('|'); else orflag = 1; \
	printf ("%s", str); }                      \
	while (0)
#define if_print_or(i,flag,orflag) do {            \
	if ((i & flag) == flag)                    \
	print_or(#flag,orflag); }                  \
	while (0)

void fcntlcmdname(int);
void rtprioname(int);
void modename(int);
void doflagsname(int, int);
void flagsname(int);
void openflagsname(int);
void atflagsname(int);
void accessmodename(int);
void mmapprotname(int);
void mmapflagsname(int);
void wait4optname(int);
void sendrecvflagsname(int);
void getfsstatflagsname(int);
void mountflagsname(int);
void rebootoptname(int);
void flockname(int);
void sockoptname(int);
void sockdomainname(int);
void sockipprotoname(int);
void socktypename(int);
void sockflagsname(int);
void sockfamilyname(int);
void thrcreateflagsname(int);
void mlockallname(int);
void shmatname(int);
void nfssvcname(int);
void whencename(int);
void pathconfname(int);
void rlimitname(int);
void shutdownhowname(int);
void prioname(int);
void madvisebehavname(int);
void msyncflagsname(int);
void clocktypename(int);
void schedpolicyname(int);
void kldunloadfflagsname(int);
void ksethrcmdname(int);
void extattrctlname(int);
void kldsymcmdname(int);
void sendfileflagsname(int);
void acltypename(int);
void rusagewho(int);
void sigactionflagname(int);
void sigprocmaskhowname(int);
void lio_listioname(int);
void minheritname(int);
void quotactlname(int);
void quotactlcmdname(int);
void ptraceopname(int);
void sigill_name(int);
void sigtrap_name(int);
void sigemt_name(int);
void sigfpe_name(int);
void sigbus_name(int);
void sigsegv_name(int);
void sigchld_name(int);
void ktracefacname(int);
void itimername(int);
void evfiltername(int);
void evflagsname(int);
void evfflagsname(int, int);
void pollfdeventname(int);
void syslogflagname(int);
void futexflagname(int);
void flocktypename(int);

extern int decimal, fancy, basecol, arg1;
