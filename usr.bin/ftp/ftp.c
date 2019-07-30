/*	$OpenBSD: ftp.c,v 1.100 2016/08/22 16:27:00 millert Exp $	*/
/*	$NetBSD: ftp.c,v 1.27 1997/08/18 10:20:23 lukem Exp $	*/

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include "ftp_var.h"

union sockaddr_union {
	struct sockaddr     sa;
	struct sockaddr_in  sin;
	struct sockaddr_in6 sin6;
};

union sockaddr_union myctladdr, hisctladdr, data_addr;

int	data = -1;
int	abrtflag = 0;
jmp_buf	ptabort;
int	ptabflg;
int	ptflag = 0;
off_t	restart_point = 0;


FILE	*cin, *cout;

char *
hookup(char *host, char *port)
{
	int s, tos, error;
	static char hostnamebuf[HOST_NAME_MAX+1];
	struct addrinfo hints, *res, *res0;
#ifndef SMALL
	struct addrinfo *ares;
#endif
	char hbuf[NI_MAXHOST];
	char *cause = "unknown";
	socklen_t namelen;

	epsv4bad = 0;

	memset((char *)&hisctladdr, 0, sizeof (hisctladdr));
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error == EAI_SERVICE) {
		/*
		 * If the services file is corrupt/missing, fall back
		 * on our hard-coded defines.
		 */
		char pbuf[NI_MAXSERV];

		pbuf[0] = '\0';
		if (strcmp(port, "ftp") == 0)
			snprintf(pbuf, sizeof(pbuf), "%d", FTP_PORT);
		else if (strcmp(port, "ftpgate") == 0)
			snprintf(pbuf, sizeof(pbuf), "%d", GATE_PORT);
		else if (strcmp(port, "http") == 0)
			snprintf(pbuf, sizeof(pbuf), "%d", HTTP_PORT);
#ifndef SMALL
		else if (strcmp(port, "https") == 0)
			snprintf(pbuf, sizeof(pbuf), "%d", HTTPS_PORT);
#endif /* !SMALL */
		if (pbuf[0])
			error = getaddrinfo(host, pbuf, &hints, &res0);
	}
	if (error) {
		if (error == EAI_SERVICE)
			warnx("%s: bad port number `%s'", host, port);
		else
			warnx("%s: %s", host, gai_strerror(error));
		code = -1;
		return (0);
	}

	if (res0->ai_canonname)
		strlcpy(hostnamebuf, res0->ai_canonname, sizeof(hostnamebuf));
	else
		strlcpy(hostnamebuf, host, sizeof(hostnamebuf));
	hostname = hostnamebuf;

#ifndef SMALL
	if (srcaddr) {
		struct addrinfo ahints;

		memset(&ahints, 0, sizeof(ahints));
		ahints.ai_family = family;
		ahints.ai_socktype = SOCK_STREAM;
		ahints.ai_flags |= AI_NUMERICHOST;
		ahints.ai_protocol = 0;

		error = getaddrinfo(srcaddr, NULL, &ahints, &ares);
		if (error) {
			warnx("%s: %s", srcaddr, gai_strerror(error));
			code = -1;
			return (0);
		}
	}
#endif /* !SMALL */

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		if (res0->ai_next)	/* if we have multiple possibilities */
		{
			if (getnameinfo(res->ai_addr, res->ai_addrlen,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "unknown", sizeof(hbuf));
			if (verbose)
				fprintf(ttyout, "Trying %s...\n", hbuf);
		}
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			cause = "socket";
			continue;
		}
#ifndef SMALL
		if (srcaddr) {
			if (ares->ai_family != res->ai_family) {
				close(s);
				s = -1;
				errno = EINVAL;
				cause = "bind";
				continue;
			}
			if (bind(s, ares->ai_addr, ares->ai_addrlen) < 0) {
				cause = "bind";
				error = errno;
				close(s);
				errno = error;
				s = -1;
				continue;
			}
		}
#endif /* !SMALL */
		for (error = connect(s, res->ai_addr, res->ai_addrlen);
		    error != 0 && errno == EINTR; error = connect_wait(s))
			continue;
		if (error != 0) {
			/* this "if" clause is to prevent print warning twice */
			if (verbose && res->ai_next) {
				if (getnameinfo(res->ai_addr, res->ai_addrlen,
				    hbuf, sizeof(hbuf), NULL, 0,
				    NI_NUMERICHOST) != 0)
					strlcpy(hbuf, "(unknown)",
					    sizeof(hbuf));
				warn("connect to address %s", hbuf);
			}
			cause = "connect";
			error = errno;
			close(s);
			errno = error;
			s = -1;
			continue;
		}

		/* finally we got one */
		break;
	}
	if (s < 0) {
		warn("%s", cause);
		code = -1;
		freeaddrinfo(res0);
		return 0;
	}
	memcpy(&hisctladdr, res->ai_addr, res->ai_addrlen);
	namelen = res->ai_addrlen;
	freeaddrinfo(res0);
	res0 = res = NULL;
