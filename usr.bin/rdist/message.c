/*	$OpenBSD: message.c,v 1.7 1998/06/26 21:21:15 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
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
 */

#ifndef lint
#if 0
static char RCSid[] = 
"$From: message.c,v 6.24 1996/07/19 17:00:35 michaelc Exp $";
#else
static char RCSid[] = 
"$OpenBSD: message.c,v 1.7 1998/06/26 21:21:15 millert Exp $";
#endif

static char sccsid[] = "@(#)common.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* !lint */

/*
 * Message handling functions for both rdist and rdistd.
 */

#include "defs.h"

#define MSGBUFSIZ	32*1024

int			debug = 0;		/* Debugging level */
int			nerrs = 0;		/* Number of errors */
char		       *tempfile = NULL;	/* Name of temporary file */

/*
 * Message Types
 */
MSGTYPE msgtypes[] = {
	{ MT_CHANGE,	"change" },
	{ MT_INFO,	"info" },
	{ MT_NOTICE,	"notice" },
	{ MT_NERROR,	"nerror" },
	{ MT_FERROR,	"ferror" },
	{ MT_WARNING,	"warning" },
	{ MT_VERBOSE,	"verbose" },
	{ MT_ALL,	"all" },
	{ MT_DEBUG,	"debug" },
	{ 0 },
};

static void msgsendstdout(), msgsendfile(), msgsendsyslog(), 
	msgsendnotify();

/*
 * Message Facilities
 */
MSGFACILITY msgfacility[] = {
	{ MF_STDOUT,	"stdout",	msgsendstdout },
	{ MF_FILE,	"file",		msgsendfile },
	{ MF_SYSLOG,	"syslog",	msgsendsyslog },
	{ MF_NOTIFY,	"notify",	msgsendnotify },
	{ 0 },
};

/*
 * Print message logging usage message
 */
extern void msgprusage()
{
	register int i, x;

	(void) fprintf(stderr, "\nWhere <msgopt> is of form\n");
	(void) fprintf(stderr, 
       "\t<facility1>=<type1>,<type2>,...:<facility2>=<type1>,<type2>...\n");

	(void) fprintf(stderr, "Valid <facility> names:");

	for (i = 0; msgfacility[i].mf_name; ++i)
		(void) fprintf(stderr, " %s", msgfacility[i].mf_name);

	(void) fprintf(stderr, "\nValid <type> names:");
	for (x = 0; msgtypes[x].mt_name; ++x)
		(void) fprintf(stderr, " %s", msgtypes[x].mt_name);

	(void) fprintf(stderr, "\n");
}

/*
 * Print enabled message logging info
 */
extern void msgprconfig()
{
	register int i, x;
	static char buf[MSGBUFSIZ];

	debugmsg(DM_MISC, "Current message logging config:");
	for (i = 0; msgfacility[i].mf_name; ++i) {
		(void) snprintf(buf, sizeof(buf),
				"    %s=", msgfacility[i].mf_name);
		for (x = 0; msgtypes[x].mt_name; ++x)
			if (IS_ON(msgfacility[i].mf_msgtypes, 
				  msgtypes[x].mt_type)) {
				if (x > 0)
					(void) strcat(buf, ",");
				(void) strcat(buf, msgtypes[x].mt_name);
			}
		debugmsg(DM_MISC, "%s", buf);
	}

}

/*
 * Get the Message Facility entry "name"
 */
static MSGFACILITY *getmsgfac(name)
	char *name;
{
	register int i;

	for (i = 0; msgfacility[i].mf_name; ++i)
		if (strcasecmp(name, msgfacility[i].mf_name) == 0)
			return(&msgfacility[i]);

	return(NULL);
}

/*
 * Get the Message Type entry named "name"
 */
static MSGTYPE *getmsgtype(name)
	char *name;
{
	register int i;

	for (i = 0; msgtypes[i].mt_name; ++i)
		if (strcasecmp(name, msgtypes[i].mt_name) == 0)
			return(&msgtypes[i]);

	return(NULL);
}

/*
 * Set Message Type information for Message Facility "msgfac" as
 * indicated by string "str".
 */
