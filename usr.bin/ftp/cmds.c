/*	$OpenBSD: cmds.c,v 1.74 2015/01/30 04:45:45 tedu Exp $	*/
/*	$NetBSD: cmds.c,v 1.27 1997/08/18 10:20:15 lukem Exp $	*/

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

/*
 * FTP User Program -- Command Routines.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <fnmatch.h>
#include <glob.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ftp_var.h"
#include "pathnames.h"
#include "cmds.h"

/*
 * Set ascii transfer type.
 */
/*ARGSUSED*/
void
setascii(int argc, char *argv[])
{

	stype[1] = "ascii";
	settype(2, stype);
}

/*
 * Set file transfer mode.
 */
/*ARGSUSED*/
void
setftmode(int argc, char *argv[])
{

	fprintf(ttyout, "We only support %s mode, sorry.\n", modename);
	code = -1;
}

/*
 * Set file transfer format.
 */
/*ARGSUSED*/
void
setform(int argc, char *argv[])
{

	fprintf(ttyout, "We only support %s format, sorry.\n", formname);
	code = -1;
}

/*
 * Set file transfer structure.
 */
/*ARGSUSED*/
void
setstruct(int argc, char *argv[])
{

	fprintf(ttyout, "We only support %s structure, sorry.\n", structname);
	code = -1;
}

void
reput(int argc, char *argv[])
{

	(void)putit(argc, argv, 1);
}

void
put(int argc, char *argv[])
{
 
	(void)putit(argc, argv, 0);
}

/*
 * Send a single file.
 */
void
putit(int argc, char *argv[], int restartit)
{
	char *cmd;
	int loc = 0;
	char *oldargv1, *oldargv2;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2 && !another(&argc, &argv, "local-file"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "remote-file")) || argc > 3) {
usage:
		fprintf(ttyout, "usage: %s local-file [remote-file]\n",
		    argv[0]);
		code = -1;
		return;
	}
	oldargv1 = argv[1];
	oldargv2 = argv[2];
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	/*
	 * If "globulize" modifies argv[1], and argv[2] is a copy of
	 * the old argv[1], make it a copy of the new argv[1].
	 */
	if (argv[1] != oldargv1 && argv[2] == oldargv1) {
		argv[2] = argv[1];
	}
	if (restartit == 1) {
		if (curtype != type)
			changetype(type, 0);
		restart_point = remotesize(argv[2], 1);
		if (restart_point < 0) {
			restart_point = 0;
			code = -1;
			return;
		}
	}
	if (strcmp(argv[0], "append") == 0) {
		restartit = 1;
	}
	cmd = restartit ? "APPE" : ((sunique) ? "STOU" : "STOR");
	if (loc && ntflag) {
		argv[2] = dotrans(argv[2]);
	}
	if (loc && mapflag) {
		argv[2] = domap(argv[2]);
	}
	sendrequest(cmd, argv[1], argv[2],
	    argv[1] != oldargv1 || argv[2] != oldargv2);
	restart_point = 0;
	if (oldargv1 != argv[1])	/* free up after globulize() */
		free(argv[1]);
}

/*
 * Send multiple files.
 */
