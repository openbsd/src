/*	$OpenBSD: enqueue.c,v 1.17 2009/06/05 20:43:57 pyr Exp $	*/

/*
 * Copyright (c) 2005 Henning Brauer <henning@bulabula.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

extern struct imsgbuf	*ibuf;

void	 usage(void);
void	 sighdlr(int);
int	 main(int, char *[]);
void	 femail_write(const void *, size_t);
void	 femail_put(const char *, ...);
void	 send_cmd(const char *);
void	 build_from(char *, struct passwd *);
int	 parse_message(FILE *, int, int);
void	 parse_addr(char *, size_t, int);
void	 parse_addr_terminal(int);
char	*qualify_addr(char *);
void	 rcpt_add(char *);
void	 received(void);
int	 open_connection(void);
int	 read_reply(void);
void	 greeting(int);
void	 mailfrom(char *);
void	 rcptto(char *);
void	 start_data(void);
void	 send_message(int);
void	 end_data(void);

enum headerfields {
	HDR_NONE,
	HDR_FROM,
	HDR_TO,
	HDR_CC,
	HDR_BCC,
	HDR_SUBJECT,
	HDR_DATE,
	HDR_MSGID
};

struct {
	char			*word;
	enum headerfields	 type;
} keywords[] = {
	{ "From:",		HDR_FROM },
	{ "To:",		HDR_TO },
	{ "Cc:",		HDR_CC },
	{ "Bcc:",		HDR_BCC },
	{ "Subject:",		HDR_SUBJECT },
	{ "Date:",		HDR_DATE },
	{ "Message-Id:",	HDR_MSGID }
};

#define	STATUS_GREETING		220
#define	STATUS_HELO		250
#define	STATUS_MAILFROM		250
#define	STATUS_RCPTTO		250
#define	STATUS_DATA		354
#define	STATUS_QUEUED		250
#define	STATUS_QUIT		221
#define	SMTP_LINELEN		1000
#define	SMTP_TIMEOUT		120
#define	TIMEOUTMSG		"Timeout\n"

#define WSP(c)			(c == ' ' || c == '\t')

int	  verbose = 0;
char	  host[MAXHOSTNAMELEN];
char	 *user = NULL;
time_t	  timestamp;

struct {
	int	  fd;
	char	 *from;
	char	 *fromname;
	char	**rcpts;
	int	  rcpt_cnt;
	char	 *data;
	size_t	  len;
	int	  saw_date;
	int	  saw_msgid;
	int	  saw_from;
} msg;

struct {
	u_int		quote;
	u_int		comment;
	u_int		esc;
	u_int		brackets;
	size_t		wpos;
	char		buf[SMTP_LINELEN];
} pstate;

void
sighdlr(int sig)
{
	if (sig == SIGALRM) {
		write(STDERR_FILENO, TIMEOUTMSG, sizeof(TIMEOUTMSG));
		_exit (2);
	}
}

int
enqueue(int argc, char *argv[])
{
	int		 i, ch, tflag = 0, status, noheader;
	char		*fake_from = NULL;
	struct passwd	*pw;

	bzero(&msg, sizeof(msg));
	time(&timestamp);

	while ((ch = getopt(argc, argv, "46B:b:E::e:F:f:iJ::mo:p:tvx")) != -1) {
		switch (ch) {
		case 'f':
			fake_from = optarg;
			break;
		case 'F':
			msg.fromname = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		/* all remaining: ignored, sendmail compat */
		case 'B':
		case 'b':
		case 'E':
		case 'e':
		case 'i':
		case 'm':
		case 'o':
		case 'p':
		case 'x':
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (gethostname(host, sizeof(host)) == -1)
		err(1, "gethostname");
	if ((pw = getpwuid(getuid())) == NULL)
		user = "anonymous";
	if (pw != NULL && (user = strdup(pw->pw_name)) == NULL)
		err(1, "strdup");

	build_from(fake_from, pw);

	while(argc > 0) {
		rcpt_add(argv[0]);
		argv++;
		argc--;
	}

	noheader = parse_message(stdin, fake_from == NULL, tflag);

	if (msg.rcpt_cnt == 0)
		errx(1, "no recipients");

	signal(SIGALRM, sighdlr);
	alarm(SMTP_TIMEOUT);

	msg.fd = open_connection();
	if ((status = read_reply()) != STATUS_GREETING)
		errx(1, "server greets us with status %d", status);
	greeting(1);
	mailfrom(msg.from);
	for (i = 0; i < msg.rcpt_cnt; i++)
		rcptto(msg.rcpts[i]);
	start_data();
	send_message(noheader);
	end_data();

	close(msg.fd);
	exit (0);
}