#ifndef SMALL
	if (srcaddr) {
		freeaddrinfo(ares);
		ares = NULL;
	}
#endif /* !SMALL */
	if (getsockname(s, &myctladdr.sa, &namelen) < 0) {
		warn("getsockname");
		code = -1;
		goto bad;
	}
	if (hisctladdr.sa.sa_family == AF_INET) {
		tos = IPTOS_LOWDELAY;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		warnx("fdopen failed.");
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		fprintf(ttyout, "Connected to %s.\n", hostname);
	if (getreply(0) > 2) {	/* read startup message from server */
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	{
	int ret, on = 1;

	ret = setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on));
#ifndef SMALL
	if (ret < 0 && debug)
		warn("setsockopt");
#endif /* !SMALL */
	}

	return (hostname);
bad:
	(void)close(s);
	return (NULL);
}

/* ARGSUSED */
void
cmdabort(int signo)
{
	int save_errno = errno;

	alarmtimer(0);
	(void) write(fileno(ttyout), "\n\r", 2);
	abrtflag++;

	errno = save_errno;
	if (ptflag)
		longjmp(ptabort, 1);
}

int
command(const char *fmt, ...)
{
	va_list ap;
	int r;
	sig_t oldintr;

	abrtflag = 0;
#ifndef SMALL
	if (debug) {
		fputs("---> ", ttyout);
		va_start(ap, fmt);
		if (strncmp("PASS ", fmt, 5) == 0)
			fputs("PASS XXXX", ttyout);
		else if (strncmp("ACCT ", fmt, 5) == 0)
			fputs("ACCT XXXX", ttyout);
		else
			vfprintf(ttyout, fmt, ap);
		va_end(ap);
		putc('\n', ttyout);
		(void)fflush(ttyout);
	}
#endif /* !SMALL */
	if (cout == NULL) {
		warnx("No control connection for command.");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	va_start(ap, fmt);
	vfprintf(cout, fmt, ap);
	va_end(ap);
	fputs("\r\n", cout);
	(void)fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)(SIGINT);
	(void)signal(SIGINT, oldintr);
	return (r);
}

int keep_alive_timeout = 60;		/* 0 -> no timeout */

static int full_noops_sent = 0;
static time_t last_timestamp = 0;	/* 0 -> no measurement yet */
static char noop[] = "NOOP\r\n";
#define NOOP_LENGTH (sizeof noop - 1)
static int current_nop_pos = 0;		/* 0 -> no noop started */

/* to achieve keep alive, we send noop one byte at a time */
static void
send_noop_char(void)
{
#ifndef SMALL
	if (debug)
		fprintf(ttyout, "---> %c\n", noop[current_nop_pos]);
#endif /* !SMALL */
	fputc(noop[current_nop_pos++], cout);
	(void)fflush(cout);
	if (current_nop_pos >= NOOP_LENGTH) {
		full_noops_sent++;
		current_nop_pos = 0;
	}
}

static void
may_reset_noop_timeout(void)
{
	if (keep_alive_timeout != 0)
		last_timestamp = time(NULL);
}

static void
may_receive_noop_ack(void)
{
	int i;

	if (cout == NULL) {
		/* Lost connection;  so just pretend we're fine. */
		current_nop_pos = full_noops_sent = 0;
		return;
	}

	/* finish sending last incomplete noop */
	if (current_nop_pos != 0) {
		fputs(&(noop[current_nop_pos]), cout);
#ifndef SMALL
		if (debug)
			fprintf(ttyout, "---> %s\n", &(noop[current_nop_pos]));
#endif /* !SMALL */
		(void)fflush(cout);
		current_nop_pos = 0;
		full_noops_sent++;
	}
	/* and get the replies */
	for (i = 0; i < full_noops_sent; i++)
		(void)getreply(0);

	full_noops_sent = 0;
}

static void
may_send_noop_char(void)
{
	if (keep_alive_timeout != 0) {
		if (last_timestamp != 0) {
			time_t t = time(NULL);

			if (t - last_timestamp >= keep_alive_timeout) {
				last_timestamp = t;
				send_noop_char();
			}
		} else {
			last_timestamp = time(NULL);
		}
	}
}

char reply_string[BUFSIZ];		/* first line of previous reply */

