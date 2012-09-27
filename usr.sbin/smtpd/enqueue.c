/*	$OpenBSD: enqueue.c,v 1.63 2012/09/27 20:34:15 chl Exp $	*/

/*
 * Copyright (c) 2005 Henning Brauer <henning@bulabula.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

extern struct imsgbuf	*ibuf;

void usage(void);
static void sighdlr(int);
static void build_from(char *, struct passwd *);
static int parse_message(FILE *, int, int, FILE *);
static void parse_addr(char *, size_t, int);
static void parse_addr_terminal(int);
static char *qualify_addr(char *);
static void rcpt_add(char *);
static int open_connection(void);
static void get_responses(FILE *, int);
static int send_line(FILE *, int, char *, ...);

enum headerfields {
	HDR_NONE,
	HDR_FROM,
	HDR_TO,
	HDR_CC,
	HDR_BCC,
	HDR_SUBJECT,
	HDR_DATE,
	HDR_MSGID,
	HDR_MIME_VERSION,
	HDR_CONTENT_TYPE,
	HDR_CONTENT_DISPOSITION,
	HDR_CONTENT_TRANSFER_ENCODING,
	HDR_USER_AGENT
};

struct {
	char			*word;
	enum headerfields	 type;
} keywords[] = {
	{ "From:",			HDR_FROM },
	{ "To:",			HDR_TO },
	{ "Cc:",			HDR_CC },
	{ "Bcc:",			HDR_BCC },
	{ "Subject:",			HDR_SUBJECT },
	{ "Date:",			HDR_DATE },
	{ "Message-Id:",		HDR_MSGID },
	{ "MIME-Version:",		HDR_MIME_VERSION },
	{ "Content-Type:",		HDR_CONTENT_TYPE },
	{ "Content-Disposition:",	HDR_CONTENT_DISPOSITION },
	{ "Content-Transfer-Encoding:",	HDR_CONTENT_TRANSFER_ENCODING },
	{ "User-Agent:",		HDR_USER_AGENT },
};

#define	LINESPLIT		990
#define	SMTP_LINELEN		1000
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
	int	  need_linesplit;
	int	  saw_date;
	int	  saw_msgid;
	int	  saw_from;
	int	  saw_mime_version;
	int	  saw_content_type;
	int	  saw_content_disposition;
	int	  saw_content_transfer_encoding;
	int	  saw_user_agent;
} msg;

struct {
	uint		quote;
	uint		comment;
	uint		esc;
	uint		brackets;
	size_t		wpos;
	char		buf[SMTP_LINELEN];
} pstate;

static void
sighdlr(int sig)
{
	if (sig == SIGALRM) {
		write(STDERR_FILENO, TIMEOUTMSG, sizeof(TIMEOUTMSG));
		_exit(2);
	}
}

static void
qp_encoded_write(FILE *fp, char *buf, size_t len)
{
	while (len) {
		if (*buf == '=')
			fprintf(fp, "=3D");
		else if (*buf == ' ' || *buf == '\t') {
			char *p = buf;
			
			while (*p != '\n') {
				if (*p != ' ' && *p != '\t')
					break;
				p++;
			}
			if (*p == '\n')
				fprintf(fp, "=%2X", *buf & 0xff);
			else
				fprintf(fp, "%c", *buf & 0xff);
		}
		else if (! isprint(*buf) && *buf != '\n')
			fprintf(fp, "=%2X", *buf & 0xff);
		else
			fprintf(fp, "%c", *buf);
		buf++;
		len--;
	}
}

int
enqueue(int argc, char *argv[])
{
	int			 i, ch, tflag = 0, noheader;
	char			*fake_from = NULL, *buf;
	struct passwd		*pw;
	FILE			*fp, *fout;
	size_t			 len;
	char			*line;
	int			 dotted;
	int			 inheaders = 0;

	bzero(&msg, sizeof(msg));
	time(&timestamp);

	while ((ch = getopt(argc, argv,
	    "A:B:b:E::e:F:f:iJ::L:mN:o:p:qR:tvx")) != -1) {
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
		case 'A':
		case 'B':
		case 'b':
		case 'E':
		case 'e':
		case 'i':
		case 'L':
		case 'm':
		case 'N': /* XXX: DSN */
		case 'o':
		case 'p':
		case 'R':
		case 'x':
			break;
		case 'q':
			/* XXX: implement "process all now" */
			return (0);
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
	if (pw != NULL)
		user = xstrdup(pw->pw_name, "enqueue");

	build_from(fake_from, pw);

	while(argc > 0) {
		rcpt_add(argv[0]);
		argv++;
		argc--;
	}

	signal(SIGALRM, sighdlr);
	alarm(300);

	fp = tmpfile();
	if (fp == NULL)
		err(1, "tmpfile");
	noheader = parse_message(stdin, fake_from == NULL, tflag, fp);

	if (msg.rcpt_cnt == 0)
		errx(1, "no recipients");

	/* init session */
	rewind(fp);

	if ((msg.fd = open_connection()) == -1)
		errx(1, "server too busy");

	fout = fdopen(msg.fd, "a+");
	if (fout == NULL)
		err(1, "fdopen");

	/* 
	 * We need to call get_responses after every command because we don't
	 * support PIPELINING on the server-side yet.
	 */

	/* banner */
	get_responses(fout, 1);

	send_line(fout, verbose, "EHLO localhost\n");
	get_responses(fout, 1);

	send_line(fout, verbose, "MAIL FROM: <%s>\n", msg.from);
	get_responses(fout, 1);

	for (i = 0; i < msg.rcpt_cnt; i++) {
		send_line(fout, verbose, "RCPT TO: <%s>\n", msg.rcpts[i]);
		get_responses(fout, 1);
	}

	send_line(fout, verbose, "DATA\n");
	get_responses(fout, 1);

	/* add From */
	if (!msg.saw_from)
		send_line(fout, 0, "From: %s%s<%s>\n",
		    msg.fromname ? msg.fromname : "",
		    msg.fromname ? " " : "", 
		    msg.from);

	/* add Date */
	if (!msg.saw_date)
		send_line(fout, 0, "Date: %s\n", time_to_text(timestamp));

	/* add Message-Id */
	if (!msg.saw_msgid)
		send_line(fout, 0, "Message-Id: <%"PRIu64".enqueue@%s>\n",
		    generate_uid(), host);

	if (msg.need_linesplit) {
		/* we will always need to mime encode for long lines */
		if (!msg.saw_mime_version)
			send_line(fout, 0, "MIME-Version: 1.0\n");
		if (!msg.saw_content_type)
			send_line(fout, 0, "Content-Type: text/plain; charset=unknown-8bit\n");
		if (!msg.saw_content_disposition)
			send_line(fout, 0, "Content-Disposition: inline\n");
		if (!msg.saw_content_transfer_encoding)
			send_line(fout, 0, "Content-Transfer-Encoding: quoted-printable\n");
	}
	if (!msg.saw_user_agent)
		send_line(fout, 0, "User-Agent: OpenSMTPD enqueuer (Demoosh)\n");

	/* add separating newline */
	if (noheader)
		send_line(fout, 0, "\n");
	else
		inheaders = 1;

	for (;;) {
		buf = fgetln(fp, &len);
		if (buf == NULL && ferror(fp))
			err(1, "fgetln");
		if (buf == NULL && feof(fp))
			break;
		/* newlines have been normalized on first parsing */
		if (buf[len-1] != '\n')
			errx(1, "expect EOL");

		dotted = 0;
		if (buf[0] == '.') {
			fputc('.', fout);
			dotted = 1;
		}

		line = buf;

		if (msg.saw_content_transfer_encoding || noheader || inheaders || !msg.need_linesplit) {
			send_line(fout, 0, "%.*s", (int)len, line);
			if (inheaders && buf[0] == '\n')
				inheaders = 0;
			continue;
		}

		/* we don't have a content transfer encoding, use our default */
		do {
			if (len < LINESPLIT) {
				qp_encoded_write(fout, line, len);
				break;
			}
			else {
				qp_encoded_write(fout, line, LINESPLIT - 2 - dotted);
				send_line(fout, 0, "=\n");
				line += LINESPLIT - 2 - dotted;
				len -= LINESPLIT - 2 - dotted;
			}
		} while (len);
	}
	send_line(fout, verbose, ".\n");
	get_responses(fout, 1);	

	send_line(fout, verbose, "QUIT\n");
	get_responses(fout, 1);	

	fclose(fp);
	fclose(fout);

	exit(0);
}

