/*	$OpenBSD: util.c,v 1.31 2002/11/08 03:30:17 fgsch Exp $	*/
/*	$NetBSD: util.c,v 1.12 1997/08/18 10:20:27 lukem Exp $	*/

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
static char rcsid[] = "$OpenBSD: util.c,v 1.31 2002/11/08 03:30:17 fgsch Exp $";
#endif /* not lint */

/*
 * FTP User Program -- Misc support routines
 */
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <glob.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#include "ftp_var.h"
#include "pathnames.h"

static void updateprogressmeter(int);

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	char *host;
	char *port;

	if (connected) {
		fprintf(ttyout, "Already connected to %s, use close first.\n",
		    hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void)another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
		fprintf(ttyout, "usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	if (gatemode)
		port = gateport;
	else
		port = ftpport;
#if 0
	if (argc > 2) {
		char *ep;
		long nport;

		nport = strtol(argv[2], &ep, 10);
		if (nport < 1 || nport > USHRT_MAX || *ep != '\0') {
			fprintf(ttyout, "%s: bad port number '%s'.\n",
			    argv[1], argv[2]);
			fprintf(ttyout, "usage: %s host-name [port]\n",
			    argv[0]);
			code = -1;
			return;
		}
		port = htons((in_port_t)nport);
	}
#else
	if (argc > 2)
		port = argv[2];
#endif

	if (gatemode) {
		if (gateserver == NULL || *gateserver == '\0')
			errx(1, "gateserver not defined (shouldn't happen)");
		host = hookup(gateserver, port);
	} else
		host = hookup(argv[1], port);

	if (host) {
		int overbose;

		if (gatemode) {
			if (command("PASSERVE %s", argv[1]) != COMPLETE)
				return;
			if (verbose)
				fprintf(ttyout,
				    "Connected via pass-through server %s\n",
				    gateserver);
		}

		connected = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void)strcpy(formname, "non-print"), form = FORM_N;
		(void)strcpy(modename, "stream"), mode = MODE_S;
		(void)strcpy(structname, "file"), stru = STRU_F;
		(void)strcpy(bytename, "8"), bytesize = 8;
		/*
		 * Set type to 0 (not specified by user),
		 * meaning binary by default, but don't bother
		 * telling server.  We can use binary
		 * for text files unless changed by the user.
		 */
		(void)strcpy(typename, "binary");
		curtype = TYPE_A;
		type = 0;
		if (autologin)
			(void)ftp_login(argv[1], NULL, NULL);

#if (defined(unix) || defined(BSD)) && NBBY == 8
/*
 * this ifdef is to keep someone form "porting" this to an incompatible
 * system and not checking this out. This way they have to think about it.
 */
		overbose = verbose;
		if (debug == 0)
			verbose = -1;
		if (command("SYST") == COMPLETE && overbose) {
			char *cp, c;
			c = 0;
			cp = strchr(reply_string + 4, ' ');
			if (cp == NULL)
				cp = strchr(reply_string + 4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			fprintf(ttyout, "Remote system type is %s.\n", reply_string + 4);
			if (cp)
				*cp = c;
		}
		if (!strncmp(reply_string, "215 UNIX Type: L8", 17)) {
			if (proxy)
				unix_proxy = 1;
			else
				unix_server = 1;
			if (overbose)
				fprintf(ttyout, "Using %s mode to transfer files.\n",
				    typename);
		} else {
			if (proxy)
				unix_proxy = 0;
			else
				unix_server = 0;
			if (overbose &&
			    !strncmp(reply_string, "215 TOPS20", 10))
				fputs(
"Remember to set tenex mode when transferring binary files from this machine.\n",
				    ttyout);
		}
		verbose = overbose;
#endif /* unix || BSD */
	}
}

/*
 * login to remote host, using given username & password if supplied
 */
int
ftp_login(host, user, pass)
	const char *host;
	char *user, *pass;
{
	char tmp[80];
	char *acct;
	char anonpass[MAXLOGNAME + 1 + MAXHOSTNAMELEN];	/* "user@hostname" */
	char hostname[MAXHOSTNAMELEN];
	struct passwd *pw;
	int n, aflag = 0, retry = 0;

	acct = NULL;
	if (user == NULL) {
		if (ruserpass(host, &user, &pass, &acct) < 0) {
			code = -1;
			return (0);
		}
	}

	/*
	 * Set up arguments for an anonymous FTP session, if necessary.
	 */
	if ((user == NULL || pass == NULL) && anonftp) {
		memset(anonpass, 0, sizeof(anonpass));
		memset(hostname, 0, sizeof(hostname));

		/*
		 * Set up anonymous login password.
		 */
		if ((user = getlogin()) == NULL) {
			if ((pw = getpwuid(getuid())) == NULL)
				user = "anonymous";
			else
				user = pw->pw_name;
		}
		gethostname(hostname, sizeof(hostname));
#ifndef DONT_CHEAT_ANONPASS
		/*
		 * Every anonymous FTP server I've encountered
		 * will accept the string "username@", and will
		 * append the hostname itself.  We do this by default
		 * since many servers are picky about not having
		 * a FQDN in the anonymous password. - thorpej@netbsd.org
		 */
		snprintf(anonpass, sizeof(anonpass) - 1, "%s@",
		    user);
#else
		snprintf(anonpass, sizeof(anonpass) - 1, "%s@%s",
		    user, hp->h_name);
#endif
		pass = anonpass;
		user = "anonymous";	/* as per RFC 1635 */
	}

tryagain:
	if (retry)
		user = "ftp";		/* some servers only allow "ftp" */

	while (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL && (pw = getpwuid(getuid())) != NULL)
			myname = pw->pw_name;
		if (myname)
			fprintf(ttyout, "Name (%s:%s): ", host, myname);
		else
			fprintf(ttyout, "Name (%s): ", host);
		*tmp = '\0';
		(void)fgets(tmp, sizeof(tmp) - 1, stdin);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0')
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = getpass("Password:");
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL)
			acct = getpass("Account:");
		n = command("ACCT %s", acct);
	}
	if ((n != COMPLETE) ||
	    (!aflag && acct != NULL && command("ACCT %s", acct) != COMPLETE)) {
		warnx("Login failed.");
		if (retry || !anonftp)
			return (0);
		else
			retry = 1;
		goto tryagain;
	}
	if (proxy)
		return (1);
	connected = -1;
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void)strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

