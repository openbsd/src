/*	$OpenBSD: main.c,v 1.23 2006/01/23 17:29:22 millert Exp $	*/
/*	$NetBSD: main.c,v 1.6 1995/05/21 16:54:10 mycroft Exp $	*/

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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] = "$OpenBSD: main.c,v 1.23 2006/01/23 17:29:22 millert Exp $";
#endif /* not lint */

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Command Interface.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "extern.h"

#define	TIMEOUT		5		/* secs between rexmt's */
#define	LBUFLEN		200		/* size of input buffer */
#define	MAXARGV		20

struct	sockaddr_in peeraddr;
int	f;
short   port;
int	trace;
int	verbose;
int	connected;
char	mode[32];
char	line[LBUFLEN];
int	margc;
char	*margv[MAXARGV+1];
char	*prompt = "tftp";
jmp_buf	toplevel;
void	intr(int);
struct	servent *sp;

void	get(int, char **);
void	help(int, char **);
void	modecmd(int, char **);
void	put(int, char **);
void	quit(int, char **);
void	setascii(int, char **);
void	setbinary(int, char **);
void	setpeer(int, char **);
void	setrexmt(int, char **);
void	settimeout(int, char **);
void	settrace(int, char **);
void	setverbose(int, char **);
void	status(int, char **);

static __dead void command(void);

static void getusage(char *);
static int makeargv(void);
static void putusage(char *);
static void settftpmode(char *);

#define HELPINDENT (sizeof("connect"))

struct cmd {
	char	*name;
	char	*help;
	void	(*handler)(int, char **);
};

char	vhelp[] = "toggle verbose mode";
char	thelp[] = "toggle packet tracing";
char	chelp[] = "connect to remote tftp";
char	qhelp[] = "exit tftp";
char	hhelp[] = "print help information";
char	shelp[] = "send file";
char	rhelp[] = "receive file";
char	mhelp[] = "set file transfer mode";
char	sthelp[] = "show current status";
char	xhelp[] = "set per-packet retransmission timeout";
char	ihelp[] = "set total retransmission timeout";
char    ashelp[] = "set mode to netascii";
char    bnhelp[] = "set mode to octet";

struct cmd cmdtab[] = {
	{ "connect",	chelp,		setpeer },
	{ "mode",       mhelp,          modecmd },
	{ "put",	shelp,		put },
	{ "get",	rhelp,		get },
	{ "quit",	qhelp,		quit },
	{ "verbose",	vhelp,		setverbose },
	{ "trace",	thelp,		settrace },
	{ "status",	sthelp,		status },
	{ "binary",     bnhelp,         setbinary },
	{ "ascii",      ashelp,         setascii },
	{ "rexmt",	xhelp,		setrexmt },
	{ "timeout",	ihelp,		settimeout },
	{ "?",		hhelp,		help },
	{ NULL,		NULL,		NULL }
};

struct	cmd *getcmd(char *);
char	*tail(char *);

int
main(int argc, char *argv[])
{
	struct sockaddr_in s_in;

	sp = getservbyname("tftp", "udp");
	if (sp == 0)
		errx(1, "udp/tftp: unknown service");
	f = socket(AF_INET, SOCK_DGRAM, 0);
	if (f < 0)
		err(3, "socket");
	bzero((char *)&s_in, sizeof (s_in));
	s_in.sin_family = AF_INET;
	if (bind(f, (struct sockaddr *)&s_in, sizeof (s_in)) < 0)
		err(1, "bind");
	strlcpy(mode, "netascii", sizeof mode);
	signal(SIGINT, intr);
	if (argc > 1) {
		if (setjmp(toplevel) != 0)
			exit(0);
		setpeer(argc, argv);
	}
	if (setjmp(toplevel) != 0)
		(void)putchar('\n');
	command();
	return (0);
}

char    hostname[MAXHOSTNAMELEN];

