/*	$OpenBSD: ospfctl.c,v 1.3 2005/02/02 18:52:32 henning Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "parser.h"
#include "log.h"

__dead void	 usage(void);
int		 show_summary_msg(struct imsg *, int);
int		 show_interface_msg(struct imsg *);
void		 print_baudrate(u_long);
const char	*print_if_type(enum iface_type type);
const char	*print_if_state(int);
const char	*print_nbr_state(int);
const char	*print_link(int);
const char	*fmt_timeframe(time_t t);
const char	*fmt_timeframe_core(time_t t);
const char	*log_id(u_int32_t );
const char	*log_adv_rtr(u_int32_t);
u_int8_t	 mask2prefixlen(struct in_addr);
void		 show_database_head(struct in_addr, u_int8_t);
int		 show_database_msg(struct imsg *);
int		 show_nbr_msg(struct imsg *);
const char	*print_ospf_options(u_int8_t);
int		 show_nbr_detail_msg(struct imsg *);

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s <command> [arg [...]]\n", __progname);
	exit(1);
}

void
imsg_event_add(struct imsgbuf *i)
{
}

int
main(int argc, char *argv[])
{ 
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	unsigned int		 ifidx = 0;
	int			 ctl_sock;
	int			 nodescr = 0;
	int			 done = 0;
	int			 n;

	/* parse options */
	if ((res = parse(argc - 1, argv + 1)) == NULL)
		exit(1);

	/* connect to ospfd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, OSPFD_SOCKET, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", OSPFD_SOCKET);

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf, ctl_sock, NULL);
	done = 0;

	/* process user request */
	switch (res->action) {
	case NONE:
		usage();
		/* not reached */
	case SHOW:
	case SHOW_SUMMARY:
		break;
	case SHOW_IFACE:
		if (*res->ifname) {
			ifidx = if_nametoindex(res->ifname);
			if (ifidx == 0)
				errx(1, "no such interface %s", res->ifname);
		}
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE, 0, 0, -1,
		    &ifidx, sizeof(ifidx));
		break;
	case SHOW_NBR:
		printf("%-15s %-3s %-17s %-9s %-15s %s\n",
		    "ID", "Pri", "State", "DeadTime", "Address", "Interface");
	case SHOW_NBR_DTAIL:
		imsg_compose(ibuf, IMSG_CTL_SHOW_NBR, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DB:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DATABASE, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBBYAREA:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DATABASE, 0, 0, -1,
		    &res->addr, sizeof(res->addr));
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		done = 1;
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (res->action) {
			case SHOW:
			case SHOW_SUMMARY:
				done = show_summary_msg(&imsg, nodescr);
				break;
			case SHOW_IFACE:
				done = show_interface_msg(&imsg);
				break;
			case SHOW_NBR:
				done = show_nbr_msg(&imsg);
				break;
			case SHOW_NBR_DTAIL:
				done = show_nbr_detail_msg(&imsg);
				break;
			case SHOW_DB:
			case SHOW_DBBYAREA:
				done = show_database_msg(&imsg);
				break;
			case NONE:
			case RELOAD:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);
	free(ibuf);

	return (0);
}

int
show_summary_msg(struct imsg *imsg, int nodescr)
{

	return (0);
}

int
show_interface_msg(struct imsg *imsg)
{
	struct ctl_iface	*iface;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE:
		iface = imsg->data;
		printf("\n");
		printf("Interface %s is %d, line protocol is %s\n",
		    iface->name, iface->linkstate, print_link(iface->flags));
		printf("  Internet address %s/%d, ",
		    inet_ntoa(iface->addr),
		    mask2prefixlen(iface->mask));
		printf("Area %s\n", inet_ntoa(iface->area));
		printf("  Router ID %s, network type %s, cost: %d\n", 
		    inet_ntoa(iface->rtr_id),
		    print_if_type(iface->type), iface->metric);
		printf("  Transmit delay is %d sec, state %s, priority %d\n",
		    iface->transfer_delay, print_if_state(iface->state),
		    iface->priority);
		printf("  Designated Router (ID) %s, ",
		    inet_ntoa(iface->dr_id));
		printf("interface address %s\n", inet_ntoa(iface->dr_addr));
		printf("  Backup Designated Router (ID) %s, ",
		    inet_ntoa(iface->bdr_id));
		printf("interface address %s\n", inet_ntoa(iface->bdr_addr));
		printf("  Timer intervals configured, "
		    "hello %d, dead %d, wait %d, retransmit %d\n",
		     iface->hello_interval, iface->dead_interval,
		     iface->dead_interval, iface->rxmt_interval);
		if (iface->hello_timer < 0) 
			printf("    Hello timer not running\n");
		else
			printf("    Hello timer due in %s\n",
			    fmt_timeframe_core(iface->hello_timer));
		printf("  Neighbor count is %d, adjacent neighbor count is "
		    "%d\n", iface->nbr_cnt, iface->adj_cnt);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0); 
}

