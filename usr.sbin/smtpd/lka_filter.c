/*	$OpenBSD: lka_filter.c,v 1.21 2018/12/21 20:38:42 gilles Exp $	*/

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

#define	PROTOCOL_VERSION	1

struct filter;
struct filter_session;
static void	filter_protocol(uint64_t, enum filter_phase, const char *);
static void	filter_protocol_next(uint64_t, uint64_t, const char *);
static void	filter_protocol_query(struct filter *, uint64_t, uint64_t, const char *, const char *);

static void	filter_data(uint64_t, const char *);
static void	filter_data_next(uint64_t, uint64_t, const char *);
static void	filter_data_query(struct filter *, uint64_t, uint64_t, const char *);

static int	filter_builtins_notimpl(struct filter_session *, struct filter *, uint64_t, const char *);
static int	filter_builtins_connect(struct filter_session *, struct filter *, uint64_t, const char *);
static int	filter_builtins_helo(struct filter_session *, struct filter *, uint64_t, const char *);
static int	filter_builtins_mail_from(struct filter_session *, struct filter *, uint64_t, const char *);
static int	filter_builtins_rcpt_to(struct filter_session *, struct filter *, uint64_t, const char *);

static void	filter_result_proceed(uint64_t);
static void	filter_result_rewrite(uint64_t, const char *);
static void	filter_result_reject(uint64_t, const char *);
static void	filter_result_disconnect(uint64_t, const char *);

static void	filter_session_io(struct io *, int, void *);
int		lka_filter_process_response(const char *, const char *);


struct filter_session {
	uint64_t	id;
	struct io	*io;

	char *filter_name;
	struct sockaddr_storage ss_src;
	struct sockaddr_storage ss_dest;
	char *rdns;
	int fcrdns;

	enum filter_phase	phase;
};

static struct filter_exec {
	enum filter_phase	phase;
	const char	       *phase_name;
	int		       (*func)(struct filter_session *, struct filter *, uint64_t, const char *);
} filter_execs[FILTER_PHASES_COUNT] = {
	{ FILTER_CONNECT,	"connect",	filter_builtins_connect },
	{ FILTER_HELO,		"helo",		filter_builtins_helo },
	{ FILTER_EHLO,		"ehlo",		filter_builtins_helo },
	{ FILTER_STARTTLS,     	"starttls",	filter_builtins_notimpl },
	{ FILTER_AUTH,     	"auth",		filter_builtins_notimpl },
	{ FILTER_MAIL_FROM,    	"mail-from",	filter_builtins_mail_from },
	{ FILTER_RCPT_TO,    	"rcpt-to",	filter_builtins_rcpt_to },
	{ FILTER_DATA,    	"data",		filter_builtins_notimpl },
	{ FILTER_DATA_LINE,    	"data-line",   	filter_builtins_notimpl },
	{ FILTER_RSET,    	"rset",		filter_builtins_notimpl },
	{ FILTER_QUIT,    	"quit",		filter_builtins_notimpl },
	{ FILTER_NOOP,    	"noop",		filter_builtins_notimpl },
	{ FILTER_HELP,    	"help",		filter_builtins_notimpl },
	{ FILTER_WIZ,    	"wiz",		filter_builtins_notimpl },
	{ FILTER_COMMIT,    	"commit",      	filter_builtins_notimpl },
};

struct filter {
	uint64_t		id;
	uint32_t		phases;
	const char	       *name;
	const char	       *proc;
	struct filter  	      **chain;
	size_t 			chain_size;
	struct filter_config   *config;
};
static struct dict filters;

struct filter_entry {
	TAILQ_ENTRY(filter_entry)	entries;
	uint64_t			id;
	const char		       *name;
};

struct filter_chain {
	TAILQ_HEAD(, filter_entry)		chain[nitems(filter_execs)];
};

static struct dict	smtp_in;

static struct tree	sessions;
static int		inited;

