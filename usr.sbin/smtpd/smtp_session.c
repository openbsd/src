/*	$OpenBSD: smtp_session.c,v 1.329 2018/05/04 10:49:49 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <inttypes.h>
#include <openssl/ssl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

#define	SMTP_LINE_MAX			16384
#define	DATA_HIWAT			65535
#define	APPEND_DOMAIN_BUFFER_SIZE	SMTP_LINE_MAX

enum smtp_state {
	STATE_NEW = 0,
	STATE_CONNECTED,
	STATE_TLS,
	STATE_HELO,
	STATE_AUTH_INIT,
	STATE_AUTH_USERNAME,
	STATE_AUTH_PASSWORD,
	STATE_AUTH_FINALIZE,
	STATE_BODY,
	STATE_QUIT,
};

enum session_flags {
	SF_EHLO			= 0x0001,
	SF_8BITMIME		= 0x0002,
	SF_SECURE		= 0x0004,
	SF_AUTHENTICATED	= 0x0008,
	SF_BOUNCE		= 0x0010,
	SF_VERIFIED		= 0x0020,
	SF_BADINPUT		= 0x0080,
};

enum {
	TX_OK = 0,
	TX_ERROR_ENVELOPE,
	TX_ERROR_SIZE,
	TX_ERROR_IO,
	TX_ERROR_LOOP,
	TX_ERROR_MALFORMED,
	TX_ERROR_RESOURCES
};

enum smtp_command {
	CMD_HELO = 0,
	CMD_EHLO,
	CMD_STARTTLS,
	CMD_AUTH,
	CMD_MAIL_FROM,
	CMD_RCPT_TO,
	CMD_DATA,
	CMD_RSET,
	CMD_QUIT,
	CMD_HELP,
	CMD_WIZ,
	CMD_NOOP,
};

struct smtp_rcpt {
	TAILQ_ENTRY(smtp_rcpt)	 entry;
 	struct mailaddr		 maddr;
	size_t			 destcount;
};

struct smtp_tx {
	struct smtp_session	*session;
	uint32_t		 msgid;

	struct envelope		 evp;
	size_t			 rcptcount;
	size_t			 destcount;
	TAILQ_HEAD(, smtp_rcpt)	 rcpts;

	time_t			 time;
	int			 error;
	size_t			 datain;
	size_t			 odatalen;
	FILE			*ofile;
	int			 hdrdone;
	int			 rcvcount;
	int			 skiphdr;
	struct rfc2822_parser	 rfc2822_parser;
};

struct smtp_session {
	uint64_t		 id;
	struct io		*io;
	struct listener		*listener;
	void			*ssl_ctx;
	struct sockaddr_storage	 ss;
	char			 hostname[HOST_NAME_MAX+1];
	char			 smtpname[HOST_NAME_MAX+1];

	int			 flags;
	enum smtp_state		 state;

	char			 helo[LINE_MAX];
	char			 cmd[LINE_MAX];
	char			 username[SMTPD_MAXMAILADDRSIZE];

	size_t			 mailcount;
	struct event		 pause;

	struct smtp_tx		*tx;
};

#define ADVERTISE_TLS(s) \
	((s)->listener->flags & F_STARTTLS && !((s)->flags & SF_SECURE))

#define ADVERTISE_AUTH(s) \
	((s)->listener->flags & F_AUTH && (s)->flags & SF_SECURE && \
	 !((s)->flags & SF_AUTHENTICATED))

#define ADVERTISE_EXT_DSN(s) \
	((s)->listener->flags & F_EXT_DSN)

static int smtp_mailaddr(struct mailaddr *, char *, int, char **, const char *);
static void smtp_session_init(void);
static int smtp_lookup_servername(struct smtp_session *);
static void smtp_connected(struct smtp_session *);
static void smtp_send_banner(struct smtp_session *);
static void smtp_tls_verified(struct smtp_session *);
static void smtp_io(struct io *, int, void *);
static void smtp_enter_state(struct smtp_session *, int);
static void smtp_reply(struct smtp_session *, char *, ...);
static void smtp_command(struct smtp_session *, char *);
static int smtp_parse_mail_args(struct smtp_session *, char *);
static int smtp_parse_rcpt_args(struct smtp_session *, char *);
static void smtp_rfc4954_auth_plain(struct smtp_session *, char *);
static void smtp_rfc4954_auth_login(struct smtp_session *, char *);
static void smtp_free(struct smtp_session *, const char *);
static const char *smtp_strstate(int);
static void smtp_tls_init(struct smtp_session *);
static int smtp_verify_certificate(struct smtp_session *);
static void smtp_auth_failure_pause(struct smtp_session *);
static void smtp_auth_failure_resume(int, short, void *);

static int  smtp_tx(struct smtp_session *);
static void smtp_tx_free(struct smtp_tx *);
static void smtp_tx_create_message(struct smtp_tx *);
static void smtp_tx_mail_from(struct smtp_tx *, char *);
static void smtp_tx_rcpt_to(struct smtp_tx *, char *);
static void smtp_tx_open_message(struct smtp_tx *);
static void smtp_tx_commit(struct smtp_tx *);
static void smtp_tx_rollback(struct smtp_tx *);
static int  smtp_tx_dataline(struct smtp_tx *, const char *);
static void smtp_message_fd(struct smtp_tx *, int);
static void smtp_message_end(struct smtp_tx *);
static int  smtp_message_printf(struct smtp_tx *, const char *, ...);

static struct { int code; const char *cmd; } commands[] = {
	{ CMD_HELO,		"HELO" },
	{ CMD_EHLO,		"EHLO" },
	{ CMD_STARTTLS,		"STARTTLS" },
	{ CMD_AUTH,		"AUTH" },
	{ CMD_MAIL_FROM,	"MAIL FROM" },
	{ CMD_RCPT_TO,		"RCPT TO" },
	{ CMD_DATA,		"DATA" },
	{ CMD_RSET,		"RSET" },
	{ CMD_QUIT,		"QUIT" },
	{ CMD_HELP,		"HELP" },
	{ CMD_WIZ,		"WIZ" },
	{ CMD_NOOP,		"NOOP" },
	{ -1, NULL },
};

static struct tree wait_lka_ptr;
static struct tree wait_lka_helo;
static struct tree wait_lka_mail;
static struct tree wait_lka_rcpt;
static struct tree wait_parent_auth;
static struct tree wait_queue_msg;
static struct tree wait_queue_fd;
static struct tree wait_queue_commit;
static struct tree wait_ssl_init;
static struct tree wait_ssl_verify;

static void
header_default_callback(const struct rfc2822_header *hdr, void *arg)
{
	struct smtp_tx         *tx = arg;
	struct rfc2822_line    *l;

	if (smtp_message_printf(tx, "%s:", hdr->name) == -1)
		return;

	TAILQ_FOREACH(l, &hdr->lines, next)
		if (smtp_message_printf(tx, "%s\n", l->buffer) == -1)
			return;
}

static void
dataline_callback(const char *line, void *arg)
{
	struct smtp_tx *tx = arg;

	smtp_message_printf(tx, "%s\n", line);
}

static void
header_bcc_callback(const struct rfc2822_header *hdr, void *arg)
{
}

static void
header_append_domain_buffer(char *buffer, char *domain, size_t len)
{
	size_t	i;
	int	escape, quote, comment, bracket;
	int	has_domain, has_bracket, has_group;
	int	pos_bracket, pos_component, pos_insert;
	char	copy[APPEND_DOMAIN_BUFFER_SIZE];

	i = 0;
	escape = quote = comment = bracket = 0;
	has_domain = has_bracket = has_group = 0;
	pos_bracket = pos_insert = pos_component = 0;
	for (i = 0; buffer[i]; ++i) {
		if (buffer[i] == '(' && !escape && !quote)
			comment++;
		if (buffer[i] == '"' && !escape && !comment)
			quote = !quote;
		if (buffer[i] == ')' && !escape && !quote && comment)
			comment--;
		if (buffer[i] == '\\' && !escape && !comment && !quote)
			escape = 1;
		else
			escape = 0;
		if (buffer[i] == '<' && !escape && !comment && !quote && !bracket) {
			bracket++;
			has_bracket = 1;
		}
		if (buffer[i] == '>' && !escape && !comment && !quote && bracket) {
			bracket--;
			pos_bracket = i;
		}
		if (buffer[i] == '@' && !escape && !comment && !quote)
			has_domain = 1;
		if (buffer[i] == ':' && !escape && !comment && !quote)
			has_group = 1;

		/* update insert point if not in comment and not on a whitespace */
		if (!comment && buffer[i] != ')' && !isspace((unsigned char)buffer[i]))
			pos_component = i;
	}

	/* parse error, do not attempt to modify */
	if (escape || quote || comment || bracket)
		return;

	/* domain already present, no need to modify */
	if (has_domain)
		return;

	/* address is group, skip */
	if (has_group)
		return;

	/* there's an address between brackets, just append domain */
	if (has_bracket) {
		pos_bracket--;
		while (isspace((unsigned char)buffer[pos_bracket]))
			pos_bracket--;
		if (buffer[pos_bracket] == '<')
			return;
		pos_insert = pos_bracket + 1;
	}
	else {
		/* otherwise append address to last component */
		pos_insert = pos_component + 1;

		/* empty address */
                if (buffer[pos_component] == '\0' ||
		    isspace((unsigned char)buffer[pos_component]))
                        return;
	}

	if (snprintf(copy, sizeof copy, "%.*s@%s%s",
		(int)pos_insert, buffer,
		domain,
		buffer+pos_insert) >= (int)sizeof copy)
		return;

	memcpy(buffer, copy, len);
}