/*
 * `another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another(pargc, pargv, prompt)
	int *pargc;
	char ***pargv;
	const char *prompt;
{
	int len = strlen(line), ret;

	if (len >= sizeof(line) - 3) {
		fputs("sorry, arguments too long.\n", ttyout);
		intr();
	}
	fprintf(ttyout, "(%s) ", prompt);
	line[len++] = ' ';
	if (fgets(&line[len], (int)(sizeof(line) - len), stdin) == NULL)
		intr();
	len += strlen(&line[len]);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	makeargv();
	ret = margc > *pargc;
	*pargc = margc;
	*pargv = margv;
	return (ret);
}

/*
 * glob files given in argv[] from the remote server.
 * if errbuf isn't NULL, store error messages there instead
 * of writing to the screen.
 */
char *
remglob(argv, doswitch, errbuf)
        char *argv[];
        int doswitch;
	char **errbuf;
{
        char temp[MAXPATHLEN];
        static char buf[MAXPATHLEN];
        static FILE *ftemp = NULL;
        static char **args;
        int oldverbose, oldhash, fd;
        char *cp, *mode;

        if (!mflag) {
                if (!doglob)
                        args = NULL;
                else {
                        if (ftemp) {
                                (void)fclose(ftemp);
                                ftemp = NULL;
                        }
                }
                return (NULL);
        }
        if (!doglob) {
                if (args == NULL)
                        args = argv;
                if ((cp = *++args) == NULL)
                        args = NULL;
                return (cp);
        }
        if (ftemp == NULL) {
		int len;

		if ((cp = getenv("TMPDIR")) == NULL)
		    cp = _PATH_TMP;
		len = strlen(cp);
		if (len + sizeof(TMPFILE) + (cp[len-1] != '/') > sizeof(temp)) {
			warnx("unable to create temporary file: %s",
			    strerror(ENAMETOOLONG));
			return (NULL);
		}

		(void)strcpy(temp, cp);
		if (temp[len-1] != '/')
			temp[len++] = '/';
		(void)strcpy(&temp[len], TMPFILE);
                if ((fd = mkstemp(temp)) < 0) {
                        warn("unable to create temporary file %s", temp);
                        return (NULL);
                }
                close(fd);
		oldverbose = verbose;
		verbose = (errbuf != NULL) ? -1 : 0;
		oldhash = hash;
		hash = 0;
                if (doswitch)
                        pswitch(!proxy);
                for (mode = "w"; *++argv != NULL; mode = "a")
                        recvrequest("NLST", temp, *argv, mode, 0, 0);
		if ((code / 100) != COMPLETE) {
			if (errbuf != NULL)
				*errbuf = reply_string;
		}
		if (doswitch)
			pswitch(!proxy);
                verbose = oldverbose;
		hash = oldhash;
                ftemp = fopen(temp, "r");
                (void)unlink(temp);
                if (ftemp == NULL) {
			if (errbuf == NULL)
				fputs("can't find list of remote files, oops.\n",
				    ttyout);
			else
				*errbuf =
				    "can't find list of remote files, oops.";
                        return (NULL);
                }
        }
        if (fgets(buf, sizeof(buf), ftemp) == NULL) {
                (void)fclose(ftemp);
		ftemp = NULL;
                return (NULL);
        }
        if ((cp = strchr(buf, '\n')) != NULL)
                *cp = '\0';
        return (buf);
}

