/*	$OpenBSD: msgs.c,v 1.29 2004/10/02 04:14:39 deraadt Exp $	*/
/*	$NetBSD: msgs.c,v 1.7 1995/09/28 06:57:40 tls Exp $	*/

/*-
 * Copyright (c) 1980, 1993
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)msgs.c	8.2 (Berkeley) 4/28/95";
#else
static char rcsid[] = "$OpenBSD: msgs.c,v 1.29 2004/10/02 04:14:39 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * msgs - a user bulletin board program
 *
 * usage:
 *	msgs [fhlopqr] [[-]number]	to read messages
 *	msgs -s				to place messages
 *	msgs -c [-days]			to clean up the bulletin board
 *
 * prompt commands are:
 *	y	print message
 *	n	flush message, go to next message
 *	q	flush message, quit
 *	p	print message, turn on 'pipe thru more' mode
 *	P	print message, turn off 'pipe thru more' mode
 *	-	reprint last message
 *	s[-][<num>] [<filename>]	save message
 *	m[-][<num>]	mail with message in temp mbox
 *	x	exit without flushing this message
 *	<num>	print message number <num>
 */

#define OBJECT		/* will object to messages without Subjects */
#define REJECT	/* will reject messages without Subjects
			   (OBJECT must be defined also) */
#undef UNBUFFERED	/* use unbuffered output */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "pathnames.h"

#define CMODE	0664		/* bounds file creation mode */
#define NO	0
#define YES	1
#define SUPERUSER	0	/* superuser uid */
#define DAEMON		1	/* daemon uid */
#define NLINES	24		/* default number of lines/crt screen */
#define NDAYS	21		/* default keep time for messages */
#define DAYS	*24*60*60	/* seconds/day */
#define MSGSRC	".msgsrc"	/* user's rc file */
#define BOUNDS	"bounds"	/* message bounds file */
#define NEXT	"Next message? [yq]"
#define MORE	"More? [ynq]"
#define NOMORE	"(No more) [q] ?"

typedef	char	bool;

FILE	*msgsrc;
FILE	*newmsg;
char	*sep = "-";
char	inbuf[BUFSIZ];
char	fname[MAXPATHLEN];
char	cmdbuf[MAXPATHLEN + MAXPATHLEN];
char	subj[128];
char	from[128];
char	date[128];
char	*ptr;
char	*in;
bool	local;
bool	ruptible;
bool	totty;
bool	seenfrom;
bool	seensubj;
bool	blankline;
bool	printing = NO;
bool	mailing = NO;
bool	quitit = NO;
bool	sending = NO;
bool	intrpflg = NO;
bool	restricted = NO;
int	uid;
int	msg;
int	prevmsg;
int	lct;
int	nlines;
int	Lpp = 0;
time_t	t;
time_t	keep;

void prmesg(int);
void onintr(int);
void onsusp(int);
int linecnt(FILE *);
int next(char *, int);
void ask(char *);
void gfrsub(FILE *);
char *nxtfld(char *);

/* option initialization */
bool	hdrs = NO;
bool	qopt = NO;
bool	hush = NO;
bool	send_msg = NO;
bool	locomode = NO;
bool	use_pager = NO;
bool	clean = NO;
bool	lastcmd = NO;
jmp_buf	tstpbuf;

