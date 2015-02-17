/*	$OpenBSD: main.c,v 1.100 2015/02/17 22:39:32 tedu Exp $	*/
/*	$NetBSD: main.c,v 1.24 1997/08/18 10:20:26 lukem Exp $	*/

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
 * FTP User Program -- Command Interface.
 */
#include <sys/types.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tls.h>

#include "cmds.h"
#include "ftp_var.h"

#ifndef SMALL
char * const ssl_verify_opts[] = {
#define SSL_CAFILE	0
	"cafile",
#define SSL_CAPATH	1
	"capath",
#define SSL_CIPHERS	2
	"ciphers",
#define SSL_DONTVERIFY	3
	"dont",
#define SSL_DOVERIFY	4
	"do",
#define SSL_VERIFYDEPTH	5
	"depth",
	NULL
};

struct tls_config *tls_config;
#endif /* !SMALL */

int family = PF_UNSPEC;
int pipeout;

int
main(volatile int argc, char *argv[])
{
	int ch, top, rval;
	struct passwd *pw = NULL;
	char *cp, homedir[PATH_MAX];
	char *outfile = NULL;
	const char *errstr;
	int dumb_terminal = 0;
#ifndef SMALL
	long long depth;
#endif

	ftpport = "ftp";
	httpport = "http";
#ifndef SMALL
	httpsport = "https";
#endif /* !SMALL */
	gateport = getenv("FTPSERVERPORT");
	if (gateport == NULL || *gateport == '\0')
		gateport = "ftpgate";
	doglob = 1;
	interactive = 1;
	autologin = 1;
	passivemode = 1;
	activefallback = 1;
	preserve = 1;
	verbose = 0;
	progress = 0;
	gatemode = 0;
#ifndef SMALL
	editing = 0;
	el = NULL;
	hist = NULL;
	cookiefile = NULL;
	resume = 0;
	srcaddr = NULL;
	marg_sl = sl_init();
#endif /* !SMALL */
	mark = HASHBYTES;
#ifdef INET6
	epsv4 = 1;
#else
	epsv4 = 0;
#endif
	epsv4bad = 0;

	/* Set default operation mode based on FTPMODE environment variable */
	if ((cp = getenv("FTPMODE")) != NULL && *cp != '\0') {
		if (strcmp(cp, "passive") == 0) {
			passivemode = 1;
			activefallback = 0;
		} else if (strcmp(cp, "active") == 0) {
			passivemode = 0;
			activefallback = 0;
		} else if (strcmp(cp, "gate") == 0) {
			gatemode = 1;
		} else if (strcmp(cp, "auto") == 0) {
			passivemode = 1;
			activefallback = 1;
		} else
			warnx("unknown FTPMODE: %s.  Using defaults", cp);
	}

	if (strcmp(__progname, "gate-ftp") == 0)
		gatemode = 1;
	gateserver = getenv("FTPSERVER");
	if (gateserver == NULL)
		gateserver = "";
	if (gatemode) {
		if (*gateserver == '\0') {
			warnx(
"Neither $FTPSERVER nor $GATE_SERVER is defined; disabling gate-ftp");
			gatemode = 0;
		}
	}

	cp = getenv("TERM");
	dumb_terminal = (cp == NULL || *cp == '\0' || !strcmp(cp, "dumb") ||
	    !strcmp(cp, "emacs") || !strcmp(cp, "su"));
	fromatty = isatty(fileno(stdin));
	if (fromatty) {
		verbose = 1;		/* verbose if from a tty */
#ifndef SMALL
		if (!dumb_terminal)
			editing = 1;	/* editing mode on if tty is usable */
#endif /* !SMALL */
	}

	ttyout = stdout;
	if (isatty(fileno(ttyout)) && !dumb_terminal && foregroundproc())
		progress = 1;		/* progress bar on if tty is usable */

#ifndef SMALL
	cookiefile = getenv("http_cookies");
	if (tls_config == NULL) {
		tls_config = tls_config_new();
		if (tls_config == NULL)
			errx(1, "tls config failed");
		tls_config_set_protocols(tls_config,
		    TLS_PROTOCOLS_ALL);
	}

#endif /* !SMALL */
	httpuseragent = NULL;

	while ((ch = getopt(argc, argv,
		    "46AaCc:dD:Eegik:Mmno:pP:r:S:s:tU:vV")) != -1) {
		switch (ch) {
		case '4':
			family = PF_INET;
			break;
		case '6':
			family = PF_INET6;
			break;
		case 'A':
			activefallback = 0;
			passivemode = 0;
			break;

		case 'a':
			anonftp = 1;
			break;

		case 'C':
#ifndef SMALL
			resume = 1;
#endif /* !SMALL */
			break;

		case 'c':
#ifndef SMALL
			cookiefile = optarg;
#endif /* !SMALL */
			break;

		case 'D':
			action = optarg;
			break;
		case 'd':
#ifndef SMALL
			options |= SO_DEBUG;
			debug++;
#endif /* !SMALL */
			break;

		case 'E':
			epsv4 = 0;
			break;

		case 'e':
#ifndef SMALL
			editing = 0;
#endif /* !SMALL */
			break;

		case 'g':
			doglob = 0;
			break;

		case 'i':
			interactive = 0;
			break;

		case 'k':
			keep_alive_timeout = strtonum(optarg, 0, INT_MAX, 
			    &errstr);
			if (errstr != NULL) {
				warnx("keep alive amount is %s: %s", errstr, 
					optarg);
				usage();
			}
			break;
		case 'M':
			progress = 0;
			break;
		case 'm':
			progress = -1;
			break;

		case 'n':
			autologin = 0;
			break;

		case 'o':
			outfile = optarg;
			if (*outfile == '\0') {
				pipeout = 0;
				outfile = NULL;
				ttyout = stdout;
			} else {
				pipeout = strcmp(outfile, "-") == 0;
				ttyout = pipeout ? stderr : stdout;
			}
			break;

		case 'p':
			passivemode = 1;
			activefallback = 0;
			break;

		case 'P':
			ftpport = optarg;
			break;

		case 'r':
			retry_connect = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				warnx("retry amount is %s: %s", errstr, 
					optarg);
				usage();
			}
			break;

		case 'S':
#ifndef SMALL
			cp = optarg;
			while (*cp) {
				char	*str;
				switch (getsubopt(&cp, ssl_verify_opts, &str)) {
				case SSL_CAFILE:
					if (str == NULL)
						errx(1, "missing CA file");
					if (tls_config_set_ca_file(
					    tls_config, str) != 0)
						errx(1, "tls ca file failed");
					break;
				case SSL_CAPATH:
					if (str == NULL)
						errx(1, "missing CA directory"
						    " path");
					if (tls_config_set_ca_path(
					    tls_config, str) != 0)
						errx(1, "tls ca path failed");
					break;
				case SSL_CIPHERS:
					if (str == NULL)
						errx(1, "missing cipher list");
					if (tls_config_set_ciphers(
					    tls_config, str) != 0)
						errx(1, "tls ciphers failed");
					break;
				case SSL_DONTVERIFY:
					tls_config_insecure_noverifyhost(
					    tls_config);
					tls_config_insecure_noverifycert(
					    tls_config);
					break;
				case SSL_DOVERIFY:
					tls_config_verify(tls_config);
					break;
				case SSL_VERIFYDEPTH:
					if (str == NULL)
						errx(1, "missing depth");
					depth = strtonum(str, 0, INT_MAX,
					    &errstr);
					if (errstr)
						errx(1, "certificate "
						    "validation depth is %s",
						    errstr);
					tls_config_set_verify_depth(
					    tls_config, (int)depth);
					break;
				default:
					errx(1, "unknown -S suboption `%s'",
					    suboptarg ? suboptarg : "");
					/* NOTREACHED */
				}
			}
#endif
			break;

		case 's':
#ifndef SMALL
			srcaddr = optarg;
#endif /* !SMALL */
			break;

		case 't':
			trace = 1;
			break;

#ifndef SMALL
		case 'U':
			free (httpuseragent);
			if (strcspn(optarg, "\r\n") != strlen(optarg))
				errx(1, "Invalid User-Agent: %s.", optarg);
			if (asprintf(&httpuseragent, "User-Agent: %s",
			    optarg) == -1)
				errx(1, "Can't allocate memory for HTTP(S) "
				    "User-Agent");
			break;
#endif /* !SMALL */

		case 'v':
			verbose = 1;
			break;

		case 'V':
			verbose = 0;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

#ifndef SMALL
	cookie_load();
#endif /* !SMALL */
	if (httpuseragent == NULL)
		httpuseragent = HTTP_USER_AGENT;

	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
	crflag = 1;	/* strip c.r. on ascii gets */
	sendport = -1;	/* not using ports */
	/*
	 * Set up the home directory in case we're globbing.
	 */
	cp = getlogin();
	if (cp != NULL) {
		pw = getpwnam(cp);
	}
	if (pw == NULL)
		pw = getpwuid(getuid());
	if (pw != NULL) {
		(void)strlcpy(homedir, pw->pw_dir, sizeof homedir);
		home = homedir;
	}

	setttywidth(0);
	(void)signal(SIGWINCH, setttywidth);

	if (argc > 0) {
		if (isurl(argv[0])) {
			rval = auto_fetch(argc, argv, outfile);
			if (rval >= 0)		/* -1 == connected and cd-ed */
				exit(rval);
		} else {
#ifndef SMALL
			char *xargv[5];

			if (setjmp(toplevel))
				exit(0);
			(void)signal(SIGINT, (sig_t)intr);
			(void)signal(SIGPIPE, (sig_t)lostpeer);
			xargv[0] = __progname;
			xargv[1] = argv[0];
			xargv[2] = argv[1];
			xargv[3] = argv[2];
			xargv[4] = NULL;
			do {
				setpeer(argc+1, xargv);
				if (!retry_connect)
					break;
				if (!connected) {
					macnum = 0;
					fputs("Retrying...\n", ttyout);
					sleep(retry_connect);
				}
			} while (!connected);
			retry_connect = 0; /* connected, stop hiding msgs */
#endif /* !SMALL */
		}
	}
#ifndef SMALL
	controlediting();
	top = setjmp(toplevel) == 0;
	if (top) {
		(void)signal(SIGINT, (sig_t)intr);
		(void)signal(SIGPIPE, (sig_t)lostpeer);
	}
	for (;;) {
		cmdscanner(top);
		top = 1;
	}
#else /* !SMALL */
	usage();
#endif /* !SMALL */
}

void
intr(void)
{
	int save_errno = errno;

	write(fileno(ttyout), "\n\r", 2);
	alarmtimer(0);

	errno = save_errno;
	longjmp(toplevel, 1);
}

void
lostpeer(void)
{
	int save_errno = errno;

	alarmtimer(0);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), SHUT_RDWR);
			(void)fclose(cout);
			cout = NULL;
		}
		if (data >= 0) {
			(void)shutdown(data, SHUT_RDWR);
			(void)close(data);
			data = -1;
		}
		connected = 0;
	}
	pswitch(1);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), SHUT_RDWR);
			(void)fclose(cout);
			cout = NULL;
		}
		connected = 0;
	}
	proxflag = 0;
	pswitch(0);
	errno = save_errno;
}

