/*	$OpenBSD: smtp_session.c,v 1.78 2009/04/28 21:55:16 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <keynote.h>

#include "smtpd.h"

int		session_rfc5321_helo_handler(struct session *, char *);
int		session_rfc5321_ehlo_handler(struct session *, char *);
int		session_rfc5321_rset_handler(struct session *, char *);
int		session_rfc5321_noop_handler(struct session *, char *);
int		session_rfc5321_data_handler(struct session *, char *);
int		session_rfc5321_mail_handler(struct session *, char *);
int		session_rfc5321_rcpt_handler(struct session *, char *);
int		session_rfc5321_vrfy_handler(struct session *, char *);
int		session_rfc5321_expn_handler(struct session *, char *);
int		session_rfc5321_turn_handler(struct session *, char *);
int		session_rfc5321_help_handler(struct session *, char *);
int		session_rfc5321_quit_handler(struct session *, char *);
int		session_rfc5321_none_handler(struct session *, char *);

int		session_rfc1652_mail_handler(struct session *, char *);

int		session_rfc3207_stls_handler(struct session *, char *);

int		session_rfc4954_auth_handler(struct session *, char *);
int		session_rfc4954_auth_plain(struct session *, char *, size_t);
int		session_rfc4954_auth_login(struct session *, char *, size_t);
void		session_auth_pickup(struct session *, char *, size_t);

void		session_read(struct bufferevent *, void *);
int		session_read_data(struct session *, char *, size_t);
void		session_write(struct bufferevent *, void *);
void		session_error(struct bufferevent *, short, void *);
void		session_command(struct session *, char *, char *);
int		session_set_path(struct path *, char *);
void		session_imsg(struct session *, enum smtp_proc_type,
		    enum imsg_type, u_int32_t, pid_t, int, void *, u_int16_t);

extern struct s_session	s_smtp;

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

int
session_rfc3207_stls_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (args != NULL) {
		session_respond(s, "501 No parameters allowed");
		return 1;
	}

	session_respond(s, "220 Ready to start TLS");

	s->s_state = S_TLS;

	return 1;
}

int
session_rfc4954_auth_handler(struct session *s, char *args)
{
	char	*method;
	char	*eom;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
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
		return session_rfc4954_auth_plain(s, eom, eom ? strlen(eom) : 0);
	else if (strcasecmp(method, "LOGIN") == 0)
		return session_rfc4954_auth_login(s, eom, eom ? strlen(eom) : 0);

	session_respond(s, "501 Syntax error");
	return 1;

}

int
session_rfc4954_auth_plain(struct session *s, char *arg, size_t nr)
{
	if (arg == NULL) {
		session_respond(s, "334");
		s->s_state = S_AUTH_INIT;
		return 1;
	}

	s->s_auth.session_id = s->s_id;
	if (strlcpy(s->s_auth.buffer, arg, sizeof(s->s_auth.buffer)) >=
	    sizeof(s->s_auth.buffer)) {
		session_respond(s, "501 Syntax error");
		return 1;
	}

	s->s_state = S_AUTH_FINALIZE;

	session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
	    &s->s_auth, sizeof(s->s_auth));

	return 1;
}

int
session_rfc4954_auth_login(struct session *s, char *arg, size_t nr)
{
	struct session_auth_req req;
	int blen = 0;
	size_t len = 0;

	switch (s->s_state) {
	case S_HELO:
		/* "Username:" base64 encoded is "VXNlcm5hbWU6" */
		session_respond(s, "334 VXNlcm5hbWU6");
		s->s_auth.session_id = s->s_id;
		s->s_state = S_AUTH_USERNAME;
		return 1;

	case S_AUTH_USERNAME:
		bzero(s->s_auth.buffer, sizeof(s->s_auth.buffer));
		if ((blen = kn_decode_base64(arg, req.buffer, sizeof(req.buffer) - 1)) == -1)
			goto err;
		/* req.buffer is a byte string, NUL terminate */
		req.buffer[blen] = '\0';
		if (! bsnprintf(s->s_auth.buffer + 1, sizeof(s->s_auth.buffer) - 1, "%s", req.buffer))
			goto err;

		/* "Password:" base64 encoded is "UGFzc3dvcmQ6" */
		session_respond(s, "334 UGFzc3dvcmQ6");
		s->s_state = S_AUTH_PASSWORD;

		return 1;

	case S_AUTH_PASSWORD: {
		if ((blen = kn_decode_base64(arg, req.buffer, sizeof(req.buffer) - 1)) == -1)
			goto err;
		/* req.buffer is a byte string, NUL terminate */
		req.buffer[blen] = '\0';

		len = strlen(s->s_auth.buffer + 1);
		if (! bsnprintf(s->s_auth.buffer + len + 2, sizeof(s->s_auth.buffer) - len - 2, "%s", req.buffer))
			goto err;

		break;
	}
	default:
		fatal("session_rfc4954_auth_login: unknown state");
	}

	s->s_state = S_AUTH_FINALIZE;

	req = s->s_auth;
	len = strlen(s->s_auth.buffer + 1) + strlen(arg) + 2;
	if (kn_encode_base64(req.buffer, len, s->s_auth.buffer, sizeof(s->s_auth.buffer)) == -1)
		goto err;

	session_imsg(s, PROC_PARENT, IMSG_PARENT_AUTHENTICATE, 0, 0, -1,
	    &s->s_auth, sizeof(s->s_auth));

	return 1;