int
main(int argc, char *argv[])
{
	bool newrc, already;
	int rcfirst = 0;		/* first message to print (from .rc) */
	int rcback = 0;			/* amount to back off of rcfirst */
	int firstmsg, nextmsg, lastmsg = 0;
	int blast = 0;
	FILE *bounds;
	char *cp;

#ifdef UNBUFFERED
	setbuf(stdout, NULL);
#endif

	time(&t);
	seteuid(uid = getuid());
	setuid(uid);
	ruptible = (signal(SIGINT, SIG_IGN) == SIG_DFL);
	if (ruptible)
		signal(SIGINT, SIG_DFL);

	argc--, argv++;
	while (argc > 0) {
		if (isdigit(argv[0][0])) {	/* starting message # */
			rcfirst = atoi(argv[0]);
		} else if (isdigit(argv[0][1])) {	/* backward offset */
			rcback = atoi(&(argv[0][1]));
		} else {
			ptr = *argv;
			while (*ptr) {
				switch (*ptr++) {
				case '-':
					break;
				case 'c':
					if (uid != SUPERUSER && uid != DAEMON) {
						fprintf(stderr, "Sorry\n");
						exit(1);
					}
					clean = YES;
					break;
				case 'f':	/* silently */
					hush = YES;
					break;
				case 'h':	/* headers only */
					hdrs = YES;
					break;
				case 'l':	/* local msgs only */
					locomode = YES;
					break;
				case 'o':	/* option to save last message */
					lastcmd = YES;
					break;
				case 'p':	/* pipe thru 'more' during long msgs */
					use_pager = YES;
					break;
				case 'q':	/* query only */
					qopt = YES;
					break;
				case 'r':	/* restricted */
					restricted = YES;
					break;
				case 's':	/* sending TO msgs */
					send_msg = YES;
					break;
				default:
					fprintf(stderr,
					    "usage: msgs [fhlopqr] [[-]number]\n"
					    "       msgs [-s]\n"
					    "       msgs [-c [-days]]\n");
					exit(1);
				}
			}
		}
		argc--, argv++;
	}

	/*
	 * determine current message bounds
	 */
	snprintf(fname, sizeof(fname), "%s/%s", _PATH_MSGS, BOUNDS);
	bounds = fopen(fname, "r");

	if (bounds == NULL) {
		if (errno == ENOENT) {
			if ((bounds = fopen(fname, "w+")) == NULL) {
				perror(fname);
				exit(1);
			}
			fprintf(bounds, "1 0\n");
			rewind(bounds);
		} else {
			perror(fname);
			exit(1);
		}
	}

	fscanf(bounds, "%d %d\n", &firstmsg, &lastmsg);
	fclose(bounds);
	blast = lastmsg;	/* save upper bound */

	if (clean)
		keep = t - (rcback? rcback : NDAYS) DAYS;

	if (clean || bounds == NULL) {	/* relocate message bounds */
		struct dirent *dp;
		struct stat stbuf;
		bool seenany = NO;
		DIR	*dirp;

		dirp = opendir(_PATH_MSGS);
		if (dirp == NULL) {
			perror(_PATH_MSGS);
			exit(errno);
		}
		chmod(fname, CMODE);

		firstmsg = 32767;
		lastmsg = 0;

		for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)){
			int i = 0;

			cp = dp->d_name;
			if (dp->d_ino == 0)
				continue;
			if (dp->d_namlen == 0)
				continue;

			if (clean)
				snprintf(inbuf, sizeof(inbuf), "%s/%s",
				    _PATH_MSGS, cp);

			while (isdigit(*cp))
				i = i * 10 + *cp++ - '0';
			if (*cp)
				continue;	/* not a message! */

			if (clean) {
				if (stat(inbuf, &stbuf) != 0)
					continue;
				if (stbuf.st_mtime < keep &&
				    stbuf.st_mode&S_IWRITE) {
					unlink(inbuf);
					continue;
				}
			}

			if (i > lastmsg)
				lastmsg = i;
			if (i < firstmsg)
				firstmsg = i;
			seenany = YES;
		}
		closedir(dirp);

		if (!seenany) {
			if (blast != 0)	/* never lower the upper bound! */
				lastmsg = blast;
			firstmsg = lastmsg + 1;
		} else if (blast > lastmsg)
			lastmsg = blast;

		if (!send_msg) {
			bounds = fopen(fname, "w");
			if (bounds == NULL) {
				perror(fname);
				exit(errno);
			}
			chmod(fname, CMODE);
			fprintf(bounds, "%d %d\n", firstmsg, lastmsg);
			fclose(bounds);
		}
	}

	if (send_msg) {
		/*
		 * Send mode - place msgs in _PATH_MSGS
		 */
		bounds = fopen(fname, "w");
		if (bounds == NULL) {
			perror(fname);
			exit(errno);
		}

		nextmsg = lastmsg + 1;
		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, nextmsg);
		newmsg = fopen(fname, "w");
		if (newmsg == NULL) {
			perror(fname);
			exit(errno);
		}
		chmod(fname, 0644);

		fprintf(bounds, "%d %d\n", firstmsg, nextmsg);
		fclose(bounds);

		sending = YES;
		if (ruptible)
			signal(SIGINT, onintr);

		if (isatty(fileno(stdin))) {
			ptr = getpwuid(uid)->pw_name;
			printf("Message %d:\nFrom %s %sSubject: ",
			    nextmsg, ptr, ctime(&t));
			fflush(stdout);
			fgets(inbuf, sizeof inbuf, stdin);
			putchar('\n');
			fflush(stdout);
			fprintf(newmsg, "From %s %sSubject: %s\n",
			    ptr, ctime(&t), inbuf);
			blankline = seensubj = YES;
		} else
			blankline = seensubj = NO;
		for (;;) {
			fgets(inbuf, sizeof inbuf, stdin);
			if (feof(stdin) || ferror(stdin))
				break;
			blankline = (blankline || (inbuf[0] == '\n'));
			seensubj = (seensubj ||
			    (!blankline && (strncmp(inbuf, "Subj", 4) == 0)));
			fputs(inbuf, newmsg);
		}
