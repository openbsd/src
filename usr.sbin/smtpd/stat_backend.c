/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "smtpd.h"

struct stat_backend	stat_backend_ramstat;
struct stat_backend	stat_backend_sqlite;

struct stat_backend *
stat_backend_lookup(const char *name)
{
	if (!strcmp(name, "ram"))
		return &stat_backend_ramstat;

	if (!strcmp(name, "sqlite"))
		return &stat_backend_sqlite;

	return (NULL);
}

void
stat_increment(const char *name)
{
	char	key[STAT_KEY_SIZE];

	if (strlcpy(key, name, sizeof key) >= sizeof key)
		log_warn("stat_increment: truncated key '%s', ignored", name);

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_INCREMENT, 0, 0, -1, key, sizeof key);
}

void
stat_decrement(const char *name)
{
	char	key[STAT_KEY_SIZE];

	if (strlcpy(key, name, sizeof key) >= sizeof key)
		log_warn("stat_increment: truncated key '%s', ignored", name);

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_DECREMENT, 0, 0, -1, key, sizeof key);
}

void
stat_set(const char *name, size_t value)
{
	struct stat_kv	kv;

	bzero(&kv, sizeof kv);
	if (strlcpy(kv.key, name, sizeof kv.key) >= sizeof kv.key)
		log_warn("stat_increment: truncated key '%s', ignored", name);
	kv.val = value;

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_SET, 0, 0, -1, &kv, sizeof kv);
}
