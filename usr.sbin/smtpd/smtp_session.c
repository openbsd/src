/*	$OpenBSD: smtp_session.c,v 1.24 2008/12/07 01:03:25 jacekm Exp $	*/

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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

void		session_command(struct session *, char *, char *);
int		session_set_path(struct path *, char *);
void		session_timeout(int, short, void *);

struct session_timeout {
	enum session_state	state;
	time_t			timeout;
};

struct session_timeout rfc5321_timeouttab[] = {
	{ S_INIT,		300 },
	{ S_GREETED,		300 },
	{ S_HELO,		300 },
	{ S_MAIL,		300 },
	{ S_RCPT,		300 },
	{ S_DATA,		120 },
	{ S_DATACONTENT,	180 },
	{ S_DONE,		600 }
};

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
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	if (args != NULL) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 Syntax error (no parameters allowed)\r\n");
		return 1;
	}

	evbuffer_add_printf(s->s_bev->output,
	    "220 Ready to start TLS\r\n");

	s->s_state = S_TLS;

	return 1;
}

int
session_rfc4954_auth_handler(struct session *s, char *args)
{
	char	*method;
	char	*eom;
	struct session_auth_req req;

	if (s->s_state == S_GREETED) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	if (args == NULL) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 Syntax error (no parameters given)\r\n");
		return 1;
	}

	method = args;
	eom = strchr(args, ' ');
	if (eom == NULL)
		eom = strchr(args, '\t');
	if (eom != NULL)
		*eom++ = '\0';

	if (eom == NULL) {
		/* NEEDS_FIX - unsupported yet */
		evbuffer_add_printf(s->s_bev->output,
		    "501 Syntax error\r\n");
		return 1;
	}

	req.session_id = s->s_id;
	if (strlcpy(req.buffer, eom, sizeof(req.buffer)) >=
	    sizeof(req.buffer)) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 Syntax error\r\n");
		return 1;
	}

	s->s_state = S_AUTH;

	imsg_compose(s->s_env->sc_ibufs[PROC_PARENT], IMSG_PARENT_AUTHENTICATE,
	    0, 0, -1, &req, sizeof(req));

	return 1;
}

int
session_rfc1652_mail_handler(struct session *s, char *args)
{
	char *body;

	if (s->s_state == S_GREETED) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	body = strrchr(args, ' ');
	if (body != NULL) {
		*body++ = '\0';

		if (strcasecmp("body=7bit", body) == 0) {
			s->s_flags &= ~F_8BITMIME;
		}

		else if (strcasecmp("body=8bitmime", body) != 0) {
			evbuffer_add_printf(s->s_bev->output,
			    "503 Invalid BODY\r\n");
			return 1;
		}

		return session_rfc5321_mail_handler(s, args);
	}

	return 0;
}

int
session_rfc5321_helo_handler(struct session *s, char *args)
{
	void	*p;
	char	 addrbuf[INET6_ADDRSTRLEN];

	if (args == NULL) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 HELO requires domain address.\r\n");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 Invalid domain name\r\n");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags = 0;

	if (s->s_ss.ss_family == PF_INET) {
		struct sockaddr_in *ssin = (struct sockaddr_in *)&s->s_ss;
		p = &ssin->sin_addr.s_addr;
	}
	if (s->s_ss.ss_family == PF_INET6) {
		struct sockaddr_in6 *ssin6 = (struct sockaddr_in6 *)&s->s_ss;
		p = &ssin6->sin6_addr.s6_addr;
	}

	bzero(addrbuf, sizeof (addrbuf));
	inet_ntop(s->s_ss.ss_family, p, addrbuf, sizeof (addrbuf));

	evbuffer_add_printf(s->s_bev->output,
	    "250 %s Hello %s [%s%s], pleased to meet you\r\n",
	    s->s_env->sc_hostname, args,
	    s->s_ss.ss_family == PF_INET ? "" : "IPv6:", addrbuf);

	return 1;
}