static void
header_address_rewrite_buffer(char *buffer, const char *address, size_t len)
{
	size_t	i;
	int	address_len;
	int	escape, quote, comment, bracket;
	int	has_bracket, has_group;
	int	pos_bracket_beg, pos_bracket_end, pos_component_beg, pos_component_end;
	int	insert_beg, insert_end;
	char	copy[APPEND_DOMAIN_BUFFER_SIZE];

	escape = quote = comment = bracket = 0;
	has_bracket = has_group = 0;
	pos_bracket_beg = pos_bracket_end = pos_component_beg = pos_component_end = 0;
	for (i = 0; buffer[i]; ++i) {
		if (buffer[i] == '(' && !escape && !quote)
			comment++;
		if (buffer[i] == '"' && !escape && !comment)
			quote = !quote;
		if (buffer[i] == ')' && !escape && !quote && comment)
			comment--;
		if (buffer[i] == '\\' && !escape && !comment && !quote)
			escape = 1;
		else
			escape = 0;
		if (buffer[i] == '<' && !escape && !comment && !quote && !bracket) {
			bracket++;
			has_bracket = 1;
			pos_bracket_beg = i+1;
		}
		if (buffer[i] == '>' && !escape && !comment && !quote && bracket) {
			bracket--;
			pos_bracket_end = i;
		}
		if (buffer[i] == ':' && !escape && !comment && !quote)
			has_group = 1;

		/* update insert point if not in comment and not on a whitespace */
		if (!comment && buffer[i] != ')' && !isspace((unsigned char)buffer[i]))
			pos_component_end = i;
	}

	/* parse error, do not attempt to modify */
	if (escape || quote || comment || bracket)
		return;

	/* address is group, skip */
	if (has_group)
		return;

	/* there's an address between brackets, just replace everything brackets */
	if (has_bracket) {
		insert_beg = pos_bracket_beg;
		insert_end = pos_bracket_end;
	}
	else {
		if (pos_component_end == 0)
			pos_component_beg = 0;
		else {
			for (pos_component_beg = pos_component_end; pos_component_beg >= 0; --pos_component_beg)
				if (buffer[pos_component_beg] == ')' || isspace(buffer[pos_component_beg]))
					break;
			pos_component_beg += 1;
			pos_component_end += 1;
		}
		insert_beg = pos_component_beg;
		insert_end = pos_component_end;
	}

	/* check that masquerade won' t overflow */
	address_len = strlen(address);
	if (strlen(buffer) - (insert_end - insert_beg) + address_len >= len)
		return;

	(void)strlcpy(copy, buffer, sizeof copy);
	(void)strlcpy(copy+insert_beg, address, sizeof (copy) - insert_beg);
	(void)strlcat(copy, buffer+insert_end, sizeof (copy));
	memcpy(buffer, copy, len);
}

static void
header_domain_append_callback(const struct rfc2822_header *hdr, void *arg)
{
	struct smtp_session    *s;
	struct smtp_tx         *tx = arg;
	struct rfc2822_line    *l;
	size_t			i, j;
	int			escape, quote, comment, skip;
	char			buffer[APPEND_DOMAIN_BUFFER_SIZE];

	s = tx->session;

	if (smtp_message_printf(tx, "%s:", hdr->name) == -1)
		return;

	j = 0;
	escape = quote = comment = skip = 0;
	memset(buffer, 0, sizeof buffer);

	TAILQ_FOREACH(l, &hdr->lines, next) {
		for (i = 0; i < strlen(l->buffer); ++i) {
			if (l->buffer[i] == '(' && !escape && !quote)
				comment++;
			if (l->buffer[i] == '"' && !escape && !comment)
				quote = !quote;
			if (l->buffer[i] == ')' && !escape && !quote && comment)
				comment--;
			if (l->buffer[i] == '\\' && !escape && !comment && !quote)
				escape = 1;
			else
				escape = 0;

			/* found a separator, buffer contains a full address */
			if (l->buffer[i] == ',' && !escape && !quote && !comment) {
				if (!skip && j + strlen(s->listener->hostname) + 1 < sizeof buffer) {
					header_append_domain_buffer(buffer, s->listener->hostname, sizeof buffer);
					if (s->flags & SF_AUTHENTICATED &&
					    s->listener->sendertable[0] &&
					    s->listener->flags & F_MASQUERADE &&
					    !(strcasecmp(hdr->name, "From")))
						header_address_rewrite_buffer(buffer, mailaddr_to_text(&tx->evp.sender),
						    sizeof buffer);
				}
				if (smtp_message_printf(tx, "%s,", buffer) == -1)
					return;
				j = 0;
				skip = 0;
				memset(buffer, 0, sizeof buffer);
			}
			else {
				if (skip) {
					if (smtp_message_printf(tx, "%c", l->buffer[i]) == -1)
						return;
				}
				else {
					buffer[j++] = l->buffer[i];
					if (j == sizeof (buffer) - 1) {
						if (smtp_message_printf(tx, "%s", buffer) == -1)
							return;
						skip = 1;
						j = 0;
						memset(buffer, 0, sizeof buffer);
					}
				}
			}
		}
		if (skip) {
			if (smtp_message_printf(tx, "\n") == -1)
				return;
		}
		else {
			buffer[j++] = '\n';
			if (j == sizeof (buffer) - 1) {
				if (smtp_message_printf(tx, "%s", buffer) == -1)
					return;
				skip = 1;
				j = 0;
				memset(buffer, 0, sizeof buffer);
			}
		}
	}

	/* end of header, if buffer is not empty we'll process it */
	if (buffer[0]) {
		if (j + strlen(s->listener->hostname) + 1 < sizeof buffer) {
			header_append_domain_buffer(buffer, s->listener->hostname, sizeof buffer);
			if (s->flags & SF_AUTHENTICATED &&
			    s->listener->sendertable[0] &&
			    s->listener->flags & F_MASQUERADE &&
			    !(strcasecmp(hdr->name, "From")))
				header_address_rewrite_buffer(buffer, mailaddr_to_text(&tx->evp.sender),
				    sizeof buffer);
		}
		smtp_message_printf(tx, "%s", buffer);
	}
}

static void
header_missing_callback(const char *header, void *arg)
{
	struct smtp_tx *tx = arg;

	if (strcasecmp(header, "message-id") == 0)
		smtp_message_printf(tx, "Message-Id: <%016"PRIx64"@%s>\n",
		    generate_uid(), tx->session->listener->hostname);

	if (strcasecmp(header, "date") == 0)
		smtp_message_printf(tx, "Date: %s\n", time_to_text(tx->time));
}

static void
smtp_session_init(void)
{
	static int	init = 0;

	if (!init) {
		tree_init(&wait_lka_ptr);
		tree_init(&wait_lka_helo);
		tree_init(&wait_lka_mail);
		tree_init(&wait_lka_rcpt);
		tree_init(&wait_parent_auth);
		tree_init(&wait_queue_msg);
		tree_init(&wait_queue_fd);
		tree_init(&wait_queue_commit);
		tree_init(&wait_ssl_init);
		tree_init(&wait_ssl_verify);
		init = 1;
	}
}

