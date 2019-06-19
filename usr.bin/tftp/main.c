/*	$OpenBSD: main.c,v 1.43 2018/09/20 11:42:42 jsg Exp $	*/
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

/*
 * TFTP User Program -- Command Interface
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */

#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/tftp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "extern.h"

#define	LBUFLEN		200		/* size of input buffer */
#define	MAXARGV		20
#define HELPINDENT	(sizeof("connect"))

void			 get(int, char **);
void			 help(int, char **);
void			 modecmd(int, char **);
void			 put(int, char **);
void			 quit(int, char **);
void			 setascii(int, char **);
void			 setbinary(int, char **);
void			 setpeer(char *, char *);
void			 parsearg(int, char **);
void			 setrexmt(int, char **);
void			 settimeout(int, char **);
void			 settrace(int, char **);
void			 setverbose(int, char **);
void			 settsize(int, char **);
void			 settout(int, char **);
void			 setblksize(int, char **);
void			 status(int, char **);
int			 readcmd(char *, int, FILE *);
static void		 getusage(char *);
static int		 makeargv(void);
static void		 putusage(char *);
static void		 settftpmode(char *);
static __dead void	 command(void);
struct cmd	 	*getcmd(char *);
char			*tail(char *);

struct sockaddr_storage	 peeraddr;
int			 f;
int			 trace;
int			 verbose;
int			 connected;
char			 mode[32];
char			 line[LBUFLEN];
int			 margc;
char			*margv[MAXARGV+1];
char			*prompt = "tftp";
void			 intr(int);
int	 		 rexmtval = TIMEOUT;
int	 		 maxtimeout = 5 * TIMEOUT;
char	 		 hostname[HOST_NAME_MAX+1];
FILE			*file = NULL;
volatile sig_atomic_t	 intrflag = 0;
char			*ackbuf;
int			 has_options = 0;
int			 opt_tsize = 0;
int			 opt_tout = 0;
int			 opt_blksize = 0;

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
char	ashelp[] = "set mode to netascii";
char	bnhelp[] = "set mode to octet";
char	oshelp[] = "toggle tsize option";
char	othelp[] = "toggle timeout option";
char	obhelp[] = "set alternative blksize option";

struct cmd {
	char	*name;
	char	*help;
	void	 (*handler)(int, char **);
};

struct cmd cmdtab[] = {
	{ "connect",	chelp,	parsearg },
	{ "mode",       mhelp,	modecmd },
	{ "put",	shelp,	put },
	{ "get",	rhelp,	get },
	{ "quit",	qhelp,	quit },
	{ "verbose",	vhelp,	setverbose },
	{ "trace",	thelp,	settrace },
	{ "status",	sthelp,	status },
	{ "binary",     bnhelp,	setbinary },
	{ "ascii",      ashelp,	setascii },
	{ "rexmt",	xhelp,	setrexmt },
	{ "timeout",	ihelp,	settimeout },
	{ "tsize",	oshelp, settsize },
	{ "tout",	othelp, settout },
	{ "blksize",	obhelp,	setblksize },
	{ "help",	hhelp,	help },
	{ "?",		hhelp,	help },
	{ NULL,		NULL,	NULL }
};

struct	modes {
	char	*m_name;
	char	*m_mode;
} modes[] = {
	{ "ascii",	"netascii" },
	{ "netascii",	"netascii" },
	{ "binary",	"octet" },
	{ "image",	"octet" },
	{ "octet",	"octet" },
/*	{ "mail",	"mail" }, */
	{ NULL,		NULL }
};

int
main(int argc, char *argv[])
{
	f = -1;

	if (pledge("stdio rpath wpath cpath dns inet", NULL) == -1)
		err(1, "pledge");

	/* set default transfer mode */
	strlcpy(mode, "netascii", sizeof(mode));

	/* set peer if given */
	if (argc > 1)
		parsearg(argc, argv);

	/* catch SIGINT */
	signal(SIGINT, intr);

	/* allocate memory for packets */
	if ((ackbuf = malloc(SEGSIZE_MAX + 4)) == NULL)
		err(1, "malloc");

	/* command prompt */
	command();

	return (0);
}

