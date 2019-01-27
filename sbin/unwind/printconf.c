/*	$OpenBSD: printconf.c,v 1.3 2019/01/27 12:40:54 florian Exp $	*/

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
	char	*pos;

	pos = strchr(name, '@');

	if (pos != NULL) {
		*pos = '\0';
		printf("%s port %s", name, pos + 1);
		*pos = '@';
	} else
		printf("%s", name);

}

void
print_config(struct unwind_conf *conf)
{
	struct unwind_forwarder	*unwind_forwarder;

	printf("strict %s\n", yesno(conf->unwind_options));

	if (!SIMPLEQ_EMPTY(&conf->unwind_forwarder_list) ||
	    !SIMPLEQ_EMPTY(&conf->unwind_dot_forwarder_list)) {
		printf("forwarder {\n");
		SIMPLEQ_FOREACH(unwind_forwarder, &conf->unwind_forwarder_list,
		    entry) {
			printf("\t");
			print_forwarder(unwind_forwarder->name);
			printf("\n");
		}
		SIMPLEQ_FOREACH(unwind_forwarder,
		    &conf->unwind_dot_forwarder_list, entry) {
			printf("\t");
			print_forwarder(unwind_forwarder->name);
			printf(" DoT\n");
		}
		printf("}\n");
	}
}