int
smtp_session(struct listener *listener, int sock,
    const struct sockaddr_storage *ss, const char *hostname)
{
	struct smtp_session	*s;

	log_debug("debug: smtp: new client on listener: %p", listener);

	smtp_session_init();

	if ((s = calloc(1, sizeof(*s))) == NULL)
		return (-1);

	s->id = generate_uid();
	s->listener = listener;
	memmove(&s->ss, ss, sizeof(*ss));
	s->io = io_new();
	io_set_callback(s->io, smtp_io, s);
	io_set_fd(s->io, sock);
	io_set_timeout(s->io, SMTPD_SESSION_TIMEOUT * 1000);
	io_set_write(s->io);

	s->state = STATE_NEW;

	(void)strlcpy(s->smtpname, listener->hostname, sizeof(s->smtpname));

	log_trace(TRACE_SMTP, "smtp: %p: connected to listener %p "
	    "[hostname=%s, port=%d, tag=%s]", s, listener,
	    listener->hostname, ntohs(listener->port), listener->tag);

	/* For local enqueueing, the hostname is already set */
	if (hostname) {
		s->flags |= SF_AUTHENTICATED;
		/* A bit of a hack */
		if (!strcmp(hostname, "localhost"))
			s->flags |= SF_BOUNCE;
		(void)strlcpy(s->hostname, hostname, sizeof(s->hostname));
		if (smtp_lookup_servername(s))
			smtp_connected(s);
	} else {
		m_create(p_lka,  IMSG_SMTP_DNS_PTR, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_sockaddr(p_lka, (struct sockaddr *)&s->ss);
		m_close(p_lka);
		tree_xset(&wait_lka_ptr, s->id, s);
	}

	/* session may have been freed by now */

	return (0);
}

void
smtp_session_imsg(struct mproc *p, struct imsg *imsg)
{
	struct ca_cert_resp_msg       	*resp_ca_cert;
	struct ca_vrfy_resp_msg       	*resp_ca_vrfy;
	struct smtp_session		*s;
	struct smtp_rcpt		*rcpt;
	void				*ssl;
	char				 user[LOGIN_NAME_MAX];
	struct msg			 m;
	const char			*line, *helo;
	uint64_t			 reqid, evpid;
	uint32_t			 msgid;
	int				 status, success, dnserror;
	void				*ssl_ctx;

	switch (imsg->hdr.type) {
	case IMSG_SMTP_DNS_PTR:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &dnserror);
		if (dnserror)
			line = "<unknown>";
		else
			m_get_string(&m, &line);
		m_end(&m);
		s = tree_xpop(&wait_lka_ptr, reqid);
		(void)strlcpy(s->hostname, line, sizeof s->hostname);
		if (smtp_lookup_servername(s))
			smtp_connected(s);
		return;

	case IMSG_SMTP_CHECK_SENDER:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &status);
		m_end(&m);
		s = tree_xpop(&wait_lka_mail, reqid);
		switch (status) {
		case LKA_OK:
			smtp_tx_create_message(s->tx);
			break;

		case LKA_PERMFAIL:
			smtp_tx_free(s->tx);
			smtp_reply(s, "%d %s", 530, "Sender rejected");
			break;
		case LKA_TEMPFAIL:
			smtp_tx_free(s->tx);
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			break;
		}
		return;

	case IMSG_SMTP_EXPAND_RCPT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &status);
		m_get_string(&m, &line);
		m_end(&m);
		s = tree_xpop(&wait_lka_rcpt, reqid);
		switch (status) {
		case LKA_OK:
			fatalx("unexpected ok");
		case LKA_PERMFAIL:
			smtp_reply(s, "%s", line);
			break;
		case LKA_TEMPFAIL:
			smtp_reply(s, "%s", line);
		}
		return;

	case IMSG_SMTP_LOOKUP_HELO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		s = tree_xpop(&wait_lka_helo, reqid);
		m_get_int(&m, &status);
		if (status == LKA_OK) {
			m_get_string(&m, &helo);
			(void)strlcpy(s->smtpname, helo, sizeof(s->smtpname));
		}
		m_end(&m);
		smtp_connected(s);
		return;

	case IMSG_SMTP_MESSAGE_CREATE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		s = tree_xpop(&wait_queue_msg, reqid);
		if (success) {
			m_get_msgid(&m, &msgid);
			s->tx->msgid = msgid;
			s->tx->evp.id = msgid_to_evpid(msgid);
			s->tx->rcptcount = 0;
			smtp_reply(s, "250 %s: Ok",
			    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		} else {
			smtp_tx_free(s->tx);
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
		}
		m_end(&m);
		return;

	case IMSG_SMTP_MESSAGE_OPEN:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);

		s = tree_xpop(&wait_queue_fd, reqid);
		if (!success || imsg->fd == -1) {
			if (imsg->fd != -1)
				close(imsg->fd);
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			return;
		}

		log_debug("smtp: %p: fd %d from queue", s, imsg->fd);

		smtp_message_fd(s->tx, imsg->fd);
		return;

	case IMSG_QUEUE_ENVELOPE_SUBMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		s = tree_xget(&wait_lka_rcpt, reqid);
		if (success) {
			m_get_evpid(&m, &evpid);
			s->tx->destcount++;
		}
		else
			s->tx->error = TX_ERROR_ENVELOPE;
		m_end(&m);
		return;

	case IMSG_QUEUE_ENVELOPE_COMMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);
		if (!success)
			fatalx("commit evp failed: not supposed to happen");
		s = tree_xpop(&wait_lka_rcpt, reqid);
		if (s->tx->error) {
			/*
			 * If an envelope failed, we can't cancel the last
			 * RCPT only so we must cancel the whole transaction
			 * and close the connection.
			 */
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
		}
		else {
			rcpt = xcalloc(1, sizeof(*rcpt), "smtp_rcpt");
			rcpt->destcount = s->tx->destcount;
			rcpt->maddr = s->tx->evp.rcpt;
			TAILQ_INSERT_TAIL(&s->tx->rcpts, rcpt, entry);

			s->tx->destcount = 0;
			s->tx->rcptcount++;
			smtp_reply(s, "250 %s %s: Recipient ok",
			    esc_code(ESC_STATUS_OK, ESC_DESTINATION_ADDRESS_VALID),
			    esc_description(ESC_DESTINATION_ADDRESS_VALID));
		}
		return;

	case IMSG_SMTP_MESSAGE_COMMIT:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);
		s = tree_xpop(&wait_queue_commit, reqid);
		if (!success) {
			smtp_tx_free(s->tx);
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			return;
		}

		smtp_reply(s, "250 %s: %08x Message accepted for delivery",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS),
		    s->tx->msgid);

		TAILQ_FOREACH(rcpt, &s->tx->rcpts, entry) {
			log_info("%016"PRIx64" smtp event=message address=%s host=%s "
			    "msgid=%08x from=<%s%s%s> to=<%s%s%s> size=%zu ndest=%zu proto=%s",
			    s->id,
			    ss_to_text(&s->ss), s->hostname,
			    s->tx->msgid,
			    s->tx->evp.sender.user,
			    s->tx->evp.sender.user[0] == '\0' ? "" : "@",
			    s->tx->evp.sender.domain,
			    rcpt->maddr.user,
			    rcpt->maddr.user[0] == '\0' ? "" : "@",
			    rcpt->maddr.domain,
			    s->tx->odatalen,
			    rcpt->destcount,
			    s->flags & SF_EHLO ? "ESMTP" : "SMTP");
		}
		smtp_tx_free(s->tx);
		s->mailcount++;
		smtp_enter_state(s, STATE_HELO);
		return;

	case IMSG_SMTP_AUTHENTICATE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &success);
		m_end(&m);

		s = tree_xpop(&wait_parent_auth, reqid);
		strnvis(user, s->username, sizeof user, VIS_WHITE | VIS_SAFE);
		if (success == LKA_OK) {
			log_info("%016"PRIx64" smtp "
			    "event=authentication user=%s address=%s "
			    "host=%s result=ok",
			    s->id, user, ss_to_text(&s->ss), s->hostname);
			s->flags |= SF_AUTHENTICATED;
			smtp_reply(s, "235 %s: Authentication succeeded",
			    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		}
		else if (success == LKA_PERMFAIL) {
			log_info("%016"PRIx64" smtp "
			    "event=authentication user=%s address=%s "
			    "host=%s result=permfail",
			    s->id, user, ss_to_text(&s->ss), s->hostname);
			smtp_auth_failure_pause(s);
			return;
		}
		else if (success == LKA_TEMPFAIL) {
			log_info("%016"PRIx64" smtp "
			    "event=authentication user=%s address=%s "
			    "host=%s result=tempfail",
			    s->id, user, ss_to_text(&s->ss), s->hostname);
			smtp_reply(s, "421 %s: Temporary failure",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
		}
		else
			fatalx("bad lka response");

		smtp_enter_state(s, STATE_HELO);
		return;

	case IMSG_SMTP_TLS_INIT:
		resp_ca_cert = imsg->data;
		s = tree_xpop(&wait_ssl_init, resp_ca_cert->reqid);

		if (resp_ca_cert->status == CA_FAIL) {
			log_info("%016"PRIx64" smtp event=closed address=%s host=%s "
			    "reason=ca-failure",
			    s->id, ss_to_text(&s->ss), s->hostname);
			smtp_free(s, "CA failure");
			return;
		}

		resp_ca_cert = xmemdup(imsg->data, sizeof *resp_ca_cert, "smtp:ca_cert");
		resp_ca_cert->cert = xstrdup((char *)imsg->data +
		    sizeof *resp_ca_cert, "smtp:ca_cert");
		ssl_ctx = dict_get(env->sc_ssl_dict, resp_ca_cert->name);
		ssl = ssl_smtp_init(ssl_ctx, s->listener->flags & F_TLS_VERIFY);
		io_set_read(s->io);
		io_start_tls(s->io, ssl);

		freezero(resp_ca_cert->cert, resp_ca_cert->cert_len);
		free(resp_ca_cert);
		return;

	case IMSG_SMTP_TLS_VERIFY:
		resp_ca_vrfy = imsg->data;
		s = tree_xpop(&wait_ssl_verify, resp_ca_vrfy->reqid);

		if (resp_ca_vrfy->status == CA_OK)
			s->flags |= SF_VERIFIED;
		else if (s->listener->flags & F_TLS_VERIFY) {
			log_info("%016"PRIx64" smtp "
			    "event=closed address=%s host=%s reason=cert-check-failed",
			    s->id, ss_to_text(&s->ss), s->hostname);
			smtp_free(s, "SSL certificate check failed");
			return;
		}
		smtp_tls_verified(s);
		io_resume(s->io, IO_IN);
		return;
	}

	log_warnx("smtp_session_imsg: unexpected %s imsg",
	    imsg_to_str(imsg->hdr.type));
	fatalx(NULL);
}

