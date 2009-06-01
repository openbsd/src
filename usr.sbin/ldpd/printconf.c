/*	$OpenBSD: printconf.c,v 1.1 2009/06/01 20:59:45 michele Exp $ */

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
void	print_rtlabel(struct ldpd_conf *);
void	print_iface(struct iface *);

void
print_mainconf(struct ldpd_conf *conf)
{
	printf("router-id %s\n\n", inet_ntoa(conf->rtr_id));

	if (conf->mode & MODE_DIST_INDEPENDENT)
		printf("Distribution: Independent\n");
	else
		printf("Distribution: Ordered\n");

	if (conf->mode & MODE_RET_LIBERAL)
		printf("Retention: Liberal\n");
	else
		printf("Retention: Conservative\n");

	if (conf->mode & MODE_ADV_ONDEMAND)
		printf("Advertisement: On demand\n");
	else
		printf("Advertisement: Unsolicited\n");
}

void
print_rtlabel(struct ldpd_conf *conf)
{
	struct n2id_label	*label;

	TAILQ_FOREACH(label, &rt_labels, entry)
		if (label->ext_tag)
			printf("rtlabel \"%s\" external-tag %u\n",
			    label->name, label->ext_tag);
}

void
print_iface(struct iface *iface)
{

	printf("\tinterface %s: %s {\n", iface->name, inet_ntoa(iface->addr));

	printf("\t\tholdtime %d\n", iface->holdtime);
	printf("\t\thello-interval %d\n", iface->hello_interval);

	if (iface->passive)
		printf("\t\tpassive\n");

	printf("\t}\n");
}

void
print_config(struct ldpd_conf *conf)
{
	struct iface	*iface;

	printf("\n");
	print_mainconf(conf);
	printf("\n");

	LIST_FOREACH(iface, &conf->iface_list, entry) {
		print_iface(iface);
	}
}
