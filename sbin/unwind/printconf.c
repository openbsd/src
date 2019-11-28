/*	$OpenBSD: printconf.c,v 1.15 2019/11/28 10:02:44 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <string.h>

#include "unwind.h"

void
print_config(struct uw_conf *conf)
{
	struct uw_forwarder	*uw_forwarder;
	int			 i;

	if (conf->res_pref.len > 0) {
		printf("preference {");
		for (i = 0; i < conf->res_pref.len; i++) {
			printf(" %s",
			    uw_resolver_type_str[conf->res_pref.types[i]]);
		}
		printf(" }\n");
	}

	if (!TAILQ_EMPTY(&conf->uw_forwarder_list) ||
	    !TAILQ_EMPTY(&conf->uw_dot_forwarder_list)) {
		printf("forwarder {\n");
		TAILQ_FOREACH(uw_forwarder, &conf->uw_forwarder_list, entry) {
			printf("\t");
			printf("%s", uw_forwarder->ip);
			if (uw_forwarder->port != 53)
				printf(" port %d", uw_forwarder->port);
			printf("\n");
		}
		TAILQ_FOREACH(uw_forwarder, &conf->uw_dot_forwarder_list,
		    entry) {
			printf("\t");
			printf("%s", uw_forwarder->ip);
			if (uw_forwarder->port != 853)
				printf(" port %d", uw_forwarder->port);
			if (uw_forwarder->auth_name[0] != '\0')
				printf(" authentication name %s",
				    uw_forwarder->auth_name);
			printf(" DoT\n");
		}
		printf("}\n");
	}

	if (conf->blocklist_file != NULL)
		printf("block list \"%s\"%s\n", conf->blocklist_file,
		    conf->blocklist_log ? " log" : "");
}
