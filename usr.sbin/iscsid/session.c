/*	$OpenBSD: session.c,v 1.1 2011/04/05 18:26:19 claudio Exp $ */

/*
 * Copyright (c) 2011 Claudio Jeker <claudio@openbsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <scsi/iscsi.h>

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

struct session *
session_find(struct initiator *i, char *name)
{
	struct session *s;

	TAILQ_FOREACH(s, &i->sessions, entry) {
		if (strcmp(s->config.SessionName, name) == 0)
			return s;
	}
	return NULL;
}

struct session *
session_new(struct initiator *i, u_int8_t st)
{
	struct session *s;

	if (!(s = calloc(1, sizeof(*s))))
		return NULL;

	/* use the same qualifier unless there is a conflict */
	s->isid_base = i->config.isid_base;
	s->isid_qual = i->config.isid_qual;
	s->cmdseqnum = arc4random();
	s->itt = arc4random();
	s->initiator = i;
	s->state = SESS_FREE;

	if (st == SESSION_TYPE_DISCOVERY)
		s->target = 0;
	else
		s->target = s->initiator->target++;

	TAILQ_INSERT_HEAD(&i->sessions, s, entry);
	TAILQ_INIT(&s->connections);
	TAILQ_INIT(&s->tasks);

	return s;
}

void
session_close(struct session *s)
{
	struct connection *c;

	while ((c = TAILQ_FIRST(&s->connections)) != NULL)
		conn_free(c);

	free(s->config.TargetName);
	free(s->config.InitiatorName);
	free(s);
}

void
session_config(struct session *s, struct session_config *sc)
{
	if (s->config.TargetName)
		free(s->config.TargetName);
	s->config.TargetName = NULL;
	if (s->config.InitiatorName)
		free(s->config.InitiatorName);
	s->config.InitiatorName = NULL;

	s->config = *sc;

	if (sc->TargetName) {
		s->config.TargetName = strdup(sc->TargetName);
		if (s->config.TargetName == NULL)
			fatal("strdup");
	}
	if (sc->InitiatorName) {
		s->config.InitiatorName = strdup(sc->InitiatorName);
		if (s->config.InitiatorName == NULL)
			fatal("strdup");
	} else
		s->config.InitiatorName = default_initiator_name();
}

void
session_task_issue(struct session *s, struct task *t)
{
	TAILQ_INSERT_TAIL(&s->tasks, t, entry);
	session_schedule(s);
}

void
session_schedule(struct session *s)
{
	struct task *t = TAILQ_FIRST(&s->tasks);
	struct connection *c;

	if (!t)
		return;

	/* XXX IMMEDIATE TASK NEED SPECIAL HANDLING !!!! */

	/* wake up a idle connection or a not busy one */
	/* XXX this needs more work as it makes the daemon go wrooOOOMM */
	TAILQ_REMOVE(&s->tasks, t, entry);
	TAILQ_FOREACH(c, &s->connections, entry)
		if (conn_task_issue(c, t))
			return;
	/* all connections are busy readd task to the head */
	TAILQ_INSERT_HEAD(&s->tasks, t, entry);
}
