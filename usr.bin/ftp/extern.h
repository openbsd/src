/*	$OpenBSD: extern.h,v 1.41 2010/06/29 23:12:33 halex Exp $	*/
/*	$NetBSD: extern.h,v 1.17 1997/08/18 10:20:19 lukem Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1994 The Regents of the University of California.
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
 *
 *	@(#)extern.h	8.3 (Berkeley) 10/9/94
 */

#include <sys/types.h>

void	abort_remote(FILE *);
void	abortpt(int);
void	abortrecv(int);
void	alarmtimer(int);
int	another(int *, char ***, const char *);
int	auto_fetch(int, char **, char *);
void	blkfree(char **);
void	cdup(int, char **);
void	cmdabort(int);
void	cmdscanner(int);
int	command(const char *, ...);
int	confirm(const char *, const char *);
FILE   *dataconn(const char *);
int	foregroundproc(void);
int	fileindir(const char *, const char *);
struct cmd *getcmd(const char *);
int	getreply(int);
int	globulize(char **);
char   *gunique(const char *);
void	help(int, char **);
char   *hookup(char *, char *);
int	initconn(void);
void	intr(void);
int	isurl(const char *);
int	ftp_login(const char *, char *, char *);
void	lostpeer(void);
void	makeargv(void);
void	progressmeter(int, const char *);
char   *prompt(void);
void	proxtrans(const char *, const char *, const char *);
void	psabort(int);
void	psummary(int);
void	pswitch(int);
void	ptransfer(int);
void	recvrequest(const char *, const char *, const char *,
	    const char *, int, int);
char   *remglob(char **, int, char **);
#ifndef SMALL
#endif /* !SMALL */
off_t	remotesize(const char *, int);
time_t	remotemodtime(const char *, int);
void	reset(int, char **);
void	rmthelp(int, char **);
void	sethash(int, char **);
void	setpeer(int, char **);
void	setttywidth(int);
char   *slurpstring(void);
void	usage(void);

#ifndef SMALL
void	abortsend(int);
unsigned char complete(EditLine *, int);
void	controlediting(void);
void	cookie_get(const char *, const char *, int, char **);
void	cookie_load(void);
void	domacro(int, char **);
void	list_vertical(StringList *);
void	parse_list(char **, char *);
char   *remglob2(char **, int, char **, FILE **ftemp, char *type);
int	ruserpass(const char *, char **, char **, char **);
void	sendrequest(const char *, const char *, const char *, int);
#endif /* !SMALL */

extern jmp_buf	abortprox;
extern int	abrtflag;
extern FILE    *cout;
extern int	data;
extern char    *home;
extern jmp_buf	jabort;
extern int	family;
extern int	proxy;
extern char	reply_string[];
extern off_t	restart_point;
extern int	keep_alive_timeout;
extern int	pipeout;

#ifndef SMALL
extern int	NCMDS;
#endif /* !SMALL */

extern char *__progname;		/* from crt0.o */

