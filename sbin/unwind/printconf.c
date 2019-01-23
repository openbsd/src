/*	$OpenBSD: printconf.c,v 1.1 2019/01/23 13:11:00 florian Exp $	*/

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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>

#include "unwind.h"

const char*	yesno(int);

const char*
yesno(int flag)
{
	return flag ? "yes" : "no";
}

void
print_config(struct unwind_conf *conf)
{
	struct unwind_forwarder	*unwind_forwarder;

	printf("strict %s\n", yesno(conf->unwind_options));

	if (!SIMPLEQ_EMPTY(&conf->unwind_forwarder_list)) {
		printf("forwarder {\n");
		SIMPLEQ_FOREACH(unwind_forwarder, &conf->unwind_forwarder_list,
		    entry)
			printf("\t%s\n", unwind_forwarder->name);
		printf("}\n");
	}
}
