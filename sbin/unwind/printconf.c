/*	$OpenBSD: printconf.c,v 1.11 2019/10/21 07:16:09 florian Exp $	*/

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

const char*	yesno(int);
void		print_forwarder(char *);

const char*
yesno(int flag)
{
	return flag ? "yes" : "no";
}

void
print_forwarder(char *name)
{
	char	*port_pos, *name_pos;

	port_pos = strchr(name, '@');
	name_pos = strchr(name, '#');

	if (port_pos != NULL) {
		*port_pos = '\0';
		if (name_pos != NULL) {
			*name_pos = '\0';
			printf("%s port %s authentication name %s", name,
			    port_pos + 1, name_pos + 1);
			*name_pos = '#';
		} else {
			printf("%s port %s", name, port_pos + 1);
		}
		*port_pos = '@';
	} else if (name_pos != NULL) {
		*name_pos = '\0';
		printf("%s authentication name %s", name, name_pos + 1);
		*name_pos = '#';
	} else
		printf("%s", name);
}

void
print_config(struct uw_conf *conf)
{
	struct uw_forwarder	*uw_forwarder;
	int			 i;

	if (conf->res_pref_len > 0) {
		printf("preference {");
		for (i = 0; i < conf->res_pref_len; i++) {
			printf(" %s", uw_resolver_type_str[conf->res_pref[i]]);
		}
		printf(" }\n");
	}

	if (!SIMPLEQ_EMPTY(&conf->uw_forwarder_list) ||
	    !SIMPLEQ_EMPTY(&conf->uw_dot_forwarder_list)) {
		printf("forwarder {\n");
		SIMPLEQ_FOREACH(uw_forwarder, &conf->uw_forwarder_list, entry) {
			printf("\t");
			print_forwarder(uw_forwarder->name);
			printf("\n");
		}
		SIMPLEQ_FOREACH(uw_forwarder, &conf->uw_dot_forwarder_list,
		    entry) {
			printf("\t");
			print_forwarder(uw_forwarder->name);
			printf(" DoT\n");
		}
		printf("}\n");
	}

	if (conf->captive_portal_host != NULL) {
		printf("captive portal {\n");
		printf("\turl \"http://%s%s\"\n", conf->captive_portal_host,
		    conf->captive_portal_path);
		printf("\texpected status %d\n",
		    conf->captive_portal_expected_status);
		if (conf->captive_portal_expected_response != NULL)
			printf("\texpected response \"%s\"\n",
			    conf->captive_portal_expected_response);
		printf("\tauto %s\n", yesno(conf->captive_portal_auto));
		printf("}\n");
	}

	if (conf->blocklist_file != NULL)
		printf("block list \"%s\"%s\n", conf->blocklist_file,
		    conf->blocklist_log ? " log" : "");
}
