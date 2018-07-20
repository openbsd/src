/*	$OpenBSD: printconf.c,v 1.3 2018/07/20 13:17:02 florian Exp $	*/

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

#include "rad.h"

const char*	yesno(int);
void		print_ra_options(const char*, const struct ra_options_conf*);
void		print_prefix_options(const char*, const struct ra_prefix_conf*);

const char*
yesno(int flag)
{
	return flag ? "yes" : "no";
}

void
print_ra_options(const char *indent, const struct ra_options_conf *ra_options)
{
	printf("%sdefault router %s\n", indent, yesno(ra_options->dfr));
	printf("%shop limit %d\n", indent, ra_options->cur_hl);
	printf("%smanaged address configuration %s\n", indent,
	    yesno(ra_options->m_flag));
	printf("%sother configuration %s\n", indent, yesno(ra_options->o_flag));
	printf("%srouter lifetime %d\n", indent, ra_options->router_lifetime);
	printf("%sreachable time %u\n", indent, ra_options->reachable_time);
	printf("%sretrans timer %u\n", indent, ra_options->retrans_timer);
}

void
print_prefix_options(const char *indent, const struct ra_prefix_conf
    *ra_prefix_conf)
{
	printf("%svalid lifetime %u\n", indent, ra_prefix_conf->vltime);
	printf("%spreferred lifetime %u\n", indent, ra_prefix_conf->pltime);
	printf("%son-link %s\n", indent, yesno(ra_prefix_conf->lflag));
	printf("%sautonomous address-configuration %s\n", indent,
	    yesno(ra_prefix_conf->aflag));
}

void
print_config(struct rad_conf *conf)
{
	struct ra_iface_conf	*iface;
	struct ra_prefix_conf	*prefix;
	struct ra_rdnss_conf	*ra_rdnss;
	struct ra_dnssl_conf	*ra_dnssl;
	char			 buf[INET6_ADDRSTRLEN], *bufp;

	print_ra_options("", &conf->ra_options);
	printf("\n");

	SIMPLEQ_FOREACH(iface, &conf->ra_iface_list, entry) {
		printf("interface %s {\n", iface->name);

		print_ra_options("\t", &iface->ra_options);

		if (iface->autoprefix) {
			printf("\tauto prefix {\n");
			print_prefix_options("\t\t", iface->autoprefix);
			printf("\t}\n");
		} else
			printf("\tno auto prefix\n");

		SIMPLEQ_FOREACH(prefix, &iface->ra_prefix_list, entry) {
			bufp = inet_net_ntop(AF_INET6, &prefix->prefix,
			    prefix->prefixlen, buf, sizeof(buf));
			printf("\tprefix %s {\n", bufp);
			print_prefix_options("\t\t", prefix);
			printf("\t}\n");
		}

		if (!SIMPLEQ_EMPTY(&iface->ra_rdnss_list) ||
		    !SIMPLEQ_EMPTY(&iface->ra_dnssl_list)) {
			printf("\tdns {\n");
			printf("\t\tlifetime %u\n", iface->rdns_lifetime);
			if (!SIMPLEQ_EMPTY(&iface->ra_rdnss_list)) {
				printf("\t\tnameserver {\n");
				SIMPLEQ_FOREACH(ra_rdnss,
				    &iface->ra_rdnss_list, entry) {
					inet_ntop(AF_INET6, &ra_rdnss->rdnss,
					    buf, sizeof(buf));
					printf("\t\t\t%s\n", buf);
				}
				printf("\t\t}\n");
			}
			if (!SIMPLEQ_EMPTY(&iface->ra_dnssl_list)) {
				printf("\t\tsearch {\n");
				SIMPLEQ_FOREACH(ra_dnssl,
				    &iface->ra_dnssl_list, entry)
					printf("\t\t\t%s\n", ra_dnssl->search);
				printf("\t\t}\n");
			}
			printf("\t}\n");
		}

		printf("}\n");
	}
}
