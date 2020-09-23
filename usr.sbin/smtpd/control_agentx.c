/*	$OpenBSD: control_agentx.c,v 1.1 2020/09/23 18:01:26 martijn Exp $ */

/*
 * Copyright (c) 2020 Martijn van Duren <martijn@openbsd.org>
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
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/uio.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <stdio.h>
#include <subagentx.h>

#include "log.h"
#include "smtpd.h"

static void control_agentx_nofd(struct subagentx *, void *, int);
static void control_agentx_tryconnect(int, short, void *);
static void control_agentx_read(int, short, void *);
static void control_agentx_appltable(struct subagentx_varbind *);
static void control_agentx_mtatable(struct subagentx_varbind *);

static struct subagentx *sa;
static struct subagentx_session *sas;
static struct subagentx_context *sac;
static struct subagentx_region *application, *mta;
static struct subagentx_index *applIndex;
static struct subagentx_object *applName, *applDirectoryName;
static struct subagentx_object *applVersion, *applUptime, *applOperStatus;
static struct subagentx_object *applDescription, *applURL;
static struct subagentx_object *mtaReceivedMessages, *mtaStoredMessages;
static struct subagentx_object *mtaTransmittedMessages, *mtaLoopsDetected;
static struct event connev, rev;

extern struct stat_digest digest;

#define APPLICATION SUBAGENTX_MIB2, 27
#define APPLTABLE APPLICATION, 1
#define APPLENTRY APPLTABLE, 1
#define APPLINDEX APPLENTRY, 1
#define APPLNAME APPLENTRY, 2
#define APPLDIRECTORYNAME APPLENTRY, 3
#define APPLVERSION APPLENTRY, 4
#define APPLUPTIME APPLENTRY, 5
#define APPLOPERSTATUS APPLENTRY, 6
#define APPLLASTCHANGE APPLENTRY, 7
#define APPLDESCRIPTION APPLENTRY, 16
#define APPLURL APPLENTRY, 17
#define MTA SUBAGENTX_MIB2, 28
#define MTATABLE MTA, 1
#define MTAENTRY MTATABLE, 1
#define MTARECEIVEDMESSAGES MTAENTRY, 1
#define MTASTOREDMESSAGES MTAENTRY, 2
#define MTATRANSMITTEDMESSAGES MTAENTRY, 3
#define MTALOOPSDETECTED MTAENTRY, 12

#define APPLOPERSTATUS_UP 1
#define APPLOPERSTATUS_DOWN 2
#define APPLOPERSTATUS_HALTED 3
#define APPLOPERSTATUS_CONGESTED 4
#define APPLOPERSTATUS_RESTARTING 5
#define APPLOPERSTATUS_QUIESCING 6

void
control_agentx(void)
{
	subagentx_log_fatal = fatalx;
	subagentx_log_warn = log_warnx;
	subagentx_log_info = log_info;
	subagentx_log_debug = log_debug;

	if ((sa = subagentx(control_agentx_nofd, NULL)) == NULL)
		fatal("subagentx");
	if ((sas = subagentx_session(sa, NULL, 0, "OpenSMTPd", 0)) == NULL)
		fatal("subagentx_session");
	if ((sac = subagentx_context(sas, env->sc_agentx->context)) == NULL)
		fatal("subagentx_context");

        if ((application = subagentx_region(sac,
	    SUBAGENTX_OID(APPLICATION), 0)) == NULL ||
            (mta = subagentx_region(sac, SUBAGENTX_OID(MTA), 0)) == NULL)
                fatal("subagentx_region");
	switch (env->sc_agentx->applIndexType) {
	case AGENTX_INDEX_TYPE_ANY:
		if ((applIndex = subagentx_index_integer_any(application,
		    SUBAGENTX_OID(APPLINDEX))) == NULL)
			fatal("subagentx_index_integer_any");
		break;
	case AGENTX_INDEX_TYPE_NEW:
		if ((applIndex = subagentx_index_integer_new(application,
		    SUBAGENTX_OID(APPLINDEX))) == NULL)
			fatal("subagentx_index_integer_new");
		break;
	case AGENTX_INDEX_TYPE_VALUE:
		if ((applIndex = subagentx_index_integer_value(application,
		    SUBAGENTX_OID(APPLINDEX),
		    env->sc_agentx->applIndex)) == NULL)
			fatal("subagentx_index_integer_value");
		break;
	case AGENTX_INDEX_TYPE_UNDEFINED:
		fatalx("%s: How did I get here?", __func__);
	}

        if ((applName = subagentx_object(application, SUBAGENTX_OID(APPLNAME),
	    &applIndex, 1, 0, control_agentx_appltable)) == NULL ||
            (applDirectoryName = subagentx_object(application,
            SUBAGENTX_OID(APPLDIRECTORYNAME), &applIndex, 1,
            0, control_agentx_appltable)) == NULL ||
            (applVersion = subagentx_object(application,
            SUBAGENTX_OID(APPLVERSION), &applIndex, 1,
            0, control_agentx_appltable)) == NULL ||
            (applUptime = subagentx_object(application,
            SUBAGENTX_OID(APPLUPTIME), &applIndex, 1,
            0, control_agentx_appltable)) == NULL ||
            (applOperStatus = subagentx_object(application,
            SUBAGENTX_OID(APPLOPERSTATUS), &applIndex, 1,
            0, control_agentx_appltable)) == NULL ||
            (applDescription = subagentx_object(application,
            SUBAGENTX_OID(APPLDESCRIPTION), &applIndex, 1,
            0, control_agentx_appltable)) == NULL ||
            (applURL = subagentx_object(application, SUBAGENTX_OID(APPLURL),
	    &applIndex, 1, 0, control_agentx_appltable)) == NULL ||
	    (mtaReceivedMessages = subagentx_object(mta,
            SUBAGENTX_OID(MTARECEIVEDMESSAGES), &applIndex, 1,
            0, control_agentx_mtatable)) == NULL ||
	    (mtaStoredMessages = subagentx_object(mta,
            SUBAGENTX_OID(MTASTOREDMESSAGES), &applIndex, 1,
            0, control_agentx_mtatable)) == NULL ||
	    (mtaTransmittedMessages = subagentx_object(mta,
            SUBAGENTX_OID(MTATRANSMITTEDMESSAGES), &applIndex, 1,
            0, control_agentx_mtatable)) == NULL ||
	    (mtaLoopsDetected = subagentx_object(mta,
            SUBAGENTX_OID(MTALOOPSDETECTED), &applIndex, 1,
            0, control_agentx_mtatable)) == NULL)
                fatal("subagentx_object");
	
}

static void
control_agentx_nofd(struct subagentx *unused, void *cookie, int close)
{
	event_del(&rev);
	control_agentx_tryconnect(-1, 0, NULL);
}

static void
control_agentx_tryconnect(int fd, short event, void *cookie)
{
	m_create(p_parent, IMSG_AGENTX_GETFD, 0, 0, -1);
	m_close(p_parent);
}

static void
control_agentx_read(int fd, short event, void *cookie)
{
	subagentx_read(sa);
}

void
control_agentx_connect(int fd)
{
	struct timeval timeout = {3, 0};

	if (fd == -1) {
		evtimer_set(&connev, control_agentx_tryconnect, NULL);
		evtimer_add(&connev, &timeout);
		return;
	}

	subagentx_connect(sa, fd);

	event_set(&rev, fd, EV_READ|EV_PERSIST, control_agentx_read, NULL);
	event_add(&rev, NULL);
}

static void
control_agentx_appltable(struct subagentx_varbind *vb)
{
	if (subagentx_varbind_get_object(vb) == applName)
		subagentx_varbind_string(vb, "OpenSMTPd");
	else if (subagentx_varbind_get_object(vb) == applDirectoryName)
		subagentx_varbind_string(vb, "");
	else if (subagentx_varbind_get_object(vb) == applVersion)
		subagentx_varbind_string(vb, SMTPD_VERSION);
	else if (subagentx_varbind_get_object(vb) == applUptime)
		subagentx_varbind_timeticks(vb,
		    (uint32_t) ((time(NULL) - digest.startup) * 100));
	else if (subagentx_varbind_get_object(vb) == applOperStatus)
		subagentx_varbind_integer(vb,
		    env->sc_flags & SMTPD_SMTP_PAUSED ?
		    APPLOPERSTATUS_DOWN : APPLOPERSTATUS_UP);
	else if (subagentx_varbind_get_object(vb) == applDescription)
		subagentx_varbind_string(vb, "OpenSMTPD is a FREE "
		    "implementation of the server-side SMTP protocol as "
		    "defined by RFC 5321, with some additional standard "
		    "extensions. It allows ordinary machines to exchange "
		    "emails with other systems speaking the SMTP protocol.");
	else if (subagentx_varbind_get_object(vb) == applURL)
		subagentx_varbind_string(vb, "");
	else
		fatalx("%s: unexpected callback", __func__);
}

static void
control_agentx_mtatable(struct subagentx_varbind *vb)
{
	if (subagentx_varbind_get_object(vb) == mtaReceivedMessages)
		subagentx_varbind_counter32(vb, (uint32_t) digest.evp_enqueued);
	else if (subagentx_varbind_get_object(vb) == mtaStoredMessages)
		subagentx_varbind_counter32(vb,
		    (uint32_t) (digest.evp_enqueued - digest.evp_dequeued));
	else if (subagentx_varbind_get_object(vb) == mtaTransmittedMessages)
		subagentx_varbind_counter32(vb, digest.evp_dequeued);
	else if (subagentx_varbind_get_object(vb) == mtaLoopsDetected)
		subagentx_varbind_counter32(vb, digest.dlv_loop);
	else
		fatalx("%s: unexpected callback", __func__);
}