err:
	s->s_state = S_HELO;
	session_respond(s, "535 Authentication failed");
	return 1;
}

int
session_rfc1652_mail_handler(struct session *s, char *args)
{
	char *body;

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	body = strrchr(args, ' ');
	if (body != NULL) {
		*body++ = '\0';

		if (strcasecmp("body=7bit", body) == 0) {
			s->s_flags &= ~F_8BITMIME;
		}

		else if (strcasecmp("body=8bitmime", body) != 0) {
			session_respond(s, "503 Invalid BODY");
			return 1;
		}

		return session_rfc5321_mail_handler(s, args);
	}

	return 0;
}

int
session_rfc5321_helo_handler(struct session *s, char *args)
{
	if (args == NULL) {
		session_respond(s, "501 HELO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags &= F_SECURE;

	session_respond(s, "250 %s Hello %s [%s], pleased to meet you",
	    s->s_env->sc_hostname, args, ss_to_text(&s->s_ss));

	return 1;
}

int
session_rfc5321_ehlo_handler(struct session *s, char *args)
{
	if (args == NULL) {
		session_respond(s, "501 EHLO requires domain address");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		session_respond(s, "501 Invalid domain name");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags &= F_SECURE;
	s->s_flags |= F_EHLO;
	s->s_flags |= F_8BITMIME;

	session_respond(s, "250-%s Hello %s [%s], pleased to meet you",
	    s->s_env->sc_hostname, args, ss_to_text(&s->s_ss));
	session_respond(s, "250-8BITMIME");

	/* only advertise starttls if listener can support it */
	if (s->s_l->flags & F_STARTTLS)
		session_respond(s, "250-STARTTLS");

	/* only advertise auth if session is secure */
	if ((s->s_l->flags & F_AUTH) && (s->s_flags & F_SECURE))
		session_respond(s, "250-AUTH PLAIN LOGIN");

	session_respond(s, "250 HELP");

	return 1;
}

int
session_rfc5321_rset_handler(struct session *s, char *args)
{
	s->s_state = S_HELO;
	session_respond(s, "250 Reset state");

	return 1;
}

int
session_rfc5321_noop_handler(struct session *s, char *args)
{
	session_respond(s, "250 OK");

	return 1;
}

int
session_rfc5321_mail_handler(struct session *s, char *args)
{
	char buffer[MAX_PATH_SIZE];

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state != S_HELO) {
		session_respond(s, "503 Sender already specified");
		return 1;
	}

	if (strlcpy(buffer, args, sizeof(buffer)) >= sizeof(buffer)) {
		session_respond(s, "553 Sender address syntax error");
		return 1;
	}

	if (! session_set_path(&s->s_msg.sender, buffer)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 Sender address syntax error");
		return 1;
	}

	s->rcptcount = 0;
	s->s_state = S_MAILREQUEST;
	s->s_msg.id = s->s_id;
	s->s_msg.session_id = s->s_id;
	s->s_msg.session_ss = s->s_ss;

	log_debug("session_mail_handler: sending notification to mfa");

	session_imsg(s, PROC_MFA, IMSG_MFA_MAIL, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

int
session_rfc5321_rcpt_handler(struct session *s, char *args)
{
	char buffer[MAX_PATH_SIZE];

	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 Need MAIL before RCPT");
		return 1;
	}

	if (strlcpy(buffer, args, sizeof(buffer)) >= sizeof(buffer)) {
		session_respond(s, "553 Recipient address syntax error");
		return 1;
	}

	if (! session_set_path(&s->s_msg.session_rcpt, buffer)) {
		/* No need to even transmit to MFA, path is invalid */
		session_respond(s, "553 Recipient address syntax error");
		return 1;
	}

	s->s_state = S_RCPTREQUEST;


	if (s->s_flags & F_AUTHENTICATED) {
		s->s_msg.flags |= F_MESSAGE_AUTHENTICATED;
	}

	session_imsg(s, PROC_MFA, IMSG_MFA_RCPT, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	return 1;
}

int
session_rfc5321_quit_handler(struct session *s, char *args)
{
	session_respond(s, "221 %s Closing connection", s->s_env->sc_hostname);

	s->s_flags |= F_QUIT;
	bufferevent_disable(s->s_bev, EV_READ);

	return 1;
}

int
session_rfc5321_data_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		session_respond(s, "503 Polite people say HELO first");
		return 1;
	}

	if (s->s_state == S_HELO) {
		session_respond(s, "503 Need MAIL before DATA");
		return 1;
	}

	if (s->s_state == S_MAIL) {
		session_respond(s, "503 Need RCPT before DATA");
		return 1;
	}

	s->s_state = S_DATAREQUEST;

	session_imsg(s, PROC_QUEUE, IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1,
	    &s->s_msg, sizeof(s->s_msg));

	return 1;
}

int
session_rfc5321_vrfy_handler(struct session *s, char *args)
{
	session_respond(s, "252 Cannot VRFY; try RCPT to attempt delivery");

	return 1;
}

int
session_rfc5321_expn_handler(struct session *s, char *args)
{
	session_respond(s, "502 Sorry, we do not allow this operation");

	return 1;
}

int
session_rfc5321_turn_handler(struct session *s, char *args)
{
	session_respond(s, "502 Sorry, we do not allow this operation");

	return 1;
}

int
session_rfc5321_help_handler(struct session *s, char *args)
{
	session_respond(s, "214- This is OpenSMTPD");
	session_respond(s, "214- To report bugs in the implementation, please "
	    "contact bugs@openbsd.org");
	session_respond(s, "214- with full details");
	session_respond(s, "214 End of HELP info");

	return 1;
}

void
session_command(struct session *s, char *cmd, char *args)
{
	int	i;

	if (!(s->s_flags & F_EHLO))
		goto rfc5321;

	/* RFC 1652 - 8BITMIME */
	for (i = 0; i < (int)(sizeof(rfc1652_cmdtab) / sizeof(struct session_cmd)); ++i)
		if (strcasecmp(rfc1652_cmdtab[i].name, cmd) == 0)
			break;
	if (i < (int)(sizeof(rfc1652_cmdtab) / sizeof(struct session_cmd))) {
		if (rfc1652_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 3207 - STARTTLS */
	for (i = 0; i < (int)(sizeof(rfc3207_cmdtab) / sizeof(struct session_cmd)); ++i)
		if (strcasecmp(rfc3207_cmdtab[i].name, cmd) == 0)
			break;
	if (i < (int)(sizeof(rfc3207_cmdtab) / sizeof(struct session_cmd))) {
		if (rfc3207_cmdtab[i].func(s, args))
			return;
	}

	/* RFC 4954 - AUTH */
	if ((s->s_l->flags & F_AUTH) && (s->s_flags & F_SECURE)) {
		for (i = 0; i < (int)(sizeof(rfc4954_cmdtab) / sizeof(struct session_cmd)); ++i)
			if (strcasecmp(rfc4954_cmdtab[i].name, cmd) == 0)
				break;
		if (i < (int)(sizeof(rfc4954_cmdtab) / sizeof(struct session_cmd))) {
			if (rfc4954_cmdtab[i].func(s, args))
				return;
		}
	}

rfc5321:
	/* RFC 5321 - SMTP */
	for (i = 0; i < (int)(sizeof(rfc5321_cmdtab) / sizeof(struct session_cmd)); ++i)
		if (strcasecmp(rfc5321_cmdtab[i].name, cmd) == 0)
			break;
	if (i < (int)(sizeof(rfc5321_cmdtab) / sizeof(struct session_cmd))) {
		if (rfc5321_cmdtab[i].func(s, args))
			return;
	}

	session_respond(s, "500 Command unrecognized");
}

void
session_auth_pickup(struct session *s, char *arg, size_t nr)
{
	if (s == NULL)
		fatal("session_pickup: desynchronized");

	bufferevent_enable(s->s_bev, EV_READ);

	switch (s->s_state) {
	case S_AUTH_INIT:
		session_rfc4954_auth_plain(s, arg, nr);
		break;
	case S_AUTH_USERNAME:
		session_rfc4954_auth_login(s, arg, nr);
		break;
	case S_AUTH_PASSWORD:
		session_rfc4954_auth_login(s, arg, nr);
		break;
	case S_AUTH_FINALIZE:
		if (s->s_flags & F_AUTHENTICATED)
			session_respond(s, "235 Authentication succeeded");
		else
			session_respond(s, "535 Authentication failed");
		s->s_state = S_HELO;
		break;
	default:
		fatal("session_auth_pickup: unknown state");
	}
	return;
}

void
session_pickup(struct session *s, struct submit_status *ss)
{
	if (s == NULL)
		fatal("session_pickup: desynchronized");

	bufferevent_enable(s->s_bev, EV_READ);

	if ((ss != NULL && ss->code == 421) ||
	    (s->s_msg.status & S_MESSAGE_TEMPFAILURE)) {
		session_respond(s, "421 Service temporarily unavailable");
		s->s_flags |= F_QUIT;
		bufferevent_disable(s->s_bev, EV_READ);
		return;
	}

	switch (s->s_state) {
	case S_INIT:
		s->s_state = S_GREETED;
		log_debug("session_pickup: greeting client");
		session_respond(s, SMTPD_BANNER, s->s_env->sc_hostname);
		break;

	case S_GREETED:
	case S_HELO:
		break;

	case S_TLS:
		s->s_flags |= F_EVLOCKED;
		bufferevent_disable(s->s_bev, EV_READ|EV_WRITE);
		s->s_state = S_GREETED;
		ssl_session_init(s);
		break;

	case S_MAILREQUEST:
		if (ss == NULL)
			fatalx("bad ss at S_MAILREQUEST");
		/* sender was not accepted, downgrade state */
		if (ss->code != 250) {
			s->s_state = S_HELO;
			session_respond(s, "%d Sender rejected", ss->code);
			return;
		}

		s->s_state = S_MAIL;
		s->s_msg.sender = ss->u.path;

		session_imsg(s, PROC_QUEUE, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
		    &s->s_msg, sizeof(s->s_msg));
		break;

	case S_MAIL:
		if (ss == NULL)
			fatalx("bad ss at S_MAIL");
		session_respond(s, "%d Sender ok", ss->code);
		break;

	case S_RCPTREQUEST:
		if (ss == NULL)
			fatalx("bad ss at S_RCPTREQUEST");
		/* recipient was not accepted */
		if (ss->code != 250) {
			/* We do not have a valid recipient, downgrade state */
			if (s->rcptcount == 0)
				s->s_state = S_MAIL;
			else
				s->s_state = S_RCPT;
			session_respond(s, "%d Recipient rejected", ss->code);
			return;
		}

		s->s_state = S_RCPT;
		s->rcptcount++;
		s->s_msg.recipient = ss->u.path;

		session_respond(s, "%d Recipient ok", ss->code);
		break;

	case S_DATAREQUEST:
		s->s_state = S_DATACONTENT;
		session_respond(s, "354 Enter mail, end with \".\" on a line by"
		    " itself");
		break;

	case S_DONE:
		s->s_state = S_HELO;
		session_respond(s, "250 %s Message accepted for delivery",
		    s->s_msg.message_id);
		log_info("%s: from=<%s@%s>, nrcpts=%zd, proto=%s, relay=%s [%s]",
		    s->s_msg.message_id,
		    s->s_msg.sender.user,
		    s->s_msg.sender.domain,
		    s->rcptcount,
		    s->s_flags & F_EHLO ? "ESMTP" : "SMTP",
		    s->s_hostname,
		    ss_to_text(&s->s_ss));

		s->s_msg.message_id[0] = '\0';
		s->s_msg.message_uid[0] = '\0';
		break;

	default:
		fatal("session_pickup: unknown state");
	}
}

void
session_init(struct listener *l, struct session *s)
{
	s->s_state = S_INIT;

	if ((s->s_bev = bufferevent_new(s->s_fd, session_read, session_write,
	    session_error, s)) == NULL)
		fatalx("session_init: bufferevent_new failed");

	bufferevent_settimeout(s->s_bev, SMTPD_SESSION_TIMEOUT, 0);

	if (l->flags & F_SMTPS) {
		log_debug("session_init: initializing ssl");
		s->s_flags |= F_EVLOCKED;
		bufferevent_disable(s->s_bev, EV_READ|EV_WRITE);
		ssl_session_init(s);
		return;
	}

	session_pickup(s, NULL);
}

void
session_read(struct bufferevent *bev, void *p)
{
	struct session	*s = p;
	char		*line;
	char		*ep;
	char		*args;
	size_t		 nr;

read:
	nr = EVBUFFER_LENGTH(bev->input);
	line = evbuffer_readline(bev->input);
	if (line == NULL) {
		if (EVBUFFER_LENGTH(bev->input) > SMTP_ANYLINE_MAX) {
			session_respond(s, "500 Line too long");
			s->s_flags |= F_QUIT;
			bufferevent_disable(s->s_bev, EV_READ);
		}
		return;
	}
	nr -= EVBUFFER_LENGTH(bev->input);

	if (s->s_state == S_DATACONTENT) {
		if (session_read_data(s, line, nr)) {
			free(line);
			return;
		}
		free(line);
		goto read;
	}

	if (IS_AUTH(s->s_state)) {
		session_auth_pickup(s, line, nr);
		free(line);
		return;
	}

	if (nr > SMTP_CMDLINE_MAX) {
		session_respond(s, "500 Line too long");
		return;
	}

	if ((ep = strchr(line, ':')) == NULL)
		ep = strchr(line, ' ');
	if (ep != NULL) {
		*ep = '\0';
		args = ++ep;
		while (isspace((int)*args))
			args++;
	} else
		args = NULL;
	log_debug("command: %s\targs: %s", line, args);
	session_command(s, line, args);
	free(line);
	return;
}

int
session_read_data(struct session *s, char *line, size_t nread)
{
	size_t len;
	size_t i;

	if (strcmp(line, ".") == 0) {
		if (! safe_fclose(s->datafp))
			s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
		s->datafp = NULL;

		if (s->s_msg.status & S_MESSAGE_PERMFAILURE) {
			session_respond(s, "554 Transaction failed");
			s->s_state = S_HELO;
		} else if (s->s_msg.status & S_MESSAGE_TEMPFAILURE) {
			session_respond(s, "421 Temporary failure");
			s->s_state = S_HELO;
		} else {
			session_imsg(s, PROC_QUEUE, IMSG_QUEUE_COMMIT_MESSAGE,
			    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
			s->s_state = S_DONE;
		}

		return 1;
	}

	/* Don't waste resources on message if it's going to bin anyway. */
	if (s->s_msg.status & (S_MESSAGE_PERMFAILURE|S_MESSAGE_TEMPFAILURE))
		return 0;

	if (nread > SMTP_TEXTLINE_MAX) {
		s->s_msg.status |= S_MESSAGE_PERMFAILURE;
		return 0;
	}

	/* "If the first character is a period and there are other characters
	 *  on the line, the first character is deleted." [4.5.2]
	 */
	if (*line == '.')
		line++;

	len = strlen(line);

	if (fwrite(line, len, 1, s->datafp) != 1 ||
	    fwrite("\n", 1, 1, s->datafp) != 1) {
		s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
		return 0;
	}

	if (! (s->s_flags & F_8BITMIME)) {
		for (i = 0; i < len; ++i)
			if (line[i] & 0x80)
				break;
		if (i != len) {
			s->s_msg.status |= S_MESSAGE_PERMFAILURE;
			return 0;
		}
	}

	return 0;
}

void
session_write(struct bufferevent *bev, void *p)
{
	struct session	*s = p;

	if (!(s->s_flags & F_QUIT)) {
		
		if (s->s_state == S_TLS)
			session_pickup(s, NULL);

		return;
	}

	session_destroy(s);
}

void
session_destroy(struct session *s)
{
	log_debug("session_destroy: killing client: %p", s);

	if (s->datafp != NULL)
		fclose(s->datafp);

	if (s->s_msg.message_id[0] != '\0' && s->s_state != S_DONE) {
		/*
		 * IMSG_QUEUE_REMOVE_MESSAGE must not be sent using session_imsg
		 * since no reply for it is expected.
		 */
		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_REMOVE_MESSAGE, 0, 0, -1, &s->s_msg,
		    sizeof(s->s_msg));
		s->s_msg.message_id[0] = '\0';
		s->s_msg.message_uid[0] = '\0';
	}

	close(s->s_fd);

	s_smtp.sessions_active--;
	if (s_smtp.sessions_active < s->s_env->sc_maxconn &&
	    !(s->s_msg.flags & F_MESSAGE_ENQUEUED))
		event_add(&s->s_l->ev, NULL);

	if (s->s_bev != NULL) {
		bufferevent_free(s->s_bev);
	}
	ssl_session_destroy(s);

	SPLAY_REMOVE(sessiontree, &s->s_env->sc_sessions, s);
	bzero(s, sizeof(*s));
	free(s);
}

void
session_error(struct bufferevent *bev, short event, void *p)
{
	struct session	*s = p;

	if (event & EVBUFFER_TIMEOUT)
		s_smtp.timeout++;
	else
		s_smtp.aborted++;

	/* If events are locked, do not destroy session
	 * but set F_QUIT flag so that we destroy it as
	 * soon as the event lock is removed.
	 */
	if (s->s_flags & F_EVLOCKED) {
		s->s_flags |= F_QUIT;
		bufferevent_disable(s->s_bev, EV_READ);
	} else
		session_destroy(s);
}

int
session_cmp(struct session *s1, struct session *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->s_id < s2->s_id)
		return (-1);

	if (s1->s_id > s2->s_id)
		return (1);

	return (0);
}

int
session_set_path(struct path *path, char *line)
{
	size_t len;

	len = strlen(line);
	if (*line != '<' || line[len - 1] != '>')
		return 0;
	line[len - 1] = '\0';

	return recipient_to_path(path, line + 1);
}

void
session_respond(struct session *s, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (evbuffer_add_vprintf(EVBUFFER_OUTPUT(s->s_bev), fmt, ap) == -1 ||
	    evbuffer_add_printf(EVBUFFER_OUTPUT(s->s_bev), "\r\n") == -1)
		fatal("session_respond: evbuffer_add_vprintf failed");
	va_end(ap);

	bufferevent_enable(s->s_bev, EV_WRITE);
}

/*
 * Send IMSG, waiting for reply safely.
 */
void
session_imsg(struct session *s, enum smtp_proc_type proc, enum imsg_type type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	imsg_compose(s->s_env->sc_ibufs[proc], type, peerid, pid, fd, data,
	    datalen);

	/*
	 * Most IMSGs require replies before session can be safely resumed.
	 * Ignore client events so that malicious client cannot trigger
	 * session_pickup at a bad time.
	 */
	bufferevent_disable(s->s_bev, EV_READ);

	/*
	 * If session is unexpectedly teared down, event(3) calls session_error
	 * without honoring EV_READ block.
	 * To avoid session data being destroyed while an IMSG requiring it
	 * is with other process, provide a flag that session_error can use to
	 * determine if it is safe to destroy session data.
	 */
	if (s->s_flags & F_EVLOCKED)
		fatalx("session_imsg: imsg sent when another is pending");
	s->s_flags |= F_EVLOCKED;
}

SPLAY_GENERATE(sessiontree, session, s_nodes, session_cmp);