#ifdef OBJECT
		if (!seensubj) {
			printf("NOTICE: Messages should have a Subject field!\n");
#ifdef REJECT
			unlink(fname);
#endif
			exit(1);
		}
#endif
		exit(ferror(stdin));
	}
	if (clean)
		exit(0);

	/*
	 * prepare to display messages
	 */
	totty = (isatty(fileno(stdout)) != 0);
	use_pager = use_pager && totty;

	if ((cp = getenv("HOME")) == NULL || *cp == '\0') {
		fprintf(stderr, "Error, no home directory!\n");
		exit(1);
	}
	snprintf(fname, sizeof(fname), "%s/%s", cp, MSGSRC);
	msgsrc = fopen(fname, "r");
	if (msgsrc) {
		newrc = NO;
		fscanf(msgsrc, "%d\n", &nextmsg);
		fclose(msgsrc);
		if (nextmsg > lastmsg+1) {
			printf("Warning: bounds have been reset (%d, %d)\n",
			    firstmsg, lastmsg);
			truncate(fname, (off_t)0);
			newrc = YES;
		} else if (!rcfirst)
			rcfirst = nextmsg - rcback;
	} else
		newrc = YES;
	msgsrc = fopen(fname, "r+");
	if (msgsrc == NULL)
		msgsrc = fopen(fname, "w");
	if (msgsrc == NULL) {
		perror(fname);
		exit(errno);
	}
	if (rcfirst) {
		if (rcfirst > lastmsg+1) {
			printf("Warning: the last message is number %d.\n",
			    lastmsg);
			rcfirst = nextmsg;
		}
		if (rcfirst > firstmsg)
			firstmsg = rcfirst;	/* don't set below first msg */
	}
	if (newrc) {
		nextmsg = firstmsg;
		fseeko(msgsrc, (off_t)0, SEEK_SET);
		fprintf(msgsrc, "%d\n", nextmsg);
		fflush(msgsrc);
	}

	if (totty) {
		struct winsize win;
		if (ioctl(fileno(stdout), TIOCGWINSZ, &win) != -1)
			Lpp = win.ws_row;
		if (Lpp <= 0) {
			char *ttype = getenv("TERM");

			if (ttype != (char *)NULL) {
				if (tgetent(NULL, ttype) <= 0
				    || (Lpp = tgetnum("li")) <= 0) {
					Lpp = NLINES;
				}
			} else
				Lpp = NLINES;
		}
	}
	Lpp -= 6;	/* for headers, etc. */

	already = NO;
	prevmsg = firstmsg;
	printing = YES;
	if (ruptible)
		signal(SIGINT, onintr);

	/*
	 * Main program loop
	 */
	for (msg = firstmsg; msg <= lastmsg; msg++) {

		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, msg);
		newmsg = fopen(fname, "r");
		if (newmsg == NULL)
			continue;

		gfrsub(newmsg);		/* get From and Subject fields */
		if (locomode && !local) {
			fclose(newmsg);
			continue;
		}

		if (qopt) {	/* This has to be located here */
			printf("There are new messages.\n");
			exit(0);
		}

		if (already && !hdrs)
			putchar('\n');

		/*
		 * Print header
		 */
		if (totty)
			signal(SIGTSTP, onsusp);
		(void) setjmp(tstpbuf);
		already = YES;
		nlines = 2;
		if (seenfrom) {
			printf("Message %d:\nFrom %s %s", msg, from, date);
			nlines++;
		}
		if (seensubj) {
			printf("Subject: %s", subj);
			nlines++;
		} else {
			if (seenfrom) {
				putchar('\n');
				nlines++;
			}
			while (nlines < 6 &&
			    fgets(inbuf, sizeof inbuf, newmsg) &&
			    inbuf[0] != '\n') {
				fputs(inbuf, stdout);
				nlines++;
			}
		}

		lct = linecnt(newmsg);
		if (lct)
			printf("(%d%slines) ", lct, seensubj? " " : " more ");

		if (hdrs) {
			printf("\n-----\n");
			fclose(newmsg);
			continue;
		}

		/*
		 * Ask user for command
		 */
		if (totty)
			ask(lct? MORE : (msg==lastmsg? NOMORE : NEXT));
		else
			inbuf[0] = 'y';
		if (totty)
			signal(SIGTSTP, SIG_DFL);
