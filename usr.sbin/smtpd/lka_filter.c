/*	$OpenBSD: lka_filter.c,v 1.13 2018/12/09 21:43:46 gilles Exp $	*/

/*
 * Copyright (c) 2018 Gilles Chehade <gilles@poolp.org>
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

#include <netinet/in.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

static void	filter_proceed(uint64_t);
static void	filter_rewrite(uint64_t, const char *);
static void	filter_reject(uint64_t, const char *);
static void	filter_disconnect(uint64_t, const char *);

static void	filter_data(uint64_t reqid, const char *line);

static void	filter_write(const char *, uint64_t, const char *, const char *);
static void	filter_write_dataline(const char *, uint64_t, const char *);

static int	filter_exec_notimpl(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_connected(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_helo(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_mail_from(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_rcpt_to(uint64_t, struct filter_rule *, const char *);

static void	filter_session_io(struct io *, int, void *);
int		lka_filter_process_response(const char *, const char *);
static void	filter_data_next(uint64_t, const char *, const char *);

#define	PROTOCOL_VERSION	1

static struct filter_exec {
	enum filter_phase	phase;
	const char	       *phase_name;
	int		       (*func)(uint64_t, struct filter_rule *, const char *);
} filter_execs[] = {
	{ FILTER_AUTH,     	"auth",		filter_exec_notimpl },
	{ FILTER_CONNECTED,	"connected",	filter_exec_connected },
	{ FILTER_DATA,    	"data",		filter_exec_notimpl },
	{ FILTER_EHLO,		"ehlo",		filter_exec_helo },
	{ FILTER_HELO,		"helo",		filter_exec_helo },
	{ FILTER_STARTTLS,     	"starttls",	filter_exec_notimpl },
	{ FILTER_MAIL_FROM,    	"mail-from",	filter_exec_mail_from },
	{ FILTER_NOOP,    	"noop",		filter_exec_notimpl },
	{ FILTER_QUIT,    	"quit",		filter_exec_notimpl },
	{ FILTER_RCPT_TO,    	"rcpt-to",	filter_exec_rcpt_to },
	{ FILTER_RSET,    	"rset",		filter_exec_notimpl },
	{ FILTER_COMMIT,    	"commit",      	filter_exec_notimpl },
};

static struct tree	sessions;
static int		inited;

struct filter_session {
	uint64_t	id;
	struct io	*io;

	struct sockaddr_storage ss_src;
	struct sockaddr_storage ss_dest;
	char *rdns;
	int fcrdns;
};

void
lka_filter_begin(uint64_t reqid,
    const struct sockaddr_storage *ss_src,
    const struct sockaddr_storage *ss_dest,
    const char *rdns,
    int fcrdns)
{
	struct filter_session	*fs;

	if (!inited) {
		tree_init(&sessions);
		inited = 1;
	}

	fs = xcalloc(1, sizeof (struct filter_session));
	fs->id = reqid;
	fs->ss_src = *ss_src;
	fs->ss_dest = *ss_dest;
	fs->rdns = xstrdup(rdns);
	fs->fcrdns = fcrdns;
	tree_xset(&sessions, fs->id, fs);
}

void
lka_filter_end(uint64_t reqid)
{
	struct filter_session	*fs;

	fs = tree_xpop(&sessions, reqid);
	free(fs->rdns);
	free(fs);
}

void
lka_filter_data_begin(uint64_t reqid)
{
	struct filter_session  *fs;
	int	sp[2];
	int	fd = -1;

	fs = tree_xget(&sessions, reqid);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) == -1)
		goto end;

	fd = sp[0];
	fs->io = io_new();
	io_set_fd(fs->io, sp[1]);
	io_set_callback(fs->io, filter_session_io, fs);

end:
	m_create(p_pony, IMSG_SMTP_FILTER_DATA_BEGIN, 0, 0, fd);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, fd != -1 ? 1 : 0);
	m_close(p_pony);
}

void
lka_filter_data_end(uint64_t reqid)
{
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	io_free(fs->io);
	fs->io = NULL;
}

static void
filter_session_io(struct io *io, int evt, void *arg)
{
	struct filter_session *fs = arg;
	char *line = NULL;
	ssize_t len;

	log_trace(TRACE_IO, "filter session: %p: %s %s", fs, io_strevent(evt),
	    io_strio(io));

	switch (evt) {
	case IO_DATAIN:
	nextline:
		line = io_getline(fs->io, &len);
		/* No complete line received */
		if (line == NULL)
			return;

		filter_data(fs->id, line);

		goto nextline;
	}
}