void
mput(int argc, char *argv[])
{
	extern int optind, optreset;
	int ch, i, restartit = 0;
	sig_t oldintr;
	char *cmd, *tp, *xargv[] = { argv[0], NULL, NULL };
	const char *errstr;
	static int depth = 0, max_depth = 0;

	optind = optreset = 1;

	if (depth)
		depth++;

	while ((ch = getopt(argc, argv, "cd:r")) != -1) {
		switch(ch) {
		case 'c':
			restartit = 1;
			break;
		case 'd':
			max_depth = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				fprintf(ttyout, "bad depth value, %s: %s\n",
				    errstr, optarg);
				code = -1;
				return;
			}
			break;
		case 'r':
			depth = 1;
			break;
		default:
			goto usage;
		}
	}

	if (argc - optind < 1 && !another(&argc, &argv, "local-files")) {
usage:
		fprintf(ttyout, "usage: %s [-cr] [-d depth] local-files\n",
		    argv[0]);
		code = -1;
		return;
	}

	argv[optind - 1] = argv[0];
	argc -= optind - 1;
	argv += optind - 1;

	mname = argv[0];
	mflag = 1;

	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	if (proxy) {
		char *cp, *tp2, tmpbuf[PATH_MAX];

		while ((cp = remglob(argv, 0, NULL)) != NULL) {
			if (*cp == '\0') {
				mflag = 0;
				continue;
			}
			if (mflag && confirm(argv[0], cp)) {
				tp = cp;
				if (mcase) {
					while (*tp && !islower(*tp)) {
						tp++;
					}
					if (!*tp) {
						tp = cp;
						tp2 = tmpbuf;
						while ((*tp2 = *tp) != '\0') {
						     if (isupper(*tp2)) {
							    *tp2 =
								tolower(*tp2);
						     }
						     tp++;
						     tp2++;
						}
					}
					tp = tmpbuf;
				}
				if (ntflag) {
					tp = dotrans(tp);
				}
				if (mapflag) {
					tp = domap(tp);
				}
				if (restartit == 1) {
					off_t ret;

					if (curtype != type)
						changetype(type, 0);
					ret = remotesize(tp, 0);
					restart_point = (ret < 0) ? 0 : ret;
				}
				cmd = restartit ? "APPE" : ((sunique) ?
				    "STOU" : "STOR");
				sendrequest(cmd, cp, tp,
				    cp != tp || !interactive);
				restart_point = 0;
				if (!mflag && fromatty) {
					if (confirm(argv[0], NULL))
						mflag = 1;
				}
			}
		}
		(void)signal(SIGINT, oldintr);
		mflag = 0;
		return;
	}

	for (i = 1; i < argc; i++) {
		char **cpp;
		glob_t gl;
		int flags;

		/* Copy files without word expansion */
		if (!doglob) {
			if (mflag && confirm(argv[0], argv[i])) {
				tp = (ntflag) ? dotrans(argv[i]) : argv[i];
				tp = (mapflag) ? domap(tp) : tp;
				if (restartit == 1) {
					off_t ret;

					if (curtype != type)
						changetype(type, 0);
					ret = remotesize(tp, 0);
					restart_point = (ret < 0) ? 0 : ret;
				}
				cmd = restartit ? "APPE" : ((sunique) ?
				    "STOU" : "STOR");
				sendrequest(cmd, argv[i], tp,
				    tp != argv[i] || !interactive);
				restart_point = 0;
				if (!mflag && fromatty) {
					if (confirm(argv[0], NULL))
						mflag = 1;
				}
			}
			continue;
		}

		/* expanding file names */
		memset(&gl, 0, sizeof(gl));
		flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;
		if (glob(argv[i], flags, NULL, &gl) || gl.gl_pathc == 0) {
			warnx("%s: not found", argv[i]);
			globfree(&gl);
			continue;
		}

		/* traverse all expanded file names */
		for (cpp = gl.gl_pathv; cpp && *cpp != NULL; cpp++) {
			struct stat filestat;

			if (!mflag)
				continue;
			if (stat(*cpp, &filestat) != 0) {
				warn("local: %s", *cpp);
				continue;
			}
			if (S_ISDIR(filestat.st_mode) && depth == max_depth)
				continue;
			if (!confirm(argv[0], *cpp))
				continue;

			/*
			 * If file is a directory then create a new one
			 * at the remote machine.
			 */
			if (S_ISDIR(filestat.st_mode)) {
				xargv[1] = *cpp;
				makedir(2, xargv);
				cd(2, xargv);
				if (dirchange != 1) {
					warnx("remote: %s", *cpp);
					continue;
				}

				if (chdir(*cpp) != 0) {
					warn("local: %s", *cpp);
					goto out;
				}

				/* Copy the whole directory recursively. */
				xargv[1] = "*";
				mput(2, xargv);

				if (chdir("..") != 0) {
					mflag = 0;
					warn("local: %s", *cpp);
					goto out;
				}

 out:
				xargv[1] = "..";
				cd(2, xargv);
				if (dirchange != 1) {
					warnx("remote: %s", *cpp);
					mflag = 0;
				}
				continue;
			}

			tp = (ntflag) ? dotrans(*cpp) : *cpp;
			tp = (mapflag) ? domap(tp) : tp;
			if (restartit == 1) {
				off_t ret;

				if (curtype != type)
					changetype(type, 0);
				ret = remotesize(tp, 0);
				restart_point = (ret < 0) ? 0 : ret;
			}
			cmd = restartit ? "APPE" : ((sunique) ?
			    "STOU" : "STOR");
			sendrequest(cmd, *cpp, tp,
			    *cpp != tp || !interactive);
			restart_point = 0;
			if (!mflag && fromatty) {
				if (confirm(argv[0], NULL))
					mflag = 1;
			}
		}
		globfree(&gl);
	}

	(void)signal(SIGINT, oldintr);

	if (depth)
		depth--;
	if (depth == 0 || mflag == 0)
		depth = max_depth = mflag = 0;
}

void
reget(int argc, char *argv[])
{

	(void)getit(argc, argv, 1, "a+w");
}

char *
onoff(int bool)
{

	return (bool ? "on" : "off");
}

/*
 * Show status.
 */
