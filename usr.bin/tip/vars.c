/*	$OpenBSD: vars.c,v 1.17 2010/07/01 21:43:38 nicm Exp $	*/
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
	  "be",		NULL, 1 },
	{ "baudrate",	V_NUMBER|V_INIT,
	  "ba",		NULL, 0 },
	{ "connect",    V_STRING|V_INIT|V_READONLY,
	  "cm",         NULL, 0 },
	{ "device",     V_STRING|V_INIT|V_READONLY,
	  "dv",         NULL, 0 },
	{ "eofread",	V_STRING|V_INIT,
	  "eofr",	NULL, 0 },
	{ "eofwrite",	V_STRING|V_INIT,
	  "eofw",	NULL, 0 },
	{ "eol",	V_STRING|V_INIT,
	  NULL,		NULL, 0 },
	{ "escape",	V_CHAR,
	  "es",		NULL, '~' },
	{ "exceptions",	V_STRING|V_INIT,
	  "ex",		NULL, 0 },
	{ "force",	V_CHAR,
	  "fo",		NULL, CTRL('p') },
	{ "framesize",	V_NUMBER|V_INIT,
	  "fr",		NULL, 0 },
	{ "host",	V_STRING|V_INIT|V_READONLY,
	  "ho",		NULL, 0 },
	{ "log",	V_STRING|V_INIT,
	  NULL,		_PATH_ACULOG, 0 },
	{ "prompt",	V_CHAR,
	  "pr",		NULL, '\n' },
	{ "raise",	V_BOOL,
	  "ra",		NULL, 0 },
	{ "raisechar",	V_CHAR,
	  "rc",		NULL, 0 },
	{ "record",	V_STRING|V_INIT,
	  "rec",	NULL, 0 },
	{ "remote",	V_STRING|V_INIT|V_READONLY,
	  NULL,		NULL, 0 },
	{ "script",	V_BOOL,
	  "sc",		NULL, 0 },
	{ "tabexpand",	V_BOOL,
	  "tab",	NULL, 0 },
	{ "verbose",	V_BOOL,
	  "verb",	NULL, 1 },
	{ "SHELL",	V_STRING|V_INIT,
	  NULL,		_PATH_BSHELL, 0 },
	{ "HOME",	V_STRING|V_INIT,
	  NULL,		NULL, 0 },
	{ "echocheck",	V_BOOL,
	  "ec",		NULL, 0 },
	{ "disconnect",	V_STRING|V_INIT,
	  "di",		NULL, 0 },
	{ "tandem",	V_BOOL,
	  "ta",		NULL, 1 },
	{ "linedelay",	V_NUMBER|V_INIT,
	  "ldelay",	NULL, 0 },
	{ "chardelay",	V_NUMBER|V_INIT,
	  "cdelay",	NULL, 0 },
	{ "etimeout",	V_NUMBER|V_INIT,
	  "et",		NULL, 0 },
	{ "rawftp",	V_BOOL,
	  "raw",	NULL, 0 },
	{ "halfduplex",	V_BOOL,
	  "hdx",	NULL, 0 },
	{ "localecho",	V_BOOL,
	  "le",		NULL, 0 },
	{ "parity",	V_STRING|V_INIT,
	  "par",	NULL, 0 },
	{ "hardwareflow", V_BOOL,
	  "hf",		NULL, 0 },
	{ "linedisc",	V_NUMBER|V_INIT,
	  "ld",		NULL, 0 },
	{ "direct",	V_BOOL,
	  "dc",	        NULL, 0 },
	{ NULL,         0,
	  NULL,         NULL, 0 }
};
