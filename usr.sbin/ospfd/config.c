/*	$OpenBSD: config.c,v 1.2 2005/01/28 17:53:33 norby Exp $ */

/*
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "log.h"

void	show_db_sum(struct lsa_hdr *);
void	show_neighbor(struct nbr *);
void	show_interface(struct iface *);
void	show_area(struct area *);

extern char *__progname;

void
show_db_sum(struct lsa_hdr *db_sum)
{

	log_debug("        age %d", db_sum->age);
	log_debug("        opts %d", db_sum->opts);
	log_debug("        type %d", db_sum->type);
	log_debug("        ls_id %s", (db_sum->ls_id));
	log_debug("        adv_rtr %s", (db_sum->adv_rtr));
	log_debug("        seq_num 0x%x", db_sum->seq_num);
	log_debug("        chksum 0x%x", db_sum->ls_chksum);
	log_debug("        len %d", db_sum->len);
}

void
show_neighbor(struct nbr *nbr)
{
	struct lsa_entry	*lsa_entry = NULL;

	log_debug("      state: %s", nbr_state_name(nbr->state));
	log_debug("      inactivity timer: ");
	log_debug("      master: %d", nbr->master);
	log_debug("      dd seq num: %d", nbr->dd_seq_num);
	log_debug("      last rx options: %d", nbr->last_rx_options);
	log_debug("      id: %s", inet_ntoa(nbr->id));
	log_debug("      priority: %d", nbr->priority);
	log_debug("      address: %s", inet_ntoa(nbr->addr));
	log_debug("      options: %d", nbr->options);
	log_debug("      dr: %s", inet_ntoa(nbr->dr));
	log_debug("      bdr: %s", inet_ntoa(nbr->bdr));

	log_debug("      ls retrans: ");

	log_debug("      db sum list: ");
	TAILQ_FOREACH(lsa_entry, &nbr->db_sum_list, entry) {
		show_db_sum(lsa_entry->le_lsa);
	}

	log_debug("      ls request: ");
	TAILQ_FOREACH(lsa_entry, &nbr->ls_req_list, entry) {
		show_db_sum(lsa_entry->le_lsa);
	}
}

void
show_interface(struct iface *iface)
{
	struct nbr	*nbr = NULL;

	log_debug("  interface: %s", iface->name);
	log_debug("    type: %s", if_type_name(iface->type));
	log_debug("    state: %s", if_state_name(iface->state));
	log_debug("    address: %s", inet_ntoa(iface->addr));
	log_debug("    mask: %s", inet_ntoa(iface->mask));
	log_debug("    area: %s", inet_ntoa(iface->area->id));
	log_debug("    hello interval: %d", iface->hello_interval);
	log_debug("    dead interval: %d", iface->dead_interval);
	log_debug("    transfer delay: %d", iface->transfer_delay);
	log_debug("    priority: %d", iface->priority);
	log_debug("    hello timer: ");
	log_debug("    wait timer: ");
	log_debug("    neighbor:");

	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		show_neighbor(nbr);
	}

	log_debug("    dr: ");
	log_debug("    bdr: ");
	log_debug("    metric: %d", iface->metric);
	log_debug("    rxmt interval: %d", iface->rxmt_interval);
	log_debug("    auth type: %s", if_auth_name(iface->auth_type));
	if (iface->auth_type == AUTH_TYPE_SIMPLE) {
		log_debug("    auth key: '%s'", iface->auth_key);
	} else {
		log_debug("    auth key:" );
	}

	log_debug("    mtu: %d", iface->mtu);
	log_debug("    fd: %d", iface->fd);
	log_debug("    passive: %d", iface->passive);
	log_debug("    ifindex: %d", iface->ifindex);
}

void
show_area(struct area *area)
{
	struct iface	*iface = NULL;

	log_debug("area: %s", inet_ntoa(area->id));

	LIST_FOREACH(iface, &area->iface_list, entry) {
		show_interface(iface);
	}

	log_debug("  transit: %d", area->transit);
	log_debug("  stub: %d", area->stub);
	log_debug("  stub default cost: %d", area->stub_default_cost);
}

void
show_config(struct ospfd_conf *xconf)
{
	struct area	*area = NULL;

	log_debug("--------------------------------------------------------");
	log_debug("dumping %s configuration", __progname);
	log_debug("--------------------------------------------------------");

	log_debug("router-id: %s", inet_ntoa(xconf->rtr_id));
	log_debug("ospf socket: %d", xconf->ospf_socket);

	LIST_FOREACH(area, &xconf->area_list, entry) {
		show_area(area);
	}
	log_debug("--------------------------------------------------------");
}
