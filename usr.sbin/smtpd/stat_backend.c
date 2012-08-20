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
	size_t	len;

	if ((len = strlcpy(key, name, sizeof key)) >= sizeof key) {
		len = sizeof(key) - 1;
		log_warn("stat_increment: truncated key '%s', ignored", name);
	}

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_INCREMENT, 0, 0, -1, key, len + 1);
}

void
stat_decrement(const char *name)
{
	char	key[STAT_KEY_SIZE];
	size_t	len;

	if ((len = strlcpy(key, name, sizeof key)) >= sizeof key) {
		len = sizeof(key) - 1;
		log_warn("stat_increment: truncated key '%s', ignored", name);
	}

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_DECREMENT, 0, 0, -1, key, len + 1);
}

void
stat_set(const char *name, size_t value)
{
	char	*s, buf[STAT_KEY_SIZE + sizeof (value)];
	size_t	 len;

	memmove(buf, &value, sizeof value);
	s = buf + sizeof value;
	if ((len = strlcpy(s, name, STAT_KEY_SIZE)) >= STAT_KEY_SIZE) {
		len = STAT_KEY_SIZE - 1;
		log_warn("stat_increment: truncated key '%s', ignored", name);
	}

	imsg_compose_event(env->sc_ievs[PROC_CONTROL],
	    IMSG_STAT_SET, 0, 0, -1, buf, sizeof (value) + len + 1);
}
