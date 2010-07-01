/*	$OpenBSD: remote.c,v 1.27 2010/07/01 21:28:01 nicm Exp $	*/
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

#include "pathnames.h"
#include "tip.h"

static char	*db_array[3] = { _PATH_REMOTE, 0, 0 };

#define cgetflag(f)	(cgetcap(bp, f, ':') != NULL)

static void	getremcap(char *);

static void
getremcap(char *host)
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
			vsetstr(DEVICE, host);
			vsetstr(HOST, host);
			if (!vgetnum(BAUDRATE))
				vsetnum(BAUDRATE, DEFBR);
			vsetnum(FRAMESIZE, DEFFS);
			return;
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

	if (cgetstr(bp, "dv", &strval) < 0)
		strval = NULL;
	vsetstr(DEVICE, strval);
	if (cgetstr(bp, "cm", &strval) < 0)
		strval = NULL;
	vsetstr(CONNECT, strval);
	if (cgetstr(bp, "di", &strval) < 0)
		strval = NULL;
	vsetstr(DISCONNECT, strval);
	if (cgetstr(bp, "el", &strval) < 0)
		strval = NULL;
	vsetstr(EOL, strval);
	if (cgetstr(bp, "ie", &strval) < 0)
		strval = NULL;
	vsetstr(EOFREAD, strval);
	if (cgetstr(bp, "oe", &strval) < 0)
		strval = NULL;
	vsetstr(EOFWRITE, strval);
	if (cgetstr(bp, "ex", &strval) < 0)
		strval = NULL;
	vsetstr(EXCEPTIONS, strval);
	if (cgetstr(bp, "re", &strval) < 0)
		strval = NULL;
	vsetstr(RECORD, strval);
	if (cgetstr(bp, "pa", &strval) < 0)
		strval = NULL;
	vsetstr(PARITY, strval);

	if (cgetstr(bp, "es", &strval) >= 0 && strval != NULL)
		vstring("es", strval);
	if (cgetstr(bp, "fo", &strval) >= 0 && strval != NULL)
		vstring("fo", strval);
	if (cgetstr(bp, "pr", &strval) >= 0 && strval != NULL)
		vstring("pr", strval);
	if (cgetstr(bp, "rc", &strval) >= 0 && strval != NULL)
		vstring("rc", strval);

	if (!vgetnum(BAUDRATE)) {
		if (cgetnum(bp, "br", &val) == -1)
			vsetnum(BAUDRATE, DEFBR);
		else
			vsetnum(BAUDRATE, val);
	}
	if (!vgetnum(LINEDISC)) {
		if (cgetnum(bp, "ld", &val) == -1)
			vsetnum(LINEDISC, TTYDISC);
		else
			vsetnum(LINEDISC, val);
	}
	if (cgetnum(bp, "fs", &val) == -1)
		vsetnum(FRAMESIZE, DEFFS);
	else
		vsetnum(FRAMESIZE, val);
	if (vgetstr(DEVICE) == NULL) {
		fprintf(stderr, "%s: missing device spec\n", host);
		exit(3);
	}

	vsetstr(HOST, host);
	if (cgetflag("hd"))
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
	if (cgetflag("vb"))
		vsetnum(VERBOSE, 1);
	if (cgetflag("nv"))
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
	if (cgetnum(bp, "dl", &val) == -1)
		vsetnum(LDELAY, 0);
	else
		vsetnum(LDELAY, val);
	if (cgetnum(bp, "cl", &val) == -1)
		vsetnum(CDELAY, 0);
	else
		vsetnum(CDELAY, val);
	if (cgetnum(bp, "et", &val) == -1)
		vsetnum(ETIMEOUT, 0);
	else
		vsetnum(ETIMEOUT, val);
}

char *
getremote(char *host)
{
	char *cp;
	static char *next;
	static int lookedup = 0;

	if (!lookedup) {
		if (host == NULL && (host = getenv("HOST")) == NULL) {
			fprintf(stderr, "%s: no host specified\n", __progname);
			exit(3);
		}
		getremcap(host);
		next = vgetstr(DEVICE);
		lookedup++;
	}
	/*
	 * We return a new device each time we're called (to allow
	 *   a rotary action to be simulated)
	 */
	if (next == NULL)
		return (NULL);
	if ((cp = strchr(next, ',')) == NULL) {
		vsetstr(DEVICE, next);
		next = NULL;
	} else {
		*cp++ = '\0';
		vsetstr(DEVICE, next);
		next = cp;
	}
	return (vgetstr(DEVICE));
}