const char *
print_if_type(enum iface_type type)
{
	switch (type) {
	case IF_TYPE_POINTOPOINT:
		return ("POINTOPOINT");
	case IF_TYPE_BROADCAST:
		return ("BROADCAST");
	case IF_TYPE_NBMA:
		return ("NBMA");
	case IF_TYPE_POINTOMULTIPOINT:
		return ("POINTOMULTIPOINT");
	case IF_TYPE_VIRTUALLINK:
		return ("VIRTUALLINK");
	default:
		return ("UNKNOWN");
	}
}

const char *
print_if_state(int state)
{
	switch (state) {
	case IF_STA_DOWN:
		return ("DOWN");
	case IF_STA_LOOPBACK:
		return ("LOOPBACK");
	case IF_STA_WAITING:
		return ("WAITING");
	case IF_STA_POINTTOPOINT:
		return ("P2P");
	case IF_STA_DROTHER:
		return ("DROTHER");
	case IF_STA_BACKUP:
		return ("BACKUP");
	case IF_STA_DR:
		return ("DR");
	default:
		return ("UNKNOWN");
	}
}

const char *
print_nbr_state(int state)
{
	switch (state) {
	case NBR_STA_DOWN:
		return ("DOWN");
	case NBR_STA_ATTEMPT:
		return ("ATTEMPT");
	case NBR_STA_INIT:
		return ("INIT");
	case NBR_STA_2_WAY:
		return ("2-WAY");
	case NBR_STA_XSTRT:
		return ("EXSTART");
	case NBR_STA_SNAP:
		return ("SNAPSHOT");
	case NBR_STA_XCHNG:
		return ("EXCHANGE");
	case NBR_STA_LOAD:
		return ("LOADING");
	case NBR_STA_FULL:
		return ("FULL");
	default:
		return ("UNKNOWN");
	}
}

const char *
print_link(int state)
{
	if (state & IFF_UP)
		return ("UP");
	else
		return ("DOWN");
}

#define TF_BUFS	8
#define TF_LEN	9

const char *
fmt_timeframe(time_t t)
{
	if (t == 0)
		return ("Never");
	else
		return (fmt_timeframe_core(time(NULL) - t));
}

const char *
fmt_timeframe_core(time_t t)
{
	char		*buf;
	static char	 tfbuf[TF_BUFS][TF_LEN];	/* ring buffer */
	static int	 idx = 0;
	unsigned	 sec, min, hrs, day, week;

	if (t == 0)
		return ("Stopped");

	buf = tfbuf[idx++];
	if (idx == TF_BUFS)
		idx = 0;

	week = t;

	sec = week % 60;
	week /= 60;
	min = week % 60;
	week /= 60;
	hrs = week % 24;
	week /= 24;
	day = week % 7;
	week /= 7;

	if (week > 0)
		snprintf(buf, TF_LEN, "%02uw%01ud%02uh", week, day, hrs);
	else if (day > 0)
		snprintf(buf, TF_LEN, "%01ud%02uh%02um", day, hrs, min);
	else
		snprintf(buf, TF_LEN, "%02u:%02u:%02u", hrs, min, sec);

	return (buf);
}

const char *
log_id(u_int32_t id)
{
	static char	buf[48];
	struct in_addr	addr;

	addr.s_addr = id;

	if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == NULL)
		return ("?");
	else
		return (buf);
}

const char *
log_adv_rtr(u_int32_t adv_rtr)
{
	static char	buf[48];
	struct in_addr	addr;

	addr.s_addr = adv_rtr;

	if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == NULL)
		return ("?");
	else
		return (buf);
}

u_int8_t
mask2prefixlen(struct in_addr ina)
{
	if (ina.s_addr == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina.s_addr)));
}