int
getreply(int expecteof)
{
	char current_line[BUFSIZ];	/* last line of previous reply */
	int c, n, lineno;
	int dig;
	int originalcode = 0, continuation = 0;
	sig_t oldintr;
	int pflag = 0;
	char *cp, *pt = pasv;

	memset(current_line, 0, sizeof(current_line));
	oldintr = signal(SIGINT, cmdabort);
	for (lineno = 0 ;; lineno++) {
		dig = n = code = 0;
		cp = current_line;
		while ((c = fgetc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = fgetc(cin)) {
				case WILL:
				case WONT:
					c = fgetc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void)fflush(cout);
					break;
				case DO:
				case DONT:
					c = fgetc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void)fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void)signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer();
				if (verbose) {
					fputs(
"421 Service not available, remote server has closed connection.\n", ttyout);
					(void)fflush(ttyout);
				}
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    ((verbose > -1 && n == '5' && dig > 4) &&
			    (((!n && c < '5') || (n && n < '5'))
			     || !retry_connect)))) {
				if (proxflag &&
				   (dig == 1 || (dig == 5 && verbose == 0)))
					fprintf(ttyout, "%s:", hostname);
				(void)putc(c, ttyout);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && (code == 227 || code == 228))
				pflag = 1;
			else if (!pflag && code == 229)
				pflag = 100;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')') {
					if (pt < &pasv[sizeof(pasv) - 1])
						*pt++ = c;
				} else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (pflag == 100 && c == '(')
				pflag = 2;
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &current_line[sizeof(current_line) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || ((verbose > -1 && n == '5') &&
		    (n < '5' || !retry_connect))) {
			(void)putc(c, ttyout);
			(void)fflush (ttyout);
		}
		if (lineno == 0) {
			size_t len = cp - current_line;

			if (len > sizeof(reply_string))
				len = sizeof(reply_string);

			(void)strlcpy(reply_string, current_line, len);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void)signal(SIGINT, oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer();
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)(SIGINT);
		return (n - '0');
	}
}

#ifndef SMALL
jmp_buf	sendabort;

/* ARGSUSED */
void
abortsend(int signo)
{
	int save_errno = errno;
	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
#define MSG "\nsend aborted\nwaiting for remote to finish abort.\n"
	(void) write(fileno(ttyout), MSG, strlen(MSG));
#undef MSG

	errno = save_errno;
	longjmp(sendabort, 1);
}

void
sendrequest(const char *cmd, const char *local, const char *remote,
    int printnames)
{
	struct stat st;
	int c, d;
	FILE * volatile fin, * volatile dout;
	int (* volatile closefunc)(FILE *);
	volatile sig_t oldinti, oldintr, oldintp;
	volatile off_t hashbytes;
	char * volatile lmode;
	char buf[BUFSIZ], *bufp;
	int oprogress, serrno;

	hashbytes = mark;
	direction = "sent";
	dout = NULL;
	bytes = 0;
	filesize = -1;
	oprogress = progress;
	if (verbose && printnames) {
		if (local && *local != '-')
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	oldinti = NULL;
	lmode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		progress = oprogress;
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	oldinti = signal(SIGINFO, psummary);
	if (strcmp(local, "-") == 0) {
		fin = stdin;
		if (progress == 1)
			progress = 0;
	} else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			warn("%s", local + 1);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGPIPE, oldintp);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			return;
		}
		if (progress == 1)
			progress = 0;
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			warn("local: %s", local);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode & S_IFMT) != S_IFREG) {
			fprintf(ttyout, "%s: not a plain file.\n", local);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			fclose(fin);
			code = -1;
			return;
		}
		filesize = st.st_size;
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		code = -1;
		progress = oprogress;
		if (closefunc != NULL)
			(*closefunc)(fin);
		return;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		int rc = -1;

		switch (curtype) {
		case TYPE_A:
			rc = fseeko(fin, restart_point, SEEK_SET);
			break;
		case TYPE_I:
			if (lseek(fileno(fin), restart_point, SEEK_SET) != -1)
				rc = 0;
			break;
		}
		if (rc == -1) {
			warn("local: %s", local);
			progress = oprogress;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		if (command("REST %lld", (long long) restart_point)
			!= CONTINUE) {
			progress = oprogress;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		lmode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			progress = oprogress;
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			progress = oprogress;
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;
	progressmeter(-1, remote);
	may_reset_noop_timeout();
	oldintp = signal(SIGPIPE, SIG_IGN);
	serrno = 0;
	switch (curtype) {

	case TYPE_I:
		d = 0;
		while ((c = read(fileno(fin), buf, sizeof(buf))) > 0) {
			may_send_noop_char();
			bytes += c;
			for (bufp = buf; c > 0; c -= d, bufp += d)
				if ((d = write(fileno(dout), bufp, (size_t)c))
				    <= 0)
					break;
			if (hash && (!progress || filesize < 0) ) {
				while (bytes >= hashbytes) {
					(void)putc('#', ttyout);
					hashbytes += mark;
				}
				(void)fflush(ttyout);
			}
		}
		if (c == -1 || d == -1)
			serrno = errno;
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (c < 0)
			warnc(serrno, "local: %s", local);
		if (d < 0) {
			if (serrno != EPIPE)
				warnc(serrno, "netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = fgetc(fin)) != EOF) {
			may_send_noop_char();
			if (c == '\n') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				if (ferror(dout))
					break;
				(void)putc('\r', dout);
				bytes++;
			}
			(void)putc(c, dout);
			bytes++;
		}
		if (ferror(fin) || ferror(dout))
			serrno = errno;
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (ferror(fin))
			warnc(serrno, "local: %s", local);
		if (ferror(dout)) {
			if (errno != EPIPE)
				warnc(serrno, "netout");
			bytes = -1;
		}
		break;
	}
	progressmeter(1, NULL);
	progress = oprogress;
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void)fclose(dout);
	(void)getreply(0);
	may_receive_noop_ack();
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer(0);
	return;
abort:
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	progress = oprogress;
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (dout)
		(void)fclose(dout);
	(void)getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (bytes > 0)
		ptransfer(0);
}
#endif /* !SMALL */