int
session_rfc5321_ehlo_handler(struct session *s, char *args)
{
	void	*p;
	char	 addrbuf[INET6_ADDRSTRLEN];

	if (args == NULL) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 EHLO requires domain address.\r\n");
		return 1;
	}

	if (strlcpy(s->s_msg.session_helo, args, sizeof(s->s_msg.session_helo))
	    >= sizeof(s->s_msg.session_helo)) {
		evbuffer_add_printf(s->s_bev->output,
		    "501 Invalid domain name\r\n");
		return 1;
	}

	s->s_state = S_HELO;
	s->s_flags = F_EHLO;
	s->s_flags |= F_8BITMIME;

	if (s->s_ss.ss_family == PF_INET) {
		struct sockaddr_in *ssin = (struct sockaddr_in *)&s->s_ss;
		p = &ssin->sin_addr.s_addr;
	}
	if (s->s_ss.ss_family == PF_INET6) {
		struct sockaddr_in6 *ssin6 = (struct sockaddr_in6 *)&s->s_ss;
		p = &ssin6->sin6_addr.s6_addr;
	}

	bzero(addrbuf, sizeof (addrbuf));
	inet_ntop(s->s_ss.ss_family, p, addrbuf, sizeof (addrbuf));
	evbuffer_add_printf(s->s_bev->output,
	    "250-%s Hello %s [%s%s], pleased to meet you\r\n",
	    s->s_env->sc_hostname, args,
	    s->s_ss.ss_family == PF_INET ? "" : "IPv6:", addrbuf);

	evbuffer_add_printf(s->s_bev->output, "250-8BITMIME\r\n");

	/* only advertise starttls if listener can support it */
	if (s->s_l->flags & F_STARTTLS)
		evbuffer_add_printf(s->s_bev->output, "250-STARTTLS\r\n");

	/* only advertise auth if session is secure */
	/*
	if (s->s_flags & F_SECURE)
		evbuffer_add_printf(s->s_bev->output, "250-AUTH %s\r\n", "PLAIN");
	 */
	evbuffer_add_printf(s->s_bev->output, "250 HELP\r\n");

	return 1;
}

int
session_rfc5321_rset_handler(struct session *s, char *args)
{
	s->s_msg.rcptcount = 0;
	s->s_state = S_HELO;
	evbuffer_add_printf(s->s_bev->output, "250 Reset state.\r\n");

	return 1;
}

int
session_rfc5321_noop_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output, "250 OK.\r\n");

	return 1;
}

int
session_rfc5321_mail_handler(struct session *s, char *args)
{
	char buffer[MAX_PATH_SIZE];

	if (s->s_state == S_GREETED) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	if (strlcpy(buffer, args, sizeof(buffer)) >= sizeof(buffer)) {
		evbuffer_add_printf(s->s_bev->output,
		    "553 Syntax error for sender address\r\n");
		return 1;
	}

	if (! session_set_path(&s->s_msg.sender, buffer)) {
		/* No need to even transmit to MFA, path is invalid */
		evbuffer_add_printf(s->s_bev->output,
		    "553 Syntax error for sender address\r\n");
		return 1;
	}

	s->s_msg.rcptcount = 0;

	s->s_state = S_MAILREQUEST;
	s->s_msg.id = s->s_id;
	s->s_msg.session_id = s->s_id;
	s->s_msg.session_ss = s->s_ss;

	log_debug("session_mail_handler: sending notification to mfa");

	imsg_compose(s->s_env->sc_ibufs[PROC_MFA], IMSG_MFA_RPATH_SUBMIT,
	    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
	bufferevent_disable(s->s_bev, EV_READ);
	return 1;
}

int
session_rfc5321_rcpt_handler(struct session *s, char *args)
{
	char buffer[MAX_PATH_SIZE];
	struct message_recipient	mr;

	if (s->s_state == S_GREETED) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	if (s->s_state == S_HELO) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Need MAIL before RCPT\r\n");
		return 1;
	}

	bzero(&mr, sizeof(mr));

	if (strlcpy(buffer, args, sizeof(buffer)) >= sizeof(buffer)) {
		evbuffer_add_printf(s->s_bev->output,
		    "553 Syntax error for recipient address\r\n");
		return 1;
	}

	if (! session_set_path(&mr.path, buffer)) {
		/* No need to even transmit to MFA, path is invalid */
		evbuffer_add_printf(s->s_bev->output,
		    "553 Syntax error for recipient address\r\n");
		return 1;
	}

	mr.id = s->s_msg.id;
	s->s_state = S_RCPTREQUEST;
	mr.ss = s->s_ss;

	imsg_compose(s->s_env->sc_ibufs[PROC_MFA], IMSG_MFA_RCPT_SUBMIT,
	    0, 0, -1, &mr, sizeof(mr));
	bufferevent_disable(s->s_bev, EV_READ);
	return 1;
}