cmnd:
		in = inbuf;
		switch (*in) {
		case 'x':
		case 'X':
			exit(0);

		case 'q':
		case 'Q':
			quitit = YES;
			printf("--Postponed--\n");
			exit(0);
			/* intentional fall-thru */
		case 'n':
		case 'N':
			if (msg >= nextmsg)
				sep = "Flushed";
			prevmsg = msg;
			break;

		case 'p':
		case 'P':
			use_pager = (*in++ == 'p');
			/* intentional fallthru */
		case '\n':
		case 'y':
		default:
			if (*in == '-') {
				msg = prevmsg-1;
				sep = "replay";
				break;
			}
			if (isdigit(*in)) {
				msg = next(in, sizeof inbuf);
				sep = in;
				break;
			}

			prmesg(nlines + lct + (seensubj? 1 : 0));
			prevmsg = msg;
		}

		printf("--%s--\n", sep);
		sep = "-";
		if (msg >= nextmsg) {
			nextmsg = msg + 1;
			fseeko(msgsrc, (off_t)0, SEEK_SET);
			fprintf(msgsrc, "%d\n", nextmsg);
			fflush(msgsrc);
		}
		if (newmsg)
			fclose(newmsg);
		if (quitit)
			break;
	}

	/*
	 * Make sure .rc file gets updated
	 */
	if (--msg >= nextmsg) {
		nextmsg = msg + 1;
		fseeko(msgsrc, (off_t)0, SEEK_SET);
		fprintf(msgsrc, "%d\n", nextmsg);
		fflush(msgsrc);
	}
	if (already && !quitit && lastcmd && totty) {
		/*
		 * save or reply to last message?
		 */
		msg = prevmsg;
		ask(NOMORE);
		if (inbuf[0] == '-' || isdigit(inbuf[0]))
			goto cmnd;
	}
	if (!(already || hush || qopt))
		printf("No new messages.\n");
	exit(0);
}