static struct dict	filter_chains;

void
lka_filter_init(void)
{
	void		*iter;
	const char	*name;
	struct filter  	*filter;
	struct filter_config	*filter_config;
	size_t		i;

	dict_init(&filters);
	dict_init(&filter_chains);

	iter = NULL;
	while (dict_iter(env->sc_filters_dict, &iter, &name, (void **)&filter_config)) {
		switch (filter_config->filter_type) {
		case FILTER_TYPE_BUILTIN:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->phases |= (1<<filter_config->phase);
			filter->config = filter_config;
			dict_set(&filters, name, filter);
			break;

		case FILTER_TYPE_PROC:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->proc = filter_config->proc;
			filter->config = filter_config;
			dict_set(&filters, name, filter);
			break;

		case FILTER_TYPE_CHAIN:
			break;
		}
	}

	iter = NULL;
	while (dict_iter(env->sc_filters_dict, &iter, &name, (void **)&filter_config)) {
		switch (filter_config->filter_type) {
		case FILTER_TYPE_CHAIN:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->chain = xcalloc(filter_config->chain_size, sizeof(void **));
			filter->chain_size = filter_config->chain_size;
			filter->config = filter_config;
			for (i = 0; i < filter->chain_size; ++i)
				filter->chain[i] = dict_xget(&filters, filter_config->chain[i]);
			dict_set(&filters, name, filter);
			break;

		case FILTER_TYPE_BUILTIN:
		case FILTER_TYPE_PROC:
			break;
		}
	}
}

void
lka_filter_register_hook(const char *name, const char *hook)
{
	struct dict		*subsystem;
	struct filter		*filter;
	const char	*filter_name;
	void		*iter;
	size_t	i;

	if (strncasecmp(hook, "smtp-in|", 8) == 0) {
		subsystem = &smtp_in;
		hook += 8;
	}
	else
		return;

	for (i = 0; i < nitems(filter_execs); i++)
		if (strcmp(hook, filter_execs[i].phase_name) == 0)
			break;
	if (i == nitems(filter_execs))
		return;

	iter = NULL;
	while (dict_iter(&filters, &iter, &filter_name, (void **)&filter))
		if (filter->proc && strcmp(name, filter->proc) == 0)
			filter->phases |= (1<<filter_execs[i].phase);
}

void
lka_filter_ready(void)
{
	struct filter  	*filter;
	struct filter  	*subfilter;
	const char	*filter_name;
	struct filter_entry	*filter_entry;
	struct filter_chain	*filter_chain;
	void		*iter;
	size_t		i;
	size_t		j;

	iter = NULL;
	while (dict_iter(&filters, &iter, &filter_name, (void **)&filter)) {
		filter_chain = xcalloc(1, sizeof *filter_chain);
		for (i = 0; i < nitems(filter_execs); i++)
			TAILQ_INIT(&filter_chain->chain[i]);
		dict_set(&filter_chains, filter_name, filter_chain);

		if (filter->chain) {
			for (i = 0; i < filter->chain_size; i++) {
				subfilter = filter->chain[i];
				for (j = 0; j < nitems(filter_execs); ++j) {
					if (subfilter->phases & (1<<j)) {
						filter_entry = xcalloc(1, sizeof *filter_entry);
						filter_entry->id = generate_uid();
						filter_entry->name = subfilter->name;
						TAILQ_INSERT_TAIL(&filter_chain->chain[j],
						    filter_entry, entries);
					}
				}
			}
			continue;
		}

		for (i = 0; i < nitems(filter_execs); ++i) {
			if (filter->phases & (1<<i)) {
				filter_entry = xcalloc(1, sizeof *filter_entry);
				filter_entry->id = generate_uid();
				filter_entry->name = filter_name;
				TAILQ_INSERT_TAIL(&filter_chain->chain[i],
				    filter_entry, entries);
			}
		}
	}
}