static void
get_responses(FILE *fin, int n)
{
	char	*buf;
	size_t	 len;
	int	 e;

	fflush(fin);
	if ((e = ferror(fin)))
		errx(1, "ferror: %i", e);

	while(n) {
		buf = fgetln(fin, &len);
		if (buf == NULL && ferror(fin))
			err(1, "fgetln");
		if (buf == NULL && feof(fin))
			break;
		if (buf == NULL || len < 1)
			err(1, "fgetln weird");

		/* account for \r\n linebreaks */
		if (len >= 2 && buf[len - 2] == '\r' && buf[len - 1] == '\n')
			buf[--len - 1] = '\n';

		if (len < 4)
			errx(1, "bad response");

		if (verbose)
			printf("<<< %.*s", (int)len, buf);

		if (buf[3] == '-')
			continue;
		if (buf[0] != '2' && buf[0] != '3')
			errx(1, "command failed: %.*s", (int)len, buf);
		n--;
	}
}

static int
send_line(FILE *fp, int v, char *fmt, ...)
{
	int ret;
	va_list ap;

	if (v)
		printf(">>> ");
	va_start(ap, fmt);
	ret = vfprintf(fp, fmt, ap);
	if (v)
		ret = vprintf(fmt, ap);
	va_end(ap);
	return (ret);
}

