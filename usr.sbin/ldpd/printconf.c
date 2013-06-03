/*	$OpenBSD: printconf.c,v 1.6 2013/06/03 16:58:14 claudio Exp $ */

/*
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>

#include "ldp.h"
#include "ldpd.h"
#include "ldpe.h"

void	print_mainconf(struct ldpd_conf *);
void	print_iface(struct iface *);

void
print_mainconf(struct ldpd_conf *conf)
{
	printf("router-id %s\n\n", inet_ntoa(conf->rtr_id));

	if (conf->flags & LDPD_FLAG_NO_FIB_UPDATE)
		printf("fib-update no\n");
	else
		printf("fib-update yes\n");

	if (conf->mode & MODE_DIST_INDEPENDENT)
		printf("distribution independent\n");
	else
		printf("distribution ordered\n");

	if (conf->mode & MODE_RET_LIBERAL)
		printf("retention liberal\n");
	else
		printf("retention conservative\n");

	if (conf->mode & MODE_ADV_ONDEMAND)
		printf("advertisement ondemand\n");
	else
		printf("advertisement unsolicited\n");

	printf("keepalive %u\n", conf->keepalive);
}

void
print_iface(struct iface *iface)
{
	printf("\ninterface %s {\n", iface->name);
	printf("\tholdtime %d\n", iface->holdtime);
	printf("\thello-interval %d\n", iface->hello_interval);
	printf("}\n");
}

void
print_config(struct ldpd_conf *conf)
{
	struct iface	*iface;

	print_mainconf(conf);
	printf("\n");

	LIST_FOREACH(iface, &conf->iface_list, entry) {
		print_iface(iface);
	}
}
