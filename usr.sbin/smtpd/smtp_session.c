/*	$OpenBSD: smtp_session.c,v 1.173 2012/10/28 08:46:26 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <ctype.h>
#include <event.h>
#include <imsg.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"

#define SMTP_MAXMAIL	100
#define SMTP_MAXRCPT	1000

#define ADVERTISE_TLS(s) \
	((s)->s_l->flags & F_STARTTLS && !((s)->s_flags & F_SECURE))

#define ADVERTISE_AUTH(s) \
	((s)->s_l->flags & F_AUTH && (s)->s_flags & F_SECURE && \
	 !((s)->s_flags & F_AUTHENTICATED))

void	 ssl_error(const char *);

static int session_rfc5321_helo_handler(struct session *, char *);
static int session_rfc5321_ehlo_handler(struct session *, char *);
static int session_rfc5321_rset_handler(struct session *, char *);
static int session_rfc5321_noop_handler(struct session *, char *);
static int session_rfc5321_data_handler(struct session *, char *);
static int session_rfc5321_mail_handler(struct session *, char *);
static int session_rfc5321_rcpt_handler(struct session *, char *);
static int session_rfc5321_vrfy_handler(struct session *, char *);
static int session_rfc5321_expn_handler(struct session *, char *);
static int session_rfc5321_turn_handler(struct session *, char *);
static int session_rfc5321_help_handler(struct session *, char *);
static int session_rfc5321_quit_handler(struct session *, char *);

static int session_rfc1652_mail_handler(struct session *, char *);

static int session_rfc3207_stls_handler(struct session *, char *);

static int session_rfc4954_auth_handler(struct session *, char *);
static void session_rfc4954_auth_plain(struct session *, char *);
static void session_rfc4954_auth_login(struct session *, char *);

static void session_line(struct session *, char *, size_t);
static void session_read_data(struct session *, char *);
static void session_command(struct session *, char *);
static void session_respond_delayed(int, short, void *);
static int session_set_mailaddr(struct mailaddr *, char *);
static void session_imsg(struct session *, enum smtp_proc_type,
    enum imsg_type, uint32_t, pid_t, int, void *, uint16_t);

static void session_enter_state(struct session *, int);

const char *session_strstate(int);

struct session_cmd {
	char	 *name;
	int		(*func)(struct session *, char *);
};

struct session_cmd rfc5321_cmdtab[] = {
	{ "helo",	session_rfc5321_helo_handler },
	{ "ehlo",	session_rfc5321_ehlo_handler },
	{ "rset",	session_rfc5321_rset_handler },
	{ "noop",	session_rfc5321_noop_handler },
	{ "data",	session_rfc5321_data_handler },
	{ "mail from",	session_rfc5321_mail_handler },
	{ "rcpt to",	session_rfc5321_rcpt_handler },
	{ "vrfy",	session_rfc5321_vrfy_handler },
	{ "expn",	session_rfc5321_expn_handler },
	{ "turn",	session_rfc5321_turn_handler },
	{ "help",	session_rfc5321_help_handler },
	{ "quit",	session_rfc5321_quit_handler }
};

struct session_cmd rfc1652_cmdtab[] = {
	{ "mail from",	session_rfc1652_mail_handler },
};

struct session_cmd rfc3207_cmdtab[] = {
	{ "starttls",	session_rfc3207_stls_handler }
};

struct session_cmd rfc4954_cmdtab[] = {
	{ "auth",	session_rfc4954_auth_handler }
};

static int
session_rfc3207_stls_handler(struct session *s, char *args)
{
	if (! ADVERTISE_TLS(s))
		return 0;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 TLS not allowed at this stage");
		return 1;
	}

	if (args != NULL) {
		session_respond(s, "501 No parameters allowed");
		return 1;
	}

	session_enter_state(s, S_TLS);
	session_respond(s, "220 Ready to start TLS");

	return 1;
}

static int
session_rfc4954_auth_handler(struct session *s, char *args)
{
	char	*method;
	char	*eom;

	if (! ADVERTISE_AUTH(s)) {
		if (s->s_flags & F_AUTHENTICATED) {
			session_respond(s, "503 Already authenticated");
			return 1;
		} else
			return 0;
	}

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 Session already in progress");
		return 1;
	}

	if (args == NULL) {
		session_respond(s, "501 No parameters given");
		return 1;
	}

	method = args;
	eom = strchr(args, ' ');
	if (eom == NULL)
		eom = strchr(args, '\t');
	if (eom != NULL)
		*eom++ = '\0';

	if (strcasecmp(method, "PLAIN") == 0)
		session_rfc4954_auth_plain(s, eom);
	else if (strcasecmp(method, "LOGIN") == 0)
		session_rfc4954_auth_login(s, eom);
	else
		session_respond(s, "504 AUTH method '%s' unsupported", method);

	return 1;
}

static void
session_rfc4954_auth_plain(struct session *s, char *arg)
{
	struct auth	*a = &s->s_auth;
	char		 buf[1024], *user, *pass;
	int		 len;

	switch (s->s_state) {
	case S_HELO:
		if (arg == NULL) {
			session_enter_state(s, S_AUTH_INIT);
			session_respond(s, "334 ");
			return;
		}
		session_enter_state(s, S_AUTH_INIT);
		/* FALLTHROUGH */

	case S_AUTH_INIT:
		/* String is not NUL terminated, leave room. */
		if ((len = __b64_pton(arg, (unsigned char *)buf, sizeof(buf) - 1)) == -1)
			goto abort;
		/* buf is a byte string, NUL terminate. */
		buf[len] = '\0';

		/*
		 * Skip "foo" in "foo\0user\0pass", if present.
		 */
		user = memchr(buf, '\0', len);
		if (user == NULL || user >= buf + len - 2)
			goto abort;
		user++; /* skip NUL */
		if (strlcpy(a->user, user, sizeof(a->user)) >= sizeof(a->user))
			goto abort;

		pass = memchr(user, '\0', len - (user - buf));
		if (pass == NULL || pass >= buf + len - 2)
			goto abort;
		pass++; /* skip NUL */
		if (strlcpy(a->pass, pass, sizeof(a->pass)) >= sizeof(a->pass))
			goto abort;

		session_enter_state(s, S_AUTH_FINALIZE);

		a->id = s->s_id;
		session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
		    a, sizeof(*a));

		bzero(a->pass, sizeof(a->pass));
		return;

	default:
		fatal("session_rfc4954_auth_plain: unknown state");
	}