int
lka_filter_process_response(const char *name, const char *line)
{
	uint64_t reqid;
	char buffer[LINE_MAX];
	char *ep = NULL;
	char *kind = NULL;
	char *qid = NULL;
	char *response = NULL;
	char *parameter = NULL;

	(void)strlcpy(buffer, line, sizeof buffer);
	if ((ep = strchr(buffer, '|')) == NULL)
		return 0;
	*ep = 0;

	kind = buffer;
	if (strcmp(kind, "filter-result") != 0 &&
	    strcmp(kind, "filter-dataline") != 0)
		return 1;

	qid = ep+1;
	if ((ep = strchr(qid, '|')) == NULL)
		return 0;
	*ep = 0;

	reqid = strtoull(qid, &ep, 16);
	if (qid[0] == '\0' || *ep != '\0')
		return 0;
	if (errno == ERANGE && reqid == ULONG_MAX)
		return 0;

	response = ep+1;
	if ((ep = strchr(response, '|'))) {
		parameter = ep + 1;
		*ep = 0;
	}

	if (strcmp(kind, "filter-dataline") == 0) {
		filter_data_next(reqid, name, response);
		return 1;
	}

	if (strcmp(response, "proceed") != 0 &&
	    strcmp(response, "reject") != 0 &&
	    strcmp(response, "disconnect") != 0 &&
	    strcmp(response, "rewrite") != 0)
		return 0;

	if (strcmp(response, "proceed") == 0 &&
	    parameter)
		return 0;

	if (strcmp(response, "proceed") != 0 &&
	    parameter == NULL)
		return 0;

	return lka_filter_response(reqid, response, parameter);
}

void
lka_filter_protocol(uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_rule	*rule;
	uint8_t			i;

	for (i = 0; i < nitems(filter_execs); ++i)
		if (phase == filter_execs[i].phase)
			break;
	if (i == nitems(filter_execs))
		goto proceed;

	TAILQ_FOREACH(rule, &env->sc_filter_rules[phase], entry) {
		if (rule->proc) {
			filter_write(rule->proc, reqid,
			    filter_execs[i].phase_name, param);
			return; /* deferred */
		}

		if (filter_execs[i].func(reqid, rule, param)) {
			if (rule->rewrite)
				filter_rewrite(reqid, rule->rewrite);
			else if (rule->disconnect)
				filter_disconnect(reqid, rule->disconnect);
			else
				filter_reject(reqid, rule->reject);
			return;
		}
	}

proceed:
	filter_proceed(reqid);
}

static void
filter_data(uint64_t reqid, const char *line)
{
	struct filter_session *fs;
	struct filter_rule *rule;

	fs = tree_xget(&sessions, reqid);

	rule = TAILQ_FIRST(&env->sc_filter_rules[FILTER_DATA_LINE]);
	filter_write_dataline(rule->proc, reqid, line);
}

static void
filter_data_next(uint64_t reqid, const char *name, const char *line)
{
	struct filter_session *fs;
	struct filter_rule *rule;

	fs = tree_xget(&sessions, reqid);

	TAILQ_FOREACH(rule, &env->sc_filter_rules[FILTER_DATA_LINE], entry) {
		if (strcmp(rule->proc, name) == 0)
			break;
	}

	if ((rule = TAILQ_NEXT(rule, entry)) == NULL)
		io_printf(fs->io, "%s\r\n", line);
	else
		filter_write_dataline(rule->proc, reqid, line);
}


int
lka_filter_response(uint64_t reqid, const char *response, const char *param)
{
	if (strcmp(response, "proceed") == 0)
		filter_proceed(reqid);
	else if (strcmp(response, "rewrite") == 0)
		filter_rewrite(reqid, param);
	else if (strcmp(response, "reject") == 0)
		filter_reject(reqid, param);
	else if (strcmp(response, "disconnect") == 0)
		filter_disconnect(reqid, param);
	else
		return 0;
	return 1;
}