int
lka_filter_proc_in_session(uint64_t reqid, const char *proc)
{
	struct filter_session	*fs;
	struct filter		*filter;
	size_t			 i;

	if ((fs = tree_get(&sessions, reqid)) == NULL)
		return 0;

	filter = dict_get(&filters, fs->filter_name);
	if (filter->proc == NULL && filter->chain == NULL)
		return 0;

	if (filter->proc)
		return strcmp(filter->proc, proc) == 0 ? 1 : 0;

	for (i = 0; i < filter->chain_size; i++)
		if (filter->chain[i]->proc &&
		    strcmp(filter->chain[i]->proc, proc) == 0)
			return 1;

	return 0;
}

void
lka_filter_begin(uint64_t reqid,
    const char *filter_name,
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
	fs->filter_name = xstrdup(filter_name);
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
	io_set_nonblocking(sp[0]);
	io_set_nonblocking(sp[1]);
	fd = sp[0];
	fs->io = io_new();
	io_set_fd(fs->io, sp[1]);
	io_set_callback(fs->io, filter_session_io, fs);

end:
	m_create(p_pony, IMSG_FILTER_SMTP_DATA_BEGIN, 0, 0, fd);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, fd != -1 ? 1 : 0);
	m_close(p_pony);
}

void
lka_filter_data_end(uint64_t reqid)
{
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	if (fs->io) {
		io_free(fs->io);
		fs->io = NULL;
	}
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

	case IO_DISCONNECTED:
		io_free(fs->io);
		fs->io = NULL;
		break;
	}
}

int
lka_filter_process_response(const char *name, const char *line)
{
	uint64_t reqid;
	uint64_t token;
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
	if (strcmp(kind, "register") == 0)
		return 1;

	if (strcmp(kind, "filter-result") != 0 &&
	    strcmp(kind, "filter-dataline") != 0)
		return 0;

	qid = ep+1;
	if ((ep = strchr(qid, '|')) == NULL)
		return 0;
	*ep = 0;

	token = strtoull(qid, &ep, 16);
	if (qid[0] == '\0' || *ep != '\0')
		return 0;
	if (errno == ERANGE && token == ULONG_MAX)
		return 0;

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
		filter_data_next(token, reqid, response);
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

	if (strcmp(response, "rewrite") == 0) {
		filter_result_rewrite(reqid, parameter);
		return 1;
	}

	if (strcmp(response, "reject") == 0) {
		filter_result_reject(reqid, parameter);
		return 1;
	}

	if (strcmp(response, "disconnect") == 0) {
		filter_result_disconnect(reqid, parameter);
		return 1;
	}

	filter_protocol_next(token, reqid, parameter);
	return 1;
}

void
lka_filter_protocol(uint64_t reqid, enum filter_phase phase, const char *param)
{
	filter_protocol(reqid, phase, param);
}

void
filter_protocol(uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_session	*fs;
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;
	uint8_t i;

	fs = tree_xget(&sessions, reqid);
	filter_chain = dict_get(&filter_chains, fs->filter_name);

	for (i = 0; i < nitems(filter_execs); ++i)
		if (phase == filter_execs[i].phase)
			break;
	if (i == nitems(filter_execs))
		goto proceed;
	if (TAILQ_EMPTY(&filter_chain->chain[i]))
		goto proceed;

	fs->phase = phase;
	TAILQ_FOREACH(filter_entry, &filter_chain->chain[i], entries) {
		filter = dict_get(&filters, filter_entry->name);
		if (filter->proc) {
			filter_protocol_query(filter, filter_entry->id, reqid,
			    filter_execs[i].phase_name, param);
			return;	/* deferred */
		}

		if (filter_execs[i].func(fs, filter, reqid, param)) {
			if (filter->config->rewrite)
				filter_result_rewrite(reqid, filter->config->rewrite);
			else if (filter->config->disconnect)
				filter_result_disconnect(reqid, filter->config->disconnect);
			else
				filter_result_reject(reqid, filter->config->reject);
			return;
		}
	}

proceed:
	filter_result_proceed(reqid);
}

