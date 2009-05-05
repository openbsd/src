/*	$OpenBSD: cmdtab.c,v 1.26 2009/05/05 19:35:30 martynas Exp $	*/
/*	$NetBSD: cmdtab.c,v 1.17 1997/08/18 10:20:17 lukem Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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

#ifndef SMALL

#include <stdio.h>
#include "ftp_var.h"
#include "cmds.h"

/*
 * User FTP -- Command Tables.
 */

char	accounthelp[] =	"send account command to remote server";
char	appendhelp[] =	"append to a file";
char	asciihelp[] =	"set ascii transfer type";
char	beephelp[] =	"beep when command completed";
char	binaryhelp[] =	"set binary transfer type";
char	casehelp[] =	"toggle mget upper/lower case id mapping";
char	cdhelp[] =	"change remote working directory";
char	cduphelp[] =	"change remote working directory to parent directory";
char	chmodhelp[] =	"change file permissions of remote file";
char	connecthelp[] =	"connect to remote ftp server";
char	crhelp[] =	"toggle carriage return stripping on ascii gets";
char	debughelp[] =	"toggle/set debugging mode";
char	deletehelp[] =	"delete remote file";
char	dirhelp[] =	"list contents of remote directory";
char	disconhelp[] =	"terminate ftp session";
char	domachelp[] =	"execute macro";
char	edithelp[] =	"toggle command line editing";
char	epsv4help[] =	"toggle use of EPSV/EPRT on IPv4 ftp";
char	formhelp[] =	"set file transfer format";
char	gatehelp[] =	"toggle gate-ftp; specify host[:port] to change proxy";
char	globhelp[] =	"toggle metacharacter expansion of local file names";
char	hashhelp[] =	"toggle printing `#' marks; specify number to set size";
char	helphelp[] =	"print local help information";
char	idlehelp[] =	"get (set) idle timer on remote side";
char	lcdhelp[] =	"change local working directory";
char	lpwdhelp[] =	"print local working directory";
char	lshelp[] =	"list contents of remote directory";
char	macdefhelp[] =  "define a macro";
char	mdeletehelp[] =	"delete multiple files";
char	mdirhelp[] =	"list contents of multiple remote directories";
char	mgethelp[] =	"get multiple files";
char	mkdirhelp[] =	"make directory on the remote machine";
char	mlshelp[] =	"list contents of multiple remote directories";
char	modehelp[] =	"set file transfer mode";
char	modtimehelp[] = "show last modification time of remote file";
char	mputhelp[] =	"send multiple files";
char	newerhelp[] =	"get file if remote file is newer than local file ";
char	nlisthelp[] =	"nlist contents of remote directory";
char	nmaphelp[] =	"set templates for default file name mapping";
char	ntranshelp[] =	"set translation table for default file name mapping";
char	pagehelp[] =	"view a remote file through your pager";
char	passivehelp[] =	"toggle passive transfer mode";
char	porthelp[] =	"toggle use of PORT/LPRT cmd for each data connection";
char	preservehelp[] ="toggle preservation of modification time of "
			"retrieved files";
char	progresshelp[] ="toggle transfer progress meter";
char	prompthelp[] =	"toggle interactive prompting on multiple commands";
char	proxyhelp[] =	"issue command on alternate connection";
char	pwdhelp[] =	"print working directory on remote machine";
char	quithelp[] =	"terminate ftp session and exit";
char	quotehelp[] =	"send arbitrary ftp command";
char	receivehelp[] =	"receive file";
char	regethelp[] =	"get file restarting at end of local file";
char	reputhelp[] =	"put file restarting at end of remote file";
char	remotehelp[] =	"get help from remote server";
char	renamehelp[] =	"rename file";
char	resethelp[] =	"clear queued command replies";
char	restarthelp[]=	"restart file transfer at bytecount";
char	rmdirhelp[] =	"remove directory on the remote machine";
char	rmtstatushelp[]="show status of remote machine";
char	runiquehelp[] = "toggle store unique for local files";
char	sendhelp[] =	"send one file";
char	shellhelp[] =	"escape to the shell";
char	sitehelp[] =	"send site specific command to remote server\n"
			"\t\tTry \"rhelp site\" or \"site help\" "
			"for more information";