#ifndef SMALL
/*
 * Generate a prompt
 */
char *
prompt(void)
{
	return ("ftp> ");
}

/*
 * Command parser.
 */
void
cmdscanner(int top)
{
	struct cmd *c;
	int num;
	HistEvent hev;

	if (!top && !editing)
		(void)putc('\n', ttyout);
	for (;;) {
		if (!editing) {
			if (fromatty) {
				fputs(prompt(), ttyout);
				(void)fflush(ttyout);
			}
			if (fgets(line, sizeof(line), stdin) == NULL)
				quit(0, 0);
			num = strlen(line);
			if (num == 0)
				break;
			if (line[--num] == '\n') {
				if (num == 0)
					break;
				line[num] = '\0';
			} else if (num == sizeof(line) - 2) {
				fputs("sorry, input line too long.\n", ttyout);
				while ((num = getchar()) != '\n' && num != EOF)
					/* void */;
				break;
			} /* else it was a line without a newline */
		} else {
			const char *buf;
			cursor_pos = NULL;

			if ((buf = el_gets(el, &num)) == NULL || num == 0) {
				putc('\n', ttyout);
				fflush(ttyout);
				quit(0, 0);
			}
			if (buf[--num] == '\n') {
				if (num == 0)
					break;
			}
			if (num >= sizeof(line)) {
				fputs("sorry, input line too long.\n", ttyout);
				break;
			}
			memcpy(line, buf, (size_t)num);
			line[num] = '\0';
			history(hist, &hev, H_ENTER, buf);
		}

		makeargv();
		if (margc == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			fputs("?Ambiguous command.\n", ttyout);
			continue;
		}
		if (c == 0) {
			/*
			 * Give editline(3) a shot at unknown commands.
			 * XXX - bogus commands with a colon in
			 *       them will not elicit an error.
			 */
			if (editing &&
			    el_parse(el, margc, (const char **)margv) != 0)
				fputs("?Invalid command.\n", ttyout);
			continue;
		}
		if (c->c_conn && !connected) {
			fputs("Not connected.\n", ttyout);
			continue;
		}
		confirmrest = 0;
		(*c->c_handler)(margc, margv);
		if (bell && c->c_bell)
			(void)putc('\007', ttyout);
		if (c->c_handler != help)
			break;
	}
	(void)signal(SIGINT, (sig_t)intr);
	(void)signal(SIGPIPE, (sig_t)lostpeer);
}

