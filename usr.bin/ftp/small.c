/*	$OpenBSD: small.c,v 1.4 2015/01/30 04:45:45 tedu Exp $	*/
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
#include "small.h"

jmp_buf	jabort;
char   *mname;
char   *home = "/";

struct	types {
	char	*t_name;
	char	*t_mode;
	int	t_type;
} types[] = {
	{ "ascii",	"A",	TYPE_A },
	{ "binary",	"I",	TYPE_I },
	{ "image",	"I",	TYPE_I },
	{ NULL }
};

/*
 * Set transfer type.
 */
void
settype(int argc, char *argv[])
{
	struct types *p;
	int comret;

	if (argc > 2) {
		char *sep;

		fprintf(ttyout, "usage: %s [", argv[0]);
		sep = "";
		for (p = types; p->t_name; p++) {
			fprintf(ttyout, "%s%s", sep, p->t_name);
			sep = " | ";
		}
		fputs("]\n", ttyout);
		code = -1;
		return;
	}
	if (argc < 2) {
		fprintf(ttyout, "Using %s mode to transfer files.\n", typename);
		code = 0;
		return;
	}
	for (p = types; p->t_name; p++)
		if (strcmp(argv[1], p->t_name) == 0)
			break;
	if (p->t_name == 0) {
		fprintf(ttyout, "%s: unknown mode.\n", argv[1]);
		code = -1;
		return;
	}
	comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void)strlcpy(typename, p->t_name, sizeof typename);
		curtype = type = p->t_type;
	}
}

/*
 * Internal form of settype; changes current type in use with server
 * without changing our notion of the type for data transfers.
 * Used to change to and from ascii for listings.
 */
void
changetype(int newtype, int show)
{
	struct types *p;
	int comret, oldverbose = verbose;

	if (newtype == 0)
		newtype = TYPE_I;
	if (newtype == curtype)
		return;
	if (
#ifndef SMALL
	    !debug &&
#endif /* !SMALL */
	    show == 0)
		verbose = 0;
	for (p = types; p->t_name; p++)
		if (newtype == p->t_type)
			break;
	if (p->t_name == 0) {
		warnx("internal error: unknown type %d.", newtype);
		return;
	}
	if (newtype == TYPE_L && bytename[0] != '\0')
		comret = command("TYPE %s %s", p->t_mode, bytename);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE)
		curtype = newtype;
	verbose = oldverbose;
}

char *stype[] = {
	"type",
	"",
	0
};

/*
 * Set binary transfer type.
 */
/*ARGSUSED*/
void
setbinary(int argc, char *argv[])
{

	stype[1] = "binary";
	settype(2, stype);
}

void
get(int argc, char *argv[])
{

	(void)getit(argc, argv, 0, restart_point ? "a+w" : "w" );
}

/*
 * Receive one file.
 */
int
getit(int argc, char *argv[], int restartit, const char *mode)
{
	int loc = 0;
	int rval = 0;
	char *oldargv1, *oldargv2, *globargv2;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
#ifndef SMALL
	if (argc < 2 && !another(&argc, &argv, "remote-file"))
		goto usage;
	if ((argc < 3 && !another(&argc, &argv, "local-file")) || argc > 3) {
usage:
		fprintf(ttyout, "usage: %s remote-file [local-file]\n",
		    argv[0]);
		code = -1;
		return (0);
	}
#endif /* !SMALL */
	oldargv1 = argv[1];
	oldargv2 = argv[2];
	if (!globulize(&argv[2])) {
		code = -1;
		return (0);
	}
	globargv2 = argv[2];
	if (loc && mcase) {
		char *tp = argv[1], *tp2, tmpbuf[PATH_MAX];

		while (*tp && !islower(*tp)) {
			tp++;
		}
		if (!*tp) {
			tp = argv[2];
			tp2 = tmpbuf;
			while ((*tp2 = *tp) != '\0') {
				if (isupper(*tp2)) {
					*tp2 = tolower(*tp2);
				}
				tp++;
				tp2++;
			}
			argv[2] = tmpbuf;
		}
	}
	if (loc && ntflag)
		argv[2] = dotrans(argv[2]);
	if (loc && mapflag)
		argv[2] = domap(argv[2]);
#ifndef SMALL
	if (restartit) {
		struct stat stbuf;
		int ret;

		ret = stat(argv[2], &stbuf);
		if (restartit == 1) {
			restart_point = (ret < 0) ? 0 : stbuf.st_size;
		} else {
			if (ret == 0) {
				time_t mtime;

				mtime = remotemodtime(argv[1], 0);
				if (mtime == -1)
					goto freegetit;
				if (stbuf.st_mtime >= mtime) {
					rval = 1;
					goto freegetit;
				}
			}
		}
	}
#endif /* !SMALL */

	recvrequest("RETR", argv[2], argv[1], mode,
	    argv[1] != oldargv1 || argv[2] != oldargv2 || !interactive, loc);
	restart_point = 0;
freegetit:
	if (oldargv2 != globargv2)	/* free up after globulize() */
		free(globargv2);
	return (rval);
}

