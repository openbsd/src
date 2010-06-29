/*	$OpenBSD: vars.c,v 1.12 2010/06/29 20:57:33 nicm Exp $	*/
/*	$NetBSD: vars.c,v 1.3 1994/12/08 09:31:19 jtc Exp $	*/

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
#include "pathnames.h"

/*
 * Definition of variables
 */
value_t vtable[] = {
	{ "beautify",	BOOL,			0,
	  "be",		(char *)TRUE },
	{ "baudrate",	NUMBER|IREMOTE|INIT,	0,
	  "ba",		(char *)&BR },
	{ "eofread",	STRING|IREMOTE|INIT,	0,
	  "eofr",	(char *)&IE },
	{ "eofwrite",	STRING|IREMOTE|INIT,	0,
	  "eofw",	(char *)&OE },
	{ "eol",	STRING|IREMOTE|INIT,	0,
	  NULL,		(char *)&EL },
	{ "escape",	CHAR,			0,
	  "es",		(char *)'~' },
	{ "exceptions",	STRING|INIT|IREMOTE,	0,
	  "ex",		(char *)&EX },
	{ "force",	CHAR,			0,
	  "fo",		(char *)CTRL('p') },
	{ "framesize",	NUMBER|IREMOTE|INIT,	0,
	  "fr",		(char *)&FS },
	{ "host",	STRING|IREMOTE|INIT,	READONLY,
	  "ho",		(char *)&HO },
	{ "log",	STRING|INIT,		0,
	  NULL,		_PATH_ACULOG },
	{ "prompt",	CHAR,			0,
	  "pr",		(char *)'\n' },
	{ "raise",	BOOL,			0,
	  "ra",		(char *)FALSE },
	{ "raisechar",	CHAR,			0,
	  "rc",		NULL },
	{ "record",	STRING|INIT|IREMOTE,	0,
	  "rec",	(char *)&RE },
	{ "remote",	STRING|INIT|IREMOTE,	READONLY,
	  NULL,		(char *)&RM },
	{ "script",	BOOL,			0,
	  "sc",		(char *)FALSE },
	{ "tabexpand",	BOOL,			0,
	  "tab",	(char *)FALSE },
	{ "verbose",	BOOL,			0,
	  "verb",	(char *)TRUE },
	{ "SHELL",	STRING|ENVIRON|INIT,	0,
	  NULL,		_PATH_BSHELL },
	{ "HOME",	STRING|ENVIRON,		0,
	  NULL,		NULL },
	{ "echocheck",	BOOL,			0,
	  "ec",		(char *)FALSE },
	{ "disconnect",	STRING|IREMOTE|INIT,	0,
	  "di",		(char *)&DI },
	{ "tandem",	BOOL,			0,
	  "ta",		(char *)TRUE },
	{ "linedelay",	NUMBER|IREMOTE|INIT,	0,
	  "ldelay",	(char *)&DL },
	{ "chardelay",	NUMBER|IREMOTE|INIT,	0,
	  "cdelay",	(char *)&CL },
	{ "etimeout",	NUMBER|IREMOTE|INIT,	0,
	  "et",		(char *)&ET },
	{ "rawftp",	BOOL,			0,
	  "raw",	(char *)FALSE },
	{ "halfduplex",	BOOL,			0,
	  "hdx",	(char *)FALSE },
	{ "localecho",	BOOL,			0,
	  "le",		(char *)FALSE },
	{ "parity",	STRING|INIT|IREMOTE,	0,
	  "par",	(char *)&PA },
	{ "hardwareflow", BOOL,			0,
	  "hf",		(char *)FALSE },
	{ "linedisc",	NUMBER|IREMOTE|INIT,	0,
	  "ld",		(char *)&LD },
	{ "direct",	BOOL,			0,
	  "dc",		(char *)FALSE },
	{ NULL, NULL, NULL, NULL, NULL }
};