static char *setmsgtypes(msgfac, str)
	MSGFACILITY *msgfac;
	char *str;
{
	static char ebuf[BUFSIZ];
	register char *cp;
	register char *strptr, *word;
	register MSGTYPE *mtp;

	/*
	 * MF_SYSLOG is the only supported message facility for the server
	 */
	if (isserver && (msgfac->mf_msgfac != MF_SYSLOG && 
			 msgfac->mf_msgfac != MF_FILE)) {
		(void) snprintf(ebuf, sizeof(ebuf),
		"The \"%s\" message facility cannot be used by the server.",
				msgfac->mf_name);
		return(ebuf);
	}

	strptr = str;

	/*
	 * Do any necessary Message Facility preparation
	 */
	switch(msgfac->mf_msgfac) {
	case MF_FILE:
		/*
		 * The MF_FILE string should look like "<file>=<types>".
		 */
		if ((cp = strchr(strptr, '=')) == NULL)
			return(
			   "No file name found for \"file\" message facility");
		*cp++ = CNULL;

		if ((msgfac->mf_fptr = fopen(strptr, "w")) == NULL)
			fatalerr("Cannot open log file for writing: %s: %s.",
				 strptr, SYSERR);
		msgfac->mf_filename = strdup(strptr);

		strptr = cp;
		break;

	case MF_NOTIFY:
		break;

	case MF_STDOUT:
		msgfac->mf_fptr = stdout;
		break;

	case MF_SYSLOG:
#if defined(LOG_OPTS)
#if	defined(LOG_FACILITY)
		openlog(progname, LOG_OPTS, LOG_FACILITY);
#else
		openlog(progname, LOG_OPTS);
#endif	/* LOG_FACILITY */
#endif	/* LOG_OPTS */
		break;
	}

	/*
	 * Parse each type word
	 */
	msgfac->mf_msgtypes = 0;	/* Start from scratch */
	while (strptr) {
		word = strptr;
		if ((cp = strchr(strptr, ',')))
			*cp++ = CNULL;
		strptr = cp;

		if ((mtp = getmsgtype(word))) {
			msgfac->mf_msgtypes |= mtp->mt_type;
			/*
			 * XXX This is really a kludge until we add real
			 * control over debugging.
			 */
			if (!debug && isserver && 
			    strcasecmp(word, "debug") == 0)
				debug = DM_ALL;
		} else {
			(void) snprintf(ebuf, sizeof(ebuf),
					"Message type \"%s\" is invalid.",
					word);
			return(ebuf);
		}
	}

	return(NULL);
}

/*
 * Parse a message logging option string
 */
extern char *msgparseopts(msgstr, doset)
	char *msgstr;
	int doset;
{
	static char ebuf[BUFSIZ], msgbuf[MSGBUFSIZ];
	register char *cp, *optstr;
	register char *word;
	MSGFACILITY *msgfac;

	if (msgstr == NULL)
		return("NULL message string");

	/* strtok() is harmful */
	(void) strcpy(msgbuf, msgstr);

	/*
	 * Each <facility>=<types> list is seperated by ":".
	 */
	for (optstr = strtok(msgbuf, ":"); optstr;
	     optstr = strtok(NULL, ":")) {

		if ((cp = strchr(optstr, '=')) == NULL)
			return("No '=' found");

		*cp++ = CNULL;
		word = optstr;
		if ((int)strlen(word) <= 0)
			return("No message facility specified");
		if ((int)strlen(cp) <= 0)
			return("No message type specified");

		if ((msgfac = getmsgfac(word)) == NULL) {
			(void) snprintf(ebuf, sizeof(ebuf),
					"%s is not a valid message facility", 
					word);
			return(ebuf);
		}
		
		if (doset) {
			char *mcp;

			if ((mcp = setmsgtypes(msgfac, cp)))
				return(mcp);
		}
	}

	if (isserver && debug) {
		debugmsg(DM_MISC, "%s", getversion());
		msgprconfig();
	}

	return(NULL);
}

/*
 * Send a message to facility "stdout".
 * For rdistd, this is really the rdist client.
 */
