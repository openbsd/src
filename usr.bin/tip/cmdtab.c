/*	$OpenBSD: cmdtab.c,v 1.10 2010/06/29 16:44:38 nicm Exp $	*/
/*	$NetBSD: cmdtab.c,v 1.3 1994/12/08 09:30:46 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include "tip.h"

esctable_t etable[] = {
	{ '!',		"shell",				shell },
	{ '<',		"receive file from remote host",	getfl },
	{ '>',		"send file to remote host",		sendfile },
	{ 't',		"take file from remote UNIX",		cu_take },
	{ 'p',		"put file to remote UNIX",		cu_put },
	{ '|',		"pipe remote file",			pipefile },
	{ '$',		"pipe local command to remote host",	pipeout },
	{ 'C',  	"connect program to remote host",	consh },
	{ 'c',		"change directory",		 	chdirectory },
	{ '.',		"exit from tip",			finish },
	{ CTRL('d'),	"exit from tip",			finish },
	{ CTRL('y'),	"suspend tip (local+remote)",		suspend },
	{ CTRL('z'),	"suspend tip (local only)",		suspend },
	{ 's',		"set variable",				variable },
	{ 'v',		"list variables",			listvariables },
	{ '?',		"get this summary",			help },
	{ '#',		"send break",				genbrk },
	{ 0, 0, 0 }
};