void
setpeer(char *host, char *port)
{
	struct addrinfo hints, *res0, *res;
	int error;
	struct sockaddr_storage ss;
	char *cause = "unknown";

	if (connected) {
		close(f);
		f = -1;
	}
	connected = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_CANONNAME;
	if (!port)
		port = "tftp";
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		warnx("%s", gai_strerror(error));
		return;
	}

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_addrlen > sizeof(peeraddr))
			continue;
		f = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (f < 0) {
			cause = "socket";
			continue;
		}

		memset(&ss, 0, sizeof(ss));
		ss.ss_family = res->ai_family;
		if (bind(f, (struct sockaddr *)&ss, res->ai_addrlen) < 0) {
			cause = "bind";
			close(f);
			f = -1;
			continue;
		}

		break;
	}

	if (f < 0)
		warn("%s", cause);
	else {
		/* res->ai_addr <= sizeof(peeraddr) is guaranteed */
		memcpy(&peeraddr, res->ai_addr, res->ai_addrlen);
		if (res->ai_canonname) {
			(void)strlcpy(hostname, res->ai_canonname,
			    sizeof(hostname));
		} else
			(void)strlcpy(hostname, host, sizeof(hostname));
		connected = 1;
	}
	freeaddrinfo(res0);
}

void
parsearg(int argc, char *argv[])
{
	if (argc < 2) {
		strlcpy(line, "Connect ", sizeof(line));
		printf("(to) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if ((argc < 2) || (argc > 3)) {
		printf("usage: %s [host [port]]\n", argv[0]);
		return;
	}
	if (argc == 2)
		setpeer(argv[1], NULL);
	else
		setpeer(argv[1], argv[2]);
}

void
modecmd(int argc, char *argv[])
{
	struct modes	*p;
	char		*sep;

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

/* ARGSUSED */
void
setbinary(int argc, char *argv[])
{
	settftpmode("octet");
}

/* ARGSUSED */
void
setascii(int argc, char *argv[])
{
	settftpmode("netascii");
}

static void
settftpmode(char *newmode)
{
	strlcpy(mode, newmode, sizeof(mode));
	if (verbose)
		printf("mode set to %s\n", mode);
}

/*
 * Send file(s).
 */
void
put(int argc, char *argv[])
{
	int	 fd;
	int	 n;
	char	*cp, *targ;

	if (argc < 2) {
		strlcpy(line, "put ", sizeof(line));
		printf("(file) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
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
	if (strrchr(argv[argc - 1], ':')) {

		for (n = 1; n < argc - 1; n++)
			if (strchr(argv[n], ':')) {
				putusage(argv[0]);
				return;
			}
		cp = argv[argc - 1];
		targ = strrchr(cp, ':');
		*targ++ = 0;
		if (cp[0] == '[' && cp[strlen(cp) - 1] == ']') {
			cp[strlen(cp) - 1] = '\0';
			cp++;
		}
		setpeer(cp, NULL);
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
		sendfile(fd, targ, mode);
		return;
	}

	/*
	 * this assumes the target is a directory on
	 * on a remote unix system.  hmmmm.
	 */
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
		sendfile(fd, cp, mode);
		free(cp);
	}
}

static void
putusage(char *s)
{
	printf("usage: %s file [[host:]remotename]\n", s);
	printf("       %s file1 file2 ... fileN [[host:]remote-directory]\n",
	    s);
}

/*
 * Receive file(s).
 */
void
get(int argc, char *argv[])
{
	int	 fd;
	int	 n;
	char	*cp;
	char	*src;

	if (argc < 2) {
		strlcpy(line, "get ", sizeof(line));
		printf("(files) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
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
		for (n = 1; n < argc; n++)
			if (strrchr(argv[n], ':') == 0) {
				getusage(argv[0]);
				return;
			}
	}
	for (n = 1; n < argc; n++) {
		src = strrchr(argv[n], ':');
		if (src == NULL)
			src = argv[n];
		else {
			char *cp;

			*src++ = 0;
			cp = argv[n];
			if (cp[0] == '[' && cp[strlen(cp) - 1] == ']') {
				cp[strlen(cp) - 1] = '\0';
				cp++;
			}
			setpeer(cp, NULL);
			if (!connected)
				continue;
		}
		if (argc < 4) {
			cp = argc == 3 ? argv[2] : tail(src);
			fd = open(cp, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			if (fd < 0) {
				warn("create: %s", cp);
				return;
			}
			if (verbose)
				printf("getting from %s:%s to %s [%s]\n",
				    hostname, src, cp, mode);
			recvfile(fd, src, mode);
			break;
		}
		cp = tail(src);	/* new .. jdg */
		fd = open(cp, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (fd < 0) {
			warn("create: %s", cp);
			continue;
		}
		if (verbose)
			printf("getting from %s:%s to %s [%s]\n",
			    hostname, src, cp, mode);
		recvfile(fd, src, mode);
	}
}

static void
getusage(char *s)
{
	printf("usage: %s [host:]file [localname]\n", s);
	printf("       %s [host1:]file1 [host2:]file2 ... [hostN:]fileN\n", s);
}

void
setrexmt(int argc, char *argv[])
{
	int		 t;
	const char	*errstr;

	if (argc < 2) {
		strlcpy(line, "Rexmt-timeout ", sizeof(line));
		printf("(value) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = strtonum(argv[1], TIMEOUT_MIN, TIMEOUT_MAX, &errstr);
	if (errstr)
		printf("%s: value is %s\n", argv[1], errstr);
	else
		rexmtval = t;
}

void
settimeout(int argc, char *argv[])
{
	int		 t;
	const char	*errstr;

	if (argc < 2) {
		strlcpy(line, "Maximum-timeout ", sizeof(line));
		printf("(value) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = strtonum(argv[1], TIMEOUT_MIN, TIMEOUT_MAX, &errstr);
	if (errstr)
		printf("%s: value is %s\n", argv[1], errstr);
	else
		maxtimeout = t;
}

/* ARGSUSED */
void
status(int argc, char *argv[])
{
	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	printf("Mode: %s Verbose: %s Tracing: %s\n",
	    mode, verbose ? "on" : "off", trace ? "on" : "off");
	printf("Rexmt-interval: %d seconds, Max-timeout: %d seconds\n",
	    rexmtval, maxtimeout);
}

/* ARGSUSED */
void
intr(int signo)
{
	intrflag = 1;
}

char *
tail(char *filename)
{
	char	*s;

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
	struct cmd	*c;

	for (;;) {
		printf("%s> ", prompt);
		if (readcmd(line, LBUFLEN, stdin) < 1)
			continue;
		if ((line[0] == 0) || (line[0] == '\n'))
			continue;
		if (makeargv())
			continue;
		if (margc == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *) - 1) {
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
	char		*p, *q;
	struct cmd	*c, *found;
	int		 nmatches, longest;

	longest = 0;
	nmatches = 0;
	found = 0;
	intrflag = 0;
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
		return ((struct cmd *) - 1);

	return (found);
}

/*
 * Slice a string up into argc/argv.
 */
static int
makeargv(void)
{
	char	 *cp;
	char	**argp = margv;
	int	  ret = 0;

	margc = 0;
	for (cp = line; *cp;) {
		if (margc >= MAXARGV) {
			printf("too many arguments\n");
			ret = 1;
			break;
		}
		while (isspace((unsigned char)*cp))
			cp++;
		if (*cp == '\0')
			break;
		*argp++ = cp;
		margc += 1;
		while (*cp != '\0' && !isspace((unsigned char)*cp))
			cp++;
		if (*cp == '\0')
			break;
		*cp++ = '\0';
	}
	*argp++ = 0;

	return (ret);
}

/* ARGSUSED */
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
	struct cmd	*c;

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
		if (c == (struct cmd *) - 1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == NULL)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%s\n", c->help);
	}
}

/* ARGSUSED */
void
settrace(int argc, char *argv[])
{
	trace = !trace;
	printf("Packet tracing %s.\n", trace ? "on" : "off");
}

/* ARGSUSED */
void
setverbose(int argc, char *argv[])
{
	verbose = !verbose;
	printf("Verbose mode %s.\n", verbose ? "on" : "off");
}

/* ARGSUSED */
void
settsize(int argc, char *argv[])
{
	opt_tsize = !opt_tsize;
	printf("Tsize option %s.\n", opt_tsize ? "on" : "off");
	if (opt_tsize)
		has_options++;
	else
		has_options--;
}

/* ARGSUSED */
void
settout(int argc, char *argv[])
{
	opt_tout = !opt_tout;
	printf("Timeout option %s.\n", opt_tout ? "on" : "off");
	if (opt_tout)
		has_options++;
	else
		has_options--;
}

void
setblksize(int argc, char *argv[])
{
	int		 t;
	const char	*errstr;

	if (argc < 2) {
		strlcpy(line, "Blocksize ", sizeof(line));
		printf("(value) ");
		readcmd(&line[strlen(line)], LBUFLEN - strlen(line), stdin);
		if (makeargv())
			return;
		argc = margc;
		argv = margv;
	}
	if (argc != 2) {
		printf("usage: %s value\n", argv[0]);
		return;
	}
	t = strtonum(argv[1], SEGSIZE_MIN, SEGSIZE_MAX, &errstr);
	if (errstr)
		printf("%s: value is %s\n", argv[1], errstr);
	else {
		if (opt_blksize == 0)
			has_options++;
		opt_blksize = t;
	}
}

int
readcmd(char *input, int len, FILE *stream)
{
	int		nfds;
	struct pollfd	pfd[1];

	fflush(stdout);

	pfd[0].fd = 0;
	pfd[0].events = POLLIN;
	nfds = poll(pfd, 1, INFTIM);
	if (nfds == -1) {
		if (intrflag) {
			intrflag = 0;
			putchar('\n');
			return (0);
		}
		exit(1);
	}

	if (fgets(input, len, stream) == NULL) {
		if (feof(stdin))
			exit(0);
		else
			return (-1);
	}

	return (1);
}