static void
filter_write(const char *name, uint64_t reqid, const char *phase, const char *param)
{
	int	n;
	time_t	tm;
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	time(&tm);
	if (strcmp(phase, "connected") == 0)
		n = io_printf(lka_proc_get_io(name),
		    "filter|%d|%zd|smtp-in|%s|%016"PRIx64"|%s|%s\n",
		    PROTOCOL_VERSION,
		    tm,
		    phase, reqid, fs->rdns, param);
	else
		n = io_printf(lka_proc_get_io(name),
		    "filter|%d|%zd|smtp-in|%s|%016"PRIx64"|%s\n",
		    PROTOCOL_VERSION,
		    tm,
		    phase, reqid, param);
	if (n == -1)
		fatalx("failed to write to processor");
}

static void
filter_write_dataline(const char *name, uint64_t reqid, const char *line)
{
	int	n;
	time_t	tm;

	time(&tm);
	n = io_printf(lka_proc_get_io(name),
	    "filter|%d|%zd|smtp-in|data-line|"
	    "%016"PRIx64"|%s\n",
	    PROTOCOL_VERSION,
	    tm, reqid, line);
	if (n == -1)
		fatalx("failed to write to processor");
}

static void
filter_proceed(uint64_t reqid)
{
	m_create(p_pony, IMSG_SMTP_FILTER_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_PROCEED);
	m_close(p_pony);
}

static void
filter_rewrite(uint64_t reqid, const char *param)
{
	m_create(p_pony, IMSG_SMTP_FILTER_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_REWRITE);
	m_add_string(p_pony, param);
	m_close(p_pony);
}

static void
filter_reject(uint64_t reqid, const char *message)
{
	m_create(p_pony, IMSG_SMTP_FILTER_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_REJECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}

static void
filter_disconnect(uint64_t reqid, const char *message)
{
	m_create(p_pony, IMSG_SMTP_FILTER_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_DISCONNECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}


/* below is code for builtin filters */

static int
filter_check_table(struct filter_rule *rule, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (rule->table) {
		if (table_lookup(rule->table, NULL, key, kind, NULL) > 0)
			ret = 1;
		ret = rule->not_table < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_regex(struct filter_rule *rule, const char *key)
{
	int	ret = 0;

	if (rule->regex) {
		if (table_lookup(rule->regex, NULL, key, K_REGEX, NULL) > 0)
			ret = 1;
		ret = rule->not_regex < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_fcrdns(struct filter_rule *rule, int fcrdns)
{
	int	ret = 0;

	if (rule->fcrdns) {
		ret = fcrdns == 0;
		ret = rule->not_fcrdns < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_rdns(struct filter_rule *rule, const char *hostname)
{
	int	ret = 0;
	struct netaddr	netaddr;

	if (rule->rdns) {
		/* if text_to_netaddress succeeds,
		 * we don't have an rDNS so the filter should match
		 */
		ret = text_to_netaddr(&netaddr, hostname);
		ret = rule->not_rdns < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_exec_notimpl(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	return 0;
}

static int
filter_exec_connected(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	if (filter_check_table(rule, K_NETADDR, param) ||
	    filter_check_regex(rule, param) ||
	    filter_check_rdns(rule, fs->rdns) ||
	    filter_check_fcrdns(rule, fs->fcrdns))
		return 1;
	return 0;
}

static int
filter_exec_helo(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	if (filter_check_table(rule, K_DOMAIN, param) ||
	    filter_check_regex(rule, param) ||
	    filter_check_rdns(rule, fs->rdns) ||
	    filter_check_fcrdns(rule, fs->fcrdns))
		return 1;
	return 0;
}

static int
filter_exec_mail_from(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	char	buffer[SMTPD_MAXMAILADDRSIZE];
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	(void)strlcpy(buffer, param+1, sizeof(buffer));
	buffer[strcspn(buffer, ">")] = '\0';
	param = buffer;

	if (filter_check_table(rule, K_MAILADDR, param) ||
	    filter_check_regex(rule, param) ||
	    filter_check_rdns(rule, fs->rdns) ||
	    filter_check_fcrdns(rule, fs->fcrdns))
		return 1;
	return 0;
}

static int
filter_exec_rcpt_to(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	char	buffer[SMTPD_MAXMAILADDRSIZE];
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	(void)strlcpy(buffer, param+1, sizeof(buffer));
	buffer[strcspn(buffer, ">")] = '\0';
	param = buffer;

	if (filter_check_table(rule, K_MAILADDR, param) ||
	    filter_check_regex(rule, param) ||
	    filter_check_rdns(rule, fs->rdns) ||
	    filter_check_fcrdns(rule, fs->fcrdns))
		return 1;
	return 0;
}
