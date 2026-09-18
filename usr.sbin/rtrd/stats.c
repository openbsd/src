/*	$OpenBSD: stats.c,v 1.3 2026/09/18 04:55:39 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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
#include <sys/tree.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"
#include "version.h"

struct global_stats global_stats;

/* 1 on fail */
int
init_stats(void)
{
	char hostname[HOSTNAME_SIZE], *node, *domain, *c;

	memset(&global_stats, 0, sizeof(struct global_stats));

	if (gethostname(hostname, sizeof hostname) != 0)
		return 1;
	c = strchr(hostname, '.');
	if (c) {
		*c = '\0';
		domain = c + 1;
	} else {
		domain = "";
	}
	node = hostname;

	snprintf(global_stats.nodename, NODENAME_SIZE, "%s", node);
	snprintf(global_stats.domainname, DOMAINNAME_SIZE, "%s", domain);
	snprintf(global_stats.release, RELEASE_SIZE, "%s", RTRD_VERSION);
	return 0;
}