void
show_database_head(struct in_addr aid, u_int8_t type)
{
	char	*header, *format;

	switch (type) {
	case LSA_TYPE_ROUTER:
		format = "Router Link States";
		break;
	case LSA_TYPE_NETWORK:
		format = "Net Link States";
		break;
	case LSA_TYPE_SUM_NETWORK:
		format = "Summary Net Link States";
		break;
	case LSA_TYPE_SUM_ROUTER:
		format = "Summary Router Link States";
		break;
	case LSA_TYPE_EXTERNAL:
		format = NULL;
		if ((header = strdup("Type-5 AS External Link States")) == NULL)
			err(1, NULL);
		break;
	default:
		errx(1, "unknown LSA type");
	}
	if (type != LSA_TYPE_EXTERNAL)
		if (asprintf(&header, "%s (Area %s)", format,
		    inet_ntoa(aid)) == -1)
			err(1, NULL);

	printf("\n%-15s %s\n", "", header);
	free(header);

	printf("\n%-15s %-15s %-4s %-10s %-8s\n",
	    "Link ID", "Adv Router", "Age", "Seq#", "Checksum");
}

int
show_database_msg(struct imsg *imsg)
{
	static struct in_addr	 area_id;
	static u_int8_t		 lasttype;
	struct area		*area;
	struct lsa_hdr		*lsa;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_DATABASE:
		lsa = imsg->data;
		if (lsa->type != lasttype)
			show_database_head(area_id, lsa->type);
		printf("%-15s %-15s %-4d 0x%08x 0x%04x\n",
		    log_id(lsa->ls_id), log_adv_rtr(lsa->adv_rtr),
		    ntohs(lsa->age), ntohl(lsa->seq_num),
		    ntohs(lsa->ls_chksum));
		lasttype = lsa->type;
		break;
	case IMSG_CTL_AREA:
		area = imsg->data;
		area_id = area->id;
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0); 
}

int
show_nbr_msg(struct imsg *imsg)
{
	struct ctl_nbr	*nbr;
	char		*state;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NBR:
		nbr = imsg->data;
		if (asprintf(&state, "%s/%s", print_nbr_state(nbr->nbr_state),
		    print_if_state(nbr->iface_state)) == -1)
			err(1, NULL);
		printf("%-15s %-3d %-17s %-9s ", inet_ntoa(nbr->id),
		    nbr->priority, state, fmt_timeframe_core(nbr->dead_timer));
		printf("%-15s %s\n", inet_ntoa(nbr->addr), nbr->name);
		free(state);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

const char *
print_ospf_options(u_int8_t opts)
{
	static char	optbuf[32];

	snprintf(optbuf, sizeof(optbuf), "*|*|%s|%s|%s|%s|%s|*",
	    opts & OSPF_OPTION_DC ? "DC" : "-",
	    opts & OSPF_OPTION_EA ? "EA" : "-",
	    opts & OSPF_OPTION_NP ? "N/P" : "-",
	    opts & OSPF_OPTION_MC ? "MC" : "-",
	    opts & OSPF_OPTION_E ? "E" : "-");
	return (optbuf);
}

int
show_nbr_detail_msg(struct imsg *imsg)
{
	struct ctl_nbr	*nbr;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NBR:
		nbr = imsg->data;
		printf("\nNeighbor %s, ", inet_ntoa(nbr->id));
		printf("interface address %s\n", inet_ntoa(nbr->addr));
		printf("  In the area %s via interface %s\n",
		    inet_ntoa(nbr->area), nbr->name);
		printf("  Neighbor priority is %d, "
		    "State is %s, %d state changes\n",
		    nbr->priority, print_nbr_state(nbr->nbr_state),
		    nbr->state_chng_cnt);
		printf("  DR is %s, ", inet_ntoa(nbr->dr));
		printf("BDR is %s\n", inet_ntoa(nbr->bdr));
		printf("  Options is 0x%x  %s\n", nbr->options,
		    print_ospf_options(nbr->options));
		printf("  Dead timer due in %s\n",
		    fmt_timeframe_core(nbr->dead_timer));
		printf("  Database Summary List %d\n", nbr->db_sum_lst_cnt);
		printf("  Link State Request List %d\n", nbr->ls_req_lst_cnt);
		printf("  Link State Retransmission List %d\n",
		    nbr->ls_retrans_lst_cnt);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}
