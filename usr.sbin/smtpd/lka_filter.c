/*	$OpenBSD: lka_filter.c,v 1.33 2018/12/26 15:55:09 eric Exp $	*/

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
static void	filter_protocol_internal(struct filter_session *, uint64_t *, uint64_t, enum filter_phase, const char *);
static void	filter_protocol(uint64_t, enum filter_phase, const char *);
static void	filter_protocol_next(uint64_t, uint64_t, enum filter_phase, const char *);
static void	filter_protocol_query(struct filter *, uint64_t, uint64_t, const char *, const char *);

static void	filter_data_internal(struct filter_session *, uint64_t, uint64_t, const char *);
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

	char *helo;
	char *mail_from;
	
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
	char		 buffer[LINE_MAX];	/* for traces */

	dict_init(&filters);
	dict_init(&filter_chains);

	/* first pass, allocate and init individual filters */
	iter = NULL;
	while (dict_iter(env->sc_filters_dict, &iter, &name, (void **)&filter_config)) {
		switch (filter_config->filter_type) {
		case FILTER_TYPE_BUILTIN:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->phases |= (1<<filter_config->phase);
			filter->config = filter_config;
			dict_set(&filters, name, filter);
			log_trace(TRACE_FILTERS, "filters init type=builtin, name=%s, hooks=%08x",
			    name, filter->phases);
			break;

		case FILTER_TYPE_PROC:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->proc = filter_config->proc;
			filter->config = filter_config;
			dict_set(&filters, name, filter);
			log_trace(TRACE_FILTERS, "filters init type=proc, name=%s, proc=%s",
			    name, filter_config->proc);
			break;

		case FILTER_TYPE_CHAIN:
			break;
		}
	}

	/* second pass, allocate and init filter chains but don't build yet */
	iter = NULL;
	while (dict_iter(env->sc_filters_dict, &iter, &name, (void **)&filter_config)) {
		switch (filter_config->filter_type) {
		case FILTER_TYPE_CHAIN:
			filter = xcalloc(1, sizeof(*filter));
			filter->name = name;
			filter->chain = xcalloc(filter_config->chain_size, sizeof(void **));
			filter->chain_size = filter_config->chain_size;
			filter->config = filter_config;

			buffer[0] = '\0';
			for (i = 0; i < filter->chain_size; ++i) {
				filter->chain[i] = dict_xget(&filters, filter_config->chain[i]);
				if (i)
					(void)strlcat(buffer, ", ", sizeof buffer);
				(void)strlcat(buffer, filter->chain[i]->name, sizeof buffer);
			}
			log_trace(TRACE_FILTERS, "filters init type=chain, name=%s { %s }", name, buffer);

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

	/* all filters are ready, actually build the filter chains */
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

	log_trace(TRACE_FILTERS, "%016"PRIx64" filters session-begin", reqid);
}

void
lka_filter_end(uint64_t reqid)
{
	struct filter_session	*fs;

	fs = tree_xpop(&sessions, reqid);
	free(fs->rdns);
	free(fs);
	log_trace(TRACE_FILTERS, "%016"PRIx64" filters session-end", reqid);
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
	log_trace(TRACE_FILTERS, "%016"PRIx64" filters data-begin fd=%d", reqid, fd);
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
	log_trace(TRACE_FILTERS, "%016"PRIx64" filters data-end", reqid);
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
	/*char *phase = NULL;*/
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

	filter_protocol_next(token, reqid, 0, parameter);
	return 1;
}

void
lka_filter_protocol(uint64_t reqid, enum filter_phase phase, const char *param)
{
	filter_protocol(reqid, phase, param);
}

static void
filter_protocol_internal(struct filter_session *fs, uint64_t *token, uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;
	const char		*phase_name = filter_execs[phase].phase_name;
	int			 resume = 1;

	if (!*token) {
		fs->phase = phase;
		resume = 0;
	}

	/* XXX - this sanity check requires a protocol change, stub for now */
	phase = fs->phase;
	if (fs->phase != phase)
		fatalx("misbehaving filter");

	/* based on token, identify the filter_entry we should apply  */
	filter_chain = dict_get(&filter_chains, fs->filter_name);
	filter_entry = TAILQ_FIRST(&filter_chain->chain[fs->phase]);
	if (*token) {
		TAILQ_FOREACH(filter_entry, &filter_chain->chain[fs->phase], entries)
		    if (filter_entry->id == *token)
			    break;
		if (filter_entry == NULL)
			fatalx("misbehaving filter");
		filter_entry = TAILQ_NEXT(filter_entry, entries);
	}

	/* no filter_entry, we either had none or reached end of chain */
	if (filter_entry == NULL) {
		log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, resume=%s, "
		    "action=proceed",
		    fs->id, phase_name, resume ? "y" : "n");
		filter_result_proceed(reqid);
		return;
	}

	/* process param with current filter_entry */
	*token = filter_entry->id;
	filter = dict_get(&filters, filter_entry->name);
	if (filter->proc) {
		log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, "
		    "resume=%s, action=deferred, filter=%s",
		    fs->id, phase_name, resume ? "y" : "n",
		    filter->name);
		filter_protocol_query(filter, filter_entry->id, reqid,
		    filter_execs[fs->phase].phase_name, param);
		return;	/* deferred response */
	}

	if (filter_execs[fs->phase].func(fs, filter, reqid, param)) {
		if (filter->config->rewrite) {
			log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, "
			    "resume=%s, action=rewrite, filter=%s, query=%s, response=%s",
			    fs->id, phase_name, resume ? "y" : "n",
			    filter->name,
			    param,
			    filter->config->rewrite);
			    filter_result_rewrite(reqid, filter->config->rewrite);
			return;
		}
		else if (filter->config->disconnect) {
			log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, "
			    "resume=%s, action=disconnect, filter=%s, query=%s, response=%s",
			    fs->id, phase_name, resume ? "y" : "n",
			    filter->name,
			    param,
			    filter->config->disconnect);
			filter_result_disconnect(reqid, filter->config->disconnect);
			return;
		}
		else {
			log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, "
			    "resume=%s, action=reject, filter=%s, query=%s, response=%s",
			    fs->id, phase_name, resume ? "y" : "n",
			    filter->name,
			    param,
			    filter->config->reject);
			filter_result_reject(reqid, filter->config->reject);
			return;
		}
	}

	log_trace(TRACE_FILTERS, "%016"PRIx64" filters protocol phase=%s, "
	    "resume=%s, action=proceed, filter=%s, query=%s",
	    fs->id, phase_name, resume ? "y" : "n",
	    filter->name,
	    param);

	/* filter_entry resulted in proceed, try next filter */
	filter_protocol_internal(fs, token, reqid, phase, param);
	return;
}