abort:
	session_respond(s, "501 Syntax error");
	session_enter_state(s, S_HELO);
}

static void
session_rfc4954_auth_login(struct session *s, char *arg)
{
	struct auth	*a = &s->s_auth;

	switch (s->s_state) {
	case S_HELO:
		session_enter_state(s, S_AUTH_USERNAME);
		session_respond(s, "334 VXNlcm5hbWU6");
		return;

	case S_AUTH_USERNAME:
		bzero(a->user, sizeof(a->user));
		if (__b64_pton(arg, (unsigned char *)a->user, sizeof(a->user) - 1) == -1)
			goto abort;

		session_enter_state(s, S_AUTH_PASSWORD);
		session_respond(s, "334 UGFzc3dvcmQ6");
		return;

	case S_AUTH_PASSWORD:
		bzero(a->pass, sizeof(a->pass));
		if (__b64_pton(arg, (unsigned char *)a->pass, sizeof(a->pass) - 1) == -1)
			goto abort;

		session_enter_state(s, S_AUTH_FINALIZE);

		a->id = s->s_id;
		session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
		    a, sizeof(*a));

		bzero(a->pass, sizeof(a->pass));
		return;
	
	default:
		fatal("session_rfc4954_auth_login: unknown state");
	}

abort:
	session_respond(s, "501 Syntax error");
	session_enter_state(s, S_HELO);
}

static int
session_rfc1652_mail_handler(struct session *s, char *args)
{
	char *body;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 5.5.1 Polite people say HELO first");
		return 1;
	}

	for (body = strrchr(args, ' '); body != NULL;
		body = strrchr(args, ' ')) {
		*body++ = '\0';

		if (strncasecmp(body, "AUTH=", 5) == 0) {
			log_debug("smtp: AUTH in MAIL FROM command, skipping");
			continue;		
		}

		if (strncasecmp(body, "BODY=", 5) == 0) {
			log_debug("smtp: BODY in MAIL FROM command");

			if (strncasecmp("body=7bit", body, 9) == 0) {
				s->s_flags &= ~F_8BITMIME;
				continue;
			}

			else if (strncasecmp("body=8bitmime", body, 13) != 0) {
				session_respond(s, "503 5.5.4 Unsupported option %s", body);
				return 1;
			}
		}
	}
	
	return session_rfc5321_mail_handler(s, args);
}