int
session_rfc5321_quit_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output, "221 %s Closing connection.\r\n",
	    s->s_env->sc_hostname);

	s->s_flags |= F_QUIT;

	return 1;
}

int
session_rfc5321_data_handler(struct session *s, char *args)
{
	if (s->s_state == S_GREETED) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Polite people say HELO first\r\n");
		return 1;
	}

	if (s->s_state == S_HELO) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Need MAIL before DATA\r\n");
		return 1;
	}

	if (s->s_state == S_MAIL) {
		evbuffer_add_printf(s->s_bev->output,
		    "503 Need RCPT before DATA\r\n");
		return 1;
	}

	s->s_state = S_DATAREQUEST;
	session_pickup(s, NULL);
	return 1;
}

int
session_rfc5321_vrfy_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output,
	    "252 Cannot VRFY user; try RCPT to attempt delivery.\r\n");

	return 1;
}

int
session_rfc5321_expn_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output,
	    "502 Sorry, we do not allow this operation.\r\n");

	return 1;
}

int
session_rfc5321_turn_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output,
	    "502 Sorry, we do not allow this operation.\r\n");

	return 1;
}

int
session_rfc5321_help_handler(struct session *s, char *args)
{
	evbuffer_add_printf(s->s_bev->output,
	    "214- This is OpenSMTPD\r\n"
	    "214- To report bugs in the implementation, please contact\r\n"
	    "214- bugs@openbsd.org with full details\r\n");
	evbuffer_add_printf(s->s_bev->output,
	    "214 End of HELP info\r\n");

	return 1;
}

void
session_command(struct session *s, char *cmd, char *args)
{
	int	i;

	bufferevent_enable(s->s_bev, EV_WRITE);

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
	/*
	for (i = 0; i < (int)(sizeof(rfc4954_cmdtab) / sizeof(struct session_cmd)); ++i)
		if (strcasecmp(rfc4954_cmdtab[i].name, cmd) == 0)
			break;
	if (i < (int)(sizeof(rfc4954_cmdtab) / sizeof(struct session_cmd))) {
		if (rfc4954_cmdtab[i].func(s, args))
			return;
	}
	*/

rfc5321:
	/* RFC 5321 - SMTP */
	for (i = 0; i < (int)(sizeof(rfc5321_cmdtab) / sizeof(struct session_cmd)); ++i)
		if (strcasecmp(rfc5321_cmdtab[i].name, cmd) == 0)
			break;
	if (i < (int)(sizeof(rfc5321_cmdtab) / sizeof(struct session_cmd))) {
		if (rfc5321_cmdtab[i].func(s, args))
			return;
	}

	evbuffer_add_printf(s->s_bev->output,
	    "500 Command unrecognized.\r\n");
}