void
femail_write(const void *buf, size_t nbytes)
{
	ssize_t	n;

	do {
		n = write(msg.fd, buf, nbytes);
	} while (n == -1 && errno == EINTR);

	if (n == 0)
		errx(1, "write: connection closed");
	if (n == -1)
		err(1, "write");
	if ((size_t)n < nbytes)
		errx(1, "short write: %ld of %lu bytes written",
		    (long)n, (u_long)nbytes);
}

void
femail_put(const char *fmt, ...)
{
	va_list	ap;
	char	buf[SMTP_LINELEN];

	va_start(ap, fmt);
	if (vsnprintf(buf, sizeof(buf), fmt, ap) >= (int)sizeof(buf))
		errx(1, "line length exceeded");
	va_end(ap);

	femail_write(buf, strlen(buf));
}

void
send_cmd(const char *cmd)
{
	if (verbose)
		printf(">>> %s\n", cmd);

	femail_put("%s\r\n", cmd);
}

void
build_from(char *fake_from, struct passwd *pw)
{
	char	*p;

	if (fake_from == NULL)
		msg.from = qualify_addr(user);
	else {
		if (fake_from[0] == '<') {
			if (fake_from[strlen(fake_from) - 1] != '>')
				errx(1, "leading < but no trailing >");
			fake_from[strlen(fake_from) - 1] = 0;
			if ((p = malloc(strlen(fake_from))) == NULL)
				err(1, "malloc");
			strlcpy(p, fake_from + 1, strlen(fake_from));

			msg.from = qualify_addr(p);
			free(p);
		} else
			msg.from = qualify_addr(fake_from);
	}

	if (msg.fromname == NULL && fake_from == NULL && pw != NULL) {
		size_t		 len;

		len = strcspn(pw->pw_gecos, ",");
		len++;	/* null termination */
		if ((msg.fromname = malloc(len)) == NULL)
			err(1, NULL);
		strlcpy(msg.fromname, pw->pw_gecos, len);
	}
}