static int
session_rfc5321_helo_handler(struct session *s, char *args)
{
	if (args == NULL || !valid_domainpart(args)) {
		session_respond(s, "501 HELO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.helo, args, sizeof(s->s_msg.helo))
	    >= sizeof(s->s_msg.helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_msg.session_id = s->s_id;
	s->s_flags &= F_SECURE|F_AUTHENTICATED;
	session_enter_state(s, S_HELO);

	session_imsg(s, PROC_MFA, IMSG_MFA_HELO, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

static int
session_rfc5321_ehlo_handler(struct session *s, char *args)
{
	if (args == NULL || !valid_domainpart(args)) {
		session_respond(s, "501 EHLO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.helo, args, sizeof(s->s_msg.helo))
	    >= sizeof(s->s_msg.helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_msg.session_id = s->s_id;
	s->s_flags &= F_SECURE|F_AUTHENTICATED;
	s->s_flags |= F_EHLO;
	s->s_flags |= F_8BITMIME;
	session_enter_state(s, S_HELO);

	session_imsg(s, PROC_MFA, IMSG_MFA_HELO, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

static int
session_rfc5321_rset_handler(struct session *s, char *args)
{
	session_enter_state(s, S_RSET);

	session_imsg(s, PROC_MFA, IMSG_MFA_RSET, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

static int
session_rfc5321_noop_handler(struct session *s, char *args)
{
	session_respond(s, "250 2.0.0 OK");

	return 1;
}

static int
session_rfc5321_mail_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 5.5.1 Polite people say HELO first");
		return 1;
	}


	if (s->s_l->flags & F_STARTTLS_REQUIRE)
		if (!(s->s_flags & F_SECURE)) {
			session_respond(s,
			    "530 5.7.0 Must issue a STARTTLS command first");
			return 1;
		}

	if (s->s_l->flags & F_AUTH_REQUIRE)
		if (!(s->s_flags & F_AUTHENTICATED)) {
			session_respond(s,
			    "530 5.7.0 Must issue an AUTH command first");
			return 1;
		}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 5.5.1 Sender already specified");
		return 1;
	}

	if (s->mailcount >= SMTP_MAXMAIL) {
		session_respond(s, "452 Too many messages sent");
		return 1;
	}

	if (! session_set_mailaddr(&s->s_msg.sender, args)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 5.1.7 Sender address syntax error");
		return 1;
	}

	s->rcptcount = 0;
	s->s_msg.id = 0;

	session_enter_state(s, S_MAIL_MFA);
	session_imsg(s, PROC_MFA, IMSG_MFA_MAIL, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

static int
session_rfc5321_rcpt_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 5.5.1 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 5.5.1 Need MAIL before RCPT");
		return 1;
	}

	if (s->rcptcount >= SMTP_MAXRCPT) {
		session_respond(s, "452 Too many recipients");
		return 1;
	}

	if (! session_set_mailaddr(&s->s_msg.rcpt, args)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 5.1.3 Recipient address syntax error");
		return 1;
	}

	session_enter_state(s, S_RCPT_MFA);
	session_imsg(s, PROC_MFA, IMSG_MFA_RCPT, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

static int
session_rfc5321_quit_handler(struct session *s, char *args)
{
	session_enter_state(s, S_QUIT);
	session_respond(s, "221 2.0.0 %s Closing connection", env->sc_hostname);
/*
	session_imsg(s, PROC_MFA, IMSG_MFA_QUIT, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
*/
	return 1;
}

static int
session_rfc5321_data_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 5.5.1 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 5.5.1 Need MAIL before DATA");
		return 1;
	}

	if (s->s_state == S_MAIL) {
		session_respond(s, "503 5.5.1 Need RCPT before DATA");
		return 1;
	}

	session_enter_state(s, S_DATA_QUEUE);

	session_imsg(s, PROC_QUEUE, IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1,
	    &s->s_msg, sizeof(s->s_msg));

	return 1;
}

static int
session_rfc5321_vrfy_handler(struct session *s, char *args)
{
	session_respond(s, "252 5.5.1 Cannot VRFY; try RCPT to attempt delivery");

	return 1;
}

static int
session_rfc5321_expn_handler(struct session *s, char *args)
{
	session_respond(s, "502 5.5.2 Sorry, we do not allow this operation");

	return 1;
}

static int
session_rfc5321_turn_handler(struct session *s, char *args)
{
	session_respond(s, "502 5.5.2 Sorry, we do not allow this operation");

	return 1;
}

static int
session_rfc5321_help_handler(struct session *s, char *args)
{
	session_respond(s, "214- This is OpenSMTPD");
	session_respond(s, "214- To report bugs in the implementation, please "
	    "contact bugs@openbsd.org");
	session_respond(s, "214- with full details");
	session_respond(s, "214 End of HELP info");

	return 1;
}

static void
session_enter_state(struct session *s, int newstate)
{
	log_trace(TRACE_SMTP, "smtp: %p: %s -> %s", s,
	    session_strstate(s->s_state),
	    session_strstate(newstate));

	s->s_state = newstate;
}

static void
session_command(struct session *s, char *cmd)
{
	char		*ep, *args;
	unsigned int	 i;

	/*
	 * unlike other commands, "mail from" and "rcpt to" contain a
	 * space in the command name.
	 */
	if (strncasecmp("mail from:", cmd, 10) == 0 ||
	    strncasecmp("rcpt to:", cmd, 8) == 0)
		ep = strchr(cmd, ':');
	else
		ep = strchr(cmd, ' ');

	if (ep != NULL) {
		*ep = '\0';
		args = ++ep;
		while (isspace((int)*args))
			args++;
	} else
		args = NULL;

	if (!(s->s_flags & F_EHLO))
		goto rfc5321;

	/* RFC 1652 - 8BITMIME */
	for (i = 0; i < nitems(rfc1652_cmdtab); ++i)
		if (strcasecmp(rfc1652_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc1652_cmdtab)) {
		if (rfc1652_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 3207 - STARTTLS */
	for (i = 0; i < nitems(rfc3207_cmdtab); ++i)
		if (strcasecmp(rfc3207_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc3207_cmdtab)) {
		if (rfc3207_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 4954 - AUTH */
	for (i = 0; i < nitems(rfc4954_cmdtab); ++i)
		if (strcasecmp(rfc4954_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc4954_cmdtab)) {
		if (rfc4954_cmdtab[i].func(s, args))
			return;
	}

rfc5321:
	/* RFC 5321 - SMTP */
	for (i = 0; i < nitems(rfc5321_cmdtab); ++i)
		if (strcasecmp(rfc5321_cmdtab[i].name, cmd) == 0)
			break;
	if (i < nitems(rfc5321_cmdtab)) {
		if (rfc5321_cmdtab[i].func(s, args))
			return;
	}

	session_respond(s, "500 Command unrecognized");
}

void
session_io(struct io *io, int evt)
{
	struct session	*s = io->arg;
	void		*ssl;
	char		*line;
	size_t		 len;

	log_trace(TRACE_IO, "smtp: %p: %s %s", s, io_strevent(evt), io_strio(io));

	switch(evt) {

	case IO_TLSREADY:
		s->s_flags |= F_SECURE;
		if (s->s_l->flags & F_SMTPS)
			stat_increment("smtp.smtps", 1);
		if (s->s_l->flags & F_STARTTLS)
			stat_increment("smtp.tls", 1);
		if (s->s_state == S_INIT) {
			io_set_write(&s->s_io);
			session_respond(s, SMTPD_BANNER, env->sc_hostname);
		}
		session_enter_state(s, S_GREETED);
		break;

	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&s->s_iobuf, &len);
		if ((line == NULL && iobuf_len(&s->s_iobuf) >= SMTP_LINE_MAX) ||
		    (line && len >= SMTP_LINE_MAX)) {
			session_respond(s, "500 5.0.0 Line too long");
			session_enter_state(s, S_QUIT);
			io_set_write(io);
			return;
		}

		if (line == NULL) {
			iobuf_normalize(&s->s_iobuf);
			return;
		}

		if (s->s_state == S_DATACONTENT && strcmp(line, ".")) {
			/* more data to come */
			session_line(s, line, len);
			goto nextline;
		}

		/* pipelining not supported */
		if (iobuf_len(&s->s_iobuf)) {
			session_respond(s, "500 5.0.0 Pipelining not supported");
			session_enter_state(s, S_QUIT);
			io_set_write(io);
			return;
		}

		session_line(s, line, len);
		iobuf_normalize(&s->s_iobuf);
		io_set_write(io);
		break;

	case IO_LOWAT:
		if (s->s_state == S_QUIT) {
			session_destroy(s, "done");
			break;
		}

		io_set_read(io);

		/* wait for the client to start tls */
		if (s->s_state == S_TLS) {
			ssl = ssl_smtp_init(s->s_l->ssl_ctx);
			io_start_tls(io, ssl);
		}
		break;

	case IO_TIMEOUT:
		session_destroy(s, "timeout");
		break;

	case IO_DISCONNECTED:
		session_destroy(s, "disconnected");
		break;

	case IO_ERROR:
		session_destroy(s, "error");
		break;

	default:
		fatal("session_io()");
	}
}

void
session_pickup(struct session *s, struct submit_status *ss)
{
	void	*ssl;

	s->s_flags &= ~F_WAITIMSG;

	if ((ss != NULL && ss->code == 421) ||
	    (s->s_dstatus & DS_TEMPFAILURE)) {
		stat_increment("smtp.tempfail", 1);
		session_respond(s, "421 Service temporarily unavailable");
		session_enter_state(s, S_QUIT);
		io_reload(&s->s_io);
		return;
	}

	switch (s->s_state) {

	case S_CONNECTED:
		session_enter_state(s, S_INIT);
		s->s_msg.session_id = s->s_id;
		s->s_msg.ss = s->s_ss;
		session_imsg(s, PROC_MFA, IMSG_MFA_CONNECT, 0, 0, -1,
			     &s->s_msg, sizeof(s->s_msg));
		break;

	case S_INIT:
		if (ss->code != 250) {
			session_destroy(s, "rejected by filter");
			return;
		}

		if (s->s_l->flags & F_SMTPS) {
			ssl = ssl_smtp_init(s->s_l->ssl_ctx);
			io_set_read(&s->s_io);
			io_start_tls(&s->s_io, ssl);
			return;
		}

		session_respond(s, SMTPD_BANNER, env->sc_hostname);
		session_enter_state(s, S_GREETED);
		break;

	case S_AUTH_FINALIZE:
		if (s->s_flags & F_AUTHENTICATED)
			session_respond(s, "235 Authentication succeeded");
		else
			session_respond(s, "535 Authentication failed");
		session_enter_state(s, S_HELO);
		break;

	case S_RSET:
		session_respond(s, "250 2.0.0 Reset state");
		session_enter_state(s, S_HELO);
		break;

	case S_HELO:
		if (ss->code != 250) {
			session_enter_state(s, S_GREETED);
			session_respond(s, "%d Helo rejected", ss->code);
			break;
		}

		session_respond(s, "250%c%s Hello %s [%s], pleased to meet you",
		    (s->s_flags & F_EHLO) ? '-' : ' ',
		    env->sc_hostname, s->s_msg.helo, ss_to_text(&s->s_ss));

		if (s->s_flags & F_EHLO) {
			/* unconditionnal extensions go first */
			session_respond(s, "250-8BITMIME");
			session_respond(s, "250-ENHANCEDSTATUSCODES");

			/* XXX - we also want to support reading SIZE from MAIL parameters */
			session_respond(s, "250-SIZE %zu", env->sc_maxsize);

			if (ADVERTISE_TLS(s))
				session_respond(s, "250-STARTTLS");

			if (ADVERTISE_AUTH(s))
				session_respond(s, "250-AUTH PLAIN LOGIN");
			session_respond(s, "250 HELP");
		}
		break;

	case S_MAIL_MFA:
		if (ss->code != 250) {
			session_enter_state(s, S_HELO);
			session_respond(s, "%d Sender rejected", ss->code);
			break;
		}

		session_enter_state(s, S_MAIL_QUEUE);
		s->s_msg.sender = ss->u.maddr;

		session_imsg(s, PROC_QUEUE, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
		    &s->s_msg, sizeof(s->s_msg));
		break;

	case S_MAIL_QUEUE:
		session_enter_state(s, S_MAIL);
		session_respond(s, "%d 2.1.0 Sender ok", ss->code);
		break;

	case S_RCPT_MFA:
		/* recipient was not accepted */
		if (ss->code != 250) {
			/* We do not have a valid recipient, downgrade state */
			if (s->rcptcount == 0)
				session_enter_state(s, S_MAIL);
			else
				session_enter_state(s, S_RCPT);
			session_respond(s, "%d 5.0.0 Recipient rejected: %s@%s", ss->code,
			    s->s_msg.rcpt.user,
			    s->s_msg.rcpt.domain);
			break;
		}

		session_enter_state(s, S_RCPT);
		s->rcptcount++;
		s->s_msg.dest = ss->u.maddr;
		session_respond(s, "%d 2.0.0 Recipient ok", ss->code);
		break;

	case S_DATA_QUEUE:
		session_enter_state(s, S_DATACONTENT);
		session_respond(s, "354 Enter mail, end with \".\" on a line by"
		    " itself");

		fprintf(s->datafp, "Received: from %s (%s [%s])\n",
		    s->s_msg.helo, s->s_hostname, ss_to_text(&s->s_ss));
		fprintf(s->datafp, "\tby %s (OpenSMTPD) with %sSMTP id %08x",
		    env->sc_hostname, s->s_flags & F_EHLO ? "E" : "",
		    evpid_to_msgid(s->s_msg.id));

		if (s->s_flags & F_SECURE) {
			fprintf(s->datafp, "\n\t(version=%s cipher=%s bits=%d)",
			    SSL_get_cipher_version(s->s_io.ssl),
			    SSL_get_cipher_name(s->s_io.ssl),
			    SSL_get_cipher_bits(s->s_io.ssl, NULL));
		}
		if (s->rcptcount == 1)
			fprintf(s->datafp, "\n\tfor <%s@%s>; ",
			    s->s_msg.rcpt.user,
			    s->s_msg.rcpt.domain);
		else
			fprintf(s->datafp, ";\n\t");

		fprintf(s->datafp, "%s\n", time_to_text(time(NULL)));
		break;

	case S_DATACONTENT:
		if (ss->code != 250)
			s->s_dstatus |= DS_PERMFAILURE;
		session_read_data(s, ss->u.dataline);
		break;

	case S_DONE:
		session_respond(s, "250 2.0.0 %08x Message accepted for delivery",
		    evpid_to_msgid(s->s_msg.id));
		log_info("%08x: from=<%s%s%s>, size=%ld, nrcpts=%zu, proto=%s, "
		    "relay=%s [%s]",
		    evpid_to_msgid(s->s_msg.id),
		    s->s_msg.sender.user,
		    s->s_msg.sender.user[0] == '\0' ? "" : "@",
		    s->s_msg.sender.domain,
		    s->s_datalen,
		    s->rcptcount,
		    s->s_flags & F_EHLO ? "ESMTP" : "SMTP",
		    s->s_hostname,
		    ss_to_text(&s->s_ss));

		session_enter_state(s, S_HELO);
		s->s_msg.id = 0;
		s->mailcount++;
		bzero(&s->s_nresp, sizeof(s->s_nresp));
		break;

	default:
		fatal("session_pickup: unknown state");
	}

	io_reload(&s->s_io);
}

static void
session_line(struct session *s, char *line, size_t len)
{
	struct submit_status ss;

	if (s->s_state != S_DATACONTENT)
		log_trace(TRACE_SMTP, "smtp: %p: <<< %s", s, line);

	switch (s->s_state) {
	case S_AUTH_INIT:
		if (s->s_dstatus & DS_TEMPFAILURE)
			goto tempfail;
		session_rfc4954_auth_plain(s, line);
		break;

	case S_AUTH_USERNAME:
	case S_AUTH_PASSWORD:
		if (s->s_dstatus & DS_TEMPFAILURE)
			goto tempfail;
		session_rfc4954_auth_login(s, line);
		break;

	case S_GREETED:
	case S_HELO:
	case S_MAIL:
	case S_RCPT:
		if (s->s_dstatus & DS_TEMPFAILURE)
			goto tempfail;
		session_command(s, line);
		break;

	case S_DATACONTENT:
		if (env->filtermask & FILTER_DATALINE) {
			bzero(&ss, sizeof(ss));
			ss.id = s->s_id;
			if (strlcpy(ss.u.dataline, line,
				sizeof(ss.u.dataline)) >= sizeof(ss.u.dataline))
				fatal("session_line: data truncation");

			session_imsg(s, PROC_MFA, IMSG_MFA_DATALINE,
			    0, 0, -1, &ss, sizeof(ss));
		} else {
			/* no filtering */
			session_read_data(s, line);
		}
		break;

	default:
		log_debug("session_read: %i", s->s_state);
		fatalx("session_read: unexpected state");
	}

	return;

tempfail:
	session_respond(s, "421 4.0.0 Service temporarily unavailable");
	stat_increment("smtp.tempfail", 1);
	session_enter_state(s, S_QUIT);
}

static void
session_read_data(struct session *s, char *line)
{
	size_t datalen;
	size_t len;
	size_t i;

	if (strcmp(line, ".") == 0) {
		s->s_datalen = ftell(s->datafp);
		if (! safe_fclose(s->datafp))
			s->s_dstatus |= DS_TEMPFAILURE;
		s->datafp = NULL;

		if (s->s_dstatus & DS_PERMFAILURE) {
			session_respond(s, "554 5.0.0 Transaction failed");
			session_enter_state(s, S_HELO);
		} else if (s->s_dstatus & DS_TEMPFAILURE) {
			session_respond(s, "421 4.0.0 Temporary failure");
			session_enter_state(s, S_QUIT);
			stat_increment("smtp.tempfail", 1);
		} else {
			session_imsg(s, PROC_QUEUE, IMSG_QUEUE_COMMIT_MESSAGE,
			    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
			session_enter_state(s, S_DONE);
		}
		return;
	}

	/* Don't waste resources on message if it's going to bin anyway. */
	if (s->s_dstatus & (DS_PERMFAILURE|DS_TEMPFAILURE))
		return;

	/* "If the first character is a period and there are other characters
	 *  on the line, the first character is deleted." [4.5.2]
	 */
	if (*line == '.')
		line++;

	len = strlen(line);

	/* If size of data overflows a size_t or exceeds max size allowed
	 * for a message, set permanent failure.
	 */
	datalen = ftell(s->datafp);
	if (SIZE_MAX - datalen < len + 1 ||
	    datalen + len + 1 > env->sc_maxsize) {
		s->s_dstatus |= DS_PERMFAILURE;
		return;
	}

	if (! (s->s_flags & F_8BITMIME)) {
		for (i = 0; i < len; ++i)
			if (line[i] & 0x80)
				line[i] = line[i] & 0x7f;
	}

	if (fprintf(s->datafp, "%s\n", line) != (int)len + 1)
		s->s_dstatus |= DS_TEMPFAILURE;
}

void
session_destroy(struct session *s, const char * reason)
{
	uint32_t msgid;

	log_debug("smtp: %p: deleting session: %s", s, reason);

	if (s->s_flags & F_ZOMBIE)
		goto finalize;

	if (s->datafp != NULL)
		fclose(s->datafp);

	if (s->s_msg.id != 0 && s->s_state != S_DONE) {
		msgid = evpid_to_msgid(s->s_msg.id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_REMOVE_MESSAGE, 0, 0, -1, &msgid, sizeof(msgid));
	}

	if (s->s_io.ssl) {
		if (s->s_l->flags & F_SMTPS)
			if (s->s_flags & F_SECURE)
				stat_decrement("smtp.smtps", 1);
		if (s->s_l->flags & F_STARTTLS)
			if (s->s_flags & F_SECURE)
				stat_decrement("smtp.tls", 1);
	}

	event_del(&s->s_ev); /* in case something was scheduled */
	io_clear(&s->s_io);
	iobuf_clear(&s->s_iobuf);

	/* resume when session count decreases to 95% */
	stat_decrement("smtp.session", 1);

	/* If the session is waiting for an imsg, do not kill it now, since
	 * the id must still be valid.
	 */
	if (s->s_flags & F_WAITIMSG) {
		s->s_flags = F_ZOMBIE;
		return;
	}

    finalize:

	smtp_destroy(s);

	SPLAY_REMOVE(sessiontree, &env->sc_sessions, s);
	bzero(s, sizeof(*s));
	free(s);
}

int
session_cmp(struct session *s1, struct session *s2)
{
	/*
	 * do not return uint64_t's
	 */
	if (s1->s_id < s2->s_id)
		return (-1);

	if (s1->s_id > s2->s_id)
		return (1);

	return (0);
}

static int
session_set_mailaddr(struct mailaddr *maddr, char *line)
{
	size_t len;

	len = strlen(line);
	if (*line != '<' || line[len - 1] != '>')
		return 0;
	line[len - 1] = '\0';

	return email_to_mailaddr(maddr, line + 1);
}

void
session_respond(struct session *s, char *fmt, ...)
{
	va_list	 ap;
	int	 n, delay;
	char	 buf[SMTP_LINE_MAX];

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n == -1 || n >= SMTP_LINE_MAX)
		fatal("session_respond: line too long");
	if (n < 4)
		fatal("session_respond: response too short");

	log_trace(TRACE_SMTP, "smtp: %p: >>> %s", s, buf);

	iobuf_xfqueue(&s->s_iobuf, "session_respond", "%s\r\n", buf);

	/*
	 * Log failures.  Might be annoying in the long term, but it is a good
	 * development aid for now.
	 */
	switch (buf[0]) {
	case '5':
	case '4':
		log_info("%08x: from=<%s@%s>, relay=%s [%s], stat=LocalError (%.*s)",
		    evpid_to_msgid(s->s_msg.id),
		    s->s_msg.sender.user, s->s_msg.sender.domain,
		    s->s_hostname, ss_to_text(&s->s_ss),
		    n, buf);
		break;
	}

	/* Detect multi-line response. */
	switch (buf[3]) {
	case '-':
		return;
	case ' ':
		break;
	default:
		fatalx("session_respond: invalid response");
	}

	/*
	 * Deal with request flooding; avoid letting response rate keep up
	 * with incoming request rate.
	 */
	s->s_nresp[s->s_state]++;

	if (s->s_state == S_RCPT)
		delay = 0;
	else if ((n = s->s_nresp[s->s_state] - FAST_RESPONSES) > 0)
		delay = MIN(1 << (n - 1), MAX_RESPONSE_DELAY);
	else
		delay = 0;

	if (delay > 0) {
		struct timeval tv = { delay, 0 };

		io_pause(&s->s_io, IO_PAUSE_OUT);
		stat_increment("smtp.delays", 1);

		/* in case session_respond is called multiple times */
		evtimer_del(&s->s_ev);
		evtimer_set(&s->s_ev, session_respond_delayed, s);
		evtimer_add(&s->s_ev, &tv);
	}
}

static void
session_respond_delayed(int fd, short event, void *p)
{
	struct session	*s = p;

	io_resume(&s->s_io, IO_PAUSE_OUT);
}

/*
 * Send IMSG, waiting for reply safely.
 */
static void
session_imsg(struct session *s, enum smtp_proc_type proc, enum imsg_type type,
    uint32_t peerid, pid_t pid, int fd, void *data, uint16_t datalen)
{
	/*
	 * Each outgoing IMSG has a response IMSG associated that must be
	 * waited for before the session can be progressed further.
	 * During the wait period:
	 * 1) session must not be destroyed,
	 * 2) session must not be read from,
	 * 3) session may be written to.
	 */

	s->s_flags |= F_WAITIMSG;
	imsg_compose_event(env->sc_ievs[proc], type, peerid, pid, fd, data,
	    datalen);
}

SPLAY_GENERATE(sessiontree, session, s_nodes, session_cmp);

#define CASE(x) case x : return #x

const char *
session_strstate(int state)
{
	static char	buf[32];

	switch (state) {
	CASE(S_NEW);
	CASE(S_CONNECTED);
	CASE(S_INIT);
	CASE(S_GREETED);
	CASE(S_TLS);
	CASE(S_AUTH_INIT);
	CASE(S_AUTH_USERNAME);
	CASE(S_AUTH_PASSWORD);
	CASE(S_AUTH_FINALIZE);
	CASE(S_RSET);
	CASE(S_HELO);
	CASE(S_MAIL_MFA);
	CASE(S_MAIL_QUEUE);
	CASE(S_MAIL);
	CASE(S_RCPT_MFA);
	CASE(S_RCPT);
	CASE(S_DATA);
	CASE(S_DATA_QUEUE);
	CASE(S_DATACONTENT);
	CASE(S_DONE);
	CASE(S_QUIT);
	CASE(S_CLOSE);
	default:
		snprintf(buf, sizeof(buf), "S_??? (%d)", state);
		return buf;
	}
}