static void
filter_protocol_next(uint64_t token, uint64_t reqid, const char *param)
{
	struct filter_session	*fs;
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;

	/* client session may have disappeared while we were in proc */
	if ((fs = tree_xget(&sessions, reqid)) == NULL)
		return;

	filter_chain = dict_get(&filter_chains, fs->filter_name);
	TAILQ_FOREACH(filter_entry, &filter_chain->chain[fs->phase], entries)
	    if (filter_entry->id == token)
		    break;

	while ((filter_entry = TAILQ_NEXT(filter_entry, entries))) {
		filter = dict_get(&filters, filter_entry->name);
		if (filter->proc) {
			filter_protocol_query(filter, filter_entry->id, reqid,
			    filter_execs[fs->phase].phase_name, param);
			return;	/* deferred */
		}

		if (filter_execs[fs->phase].func(fs, filter, reqid, param)) {
			if (filter->config->rewrite)
				filter_result_rewrite(reqid, filter->config->rewrite);
			else if (filter->config->disconnect)
				filter_result_disconnect(reqid, filter->config->disconnect);
			else
				filter_result_reject(reqid, filter->config->reject);
			return;
		}
	}

	filter_result_proceed(reqid);
}


static void
filter_data(uint64_t reqid, const char *line)
{
	struct filter_session	*fs;
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;

	fs = tree_xget(&sessions, reqid);

	fs->phase = FILTER_DATA_LINE;
	filter_chain = dict_get(&filter_chains, fs->filter_name);
	filter_entry = TAILQ_FIRST(&filter_chain->chain[fs->phase]);
	if (filter_entry == NULL) {
		io_printf(fs->io, "%s\r\n", line);
		return;
	}

	filter = dict_get(&filters, filter_entry->name);
	filter_data_query(filter, filter_entry->id, reqid, line);
}

static void
filter_data_next(uint64_t token, uint64_t reqid, const char *line)
{
	struct filter_session	*fs;
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;

	/* client session may have disappeared while we were in proc */
	if ((fs = tree_get(&sessions, reqid)) == NULL)
		return;

	filter_chain = dict_get(&filter_chains, fs->filter_name);

	TAILQ_FOREACH(filter_entry, &filter_chain->chain[fs->phase], entries)
	    if (filter_entry->id == token)
		    break;

	if ((filter_entry = TAILQ_NEXT(filter_entry, entries))) {
		filter = dict_get(&filters, filter_entry->name);
		filter_data_query(filter, filter_entry->id, reqid, line);
		return;
	}

	io_printf(fs->io, "%s\r\n", line);
}


int
lka_filter_response(uint64_t reqid, const char *response, const char *param)
{
	if (strcmp(response, "proceed") == 0)
		filter_result_proceed(reqid);
	else if (strcmp(response, "rewrite") == 0)
		filter_result_rewrite(reqid, param);
	else if (strcmp(response, "reject") == 0)
		filter_result_reject(reqid, param);
	else if (strcmp(response, "disconnect") == 0)
		filter_result_disconnect(reqid, param);
	else
		return 0;
	return 1;
}

static void
filter_protocol_query(struct filter *filter, uint64_t token, uint64_t reqid, const char *phase, const char *param)
{
	int	n;
	time_t	tm;
	struct filter_session	*fs;

	fs = tree_xget(&sessions, reqid);
	time(&tm);
	if (strcmp(phase, "connect") == 0)
		n = io_printf(lka_proc_get_io(filter->proc),
		    "filter|%d|%zd|smtp-in|%s|%016"PRIx64"|%016"PRIx64"|%s|%s\n",
		    PROTOCOL_VERSION,
		    tm,
		    phase, token, reqid, fs->rdns, param);
	else
		n = io_printf(lka_proc_get_io(filter->proc),
		    "filter|%d|%zd|smtp-in|%s|%016"PRIx64"|%016"PRIx64"|%s\n",
		    PROTOCOL_VERSION,
		    tm,
		    phase, token, reqid, param);
	if (n == -1)
		fatalx("failed to write to processor");
}