static void msgsendstdout(msgfac, mtype, flags, msgbuf)
	/*ARGSUSED*/
	MSGFACILITY *msgfac;
	int mtype;
	int flags;
	char *msgbuf;
{
	char cmd;

	if (isserver) {
		if (rem_w < 0 || IS_ON(flags, MT_NOREMOTE))
			return;

		cmd = CNULL;

		switch(mtype) {
		case MT_NERROR:		cmd = C_ERRMSG;		break;
		case MT_FERROR:		cmd = C_FERRMSG;	break;
		case MT_NOTICE:		cmd = C_NOTEMSG;	break;
		case MT_REMOTE:		cmd = C_LOGMSG;		break;
		}

		if (cmd != CNULL)
			(void) sendcmd(cmd, "%s", msgbuf);
	} else {
		switch(mtype) {
		case MT_FERROR:
		case MT_NERROR:
			if (msgbuf && *msgbuf) {
				(void) fprintf(stderr, "%s\n", msgbuf);
				(void) fflush(stderr);
			}
			break;

		case MT_DEBUG:
			/* 
			 * Only things that are strictly MT_DEBUG should
			 * be shown.
			 */
			if (flags != MT_DEBUG)
				return;
		case MT_NOTICE:
		case MT_CHANGE:
		case MT_INFO:
		case MT_VERBOSE:
		case MT_WARNING:
			if (msgbuf && *msgbuf) {
				(void) printf("%s\n", msgbuf);
				(void) fflush(stdout);
			}
			break;
		}
	}
}

/*
 * Send a message to facility "syslog"
 */
static void msgsendsyslog(msgfac, mtype, flags, msgbuf)
	/*ARGSUSED*/
	MSGFACILITY *msgfac;
	int mtype;
	int flags;
	char *msgbuf;
{
	int syslvl = 0;

	if (!msgbuf || !*msgbuf)
		return;

	switch(mtype) {
#if	defined(SL_NERROR)
	case MT_NERROR:		syslvl = SL_NERROR;	break;
#endif
#if	defined(SL_FERROR)
	case MT_FERROR:		syslvl = SL_FERROR;	break;
#endif
#if	defined(SL_WARNING)
	case MT_WARNING:	syslvl = SL_WARNING;	break;
#endif
#if	defined(SL_CHANGE)
	case MT_CHANGE:		syslvl = SL_CHANGE;	break;
#endif
#if	defined(SL_INFO)
	case MT_SYSLOG:
	case MT_VERBOSE:
	case MT_INFO:		syslvl = SL_INFO;	break;
#endif
#if	defined(SL_NOTICE)
	case MT_NOTICE:		syslvl = SL_NOTICE;	break;
#endif
#if	defined(SL_DEBUG)
	case MT_DEBUG:		syslvl = SL_DEBUG;	break;
#endif
	}

	if (syslvl)
		syslog(syslvl, "%s", msgbuf);
}

/*
 * Send a message to a "file" facility.
 */
static void msgsendfile(msgfac, mtype, flags, msgbuf)
	/*ARGSUSED*/
	MSGFACILITY *msgfac;
	int mtype;
	int flags;
	char *msgbuf;
{
	if (msgfac->mf_fptr == NULL)
		return;

	if (!msgbuf || !*msgbuf)
		return;

	(void) fprintf(msgfac->mf_fptr, "%s\n", msgbuf);
	(void) fflush(msgfac->mf_fptr);
}

/*
 * Same method as msgsendfile()
 */
static void msgsendnotify(msgfac, mtype, flags, msgbuf)
	/*ARGSUSED*/
	MSGFACILITY *msgfac;
	int mtype;
	int flags;
	char *msgbuf;
{
	if (IS_ON(flags, MT_DEBUG))
		return;

	if (!msgbuf || !*msgbuf)
		return;

	if (!msgfac->mf_fptr) {
		register char *cp;
		int fd;
		char *getenv();

		/*
		 * Create and open a new temporary file
		 */
		if ((cp = getenv("TMPDIR")) == NULL)
			cp = _PATH_TMP;
		tempfile = (char *) xmalloc(strlen(cp) + 1 + 
					    strlen(_RDIST_TMP) + 2);
		(void) sprintf(tempfile, "%s/%s", cp, _RDIST_TMP);

		msgfac->mf_filename = tempfile;
		if ((fd = mkstemp(msgfac->mf_filename)) == -1 ||
		    (msgfac->mf_fptr = fdopen(fd, "w")) == NULL) {
			if (fd != -1)
				close(fd);
			fatalerr("Cannot open notify file for writing: %s: %s.",
			      msgfac->mf_filename, SYSERR);
		}
		debugmsg(DM_MISC, "Created notify temp file '%s'",
			 msgfac->mf_filename);
	}

	if (msgfac->mf_fptr == NULL)
		return;

	(void) fprintf(msgfac->mf_fptr, "%s\n", msgbuf);
	(void) fflush(msgfac->mf_fptr);
}

/*
 * Insure currenthost is set to something reasonable.
 */