void
session_pickup(struct session *s, struct submit_status *ss)
{
	if (s == NULL)
		fatal("session_pickup: desynchronized");

	bufferevent_enable(s->s_bev, EV_READ|EV_WRITE);

	if (ss != NULL && ss->code == 421)
		goto tempfail;

	switch (s->s_state) {
	case S_INIT:
		s->s_state = S_GREETED;
		log_debug("session_pickup: greeting client");
		evbuffer_add_printf(s->s_bev->output,
		    SMTPD_BANNER, s->s_env->sc_hostname);
		break;

	case S_GREETED:
	case S_HELO:
		break;

	case S_TLS:
		bufferevent_disable(s->s_bev, EV_READ|EV_WRITE);
		s->s_state = S_GREETED;
		ssl_session_init(s);
		break;

	case S_AUTH:
		if (s->s_flags & F_AUTHENTICATED) {
			evbuffer_add_printf(s->s_bev->output,
			    "235 Authentication Succeeded\r\n");
		}
		else {
			evbuffer_add_printf(s->s_bev->output,
			    "535 Authentication Credentials Invalid\r\n");
		}
		break;

	case S_MAILREQUEST:
		/* sender was not accepted, downgrade state */
		if (ss->code != 250) {
			s->s_state = S_HELO;
			evbuffer_add_printf(s->s_bev->output,
			    "%d Sender rejected\r\n", ss->code);
			return;
		}

		s->s_state = S_MAIL;
		s->s_msg.sender = ss->u.path;

		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1, &s->s_msg,
		    sizeof(s->s_msg));
		bufferevent_disable(s->s_bev, EV_READ);
		break;

	case S_MAIL:

		evbuffer_add_printf(s->s_bev->output, "%d Sender ok\r\n",
		    ss->code);

		break;

	case S_RCPTREQUEST:
		/* recipient was not accepted */
		if (ss->code != 250) {
			/* We do not have a valid recipient, downgrade state */
			if (s->s_msg.rcptcount == 0)
				s->s_state = S_MAIL;
			else
				s->s_state = S_RCPT;
			evbuffer_add_printf(s->s_bev->output,
			    "%d Recipient rejected\r\n", ss->code);
			return;
		}

		s->s_state = S_RCPT;
		s->s_msg.rcptcount++;
		s->s_msg.recipient = ss->u.path;
		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1, &s->s_msg,
		    sizeof(s->s_msg));
		bufferevent_disable(s->s_bev, EV_READ);
		break;

	case S_RCPT:
		evbuffer_add_printf(s->s_bev->output, "%d Recipient ok\r\n",
		    ss->code);
		break;

	case S_DATAREQUEST:
		s->s_state = S_DATA;
		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1, &s->s_msg,
		    sizeof(s->s_msg));
		bufferevent_disable(s->s_bev, EV_READ);
		break;

	case S_DATA:
		if (s->s_msg.datafp == NULL)
			goto tempfail;

		s->s_state = S_DATACONTENT;
		evbuffer_add_printf(s->s_bev->output,
		    "354 Enter mail, end with \".\" on a line by itself\r\n");
		break;

	case S_DONE:
		s->s_state = S_HELO;

		evbuffer_add_printf(s->s_bev->output,
		    "250 %s Message accepted for delivery\r\n",
		    s->s_msg.message_id);

		break;

	default:
		log_debug("session_pickup: state value: %d", s->s_state);
		fatal("session_pickup: unknown state");
		break;
	}

	return;

tempfail:
	s->s_flags |= F_QUIT;
	evbuffer_add_printf(s->s_bev->output,
	    "421 Service temporarily unavailable\r\n");
	return;
}