static void
filter_data_query(struct filter *filter, uint64_t token, uint64_t reqid, const char *line)
{
	int	n;
	time_t	tm;

	time(&tm);
	n = io_printf(lka_proc_get_io(filter->proc),
	    "filter|%d|%zd|smtp-in|data-line|"
	    "%016"PRIx64"|%016"PRIx64"|%s\n",
	    PROTOCOL_VERSION,
	    tm, token, reqid, line);
	if (n == -1)
		fatalx("failed to write to processor");
}

static void
filter_result_proceed(uint64_t reqid)
{
	m_create(p_pony, IMSG_FILTER_SMTP_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_PROCEED);
	m_close(p_pony);
}

static void
filter_result_rewrite(uint64_t reqid, const char *param)
{
	m_create(p_pony, IMSG_FILTER_SMTP_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_REWRITE);
	m_add_string(p_pony, param);
	m_close(p_pony);
}

static void
filter_result_reject(uint64_t reqid, const char *message)
{
	m_create(p_pony, IMSG_FILTER_SMTP_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_REJECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}

static void
filter_result_disconnect(uint64_t reqid, const char *message)
{
	m_create(p_pony, IMSG_FILTER_SMTP_PROTOCOL, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, FILTER_DISCONNECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}


/* below is code for builtin filters */

static int
filter_check_rdns_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->rdns_table) {
		if (table_lookup(filter->config->rdns_table, NULL, key, kind, NULL) > 0)
			ret = 1;
		ret = filter->config->not_rdns_table < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_rdns_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->rdns_regex) {
		if (table_lookup(filter->config->rdns_regex, NULL, key, K_REGEX, NULL) > 0)
			ret = 1;
		ret = filter->config->not_rdns_regex < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_src_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->src_table) {
		if (table_lookup(filter->config->src_table, NULL, key, kind, NULL) > 0)
			ret = 1;
		ret = filter->config->not_src_table < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_src_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->src_regex) {
		if (table_lookup(filter->config->src_regex, NULL, key, K_REGEX, NULL) > 0)
			ret = 1;
		ret = filter->config->not_src_regex < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_fcrdns(struct filter *filter, int fcrdns)
{
	int	ret = 0;

	if (filter->config->fcrdns) {
		ret = fcrdns == 0;
		ret = filter->config->not_fcrdns < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_rdns(struct filter *filter, const char *hostname)
{
	int	ret = 0;
	struct netaddr	netaddr;

	if (filter->config->rdns) {
		/* if text_to_netaddress succeeds,
		 * we don't have an rDNS so the filter should match
		 */
		ret = text_to_netaddr(&netaddr, hostname);
		ret = filter->config->not_rdns < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_builtins_notimpl(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return 0;
}

static int
filter_builtins_global(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	if (filter_check_fcrdns(filter, fs->fcrdns) ||
	    filter_check_rdns(filter, fs->rdns) ||
	    filter_check_rdns_table(filter, K_DOMAIN, fs->rdns) ||
	    filter_check_rdns_regex(filter, fs->rdns) ||
	    filter_check_src_table(filter, K_NETADDR, ss_to_text(&fs->ss_src)) ||
	    filter_check_src_regex(filter, ss_to_text(&fs->ss_src)))
		return 1;
	return 0;
}

static int
filter_builtins_connect(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid, param);
}

static int
filter_builtins_helo(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid, param);
}

static int
filter_builtins_mail_from(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid, param);
}

static int
filter_builtins_rcpt_to(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid, param);
}