/*ARGSUSED*/
void
status(int argc, char *argv[])
{
	int i;

	if (connected)
		fprintf(ttyout, "Connected %sto %s.\n",
		    connected == -1 ? "and logged in" : "", hostname);
	else
		fputs("Not connected.\n", ttyout);
	if (!proxy) {
		pswitch(1);
		if (connected) {
			fprintf(ttyout, "Connected for proxy commands to %s.\n",
			    hostname);
		}
		else {
			fputs("No proxy connection.\n", ttyout);
		}
		pswitch(0);
	}
	fprintf(ttyout, "Gate ftp: %s, server %s, port %s.\n", onoff(gatemode),
	    *gateserver ? gateserver : "(none)", gateport);
	fprintf(ttyout, "Passive mode: %s.\n", onoff(passivemode));
	fprintf(ttyout, "Mode: %s; Type: %s; Form: %s; Structure: %s.\n",
		modename, typename, formname, structname);
	fprintf(ttyout, "Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s.\n",
		onoff(verbose), onoff(bell), onoff(interactive),
		onoff(doglob));
	fprintf(ttyout, "Store unique: %s; Receive unique: %s.\n", onoff(sunique),
		onoff(runique));
	fprintf(ttyout, "Preserve modification times: %s.\n", onoff(preserve));
	fprintf(ttyout, "Case: %s; CR stripping: %s.\n", onoff(mcase), onoff(crflag));
	if (ntflag) {
		fprintf(ttyout, "Ntrans: (in) %s (out) %s\n", ntin, ntout);
	}
	else {
		fputs("Ntrans: off.\n", ttyout);
	}
	if (mapflag) {
		fprintf(ttyout, "Nmap: (in) %s (out) %s\n", mapin, mapout);
	}
	else {
		fputs("Nmap: off.\n", ttyout);
	}
	fprintf(ttyout, "Hash mark printing: %s; Mark count: %d; Progress bar: %s.\n",
	    onoff(hash), mark, onoff(progress));
	fprintf(ttyout, "Use of PORT/LPRT cmds: %s.\n", onoff(sendport));
	fprintf(ttyout, "Use of EPSV/EPRT cmds for IPv4: %s%s.\n", onoff(epsv4),
	    epsv4bad ? " (disabled for this connection)" : "");
	fprintf(ttyout, "Command line editing: %s.\n", onoff(editing));
	if (macnum > 0) {
		fputs("Macros:\n", ttyout);
		for (i=0; i<macnum; i++) {
			fprintf(ttyout, "\t%s\n", macros[i].mac_name);
		}
	}
	code = 0;
}

/*
 * Toggle a variable
 */
int
togglevar(int argc, char *argv[], int *var, const char *mesg)
{
	if (argc < 2) {
		*var = !*var;
	} else if (argc == 2 && strcasecmp(argv[1], "on") == 0) {
		*var = 1;
	} else if (argc == 2 && strcasecmp(argv[1], "off") == 0) {
		*var = 0;
	} else {
		fprintf(ttyout, "usage: %s [on | off]\n", argv[0]);
		return (-1);
	}
	if (mesg)
		fprintf(ttyout, "%s %s.\n", mesg, onoff(*var));
	return (*var);
}

/*
 * Set beep on cmd completed mode.
 */
/*ARGSUSED*/
void
setbell(int argc, char *argv[])
{

	code = togglevar(argc, argv, &bell, "Bell mode");
}

/*
 * Set command line editing
 */
/*ARGSUSED*/
void
setedit(int argc, char *argv[])
{

	code = togglevar(argc, argv, &editing, "Editing mode");
	controlediting();
}

/*
 * Toggle use of IPv4 EPSV/EPRT
 */
/*ARGSUSED*/
void
setepsv4(int argc, char *argv[])
{

	code = togglevar(argc, argv, &epsv4, "EPSV/EPRT on IPv4");
	epsv4bad = 0;
}

/*
 * Turn on packet tracing.
 */
/*ARGSUSED*/
void
settrace(int argc, char *argv[])
{

	code = togglevar(argc, argv, &trace, "Packet tracing");
}

/*
 * Toggle hash mark printing during transfers, or set hash mark bytecount.
 */
/*ARGSUSED*/
void
sethash(int argc, char *argv[])
{
	if (argc == 1)
		hash = !hash;
	else if (argc != 2) {
		fprintf(ttyout, "usage: %s [on | off | size]\n", argv[0]);
		code = -1;
		return;
	} else if (strcasecmp(argv[1], "on") == 0)
		hash = 1;
	else if (strcasecmp(argv[1], "off") == 0)
		hash = 0;
	else {
		int nmark;
		const char *errstr;

		nmark = strtonum(argv[1], 1, INT_MAX, &errstr);
		if (errstr) {
			fprintf(ttyout, "bytecount value is %s: %s\n",
			    errstr, argv[1]);
			code = -1;
			return;
		}
		mark = nmark;
		hash = 1;
	}
	fprintf(ttyout, "Hash mark printing %s", onoff(hash));
	if (hash)
		fprintf(ttyout, " (%d bytes/hash mark)", mark);
	fputs(".\n", ttyout);
	code = hash;
}

/*
 * Turn on printing of server echo's.
 */
/*ARGSUSED*/
void
setverbose(int argc, char *argv[])
{

	code = togglevar(argc, argv, &verbose, "Verbose mode");
}

