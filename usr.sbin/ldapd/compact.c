/*	$OpenBSD: compact.c,v 1.1 2010/05/31 17:36:31 martinh Exp $ */

/*
 * Copyright (c) 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "ldapd.h"

static int		 continue_compaction(struct ctl_conn *c);
static void		 stop_compaction(struct ctl_conn *c);

void
check_compaction(pid_t pid, int status)
{
	struct ctl_conn		*c;

	if ((c = control_connbypid(pid)) == NULL)
		return;

	if (WIFEXITED(status)) {
		log_debug("compaction process %d exited with status %d",
		    pid, WEXITSTATUS(status));
		if (WEXITSTATUS(status) == 0) {
			control_report_compaction(c, 0);
			continue_compaction(c);
			return;
		}
	} else if (WIFSIGNALED(status))
		log_warn("compaction process %d exited due to signal %d",
		    pid, WTERMSIG(status));
	else
		log_debug("compaction process %d exited", pid);

	/* Compaction failed, no need to continue (disk might be full).
	 */
	control_report_compaction(c, 1);
	if (c->ns != NULL)
		c->ns->compacting = 0;
	c->ns = NULL;
}

static pid_t
compact(struct btree *bt)
{
	pid_t		 pid;

	pid = fork();
	if (pid < 0) {
		log_warn("compaction monitor: fork");
		return -1;
	}
	if (pid > 0)
		return pid;

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	setproctitle("compacting %s", btree_get_path(bt));
	_exit(btree_compact(bt));
}

static int
continue_compaction(struct ctl_conn *c)
{
again:
	switch (c->state) {
	case COMPACT_DATA:
		log_info("compacting namespace %s (entries)", c->ns->suffix);
		if (namespace_commit(c->ns) != 0) {
			control_report_compaction(c, 1);
			goto fail;
		}
		c->ns->compacting = 1;
		if ((c->pid = compact(c->ns->data_db)) < 0)
			goto fail;
		c->state = COMPACT_INDX;
		break;
	case COMPACT_INDX:
		if (namespace_reopen_data(c->ns) != 0)
			goto fail;
		log_info("compacting namespace %s (index)", c->ns->suffix);
		if ((c->pid = compact(c->ns->indx_db)) < 0)
			goto fail;
		c->state = COMPACT_DONE;
		break;
	case COMPACT_DONE:
		if (namespace_reopen_indx(c->ns) != 0)
			goto fail;
		c->ns->compacting = 0;
		c->pid = 0;
		namespace_queue_schedule(c->ns);

		if (c->all) {
			/* Proceed with the next namespace that isn't 
			 * already being compacted or indexed.
			 */
			while ((c->ns = TAILQ_NEXT(c->ns, next)) != NULL) {
				if (!c->ns->compacting && !c->ns->indexing)
					break;
			}
		} else
			c->ns = NULL;

		if (c->ns == NULL) {
			control_end(c);
			return 0;
		}
		c->state = COMPACT_DATA;
		goto again;
		break;
	default:
		assert(0);
	}

	return 0;

fail:
	control_end(c);
	namespace_remove(c->ns);
	c->ns = NULL;
	return -1;
}

/* Run compaction for the given namespace, or all namespaces if ns is NULL.
 *
 * Returns 0 on success, or -1 on error.
 */
int
run_compaction(struct ctl_conn *c, struct namespace *ns)
{
	if (ns == NULL) {
		c->all = 1;
		c->ns = TAILQ_FIRST(&conf->namespaces);
	} else {
		c->all = 0;
		c->ns = ns;
	}

	c->closecb = stop_compaction;
	c->state = COMPACT_DATA;
	return continue_compaction(c);
}

static void
stop_compaction(struct ctl_conn *c)
{
	if (c->pid != 0) {
		log_info("stopping compaction process %i", c->pid);
		if (kill(c->pid, SIGKILL) != 0)
			log_warn("failed to stop compaction process");
		c->pid = 0;
	}

	if (c->ns != NULL) {
		log_info("stopped compacting namespace %s", c->ns->suffix);
		c->ns->compacting = 0;
		namespace_queue_schedule(c->ns);
	}
	c->ns = NULL;
}