static void
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
			p = xstrdup(fake_from + 1, "build_from");

			msg.from = qualify_addr(p);
			free(p);
		} else
			msg.from = qualify_addr(fake_from);
	}

	if (msg.fromname == NULL && fake_from == NULL && pw != NULL) {
		int	 len, apos;

		len = strcspn(pw->pw_gecos, ",");
		if ((p = memchr(pw->pw_gecos, '&', len))) {
			apos = p - pw->pw_gecos;
			if (asprintf(&msg.fromname, "%.*s%s%.*s",
			    apos, pw->pw_gecos,
			    pw->pw_name,
			    len - apos - 1, p + 1) == -1)
				err(1, NULL);
			msg.fromname[apos] = toupper(msg.fromname[apos]);
		} else {
			if (asprintf(&msg.fromname, "%.*s", len,
			    pw->pw_gecos) == -1)
				err(1, NULL);
		}
	}
}

static int
parse_message(FILE *fin, int get_from, int tflag, FILE *fout)
{
	char	*buf;
	size_t	 len;
	uint	 i, cur = HDR_NONE;
	uint	 header_seen = 0, header_done = 0;

	bzero(&pstate, sizeof(pstate));
	for (;;) {
		buf = fgetln(fin, &len);
		if (buf == NULL && ferror(fin))
			err(1, "fgetln");
		if (buf == NULL && feof(fin))
			break;
		if (buf == NULL || len < 1)
			err(1, "fgetln weird");

		/* account for \r\n linebreaks */
		if (len >= 2 && buf[len - 2] == '\r' && buf[len - 1] == '\n')
			buf[--len - 1] = '\n';

		if (len == 1 && buf[0] == '\n')		/* end of header */
			header_done = 1;

		if (!WSP(buf[0])) {	/* whitespace -> continuation */
			if (cur == HDR_FROM)
				parse_addr_terminal(1);
			if (cur == HDR_TO || cur == HDR_CC || cur == HDR_BCC)
				parse_addr_terminal(0);
			cur = HDR_NONE;
		}

		/* not really exact, if we are still in headers */
		if (len + (buf[len - 1] == '\n' ? 0 : 1) >= LINESPLIT)
			msg.need_linesplit = 1;

		for (i = 0; !header_done && cur == HDR_NONE &&
		    i < nitems(keywords); i++)
			if (len > strlen(keywords[i].word) &&
			    !strncasecmp(buf, keywords[i].word,
			    strlen(keywords[i].word)))
				cur = keywords[i].type;

		if (cur != HDR_NONE)
			header_seen = 1;

		if (cur != HDR_BCC) {
			send_line(fout, 0, "%.*s", (int)len, buf);
			if (buf[len - 1] != '\n')
				fputc('\n', fout);
			if (ferror(fout))
				err(1, "write error");
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
		if (cur == HDR_MIME_VERSION)
			msg.saw_mime_version = 1;
		if (cur == HDR_CONTENT_TYPE)
			msg.saw_content_type = 1;
		if (cur == HDR_CONTENT_DISPOSITION)
			msg.saw_content_disposition = 1;
		if (cur == HDR_CONTENT_TRANSFER_ENCODING)
			msg.saw_content_transfer_encoding = 1;
		if (cur == HDR_USER_AGENT)
			msg.saw_user_agent = 1;
	}

	return (!header_seen);
}

static void
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

static void
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

static char *
qualify_addr(char *in)
{
	char	*out;

	if (strlen(in) > 0 && strchr(in, '@') == NULL) {
		if (asprintf(&out, "%s@%s", in, host) == -1)
			err(1, "qualify asprintf");
	} else
		out = xstrdup(in, "qualify_addr");

	return (out);
}

static void
rcpt_add(char *addr)
{
	void	*nrcpts;
	char	*p;
	int	n;

	n = 1;
	p = addr;
	while ((p = strchr(p, ',')) != NULL) {
		n++;
		p++;
	}

	if ((nrcpts = realloc(msg.rcpts,
	    sizeof(char *) * (msg.rcpt_cnt + n))) == NULL)
		err(1, "rcpt_add realloc");
	msg.rcpts = nrcpts;

	while (n--) {
		if ((p = strchr(addr, ',')) != NULL)
			*p++ = '\0';
		msg.rcpts[msg.rcpt_cnt++] = qualify_addr(addr);
		addr = p;
	}
}

static int
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

		fd = imsg.fd;
		imsg_free(&imsg);

		break;
	}

	return fd;
}

int
enqueue_offline(int argc, char *argv[])
{
	char	 path[MAXPATHLEN];
	FILE	*fp;
	int	 i, fd, ch;

	if (ckdir(PATH_SPOOL PATH_OFFLINE, 01777, 0, 0, 0) == 0)
		errx(1, "error in offline directory setup");

	if (! bsnprintf(path, sizeof(path), "%s%s/%lld.XXXXXXXXXX", PATH_SPOOL,
		PATH_OFFLINE, (long long int) time(NULL)))
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
