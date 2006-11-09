/*	$OpenBSD: printconf.c,v 1.2 2006/11/09 04:06:09 joel Exp $ */

/*
 * Copyright (c) 2004, 2005, 2006 Esben Norby <norby@openbsd.org>
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

#include "rip.h"
#include "ripd.h"
#include "ripe.h"

void	 print_mainconf(struct ripd_conf *);
void	 print_iface(struct iface *);

void
print_mainconf(struct ripd_conf *conf)
{
	if (conf->flags & RIPD_FLAG_NO_FIB_UPDATE)
		printf("fib-update no\n");
	else
		printf("fib-update yes\n");

	if (conf->redistribute_flags & REDISTRIBUTE_STATIC)
		printf("redistribute static\n");
	else if (conf->redistribute_flags & REDISTRIBUTE_CONNECTED)
		printf("redistribute connected\n");
	else if (conf->redistribute_flags & REDISTRIBUTE_DEFAULT)
		printf("redistribute default\n");
	else
		printf("redistribute none\n");

	if (conf->options & OPT_SPLIT_HORIZON)
		printf("split-horizon default\n");
	else if (conf->options & OPT_SPLIT_POISONED)
		printf("split-horizon poisoned\n");
	else
		printf("split-horizon none\n");

	if (conf->options & OPT_TRIGGERED_UPDATES)
		printf("triggered-updates yes\n");
	else
		printf("triggered-updates no\n");
}

void
print_iface(struct iface *iface)
{
	printf("interface %s {\n", iface->name);

	if (iface->passive)
		printf("\tpassive\n");

	printf("\tcost %d\n", iface->cost);

	printf("\tauth-type %s\n", if_auth_name(iface->auth_type));
	switch (iface->auth_type) {
	case AUTH_NONE:
		break;
	case AUTH_SIMPLE:
		printf("\tauth-key XXXXXX\n");
		break;
	case AUTH_CRYPT:
		break;
	default:
		printf("\tunknown auth type!\n");
		break;
	}

	printf("}\n");
}

void
print_config(struct ripd_conf *conf)
{
	struct iface	*iface;

	printf("\n");
	print_mainconf(conf);
	printf("\n");

	LIST_FOREACH(iface, &conf->iface_list, entry) {
		printf("ooo\n");
		print_iface(iface);
	}
	printf("\n");

}