void
setpeer(int argc, char *argv[])
{
	struct hostent *host;

	if (argc < 2) {
		strlcpy(line, "Connect ", sizeof line);
		printf("(to) ");
		fgets(&line[strlen(line)], LBUFLEN-strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if ((argc < 2) || (argc > 3)) {
		printf("usage: %s [host [port]]\n", argv[0]);
		return;
	}
	if (inet_aton(argv[1], &peeraddr.sin_addr) != 0) {
		peeraddr.sin_family = AF_INET;
		(void) strncpy(hostname, argv[1], sizeof hostname);
		hostname[sizeof(hostname)-1] = '\0';
	} else {
		host = gethostbyname(argv[1]);
		if (host == 0) {
			connected = 0;
			printf("%s: unknown host\n", argv[1]);
			return;
		}
		peeraddr.sin_family = host->h_addrtype;
		bcopy(host->h_addr, &peeraddr.sin_addr, host->h_length);
		(void) strlcpy(hostname, host->h_name, sizeof hostname);
	}
	port = sp->s_port;
	if (argc == 3) {
		port = atoi(argv[2]);
		if (port < 0) {
			printf("%s: bad port number\n", argv[2]);
			connected = 0;
			return;
		}
		port = htons(port);
	}
	connected = 1;
}

struct	modes {
	char *m_name;
	char *m_mode;
} modes[] = {
	{ "ascii",	"netascii" },
	{ "netascii",   "netascii" },
	{ "binary",     "octet" },
	{ "image",      "octet" },
	{ "octet",     "octet" },
/*      { "mail",       "mail" },       */
	{ NULL,		NULL }
};

void
modecmd(int argc, char *argv[])
{
	struct modes *p;
	char *sep;

	if (argc < 2) {
		printf("Using %s mode to transfer files.\n", mode);
		return;
	}
	if (argc == 2) {
		for (p = modes; p->m_name != NULL; p++)
			if (strcmp(argv[1], p->m_name) == 0)
				break;
		if (p->m_name) {
			settftpmode(p->m_mode);
			return;
		}
		printf("%s: unknown mode\n", argv[1]);
		/* drop through and print usage message */
	}

	printf("usage: %s [", argv[0]);
	sep = " ";
	for (p = modes; p->m_name != NULL; p++) {
		printf("%s%s", sep, p->m_name);
		if (*sep == ' ')
			sep = " | ";
	}
	printf(" ]\n");
	return;
}

void
setbinary(int argc, char *argv[])
{

	settftpmode("octet");
}

void
setascii(int argc, char *argv[])
{

	settftpmode("netascii");
}

static void
settftpmode(char *newmode)
{
	strlcpy(mode, newmode, sizeof mode);
	if (verbose)
		printf("mode set to %s\n", mode);
}


/*
 * Send file(s).
 */
void
put(int argc, char *argv[])
{
	int fd;
	int n;
	char *cp, *targ;

	if (argc < 2) {
		strlcpy(line, "send ", sizeof line);
		printf("(file) ");
		fgets(&line[strlen(line)], LBUFLEN-strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		putusage(argv[0]);
		return;
	}
	targ = argv[argc - 1];
	if (strchr(argv[argc - 1], ':')) {
		char *cp;
		struct hostent *hp;

		for (n = 1; n < argc - 1; n++)
			if (strchr(argv[n], ':')) {
				putusage(argv[0]);
				return;
			}
		cp = argv[argc - 1];
		targ = strchr(cp, ':');
		*targ++ = 0;
		hp = gethostbyname(cp);
		if (hp == NULL) {
			warnx("%s: %s", cp, hstrerror(h_errno));
			return;
		}
		bcopy(hp->h_addr, (caddr_t)&peeraddr.sin_addr, hp->h_length);
		peeraddr.sin_family = hp->h_addrtype;
		connected = 1;
		port = sp->s_port;
		strlcpy(hostname, hp->h_name, sizeof hostname);
	}
	if (!connected) {
		printf("No target machine specified.\n");
		return;
	}
	if (argc < 4) {
		cp = argc == 2 ? tail(targ) : argv[1];
		fd = open(cp, O_RDONLY);
		if (fd < 0) {
			warn("open: %s", cp);
			return;
		}
		if (verbose)
			printf("putting %s to %s:%s [%s]\n",
			    cp, hostname, targ, mode);
		peeraddr.sin_port = port;
		sendfile(fd, targ, mode);
		return;
	}

	/* this assumes the target is a directory */
	/* on a remote unix system.  hmmmm.  */
	for (n = 1; n < argc - 1; n++) {
		if (asprintf(&cp, "%s/%s", targ, tail(argv[n])) == -1)
			err(1, "asprintf");
		fd = open(argv[n], O_RDONLY);
		if (fd < 0) {
			warn("open: %s", argv[n]);
			free(cp);
			continue;
		}
		if (verbose)
			printf("putting %s to %s:%s [%s]\n",
			    argv[n], hostname, cp, mode);
		peeraddr.sin_port = port;
		sendfile(fd, cp, mode);
		free(cp);
	}
}

static void
putusage(char *s)
{
	printf("usage: %s file [[host:]remotename]\n", s);
	printf("       %s file1 file2 ... fileN [[host:]remote-directory]\n", s);
}

/*
 * Receive file(s).
 */
void
get(int argc, char *argv[])
{
	int fd;
	int n;
	char *cp;
	char *src;

	if (argc < 2) {
		strlcpy(line, "get ", sizeof line);
		printf("(files) ");
		fgets(&line[strlen(line)], LBUFLEN-strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc < 2) {
		getusage(argv[0]);
		return;
	}
	if (!connected) {
		for (n = 1; n < argc ; n++)
			if (strchr(argv[n], ':') == 0) {
				getusage(argv[0]);
				return;
			}
	}
	for (n = 1; n < argc ; n++) {
		src = strchr(argv[n], ':');
		if (src == NULL)
			src = argv[n];
		else {
			struct hostent *hp;

			*src++ = 0;
			hp = gethostbyname(argv[n]);
			if (hp == NULL) {
				warnx("%s: %s", argv[n], hstrerror(h_errno));
				continue;
			}
			bcopy(hp->h_addr, (caddr_t)&peeraddr.sin_addr,
			    hp->h_length);
			peeraddr.sin_family = hp->h_addrtype;
			connected = 1;
			strlcpy(hostname, hp->h_name, sizeof hostname);
		}
		if (argc < 4) {
			cp = argc == 3 ? argv[2] : tail(src);
			fd = creat(cp, 0644);
			if (fd < 0) {
				warn("create: %s", cp);
				return;
			}
			if (verbose)
				printf("getting from %s:%s to %s [%s]\n",
				    hostname, src, cp, mode);
			peeraddr.sin_port = port;
			recvfile(fd, src, mode);
			break;
		}
		cp = tail(src);         /* new .. jdg */
		fd = creat(cp, 0644);
		if (fd < 0) {
			warn("create: %s", cp);
			continue;
		}
		if (verbose)
			printf("getting from %s:%s to %s [%s]\n",
			    hostname, src, cp, mode);
		peeraddr.sin_port = port;
		recvfile(fd, src, mode);
	}
}

static void
getusage(char *s)
{
	printf("usage: %s [host:]file [localname]\n", s);
	printf("       %s [host1:]file1 [host2:]file2 ... [hostN:]fileN\n", s);
}

int	rexmtval = TIMEOUT;

void
setrexmt(int argc, char *argv[])
{
	int t;

	if (argc < 2) {
		strlcpy(line, "Rexmt-timeout ", sizeof line);
		printf("(value) ");
		fgets(&line[strlen(line)], LBUFLEN-strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = atoi(argv[1]);
	if (t < 0)
		printf("%s: bad value\n", argv[1]);
	else
		rexmtval = t;
}

int	maxtimeout = 5 * TIMEOUT;

void
settimeout(int argc, char *argv[])
{
	int t;

	if (argc < 2) {
		strlcpy(line, "Maximum-timeout ", sizeof line);
		printf("(value) ");
		fgets(&line[strlen(line)], LBUFLEN-strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = atoi(argv[1]);
	if (t < 0)
		printf("%s: bad value\n", argv[1]);
	else
		maxtimeout = t;
}

void
status(int argc, char *argv[])
{
	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	printf("Mode: %s Verbose: %s Tracing: %s\n", mode,
		verbose ? "on" : "off", trace ? "on" : "off");
	printf("Rexmt-interval: %d seconds, Max-timeout: %d seconds\n",
		rexmtval, maxtimeout);
}

void
intr(int signo)
{

	signal(SIGALRM, SIG_IGN);
	alarm(0);
	longjmp(toplevel, -1);
}

char *
tail(char *filename)
{
	char *s;

	while (*filename) {
		s = strrchr(filename, '/');
		if (s == NULL)
			break;
		if (s[1])
			return (s + 1);
		*s = '\0';
	}
	return (filename);
}

/*
 * Command parser.
 */
static __dead void
command(void)
{
	struct cmd *c;

	for (;;) {
		printf("%s> ", prompt);
		if (fgets(line, LBUFLEN, stdin) == 0) {
			if (feof(stdin)) {
				exit(0);
			} else {
				continue;
			}
		}
		if ((line[0] == 0) || (line[0] == '\n'))
			continue;
		if (makeargv())
			continue;
		if (margc == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			printf("?Invalid command\n");
			continue;
		}
		(*c->handler)(margc, margv);
	}
}

struct cmd *
getcmd(char *name)
{
	char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->name) != NULL; c++) {
		for (q = name; *q == *p++; q++)
			if (*q == 0)		/* exact match? */
				return (c);
		if (!*q) {			/* the name was a prefix */
			if (q - name > longest) {
				longest = q - name;
				nmatches = 1;
				found = c;
			} else if (q - name == longest)
				nmatches++;
		}
	}
	if (nmatches > 1)
		return ((struct cmd *)-1);
	return (found);
}

/*
 * Slice a string up into argc/argv.
 */
static int
makeargv(void)
{
	char *cp;
	char **argp = margv;
	int ret = 0;

	margc = 0;
	for (cp = line; *cp;) {
		if (margc >= MAXARGV) {
			printf("too many arguments\n");
			ret = 1;
			break;
		}
		while (isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*argp++ = cp;
		margc += 1;
		while (*cp != '\0' && !isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*cp++ = '\0';
	}
	*argp++ = 0;
	return (ret);
}

void
quit(int argc, char *argv[])
{

	exit(0);
}

/*
 * Help command.
 */
void
help(int argc, char *argv[])
{
	struct cmd *c;

	if (argc == 1) {
		printf("Commands may be abbreviated.  Commands are:\n\n");
		for (c = cmdtab; c->name != NULL; c++)
			printf("%-*s\t%s\n", (int)HELPINDENT, c->name, c->help);
		return;
	}
	while (--argc > 0) {
		char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%s\n", c->help);
	}
}

void
settrace(int argc, char *argv[])
{
	trace = !trace;
	printf("Packet tracing %s.\n", trace ? "on" : "off");
}

void
setverbose(int argc, char *argv[])
{
	verbose = !verbose;
	printf("Verbose mode %s.\n", verbose ? "on" : "off");
}