/* XXX - Signal race. */
/* ARGSUSED */
void
mabort(int signo)
{
	int save_errno = errno;

	alarmtimer(0);
	(void) write(fileno(ttyout), "\n\r", 2);
#ifndef SMALL
	if (mflag && fromatty) {
		/* XXX signal race, crazy unbelievable stdio misuse */
		if (confirm(mname, NULL)) {
			errno = save_errno;
			longjmp(jabort, 1);
		}
	}
#endif /* !SMALL */
	mflag = 0;
	errno = save_errno;
	longjmp(jabort, 1);
}

/*
 * Get multiple files.
 */
void
mget(int argc, char *argv[])
{
	extern int optind, optreset;
	sig_t oldintr;
	int ch, xargc = 2;
	char *cp, localcwd[PATH_MAX], *xargv[] = { argv[0], NULL, NULL };
	static int restartit = 0;
#ifndef SMALL
	extern char *optarg;
	const char *errstr;
	int i = 1;
	char type = 0, *dummyargv[] = { argv[0], ".", NULL };
	FILE *ftemp = NULL;
	static int depth = 0, max_depth = 0;

	optind = optreset = 1;

	if (depth)
		depth++;

	while ((ch = getopt(argc, argv, "cd:nr")) != -1) {
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
		case 'n':
			restartit = -1;
			break;
		case 'r':
			depth = 1;
			break;
		default:
			goto usage;
		}
	}

	if (argc - optind < 1 && !another(&argc, &argv, "remote-files")) {
usage:
		fprintf(ttyout, "usage: %s [-cnr] [-d depth] remote-files\n",
		    argv[0]);
		code = -1;
		return;
	}

	argv[optind - 1] = argv[0];
	argc -= optind - 1;
	argv += optind - 1;
#endif /* !SMALL */

	mname = argv[0];
	mflag = 1;
	if (getcwd(localcwd, sizeof(localcwd)) == NULL)
		err(1, "can't get cwd");

	oldintr = signal(SIGINT, mabort);
	(void)setjmp(jabort);
	while ((cp =
#ifdef SMALL
	    remglob(argv, proxy, NULL)) != NULL
	    ) {
#else /* SMALL */
	    depth ? remglob2(dummyargv, proxy, NULL, &ftemp, &type) :
	    remglob(argv, proxy, NULL)) != NULL
	    || (mflag && depth && ++i < argc)
	    ) {
		if (cp == NULL)
			continue;
#endif /* SMALL */
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (!mflag)
			continue;
#ifndef SMALL
		if (depth && fnmatch(argv[i], cp, FNM_PATHNAME) != 0)
			continue;
#endif /* !SMALL */
		if (!fileindir(cp, localcwd)) {
			fprintf(ttyout, "Skipping non-relative filename `%s'\n",
			    cp);
			continue;
		}
#ifndef SMALL
		if (type == 'd' && depth == max_depth)
			continue;
		if (!confirm(argv[0], cp))
			continue;
		if (type == 'd') {
			mkdir(cp, 0755);
			if (chdir(cp) != 0) {
				warn("local: %s", cp);
				continue;
			}

			xargv[1] = cp;
			cd(xargc, xargv);
			if (dirchange != 1)
				goto out;

			xargv[1] = "*";
			mget(xargc, xargv);

			xargv[1] = "..";
			cd(xargc, xargv);
			if (dirchange != 1) {
				mflag = 0;
				goto out;
			}

out:
			if (chdir("..") != 0) {
				warn("local: %s", cp);
				mflag = 0;
			}
			continue;
		}
		if (type == 's')
			/* Currently ignored. */
			continue;
#endif /* !SMALL */
		xargv[1] = cp;
		(void)getit(xargc, xargv, restartit,
		    (restartit == 1 || restart_point) ? "a+w" : "w");
#ifndef SMALL
		if (!mflag && fromatty) {
			if (confirm(argv[0], NULL))
				mflag = 1;
		}
#endif /* !SMALL */
	}
	(void)signal(SIGINT, oldintr);
#ifndef SMALL
	if (depth)
		depth--;
	if (depth == 0 || mflag == 0)
		depth = max_depth = mflag = restartit = 0;
#else /* !SMALL */
	mflag = 0;
#endif /* !SMALL */
}

/*
 * Set current working directory on remote machine.
 */
void
cd(int argc, char *argv[])
{
	int r;

#ifndef SMALL
	if ((argc < 2 && !another(&argc, &argv, "remote-directory")) ||
	    argc > 2) {
		fprintf(ttyout, "usage: %s remote-directory\n", argv[0]);
		code = -1;
		return;
	}
#endif /* !SMALL */
	r = command("CWD %s", argv[1]);
	if (r == ERROR && code == 500) {
		if (verbose)
			fputs("CWD command not recognized, trying XCWD.\n", ttyout);
		r = command("XCWD %s", argv[1]);
	}
	if (r == ERROR && code == 550) {
		dirchange = 0;
		return;
	}
	if (r == COMPLETE)
		dirchange = 1;
}