int
confirm(cmd, file)
	const char *cmd, *file;
{
	char line[BUFSIZ];

	if (!interactive || confirmrest)
		return (1);
top:
	fprintf(ttyout, "%s %s? ", cmd, file);
	(void)fflush(ttyout);
	if (fgets(line, sizeof(line), stdin) == NULL)
		return (0);
	switch (tolower(*line)) {
		case 'n':
			return (0);
		case 'p':
			interactive = 0;
			fputs("Interactive mode: off.\n", ttyout);
			break;
		case 'a':
			confirmrest = 1;
			fprintf(ttyout, "Prompting off for duration of %s.\n", cmd);
			break;
		case 'y':
			return(1);
			break;
		default:
			fprintf(ttyout, "n, y, p, a, are the only acceptable commands!\n");
			goto top;
			break;
	}
	return (1);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
int
globulize(cpp)
	char **cpp;
{
	glob_t gl;
	int flags;

	if (!doglob)
		return (1);

	flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;
	memset(&gl, 0, sizeof(gl));
	if (glob(*cpp, flags, NULL, &gl) ||
	    gl.gl_pathc == 0) {
		warnx("%s: not found", *cpp);
		globfree(&gl);
		return (0);
	}
		/* XXX: caller should check if *cpp changed, and
		 *	free(*cpp) if that is the case
		 */
	*cpp = strdup(gl.gl_pathv[0]);
	if (*cpp == NULL)
		err(1, NULL);
	globfree(&gl);
	return (1);
}

/*
 * determine size of remote file
 */
off_t
remotesize(file, noisy)
	const char *file;
	int noisy;
{
	int overbose;
	off_t size;

	overbose = verbose;
	size = -1;
	if (debug == 0)
		verbose = -1;
	if (command("SIZE %s", file) == COMPLETE) {
		char *cp, *ep;

		cp = strchr(reply_string, ' ');
		if (cp != NULL) {
			cp++;
			size = strtoq(cp, &ep, 10);
			if (*ep != '\0' && !isspace(*ep))
				size = -1;
		}
	} else if (noisy && debug == 0) {
		fputs(reply_string, ttyout);
		fputc('\n', ttyout);
	}
	verbose = overbose;
	return (size);
}

/*
 * determine last modification time (in GMT) of remote file
 */
time_t
remotemodtime(file, noisy)
	const char *file;
	int noisy;
{
	int overbose;
	time_t rtime;
	int ocode;

	overbose = verbose;
	ocode = code;
	rtime = -1;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", file) == COMPLETE) {
		struct tm timebuf;
		int yy, mo, day, hour, min, sec;
 		/*
 		 * time-val = 14DIGIT [ "." 1*DIGIT ]
 		 *		YYYYMMDDHHMMSS[.sss]
 		 * mdtm-response = "213" SP time-val CRLF / error-response
 		 */
		/* TODO: parse .sss as well, use timespecs. */
		char *timestr = reply_string;

		/* Repair `19%02d' bug on server side */
		while (!isspace(*timestr))
			timestr++;
		while (isspace(*timestr))
			timestr++;
		if (strncmp(timestr, "191", 3) == 0) {
 			fprintf(ttyout,
 	    "Y2K warning! Fixed incorrect time-val received from server.\n");
	    		timestr[0] = ' ';
			timestr[1] = '2';
			timestr[2] = '0';
		}
		sscanf(reply_string, "%*s %04d%02d%02d%02d%02d%02d", &yy, &mo,
			&day, &hour, &min, &sec);
		memset(&timebuf, 0, sizeof(timebuf));
		timebuf.tm_sec = sec;
		timebuf.tm_min = min;
		timebuf.tm_hour = hour;
		timebuf.tm_mday = day;
		timebuf.tm_mon = mo - 1;
		timebuf.tm_year = yy - TM_YEAR_BASE;
		timebuf.tm_isdst = -1;
		rtime = mktime(&timebuf);
		if (rtime == -1 && (noisy || debug != 0))
			fprintf(ttyout, "Can't convert %s to a time.\n", reply_string);
		else
			rtime += timebuf.tm_gmtoff;	/* conv. local -> GMT */
	} else if (noisy && debug == 0) {
		fputs(reply_string, ttyout);
		fputc('\n', ttyout);
	}
	verbose = overbose;
	if (rtime == -1)
		code = ocode;
	return (rtime);
}

