/*
 * Copyright (C) 2000-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: entropy.c,v 1.60.2.3 2002/08/05 06:57:16 marka Exp $ */

/*
 * This is the system depenedent part of the ISC entropy API.
 */

#include <config.h>

#include <sys/param.h>	/* Openserver 5.0.6A and FD_SETSIZE */
#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>

#include <isc/platform.h>
#include <isc/strerror.h>

#ifdef ISC_PLATFORM_NEEDSYSSELECTH
#include <sys/select.h>
#endif

#include "errno2result.h"

/*
 * There is only one variable in the entropy data structures that is not
 * system independent, but pulling the structure that uses it into this file
 * ultimately means pulling several other independent structures here also to
 * resolve their interdependencies.  Thus only the problem variable's type
 * is defined here.
 */
#define FILESOURCE_HANDLE_TYPE	int

#include "../entropy.c"

static unsigned int
get_from_filesource(isc_entropysource_t *source, isc_uint32_t desired) {
	isc_entropy_t *ent = source->ent;
	unsigned char buf[128];
	int fd = source->sources.file.handle;
	ssize_t n, ndesired;
	unsigned int added;

	if (source->bad)
		return (0);

	desired = desired / 8 + (((desired & 0x07) > 0) ? 1 : 0);

	added = 0;
	while (desired > 0) {
		ndesired = ISC_MIN(desired, sizeof(buf));
		n = read(fd, buf, ndesired);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR)
				goto out;
			close(fd);
			source->bad = ISC_TRUE;
			goto out;
		}
		if (n == 0) {
			close(fd);
			source->bad = ISC_TRUE;
			goto out;
		}

		entropypool_adddata(ent, buf, n, n * 8);
		added += n * 8;
		desired -= n;
	}

 out:
	return (added);
}

/*
 * Poll each source, trying to get data from it to stuff into the entropy
 * pool.
 */
static void
fillpool(isc_entropy_t *ent, unsigned int desired, isc_boolean_t blocking) {
	unsigned int added;
	unsigned int remaining;
	unsigned int needed;
	unsigned int nsource;
	isc_entropysource_t *source;

	REQUIRE(VALID_ENTROPY(ent));

	needed = desired;

	/*
	 * This logic is a little strange, so an explanation is in order.
	 *
	 * If needed is 0, it means we are being asked to "fill to whatever
	 * we think is best."  This means that if we have at least a
	 * partially full pool (say, > 1/4th of the pool) we probably don't
	 * need to add anything.
	 *
	 * Also, we will check to see if the "pseudo" count is too high.
	 * If it is, try to mix in better data.  Too high is currently
	 * defined as 1/4th of the pool.
	 *
	 * Next, if we are asked to add a specific bit of entropy, make
	 * certain that we will do so.  Clamp how much we try to add to
	 * (DIGEST_SIZE * 8 < needed < POOLBITS - entropy).
	 *
	 * Note that if we are in a blocking mode, we will only try to
	 * get as much data as we need, not as much as we might want
	 * to build up.
	 */
	if (needed == 0) {
		REQUIRE(!blocking);

		if ((ent->pool.entropy >= RND_POOLBITS / 4)
		    && (ent->pool.pseudo <= RND_POOLBITS / 4))
			return;

		needed = THRESHOLD_BITS * 4;
	} else {
		needed = ISC_MAX(needed, THRESHOLD_BITS);
		needed = ISC_MIN(needed, RND_POOLBITS);
	}

	/*
	 * In any case, clamp how much we need to how much we can add.
	 */
	needed = ISC_MIN(needed, RND_POOLBITS - ent->pool.entropy);

	/*
	 * But wait!  If we're not yet initialized, we need at least
	 *	THRESHOLD_BITS
	 * of randomness.
	 */
	if (ent->initialized < THRESHOLD_BITS)
		needed = ISC_MAX(needed, THRESHOLD_BITS - ent->initialized);

	/*
	 * Poll each file source to see if we can read anything useful from
	 * it.  XXXMLG When where are multiple sources, we should keep a
	 * record of which one we last used so we can start from it (or the
	 * next one) to avoid letting some sources build up entropy while
	 * others are always drained.
	 */

	added = 0;
	remaining = needed;
	if (ent->nextsource == NULL) {
		ent->nextsource = ISC_LIST_HEAD(ent->sources);
		if (ent->nextsource == NULL)
			return;
	}
	source = ent->nextsource;
 again_file:
	for (nsource = 0 ; nsource < ent->nsources ; nsource++) {
		unsigned int got;

		if (remaining == 0)
			break;

		got = 0;

		if (source->type == ENTROPY_SOURCETYPE_FILE)
			got = get_from_filesource(source, remaining);

		added += got;

		remaining -= ISC_MIN(remaining, got);

		source = ISC_LIST_NEXT(source, link);
		if (source == NULL)
			source = ISC_LIST_HEAD(ent->sources);
	}
	ent->nextsource = source;

	if (blocking && remaining != 0) {
		int fds;

		fds = wait_for_sources(ent);
		if (fds > 0)
			goto again_file;
	}

	/*
	 * Here, if there are bits remaining to be had and we can block,
	 * check to see if we have a callback source.  If so, call them.
	 */
	source = ISC_LIST_HEAD(ent->sources);
	while ((remaining != 0) && (source != NULL)) {
		unsigned int got;

		got = 0;

		if (source->type == ENTROPY_SOURCETYPE_CALLBACK)
			got = get_from_callback(source, remaining, blocking);

		added += got;
		remaining -= ISC_MIN(remaining, got);

		if (added >= needed)
			break;

		source = ISC_LIST_NEXT(source, link);
	}

	/*
	 * Mark as initialized if we've added enough data.
	 */
	if (ent->initialized < THRESHOLD_BITS)
		ent->initialized += added;
}