static void
filter_data_internal(struct filter_session *fs, uint64_t token, uint64_t reqid, const char *line)
{
	struct filter_chain	*filter_chain;
	struct filter_entry	*filter_entry;
	struct filter		*filter;

	if (!token)
		fs->phase = FILTER_DATA_LINE;
	if (fs->phase != FILTER_DATA_LINE)
		fatalx("misbehaving filter");

	/* based on token, identify the filter_entry we should apply  */
	filter_chain = dict_get(&filter_chains, fs->filter_name);
	filter_entry = TAILQ_FIRST(&filter_chain->chain[fs->phase]);
	if (token) {
		TAILQ_FOREACH(filter_entry, &filter_chain->chain[fs->phase], entries)
		    if (filter_entry->id == token)
			    break;
		if (filter_entry == NULL)
			fatalx("misbehaving filter");
		filter_entry = TAILQ_NEXT(filter_entry, entries);
	}

	/* no filter_entry, we either had none or reached end of chain */
	if (filter_entry == NULL) {
		io_printf(fs->io, "%s\r\n", line);
		return;
	}

	/* pass data to the filter */
	filter = dict_get(&filters, filter_entry->name);
	filter_data_query(filter, filter_entry->id, reqid, line);
}

static void
filter_protocol(uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_session  *fs;
	uint64_t		token = 0;
	char		       *nparam = NULL;
	
	fs = tree_xget(&sessions, reqid);

	switch (phase) {
	case FILTER_HELO:
	case FILTER_EHLO:
		if (fs->helo)
			free(fs->helo);
		fs->helo = xstrdup(param);
		break;
	case FILTER_MAIL_FROM:
		if (fs->mail_from)
			free(fs->mail_from);

		fs->mail_from = xstrdup(param + 1);
		*strchr(fs->mail_from, '>') = '\0';
		param = fs->mail_from;

		break;
	case FILTER_RCPT_TO:
		nparam = xstrdup(param + 1);
		*strchr(nparam, '>') = '\0';
		param = nparam;
		break;
	case FILTER_STARTTLS:
	case FILTER_AUTH:
		/* TBD */
		break;
	default:
		break;
	}
	filter_protocol_internal(fs, &token, reqid, phase, param);
	if (nparam)
		free(nparam);
}

static void
filter_protocol_next(uint64_t token, uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_session  *fs;

	/* session can legitimately disappear on a resume */
	if ((fs = tree_get(&sessions, reqid)) == NULL)
		return;

	filter_protocol_internal(fs, &token, reqid, phase, param);
}

static void
filter_data(uint64_t reqid, const char *line)
{
	struct filter_session  *fs;

	fs = tree_xget(&sessions, reqid);

	filter_data_internal(fs, 0, reqid, line);
}