extern void checkhostname()
{
	static char mbuf[MAXHOSTNAMELEN];
	char *cp;

	if (!currenthost) {
		if (gethostname(mbuf, sizeof(mbuf)) == 0) {
			if ((cp = strchr(mbuf, '.')) != NULL)
				*cp = CNULL;
			currenthost = strdup(mbuf);
		} else
			currenthost = "(unknown)";
	}
}

/*
 * Print a message contained in "msgbuf" if a level "lvl" is set.
 */
static void _message(flags, msgbuf)
	int flags;
	char *msgbuf;
{
	register int i, x;
	register char *cp;
	static char mbuf[2048];

	if (msgbuf && *msgbuf) {
		/*
		 * Ensure no stray newlines are present
		 */
		if ((cp = strchr(msgbuf, '\n')))
			*cp = CNULL;

		checkhostname();
		if (strncmp(currenthost, msgbuf, strlen(currenthost)) == 0)
			(void) strcpy(mbuf, msgbuf);
		else
			(void) sprintf(mbuf, "%s: %s", currenthost, msgbuf);
	} else
		(void) strcpy(mbuf, "");

	/*
	 * Special case for messages that only get
	 * logged to the system log facility
	 */
	if (IS_ON(flags, MT_SYSLOG)) {
		msgsendsyslog(NULL, MT_SYSLOG, flags, mbuf);
		return;
	}

	/*
	 * Special cases
	 */
	if (isserver && IS_ON(flags, MT_NOTICE)) {
		msgsendstdout(NULL, MT_NOTICE, flags, mbuf);
		return;
	} else if (isserver && IS_ON(flags, MT_REMOTE))
		msgsendstdout(NULL, MT_REMOTE, flags, mbuf);
	else if (isserver && IS_ON(flags, MT_NERROR))
		msgsendstdout(NULL, MT_NERROR, flags, mbuf);
	else if (isserver && IS_ON(flags, MT_FERROR))
		msgsendstdout(NULL, MT_FERROR, flags, mbuf);

	/*
	 * For each Message Facility, check each Message Type to see
	 * if the bits in "flags" are set.  If so, call the appropriate
	 * Message Facility to dispatch the message.
	 */
	for (i = 0; msgfacility[i].mf_name; ++i)
		for (x = 0; msgtypes[x].mt_name; ++x)
			/* 
			 * XXX MT_ALL should not be used directly 
			 */
			if (msgtypes[x].mt_type != MT_ALL &&
			    IS_ON(flags, msgtypes[x].mt_type) &&
			    IS_ON(msgfacility[i].mf_msgtypes,
				  msgtypes[x].mt_type))
				(*msgfacility[i].mf_sendfunc)(&msgfacility[i],
							   msgtypes[x].mt_type,
							      flags,
							      mbuf);
}

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs front-end to _message()
 */
extern void message(va_alist)
	va_dcl
{
	static char buf[MSGBUFSIZ];
	va_list args;
	char *fmt;
	int lvl;

	va_start(args);
	lvl = (int) va_arg(args, int);
	fmt = (char *) va_arg(args, char *);
	va_end(args);

	(void) vsprintf(buf, fmt, args);

	_message(lvl, buf);
}
#endif	/* ARG_VARARGS */

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg front-end to _message()
 */
extern void message(int lvl, char *fmt, ...)
{
	static char buf[MSGBUFSIZ];
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_message(lvl, buf);
}
#endif	/* ARG_STDARG */


#if	!defined(ARG_TYPE)
/*
 * Simple front-end to _message()
 */
/*VARARGS2*/
extern void message(lvl, fmt, a1, a2, a3, a4, a5)
	int lvl;
	char *fmt;
{
	static char buf[MSGBUFSIZ];

	(void) sprintf(buf, fmt, a1, a2, a3, a4, a5);

	_message(lvl, buf);
}
#endif	/* !ARG_TYPE */

/*
 * Display a debugging message
 */
static void _debugmsg(lvl, buf)
	int lvl;
	char *buf;
{
	if (IS_ON(debug, lvl))
		_message(MT_DEBUG, buf);
}

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs front-end to _debugmsg()
 */
extern void debugmsg(va_alist)
	va_dcl
{
	static char buf[MSGBUFSIZ];
	va_list args;
	char *fmt;
	int lvl;

	va_start(args);
	lvl = (int) va_arg(args, int);
	fmt = (char *) va_arg(args, char *);
	va_end(args);

	(void) vsprintf(buf, fmt, args);

	_debugmsg(lvl, buf);
}
#endif	/* ARG_VARARGS */

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg front-end to _debugmsg()
 */