int
parse_message(FILE *fin, int get_from, int tflag)
{
	char	*buf, *twodots = "..";
	size_t	 len, new_len;
	void	*newp;
	u_int	 i, cur = HDR_NONE, dotonly;
	u_int	 header_seen = 0, header_done = 0;

	bzero(&pstate, sizeof(pstate));
	for (;;) {
		buf = fgetln(fin, &len);
		if (buf == NULL && ferror(fin))
			err(1, "fgetln");
		if (buf == NULL && feof(fin))
			break;

		/* account for \r\n linebreaks */
		if (len >= 2 && buf[len - 2] == '\r' && buf[len - 1] == '\n')
			buf[--len - 1] = '\n';

		if (len == 1 && buf[0] == '\n')		/* end of header */
			header_done = 1;

		if (buf == NULL || len < 1)
			err(1, "fgetln weird");

		if (!WSP(buf[0])) {	/* whitespace -> continuation */
			if (cur == HDR_FROM)
				parse_addr_terminal(1);
			if (cur == HDR_TO || cur == HDR_CC || cur == HDR_BCC)
				parse_addr_terminal(0);
			cur = HDR_NONE;
		}

		for (i = 0; !header_done && cur == HDR_NONE &&
		    i < nitems(keywords); i++)
			if (len > strlen(keywords[i].word) &&
			    !strncasecmp(buf, keywords[i].word,
			    strlen(keywords[i].word)))
				cur = keywords[i].type;

		if (cur != HDR_NONE)
			header_seen = 1;

		if (cur != HDR_BCC) {
			/* save data, \n -> \r\n, . -> .. */
			if (buf[len - 1] == '\n')
				new_len = msg.len + len + 1;
			else
				new_len = msg.len + len + 2;

			if ((len == 1 && buf[0] == '.') ||
			    (len > 1 && buf[0] == '.' && buf[1] == '\n')) {
				dotonly = 1;
				new_len++;
			} else
				dotonly = 0;

			if ((newp = realloc(msg.data, new_len)) == NULL)
				err(1, "realloc header");
			msg.data = newp;
			if (dotonly)
				memcpy(msg.data + msg.len, twodots, 2);
			else
				memcpy(msg.data + msg.len, buf, len);
			msg.len = new_len;
			msg.data[msg.len - 2] = '\r';
			msg.data[msg.len - 1] = '\n';
		}

		/*
		 * using From: as envelope sender is not sendmail compatible,
		 * but I really want it that way - maybe needs a knob
		 */
		if (cur == HDR_FROM) {
			msg.saw_from++;
			if (get_from)
				parse_addr(buf, len, 1);
		}

		if (tflag && (cur == HDR_TO || cur == HDR_CC || cur == HDR_BCC))
			parse_addr(buf, len, 0);

		if (cur == HDR_DATE)
			msg.saw_date++;
		if (cur == HDR_MSGID)
			msg.saw_msgid++;
	}

	return (!header_seen);
}

void
parse_addr(char *s, size_t len, int is_from)
{
	size_t	 pos = 0;
	int	 terminal = 0;

	/* unless this is a continuation... */
	if (!WSP(s[pos]) && s[pos] != ',' && s[pos] != ';') {
		/* ... skip over everything before the ':' */
		for (; pos < len && s[pos] != ':'; pos++)
			;	/* nothing */
		/* ... and check & reset parser state */
		parse_addr_terminal(is_from);
	}

	/* skip over ':' ',' ';' and whitespace */
	for (; pos < len && !pstate.quote && (WSP(s[pos]) || s[pos] == ':' ||
	    s[pos] == ',' || s[pos] == ';'); pos++)
		;	/* nothing */

	for (; pos < len; pos++) {
		if (!pstate.esc && !pstate.quote && s[pos] == '(')
			pstate.comment++;
		if (!pstate.comment && !pstate.esc && s[pos] == '"')
			pstate.quote = !pstate.quote;

		if (!pstate.comment && !pstate.quote && !pstate.esc) {
			if (s[pos] == ':') {	/* group */
				for(pos++; pos < len && WSP(s[pos]); pos++)
					;	/* nothing */
				pstate.wpos = 0;
			}
			if (s[pos] == '\n' || s[pos] == '\r')
				break;
			if (s[pos] == ',' || s[pos] == ';') {
				terminal = 1;
				break;
			}
			if (s[pos] == '<') {
				pstate.brackets = 1;
				pstate.wpos = 0;
			}
			if (pstate.brackets && s[pos] == '>')
				terminal = 1;
		}

		if (!pstate.comment && !terminal && (!(!(pstate.quote ||
		    pstate.esc) && (s[pos] == '<' || WSP(s[pos]))))) {
			if (pstate.wpos >= sizeof(pstate.buf))
				errx(1, "address exceeds buffer size");
			pstate.buf[pstate.wpos++] = s[pos];
		}

		if (!pstate.quote && pstate.comment && s[pos] == ')')
			pstate.comment--;

		if (!pstate.esc && !pstate.comment && !pstate.quote &&
		    s[pos] == '\\')
			pstate.esc = 1;
		else
			pstate.esc = 0;
	}

	if (terminal)
		parse_addr_terminal(is_from);

	for (; pos < len && (s[pos] == '\r' || s[pos] == '\n'); pos++)
		;	/* nothing */

	if (pos < len)
		parse_addr(s + pos, len - pos, is_from);
}

