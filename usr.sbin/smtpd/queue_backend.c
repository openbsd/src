/*	$OpenBSD: queue_backend.c,v 1.34 2012/08/26 11:21:28 gilles Exp $	*/

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
#include <errno.h>
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

static const char* envelope_validate(struct envelope *);

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
	char	msgpath[MAXPATHLEN];
	char	tmppath[MAXPATHLEN];
	int	fdin = -1, fdout = -1;

	queue_message_incoming_path(msgid, msgpath, sizeof msgpath);
	strlcat(msgpath, PATH_MESSAGE, sizeof(msgpath));

	if (env->sc_queue_flags & QUEUE_COMPRESS) {

		bsnprintf(tmppath, sizeof tmppath, "%s.comp", msgpath);
		fdin = open(msgpath, O_RDONLY);
		fdout = open(tmppath, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fdin == -1 || fdout == -1)
			goto err;
		if (! compress_file(fdin, fdout))
			goto err;
		close(fdin);
		close(fdout);

		if (rename(tmppath, msgpath) == -1) {
			if (errno == ENOSPC)
				return (0);
			fatal("queue_message_commit: rename");
		}
	}

#if 0
	if (env->sc_queue_flags & QUEUE_ENCRYPT) {

		bsnprintf(tmppath, sizeof tmppath, "%s.crypt", msgpath);
		fdin = open(msgpath, O_RDONLY);
		fdout = open(tmppath, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fdin == -1 || fdout == -1)
			goto err;
		if (! encrypt_file(fdin, fdout))
			goto err;
		close(fdin);
		close(fdout);

		if (rename(tmppath, msgpath) == -1) {
			if (errno == ENOSPC)
				return (0);
			fatal("queue_message_commit: rename");
		}
	}
#endif

	return env->sc_queue->message(QOP_COMMIT, &msgid);

err:
	if (fdin != -1)
		close(fdin);
	if (fdout != -1)
		close(fdout);
	return 0;
}

int
queue_message_corrupt(uint32_t msgid)
{
	return env->sc_queue->message(QOP_CORRUPT, &msgid);
}

int
queue_message_fd_r(uint32_t msgid)
{
	int	fdin, fdout;

	fdin = env->sc_queue->message(QOP_FD_R, &msgid);

#if 0
	if (env->sc_queue_flags & QUEUE_ENCRYPT) {
		fdout = mktmpfile();
		if (! decrypt_file(fdin, fdout))
			goto err;
		close(fdin);
		fdin = fdout;
	}
#endif

	if (env->sc_queue_flags & QUEUE_COMPRESS) {
		fdout = mktmpfile();
		if (! uncompress_file(fdin, fdout))
			goto err;
		close(fdin);
		fdin = fdout;
	}

	return (fdin);

err:
	if (fdin != -1)
		close(fdin);
	if (fdout != -1)
		close(fdout);
	return -1;
}

int
queue_message_fd_rw(uint32_t msgid)
{
	char msgpath[MAXPATHLEN];

	queue_message_incoming_path(msgid, msgpath, sizeof msgpath);
	strlcat(msgpath, PATH_MESSAGE, sizeof(msgpath));

	return open(msgpath, O_RDWR | O_CREAT | O_EXCL, 0600);
}

static int
queue_envelope_dump_buffer(struct envelope *ep, char *evpbuf, size_t evpbufsize)
{
	char		 evpbufcom[sizeof(struct envelope)];
/*	char		 evpbufenc[sizeof(struct envelope)];*/
	char		*evp;
	size_t		 evplen;

	evp = evpbuf;
	evplen = envelope_dump_buffer(ep, evpbuf, evpbufsize);
	if (evplen == 0)
		return (0);

	if (env->sc_queue_flags & QUEUE_COMPRESS) {
		evplen = compress_buffer(evp, evplen, evpbufcom, sizeof evpbufcom);
		if (evplen == 0)
			return (0);
		evp = evpbufcom;
	}

#if 0
	if (env->sc_queue_flags & QUEUE_ENCRYPT) {
		evplen = encrypt_buffer(evp, evplen, evpbufenc, sizeof evpbufenc);
		if (evplen == 0)
			return (0);
		evp = evpbufenc;
	}
#endif

	memmove(evpbuf, evp, evplen);

	return (evplen);
}

static int
queue_envelope_load_buffer(struct envelope *ep, char *evpbuf, size_t evpbufsize)
{
	char		 evpbufcom[sizeof(struct envelope)];
/*	char		 evpbufenc[sizeof(struct envelope)];*/
	char		*evp;
	size_t		 evplen;

	evp = evpbuf;
	evplen = evpbufsize;

#if 0
	if (env->sc_queue_flags & QUEUE_ENCRYPT) {
		evplen = decrypt_buffer(evp, evplen, evpbufenc, sizeof evpbufenc);
		if (evplen == 0)
			return (0);
		evp = evpbufenc;
	}
#endif

	if (env->sc_queue_flags & QUEUE_COMPRESS) {
		evplen = uncompress_buffer(evp, evplen, evpbufcom, sizeof evpbufcom);
		if (evplen == 0)
			return (0);
		evp = evpbufcom;
	}

	return (envelope_load_buffer(ep, evp, evplen));
}

int
queue_envelope_create(struct envelope *ep)
{
	int		 r;
	char		 evpbuf[sizeof(struct envelope)];
	size_t		 evplen;

	ep->creation = time(NULL);
	evplen = queue_envelope_dump_buffer(ep, evpbuf, sizeof evpbuf);
	if (evplen == 0)
		return (0);

	r = env->sc_queue->envelope(QOP_CREATE, &ep->id, evpbuf, evplen);
	if (!r) {
		ep->creation = 0;
		ep->id = 0;
	}
	return (r);
}

int
queue_envelope_delete(struct envelope *ep)
{
	return env->sc_queue->envelope(QOP_DELETE, &ep->id, NULL, 0);
}

int
queue_envelope_load(uint64_t evpid, struct envelope *ep)
{
	const char	*e;
	char		 evpbuf[sizeof(struct envelope)];
	size_t		 evplen;

	ep->id = evpid;
	evplen = env->sc_queue->envelope(QOP_LOAD, &ep->id, evpbuf, sizeof evpbuf);
	if (evplen == 0)
		return (0);
		
	if (queue_envelope_load_buffer(ep, evpbuf, evplen)) {
		if ((e = envelope_validate(ep)) == NULL) {
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
	char	 evpbuf[sizeof(struct envelope)];
	size_t	 evplen;

	evplen = queue_envelope_dump_buffer(ep, evpbuf, sizeof evpbuf);
	if (evplen == 0)
		return (0);

	return env->sc_queue->envelope(QOP_UPDATE, &ep->id, evpbuf, evplen);
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
envelope_validate(struct envelope *ep)
{
	if (ep->version != SMTPD_ENVELOPE_VERSION)
		return "version mismatch";

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
