/*	$OpenBSD: extern.h,v 1.24 2002/11/08 03:30:17 fgsch Exp $	*/
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
 *	@(#)extern.h	8.3 (Berkeley) 10/9/94
 */

#include <sys/types.h>

void    abort_remote(FILE *);
void    abortpt(int);
void    abortrecv(int);
void    abortsend(int);
void	account(int, char **);
void	alarmtimer(int);
int	another(int *, char ***, const char *);
int	auto_fetch(int, char **, char *);
void	blkfree(char **);
void	cd(int, char **);
void	cdup(int, char **);
void	changetype(int, int);
void	cmdabort(int);
void	cmdscanner(int);
int	command(const char *, ...);
#ifndef SMALL
unsigned char complete(EditLine *, int);
void	controlediting(void);
#endif /* !SMALL */
int	confirm(const char *, const char *);
FILE   *dataconn(const char *);
void	delete(int, char **);
void	disconnect(int, char **);
void	do_chmod(int, char **);
void	do_umask(int, char **);
void	domacro(int, char **);
char   *domap(char *);
void	doproxy(int, char **);
char   *dotrans(char *);
int     empty(fd_set *, int);
int	foregroundproc(void);
void	get(int, char **);
struct cmd *getcmd(const char *);
int	getit(int, char **, int, const char *);
int	getreply(int);
int	globulize(char **);
char   *gunique(const char *);
void	help(int, char **);
char   *hookup(char *, char *);
void	idle(int, char **);
int     initconn(void);
void	intr(void);
int	isurl(const char *);
void	list_vertical(StringList *);
void	lcd(int, char **);
int	ftp_login(const char *, char *, char *);
void	lostpeer(void);
void	lpwd(int, char **);
void	ls(int, char **);
void	mabort(int);
void	macdef(int, char **);
void	makeargv(void);
void	makedir(int, char **);
void	mdelete(int, char **);
void	mget(int, char **);
void	mls(int, char **);
void	modtime(int, char **);
void	mput(int, char **);
char   *onoff(int);
void	newer(int, char **);
void	page(int, char **);
void    progressmeter(int);
char   *prompt(void);
void	proxabort(int);
void    proxtrans(const char *, const char *, const char *);
void    psabort(int);
void	psummary(int);
void    pswitch(int);
void    ptransfer(int);
void	put(int, char **);
void	pwd(int, char **);
void	quit(int, char **);
void	quote(int, char **);
void	quote1(const char *, int, char **);
void    recvrequest(const char *, const char *, const char *,
	    const char *, int, int);
void	reget(int, char **);
char   *remglob(char **, int, char **);
off_t	remotesize(const char *, int);
time_t	remotemodtime(const char *, int);
void	removedir(int, char **);
void	renamefile(int, char **);
void    reset(int, char **);
void	restart(int, char **);
void	rmthelp(int, char **);
void	rmtstatus(int, char **);
int	ruserpass(const char *, char **, char **, char **);
void    sendrequest(const char *, const char *, const char *, int);
void	setascii(int, char **);
void	setbell(int, char **);
void	setbinary(int, char **);
void	setcase(int, char **);
void	setcr(int, char **);
void	setdebug(int, char **);
void	setedit(int, char **);
void	setepsv4(int, char **);
void	setform(int, char **);
void	setftmode(int, char **);
void	setgate(int, char **);
void	setglob(int, char **);
void	sethash(int, char **);
void	setnmap(int, char **);
void	setntrans(int, char **);
void	setpassive(int, char **);
void	setpeer(int, char **);
void	setport(int, char **);
void	setpreserve(int, char **);
void	setprogress(int, char **);
void	setprompt(int, char **);
void	setrunique(int, char **);
void	setstruct(int, char **);
void	setsunique(int, char **);
void	settenex(int, char **);
void	settrace(int, char **);
void	setttywidth(int);
void	settype(int, char **);
void	setverbose(int, char **);
void	shell(int, char **);
void	site(int, char **);
void	sizecmd(int, char **);
char   *slurpstring(void);
void	status(int, char **);
void	syst(int, char **);
int	togglevar(int, char **, int *, const char *);
void	usage(void);
void	user(int, char **);


extern jmp_buf	abortprox;
extern int	abrtflag;
extern struct	cmd cmdtab[];
extern FILE    *cout;
extern int	data;
extern char    *home;
extern jmp_buf	jabort;
extern int	family;
extern int	proxy;
extern char	reply_string[];
extern off_t	restart_point;
extern int	NCMDS;

extern char *__progname;		/* from crt0.o */