extern void debugmsg(int lvl, char *fmt, ...)
{
	static char buf[MSGBUFSIZ];
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_debugmsg(lvl, buf);
}
#endif	/* ARG_STDARG */

#if	!defined(ARG_TYPE)
/*
 * Simple front-end to _debugmsg()
 */
/*VARARGS2*/
extern void debugmsg(lvl, fmt, a1, a2, a3, a4, a5)
	int lvl;
	char *fmt;
{
	static char buf[MSGBUFSIZ];

	(void) sprintf(buf, fmt, a1, a2, a3, a4, a5);

	_debugmsg(lvl, buf);
}
#endif	/* ARG_TYPE */

/*
 * Print an error message
 */
static void _error(msg)
	char *msg;
{
	static char buf[MSGBUFSIZ];

	nerrs++;
	buf[0] = CNULL;

	if (msg) {
		if (isserver)
			(void) sprintf(buf, "REMOTE ERROR: %s", msg);
		else
			(void) sprintf(buf, "LOCAL ERROR: %s", msg);
	}

	_message(MT_NERROR, (buf[0]) ? buf : NULL);
}

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs frontend to _error()
 */
extern void error(va_alist)
	va_dcl
{
	static char buf[MSGBUFSIZ];
	va_list args;
	char *fmt;

	buf[0] = CNULL;
	va_start(args);
	fmt = (char *) va_arg(args, char *);
	if (fmt)
		(void) vsprintf(buf, fmt, args);
	va_end(args);

	_error((buf[0]) ? buf : NULL);
}
#endif	/* ARG_VARARGS */

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg frontend to _error()
 */
extern void error(char *fmt, ...)
{
	static char buf[MSGBUFSIZ];
	va_list args;

	buf[0] = CNULL;
	va_start(args, fmt);
	if (fmt)
		(void) vsprintf(buf, fmt, args);
	va_end(args);

	_error((buf[0]) ? buf : NULL);
}
#endif	/* ARG_STDARG */

#if	!defined(ARG_TYPE)
/*
 * Simple frontend to _error()
 */
/*VARARGS1*/
extern void error(fmt, a1, a2, a3, a4, a5, a6)
	char *fmt;
{
	static char buf[MSGBUFSIZ];

	buf[0] = CNULL;
	if (fmt)
		(void) sprintf(buf, fmt, a1, a2, a3, a4, a5, a6);

	_error((buf[0]) ? buf : NULL);
}
#endif /* ARG_TYPE */

/*
 * Display a fatal message
 */
static void _fatalerr(msg)
	char *msg;
{
	static char buf[MSGBUFSIZ];

	++nerrs;

	if (isserver)
		(void) sprintf(buf, "REMOTE ERROR: %s", msg);
	else
		(void) sprintf(buf, "LOCAL ERROR: %s", msg);

	_message(MT_FERROR, buf);

	exit(nerrs);
}

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs front-end to _fatalerr()
 */
extern void fatalerr(va_alist)
	va_dcl
{
	static char buf[MSGBUFSIZ];
	va_list args;
	char *fmt;

	va_start(args);
	fmt = (char *) va_arg(args, char *);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_fatalerr(buf);
}
#endif	/* ARG_VARARGS */

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg front-end to _fatalerr()
 */
extern void fatalerr(char *fmt, ...)
{
	static char buf[MSGBUFSIZ];
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(buf, fmt, args);
	va_end(args);

	_fatalerr(buf);
}
#endif	/* ARG_STDARG */

#if	!defined(ARG_TYPE)
/*
 * Simple front-end to _fatalerr()
 */
/*VARARGS1*/
extern void fatalerr(fmt, a1, a2, a3, a4, a5)
	char *fmt;
{
	static char buf[MSGBUFSIZ];

	(void) sprintf(buf, fmt, a1, a2, a3, a4, a5);

	_fatalerr(buf);
}
#endif	/* !ARG_TYPE */

/*
 * Get the name of the file used for notify.
 * A side effect is that the file pointer to the file
 * is closed.  We assume this function is only called when
 * we are ready to read the file.
 */
extern char *getnotifyfile()
{
	register int i;

	for (i = 0; msgfacility[i].mf_name; i++)
		if (msgfacility[i].mf_msgfac == MF_NOTIFY &&
		    msgfacility[i].mf_fptr) {
			(void) fclose(msgfacility[i].mf_fptr);
			msgfacility[i].mf_fptr = NULL;
			return(msgfacility[i].mf_filename);
		}

	return(NULL);
}
