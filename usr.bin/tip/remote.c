/*	$OpenBSD: remote.c,v 1.33 2010/08/01 20:27:51 nicm Exp $	*/
/*	$NetBSD: remote.c,v 1.5 1997/04/20 00:02:45 mellon Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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

#include <stdio.h>
#include <stdlib.h>

#include "tip.h"

static char	*db_array[3] = { _PATH_REMOTE, 0, 0 };

#define cgetflag(f)	(cgetcap(bp, f, ':') != NULL)

char *
getremote(char *host)
{
	char   *bp, *rempath, *strval;
	int	stat;
	long	val;

	rempath = getenv("REMOTE");
	if (rempath != NULL) {
		if (*rempath != '/')
			/* we have an entry */
			cgetset(rempath);
		else {	/* we have a path */
			db_array[1] = rempath;
			db_array[2] = _PATH_REMOTE;
		}
	}

	if ((stat = cgetent(&bp, db_array, host)) < 0) {
		if (vgetstr(DEVICE) != NULL ||
		    (host[0] == '/' && access(host, R_OK | W_OK) == 0)) {
			if (vgetstr(DEVICE) == NULL)
				vsetstr(DEVICE, host);
			vsetstr(HOST, host);
			if (!vgetnum(BAUDRATE))
				vsetnum(BAUDRATE, DEFBR);
			vsetnum(FRAMESIZE, DEFFS);
			return (vgetstr(DEVICE));
		}
		switch (stat) {
		case -1:
			fprintf(stderr, "%s: unknown host %s\n", __progname,
			    host);
			break;
		case -2:
			fprintf(stderr,
			    "%s: can't open host description file\n",
			    __progname);
			break;
		case -3:
			fprintf(stderr,
			    "%s: possible reference loop in host description file\n", __progname);
			break;
		}
		exit(3);
	}

	/* String options. Use if not already set. */
	if (vgetstr(DEVICE) == NULL && cgetstr(bp, "dv", &strval) >= 0)
		vsetstr(DEVICE, strval);
	if (vgetstr(CONNECT) == NULL && cgetstr(bp, "cm", &strval) >= 0)
		vsetstr(CONNECT, strval);
	if (vgetstr(DISCONNECT) == NULL && cgetstr(bp, "di", &strval) >= 0)
		vsetstr(DISCONNECT, strval);
	if (vgetstr(EOL) == NULL && cgetstr(bp, "el", &strval) >= 0)
		vsetstr(EOL, strval);
	if (vgetstr(EOFREAD) == NULL && cgetstr(bp, "ie", &strval) >= 0)
		vsetstr(EOFREAD, strval);
	if (vgetstr(EOFWRITE) == NULL && cgetstr(bp, "oe", &strval) >= 0)
		vsetstr(EOFWRITE, strval);
	if (vgetstr(EXCEPTIONS) == NULL && cgetstr(bp, "ex", &strval) >= 0)
		vsetstr(EXCEPTIONS, strval);
	if (vgetstr(RECORD) == NULL && cgetstr(bp, "re", &strval) >= 0)
		vsetstr(RECORD, strval);
	if (vgetstr(PARITY) == NULL && cgetstr(bp, "pa", &strval) >= 0)
		vsetstr(PARITY, strval);

	/* Numbers with default values. Set if currently zero (XXX ugh). */
	if (vgetnum(BAUDRATE) == 0) {
		if (cgetnum(bp, "br", &val) < 0)
			vsetnum(BAUDRATE, DEFBR);
		else
			vsetnum(BAUDRATE, val);
	}
	if (vgetnum(LINEDISC) == 0) { /* XXX relies on TTYDISC == 0 */
		if (cgetnum(bp, "ld", &val) < 0)
			vsetnum(LINEDISC, TTYDISC);
		else
			vsetnum(LINEDISC, val);
	}
	if (vgetnum(FRAMESIZE) == 0) {
		if (cgetnum(bp, "fs", &val) < 0)
			vsetnum(FRAMESIZE, DEFFS);
		else
			vsetnum(FRAMESIZE, val);
	}

	/* Numbers - default values already set in vinit() or zero. */
	if (cgetnum(bp, "es", &val) >= 0)
		vsetnum(ESCAPE, val);
	if (cgetnum(bp, "fo", &val) >= 0)
		vsetnum(FORCE, val);
	if (cgetnum(bp, "pr", &val) >= 0)
		vsetnum(PROMPT, val);
	if (cgetnum(bp, "rc", &val) >= 0)
		vsetnum(RAISECHAR, val);

	/* Numbers - default is zero. */
	if (cgetnum(bp, "dl", &val) < 0)
		vsetnum(LDELAY, 0);
	else
		vsetnum(LDELAY, val);
	if (cgetnum(bp, "cl", &val) < 0)
		vsetnum(CDELAY, 0);
	else
		vsetnum(CDELAY, val);
	if (cgetnum(bp, "et", &val) < 0)
		vsetnum(ETIMEOUT, 0);
	else
		vsetnum(ETIMEOUT, val);

	/* Flag options. */
	if (cgetflag("hd")) /* XXX overrides command line */
		vsetnum(HALFDUPLEX, 1);
	if (cgetflag("ra"))
		vsetnum(RAISE, 1);
	if (cgetflag("ec"))
		vsetnum(ECHOCHECK, 1);
	if (cgetflag("be"))
		vsetnum(BEAUTIFY, 1);
	if (cgetflag("nb"))
		vsetnum(BEAUTIFY, 0);
	if (cgetflag("sc"))
		vsetnum(SCRIPT, 1);
	if (cgetflag("tb"))
		vsetnum(TABEXPAND, 1);
	if (cgetflag("vb")) /* XXX overrides command line */
		vsetnum(VERBOSE, 1);
	if (cgetflag("nv")) /* XXX overrides command line */
		vsetnum(VERBOSE, 0);
	if (cgetflag("ta"))
		vsetnum(TAND, 1);
	if (cgetflag("nt"))
		vsetnum(TAND, 0);
	if (cgetflag("rw"))
		vsetnum(RAWFTP, 1);
	if (cgetflag("hd"))
		vsetnum(HALFDUPLEX, 1);
	if (cgetflag("dc"))
		vsetnum(DC, 1);
	if (cgetflag("hf"))
		vsetnum(HARDWAREFLOW, 1);

	if (vgetstr(RECORD) == NULL)
		vsetstr(RECORD, "tip.record");
	if (vgetstr(EXCEPTIONS) == NULL)
		vsetstr(EXCEPTIONS, "\t\n\b\f");

	vsetstr(HOST, host);
	if (vgetstr(DEVICE) == NULL) {
		fprintf(stderr, "%s: missing device spec\n", host);
		exit(3);
	}
	return (vgetstr(DEVICE));
}