void
parse_addr_terminal(int is_from)
{
	if (pstate.comment || pstate.quote || pstate.esc)
		errx(1, "syntax error in address");
	if (pstate.wpos) {
		if (pstate.wpos >= sizeof(pstate.buf))
			errx(1, "address exceeds buffer size");
		pstate.buf[pstate.wpos] = '\0';
		if (is_from)
			msg.from = qualify_addr(pstate.buf);
		else
			rcpt_add(pstate.buf);
		pstate.wpos = 0;
	}	
}

char *
qualify_addr(char *in)
{
	char	*out;

	if (strchr(in, '@') == NULL) {
		if (asprintf(&out, "%s@%s", in, host) == -1)
			err(1, "qualify asprintf");
	} else
		if ((out = strdup(in)) == NULL)
			err(1, "qualify strdup");

	return (out);
}

void
rcpt_add(char *addr)
{
	void	*nrcpts;

	if ((nrcpts = realloc(msg.rcpts,
	    sizeof(char *) * (msg.rcpt_cnt + 1))) == NULL)
		err(1, "rcpt_add realloc");
	msg.rcpts = nrcpts;
	msg.rcpts[msg.rcpt_cnt++] = qualify_addr(addr);
}

void
received(void)
{
	femail_put(
	    "Received: (from %s@%s, uid %lu)\r\n\tby %s\r\n\t%s\r\n",
	    user, "localhost", (u_long)getuid(), host, time_to_text(timestamp));
}

