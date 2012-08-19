/*	$OpenBSD: queue_backend.c,v 1.29 2012/08/19 14:16:58 chl Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/stat.h>

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static const char* envelope_validate(struct envelope *, uint64_t);

extern struct queue_backend	queue_backend_fs;

int
queue_message_incoming_path(uint32_t msgid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%08x",
	    PATH_INCOMING,
	    msgid);
}

int
queue_envelope_incoming_path(uint64_t evpid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%08x%s/%016" PRIx64,
	    PATH_INCOMING,
	    evpid_to_msgid(evpid),
	    PATH_ENVELOPES,
	    evpid);
}

int
queue_message_incoming_delete(uint32_t msgid)
{
	char rootdir[MAXPATHLEN];

	if (! queue_message_incoming_path(msgid, rootdir, sizeof(rootdir)))
		fatal("queue_message_incoming_delete: snprintf");

	if (rmtree(rootdir, 0) == -1)
		fatal("queue_message_incoming_delete: rmtree");

	return 1;
}

struct queue_backend *
queue_backend_lookup(const char *name)
{
	if (!strcmp(name, "fs"))
		return &queue_backend_fs;

	return (NULL);
}

int
queue_message_create(uint32_t *msgid)
{
	return env->sc_queue->message(QOP_CREATE, msgid);
}

int
queue_message_delete(uint32_t msgid)
{
	return env->sc_queue->message(QOP_DELETE, &msgid);
}

int
queue_message_commit(uint32_t msgid)
{
	return env->sc_queue->message(QOP_COMMIT, &msgid);
}

int
queue_message_corrupt(uint32_t msgid)
{
	return env->sc_queue->message(QOP_CORRUPT, &msgid);
}

int
queue_message_fd_r(uint32_t msgid)
{
	return env->sc_queue->message(QOP_FD_R, &msgid);
}

int
queue_message_fd_rw(uint32_t msgid)
{
	char msgpath[MAXPATHLEN];

	queue_message_incoming_path(msgid, msgpath, sizeof msgpath);
	strlcat(msgpath, PATH_MESSAGE, sizeof(msgpath));

	return open(msgpath, O_RDWR | O_CREAT | O_EXCL, 0600);
}

int
queue_envelope_create(struct envelope *ep)
{
	int r;

	ep->creation = time(NULL);
	r = env->sc_queue->envelope(QOP_CREATE, ep);
	if (!r) {
		ep->creation = 0;
		ep->id = 0;
	}
	return (r);
}

int
queue_envelope_delete(struct envelope *ep)
{
	return env->sc_queue->envelope(QOP_DELETE, ep);
}

int
queue_envelope_load(uint64_t evpid, struct envelope *ep)
{
	const char	*e;

	ep->id = evpid;
	if (env->sc_queue->envelope(QOP_LOAD, ep)) {
		if ((e = envelope_validate(ep, evpid)) == NULL) {
			ep->id = evpid;
			return (1);
		}
		log_debug("invalid envelope %016" PRIx64 ": %s", ep->id, e);
	}
	return (0);
}

int
queue_envelope_update(struct envelope *ep)
{
	return env->sc_queue->envelope(QOP_UPDATE, ep);
}

void *
qwalk_new(uint32_t msgid)
{
	return env->sc_queue->qwalk_new(msgid);
}

int
qwalk(void *hdl, uint64_t *evpid)
{
	return env->sc_queue->qwalk(hdl, evpid);
}

void
qwalk_close(void *hdl)
{
	return env->sc_queue->qwalk_close(hdl);
}

uint32_t
queue_generate_msgid(void)
{
	uint32_t msgid;

	while((msgid = arc4random_uniform(0xffffffff)) == 0)
		;

	return msgid;
}

uint64_t
queue_generate_evpid(uint32_t msgid)
{
	uint32_t rnd;
	uint64_t evpid;

	while((rnd = arc4random_uniform(0xffffffff)) == 0)
		;

	evpid = msgid;
	evpid <<= 32;
	evpid |= rnd;

	return evpid;
}


/**/
static const char*
envelope_validate(struct envelope *ep, uint64_t id)
{
	if (ep->version != SMTPD_ENVELOPE_VERSION)
		return "version mismatch";

	if (evpid_to_msgid(ep->id) != (evpid_to_msgid(id)))
		return "msgid mismatch";

	if (memchr(ep->helo, '\0', sizeof(ep->helo)) == NULL)
		return "invalid helo";
	if (ep->helo[0] == '\0')
		return "empty helo";

	if (memchr(ep->hostname, '\0', sizeof(ep->hostname)) == NULL)
		return "invalid hostname";
	if (ep->hostname[0] == '\0')
		return "empty hostname";

	if (memchr(ep->errorline, '\0', sizeof(ep->errorline)) == NULL)
		return "invalid error line";

	return NULL;
}