char	sizecmdhelp[] = "show size of remote file";
char	statushelp[] =	"show current status";
char	structhelp[] =	"set file transfer structure";
char	suniquehelp[] = "toggle store unique on remote machine";
char	systemhelp[] =  "show remote system type";
char	tenexhelp[] =	"set tenex file transfer type";
char	tracehelp[] =	"toggle packet tracing";
char	typehelp[] =	"set file transfer type";
char	umaskhelp[] =	"get (set) umask on remote side";
char	userhelp[] =	"send new user information";
char	verbosehelp[] =	"toggle verbose mode";

char	empty[] = "";

#define CMPL(x)	__STRING(x), 
#define CMPL0	"",
#define H(x)	x

struct cmd cmdtab[] = {
	{ "!",		H(shellhelp),	0, 0, 0, CMPL0		shell },
	{ "$",		H(domachelp),	1, 0, 0, CMPL0		domacro },
	{ "account",	H(accounthelp),	0, 1, 1, CMPL0		account},
	{ "append",	H(appendhelp),	1, 1, 1, CMPL(lr)	put },
	{ "ascii",	H(asciihelp),	0, 1, 1, CMPL0		setascii },
	{ "bell",	H(beephelp),	0, 0, 0, CMPL0		setbell },
	{ "binary",	H(binaryhelp),	0, 1, 1, CMPL0		setbinary },
	{ "bye",	H(quithelp),	0, 0, 0, CMPL0		quit },
	{ "case",	H(casehelp),	0, 0, 1, CMPL0		setcase },
	{ "cd",		H(cdhelp),	0, 1, 1, CMPL(r)	cd },
	{ "cdup",	H(cduphelp),	0, 1, 1, CMPL0		cdup },
	{ "chmod",	H(chmodhelp),	0, 1, 1, CMPL(nr)	do_chmod },
	{ "close",	H(disconhelp),	0, 1, 1, CMPL0		disconnect },
	{ "cr",		H(crhelp),	0, 0, 0, CMPL0		setcr },
	{ "debug",	H(debughelp),	0, 0, 0, CMPL0		setdebug },
	{ "delete",	H(deletehelp),	0, 1, 1, CMPL(r)	deletecmd },
	{ "dir",	H(dirhelp),	1, 1, 1, CMPL(rl)	ls },
	{ "disconnect",	H(disconhelp),	0, 1, 1, CMPL0		disconnect },
	{ "edit",	H(edithelp),	0, 0, 0, CMPL0		setedit },
	{ "epsv4",	H(epsv4help),	0, 0, 0, CMPL0		setepsv4 },
	{ "exit",	H(quithelp),	0, 0, 0, CMPL0		quit },
	{ "form",	H(formhelp),	0, 1, 1, CMPL0		setform },
	{ "ftp",	H(connecthelp),	0, 0, 1, CMPL0		setpeer },
	{ "get",	H(receivehelp),	1, 1, 1, CMPL(rl)	get },
	{ "gate",	H(gatehelp),	0, 0, 0, CMPL0		setgate },
	{ "glob",	H(globhelp),	0, 0, 0, CMPL0		setglob },
	{ "hash",	H(hashhelp),	0, 0, 0, CMPL0		sethash },
	{ "help",	H(helphelp),	0, 0, 1, CMPL(C)	help },
	{ "idle",	H(idlehelp),	0, 1, 1, CMPL0		idle },
	{ "image",	H(binaryhelp),	0, 1, 1, CMPL0		setbinary },
	{ "lcd",	H(lcdhelp),	0, 0, 0, CMPL(l)	lcd },
	{ "less",	H(pagehelp),	1, 1, 1, CMPL(r)	page },
	{ "lpwd",	H(lpwdhelp),	0, 0, 0, CMPL0		lpwd },
	{ "ls",		H(lshelp),	1, 1, 1, CMPL(rl)	ls },
	{ "macdef",	H(macdefhelp),	0, 0, 0, CMPL0		macdef },
	{ "mdelete",	H(mdeletehelp),	1, 1, 1, CMPL(R)	mdelete },
	{ "mdir",	H(mdirhelp),	1, 1, 1, CMPL(R)	mls },
	{ "mget",	H(mgethelp),	1, 1, 1, CMPL(R)	mget },
	{ "mkdir",	H(mkdirhelp),	0, 1, 1, CMPL(r)	makedir },
	{ "mls",	H(mlshelp),	1, 1, 1, CMPL(R)	mls },
	{ "mode",	H(modehelp),	0, 1, 1, CMPL0		setftmode },
	{ "modtime",	H(modtimehelp),	0, 1, 1, CMPL(r)	modtime },
	{ "more",	H(pagehelp),	1, 1, 1, CMPL(r)	page },
	{ "mput",	H(mputhelp),	1, 1, 1, CMPL(L)	mput },
	{ "msend",	H(mputhelp),	1, 1, 1, CMPL(L)	mput },
	{ "newer",	H(newerhelp),	1, 1, 1, CMPL(r)	newer },
	{ "nlist",	H(nlisthelp),	1, 1, 1, CMPL(rl)	ls },
	{ "nmap",	H(nmaphelp),	0, 0, 1, CMPL0		setnmap },
	{ "ntrans",	H(ntranshelp),	0, 0, 1, CMPL0		setntrans },
	{ "open",	H(connecthelp),	0, 0, 1, CMPL0		setpeer },
	{ "page",	H(pagehelp),	1, 1, 1, CMPL(r)	page },
	{ "passive",	H(passivehelp),	0, 0, 0, CMPL0		setpassive },
	{ "preserve",	H(preservehelp),0, 0, 0, CMPL0		setpreserve },
	{ "progress",	H(progresshelp),0, 0, 0, CMPL0		setprogress },
	{ "prompt",	H(prompthelp),	0, 0, 0, CMPL0		setprompt },
	{ "proxy",	H(proxyhelp),	0, 0, 1, CMPL(c)	doproxy },
	{ "put",	H(sendhelp),	1, 1, 1, CMPL(lr)	put },
	{ "pwd",	H(pwdhelp),	0, 1, 1, CMPL0		pwd },
	{ "quit",	H(quithelp),	0, 0, 0, CMPL0		quit },
	{ "quote",	H(quotehelp),	1, 1, 1, CMPL0		quote },
	{ "recv",	H(receivehelp),	1, 1, 1, CMPL(rl)	get },
	{ "reget",	H(regethelp),	1, 1, 1, CMPL(rl)	reget },
	{ "rename",	H(renamehelp),	0, 1, 1, CMPL(rr)	renamefile },
	{ "reput",	H(reputhelp),	1, 1, 1, CMPL(lr)	reput },
	{ "reset",	H(resethelp),	0, 1, 1, CMPL0		reset },
	{ "restart",	H(restarthelp),	1, 1, 1, CMPL0		restart },
	{ "rhelp",	H(remotehelp),	0, 1, 1, CMPL0		rmthelp },
	{ "rmdir",	H(rmdirhelp),	0, 1, 1, CMPL(r)	removedir },
	{ "rstatus",	H(rmtstatushelp),0, 1, 1, CMPL(r)	rmtstatus },
	{ "runique",	H(runiquehelp),	0, 0, 1, CMPL0		setrunique },
	{ "send",	H(sendhelp),	1, 1, 1, CMPL(lr)	put },
	{ "sendport",	H(porthelp),	0, 0, 0, CMPL0		setport },
	{ "site",	H(sitehelp),	0, 1, 1, CMPL0		site },
	{ "size",	H(sizecmdhelp),	1, 1, 1, CMPL(r)	sizecmd },
	{ "status",	H(statushelp),	0, 0, 1, CMPL0		status },
	{ "struct",	H(structhelp),	0, 1, 1, CMPL0		setstruct },
	{ "sunique",	H(suniquehelp),	0, 0, 1, CMPL0		setsunique },
	{ "system",	H(systemhelp),	0, 1, 1, CMPL0		syst },
	{ "tenex",	H(tenexhelp),	0, 1, 1, CMPL0		settenex },
	{ "trace",	H(tracehelp),	0, 0, 0, CMPL0		settrace },
	{ "type",	H(typehelp),	0, 1, 1, CMPL0		settype },
	{ "umask",	H(umaskhelp),	0, 1, 1, CMPL0		do_umask },
	{ "user",	H(userhelp),	0, 1, 1, CMPL0		user },
	{ "verbose",	H(verbosehelp),	0, 0, 0, CMPL0		setverbose },
	{ "?",		H(helphelp),	0, 0, 1, CMPL(C)	help },
	{ 0 }
};

int	NCMDS = (sizeof(cmdtab) / sizeof(cmdtab[0])) - 1;

#endif /* !SMALL */