int
open_connection(void)
{
	struct imsg	imsg;
	int		fd;
	int		n;

	imsg_compose(ibuf, IMSG_SMTP_ENQUEUE, 0, 0, -1, NULL, 0);

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (1) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		if ((n = imsg_get(ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");
		if (n == 0)
			continue;

		switch (imsg.hdr.type) {
		case IMSG_CTL_OK:
			break;
		case IMSG_CTL_FAIL:
			errx(1, "server disallowed submission request");
		default:
			errx(1, "unexpected imsg reply type");
		}

		fd = imsg_get_fd(ibuf);
		imsg_free(&imsg);

		break;
	}

	return fd;
}

int
read_reply(void)
{
	char		*lbuf = NULL;
	size_t		 len, pos, spos;
	long		 status = 0;
	char		 buf[BUFSIZ];
	ssize_t		 rlen;
	int		 done = 0;

	for (len = pos = spos = 0; !done;) {
		if (pos == 0 ||
		    (pos > 0 && memchr(buf + pos, '\n', len - pos) == NULL)) {
			memmove(buf, buf + pos, len - pos);
			len -= pos;
			pos = 0;
			if ((rlen = read(msg.fd, buf + len,
			    sizeof(buf) - len)) == -1)
				err(1, "read");
			len += rlen;
		}
		spos = pos;

		/* status code */
		for (; pos < len && buf[pos] >= '0' && buf[pos] <= '9'; pos++)
			;	/* nothing */

		if (pos == len)
			return (0);

		if (buf[pos] == ' ')
			done = 1;
		else if (buf[pos] != '-')
			errx(1, "invalid syntax in reply from server");

		/* skip up to \n */
		for (; pos < len && buf[pos - 1] != '\n'; pos++)
			;	/* nothing */

		if (verbose) {
			size_t	clen;

			clen = pos - spos + 1;	/* + 1 for trailing \0 */
			if (buf[pos - 1] == '\n')
				clen--;
			if (buf[pos - 2] == '\r')
				clen--;
			if ((lbuf = malloc(clen)) == NULL)
				err(1, NULL);
			strlcpy(lbuf, buf + spos, clen);
			printf("<<< %s\n", lbuf);
			free(lbuf);
		}
	}

	status = strtol(buf, NULL, 10);
	if (status < 100 || status > 999)
		errx(1, "error reading status: out of range");

	return (status);
}

void
greeting(int use_ehlo)
{
	int	 status;
	char	*cmd, *how;

	if (use_ehlo)
		how = "EHLO";
	else
		how = "HELO";

	if (asprintf(&cmd, "%s %s", how, host) == -1)
		err(1, "asprintf");
	send_cmd(cmd);
	free(cmd);

	if ((status = read_reply()) != STATUS_HELO) {
		if (use_ehlo)
			greeting(0);
		else
			errx(1, "remote host refuses our greeting");
	}
}

void
mailfrom(char *addr)
{
	int	 status;
	char	*cmd;

	if (asprintf(&cmd, "MAIL FROM:<%s>", addr) == -1)
		err(1, "asprintf");
	send_cmd(cmd);
	free(cmd);

	if ((status = read_reply()) != STATUS_MAILFROM)
		errx(1, "mail from %s refused by server", addr);
}

void
rcptto(char *addr)
{
	int	 status;
	char	*cmd;

	if (asprintf(&cmd, "RCPT TO:<%s>", addr) == -1)
		err(1, "asprintf");
	send_cmd(cmd);
	free(cmd);

	if ((status = read_reply()) != STATUS_RCPTTO)
		errx(1, "rcpt to %s refused by server", addr);
}

void
start_data(void)
{
	int	 status;

	send_cmd("DATA");

	if ((status = read_reply()) != STATUS_DATA)
		errx(1, "server sends error after DATA");
}

void
send_message(int noheader)
{
	/* our own headers */
	received();

	if (!msg.saw_from) {
		if (msg.fromname != NULL)
			femail_put("From: %s <%s>\r\n", msg.fromname, msg.from);
		else
			femail_put("From: %s\r\n", msg.from);
	}

	if (!msg.saw_date)
		femail_put("Date: %s\r\n", time_to_text(timestamp));

	if (!msg.saw_msgid)
		femail_put("Message-Id: <%llu.enqueue@%s>\r\n",
		    queue_generate_id(), host);

	if (noheader)
		femail_write("\r\n", 2);

	if (msg.data != NULL)
		femail_write(msg.data, msg.len);
}

void
end_data(void)
{
	int	status;

	femail_write(".\r\n", 3);

	if ((status = read_reply()) != STATUS_QUEUED)
		errx(1, "error after sending mail, got status %d", status);

	send_cmd("QUIT");

	if ((status = read_reply()) != STATUS_QUIT)
		errx(1, "server sends error after QUIT");
}

int
enqueue_offline(int argc, char *argv[])
{
	char	 path[MAXPATHLEN];
	FILE	*fp;
	int	 i, fd, ch;

	if (! bsnprintf(path, sizeof(path), "%s%s/%d,XXXXXXXXXX", PATH_SPOOL,
		PATH_OFFLINE, time(NULL)))
		err(1, "snprintf");

	if ((fd = mkstemp(path)) == -1 || (fp = fdopen(fd, "w+")) == NULL) {
		warn("cannot create temporary file %s", path);
		if (fd != -1)
			unlink(path);
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		if (strchr(argv[i], '|') != NULL) {
			warnx("%s contains illegal character", argv[i]);
			unlink(path);
			exit(1);
		}
		fprintf(fp, "%s%s", i == 1 ? "" : "|", argv[i]);
	}

	fprintf(fp, "\n");

	while ((ch = fgetc(stdin)) != EOF)
		if (fputc(ch, fp) == EOF) {
			warn("write error");
			unlink(path);
			exit(1);
		}

	if (ferror(stdin)) {
		warn("read error");
		unlink(path);
		exit(1);
	}

	fclose(fp);

	return (0);
}