/*
 * Toggle PORT/LPRT cmd use before each data connection.
 */
/*ARGSUSED*/
void
setport(int argc, char *argv[])
{

	code = togglevar(argc, argv, &sendport, "Use of PORT/LPRT cmds");
}

/*
 * Toggle transfer progress bar.
 */
/*ARGSUSED*/
void
setprogress(int argc, char *argv[])
{

	code = togglevar(argc, argv, &progress, "Progress bar");
}

/*
 * Turn on interactive prompting during mget, mput, and mdelete.
 */
/*ARGSUSED*/
void
setprompt(int argc, char *argv[])
{

	code = togglevar(argc, argv, &interactive, "Interactive mode");
}

/*
 * Toggle gate-ftp mode, or set gate-ftp server
 */
/*ARGSUSED*/
void
setgate(int argc, char *argv[])
{
	static char gsbuf[HOST_NAME_MAX+1];

	if (argc > 3) {
		fprintf(ttyout, "usage: %s [on | off | host [port]]\n",
		    argv[0]);
		code = -1;
		return;
	} else if (argc < 2) {
		gatemode = !gatemode;
	} else {
		if (argc == 2 && strcasecmp(argv[1], "on") == 0)
			gatemode = 1;
		else if (argc == 2 && strcasecmp(argv[1], "off") == 0)
			gatemode = 0;
		else {
			if (argc == 3) {
				gateport = strdup(argv[2]);
				if (gateport == NULL)
					err(1, NULL);
			}
			strlcpy(gsbuf, argv[1], sizeof(gsbuf));
			gateserver = gsbuf;
			gatemode = 1;
		}
	}
	if (gatemode && (gateserver == NULL || *gateserver == '\0')) {
		fprintf(ttyout,
		    "Disabling gate-ftp mode - no gate-ftp server defined.\n");
		gatemode = 0;
	} else {
		fprintf(ttyout, "Gate ftp: %s, server %s, port %s.\n",
		    onoff(gatemode),
		    *gateserver ? gateserver : "(none)", gateport);
	}
	code = gatemode;
}

/*
 * Toggle metacharacter interpretation on local file names.
 */
/*ARGSUSED*/
void
setglob(int argc, char *argv[])
{

	code = togglevar(argc, argv, &doglob, "Globbing");
}

/*
 * Toggle preserving modification times on retrieved files.
 */
/*ARGSUSED*/
void
setpreserve(int argc, char *argv[])
{

	code = togglevar(argc, argv, &preserve, "Preserve modification times");
}

/*
 * Set debugging mode on/off and/or set level of debugging.
 */
/*ARGSUSED*/
void
setdebug(int argc, char *argv[])
{
	if (argc > 2) {
		fprintf(ttyout, "usage: %s [on | off | debuglevel]\n", argv[0]);
		code = -1;
		return;
	} else if (argc == 2) {
		if (strcasecmp(argv[1], "on") == 0)
			debug = 1;
		else if (strcasecmp(argv[1], "off") == 0)
			debug = 0;
		else {
			const char *errstr;
			int val;

			val = strtonum(argv[1], 0, INT_MAX, &errstr);
			if (errstr) {
				fprintf(ttyout, "debugging value is %s: %s\n",
				    errstr, argv[1]);
				code = -1;
				return;
			}
			debug = val;
		}
	} else
		debug = !debug;
	if (debug)
		options |= SO_DEBUG;
	else
		options &= ~SO_DEBUG;
	fprintf(ttyout, "Debugging %s (debug=%d).\n", onoff(debug), debug);
	code = debug > 0;
}

/*
 * Set current working directory on local machine.
 */
void
lcd(int argc, char *argv[])
{
	char buf[PATH_MAX];
	char *oldargv1;

	if (argc < 2)
		argc++, argv[1] = home;
	if (argc != 2) {
		fprintf(ttyout, "usage: %s [local-directory]\n", argv[0]);
		code = -1;
		return;
	}
	oldargv1 = argv[1];
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	if (chdir(argv[1]) < 0) {
		warn("local: %s", argv[1]);
		code = -1;
	} else {
		if (getcwd(buf, sizeof(buf)) != NULL)
			fprintf(ttyout, "Local directory now %s\n", buf);
		else
			warn("getcwd: %s", argv[1]);
		code = 0;
	}
	if (oldargv1 != argv[1])	/* free up after globulize() */
		free(argv[1]);
}

/*
 * Delete a single file.
 */
