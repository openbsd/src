/*	$OpenBSD: vars.c,v 1.13 2010/06/29 21:34:50 nicm Exp $	*/
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
	{ "beautify",	V_BOOL,
	  "be",		(char *)1 },
	{ "baudrate",	V_NUMBER|V_IREMOTE|V_INIT,
	  "ba",		(char *)&BR },
	{ "eofread",	V_STRING|V_IREMOTE|V_INIT,
	  "eofr",	(char *)&IE },
	{ "eofwrite",	V_STRING|V_IREMOTE|V_INIT,
	  "eofw",	(char *)&OE },
	{ "eol",	V_STRING|V_IREMOTE|V_INIT,
	  NULL,		(char *)&EL },
	{ "escape",	V_CHAR,
	  "es",		(char *)'~' },
	{ "exceptions",	V_STRING|V_INIT|V_IREMOTE,
	  "ex",		(char *)&EX },
	{ "force",	V_CHAR,
	  "fo",		(char *)CTRL('p') },
	{ "framesize",	V_NUMBER|V_IREMOTE|V_INIT,
	  "fr",		(char *)&FS },
	{ "host",	V_STRING|V_IREMOTE|V_INIT|V_READONLY,
	  "ho",		(char *)&HO },
	{ "log",	V_STRING|V_INIT,
	  NULL,		_PATH_ACULOG },
	{ "prompt",	V_CHAR,
	  "pr",		(char *)'\n' },
	{ "raise",	V_BOOL,
	  "ra",		(char *)0 },
	{ "raisechar",	V_CHAR,
	  "rc",		NULL },
	{ "record",	V_STRING|V_INIT|V_IREMOTE,
	  "rec",	(char *)&RE },
	{ "remote",	V_STRING|V_INIT|V_IREMOTE|V_READONLY,
	  NULL,		(char *)&RM },
	{ "script",	V_BOOL,
	  "sc",		(char *)0 },
	{ "tabexpand",	V_BOOL,
	  "tab",	(char *)0 },
	{ "verbose",	V_BOOL,
	  "verb",	(char *)1 },
	{ "SHELL",	V_STRING|V_ENVIRON|V_INIT,
	  NULL,		_PATH_BSHELL },
	{ "HOME",	V_STRING|V_ENVIRON,
	  NULL,		NULL },
	{ "echocheck",	V_BOOL,
	  "ec",		(char *)0 },
	{ "disconnect",	V_STRING|V_IREMOTE|V_INIT,
	  "di",		(char *)&DI },
	{ "tandem",	V_BOOL,
	  "ta",		(char *)1 },
	{ "linedelay",	V_NUMBER|V_IREMOTE|V_INIT,
	  "ldelay",	(char *)&DL },
	{ "chardelay",	V_NUMBER|V_IREMOTE|V_INIT,
	  "cdelay",	(char *)&CL },
	{ "etimeout",	V_NUMBER|V_IREMOTE|V_INIT,
	  "et",		(char *)&ET },
	{ "rawftp",	V_BOOL,
	  "raw",	(char *)0 },
	{ "halfduplex",	V_BOOL,
	  "hdx",	(char *)0 },
	{ "localecho",	V_BOOL,
	  "le",		(char *)0 },
	{ "parity",	V_STRING|V_INIT|V_IREMOTE,
	  "par",	(char *)&PA },
	{ "hardwareflow", V_BOOL,
	  "hf",		(char *)0 },
	{ "linedisc",	V_NUMBER|V_IREMOTE|V_INIT,
	  "ld",		(char *)&LD },
	{ "direct",	V_BOOL,
	  "dc",		(char *)0 },
	{ NULL,         0,
	  NULL,         NULL }
};