static void
filter_data_next(uint64_t token, uint64_t reqid, const char *line)
{
	struct filter_session  *fs;

	/* session can legitimately disappear on a resume */
	if ((fs = tree_get(&sessions, reqid)) == NULL)
		return;

	filter_data_internal(fs, token, reqid, line);
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

	if (filter->config->rdns_table == NULL)
		return 0;
	
	if (table_match(filter->config->rdns_table, kind, key) > 0)
		ret = 1;

	return filter->config->not_rdns_table < 0 ? !ret : ret;
}

static int
filter_check_rdns_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->rdns_regex == NULL)
		return 0;

	if (table_match(filter->config->rdns_regex, K_REGEX, key) > 0)
		ret = 1;
	return filter->config->not_rdns_regex < 0 ? !ret : ret;
}

static int
filter_check_src_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->src_table == NULL)
		return 0;

	if (table_match(filter->config->src_table, kind, key) > 0)
		ret = 1;
	return filter->config->not_src_table < 0 ? !ret : ret;
}

static int
filter_check_src_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->src_regex == NULL)
		return 0;

	if (table_match(filter->config->src_regex, K_REGEX, key) > 0)
		ret = 1;
	return filter->config->not_src_regex < 0 ? !ret : ret;
}

static int
filter_check_helo_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->helo_table == NULL)
		return 0;

	if (table_match(filter->config->helo_table, kind, key) > 0)
		ret = 1;
	return filter->config->not_helo_table < 0 ? !ret : ret;
}

static int
filter_check_helo_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->helo_regex == NULL)
		return 0;

	if (table_match(filter->config->helo_regex, K_REGEX, key) > 0)
		ret = 1;
	return filter->config->not_helo_regex < 0 ? !ret : ret;
}

static int
filter_check_mail_from_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->mail_from_table == NULL)
		return 0;

	if (table_match(filter->config->mail_from_table, kind, key) > 0)
		ret = 1;
	return filter->config->not_mail_from_table < 0 ? !ret : ret;
}

static int
filter_check_mail_from_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->mail_from_regex == NULL)
		return 0;

	if (table_match(filter->config->mail_from_regex, K_REGEX, key) > 0)
		ret = 1;
	return filter->config->not_mail_from_regex < 0 ? !ret : ret;
}

static int
filter_check_rcpt_to_table(struct filter *filter, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (filter->config->rcpt_to_table == NULL)
		return 0;

	if (table_match(filter->config->rcpt_to_table, kind, key) > 0)
		ret = 1;
	return filter->config->not_rcpt_to_table < 0 ? !ret : ret;
}

static int
filter_check_rcpt_to_regex(struct filter *filter, const char *key)
{
	int	ret = 0;

	if (filter->config->rcpt_to_regex == NULL)
		return 0;

	if (table_match(filter->config->rcpt_to_regex, K_REGEX, key) > 0)
		ret = 1;
	return filter->config->not_rcpt_to_regex < 0 ? !ret : ret;
}

static int
filter_check_fcrdns(struct filter *filter, int fcrdns)
{
	int	ret = 0;

	if (!filter->config->fcrdns)
		return 0;

	ret = fcrdns == 0;
	return filter->config->not_fcrdns < 0 ? !ret : ret;
}

static int
filter_check_rdns(struct filter *filter, const char *hostname)
{
	int	ret = 0;
	struct netaddr	netaddr;

	if (!filter->config->rdns)
		return 0;

	/* if text_to_netaddress succeeds,
	 * we don't have an rDNS so the filter should match
	 */
	ret = text_to_netaddr(&netaddr, hostname);
	return filter->config->not_rdns < 0 ? !ret : ret;
}

static int
filter_builtins_notimpl(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return 0;
}

static int
filter_builtins_global(struct filter_session *fs, struct filter *filter, uint64_t reqid)
{
	return filter_check_fcrdns(filter, fs->fcrdns) ||
	    filter_check_rdns(filter, fs->rdns) ||
	    filter_check_rdns_table(filter, K_DOMAIN, fs->rdns) ||
	    filter_check_rdns_regex(filter, fs->rdns) ||
	    filter_check_src_table(filter, K_NETADDR, ss_to_text(&fs->ss_src)) ||
	    filter_check_src_regex(filter, ss_to_text(&fs->ss_src)) ||
	    filter_check_helo_table(filter, K_DOMAIN, fs->helo) ||
	    filter_check_helo_regex(filter, fs->helo) ||
	    filter_check_mail_from_table(filter, K_MAILADDR, fs->mail_from) ||
	    filter_check_mail_from_regex(filter, fs->mail_from);
}

static int
filter_builtins_connect(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid);
}

static int
filter_builtins_helo(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid);
}

static int
filter_builtins_mail_from(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid);
}

static int
filter_builtins_rcpt_to(struct filter_session *fs, struct filter *filter, uint64_t reqid, const char *param)
{
	return filter_builtins_global(fs, filter, reqid) ||
	    filter_check_rcpt_to_table(filter, K_MAILADDR, param) ||
	    filter_check_rcpt_to_regex(filter, param);
}