void
prmesg(int length)
{
	FILE *outf;
	char *env_pager;

	if (use_pager && length > Lpp) {
		signal(SIGPIPE, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if ((env_pager = getenv("PAGER")) == NULL || *env_pager == '\0') {
			snprintf(cmdbuf, sizeof(cmdbuf), _PATH_PAGER, Lpp);
		} else {
			snprintf(cmdbuf, sizeof(cmdbuf), "%s", env_pager);
		}
		outf = popen(cmdbuf, "w");
		if (!outf)
			outf = stdout;
		else
			setbuf(outf, (char *)NULL);
	}
	else
		outf = stdout;

	if (seensubj)
		putc('\n', outf);

	while (fgets(inbuf, sizeof inbuf, newmsg)) {
		fputs(inbuf, outf);
		if (ferror(outf)) {
			clearerr(outf);
			break;
		}
	}

	if (outf != stdout) {
		pclose(outf);
		signal(SIGPIPE, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	} else {
		fflush(stdout);
	}

	/* trick to force wait on output */
	tcdrain(fileno(stdout));
}

/* ARGSUSED */
void
onintr(int signo)
{
	int save_errno = errno;

	signal(SIGINT, onintr);
	if (mailing)
		unlink(fname);
	if (sending) {
		unlink(fname);
		write(STDOUT_FILENO, "--Killed--\n", strlen("--Killed--\n"));
		_exit(1);
	}
	if (printing) {
		write(STDOUT_FILENO, "\n", 1);
		if (hdrs)
			_exit(0);
		sep = "Interrupt";
		if (newmsg)
			fseeko(newmsg, (off_t)0, SEEK_END);
		intrpflg = YES;
	}
	errno = save_errno;
}

/*
 * We have just gotten a susp.  Suspend and prepare to resume.
 */
/* ARGSUSED */
void
onsusp(int signo)
{
	int save_errno = errno;
	sigset_t emptyset;

	signal(SIGTSTP, SIG_DFL);
	sigemptyset(&emptyset);
	sigprocmask(SIG_SETMASK, &emptyset, NULL);
	kill(0, SIGTSTP);
	signal(SIGTSTP, onsusp);
	errno = save_errno;

	if (!mailing)
		longjmp(tstpbuf, 1);
}

int
linecnt(FILE *f)
{
	off_t oldpos = ftello(f);

	int l = 0;
	char lbuf[BUFSIZ];

	while (fgets(lbuf, sizeof lbuf, f))
		l++;
	clearerr(f);
	fseeko(f, oldpos, SEEK_SET);
	return (l);
}

int
next(char *buf, int len)
{
	int i;
	sscanf(buf, "%d", &i);
	snprintf(buf, len, "Goto %d", i);
	return(--i);
}

void
ask(char *prompt)
{
	char	inch;
	int	n, cmsg, fd;
	off_t	oldpos;
	FILE	*cpfrom, *cpto;

	printf("%s ", prompt);
	fflush(stdout);
	intrpflg = NO;
	(void) fgets(inbuf, sizeof inbuf, stdin);
	if ((n = strlen(inbuf)) > 0 && inbuf[n - 1] == '\n')
		inbuf[n - 1] = '\0';
	if (intrpflg)
		inbuf[0] = 'x';

	/*
	 * Handle 'mail' and 'save' here.
	 */
	if (((inch = inbuf[0]) == 's' || inch == 'm') && !restricted) {
		if (inbuf[1] == '-')
			cmsg = prevmsg;
		else if (isdigit(inbuf[1]))
			cmsg = atoi(&inbuf[1]);
		else
			cmsg = msg;
		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, cmsg);

		oldpos = ftello(newmsg);

		cpfrom = fopen(fname, "r");
		if (!cpfrom) {
			printf("Message %d not found\n", cmsg);
			ask (prompt);
			return;
		}

		if (inch == 's') {
			in = nxtfld(inbuf);
			if (*in) {
				for (n=0;
				    in[n] > ' ' && n < sizeof fname - 1;
				    n++) {
					fname[n] = in[n];
				}
				fname[n] = NULL;
			}
			else
				strlcpy(fname, "Messages", sizeof fname);
			fd = open(fname, O_RDWR|O_EXCL|O_CREAT|O_APPEND, 0666);
		} else {
			strlcpy(fname, _PATH_TMPFILE, sizeof fname);
			fd = mkstemp(fname);
			if (fd != -1) {
				snprintf(cmdbuf, sizeof(cmdbuf), _PATH_MAIL, fname);
				mailing = YES;
			}
		}
		if (fd == -1 || (cpto = fdopen(fd, "a")) == NULL) {
			if (fd != -1)
				close(fd);
			perror(fname);
			mailing = NO;
			fseeko(newmsg, oldpos, SEEK_SET);
			ask(prompt);
			return;
		}

		while ((n = fread(inbuf, 1, sizeof inbuf, cpfrom)))
			fwrite(inbuf, 1, n, cpto);

		fclose(cpfrom);
		fclose(cpto);
		fseeko(newmsg, oldpos, SEEK_SET);	/* reposition current message */
		if (inch == 's')
			printf("Message %d saved in \"%s\"\n", cmsg, fname);
		else {
			system(cmdbuf);
			unlink(fname);
			mailing = NO;
		}
		ask(prompt);
	}
}

void
gfrsub(FILE *infile)
{
	off_t frompos;

	seensubj = seenfrom = NO;
	local = YES;
	subj[0] = from[0] = date[0] = NULL;

	/*
	 * Is this a normal message?
	 */
	if (fgets(inbuf, sizeof inbuf, infile)) {
		if (strncmp(inbuf, "From", 4)==0) {
			/*
			 * expected form starts with From
			 */
			seenfrom = YES;
			frompos = ftello(infile);
			ptr = from;
			in = nxtfld(inbuf);
			if (*in) {
				while (*in && *in > ' ' &&
				    ptr - from < sizeof from -1) {
					if (*in == ':' || *in == '@' || *in == '!')
						local = NO;
					*ptr++ = *in++;
				}
			}
			*ptr = NULL;
			if (*(in = nxtfld(in)))
				strncpy(date, in, sizeof date);
			else {
				date[0] = '\n';
				date[1] = NULL;
			}
		} else {
			/*
			 * not the expected form
			 */
			fseeko(infile, (off_t)0, SEEK_SET);
			return;
		}
	} else
		/*
		 * empty file ?
		 */
		return;

	/*
	 * look for Subject line until EOF or a blank line
	 */
	while (fgets(inbuf, sizeof inbuf, infile) &&
	    !(blankline = (inbuf[0] == '\n'))) {
		/*
		 * extract Subject line
		 */
		if (!seensubj && strncmp(inbuf, "Subj", 4)==0) {
			seensubj = YES;
			frompos = ftello(infile);
			strncpy(subj, nxtfld(inbuf), sizeof subj);
		}
	}
	if (!blankline)
		/*
		 * ran into EOF
		 */
		fseeko(infile, frompos, SEEK_SET);

	if (!seensubj)
		/*
		 * for possible use with Mail
		 */
		strncpy(subj, "(No Subject)\n", sizeof subj);
}

char *
nxtfld(char *s)
{
	if (*s)
		while (*s && *s > ' ')
			s++;	/* skip over this field */
	if (*s)
		while (*s && *s <= ' ')
			s++;	/* find start of next field */
	return (s);
}