static void
smtp_tls_verified(struct smtp_session *s)
{
	X509 *x;

	x = SSL_get_peer_certificate(io_ssl(s->io));
	if (x) {
		log_info("%016"PRIx64" smtp "
		    "event=client-cert-check address=%s host=%s result=\"%s\"",
		    s->id, ss_to_text(&s->ss), s->hostname,
		    (s->flags & SF_VERIFIED) ? "success" : "failure");
		X509_free(x);
	}

	if (s->listener->flags & F_SMTPS) {
		stat_increment("smtp.smtps", 1);
		io_set_write(s->io);
		smtp_send_banner(s);
	}
	else {
		stat_increment("smtp.tls", 1);
		smtp_enter_state(s, STATE_HELO);
	}
}

static void
smtp_io(struct io *io, int evt, void *arg)
{
	struct smtp_session    *s = arg;
	char		       *line;
	size_t			len;
	int			eom;

	log_trace(TRACE_IO, "smtp: %p: %s %s", s, io_strevent(evt),
	    io_strio(io));

	switch (evt) {

	case IO_TLSREADY:
		log_info("%016"PRIx64" smtp event=starttls address=%s host=%s ciphers=\"%s\"",
		    s->id, ss_to_text(&s->ss), s->hostname, ssl_to_text(io_ssl(s->io)));

		s->flags |= SF_SECURE;
		s->helo[0] = '\0';

		if (smtp_verify_certificate(s)) {
			io_pause(s->io, IO_IN);
			break;
		}

		if (s->listener->flags & F_TLS_VERIFY) {
			log_info("%016"PRIx64" smtp "
			    "event=closed address=%s host=%s reason=no-client-cert",
			    s->id, ss_to_text(&s->ss), s->hostname);
			smtp_free(s, "client did not present certificate");
			return;
		}

		smtp_tls_verified(s);
		break;

	case IO_DATAIN:
	    nextline:
		line = io_getline(s->io, &len);
		if ((line == NULL && io_datalen(s->io) >= SMTP_LINE_MAX) ||
		    (line && len >= SMTP_LINE_MAX)) {
			s->flags |= SF_BADINPUT;
			smtp_reply(s, "500 %s: Line too long",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_OTHER_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			io_set_write(io);
			return;
		}

		/* No complete line received */
		if (line == NULL)
			return;

		/* Message body */
		eom = 0;
		if (s->state == STATE_BODY) {
			eom = smtp_tx_dataline(s->tx, line);
			if (eom == 0)
				goto nextline;
		}

		/* Pipelining not supported */
		if (io_datalen(s->io)) {
			s->flags |= SF_BADINPUT;
			smtp_reply(s, "500 %s %s: Pipelining not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			smtp_enter_state(s, STATE_QUIT);
			io_set_write(io);
			return;
		}

		if (eom) {
			smtp_message_end(s->tx);
			io_set_write(io);
			return;
		}

		/* Must be a command */
		if (strlcpy(s->cmd, line, sizeof(s->cmd)) >= sizeof(s->cmd)) {
			s->flags |= SF_BADINPUT;
			smtp_reply(s, "500 %s: Command line too long",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_OTHER_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			io_set_write(io);
			return;
		}
		io_set_write(io);
		smtp_command(s, line);
		break;

	case IO_LOWAT:
		if (s->state == STATE_QUIT) {
			log_info("%016"PRIx64" smtp event=closed address=%s host=%s "
			    "reason=quit",
			    s->id, ss_to_text(&s->ss), s->hostname);
			smtp_free(s, "done");
			break;
		}

		/* Wait for the client to start tls */
		if (s->state == STATE_TLS) {
			smtp_tls_init(s);
			break;
		}

		io_set_read(io);
		break;

	case IO_TIMEOUT:
		log_info("%016"PRIx64" smtp event=closed address=%s host=%s "
		    "reason=timeout",
		    s->id, ss_to_text(&s->ss), s->hostname);
		smtp_free(s, "timeout");
		break;

	case IO_DISCONNECTED:
		log_info("%016"PRIx64" smtp event=closed address=%s host=%s "
		    "reason=disconnect",
		    s->id, ss_to_text(&s->ss), s->hostname);
		smtp_free(s, "disconnected");
		break;

	case IO_ERROR:
		log_info("%016"PRIx64" smtp event=closed address=%s host=%s "
		    "reason=\"io-error: %s\"",
		    s->id, ss_to_text(&s->ss), s->hostname, io_error(io));
		smtp_free(s, "IO error");
		break;

	default:
		fatalx("smtp_io()");
	}
}

static void
smtp_command(struct smtp_session *s, char *line)
{
	char			       *args, *eom, *method;
	int				cmd, i;

	log_trace(TRACE_SMTP, "smtp: %p: <<< %s", s, line);

	/*
	 * These states are special.
	 */
	if (s->state == STATE_AUTH_INIT) {
		smtp_rfc4954_auth_plain(s, line);
		return;
	}
	if (s->state == STATE_AUTH_USERNAME || s->state == STATE_AUTH_PASSWORD) {
		smtp_rfc4954_auth_login(s, line);
		return;
	}

	/*
	 * Unlike other commands, "mail from" and "rcpt to" contain a
	 * space in the command name.
	 */
	if (strncasecmp("mail from:", line, 10) == 0 ||
	    strncasecmp("rcpt to:", line, 8) == 0)
		args = strchr(line, ':');
	else
		args = strchr(line, ' ');

	if (args) {
		*args++ = '\0';
		while (isspace((unsigned char)*args))
			args++;
	}

	cmd = -1;
	for (i = 0; commands[i].code != -1; i++)
		if (!strcasecmp(line, commands[i].cmd)) {
			cmd = commands[i].code;
			break;
		}

	switch (cmd) {
	/*
	 * INIT
	 */
	case CMD_HELO:
	case CMD_EHLO:
		if (s->helo[0]) {
			smtp_reply(s, "503 %s %s: Already identified",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (args == NULL) {
			smtp_reply(s, "501 %s %s: %s requires domain name",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND),
			    (cmd == CMD_HELO) ? "HELO" : "EHLO");

			break;
		}

		if (!valid_domainpart(args)) {
			smtp_reply(s, "501 %s %s: Invalid domain name",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}
		(void)strlcpy(s->helo, args, sizeof(s->helo));
		s->flags &= SF_SECURE | SF_AUTHENTICATED | SF_VERIFIED;
		if (cmd == CMD_EHLO) {
			s->flags |= SF_EHLO;
			s->flags |= SF_8BITMIME;
		}

		smtp_enter_state(s, STATE_HELO);
		smtp_reply(s, "250%c%s Hello %s [%s], pleased to meet you",
		    (s->flags & SF_EHLO) ? '-' : ' ',
		    s->smtpname,
		    s->helo,
		    ss_to_text(&s->ss));

		if (s->flags & SF_EHLO) {
			smtp_reply(s, "250-8BITMIME");
			smtp_reply(s, "250-ENHANCEDSTATUSCODES");
			smtp_reply(s, "250-SIZE %zu", env->sc_maxsize);
			if (ADVERTISE_EXT_DSN(s))
				smtp_reply(s, "250-DSN");
			if (ADVERTISE_TLS(s))
				smtp_reply(s, "250-STARTTLS");
			if (ADVERTISE_AUTH(s))
				smtp_reply(s, "250-AUTH PLAIN LOGIN");
			smtp_reply(s, "250 HELP");
		}
		break;
	/*
	 * SETUP
	 */
	case CMD_STARTTLS:
		if (s->helo[0] == '\0' || s->tx) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (!(s->listener->flags & F_STARTTLS)) {
			smtp_reply(s, "503 %s %s: Command not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->flags & SF_SECURE) {
			smtp_reply(s, "503 %s %s: Channel already secured",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}
		if (args != NULL) {
			smtp_reply(s, "501 %s %s: No parameters allowed",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}
		smtp_reply(s, "220 %s: Ready to start TLS",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		smtp_enter_state(s, STATE_TLS);
		break;

	case CMD_AUTH:
		if (s->helo[0] == '\0' || s->tx) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->flags & SF_AUTHENTICATED) {
			smtp_reply(s, "503 %s %s: Already authenticated",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (!ADVERTISE_AUTH(s)) {
			smtp_reply(s, "503 %s %s: Command not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (args == NULL) {
			smtp_reply(s, "501 %s %s: No parameters given",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}

		method = args;
		eom = strchr(args, ' ');
		if (eom == NULL)
			eom = strchr(args, '\t');
		if (eom != NULL)
			*eom++ = '\0';
		if (strcasecmp(method, "PLAIN") == 0)
			smtp_rfc4954_auth_plain(s, eom);
		else if (strcasecmp(method, "LOGIN") == 0)
			smtp_rfc4954_auth_login(s, eom);
		else
			smtp_reply(s, "504 %s %s: AUTH method \"%s\" not supported",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_SECURITY_FEATURES_NOT_SUPPORTED),
			    esc_description(ESC_SECURITY_FEATURES_NOT_SUPPORTED),
			    method);
		break;

	case CMD_MAIL_FROM:
		if (s->helo[0] == '\0' || s->tx) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));

			break;
		}

		if (s->listener->flags & F_STARTTLS_REQUIRE &&
		    !(s->flags & SF_SECURE)) {
			smtp_reply(s,
			    "530 %s %s: Must issue a STARTTLS command first",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->listener->flags & F_AUTH_REQUIRE &&
		    !(s->flags & SF_AUTHENTICATED)) {
			smtp_reply(s,
			    "530 %s %s: Must issue an AUTH command first",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->mailcount >= env->sc_session_max_mails) {
			/* we can pretend we had too many recipients */
			smtp_reply(s, "452 %s %s: Too many messages sent",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_TOO_MANY_RECIPIENTS),
			    esc_description(ESC_TOO_MANY_RECIPIENTS));
			break;
		}

		if (!smtp_tx(s)) {
			smtp_reply(s, "421 %s: Temporary Error",
			    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
			smtp_enter_state(s, STATE_QUIT);
			break;
		}

		smtp_tx_mail_from(s->tx, args);
		break;
	/*
	 * TRANSACTION
	 */
	case CMD_RCPT_TO:
		if (s->tx == NULL) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		smtp_tx_rcpt_to(s->tx, args);
		break;

	case CMD_RSET:
		if (s->helo[0] == '\0') {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}

		if (s->tx) {
			if (s->tx->msgid)
				smtp_tx_rollback(s->tx);
			smtp_tx_free(s->tx);
		}

		smtp_reply(s, "250 %s: Reset state",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_DATA:
		if (s->tx == NULL) {
			smtp_reply(s, "503 %s %s: Command not allowed at this point.",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
			break;
		}
		if (s->tx->rcptcount == 0) {
			smtp_reply(s, "503 %s %s: No recipient specified",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS));
			break;
		}

		smtp_tx_open_message(s->tx);
		break;
	/*
	 * ANY
	 */
	case CMD_QUIT:
		smtp_reply(s, "221 %s: Bye",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		smtp_enter_state(s, STATE_QUIT);
		break;

	case CMD_NOOP:
		smtp_reply(s, "250 %s: Ok",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_HELP:
		smtp_reply(s, "214- This is " SMTPD_NAME);
		smtp_reply(s, "214- To report bugs in the implementation, "
		    "please contact bugs@openbsd.org");
		smtp_reply(s, "214- with full details");
		smtp_reply(s, "214 %s: End of HELP info",
		    esc_code(ESC_STATUS_OK, ESC_OTHER_STATUS));
		break;

	case CMD_WIZ:
		smtp_reply(s, "500 %s %s: this feature is not supported yet ;-)",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
		break;

	default:
		smtp_reply(s, "500 %s %s: Command unrecognized",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND),
			    esc_description(ESC_INVALID_COMMAND));
		break;
	}
}

static void
smtp_rfc4954_auth_plain(struct smtp_session *s, char *arg)
{
	char		 buf[1024], *user, *pass;
	int		 len;

	switch (s->state) {
	case STATE_HELO:
		if (arg == NULL) {
			smtp_enter_state(s, STATE_AUTH_INIT);
			smtp_reply(s, "334 ");
			return;
		}
		smtp_enter_state(s, STATE_AUTH_INIT);
		/* FALLTHROUGH */

	case STATE_AUTH_INIT:
		/* String is not NUL terminated, leave room. */
		if ((len = base64_decode(arg, (unsigned char *)buf,
			    sizeof(buf) - 1)) == -1)
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
		if (strlcpy(s->username, user, sizeof(s->username))
		    >= sizeof(s->username))
			goto abort;

		pass = memchr(user, '\0', len - (user - buf));
		if (pass == NULL || pass >= buf + len - 2)
			goto abort;
		pass++; /* skip NUL */

		m_create(p_lka,  IMSG_SMTP_AUTHENTICATE, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_string(p_lka, s->listener->authtable);
		m_add_string(p_lka, user);
		m_add_string(p_lka, pass);
		m_close(p_lka);
		tree_xset(&wait_parent_auth, s->id, s);
		return;

	default:
		fatal("smtp_rfc4954_auth_plain: unknown state");
	}

abort:
	smtp_reply(s, "501 %s %s: Syntax error",
	    esc_code(ESC_STATUS_PERMFAIL, ESC_SYNTAX_ERROR),
	    esc_description(ESC_SYNTAX_ERROR));
	smtp_enter_state(s, STATE_HELO);
}

static void
smtp_rfc4954_auth_login(struct smtp_session *s, char *arg)
{
	char		buf[LINE_MAX];

	switch (s->state) {
	case STATE_HELO:
		smtp_enter_state(s, STATE_AUTH_USERNAME);
		if (arg != NULL && *arg != '\0') {
			smtp_rfc4954_auth_login(s, arg);
			return;
		}
		smtp_reply(s, "334 VXNlcm5hbWU6");
		return;

	case STATE_AUTH_USERNAME:
		memset(s->username, 0, sizeof(s->username));
		if (base64_decode(arg, (unsigned char *)s->username,
				  sizeof(s->username) - 1) == -1)
			goto abort;

		smtp_enter_state(s, STATE_AUTH_PASSWORD);
		smtp_reply(s, "334 UGFzc3dvcmQ6");
		return;

	case STATE_AUTH_PASSWORD:
		memset(buf, 0, sizeof(buf));
		if (base64_decode(arg, (unsigned char *)buf,
				  sizeof(buf)-1) == -1)
			goto abort;

		m_create(p_lka,  IMSG_SMTP_AUTHENTICATE, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_string(p_lka, s->listener->authtable);
		m_add_string(p_lka, s->username);
		m_add_string(p_lka, buf);
		m_close(p_lka);
		tree_xset(&wait_parent_auth, s->id, s);
		return;

	default:
		fatal("smtp_rfc4954_auth_login: unknown state");
	}

abort:
	smtp_reply(s, "501 %s %s: Syntax error",
	    esc_code(ESC_STATUS_PERMFAIL, ESC_SYNTAX_ERROR),
	    esc_description(ESC_SYNTAX_ERROR));
	smtp_enter_state(s, STATE_HELO);
}

static int
smtp_parse_rcpt_args(struct smtp_session *s, char *args)
{
	char *b, *p;

	while ((b = strsep(&args, " "))) {
		if (*b == '\0')
			continue;

		if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "NOTIFY=", 7) == 0) {
			b += 7;
			while ((p = strsep(&b, ","))) {
				if (strcasecmp(p, "SUCCESS") == 0)
					s->tx->evp.dsn_notify |= DSN_SUCCESS;
				else if (strcasecmp(p, "FAILURE") == 0)
					s->tx->evp.dsn_notify |= DSN_FAILURE;
				else if (strcasecmp(p, "DELAY") == 0)
					s->tx->evp.dsn_notify |= DSN_DELAY;
				else if (strcasecmp(p, "NEVER") == 0)
					s->tx->evp.dsn_notify |= DSN_NEVER;
			}

			if (s->tx->evp.dsn_notify & DSN_NEVER &&
			    s->tx->evp.dsn_notify & (DSN_SUCCESS | DSN_FAILURE |
			    DSN_DELAY)) {
				smtp_reply(s,
				    "553 NOTIFY option NEVER cannot be \
				    combined with other options");
				return (-1);
			}
		} else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "ORCPT=", 6) == 0) {
			b += 6;
			if (!text_to_mailaddr(&s->tx->evp.dsn_orcpt, b)) {
				smtp_reply(s, "553 ORCPT address syntax error");
				return (-1);
			}
		} else {
			smtp_reply(s, "503 Unsupported option %s", b);
			return (-1);
		}
	}

	return (0);
}

static int
smtp_parse_mail_args(struct smtp_session *s, char *args)
{
	char *b;

	while ((b = strsep(&args, " "))) {
		if (*b == '\0')
			continue;

		if (strncasecmp(b, "AUTH=", 5) == 0)
			log_debug("debug: smtp: AUTH in MAIL FROM command");
		else if (strncasecmp(b, "SIZE=", 5) == 0)
			log_debug("debug: smtp: SIZE in MAIL FROM command");
		else if (strcasecmp(b, "BODY=7BIT") == 0)
			/* XXX only for this transaction */
			s->flags &= ~SF_8BITMIME;
		else if (strcasecmp(b, "BODY=8BITMIME") == 0)
			;
		else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "RET=", 4) == 0) {
			b += 4;
			if (strcasecmp(b, "HDRS") == 0)
				s->tx->evp.dsn_ret = DSN_RETHDRS;
			else if (strcasecmp(b, "FULL") == 0)
				s->tx->evp.dsn_ret = DSN_RETFULL;
		} else if (ADVERTISE_EXT_DSN(s) && strncasecmp(b, "ENVID=", 6) == 0) {
			b += 6;
			if (strlcpy(s->tx->evp.dsn_envid, b, sizeof(s->tx->evp.dsn_envid))
			    >= sizeof(s->tx->evp.dsn_envid)) {
				smtp_reply(s, "503 %s %s: option too large, truncated: %s",
				    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
				    esc_description(ESC_INVALID_COMMAND_ARGUMENTS), b);
				return (-1);
			}
		} else {
			smtp_reply(s, "503 %s %s: Unsupported option %s",
			    esc_code(ESC_STATUS_PERMFAIL, ESC_INVALID_COMMAND_ARGUMENTS),
			    esc_description(ESC_INVALID_COMMAND_ARGUMENTS), b);
			return (-1);
		}
	}

	return (0);
}

static int
smtp_lookup_servername(struct smtp_session *s)
{
	struct sockaddr		*sa;
	socklen_t		 sa_len;
	struct sockaddr_storage	 ss;

	if (s->listener->hostnametable[0]) {
		sa_len = sizeof(ss);
		sa = (struct sockaddr *)&ss;
		if (getsockname(io_fileno(s->io), sa, &sa_len) == -1) {
			log_warn("warn: getsockname()");
		}
		else {
			m_create(p_lka, IMSG_SMTP_LOOKUP_HELO, 0, 0, -1);
			m_add_id(p_lka, s->id);
			m_add_string(p_lka, s->listener->hostnametable);
			m_add_sockaddr(p_lka, sa);
			m_close(p_lka);
			tree_xset(&wait_lka_helo, s->id, s);
			return 0;
		}
	}
	return 1;
}

static void
smtp_connected(struct smtp_session *s)
{
	struct sockaddr_storage	ss;
	socklen_t		sl;

	smtp_enter_state(s, STATE_CONNECTED);

	log_info("%016"PRIx64" smtp event=connected address=%s host=%s",
	    s->id, ss_to_text(&s->ss), s->hostname);

	sl = sizeof(ss);
	if (getsockname(io_fileno(s->io), (struct sockaddr*)&ss, &sl) == -1) {
		smtp_free(s, strerror(errno));
		return;
	}

	if (s->listener->flags & F_SMTPS) {
		smtp_tls_init(s);
		return;
	}

	smtp_send_banner(s);
}

static void
smtp_send_banner(struct smtp_session *s)
{
	smtp_reply(s, "220 %s ESMTP %s", s->smtpname, SMTPD_NAME);
}

void
smtp_enter_state(struct smtp_session *s, int newstate)
{
	log_trace(TRACE_SMTP, "smtp: %p: %s -> %s", s,
	    smtp_strstate(s->state),
	    smtp_strstate(newstate));

	s->state = newstate;
}

static void
smtp_reply(struct smtp_session *s, char *fmt, ...)
{
	va_list	 ap;
	int	 n;
	char	 buf[LINE_MAX], tmp[LINE_MAX];

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n == -1 || n >= LINE_MAX)
		fatalx("smtp_reply: line too long");
	if (n < 4)
		fatalx("smtp_reply: response too short");

	log_trace(TRACE_SMTP, "smtp: %p: >>> %s", s, buf);

	io_xprintf(s->io, "%s\r\n", buf);

	switch (buf[0]) {
	case '5':
	case '4':
		if (s->flags & SF_BADINPUT) {
			log_info("%016"PRIx64" smtp "
			    "event=bad-input address=%s host=%s result=\"%.*s\"",
			    s->id, ss_to_text(&s->ss), s->hostname, n, buf);
		}
		else if (s->state == STATE_AUTH_INIT) {
			log_info("%016"PRIx64" smtp "
			    "event=failed-command address=%s host=%s "
			    "command=\"AUTH PLAIN (...)\" result=\"%.*s\"",
			    s->id, ss_to_text(&s->ss), s->hostname, n, buf);
		}
		else if (s->state == STATE_AUTH_USERNAME) {
			log_info("%016"PRIx64" smtp "
			    "event=failed-command address=%s host=%s "
			    "command=\"AUTH LOGIN (username)\" result=\"%.*s\"",
			    s->id, ss_to_text(&s->ss), s->hostname, n, buf);
		}
		else if (s->state == STATE_AUTH_PASSWORD) {
			log_info("%016"PRIx64" smtp "
			    "event=failed-command address=%s host=%s "
			    "command=\"AUTH LOGIN (password)\" result=\"%.*s\"",
			    s->id, ss_to_text(&s->ss), s->hostname, n, buf);
		}
		else {
			strnvis(tmp, s->cmd, sizeof tmp, VIS_SAFE | VIS_CSTYLE);
			log_info("%016"PRIx64" smtp "
			    "event=failed-command address=%s host=%s command=\"%s\" "
			    "result=\"%.*s\"",
			    s->id, ss_to_text(&s->ss), s->hostname, tmp, n, buf);
		}
		break;
	}
}

static void
smtp_free(struct smtp_session *s, const char * reason)
{
	log_debug("debug: smtp: %p: deleting session: %s", s, reason);

	if (s->tx) {
		if (s->tx->msgid)
			smtp_tx_rollback(s->tx);
		smtp_tx_free(s->tx);
	}

	if (s->flags & SF_SECURE && s->listener->flags & F_SMTPS)
		stat_decrement("smtp.smtps", 1);
	if (s->flags & SF_SECURE && s->listener->flags & F_STARTTLS)
		stat_decrement("smtp.tls", 1);

	io_free(s->io);
	free(s);

	smtp_collect();
}

static int
smtp_mailaddr(struct mailaddr *maddr, char *line, int mailfrom, char **args,
    const char *domain)
{
	char   *p, *e;

	if (line == NULL)
		return (0);

	if (*line != '<')
		return (0);

	e = strchr(line, '>');
	if (e == NULL)
		return (0);
	*e++ = '\0';
	while (*e == ' ')
		e++;
	*args = e;

	if (!text_to_mailaddr(maddr, line + 1))
		return (0);

	p = strchr(maddr->user, ':');
	if (p != NULL) {
		p++;
		memmove(maddr->user, p, strlen(p) + 1);
	}

	if (!valid_localpart(maddr->user) ||
	    !valid_domainpart(maddr->domain)) {
		/* accept empty return-path in MAIL FROM, required for bounces */
		if (mailfrom && maddr->user[0] == '\0' && maddr->domain[0] == '\0')
			return (1);

		/* no user-part, reject */
		if (maddr->user[0] == '\0')
			return (0);

		/* no domain, local user */
		if (maddr->domain[0] == '\0') {
			(void)strlcpy(maddr->domain, domain,
			    sizeof(maddr->domain));
			return (1);
		}
		return (0);
	}

	return (1);
}

static void
smtp_tls_init(struct smtp_session *s)
{
	struct ca_cert_req_msg req_ca_cert;

	req_ca_cert.reqid = s->id;
	if (s->listener->pki_name[0]) {
		(void)strlcpy(req_ca_cert.name, s->listener->pki_name,
		    sizeof req_ca_cert.name);
		req_ca_cert.fallback = 0;
	}
	else {
		(void)strlcpy(req_ca_cert.name, s->smtpname,
		    sizeof req_ca_cert.name);
		req_ca_cert.fallback = 1;
	}
	m_compose(p_lka, IMSG_SMTP_TLS_INIT, 0, 0, -1,
	    &req_ca_cert, sizeof(req_ca_cert));
	tree_xset(&wait_ssl_init, s->id, s);
}

static int
smtp_verify_certificate(struct smtp_session *s)
{
#define MAX_CERTS	16
#define MAX_CERT_LEN	(MAX_IMSGSIZE - (IMSG_HEADER_SIZE + sizeof(req_ca_vrfy)))
	struct ca_vrfy_req_msg	req_ca_vrfy;
	struct iovec		iov[2];
	X509		       *x;
	STACK_OF(X509)	       *xchain;
	const char	       *name;
	unsigned char	       *cert_der[MAX_CERTS];
	int			cert_len[MAX_CERTS];
	int			i, cert_count, res;

	res = 0;
	memset(cert_der, 0, sizeof(cert_der));
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);

	/* Send the client certificate */
	if (s->listener->ca_name[0]) {
		name = s->listener->ca_name;
		req_ca_vrfy.fallback = 0;
	}
	else {
		name = s->smtpname;
		req_ca_vrfy.fallback = 1;
	}

	if (strlcpy(req_ca_vrfy.name, name, sizeof req_ca_vrfy.name)
	    >= sizeof req_ca_vrfy.name)
		return 0;

	x = SSL_get_peer_certificate(io_ssl(s->io));
	if (x == NULL)
		return 0;
	xchain = SSL_get_peer_cert_chain(io_ssl(s->io));

	/*
	 * Client provided a certificate and possibly a certificate chain.
	 * SMTP can't verify because it does not have the information that
	 * it needs, instead it will pass the certificate and chain to the
	 * lookup process and wait for a reply.
	 *
	 */

	cert_len[0] = i2d_X509(x, &cert_der[0]);
	X509_free(x);

	if (cert_len[0] < 0) {
		log_warnx("warn: failed to encode certificate");
		goto end;
	}
	log_debug("debug: certificate 0: len=%d", cert_len[0]);
	if (cert_len[0] > (int)MAX_CERT_LEN) {
		log_warnx("warn: certificate too long");
		goto end;
	}

	if (xchain) {
		cert_count = sk_X509_num(xchain);
		log_debug("debug: certificate chain len: %d", cert_count);
		if (cert_count >= MAX_CERTS) {
			log_warnx("warn: certificate chain too long");
			goto end;
		}
	}
	else
		cert_count = 0;

	for (i = 0; i < cert_count; ++i) {
		x = sk_X509_value(xchain, i);
		cert_len[i+1] = i2d_X509(x, &cert_der[i+1]);
		if (cert_len[i+1] < 0) {
			log_warnx("warn: failed to encode certificate");
			goto end;
		}
		log_debug("debug: certificate %i: len=%d", i+1, cert_len[i+1]);
		if (cert_len[i+1] > (int)MAX_CERT_LEN) {
			log_warnx("warn: certificate too long");
			goto end;
		}
	}

	tree_xset(&wait_ssl_verify, s->id, s);

	/* Send the client certificate */
	req_ca_vrfy.reqid = s->id;
	req_ca_vrfy.cert_len = cert_len[0];
	req_ca_vrfy.n_chain = cert_count;
	iov[0].iov_base = &req_ca_vrfy;
	iov[0].iov_len = sizeof(req_ca_vrfy);
	iov[1].iov_base = cert_der[0];
	iov[1].iov_len = cert_len[0];
	m_composev(p_lka, IMSG_SMTP_TLS_VERIFY_CERT, 0, 0, -1,
	    iov, nitems(iov));

	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	req_ca_vrfy.reqid = s->id;

	/* Send the chain, one cert at a time */
	for (i = 0; i < cert_count; ++i) {
		req_ca_vrfy.cert_len = cert_len[i+1];
		iov[1].iov_base = cert_der[i+1];
		iov[1].iov_len  = cert_len[i+1];
		m_composev(p_lka, IMSG_SMTP_TLS_VERIFY_CHAIN, 0, 0, -1,
		    iov, nitems(iov));
	}

	/* Tell lookup process that it can start verifying, we're done */
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	req_ca_vrfy.reqid = s->id;
	m_compose(p_lka, IMSG_SMTP_TLS_VERIFY, 0, 0, -1,
	    &req_ca_vrfy, sizeof req_ca_vrfy);

	res = 1;

    end:
	for (i = 0; i < MAX_CERTS; ++i)
		free(cert_der[i]);

	return res;
}

static void
smtp_auth_failure_resume(int fd, short event, void *p)
{
	struct smtp_session *s = p;

	smtp_reply(s, "535 Authentication failed");
	smtp_enter_state(s, STATE_HELO);
}

static void
smtp_auth_failure_pause(struct smtp_session *s)
{
	struct timeval	tv;

	tv.tv_sec = 0;
	tv.tv_usec = arc4random_uniform(1000000);
	log_trace(TRACE_SMTP, "smtp: timing-attack protection triggered, "
	    "will defer answer for %lu microseconds", tv.tv_usec);
	evtimer_set(&s->pause, smtp_auth_failure_resume, s);
	evtimer_add(&s->pause, &tv);
}

static int
smtp_tx(struct smtp_session *s)
{
	struct smtp_tx *tx;

	tx = calloc(1, sizeof(*tx));
	if (tx == NULL)
		return 0;

	TAILQ_INIT(&tx->rcpts);

	s->tx = tx;
	tx->session = s;

	/* setup the envelope */
	tx->evp.ss = s->ss;
	(void)strlcpy(tx->evp.tag, s->listener->tag, sizeof(tx->evp.tag));
	(void)strlcpy(tx->evp.smtpname, s->smtpname, sizeof(tx->evp.smtpname));
	(void)strlcpy(tx->evp.hostname, s->hostname, sizeof tx->evp.hostname);
	(void)strlcpy(tx->evp.helo, s->helo, sizeof(tx->evp.helo));

	if (s->flags & SF_BOUNCE)
		tx->evp.flags |= EF_BOUNCE;
	if (s->flags & SF_AUTHENTICATED)
		tx->evp.flags |= EF_AUTHENTICATED;

	/* Setup parser and callbacks */
	rfc2822_parser_init(&tx->rfc2822_parser);
	rfc2822_header_default_callback(&tx->rfc2822_parser,
	    header_default_callback, tx);
	rfc2822_header_callback(&tx->rfc2822_parser, "bcc",
	    header_bcc_callback, tx);
	rfc2822_header_callback(&tx->rfc2822_parser, "from",
	    header_domain_append_callback, tx);
	rfc2822_header_callback(&tx->rfc2822_parser, "to",
	    header_domain_append_callback, tx);
	rfc2822_header_callback(&tx->rfc2822_parser, "cc",
	    header_domain_append_callback, tx);
	rfc2822_body_callback(&tx->rfc2822_parser,
	    dataline_callback, tx);

	if (s->listener->local || s->listener->port == htons(587)) {
		rfc2822_missing_header_callback(&tx->rfc2822_parser, "date",
		    header_missing_callback, tx);
		rfc2822_missing_header_callback(&tx->rfc2822_parser, "message-id",
		    header_missing_callback, tx);
	}

	return 1;
}

static void
smtp_tx_free(struct smtp_tx *tx)
{
	struct smtp_rcpt *rcpt;

	rfc2822_parser_release(&tx->rfc2822_parser);

	while ((rcpt = TAILQ_FIRST(&tx->rcpts))) {
		TAILQ_REMOVE(&tx->rcpts, rcpt, entry);
		free(rcpt);
	}

	if (tx->ofile)
		fclose(tx->ofile);

	tx->session->tx = NULL;

	free(tx);
}

static void
smtp_tx_mail_from(struct smtp_tx *tx, char *line)
{
	if (smtp_mailaddr(&tx->evp.sender, line, 1, &line,
		tx->session->smtpname) == 0) {
		smtp_reply(tx->session, "553 %s: Sender address syntax error",
		    esc_code(ESC_STATUS_PERMFAIL, ESC_OTHER_ADDRESS_STATUS));
		smtp_tx_free(tx);
		return;
	}

	if (line && smtp_parse_mail_args(tx->session, line) == -1) {
		smtp_tx_free(tx);
		return;
	}

	/* only check sendertable if defined and user has authenticated */
	if (tx->session->flags & SF_AUTHENTICATED &&
	    tx->session->listener->sendertable[0]) {
		m_create(p_lka, IMSG_SMTP_CHECK_SENDER, 0, 0, -1);
		m_add_id(p_lka, tx->session->id);
		m_add_string(p_lka, tx->session->listener->sendertable);
		m_add_string(p_lka, tx->session->username);
		m_add_mailaddr(p_lka, &tx->evp.sender);
		m_close(p_lka);
		tree_xset(&wait_lka_mail, tx->session->id, tx->session);
	}
	else
		smtp_tx_create_message(tx);
}

static void
smtp_tx_create_message(struct smtp_tx *tx)
{
	m_create(p_queue, IMSG_SMTP_MESSAGE_CREATE, 0, 0, -1);
	m_add_id(p_queue, tx->session->id);
	m_close(p_queue);
	tree_xset(&wait_queue_msg, tx->session->id, tx->session);
}

static void
smtp_tx_rcpt_to(struct smtp_tx *tx, char *line)
{
	if (tx->rcptcount >= env->sc_session_max_rcpt) {
		smtp_reply(tx->session, "451 %s %s: Too many recipients",
		    esc_code(ESC_STATUS_TEMPFAIL, ESC_TOO_MANY_RECIPIENTS),
		    esc_description(ESC_TOO_MANY_RECIPIENTS));
		return;
	}

	if (smtp_mailaddr(&tx->evp.rcpt, line, 0, &line,
	    tx->session->smtpname) == 0) {
		smtp_reply(tx->session,
		    "501 %s: Recipient address syntax error",
		    esc_code(ESC_STATUS_PERMFAIL,
		        ESC_BAD_DESTINATION_MAILBOX_ADDRESS_SYNTAX));
		return;
	}

	if (line && smtp_parse_rcpt_args(tx->session, line) == -1)
		return;

	m_create(p_lka, IMSG_SMTP_EXPAND_RCPT, 0, 0, -1);
	m_add_id(p_lka, tx->session->id);
	m_add_envelope(p_lka, &tx->evp);
	m_close(p_lka);
	tree_xset(&wait_lka_rcpt, tx->session->id, tx->session);
}

static void
smtp_tx_open_message(struct smtp_tx *tx)
{
	m_create(p_queue, IMSG_SMTP_MESSAGE_OPEN, 0, 0, -1);
	m_add_id(p_queue, tx->session->id);
	m_add_msgid(p_queue, tx->msgid);
	m_close(p_queue);
	tree_xset(&wait_queue_fd, tx->session->id, tx->session);
}

static void
smtp_tx_commit(struct smtp_tx *tx)
{
	m_create(p_queue, IMSG_SMTP_MESSAGE_COMMIT, 0, 0, -1);
	m_add_id(p_queue, tx->session->id);
	m_add_msgid(p_queue, tx->msgid);
	m_close(p_queue);
	tree_xset(&wait_queue_commit, tx->session->id, tx->session);
}

static void
smtp_tx_rollback(struct smtp_tx *tx)
{
	m_create(p_queue, IMSG_SMTP_MESSAGE_ROLLBACK, 0, 0, -1);
	m_add_msgid(p_queue, tx->msgid);
	m_close(p_queue);
}

static int
smtp_tx_dataline(struct smtp_tx *tx, const char *line)
{
	int	ret;

	log_trace(TRACE_SMTP, "<<< [MSG] %s", line);

	if (!strcmp(line, ".")) {
		log_trace(TRACE_SMTP, "<<< [EOM]");
		if (!tx->error)
			rfc2822_parser_flush(&tx->rfc2822_parser);
		return 1;
	}

	/* ignore data line if an error is set */
	if (tx->error)
		return 0;

	/* escape lines starting with a '.' */
	if (line[0] == '.')
		line += 1;

	/* account for newline */
	tx->datain += strlen(line) + 1;
	if (tx->datain > env->sc_maxsize) {
		tx->error = TX_ERROR_SIZE;
		return 0;
	}

	if (!tx->hdrdone) {

		/* folded header that must be skipped */
		if (isspace((unsigned char)line[0]) && tx->skiphdr)
			return 0;
		tx->skiphdr = 0;

		/* BCC should be stripped from headers */
		if (strncasecmp("bcc:", line, 4) == 0) {
			tx->skiphdr = 1;
			return 0;
		}

		/* check for loop */
		if (strncasecmp("Received: ", line, 10) == 0)
			tx->rcvcount++;
		if (tx->rcvcount == MAX_HOPS_COUNT) {
			tx->error = TX_ERROR_LOOP;
			log_warnx("warn: loop detected");
			return 0;
		}

		if (line[0] == '\0')
			tx->hdrdone = 1;
	}

	ret = rfc2822_parser_feed(&tx->rfc2822_parser, line);
	if (ret == -1)
		tx->error = TX_ERROR_RESOURCES;

	if (ret == 0)
		tx->error = TX_ERROR_MALFORMED;

	return 0;
}

static void
smtp_message_fd(struct smtp_tx *tx, int fd)
{
	struct smtp_session *s;
	X509 *x;

	s = tx->session;

	log_debug("smtp: %p: message fd %d", s, fd);

	if ((tx->ofile = fdopen(fd, "w")) == NULL) {
		close(fd);
		smtp_reply(s, "421 %s: Temporary Error",
		    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
		smtp_enter_state(s, STATE_QUIT);
		return;
	}

	smtp_message_printf(tx, "Received: ");
	if (!(s->listener->flags & F_MASK_SOURCE)) {
		smtp_message_printf(tx, "from %s (%s [%s])",
		    s->helo,
		    s->hostname,
		    ss_to_text(&s->ss));
	}
	smtp_message_printf(tx, "\n\tby %s (%s) with %sSMTP%s%s id %08x",
	    s->smtpname,
	    SMTPD_NAME,
	    s->flags & SF_EHLO ? "E" : "",
	    s->flags & SF_SECURE ? "S" : "",
	    s->flags & SF_AUTHENTICATED ? "A" : "",
	    tx->msgid);

	if (s->flags & SF_SECURE) {
		x = SSL_get_peer_certificate(io_ssl(s->io));
		smtp_message_printf(tx, " (%s:%s:%d:%s)",
		    SSL_get_version(io_ssl(s->io)),
		    SSL_get_cipher_name(io_ssl(s->io)),
		    SSL_get_cipher_bits(io_ssl(s->io), NULL),
		    (s->flags & SF_VERIFIED) ? "YES" : (x ? "FAIL" : "NO"));
		X509_free(x);

		if (s->listener->flags & F_RECEIVEDAUTH) {
			smtp_message_printf(tx, " auth=%s",
			    s->username[0] ? "yes" : "no");
			if (s->username[0])
				smtp_message_printf(tx, " user=%s", s->username);
		}
	}

	if (tx->rcptcount == 1) {
		smtp_message_printf(tx, "\n\tfor <%s@%s>",
		    tx->evp.rcpt.user,
		    tx->evp.rcpt.domain);
	}

	smtp_message_printf(tx, ";\n\t%s\n", time_to_text(time(&tx->time)));

	smtp_enter_state(s, STATE_BODY);
	smtp_reply(s, "354 Enter mail, end with \".\""
	    " on a line by itself");
}

static void
smtp_message_end(struct smtp_tx *tx)
{
	struct smtp_session *s;

	s = tx->session;

	log_debug("debug: %p: end of message, error=%d", s, tx->error);

	fclose(tx->ofile);
	tx->ofile = NULL;

	switch(tx->error) {
	case TX_OK:
		smtp_tx_commit(tx);
		return;		

	case TX_ERROR_SIZE:
		smtp_reply(s, "554 %s %s: Transaction failed, message too big",
		    esc_code(ESC_STATUS_PERMFAIL, ESC_MESSAGE_TOO_BIG_FOR_SYSTEM),
		    esc_description(ESC_MESSAGE_TOO_BIG_FOR_SYSTEM));
		break;

	case TX_ERROR_LOOP:
		smtp_reply(s, "500 %s %s: Loop detected",
		   esc_code(ESC_STATUS_PERMFAIL, ESC_ROUTING_LOOP_DETECTED),
		   esc_description(ESC_ROUTING_LOOP_DETECTED));
		break;

	case TX_ERROR_MALFORMED:
		smtp_reply(s, "550 %s %s: Message is not RFC 2822 compliant",
		    esc_code(ESC_STATUS_PERMFAIL, ESC_DELIVERY_NOT_AUTHORIZED_MESSAGE_REFUSED),
		    esc_description(ESC_DELIVERY_NOT_AUTHORIZED_MESSAGE_REFUSED));
		break;

	case TX_ERROR_IO:
	case TX_ERROR_RESOURCES:
		smtp_reply(s, "421 %s: Temporary Error",
		    esc_code(ESC_STATUS_TEMPFAIL, ESC_OTHER_MAIL_SYSTEM_STATUS));
		break;

	default:
		/* fatal? */
		smtp_reply(s, "421 Internal server error");
	}

	smtp_tx_rollback(tx);
	smtp_tx_free(tx);
	smtp_enter_state(s, STATE_HELO);
}

static int
smtp_message_printf(struct smtp_tx *tx, const char *fmt, ...)
{
	va_list	ap;
	int	len;

	if (tx->error)
		return -1;

	va_start(ap, fmt);
	len = vfprintf(tx->ofile, fmt, ap);
	va_end(ap);

	if (len < 0) {
		log_warn("smtp-in: session %016"PRIx64": vfprintf", tx->session->id);
		tx->error = TX_ERROR_IO;
	}
	else
		tx->odatalen += len;

	return len;
}

#define CASE(x) case x : return #x

const char *
smtp_strstate(int state)
{
	static char	buf[32];

	switch (state) {
	CASE(STATE_NEW);
	CASE(STATE_CONNECTED);
	CASE(STATE_TLS);
	CASE(STATE_HELO);
	CASE(STATE_AUTH_INIT);
	CASE(STATE_AUTH_USERNAME);
	CASE(STATE_AUTH_PASSWORD);
	CASE(STATE_AUTH_FINALIZE);
	CASE(STATE_BODY);
	CASE(STATE_QUIT);
	default:
		(void)snprintf(buf, sizeof(buf), "STATE_??? (%d)", state);
		return (buf);
	}
}