void
session_init(struct listener *l, struct session *s)
{
	s->s_state = S_INIT;
	s->s_env = l->env;
	s->s_l = l;
	s->s_id = queue_generate_id();
	strlcpy(s->s_hostname, "<unknown>", MAXHOSTNAMELEN);
	strlcpy(s->s_msg.session_hostname, s->s_hostname, MAXHOSTNAMELEN);

	SPLAY_INSERT(sessiontree, &s->s_env->sc_sessions, s);

	imsg_compose(s->s_env->sc_ibufs[PROC_LKA], IMSG_LKA_HOSTNAME_LOOKUP,
	    0, 0, -1, s, sizeof(struct session));

	if ((s->s_bev = bufferevent_new(s->s_fd, session_read, session_write,
	    session_error, s)) == NULL)
		fatal(NULL);

	if (l->flags & F_SSMTP) {
		log_debug("session_init: initializing ssl");
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

read:
	s->s_tm = time(NULL);
	line = evbuffer_readline(bev->input);
	if (line == NULL) {
		if (s->s_state != S_DATACONTENT)
			bufferevent_disable(s->s_bev, EV_READ);
		return;
	}

	if (s->s_state == S_DATACONTENT) {
		/*log_debug("content: %s", line);*/
		if (strcmp(line, ".") == 0) {
			s->s_state = S_DONE;
			fclose(s->s_msg.datafp);
			s->s_msg.datafp = NULL;

			bufferevent_disable(s->s_bev, EV_READ);

			if (s->s_msg.status & S_MESSAGE_PERMFAILURE) {
				bufferevent_disable(s->s_bev, EV_WRITE);
				evbuffer_add_printf(s->s_bev->output,
				    "554 Transaction failed\r\n");

				/* Remove message file */
				imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_REMOVE_MESSAGE,
				    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
				free(line);
				return;
			}
			session_msg_submit(s);
			free(line);
			return;
		} else {
			size_t i;
			size_t len;

			len = strlen(line);
			fwrite(line, 1, len, s->s_msg.datafp);
			fwrite("\n", 1, 1, s->s_msg.datafp);
			fflush(s->s_msg.datafp);

			if (! (s->s_flags & F_8BITMIME)) {
				for (i = 0; i < len; ++i) {
					if (line[i] & 0x80) {
						s->s_msg.status |= S_MESSAGE_PERMFAILURE;
						strlcpy(s->s_msg.session_errorline,
						    "8BIT data transfered over 7BIT limited channel",
							sizeof s->s_msg.session_errorline);
					}
				}
			}
			free(line);
		}
		goto read;
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
	/*
	 * cleanup
	 */
	log_debug("session_destroy: killing client: %p", s);
	close(s->s_fd);

	if (s->s_msg.datafp != NULL) {
		fclose(s->s_msg.datafp);
		s->s_msg.datafp = NULL;
	}

	if (s->s_state >= S_MAIL) {
		imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_REMOVE_MESSAGE,
		    0, 0, -1, &s->s_msg, sizeof(s->s_msg));
	}

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

	session_destroy(s);
}

void
session_msg_submit(struct session *s)
{
	imsg_compose(s->s_env->sc_ibufs[PROC_QUEUE],
	    IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1, &s->s_msg,
	    sizeof(s->s_msg));
	s->s_state = S_DONE;
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
	char *username;
	char *hostname;

	len = strlen(line);
	if (*line != '<' || line[len - 1] != '>')
		return 0;
	line[len - 1] = '\0';

	username = line + 1;
	hostname = strchr(username, '@');

	if (username[0] == '\0') {
		*path->user = '\0';
		*path->domain = '\0';
		return 1;
	}

	if (hostname == NULL) {
		if (strcasecmp(username, "postmaster") != 0)
			return 0;
		hostname = "localhost";
	} else {
		*hostname++ = '\0';
	}

	if (strlcpy(path->user, username, sizeof(path->user))
	    >= MAX_LOCALPART_SIZE)
		return 0;

	if (strlcpy(path->domain, hostname, sizeof(path->domain))
	    >= MAX_DOMAINPART_SIZE)
		return 0;

	return 1;
}

void
session_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct session		*sessionp;
	struct session		*rmsession;
	struct timeval		 tv;
	time_t			 tm;
	u_int8_t		 i;

	tm = time(NULL);
	rmsession = NULL;
	SPLAY_FOREACH(sessionp, sessiontree, &env->sc_sessions) {

		if (rmsession != NULL) {
			session_destroy(rmsession);
			rmsession = NULL;
		}

		for (i = 0; i < sizeof (rfc5321_timeouttab) /
			 sizeof(struct session_timeout); ++i)
			if (rfc5321_timeouttab[i].state == sessionp->s_state)
				break;

		if (i == sizeof (rfc5321_timeouttab) / sizeof (struct session_timeout)) {
			if (tm - SMTPD_SESSION_TIMEOUT < sessionp->s_tm)
				continue;
		}
		else if (tm - rfc5321_timeouttab[i].timeout < sessionp->s_tm) {
				continue;
		}

		rmsession = sessionp;
	}

	if (rmsession != NULL)
		session_destroy(rmsession);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}


SPLAY_GENERATE(sessiontree, session, s_nodes, session_cmp);
