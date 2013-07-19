/*	$OpenBSD: queue_backend.c,v 1.45 2013/07/19 11:14:08 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
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
extern struct queue_backend	queue_backend_null;
extern struct queue_backend	queue_backend_ram;

static struct queue_backend	*backend;

#ifdef QUEUE_PROFILING

static struct {
	struct timespec	 t0;
	const char	*name;
} profile;

static inline void profile_enter(const char *name)
{
	if ((profiling & PROFILE_QUEUE) == 0)
		return;

	profile.name = name;
	clock_gettime(CLOCK_MONOTONIC, &profile.t0);
}

static inline void profile_leave(void)
{
	struct timespec	 t1, dt;

	if ((profiling & PROFILE_QUEUE) == 0)
		return;

	clock_gettime(CLOCK_MONOTONIC, &t1);
	timespecsub(&t1, &profile.t0, &dt);
	log_debug("profile-queue: %s %li.%06li", profile.name,
	    dt.tv_sec * 1000000 + dt.tv_nsec / 1000000,
	    dt.tv_nsec % 1000000);
}
#else
#define profile_enter(x)	do {} while (0)
#define profile_leave()		do {} while (0)
#endif

int
queue_message_incoming_path(uint32_t msgid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%08x",
	    PATH_INCOMING,
	    msgid);
}

int
queue_init(const char *name, int server)
{
	struct passwd	*pwq;
	int		 r;

	pwq = getpwnam(SMTPD_QUEUE_USER);
	if (pwq == NULL)
		pwq = getpwnam(SMTPD_USER);
	if (pwq == NULL)
		errx(1, "unknown user %s", SMTPD_USER);

	if (!strcmp(name, "fs"))
		backend = &queue_backend_fs;
	if (!strcmp(name, "null"))
		backend = &queue_backend_null;
	if (!strcmp(name, "ram"))
		backend = &queue_backend_ram;

	if (backend == NULL) {
		log_warn("could not find queue backend \"%s\"", name);
		return (0);
	}

	if (server) {
		if (ckdir(PATH_SPOOL, 0711, 0, 0, 1) == 0)
			errx(1, "error in spool directory setup");
		if (ckdir(PATH_SPOOL PATH_OFFLINE, 01777, 0, 0, 1) == 0)
			errx(1, "error in offline directory setup");
		if (ckdir(PATH_SPOOL PATH_PURGE, 0700, pwq->pw_uid, 0, 1) == 0)
			errx(1, "error in purge directory setup");

		mvpurge(PATH_SPOOL PATH_TEMPORARY, PATH_SPOOL PATH_PURGE);

		if (ckdir(PATH_SPOOL PATH_TEMPORARY, 0700, pwq->pw_uid, 0, 1) == 0)
			errx(1, "error in purge directory setup");
	}

	r = backend->init(pwq, server);

	log_trace(TRACE_QUEUE, "queue-backend: queue_init(%i) -> %i", server, r);

	return (r);
}

int
queue_message_create(uint32_t *msgid)
{
	int	r;

	profile_enter("queue_message_create");
	r = backend->message(QOP_CREATE, msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_create() -> %i (%08"PRIx32")",
	    r, *msgid);

	return (r);
}

int
queue_message_delete(uint32_t msgid)
{
	int	r;

	profile_enter("queue_message_delete");
	r = backend->message(QOP_DELETE, &msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_delete(%08"PRIx32") -> %i", msgid, r);

	return (r);
}

int
queue_message_commit(uint32_t msgid)
{
	int	r;
	char	msgpath[MAXPATHLEN];
	char	tmppath[MAXPATHLEN];
	FILE	*ifp = NULL;
	FILE	*ofp = NULL;

	profile_enter("queue_message_commit");
	queue_message_incoming_path(msgid, msgpath, sizeof msgpath);
	strlcat(msgpath, PATH_MESSAGE, sizeof(msgpath));

	if (env->sc_queue_flags & QUEUE_COMPRESSION) {

		bsnprintf(tmppath, sizeof tmppath, "%s.comp", msgpath);
		ifp = fopen(msgpath, "r");
		ofp = fopen(tmppath, "w+");
		if (ifp == NULL || ofp == NULL)
			goto err;
		if (! compress_file(ifp, ofp))
			goto err;
		fclose(ifp);
		fclose(ofp);
		ifp = NULL;
		ofp = NULL;

		if (rename(tmppath, msgpath) == -1) {
			if (errno == ENOSPC)
				return (0);
			fatal("queue_message_commit: rename");
		}
	}

	r = backend->message(QOP_COMMIT, &msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_commit(%08"PRIx32") -> %i",
	    msgid, r);

	return (r);

err:
	if (ifp)
		fclose(ifp);
	if (ofp)
		fclose(ofp);
	return 0;
}

int
queue_message_corrupt(uint32_t msgid)
{
	int	r;

	profile_enter("queue_message_corrupt");
	r = backend->message(QOP_CORRUPT, &msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_corrupt(%08"PRIx32") -> %i", msgid, r);

	return (r);
}

int
queue_message_fd_r(uint32_t msgid)
{
	int	fdin = -1, fdout = -1, fd = -1;
	FILE	*ifp = NULL;
	FILE	*ofp = NULL;

	profile_enter("queue_message_fd_r");
	fdin = backend->message(QOP_FD_R, &msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_fd_r(%08"PRIx32") -> %i", msgid, fdin);

	if (fdin == -1)
		return (-1);

	if (env->sc_queue_flags & QUEUE_COMPRESSION) {
		if ((fdout = mktmpfile()) == -1)
			goto err;
		if ((fd = dup(fdout)) == -1)
			goto err;
		if ((ifp = fdopen(fdin, "r")) == NULL)
			goto err;
		fdin = fd;
		fd = -1;
		if ((ofp = fdopen(fdout, "w+")) == NULL)
			goto err;

		if (! uncompress_file(ifp, ofp))
			goto err;

		fclose(ifp);
		fclose(ofp);
		lseek(fdin, SEEK_SET, 0);
	}

	return (fdin);

err:
	if (fd != -1)
		close(fd);
	if (fdin != -1)
		close(fdin);
	if (fdout != -1)
		close(fdout);
	if (ifp)
		fclose(ifp);
	if (ofp)
		fclose(ofp);
	return -1;
}

int
queue_message_fd_rw(uint32_t msgid)
{
	int	r;

	profile_enter("queue_message_fd_rw");
	r = backend->message(QOP_FD_RW, &msgid);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_message_fd_rw(%08"PRIx32") -> %i", msgid, r);

	return (r);
}

static int
queue_envelope_dump_buffer(struct envelope *ep, char *evpbuf, size_t evpbufsize)
{
	char   *evp;
	size_t	evplen;
	size_t	complen;
	char	compbuf[sizeof(struct envelope)];

	evp = evpbuf;
	evplen = envelope_dump_buffer(ep, evpbuf, evpbufsize);
	if (evplen == 0)
		return (0);

	if (env->sc_queue_flags & QUEUE_COMPRESSION) {
		complen = compress_chunk(evp, evplen, compbuf, sizeof compbuf);
		if (complen == 0)
			return (0);
		evp = compbuf;
		evplen = complen;
	}

	memmove(evpbuf, evp, evplen);

	return (evplen);
}

static int
queue_envelope_load_buffer(struct envelope *ep, char *evpbuf, size_t evpbufsize)
{
	char		*evp;
	size_t		 evplen;
	char		 compbuf[sizeof(struct envelope)];
	size_t		 complen;

	evp = evpbuf;
	evplen = evpbufsize;

	if (env->sc_queue_flags & QUEUE_COMPRESSION) {
		complen = uncompress_chunk(evp, evplen, compbuf, sizeof compbuf);
		if (complen == 0)
			return (0);
		evp = compbuf;
		evplen = complen;
	}

	return (envelope_load_buffer(ep, evp, evplen));
}

int
queue_envelope_create(struct envelope *ep)
{
	int		 r;
	char		 evpbuf[sizeof(struct envelope)];
	size_t		 evplen;
	uint64_t	 evpid;

	ep->creation = time(NULL);
	evplen = queue_envelope_dump_buffer(ep, evpbuf, sizeof evpbuf);
	if (evplen == 0)
		return (0);

	evpid = ep->id;

	profile_enter("queue_envelope_create");
	r = backend->envelope(QOP_CREATE, &ep->id, evpbuf, evplen);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_envelope_create(%016"PRIx64", %zu) -> %i (%016"PRIx64")",
	    evpid, evplen, r, ep->id);

	if (!r) {
		ep->creation = 0;
		ep->id = 0;
	}

	return (r);
}

int
queue_envelope_delete(uint64_t evpid)
{
	int	r;

	profile_enter("queue_envelope_delete");
	r = backend->envelope(QOP_DELETE, &evpid, NULL, 0);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_envelope_delete(%016"PRIx64") -> %i",
	    evpid, r);

	return (r);
}

int
queue_envelope_load(uint64_t evpid, struct envelope *ep)
{
	const char	*e;
	char		 evpbuf[sizeof(struct envelope)];
	size_t		 evplen;

	ep->id = evpid;
	profile_enter("queue_envelope_load");
	evplen = backend->envelope(QOP_LOAD, &ep->id, evpbuf, sizeof evpbuf);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_envelope_load(%016"PRIx64") -> %zu",
	    evpid, evplen);

	if (evplen == 0)
		return (0);

	if (queue_envelope_load_buffer(ep, evpbuf, evplen)) {
		if ((e = envelope_validate(ep)) == NULL) {
			ep->id = evpid;
			return (1);
		}
		log_debug("debug: invalid envelope %016" PRIx64 ": %s",
		    ep->id, e);
	}

	(void)queue_message_corrupt(evpid_to_msgid(evpid));
	return (0);
}

int
queue_envelope_update(struct envelope *ep)
{
	char	evpbuf[sizeof(struct envelope)];
	size_t	evplen;
	int	r;

	evplen = queue_envelope_dump_buffer(ep, evpbuf, sizeof evpbuf);
	if (evplen == 0)
		return (0);

	profile_enter("queue_envelope_update");
	r = backend->envelope(QOP_UPDATE, &ep->id, evpbuf, evplen);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_envelope_update(%016"PRIx64") -> %i",
	    ep->id, r);

	return (r);
}

int
queue_envelope_walk(struct envelope *ep)
{
	const char	*e;
	uint64_t	 evpid;
	char		 evpbuf[sizeof(struct envelope)];
	int		 r;

	profile_enter("queue_envelope_walk");
	r = backend->envelope(QOP_WALK, &evpid, evpbuf, sizeof evpbuf);
	profile_leave();

	log_trace(TRACE_QUEUE,
	    "queue-backend: queue_envelope_walk() -> %i (%016"PRIx64")",
	    r, evpid);

	if (r == -1)
		return (r);

	if (r && queue_envelope_load_buffer(ep, evpbuf, (size_t)r)) {
		if ((e = envelope_validate(ep)) == NULL) {
			ep->id = evpid;
			return (1);
		}
		log_debug("debug: invalid envelope %016" PRIx64 ": %s",
		    ep->id, e);
	}

	(void)queue_message_corrupt(evpid_to_msgid(evpid));
	return (0);
}

uint32_t
queue_generate_msgid(void)
{
	uint32_t msgid;

	while ((msgid = arc4random_uniform(0xffffffff)) == 0)
		;

	return msgid;
}

uint64_t
queue_generate_evpid(uint32_t msgid)
{
	uint32_t rnd;
	uint64_t evpid;

	while ((rnd = arc4random_uniform(0xffffffff)) == 0)
		;

	evpid = msgid;
	evpid <<= 32;
	evpid |= rnd;

	return evpid;
}

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