static int
wait_for_sources(isc_entropy_t *ent) {
	isc_entropysource_t *source;
	int maxfd, fd;
	int cc;
	fd_set reads;

	maxfd = -1;
	FD_ZERO(&reads);

	source = ISC_LIST_HEAD(ent->sources);
	while (source != NULL) {
		if (source->type == ENTROPY_SOURCETYPE_FILE) {
			fd = source->sources.file.handle;
			if (fd >= 0) {
				maxfd = ISC_MAX(maxfd, fd);
				FD_SET(fd, &reads);
			}
		}
		source = ISC_LIST_NEXT(source, link);
	}

	if (maxfd < 0)
		return (-1);

	cc = select(maxfd + 1, &reads, NULL, NULL, NULL);
	if (cc < 0)
		return (-1);

	return (cc);
}

static void
destroyfilesource(isc_entropyfilesource_t *source) {
	close(source->handle);
}

/*
 * Make a fd non-blocking
 */
static isc_result_t
make_nonblock(int fd) {
	int ret;
	int flags;
	char strbuf[ISC_STRERRORSIZE];

	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);

	if (ret == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "fcntl(%d, F_SETFL, %d): %s",
				 fd, flags, strbuf);

		return (ISC_R_UNEXPECTED);
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_entropy_createfilesource(isc_entropy_t *ent, const char *fname) {
	int fd;
	isc_result_t ret;
	isc_entropysource_t *source;

	REQUIRE(VALID_ENTROPY(ent));
	REQUIRE(fname != NULL);

	LOCK(&ent->lock);

	source = NULL;

	fd = open(fname, O_RDONLY | O_NONBLOCK, 0);
	if (fd < 0) {
		ret = isc__errno2result(errno);
		goto errout;
	}
	ret = make_nonblock(fd);
	if (ret != ISC_R_SUCCESS)
		goto closefd;

	source = isc_mem_get(ent->mctx, sizeof(isc_entropysource_t));
	if (source == NULL) {
		ret = ISC_R_NOMEMORY;
		goto closefd;
	}

	/*
	 * From here down, no failures can occur.
	 */
	source->magic = SOURCE_MAGIC;
	source->type = ENTROPY_SOURCETYPE_FILE;
	source->ent = ent;
	source->total = 0;
	source->bad = ISC_FALSE;
	memset(source->name, 0, sizeof(source->name));
	ISC_LINK_INIT(source, link);
	source->sources.file.handle = fd;

	/*
	 * Hook it into the entropy system.
	 */
	ISC_LIST_APPEND(ent->sources, source, link);
	ent->nsources++;

	UNLOCK(&ent->lock);
	return (ISC_R_SUCCESS);

 closefd:
	close(fd);

 errout:
	if (source != NULL)
		isc_mem_put(ent->mctx, source, sizeof(isc_entropysource_t));

	UNLOCK(&ent->lock);

	return (ret);
}