jmp_buf	recvabort;

/* ARGSUSED */
void
abortrecv(int signo)
{

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	fputs("\nreceive aborted\nwaiting for remote to finish abort.\n", ttyout);
	(void)fflush(ttyout);
	longjmp(recvabort, 1);
}

void
recvrequest(const char *cmd, const char * volatile local, const char *remote,
    const char *lmode, int printnames, int ignorespecial)
{
	FILE * volatile fout, * volatile din;
	int (* volatile closefunc)(FILE *);
	volatile sig_t oldinti, oldintr, oldintp;
	int c, d, serrno;
	volatile int is_retr, tcrflag, bare_lfs;
	static size_t bufsize;
	static char *buf;
	volatile off_t hashbytes;
	struct stat st;
	time_t mtime;
	int oprogress;
	int opreserve;

	fout = NULL;
	din = NULL;
	oldinti = NULL;
	hashbytes = mark;
	direction = "received";
	bytes = 0;
	bare_lfs = 0;
	filesize = -1;
	oprogress = progress;
	opreserve = preserve;
	is_retr = strcmp(cmd, "RETR") == 0;
	if (is_retr && verbose && printnames) {
		if (local && (ignorespecial || *local != '-'))
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		progress = oprogress;
		preserve = opreserve;
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	oldinti = signal(SIGINFO, psummary);
	if (ignorespecial || (strcmp(local, "-") && *local != '|')) {
		if (access(local, W_OK) < 0) {
			char *dir;

			if (errno != ENOENT && errno != EACCES) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			dir = strrchr(local, '/');
			if (dir != NULL)
				*dir = 0;
			d = access(dir == local ? "/" : dir ? local : ".", W_OK);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, (S_IRUSR|S_IWUSR)) < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else {
		if (curtype != type)
			changetype(type, 0);
		filesize = remotesize(remote, 0);
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST %lld", (long long) restart_point) != CONTINUE)
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (!ignorespecial && strcmp(local, "-") == 0) {
		fout = stdout;
		preserve = 0;
	} else if (!ignorespecial && *local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			warn("%s", local+1);
			goto abort;
		}
		if (progress == 1)
			progress = 0;
		preserve = 0;
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			warn("local: %s", local);
			goto abort;
		}
		closefunc = fclose;
	}
	if (fstat(fileno(fout), &st) < 0 || st.st_blksize == 0)
		st.st_blksize = BUFSIZ;
	if (st.st_blksize > bufsize) {
		(void)free(buf);
		buf = malloc((unsigned)st.st_blksize);
		if (buf == NULL) {
			warn("malloc");
			bufsize = 0;
			goto abort;
		}
		bufsize = st.st_blksize;
	}
	if ((st.st_mode & S_IFMT) != S_IFREG) {
		if (progress == 1)
			progress = 0;
		preserve = 0;
	}
	progressmeter(-1, remote);
	may_reset_noop_timeout();
	serrno = 0;
	switch (curtype) {

	case TYPE_I:
		if (restart_point &&
		    lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			warn("local: %s", local);
			progress = oprogress;
			preserve = opreserve;
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		errno = d = 0;
		while ((c = read(fileno(din), buf, bufsize)) > 0) {
			ssize_t	wr;
			size_t	rd = c;

			may_send_noop_char();
			d = 0;
			do {
				wr = write(fileno(fout), buf + d, rd);
				if (wr == -1) {
					d = -1;
					break;
				}
				d += wr;
				rd -= wr;
			} while (d < c);
			if (rd != 0)
				break;
			bytes += c;
			if (hash && (!progress || filesize < 0)) {
				while (bytes >= hashbytes) {
					(void)putc('#', ttyout);
					hashbytes += mark;
				}
				(void)fflush(ttyout);
			}
		}
		if (c == -1 || d < c)
			serrno = errno;
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (c < 0) {
			if (serrno != EPIPE)
				warnc(serrno, "netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				warnc(serrno, "local: %s", local);
			else
				warnx("%s: short write", local);
		}
		break;

	case TYPE_A:
		if (restart_point) {
			int i, n, ch;

			if (fseek(fout, 0L, SEEK_SET) < 0)
				goto done;
			n = restart_point;
			for (i = 0; i++ < n;) {
				if ((ch = fgetc(fout)) == EOF) {
					if (!ferror(fout))
						errno = 0;
					goto done;
				}
				if (ch == '\n')
					i++;
			}
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
done:
				if (errno)
					warn("local: %s", local);
				else
					warnx("local: %s", local);
				progress = oprogress;
				preserve = opreserve;
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}
		while ((c = fgetc(din)) != EOF) {
			may_send_noop_char();
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				bytes++;
				if ((c = fgetc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void)putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void)putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (ferror(din) || ferror(fout))
			serrno = errno;
		if (bare_lfs) {
			fprintf(ttyout,
"WARNING! %d bare linefeeds received in ASCII mode.\n", bare_lfs);
			fputs("File may not have transferred correctly.\n",
			    ttyout);
		}
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (ferror(din)) {
			if (serrno != EPIPE)
				warnc(serrno, "netin");
			bytes = -1;
		}
		if (ferror(fout))
			warnc(serrno, "local: %s", local);
		break;
	}
	progressmeter(1, NULL);
	progress = oprogress;
	preserve = opreserve;
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)fclose(din);
	(void)getreply(0);
	may_receive_noop_ack();
	if (bytes >= 0 && is_retr) {
		if (bytes > 0)
			ptransfer(0);
		if (preserve && (closefunc == fclose)) {
			mtime = remotemodtime(remote, 0);
			if (mtime != -1) {
				struct utimbuf ut;

				ut.actime = time(NULL);
				ut.modtime = mtime;
				if (utime(local, &ut) == -1)
					fprintf(ttyout,
				"Can't change modification time on %s to %s",
					    local, asctime(localtime(&mtime)));
			}
		}
	}
	return;

abort:
	/* abort using RFC959 recommended IP,SYNC sequence */
	progress = oprogress;
	preserve = opreserve;
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void)fclose(din);
	if (bytes > 0)
		ptransfer(0);
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn(void)
{
	char *p, *a;
	int result = ERROR, tmpno = 0;
	int on = 1;
	int error;
	u_int addr[16], port[2];
	u_int af, hal, pal;
	char *pasvcmd = NULL;
	socklen_t namelen;
#ifndef SMALL
	struct addrinfo *ares;
#endif

	if (myctladdr.sa.sa_family == AF_INET6
	 && (IN6_IS_ADDR_LINKLOCAL(&myctladdr.sin6.sin6_addr)
	  || IN6_IS_ADDR_SITELOCAL(&myctladdr.sin6.sin6_addr))) {
		warnx("use of scoped address can be troublesome");
	}
#ifndef SMALL
	if (srcaddr) {
		struct addrinfo ahints;

		memset(&ahints, 0, sizeof(ahints));
		ahints.ai_family = family;
		ahints.ai_socktype = SOCK_STREAM;
		ahints.ai_flags |= AI_NUMERICHOST;
		ahints.ai_protocol = 0;

		error = getaddrinfo(srcaddr, NULL, &ahints, &ares);
		if (error) {
			warnx("%s: %s", srcaddr, gai_strerror(error));
			code = -1;
			return (0);
		}
	}
#endif /* !SMALL */
reinit:
	if (passivemode) {
		data_addr = myctladdr;
		data = socket(data_addr.sa.sa_family, SOCK_STREAM, 0);
		if (data < 0) {
			warn("socket");
			return (1);
		}
#ifndef SMALL
		if (srcaddr) {
			if (bind(data, ares->ai_addr, ares->ai_addrlen) < 0) {
				warn("bind");
				close(data);
				return (1);
			}
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			       sizeof(on)) < 0)
			warn("setsockopt (ignored)");
#endif /* !SMALL */
		switch (data_addr.sa.sa_family) {
		case AF_INET:
			if (epsv4 && !epsv4bad) {
				int ov;
				/* shut this command up in case it fails */
				ov = verbose;
				verbose = -1;
				result = command(pasvcmd = "EPSV");
				/*
				 * now back to whatever verbosity we had before
				 * and we can try PASV
				 */
				verbose = ov;
				if (code / 10 == 22 && code != 229) {
					fputs(
"wrong server: return code must be 229\n",
						ttyout);
					result = COMPLETE + 1;
				}
				if (result != COMPLETE) {
					epsv4bad = 1;
#ifndef SMALL
					if (debug) {
						fputs(
"disabling epsv4 for this connection\n",
						    ttyout);
					}
#endif /* !SMALL */
				}
			}
			if (result != COMPLETE)
				result = command(pasvcmd = "PASV");
			break;
		case AF_INET6:
			result = command(pasvcmd = "EPSV");
			if (code / 10 == 22 && code != 229) {
				fputs(
"wrong server: return code must be 229\n",
					ttyout);
				result = COMPLETE + 1;
			}
			if (result != COMPLETE)
				result = command(pasvcmd = "LPSV");
			break;
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result != COMPLETE) {
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
				activefallback = 0;
				goto reinit;
			}
			fputs("Passive mode refused.\n", ttyout);
			goto bad;
		}

#define pack2(var, off) \
	(((var[(off) + 0] & 0xff) << 8) | ((var[(off) + 1] & 0xff) << 0))
#define pack4(var, off) \
	(((var[(off) + 0] & 0xff) << 24) | ((var[(off) + 1] & 0xff) << 16) | \
	 ((var[(off) + 2] & 0xff) << 8) | ((var[(off) + 3] & 0xff) << 0))

		/*
		 * What we've got at this point is a string of comma separated
		 * one-byte unsigned integer values, separated by commas.
		 */
		if (!pasvcmd)
			goto bad;
		if (strcmp(pasvcmd, "PASV") == 0) {
			if (data_addr.sa.sa_family != AF_INET) {
				fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
				goto bad;
			}
			if (code / 10 == 22 && code != 227) {
				fputs("wrong server: return code must be 227\n",
					ttyout);
				goto bad;
			}
			error = sscanf(pasv, "%u,%u,%u,%u,%u,%u",
					&addr[0], &addr[1], &addr[2], &addr[3],
					&port[0], &port[1]);
			if (error != 6) {
				fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
				goto bad;
			}
			memset(&data_addr, 0, sizeof(data_addr));
			data_addr.sin.sin_family = AF_INET;
			data_addr.sin.sin_len = sizeof(struct sockaddr_in);
			data_addr.sin.sin_addr.s_addr =
				htonl(pack4(addr, 0));
			data_addr.sin.sin_port = htons(pack2(port, 0));
		} else if (strcmp(pasvcmd, "LPSV") == 0) {
			if (code / 10 == 22 && code != 228) {
				fputs("wrong server: return code must be 228\n",
					ttyout);
				goto bad;
			}
			switch (data_addr.sa.sa_family) {
			case AF_INET:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&pal, &port[0], &port[1]);
				if (error != 9) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					goto bad;
				}
				if (af != 4 || hal != 4 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}

				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.sin.sin_family = AF_INET;
				data_addr.sin.sin_len = sizeof(struct sockaddr_in);
				data_addr.sin.sin_addr.s_addr =
					htonl(pack4(addr, 0));
				data_addr.sin.sin_port = htons(pack2(port, 0));
				break;
			case AF_INET6:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&addr[4], &addr[5], &addr[6], &addr[7],
					&addr[8], &addr[9], &addr[10],
					&addr[11], &addr[12], &addr[13],
					&addr[14], &addr[15],
					&pal, &port[0], &port[1]);
				if (error != 21) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					goto bad;
				}
				if (af != 6 || hal != 16 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					goto bad;
				}

				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.sin6.sin6_family = AF_INET6;
				data_addr.sin6.sin6_len = sizeof(struct sockaddr_in6);
			    {
				u_int32_t *p32;
				p32 = (u_int32_t *)&data_addr.sin6.sin6_addr;
				p32[0] = htonl(pack4(addr, 0));
				p32[1] = htonl(pack4(addr, 4));
				p32[2] = htonl(pack4(addr, 8));
				p32[3] = htonl(pack4(addr, 12));
			    }
				data_addr.sin6.sin6_port = htons(pack2(port, 0));
				break;
			default:
				fputs("Bad family!\n", ttyout);
				goto bad;
			}
		} else if (strcmp(pasvcmd, "EPSV") == 0) {
			char delim[4];

			port[0] = 0;
			if (code / 10 == 22 && code != 229) {
				fputs("wrong server: return code must be 229\n",
					ttyout);
				goto bad;
			}
			if (sscanf(pasv, "%c%c%c%d%c", &delim[0],
					&delim[1], &delim[2], &port[1],
					&delim[3]) != 5) {
				fputs("parse error!\n", ttyout);
				goto bad;
			}
			if (delim[0] != delim[1] || delim[0] != delim[2]
			 || delim[0] != delim[3]) {
				fputs("parse error!\n", ttyout);
				goto bad;
			}
			data_addr = hisctladdr;
			data_addr.sin.sin_port = htons(port[1]);
		} else
			goto bad;

		for (error = connect(data, &data_addr.sa, data_addr.sa.sa_len);
		    error != 0 && errno == EINTR;
		    error = connect_wait(data))
			continue;
		if (error != 0) {
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
				activefallback = 0;
				goto reinit;
			}
			warn("connect");
			goto bad;
		}
		if (data_addr.sa.sa_family == AF_INET) {
			on = IPTOS_THROUGHPUT;
			if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
				       sizeof(int)) < 0)
				warn("setsockopt TOS (ignored)");
		}
		return (0);
	}

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin.sin_port = 0;	/* let system pick one */
	if (data != -1)
		(void)close(data);
	data = socket(data_addr.sa.sa_family, SOCK_STREAM, 0);
	if (data < 0) {
		warn("socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
				sizeof(on)) < 0) {
			warn("setsockopt (reuse address)");
			goto bad;
		}
	switch (data_addr.sa.sa_family) {
	case AF_INET:
		on = IP_PORTRANGE_HIGH;
		if (setsockopt(data, IPPROTO_IP, IP_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			warn("setsockopt IP_PORTRANGE (ignored)");
		break;
	case AF_INET6:
		on = IPV6_PORTRANGE_HIGH;
		if (setsockopt(data, IPPROTO_IPV6, IPV6_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			warn("setsockopt IPV6_PORTRANGE (ignored)");
		break;
	}
	if (bind(data, &data_addr.sa, data_addr.sa.sa_len) < 0) {
		warn("bind");
		goto bad;
	}
#ifndef SMALL
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			sizeof(on)) < 0)
		warn("setsockopt (ignored)");
#endif /* !SMALL */
	namelen = sizeof(data_addr);
	if (getsockname(data, &data_addr.sa, &namelen) < 0) {
		warn("getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		warn("listen");

#define	UC(b)	(((int)b)&0xff)

	if (sendport) {
		char hname[NI_MAXHOST], pbuf[NI_MAXSERV];
		int af_tmp;
		union sockaddr_union tmp;

		tmp = data_addr;
		switch (tmp.sa.sa_family) {
		case AF_INET:
			if (!epsv4 || epsv4bad) {
				result = COMPLETE +1;
				break;
			}
			/*FALLTHROUGH*/
		case AF_INET6:
			if (tmp.sa.sa_family == AF_INET6)
				tmp.sin6.sin6_scope_id = 0;
			af_tmp = (tmp.sa.sa_family == AF_INET) ? 1 : 2;
			if (getnameinfo(&tmp.sa, tmp.sa.sa_len, hname,
			    sizeof(hname), pbuf, sizeof(pbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV)) {
				result = ERROR;
			} else {
				result = command("EPRT |%d|%s|%s|",
				    af_tmp, hname, pbuf);
				if (result != COMPLETE) {
					epsv4bad = 1;
#ifndef SMALL
					if (debug) {
						fputs(
"disabling epsv4 for this connection\n",
						    ttyout);
					}
#endif /* !SMALL */
				}
			}
			break;
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result == COMPLETE)
			goto skip_port;

		switch (data_addr.sa.sa_family) {
		case AF_INET:
			a = (char *)&data_addr.sin.sin_addr;
			p = (char *)&data_addr.sin.sin_port;
			result = command("PORT %d,%d,%d,%d,%d,%d",
				 UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				 UC(p[0]), UC(p[1]));
			break;
		case AF_INET6:
			a = (char *)&data_addr.sin6.sin6_addr;
			p = (char *)&data_addr.sin6.sin6_port;
			result = command(
"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
				 6, 16,
				 UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
				 UC(a[4]),UC(a[5]),UC(a[6]),UC(a[7]),
				 UC(a[8]),UC(a[9]),UC(a[10]),UC(a[11]),
				 UC(a[12]),UC(a[13]),UC(a[14]),UC(a[15]),
				 2, UC(p[0]), UC(p[1]));
			break;
		default:
			result = COMPLETE + 1; /* xxx */
		}
	skip_port:

		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
	if (data_addr.sa.sa_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
	return (0);
bad:
	(void)close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(const char *lmode)
{
	union sockaddr_union from;
	socklen_t fromlen = myctladdr.sa.sa_len;
	int s;

	if (passivemode)
		return (fdopen(data, lmode));

	s = accept(data, &from.sa, &fromlen);
	if (s < 0) {
		warn("accept");
		(void)close(data), data = -1;
		return (NULL);
	}
	(void)close(data);
	data = s;
	if (from.sa.sa_family == AF_INET) {
		int tos = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
				sizeof(int)) < 0) {
			warn("setsockopt TOS (ignored)");
		}
	}
	return (fdopen(data, lmode));
}

/* ARGSUSED */
void
psummary(int signo)
{
	int save_errno = errno;

	if (bytes > 0)
		ptransfer(1);
	errno = save_errno;
}

/* ARGSUSED */
void
psabort(int signo)
{

	alarmtimer(0);
	abrtflag++;
}

void
pswitch(int flag)
{
	sig_t oldintr;
	static struct comvars {
		int connect;
		char name[HOST_NAME_MAX+1];
		union sockaddr_union mctl;
		union sockaddr_union hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[PATH_MAX];
		char mo[PATH_MAX];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void)strlcpy(ip->name, hostname, sizeof(ip->name));
	} else
		ip->name[0] = '\0';
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void)strlcpy(ip->nti, ntin, sizeof(ip->nti));
	(void)strlcpy(ntin, op->nti, sizeof ntin);
	(void)strlcpy(ip->nto, ntout, sizeof(ip->nto));
	(void)strlcpy(ntout, op->nto, sizeof ntout);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void)strlcpy(ip->mi, mapin, sizeof(ip->mi));
	(void)strlcpy(mapin, op->mi, sizeof mapin);
	(void)strlcpy(ip->mo, mapout, sizeof(ip->mo));
	(void)strlcpy(mapout, op->mo, sizeof mapout);
	(void)signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

/* ARGSUSED */
void
abortpt(int signo)
{

	alarmtimer(0);
	putc('\n', ttyout);
	(void)fflush(ttyout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

void
proxtrans(const char *cmd, const char *local, const char *remote)
{
	volatile sig_t oldintr;
	int prox_type, nfnd;
	volatile int secndflag;
	char * volatile cmd2;
	struct pollfd pfd[1];

	oldintr = NULL;
	secndflag = 0;
	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		fputs("proxy server does not support third party transfers.\n",
		    ttyout);
		return;
	}
	pswitch(0);
	if (!connected) {
		fputs("No primary connection.\n", ttyout);
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void)signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void)getreply(0);
	pswitch(0);
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	fprintf(ttyout, "local: %s remote: %s\n", local, remote);
	return;
abort:
	(void)signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void)signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void)signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (cpend) {
		pfd[0].fd = fileno(cin);
		pfd[0].events = POLLIN;
		if ((nfnd = poll(pfd, 1, 10 * 1000)) <= 0) {
			if (nfnd < 0)
				warn("abort");
			if (ptabflg)
				code = -1;
			lostpeer();
		}
		(void)getreply(0);
		(void)getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void)signal(SIGINT, oldintr);
}

#ifndef SMALL
/* ARGSUSED */
void
reset(int argc, char *argv[])
{
	struct pollfd pfd[1];
	int nfnd = 1;

	pfd[0].fd = fileno(cin);
	pfd[0].events = POLLIN;
	while (nfnd > 0) {
		if ((nfnd = poll(pfd, 1, 0)) < 0) {
			warn("reset");
			code = -1;
			lostpeer();
		} else if (nfnd) {
			(void)getreply(0);
		}
	}
}
#endif

char *
gunique(const char *local)
{
	static char new[PATH_MAX];
	char *cp = strrchr(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp == local ? "/" : cp ? local : ".", W_OK);
	if (cp)
		*cp = '/';
	if (d < 0) {
		warn("local: %s", local);
		return ((char *) 0);
	}
	(void)strlcpy(new, local, sizeof new);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			fputs("runique: can't find unique file name.\n", ttyout);
			return ((char *) 0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, F_OK)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}

jmp_buf forceabort;

/* ARGSUSED */
static void
abortforce(int signo)
{
	int save_errno = errno;

#define MSG	"Forced abort.  The connection will be closed.\n"
	(void) write(fileno(ttyout), MSG, strlen(MSG));
#undef MSG

	errno = save_errno;
	longjmp(forceabort, 1);
}

void
abort_remote(FILE *din)
{
	char buf[BUFSIZ];
	nfds_t nfds;
	int nfnd;
	struct pollfd pfd[2];
	sig_t oldintr;

	if (cout == NULL || setjmp(forceabort)) {
		if (cout)
			fclose(cout);
		warnx("Lost control connection for abort.");
		if (ptabflg)
			code = -1;
		lostpeer();
		return;
	}

	oldintr = signal(SIGINT, abortforce);

	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	snprintf(buf, sizeof buf, "%c%c%c", IAC, IP, IAC);
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		warn("abort");
	fprintf(cout, "%cABOR\r\n", DM);
	(void)fflush(cout);
	pfd[0].fd = fileno(cin);
	pfd[0].events = POLLIN;
	nfds = 1;
	if (din) {
		pfd[1].fd = fileno(din);
		pfd[1].events = POLLIN;
		nfds++;
	}
	if ((nfnd = poll(pfd, nfds, 10 * 1000)) <= 0) {
		if (nfnd < 0)
			warn("abort");
		if (ptabflg)
			code = -1;
		lostpeer();
	}
	if (din && (pfd[1].revents & POLLIN)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			/* LOOP */;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void)getreply(0);
	}
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
}
