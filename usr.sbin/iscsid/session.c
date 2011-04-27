/*	$OpenBSD: session.c,v 1.2 2011/04/27 07:25:26 claudio Exp $ */

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

void	session_fsm_callback(int, short, void *);
int	sess_do_start(struct session *, struct sessev *);
int	sess_do_fail(struct session *, struct sessev *);

const char *sess_state(int);
const char *sess_event(enum s_event);

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
	s->state = SESS_INIT;

	if (st == SESSION_TYPE_DISCOVERY)
		s->target = 0;
	else
		s->target = s->initiator->target++;

	TAILQ_INSERT_HEAD(&i->sessions, s, entry);
	TAILQ_INIT(&s->connections);
	TAILQ_INIT(&s->tasks);
	SIMPLEQ_INIT(&s->fsmq);
	evtimer_set(&s->fsm_ev, session_fsm_callback, s);

	return s;
}

void
session_cleanup(struct session *s)
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
	TAILQ_FOREACH(c, &s->connections, entry)
		if (conn_task_ready(c)) {
			TAILQ_REMOVE(&s->tasks, t, entry);
			conn_task_issue(c, t);
			return;
		}
}

/*
 * The session FSM runs from a callback so that the connection FSM can finish.
 */
void
session_fsm(struct session *s, enum s_event ev, struct connection *c)
{
	struct timeval  tv;
	struct sessev *sev;

	if ((sev = malloc(sizeof(*sev))) == NULL)
		fatal("session_fsm");
	sev->conn = c;
	sev->event = ev;
	SIMPLEQ_INSERT_TAIL(&s->fsmq, sev, entry);

	timerclear(&tv);
	if (evtimer_add(&s->fsm_ev, &tv) == -1)
		fatal("session_fsm");
}

struct {
	int		state;
	enum s_event	event;
	int		(*action)(struct session *, struct sessev *);
} s_fsm[] = {
	{ SESS_INIT, SESS_EV_START, sess_do_start },
	{ SESS_FREE, SESS_EV_CONN_FAIL, sess_do_fail },
	{ SESS_FREE, SESS_EV_CONN_CLOSED, sess_do_fail },
	{ 0, 0, NULL }
};

/* ARGSUSED */
void
session_fsm_callback(int fd, short event, void *arg)
{
	struct session *s = arg;
	struct sessev *sev;
	int	i, ns;

	while ((sev = SIMPLEQ_FIRST(&s->fsmq))) {
		SIMPLEQ_REMOVE_HEAD(&s->fsmq, entry);
		for (i = 0; s_fsm[i].action != NULL; i++) {
			if (s->state & s_fsm[i].state &&
			    sev->event == s_fsm[i].event) {
				log_debug("sess_fsm[%s]: %s ev %s",
				    s->config.SessionName, sess_state(s->state),
				    sess_event(sev->event));
				ns = s_fsm[i].action(s, sev);
				if (ns == -1)
					/* XXX better please */
					fatalx("sess_fsm: action failed");
				log_debug("sess_fsm[%s]: new state %s",
				    s->config.SessionName,
				    sess_state(ns));
				s->state = ns;
				break;
			}
		}
		if (s_fsm[i].action == NULL) {
			log_warnx("sess_fsm[%s]: unhandled state transition "
			    "[%s, %s]", s->config.SessionName,
			    sess_state(s->state), sess_event(sev->event));
			fatalx("bjork bjork bjork");
		}
		free(sev);
	}
}

int
sess_do_start(struct session *s, struct sessev *sev)
{
	log_debug("new connection to %s",
	    log_sockaddr(&s->config.connection.TargetAddr));
	conn_new(s, &s->config.connection);

	return (SESS_FREE);
}

int
sess_do_fail(struct session *s, struct sessev *sev)
{
	/*
	 * cleanup connections:
	 * Connections in state FREE can be removed.
	 * Connections in any error state will cause the session to enter
	 * the FAILED state. If no sessions are left and the session was
	 * not already FREE then explicit recovery needs to be done.
	 */
	return (SESS_FREE);
}

const char *
sess_state(int s)
{
	static char buf[15];

	switch (s) {
	case SESS_INIT:
		return "INIT";
	case SESS_FREE:
		return "FREE";
	case SESS_LOGGED_IN:
		return "LOGGED_IN";
	case SESS_FAILED:
		return "FAILED";
	default:
		snprintf(buf, sizeof(buf), "UKNWN %x", s);
		return buf;
	}
	/* NOTREACHED */
}

const char *
sess_event(enum s_event e)
{
	static char buf[15];

	switch (e) {
	case SESS_EV_START:
		return "start";
	case SESS_EV_CONN_FAIL:
		return "connection fail";
	case SESS_EV_CONN_CLOSED:
		return "connection closed";
	case SESS_EV_FAIL:
		return "fail";
	}

	snprintf(buf, sizeof(buf), "UKNWN %d", e);
	return buf;
}