/*
 * Terminate session, but don't exit.
 */
/* ARGSUSED */
void
disconnect(int argc, char *argv[])
{

	if (!connected)
		return;
	(void)command("QUIT");
	if (cout) {
		(void)fclose(cout);
	}
	cout = NULL;
	connected = 0;
	data = -1;
#ifndef SMALL
	if (!proxy) {
		macnum = 0;
	}
#endif /* !SMALL */
}

char *
dotrans(char *name)
{
	static char new[PATH_MAX];
	char *cp1, *cp2 = new;
	int i, ostop, found;

	for (ostop = 0; *(ntout + ostop) && ostop < 16; ostop++)
		continue;
	for (cp1 = name; *cp1; cp1++) {
		found = 0;
		for (i = 0; *(ntin + i) && i < 16; i++) {
			if (*cp1 == *(ntin + i)) {
				found++;
				if (i < ostop) {
					*cp2++ = *(ntout + i);
				}
				break;
			}
		}
		if (!found) {
			*cp2++ = *cp1;
		}
	}
	*cp2 = '\0';
	return (new);
}

char *
domap(char *name)
{
	static char new[PATH_MAX];
	char *cp1 = name, *cp2 = mapin;
	char *tp[9], *te[9];
	int i, toks[9], toknum = 0, match = 1;

	for (i=0; i < 9; ++i) {
		toks[i] = 0;
	}
	while (match && *cp1 && *cp2) {
		switch (*cp2) {
			case '\\':
				if (*++cp2 != *cp1) {
					match = 0;
				}
				break;
			case '$':
				if (*(cp2+1) >= '1' && (*cp2+1) <= '9') {
					if (*cp1 != *(++cp2+1)) {
						toks[toknum = *cp2 - '1']++;
						tp[toknum] = cp1;
						while (*++cp1 && *(cp2+1)
							!= *cp1);
						te[toknum] = cp1;
					}
					cp2++;
					break;
				}
				/* FALLTHROUGH */
			default:
				if (*cp2 != *cp1) {
					match = 0;
				}
				break;
		}
		if (match && *cp1) {
			cp1++;
		}
		if (match && *cp2) {
			cp2++;
		}
	}
	if (!match && *cp1) /* last token mismatch */
	{
		toks[toknum] = 0;
	}
	cp1 = new;
	*cp1 = '\0';
	cp2 = mapout;
	while (*cp2) {
		match = 0;
		switch (*cp2) {
			case '\\':
				if (*(cp2 + 1)) {
					*cp1++ = *++cp2;
				}
				break;
			case '[':
LOOP:
				if (*++cp2 == '$' && isdigit(*(cp2+1))) {
					if (*++cp2 == '0') {
						char *cp3 = name;

						while (*cp3) {
							*cp1++ = *cp3++;
						}
						match = 1;
					}
					else if (toks[toknum = *cp2 - '1']) {
						char *cp3 = tp[toknum];

						while (cp3 != te[toknum]) {
							*cp1++ = *cp3++;
						}
						match = 1;
					}
				}
				else {
					while (*cp2 && *cp2 != ',' &&
					    *cp2 != ']') {
						if (*cp2 == '\\') {
							cp2++;
						}
						else if (*cp2 == '$' &&
   						        isdigit(*(cp2+1))) {
							if (*++cp2 == '0') {
							   char *cp3 = name;

							   while (*cp3) {
								*cp1++ = *cp3++;
							   }
							}
							else if (toks[toknum =
							    *cp2 - '1']) {
							   char *cp3=tp[toknum];

							   while (cp3 !=
								  te[toknum]) {
								*cp1++ = *cp3++;
							   }
							}
						}
						else if (*cp2) {
							*cp1++ = *cp2++;
						}
					}
					if (!*cp2) {
						fputs(
"nmap: unbalanced brackets.\n", ttyout);
						return (name);
					}
					match = 1;
					cp2--;
				}
				if (match) {
					while (*++cp2 && *cp2 != ']') {
					      if (*cp2 == '\\' && *(cp2 + 1)) {
							cp2++;
					      }
					}
					if (!*cp2) {
						fputs(
"nmap: unbalanced brackets.\n", ttyout);
						return (name);
					}
					break;
				}
				switch (*++cp2) {
					case ',':
						goto LOOP;
					case ']':
						break;
					default:
						cp2--;
						goto LOOP;
				}
				break;
			case '$':
				if (isdigit(*(cp2 + 1))) {
					if (*++cp2 == '0') {
						char *cp3 = name;

						while (*cp3) {
							*cp1++ = *cp3++;
						}
					}
					else if (toks[toknum = *cp2 - '1']) {
						char *cp3 = tp[toknum];

						while (cp3 != te[toknum]) {
							*cp1++ = *cp3++;
						}
					}
					break;
				}
				/* FALLTHROUGH */
			default:
				*cp1++ = *cp2;
				break;
		}
		cp2++;
	}
	*cp1 = '\0';
	if (!*new) {
		return (name);
	}
	return (new);
}