void
deletecmd(int argc, char *argv[])
{

	if ((argc < 2 && !another(&argc, &argv, "remote-file")) || argc > 2) {
		fprintf(ttyout, "usage: %s remote-file\n", argv[0]);
		code = -1;
		return;
	}
	(void)command("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
void
mdelete(int argc, char *argv[])
{
	sig_t oldintr;
	char *cp;

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		fprintf(ttyout, "usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	while ((cp = remglob(argv, 0, NULL)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			(void)command("DELE %s", cp);
			if (!mflag && fromatty) {
				if (confirm(argv[0], NULL))
					mflag = 1;
			}
		}
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Rename a remote file.
 */
void
renamefile(int argc, char *argv[])
{

	if (argc < 2 && !another(&argc, &argv, "from-name"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "to-name")) || argc > 3) {
usage:
		fprintf(ttyout, "usage: %s from-name to-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RNFR %s", argv[1]) == CONTINUE)
		(void)command("RNTO %s", argv[2]);
}

/*
 * Get a directory listing of remote files.
 */
void
ls(int argc, char *argv[])
{
	const char *cmd;
	char *oldargv2, *globargv2;

	if (argc < 2)
		argc++, argv[1] = NULL;
	if (argc < 3)
		argc++, argv[2] = "-";
	if (argc > 3) {
		fprintf(ttyout, "usage: %s [remote-directory [local-file]]\n",
		    argv[0]);
		code = -1;
		return;
	}
	cmd = strcmp(argv[0], "nlist") == 0 ? "NLST" : "LIST";
	oldargv2 = argv[2];
	if (strcmp(argv[2], "-") && !globulize(&argv[2])) {
		code = -1;
		return;
	}
	globargv2 = argv[2];
	if (strcmp(argv[2], "-") && *argv[2] != '|' && (!globulize(&argv[2]) ||
	    !confirm("output to local-file:", argv[2]))) {
		code = -1;
		goto freels;
	}
	recvrequest(cmd, argv[2], argv[1], "w", 0, 0);

	/* flush results in case commands are coming from a pipe */
	fflush(ttyout);
freels:
	if (argv[2] != globargv2)		/* free up after globulize() */
		free(argv[2]);
	if (globargv2 != oldargv2)
		free(globargv2);
}

/*
 * Get a directory listing of multiple remote files.
 */
void
mls(int argc, char *argv[])
{
	sig_t oldintr;
	int i;
	char lmode[1], *dest, *odest;

	if (argc < 2 && !another(&argc, &argv, "remote-files"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "local-file")) {
usage:
		fprintf(ttyout, "usage: %s remote-files local-file\n", argv[0]);
		code = -1;
		return;
	}
	odest = dest = argv[argc - 1];
	argv[argc - 1] = NULL;
	if (strcmp(dest, "-") && *dest != '|')
		if (!globulize(&dest) ||
		    !confirm("output to local-file:", dest)) {
			code = -1;
			return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	for (i = 1; mflag && i < argc-1; ++i) {
		*lmode = (i == 1) ? 'w' : 'a';
		recvrequest("LIST", dest, argv[i], lmode, 0, 0);
		if (!mflag && fromatty) {
			if (confirm(argv[0], NULL))
				mflag ++;
		}
	}
	(void)signal(SIGINT, oldintr);
	mflag = 0;
	if (dest != odest)			/* free up after globulize() */
		free(dest);
}

/*
 * Do a shell escape
 */
/*ARGSUSED*/
void
shell(int argc, char *argv[])
{
	pid_t pid;
	sig_t old1, old2;
	char shellnam[PATH_MAX], *shellp, *namep;
	int wait_status;

	old1 = signal (SIGINT, SIG_IGN);
	old2 = signal (SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		for (pid = 3; pid < 20; pid++)
			(void)close(pid);
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);
		shellp = getenv("SHELL");
		if (shellp == NULL || *shellp == '\0')
			shellp = _PATH_BSHELL;
		namep = strrchr(shellp, '/');
		if (namep == NULL)
			namep = shellp;
		shellnam[0] = '-';
		(void)strlcpy(shellnam + 1, ++namep, sizeof(shellnam) - 1);
		if (strcmp(namep, "sh") != 0)
			shellnam[0] = '+';
		if (debug) {
			fputs(shellp, ttyout);
			fputc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (argc > 1) {
			execl(shellp, shellnam, "-c", altarg, (char *)0);
		}
		else {
			execl(shellp, shellnam, (char *)0);
		}
		warn("%s", shellp);
		code = -1;
		exit(1);
	}
	if (pid > 0)
		while (wait(&wait_status) != pid)
			;
	(void)signal(SIGINT, old1);
	(void)signal(SIGQUIT, old2);
	if (pid == -1) {
		warn("Try again later");
		code = -1;
	}
	else {
		code = 0;
	}
}

/*
 * Send new user information (re-login)
 */
void
user(int argc, char *argv[])
{
	char acctname[80];
	int n, aflag = 0;

	if (argc < 2)
		(void)another(&argc, &argv, "username");
	if (argc < 2 || argc > 4) {
		fprintf(ttyout, "usage: %s username [password [account]]\n",
		    argv[0]);
		code = -1;
		return;
	}
	n = command("USER %s", argv[1]);
	if (n == CONTINUE) {
		if (argc < 3 )
			argv[2] = getpass("Password:"), argc++;
		n = command("PASS %s", argv[2]);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			(void)fputs("Account: ", ttyout);
			(void)fflush(ttyout);
			if (fgets(acctname, sizeof(acctname), stdin) == NULL) {
				clearerr(stdin);
				goto fail;
			}

			acctname[strcspn(acctname, "\n")] = '\0';

			argv[3] = acctname;
			argc++;
		}
		n = command("ACCT %s", argv[3]);
		aflag++;
	}
	if (n != COMPLETE) {
 fail:
		fputs("Login failed.\n", ttyout);
		return;
	}
	if (!aflag && argc == 4) {
		(void)command("ACCT %s", argv[3]);
	}
	connected = -1;
}

/*
 * Print working directory on remote machine.
 */
/*ARGSUSED*/
void
pwd(int argc, char *argv[])
{
	int oldverbose = verbose;

	/*
	 * If we aren't verbose, this doesn't do anything!
	 */
	verbose = 1;
	if (command("PWD") == ERROR && code == 500) {
		fputs("PWD command not recognized, trying XPWD.\n", ttyout);
		(void)command("XPWD");
	}
	verbose = oldverbose;
}

/*
 * Print working directory on local machine.
 */
/* ARGSUSED */
void
lpwd(int argc, char *argv[])
{
	char buf[PATH_MAX];

	if (getcwd(buf, sizeof(buf)) != NULL)
		fprintf(ttyout, "Local directory %s\n", buf);
	else
		warn("getcwd");
	code = 0;
}

/*
 * Make a directory.
 */
void
makedir(int argc, char *argv[])
{

	if ((argc < 2 && !another(&argc, &argv, "directory-name")) ||
	    argc > 2) {
		fprintf(ttyout, "usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("MKD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			fputs("MKD command not recognized, trying XMKD.\n", ttyout);
		(void)command("XMKD %s", argv[1]);
	}
}

/*
 * Remove a directory.
 */
void
removedir(int argc, char *argv[])
{

	if ((argc < 2 && !another(&argc, &argv, "directory-name")) ||
	    argc > 2) {
		fprintf(ttyout, "usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RMD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			fputs("RMD command not recognized, trying XRMD.\n", ttyout);
		(void)command("XRMD %s", argv[1]);
	}
}

/*
 * Send a line, verbatim, to the remote machine.
 */
void
quote(int argc, char *argv[])
{

	if (argc < 2 && !another(&argc, &argv, "command line to send")) {
		fprintf(ttyout, "usage: %s arg ...\n", argv[0]);
		code = -1;
		return;
	}
	quote1("", argc, argv);
}

/*
 * Send a SITE command to the remote machine.  The line
 * is sent verbatim to the remote machine, except that the
 * word "SITE" is added at the front.
 */
void
site(int argc, char *argv[])
{

	if (argc < 2 && !another(&argc, &argv, "arguments to SITE command")) {
		fprintf(ttyout, "usage: %s arg ...\n", argv[0]);
		code = -1;
		return;
	}
	quote1("SITE", argc, argv);
}

/*
 * Turn argv[1..argc) into a space-separated string, then prepend initial text.
 * Send the result as a one-line command and get response.
 */
void
quote1(const char *initial, int argc, char *argv[])
{
	int i, len;
	char buf[BUFSIZ];		/* must be >= sizeof(line) */

	(void)strlcpy(buf, initial, sizeof(buf));
	if (argc > 1) {
		for (i = 1, len = strlen(buf); i < argc && len < sizeof(buf)-1; i++) {
			/* Space for next arg */
			if (len > 1)
				buf[len++] = ' ';

			/* Sanity check */
			if (len >= sizeof(buf) - 1)
				break;

			/* Copy next argument, NUL terminate always */
			strlcpy(&buf[len], argv[i], sizeof(buf) - len);

			/* Update string length */
			len = strlen(buf);
		}
	}

	/* Make double (triple?) sure the sucker is NUL terminated */
	buf[sizeof(buf) - 1] = '\0';

	if (command("%s", buf) == PRELIM) {
		while (getreply(0) == PRELIM)
			continue;
	}
}

void
do_chmod(int argc, char *argv[])
{

	if (argc < 2 && !another(&argc, &argv, "mode"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "file")) || argc > 3) {
usage:
		fprintf(ttyout, "usage: %s mode file\n", argv[0]);
		code = -1;
		return;
	}
	(void)command("SITE CHMOD %s %s", argv[1], argv[2]);
}

void
do_umask(int argc, char *argv[])
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "SITE UMASK" : "SITE UMASK %s", argv[1]);
	verbose = oldverbose;
}

void
idle(int argc, char *argv[])
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "SITE IDLE" : "SITE IDLE %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Ask the other side for help.
 */
void
rmthelp(int argc, char *argv[])
{
	int oldverbose = verbose;

	verbose = 1;
	(void)command(argc == 1 ? "HELP" : "HELP %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
/*ARGSUSED*/
void
quit(int argc, char *argv[])
{

	if (connected)
		disconnect(0, 0);
	pswitch(1);
	if (connected) {
		disconnect(0, 0);
	}
	exit(0);
}

void
account(int argc, char *argv[])
{
	char *ap;

	if (argc > 2) {
		fprintf(ttyout, "usage: %s [password]\n", argv[0]);
		code = -1;
		return;
	}
	else if (argc == 2)
		ap = argv[1];
	else
		ap = getpass("Account:");
	(void)command("ACCT %s", ap);
}

jmp_buf abortprox;

/* ARGSUSED */
void
proxabort(int signo)
{
	int save_errno = errno;

	alarmtimer(0);
	if (!proxy) {
		pswitch(1);
	}
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	errno = save_errno;
	longjmp(abortprox, 1);
}

void
doproxy(int argc, char *argv[])
{
	struct cmd *c;
	int cmdpos;
	sig_t oldintr;

	if (argc < 2 && !another(&argc, &argv, "command")) {
		fprintf(ttyout, "usage: %s command\n", argv[0]);
		code = -1;
		return;
	}
	c = getcmd(argv[1]);
	if (c == (struct cmd *) -1) {
		fputs("?Ambiguous command.\n", ttyout);
		(void)fflush(ttyout);
		code = -1;
		return;
	}
	if (c == 0) {
		fputs("?Invalid command.\n", ttyout);
		(void)fflush(ttyout);
		code = -1;
		return;
	}
	if (!c->c_proxy) {
		fputs("?Invalid proxy command.\n", ttyout);
		(void)fflush(ttyout);
		code = -1;
		return;
	}
	if (setjmp(abortprox)) {
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, proxabort);
	pswitch(1);
	if (c->c_conn && !connected) {
		fputs("Not connected.\n", ttyout);
		(void)fflush(ttyout);
		pswitch(0);
		(void)signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	cmdpos = strcspn(line, " \t");
	if (cmdpos > 0)		/* remove leading "proxy " from input buffer */
		memmove(line, line + cmdpos + 1, strlen(line) - cmdpos + 1);
	(*c->c_handler)(argc-1, argv+1);
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	(void)signal(SIGINT, oldintr);
}

void
setcase(int argc, char *argv[])
{

	code = togglevar(argc, argv, &mcase, "Case mapping");
}

void
setcr(int argc, char *argv[])
{

	code = togglevar(argc, argv, &crflag, "Carriage Return stripping");
}

void
setntrans(int argc, char *argv[])
{
	if (argc == 1) {
		ntflag = 0;
		fputs("Ntrans off.\n", ttyout);
		code = ntflag;
		return;
	}
	ntflag++;
	code = ntflag;
	(void)strlcpy(ntin, argv[1], sizeof(ntin));
	if (argc == 2) {
		ntout[0] = '\0';
		return;
	}
	(void)strlcpy(ntout, argv[2], sizeof(ntout));
}

void
setnmap(int argc, char *argv[])
{
	char *cp;

	if (argc == 1) {
		mapflag = 0;
		fputs("Nmap off.\n", ttyout);
		code = mapflag;
		return;
	}
	if ((argc < 3 && !another(&argc, &argv, "outpattern")) || argc > 3) {
		fprintf(ttyout, "usage: %s [inpattern outpattern]\n", argv[0]);
		code = -1;
		return;
	}
	mapflag = 1;
	code = 1;
	cp = strchr(altarg, ' ');
	if (proxy) {
		while(*++cp == ' ')
			continue;
		altarg = cp;
		cp = strchr(altarg, ' ');
	}
	*cp = '\0';
	(void)strncpy(mapin, altarg, PATH_MAX - 1);
	while (*++cp == ' ')
		continue;
	(void)strncpy(mapout, cp, PATH_MAX - 1);
}

void
setpassive(int argc, char *argv[])
{

	code = togglevar(argc, argv, &passivemode,
	    verbose ? "Passive mode" : NULL);
}

void
setsunique(int argc, char *argv[])
{

	code = togglevar(argc, argv, &sunique, "Store unique");
}

void
setrunique(int argc, char *argv[])
{

	code = togglevar(argc, argv, &runique, "Receive unique");
}

/* change directory to parent directory */
/* ARGSUSED */
void
cdup(int argc, char *argv[])
{
	int r;

	r = command("CDUP");
	if (r == ERROR && code == 500) {
		if (verbose)
			fputs("CDUP command not recognized, trying XCUP.\n", ttyout);
		r = command("XCUP");
	}
	if (r == COMPLETE)
		dirchange = 1;
}

/*
 * Restart transfer at specific point
 */
void
restart(int argc, char *argv[])
{
	quad_t nrestart_point;
	char *ep;

	if (argc != 2)
		fputs("restart: offset not specified.\n", ttyout);
	else {
		nrestart_point = strtoq(argv[1], &ep, 10);
		if (nrestart_point == QUAD_MAX || *ep != '\0')
			fputs("restart: invalid offset.\n", ttyout);
		else {
			fprintf(ttyout, "Restarting at %lld. Execute get, put "
				"or append to initiate transfer\n",
				(long long)nrestart_point);
			restart_point = nrestart_point;
		}
	}
}

/* 
 * Show remote system type
 */
/* ARGSUSED */
void
syst(int argc, char *argv[])
{

	(void)command("SYST");
}

void
macdef(int argc, char *argv[])
{
	char *tmp;
	int c;

	if (macnum == 16) {
		fputs("Limit of 16 macros have already been defined.\n", ttyout);
		code = -1;
		return;
	}
	if ((argc < 2 && !another(&argc, &argv, "macro-name")) || argc > 2) {
		fprintf(ttyout, "usage: %s macro-name\n", argv[0]);
		code = -1;
		return;
	}
	if (interactive)
		fputs(
"Enter macro line by line, terminating it with a null line.\n", ttyout);
	(void)strlcpy(macros[macnum].mac_name, argv[1],
	    sizeof(macros[macnum].mac_name));
	if (macnum == 0)
		macros[macnum].mac_start = macbuf;
	else
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	tmp = macros[macnum].mac_start;
	while (tmp != macbuf+4096) {
		if ((c = getchar()) == EOF) {
			fputs("macdef: end of file encountered.\n", ttyout);
			code = -1;
			return;
		}
		if ((*tmp = c) == '\n') {
			if (tmp == macros[macnum].mac_start) {
				macros[macnum++].mac_end = tmp;
				code = 0;
				return;
			}
			if (*(tmp-1) == '\0') {
				macros[macnum++].mac_end = tmp - 1;
				code = 0;
				return;
			}
			*tmp = '\0';
		}
		tmp++;
	}
	while (1) {
		while ((c = getchar()) != '\n' && c != EOF)
			/* LOOP */;
		if (c == EOF || getchar() == '\n') {
			fputs("Macro not defined - 4K buffer exceeded.\n", ttyout);
			code = -1;
			return;
		}
	}
}

/*
 * Get size of file on remote machine
 */
void
sizecmd(int argc, char *argv[])
{
	off_t size;

	if ((argc < 2 && !another(&argc, &argv, "file")) || argc > 2) {
		fprintf(ttyout, "usage: %s file\n", argv[0]);
		code = -1;
		return;
	}
	size = remotesize(argv[1], 1);
	if (size != -1)
		fprintf(ttyout, "%s\t%lld\n", argv[1], (long long)size);
	code = size;
}

/*
 * Get last modification time of file on remote machine
 */
void
modtime(int argc, char *argv[])
{
	time_t mtime;

	if ((argc < 2 && !another(&argc, &argv, "file")) || argc > 2) {
		fprintf(ttyout, "usage: %s file\n", argv[0]);
		code = -1;
		return;
	}
	mtime = remotemodtime(argv[1], 1);
	if (mtime != -1)
		fprintf(ttyout, "%s\t%s", argv[1], asctime(localtime(&mtime)));
	code = mtime;
}

/*
 * Show status on remote machine
 */
void
rmtstatus(int argc, char *argv[])
{

	(void)command(argc > 1 ? "STAT %s" : "STAT" , argv[1]);
}

/*
 * Get file if modtime is more recent than current file
 */
void
newer(int argc, char *argv[])
{

	if (getit(argc, argv, -1, "w"))
		fprintf(ttyout, "Local file \"%s\" is newer than remote file \"%s\".\n",
			argv[2], argv[1]);
}

/*
 * Display one file through $PAGER (defaults to "more").
 */
void
page(int argc, char *argv[])
{
	int orestart_point, ohash, overbose;
	char *p, *pager, *oldargv1;

	if ((argc < 2 && !another(&argc, &argv, "file")) || argc > 2) {
		fprintf(ttyout, "usage: %s file\n", argv[0]);
		code = -1;
		return;
	}
	oldargv1 = argv[1];
	if (!globulize(&argv[1])) {
		code = -1;
		return;
	}
	p = getenv("PAGER");
	if (p == NULL || (*p == '\0'))
		p = PAGER;
	if (asprintf(&pager, "|%s", p) == -1)
		errx(1, "Can't allocate memory for $PAGER");

	orestart_point = restart_point;
	ohash = hash;
	overbose = verbose;
	restart_point = hash = verbose = 0;
	recvrequest("RETR", pager, argv[1], "r+w", 1, 0);
	(void)free(pager);
	restart_point = orestart_point;
	hash = ohash;
	verbose = overbose;
	if (oldargv1 != argv[1])	/* free up after globulize() */
		free(argv[1]);
}

#endif /* !SMALL */