struct cmd *
getcmd(const char *name)
{
	const char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	if (name == NULL)
		return (0);

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->c_name) != NULL; c++) {
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

int slrflag;

void
makeargv(void)
{
	char *argp;

	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	marg_sl->sl_cur = 0;		/* reset to start of marg_sl */
	for (margc = 0; ; margc++) {
		argp = slurpstring();
		sl_add(marg_sl, argp);
		if (argp == NULL)
			break;
	}
	if (cursor_pos == line) {
		cursor_argc = 0;
		cursor_argo = 0;
	} else if (cursor_pos != NULL) {
		cursor_argc = margc;
		cursor_argo = strlen(margv[margc-1]);
	}
}

#define INC_CHKCURSOR(x)	{ (x)++ ; \
				if (x == cursor_pos) { \
					cursor_argc = margc; \
					cursor_argo = ap-argbase; \
					cursor_pos = NULL; \
				} }

/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
char *
slurpstring(void)
{
	int got_one = 0;
	char *sb = stringbase;
	char *ap = argbase;
	char *tmp = argbase;		/* will return this if token found */

	if (*sb == '!' || *sb == '$') {	/* recognize ! as a token for shell */
		switch (slrflag) {	/* and $ as token for macro invoke */
			case 0:
				slrflag++;
				INC_CHKCURSOR(stringbase);
				return ((*sb == '!') ? "!" : "$");
				/* NOTREACHED */
			case 1:
				slrflag++;
				altarg = stringbase;
				break;
			default:
				break;
		}
	}

S0:
	switch (*sb) {

	case '\0':
		goto OUT;

	case ' ':
	case '\t':
		INC_CHKCURSOR(sb);
		goto S0;

	default:
		switch (slrflag) {
			case 0:
				slrflag++;
				break;
			case 1:
				slrflag++;
				altarg = sb;
				break;
			default:
				break;
		}
		goto S1;
	}

S1:
	switch (*sb) {

	case ' ':
	case '\t':
	case '\0':
		goto OUT;	/* end of token */

	case '\\':
		INC_CHKCURSOR(sb);
		goto S2;	/* slurp next character */

	case '"':
		INC_CHKCURSOR(sb);
		goto S3;	/* slurp quoted string */

	default:
		*ap = *sb;	/* add character to token */
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		INC_CHKCURSOR(sb);
		goto S1;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S3;
	}

OUT:
	if (got_one)
		*ap++ = '\0';
	argbase = ap;			/* update storage pointer */
	stringbase = sb;		/* update scan pointer */
	if (got_one) {
		return (tmp);
	}
	switch (slrflag) {
		case 0:
			slrflag++;
			break;
		case 1:
			slrflag++;
			altarg = (char *) 0;
			break;
		default:
			break;
	}
	return ((char *)0);
}

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help(int argc, char *argv[])
{
	struct cmd *c;

	if (argc == 1) {
		StringList *buf;

		buf = sl_init();
		fprintf(ttyout, "%sommands may be abbreviated.  Commands are:\n\n",
		    proxy ? "Proxy c" : "C");
		for (c = cmdtab; c < &cmdtab[NCMDS]; c++)
			if (c->c_name && (!proxy || c->c_proxy))
				sl_add(buf, c->c_name);
		list_vertical(buf);
		sl_free(buf, 0);
		return;
	}

#define HELPINDENT ((int) sizeof("disconnect"))

	while (--argc > 0) {
		char *arg;

		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			fprintf(ttyout, "?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			fprintf(ttyout, "?Invalid help command %s\n", arg);
		else
			fprintf(ttyout, "%-*s\t%s\n", HELPINDENT,
				c->c_name, c->c_help);
	}
}
#endif /* !SMALL */

void
usage(void)
{
	fprintf(stderr, "usage: "
#ifndef SMALL
	    "%1$s [-46AadEegiMmnptVv] [-D title] [-k seconds] [-P port] "
	    "[-r seconds]\n"
	    "           [-s srcaddr] [host [port]]\n"
	    "       %1$s [-C] [-o output] [-s srcaddr]\n"
	    "           ftp://[user:password@]host[:port]/file[/] ...\n"
	    "       %1$s [-C] [-c cookie] [-o output] [-S ssl_options] "
	    "[-s srcaddr]\n"
	    "           [-U useragent] "
	    "http[s]://[user:password@]host[:port]/file ...\n"
	    "       %1$s [-C] [-o output] [-s srcaddr] file:file ...\n"
	    "       %1$s [-C] [-o output] [-s srcaddr] host:/file[/] ...\n",
#else /* !SMALL */
	    "%1$s [-o output] ftp://[user:password@]host[:port]/file[/] ...\n"
	    "       %1$s [-o output] http://host[:port]/file ...\n"
	    "       %1$s [-o output] file:file ...\n"
	    "       %1$s [-o output] host:/file[/] ...\n",
#endif /* !SMALL */
	    __progname);
	exit(1);
}