/*
 * Returns true if this is the controlling/foreground process, else false.
 */
int
foregroundproc()
{
	static pid_t pgrp = -1;
	int ctty_pgrp;

	if (pgrp == -1)
		pgrp = getpgrp();

	return((ioctl(STDOUT_FILENO, TIOCGPGRP, &ctty_pgrp) != -1 &&
	    ctty_pgrp == pgrp));
}

static void
updateprogressmeter(dummy)
	int dummy;
{
	int save_errno = errno;

	/* update progressmeter if foreground process or in -m mode */
	if (foregroundproc() || progress == -1)
		progressmeter(0);
	errno = save_errno;
}

/*
 * Display a transfer progress bar if progress is non-zero.
 * SIGALRM is hijacked for use by this function.
 * - Before the transfer, set filesize to size of file (or -1 if unknown),
 *   and call with flag = -1. This starts the once per second timer,
 *   and a call to updateprogressmeter() upon SIGALRM.
 * - During the transfer, updateprogressmeter will call progressmeter
 *   with flag = 0
 * - After the transfer, call with flag = 1
 */
static struct timeval start;

void
progressmeter(flag)
	int flag;
{
	/*
	 * List of order of magnitude prefixes.
	 * The last is `P', as 2^64 = 16384 Petabytes
	 */
	static const char prefixes[] = " KMGTP";

	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, remaining;
	char buf[512];

	if (flag == -1) {
		(void)gettimeofday(&start, (struct timezone *)0);
		lastupdate = start;
		lastsize = restart_point;
	}
	(void)gettimeofday(&now, (struct timezone *)0);
	if (!progress || filesize < 0)
		return;
	cursize = bytes + restart_point;

	if (filesize)
		ratio = cursize * 100 / filesize;
	else
		ratio = 100;
	ratio = MAX(ratio, 0);
	ratio = MIN(ratio, 100);
	snprintf(buf, sizeof(buf), "\r%3d%% ", ratio);

	barlength = ttywidth - 30;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "|%.*s%*s|", i, 
		    "*******************************************************"
		    "*******************************************************"
		    "*******************************************************"
		    "*******************************************************"
		    "*******************************************************"
		    "*******************************************************"
		    "*******************************************************",
		    barlength - i, "");
	}

	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	    " %5lld %c%c ", (long long)abbrevsize, prefixes[i],
	    prefixes[i] == ' ' ? ' ' : 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {	/* fudge out stalled time */
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}

	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (flag == 1) {
		i = (int)elapsed / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "   ");
		i = (int)elapsed % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%02d:%02d    ", i / 60, i % 60);
	} else if (bytes <= 0 || elapsed <= 0.0 || cursize > filesize) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " - stalled -");
	} else {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%02d:%02d ETA", i / 60, i % 60);
	}
	(void)write(fileno(ttyout), buf, strlen(buf));

	if (flag == -1) {
		(void)signal(SIGALRM, updateprogressmeter);
		alarmtimer(1);		/* set alarm timer for 1 Hz */
	} else if (flag == 1) {
		alarmtimer(0);
		(void)putc('\n', ttyout);
	}
	fflush(ttyout);
}

/*
 * Display transfer statistics.
 * Requires start to be initialised by progressmeter(-1),
 * direction to be defined by xfer routines, and filesize and bytes
 * to be updated by xfer routines
 * If siginfo is nonzero, an ETA is displayed, and the output goes to STDERR
 * instead of TTYOUT.
 */
void
ptransfer(siginfo)
	int siginfo;
{
	struct timeval now, td;
	double elapsed;
	off_t bs;
	int meg, remaining, hh;
	char buf[100];

	if (!verbose && !siginfo)
		return;

	(void)gettimeofday(&now, (struct timezone *)0);
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);
	bs = bytes / (elapsed == 0.0 ? 1 : elapsed);
	meg = 0;
	if (bs > (1024 * 1024))
		meg = 1;
	(void)snprintf(buf, sizeof(buf),
	    "%lld byte%s %s in %.2f seconds (%.2f %sB/s)\n",
	    (long long)bytes, bytes == 1 ? "" : "s", direction, elapsed,
	    bs / (1024.0 * (meg ? 1024.0 : 1.0)), meg ? "M" : "K");
	if (siginfo && bytes > 0 && elapsed > 0.0 && filesize >= 0
	    && bytes + restart_point <= filesize) {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		hh = remaining / 3600;
		remaining %= 3600;
			/* "buf+len(buf) -1" to overwrite \n */
		snprintf(buf + strlen(buf) - 1, sizeof(buf) - strlen(buf),
		    "  ETA: %02d:%02d:%02d\n", hh, remaining / 60,
		    remaining % 60);
	}
	(void)write(siginfo ? STDERR_FILENO : fileno(ttyout), buf, strlen(buf));
}

/*
 * List words in stringlist, vertically arranged
 */
void
list_vertical(sl)
	StringList *sl;
{
	int i, j, w;
	int columns, width, lines, items;
	char *p;

	width = items = 0;

	for (i = 0 ; i < sl->sl_cur ; i++) {
		w = strlen(sl->sl_str[i]);
		if (w > width)
			width = w;
	}
	width = (width + 8) &~ 7;

	columns = ttywidth / width;
	if (columns == 0)
		columns = 1;
	lines = (sl->sl_cur + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			p = sl->sl_str[j * lines + i];
			if (p)
				fputs(p, ttyout);
			if (j * lines + i + lines >= sl->sl_cur) {
				putc('\n', ttyout);
				break;
			}
			w = strlen(p);
			while (w < width) {
				w = (w + 8) &~ 7;
				(void)putc('\t', ttyout);
			}
		}
	}
}

/*
 * Update the global ttywidth value, using TIOCGWINSZ.
 */
void
setttywidth(a)
	int a;
{
	int save_errno = errno;
	struct winsize winsize;

	if (ioctl(fileno(ttyout), TIOCGWINSZ, &winsize) != -1)
		ttywidth = winsize.ws_col ? winsize.ws_col : 80;
	else
		ttywidth = 80;
	errno = save_errno;
}

/*
 * Set the SIGALRM interval timer for wait seconds, 0 to disable.
 */
void
alarmtimer(wait)
	int wait;
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

/*
 * Setup or cleanup EditLine structures
 */
#ifndef SMALL
void
controlediting()
{
	if (editing && el == NULL && hist == NULL) {
		el = el_init(__progname, stdin, ttyout); /* init editline */
		hist = history_init();		/* init the builtin history */
		history(hist, H_EVENT, 100);	/* remember 100 events */
		el_set(el, EL_HIST, history, hist);	/* use history */

		el_set(el, EL_EDITOR, "emacs");	/* default editor is emacs */
		el_set(el, EL_PROMPT, prompt);	/* set the prompt function */

		/* add local file completion, bind to TAB */
		el_set(el, EL_ADDFN, "ftp-complete",
		    "Context sensitive argument completion",
		    complete);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);

		el_source(el, NULL);	/* read ~/.editrc */
		el_set(el, EL_SIGNAL, 1);
	} else if (!editing) {
		if (hist) {
			history_end(hist);
			hist = NULL;
		}
		if (el) {
			el_end(el);
			el = NULL;
		}
	}
}
#endif /* !SMALL */
