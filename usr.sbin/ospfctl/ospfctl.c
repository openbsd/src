/*	$OpenBSD: ospfctl.c,v 1.64 2016/12/05 22:39:25 jca Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "parser.h"

__dead void	 usage(void);
int		 show_summary_msg(struct imsg *);
uint64_t	 get_ifms_type(uint8_t);
int		 show_interface_msg(struct imsg *);
int		 show_interface_detail_msg(struct imsg *);
const char	*print_link(int);
const char	*fmt_timeframe(time_t t);
const char	*fmt_timeframe_core(time_t t);
const char	*log_id(u_int32_t );
const char	*log_adv_rtr(u_int32_t);
void		 show_database_head(struct in_addr, char *, u_int8_t);
int		 show_database_msg(struct imsg *);
char		*print_ls_type(u_int8_t);
void		 show_db_hdr_msg_detail(struct lsa_hdr *);
char		*print_rtr_link_type(u_int8_t);
const char	*print_ospf_flags(u_int8_t);
int		 show_db_msg_detail(struct imsg *imsg);
int		 show_nbr_msg(struct imsg *);
const char	*print_ospf_options(u_int8_t);
int		 show_nbr_detail_msg(struct imsg *);
int		 show_rib_msg(struct imsg *);
void		 show_rib_head(struct in_addr, u_int8_t, u_int8_t);
const char	*print_ospf_rtr_flags(u_int8_t);
int		 show_rib_detail_msg(struct imsg *);
void		 show_fib_head(void);
int		 show_fib_msg(struct imsg *);
void		 show_interface_head(void);
const char *	 get_media_descr(uint64_t);
const char *	 get_linkstate(uint8_t, int);
void		 print_baudrate(u_int64_t);
int		 show_fib_interface_msg(struct imsg *);

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	unsigned int		 ifidx = 0;
	int			 ctl_sock;
	int			 done = 0;
	int			 n, verbose = 0;
	int			 ch;
	char			*sockname;

	sockname = OSPFD_SOCKET;
	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			sockname = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* parse options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	/* connect to ospfd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;

	strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, ctl_sock);
	done = 0;

	/* process user request */
	switch (res->action) {
	case NONE:
		usage();
		/* not reached */
	case SHOW:
	case SHOW_SUM:
		imsg_compose(ibuf, IMSG_CTL_SHOW_SUM, 0, 0, -1, NULL, 0);
		break;
	case SHOW_IFACE:
		printf("%-11s %-18s %-6s %-10s %-10s %-8s %3s %3s\n",
		    "Interface", "Address", "State", "HelloTimer", "Linkstate",
		    "Uptime", "nc", "ac");
		/*FALLTHROUGH*/
	case SHOW_IFACE_DTAIL:
		if (*res->ifname) {
			ifidx = if_nametoindex(res->ifname);
			if (ifidx == 0)
				errx(1, "no such interface %s", res->ifname);
		}
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE, 0, 0, -1,
		    &ifidx, sizeof(ifidx));
		break;
	case SHOW_NBR:
		printf("%-15s %-3s %-12s %-8s %-15s %-9s %s\n", "ID", "Pri",
		    "State", "DeadTime", "Address", "Iface","Uptime");
		/*FALLTHROUGH*/
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
	case SHOW_DBEXT:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_EXT, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBNET:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_NET, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBRTR:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_RTR, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBSELF:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_SELF, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBSUM:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_SUM, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBASBR:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_ASBR, 0, 0, -1, NULL, 0);
		break;
	case SHOW_DBOPAQ:
		imsg_compose(ibuf, IMSG_CTL_SHOW_DB_OPAQ, 0, 0, -1, NULL, 0);
		break;
	case SHOW_RIB:
		printf("%-20s %-17s %-12s %-9s %-7s %-8s\n", "Destination",
		    "Nexthop", "Path Type", "Type", "Cost", "Uptime");
		/*FALLTHROUGH*/
	case SHOW_RIB_DTAIL:
		imsg_compose(ibuf, IMSG_CTL_SHOW_RIB, 0, 0, -1, NULL, 0);
		break;
	case SHOW_FIB:
		if (!res->addr.s_addr)
			imsg_compose(ibuf, IMSG_CTL_KROUTE, 0, 0, -1,
			    &res->flags, sizeof(res->flags));
		else
			imsg_compose(ibuf, IMSG_CTL_KROUTE_ADDR, 0, 0, -1,
			    &res->addr, sizeof(res->addr));
		show_fib_head();
		break;
	case SHOW_FIB_IFACE:
		if (*res->ifname)
			imsg_compose(ibuf, IMSG_CTL_IFINFO, 0, 0, -1,
			    res->ifname, sizeof(res->ifname));
		else
			imsg_compose(ibuf, IMSG_CTL_IFINFO, 0, 0, -1, NULL, 0);
		show_interface_head();
		break;
	case FIB:
		errx(1, "fib couple|decouple");
		break;
	case FIB_COUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_COUPLE, 0, 0, -1, NULL, 0);
		printf("couple request sent.\n");
		done = 1;
		break;
	case FIB_DECOUPLE:
		imsg_compose(ibuf, IMSG_CTL_FIB_DECOUPLE, 0, 0, -1, NULL, 0);
		printf("decouple request sent.\n");
		done = 1;
		break;
	case FIB_RELOAD:
		imsg_compose(ibuf, IMSG_CTL_FIB_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		done = 1;
		break;
	case LOG_VERBOSE:
		verbose = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_LOG_VERBOSE, 0, 0, -1,
		    &verbose, sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		done = 1;
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
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
			case SHOW_SUM:
				done = show_summary_msg(&imsg);
				break;
			case SHOW_IFACE:
				done = show_interface_msg(&imsg);
				break;
			case SHOW_IFACE_DTAIL:
				done = show_interface_detail_msg(&imsg);
				break;
			case SHOW_NBR:
				done = show_nbr_msg(&imsg);
				break;
			case SHOW_NBR_DTAIL:
				done = show_nbr_detail_msg(&imsg);
				break;
			case SHOW_DB:
			case SHOW_DBBYAREA:
			case SHOW_DBSELF:
				done = show_database_msg(&imsg);
				break;
			case SHOW_DBEXT:
			case SHOW_DBNET:
			case SHOW_DBRTR:
			case SHOW_DBSUM:
			case SHOW_DBASBR:
			case SHOW_DBOPAQ:
				done = show_db_msg_detail(&imsg);
				break;
			case SHOW_RIB:
				done = show_rib_msg(&imsg);
				break;
			case SHOW_RIB_DTAIL:
				done = show_rib_detail_msg(&imsg);
				break;
			case SHOW_FIB:
				done = show_fib_msg(&imsg);
				break;
			case SHOW_FIB_IFACE:
				done = show_fib_interface_msg(&imsg);
				break;
			case NONE:
			case FIB:
			case FIB_COUPLE:
			case FIB_DECOUPLE:
			case FIB_RELOAD:
			case LOG_VERBOSE:
			case LOG_BRIEF:
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
show_summary_msg(struct imsg *imsg)
{
	struct ctl_sum		*sum;
	struct ctl_sum_area	*sumarea;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_SUM:
		sum = imsg->data;
		printf("Router ID: %s\n", inet_ntoa(sum->rtr_id));
		printf("Uptime: %s\n", fmt_timeframe_core(sum->uptime));
		printf("RFC1583 compatibility flag is ");
		if (sum->rfc1583compat)
			printf("enabled\n");
		else
			printf("disabled\n");

		printf("SPF delay is %d msec(s), hold time between two SPFs "
		    "is %d msec(s)\n", sum->spf_delay, sum->spf_hold_time);
		printf("Number of external LSA(s) %d (Checksum sum 0x%x)\n",
		    sum->num_ext_lsa, sum->ext_lsa_cksum);
		printf("Number of areas attached to this router: %d\n",
		    sum->num_area);
		break;
	case IMSG_CTL_SHOW_SUM_AREA:
		sumarea = imsg->data;
		printf("\nArea ID: %s\n", inet_ntoa(sumarea->area));
		printf("  Number of interfaces in this area: %d\n",
		    sumarea->num_iface);
		printf("  Number of fully adjacent neighbors in this "
		    "area: %d\n", sumarea->num_adj_nbr);
		printf("  SPF algorithm executed %d time(s)\n",
		    sumarea->num_spf_calc);
		printf("  Number LSA(s) %d (Checksum sum 0x%x)\n",
		    sumarea->num_lsa, sumarea->lsa_cksum);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

uint64_t
get_ifms_type(uint8_t if_type)
{
	switch (if_type) {
	case IFT_ETHER:
		return (IFM_ETHER);
	case IFT_FDDI:
		return (IFM_FDDI);
	case IFT_CARP:
		return (IFM_CARP);
	case IFT_PPP:
		return (IFM_TDM);
	default:
		return (0);
	}
}

int
show_interface_msg(struct imsg *imsg)
{
	struct ctl_iface	*iface;
	char			*netid;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE:
		iface = imsg->data;

		if (asprintf(&netid, "%s/%d", inet_ntoa(iface->addr),
		    mask2prefixlen(iface->mask.s_addr)) == -1)
			err(1, NULL);
		printf("%-11s %-18s %-6s %-10s %-10s %s %3d %3d\n",
		    iface->name, netid, if_state_name(iface->state),
		    iface->hello_timer.tv_sec < 0 ? "-" :
		    fmt_timeframe_core(iface->hello_timer.tv_sec),
		    get_linkstate(iface->if_type, iface->linkstate),
		    fmt_timeframe_core(iface->uptime),
		    iface->nbr_cnt, iface->adj_cnt);
		free(netid);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

int
show_interface_detail_msg(struct imsg *imsg)
{
	struct ctl_iface	*iface;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE:
		iface = imsg->data;
		printf("\n");
		printf("Interface %s, line protocol is %s\n",
		    iface->name, print_link(iface->flags));
		printf("  Internet address %s/%d, ",
		    inet_ntoa(iface->addr),
		    mask2prefixlen(iface->mask.s_addr));
		printf("Area %s\n", inet_ntoa(iface->area));
		printf("  Linkstate %s,",
		    get_linkstate(iface->if_type, iface->linkstate));
		printf(" mtu %d\n", iface->mtu);
		printf("  Router ID %s, network type %s, cost: %d\n",
		    inet_ntoa(iface->rtr_id),
		    if_type_name(iface->type), iface->metric);
		printf("  Transmit delay is %d sec(s), state %s, priority %d\n",
		    iface->transmit_delay, if_state_name(iface->state),
		    iface->priority);
		printf("  Designated Router (ID) %s, ",
		    inet_ntoa(iface->dr_id));
		printf("interface address %s\n", inet_ntoa(iface->dr_addr));
		printf("  Backup Designated Router (ID) %s, ",
		    inet_ntoa(iface->bdr_id));
		printf("interface address %s\n", inet_ntoa(iface->bdr_addr));
		if (iface->dead_interval == FAST_RTR_DEAD_TIME) {
			printf("  Timer intervals configured, "
			    "hello %d msec, dead %d, wait %d, retransmit %d\n",
			     iface->fast_hello_interval, iface->dead_interval,
			     iface->dead_interval, iface->rxmt_interval);

		} else {
			printf("  Timer intervals configured, "
			    "hello %d, dead %d, wait %d, retransmit %d\n",
			     iface->hello_interval, iface->dead_interval,
			     iface->dead_interval, iface->rxmt_interval);
		}
		if (iface->passive)
			printf("    Passive interface (No Hellos)\n");
		else if (iface->hello_timer.tv_sec < 0)
			printf("    Hello timer not running\n");
		else
			printf("    Hello timer due in %s+%ldmsec\n",
			    fmt_timeframe_core(iface->hello_timer.tv_sec),
			    iface->hello_timer.tv_usec / 1000);
		printf("    Uptime %s\n", fmt_timeframe_core(iface->uptime));
		printf("  Neighbor count is %d, adjacent neighbor count is "
		    "%d\n", iface->nbr_cnt, iface->adj_cnt);
		if (iface->auth_type > 0) {
			switch (iface->auth_type) {
			case AUTH_SIMPLE:
				printf("  Simple password authentication "
				    "enabled\n");
				break;
			case AUTH_CRYPT:
				printf("  Message digest authentication "
				    "enabled\n");
				printf("    Primary key id is %d\n",
				    iface->auth_keyid);
				break;
			default:
				break;
			}
		}
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
	unsigned int	 sec, min, hrs, day;
	unsigned long long	week;

	if (t == 0)
		return ("00:00:00");

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
		snprintf(buf, TF_LEN, "%02lluw%01ud%02uh", week, day, hrs);
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

/* prototype defined in ospfd.h and shared with the kroute.c version */
u_int8_t
mask2prefixlen(in_addr_t ina)
{
	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

void
show_database_head(struct in_addr aid, char *ifname, u_int8_t type)
{
	char	*header, *format;
	int	 cleanup = 0;

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
	case LSA_TYPE_LINK_OPAQ:
		format = "Type-9 Link Local Opaque Link States";
		break;
	case LSA_TYPE_AREA_OPAQ:
		format = "Type-10 Area Local Opaque Link States";
		break;
	case LSA_TYPE_AS_OPAQ:
		format = NULL;
		if ((header = strdup("Type-11 AS Wide Opaque Link States")) ==
		    NULL)
			err(1, NULL);
		break;
	default:
		if (asprintf(&format, "LSA type %x", ntohs(type)) == -1)
			err(1, NULL);
		cleanup = 1;
		break;
	}
	if (type == LSA_TYPE_LINK_OPAQ) {
		if (asprintf(&header, "%s (Area %s Interface %s)", format,
		    inet_ntoa(aid), ifname) == -1)
			err(1, NULL);
	} else if (type != LSA_TYPE_EXTERNAL && type != LSA_TYPE_AS_OPAQ)
		if (asprintf(&header, "%s (Area %s)", format,
		    inet_ntoa(aid)) == -1)
			err(1, NULL);

	printf("\n%-15s %s\n\n", "", header);
	free(header);
	if (cleanup)
		free(format);
}

int
show_database_msg(struct imsg *imsg)
{
	static struct in_addr	 area_id;
	static char		 ifname[IF_NAMESIZE];
	static u_int8_t		 lasttype;
	struct area		*area;
	struct iface		*iface;
	struct lsa_hdr		*lsa;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_DATABASE:
	case IMSG_CTL_SHOW_DB_SELF:
		lsa = imsg->data;
		if (lsa->type != lasttype) {
			show_database_head(area_id, ifname, lsa->type);
			printf("%-15s %-15s %-4s %-10s %-8s\n", "Link ID",
			    "Adv Router", "Age", "Seq#", "Checksum");
		}
		printf("%-15s %-15s %-4d 0x%08x 0x%04x\n",
		    log_id(lsa->ls_id), log_adv_rtr(lsa->adv_rtr),
		    ntohs(lsa->age), ntohl(lsa->seq_num),
		    ntohs(lsa->ls_chksum));
		lasttype = lsa->type;
		break;
	case IMSG_CTL_AREA:
		area = imsg->data;
		area_id = area->id;
		lasttype = 0;
		break;
	case IMSG_CTL_IFACE:
		iface = imsg->data;
		strlcpy(ifname, iface->name, sizeof(ifname));
		lasttype = 0;
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

char *
print_ls_type(u_int8_t type)
{
	switch (type) {
	case LSA_TYPE_ROUTER:
		return ("Router");
	case LSA_TYPE_NETWORK:
		return ("Network");
	case LSA_TYPE_SUM_NETWORK:
		return ("Summary (Network)");
	case LSA_TYPE_SUM_ROUTER:
		return ("Summary (Router)");
	case LSA_TYPE_EXTERNAL:
		return ("AS External");
	case LSA_TYPE_LINK_OPAQ:
		return ("Type-9 Opaque");
	case LSA_TYPE_AREA_OPAQ:
		return ("Type-10 Opaque");
	case LSA_TYPE_AS_OPAQ:
		return ("Type-11 Opaque");
	default:
		return ("Unknown");
	}
}

void
show_db_hdr_msg_detail(struct lsa_hdr *lsa)
{
	printf("LS age: %d\n", ntohs(lsa->age));
	printf("Options: %s\n", print_ospf_options(lsa->opts));
	printf("LS Type: %s\n", print_ls_type(lsa->type));

	switch (lsa->type) {
	case LSA_TYPE_ROUTER:
		printf("Link State ID: %s\n", log_id(lsa->ls_id));
		break;
	case LSA_TYPE_NETWORK:
		printf("Link State ID: %s (address of Designated Router)\n",
		    log_id(lsa->ls_id));
		break;
	case LSA_TYPE_SUM_NETWORK:
		printf("Link State ID: %s (Network ID)\n", log_id(lsa->ls_id));
		break;
	case LSA_TYPE_SUM_ROUTER:
		printf("Link State ID: %s (ASBR Router ID)\n",
		    log_id(lsa->ls_id));
		break;
	case LSA_TYPE_EXTERNAL:
		printf("Link State ID: %s (External Network Number)\n",
		     log_id(lsa->ls_id));
		break;
	case LSA_TYPE_LINK_OPAQ:
	case LSA_TYPE_AREA_OPAQ:
	case LSA_TYPE_AS_OPAQ:
		printf("Link State ID: %s Type %d ID %d\n", log_id(lsa->ls_id),
		    LSA_24_GETHI(ntohl(lsa->ls_id)),
		    LSA_24_GETLO(ntohl(lsa->ls_id)));
		break;
	}

	printf("Advertising Router: %s\n", log_adv_rtr(lsa->adv_rtr));
	printf("LS Seq Number: 0x%08x\n", ntohl(lsa->seq_num));
	printf("Checksum: 0x%04x\n", ntohs(lsa->ls_chksum));
	printf("Length: %d\n", ntohs(lsa->len));
}

char *
print_rtr_link_type(u_int8_t type)
{
	switch (type) {
	case LINK_TYPE_POINTTOPOINT:
		return ("Point-to-Point");
	case LINK_TYPE_TRANSIT_NET:
		return ("Transit Network");
	case LINK_TYPE_STUB_NET:
		return ("Stub Network");
	case LINK_TYPE_VIRTUAL:
		return ("Virtual Link");
	default:
		return ("Unknown");
	}
}

const char *
print_ospf_flags(u_int8_t opts)
{
	static char	optbuf[32];

	snprintf(optbuf, sizeof(optbuf), "*|*|*|*|*|%s|%s|%s",
	    opts & OSPF_RTR_V ? "V" : "-",
	    opts & OSPF_RTR_E ? "E" : "-",
	    opts & OSPF_RTR_B ? "B" : "-");
	return (optbuf);
}

int
show_db_msg_detail(struct imsg *imsg)
{
	static struct in_addr	 area_id;
	static char		 ifname[IF_NAMESIZE];
	static u_int8_t		 lasttype;
	struct in_addr		 addr, data;
	struct area		*area;
	struct iface		*iface;
	struct lsa		*lsa;
	struct lsa_rtr_link	*rtr_link;
	struct lsa_asext	*asext;
	u_int16_t		 i, nlinks, off;

	/* XXX sanity checks! */

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_DB_EXT:
		lsa = imsg->data;
		if (lsa->hdr.type != lasttype)
			show_database_head(area_id, ifname, lsa->hdr.type);
		show_db_hdr_msg_detail(&lsa->hdr);
		addr.s_addr = lsa->data.asext.mask;
		printf("Network Mask: %s\n", inet_ntoa(addr));

		asext = (struct lsa_asext *)((char *)lsa + sizeof(lsa->hdr));

		printf("    Metric type: ");
		if (ntohl(lsa->data.asext.metric) & LSA_ASEXT_E_FLAG)
			printf("2\n");
		else
			printf("1\n");
		printf("    Metric: %d\n", ntohl(asext->metric)
		    & LSA_METRIC_MASK);
		addr.s_addr = asext->fw_addr;
		printf("    Forwarding Address: %s\n", inet_ntoa(addr));
		printf("    External Route Tag: %d\n\n", ntohl(asext->ext_tag));

		lasttype = lsa->hdr.type;
		break;
	case IMSG_CTL_SHOW_DB_NET:
		lsa = imsg->data;
		if (lsa->hdr.type != lasttype)
			show_database_head(area_id, ifname, lsa->hdr.type);
		show_db_hdr_msg_detail(&lsa->hdr);
		addr.s_addr = lsa->data.net.mask;
		printf("Network Mask: %s\n", inet_ntoa(addr));

		nlinks = (ntohs(lsa->hdr.len) - sizeof(struct lsa_hdr)
		    - sizeof(u_int32_t)) / sizeof(struct lsa_net_link);
		off = sizeof(lsa->hdr) + sizeof(u_int32_t);
		printf("Number of Routers: %d\n", nlinks);

		for (i = 0; i < nlinks; i++) {
			addr.s_addr = lsa->data.net.att_rtr[i];
			printf("    Attached Router: %s\n", inet_ntoa(addr));
		}

		printf("\n");
		lasttype = lsa->hdr.type;
		break;
	case IMSG_CTL_SHOW_DB_RTR:
		lsa = imsg->data;
		if (lsa->hdr.type != lasttype)
			show_database_head(area_id, ifname, lsa->hdr.type);
		show_db_hdr_msg_detail(&lsa->hdr);
		printf("Flags: %s\n", print_ospf_flags(lsa->data.rtr.flags));
		nlinks = ntohs(lsa->data.rtr.nlinks);
		printf("Number of Links: %d\n\n", nlinks);

		off = sizeof(lsa->hdr) + sizeof(struct lsa_rtr);

		for (i = 0; i < nlinks; i++) {
			rtr_link = (struct lsa_rtr_link *)((char *)lsa + off);

			printf("    Link connected to: %s\n",
			    print_rtr_link_type(rtr_link->type));

			addr.s_addr = rtr_link->id;
			data.s_addr = rtr_link->data;

			switch (rtr_link->type) {
			case LINK_TYPE_POINTTOPOINT:
			case LINK_TYPE_VIRTUAL:
				printf("    Link ID (Neighbors Router ID):"
				    " %s\n", inet_ntoa(addr));
				printf("    Link Data (Router Interface "
				    "address): %s\n", inet_ntoa(data));
				break;
			case LINK_TYPE_TRANSIT_NET:
				printf("    Link ID (Designated Router "
				    "address): %s\n", inet_ntoa(addr));
				printf("    Link Data (Router Interface "
				    "address): %s\n", inet_ntoa(data));
				break;
			case LINK_TYPE_STUB_NET:
				printf("    Link ID (Network ID): %s\n",
				    inet_ntoa(addr));
				printf("    Link Data (Network Mask): %s\n",
				    inet_ntoa(data));
				break;
			default:
				printf("    Link ID (Unknown): %s\n",
				    inet_ntoa(addr));
				printf("    Link Data (Unknown): %s\n",
				    inet_ntoa(data));
				break;
			}

			printf("    Metric: %d\n\n", ntohs(rtr_link->metric));

			off += sizeof(struct lsa_rtr_link) +
			    rtr_link->num_tos * sizeof(u_int32_t);
		}

		lasttype = lsa->hdr.type;
		break;
	case IMSG_CTL_SHOW_DB_SUM:
	case IMSG_CTL_SHOW_DB_ASBR:
		lsa = imsg->data;
		if (lsa->hdr.type != lasttype)
			show_database_head(area_id, ifname, lsa->hdr.type);
		show_db_hdr_msg_detail(&lsa->hdr);
		addr.s_addr = lsa->data.sum.mask;
		printf("Network Mask: %s\n", inet_ntoa(addr));
		printf("Metric: %d\n\n", ntohl(lsa->data.sum.metric) &
		    LSA_METRIC_MASK);
		lasttype = lsa->hdr.type;
		break;
	case IMSG_CTL_SHOW_DB_OPAQ:
		lsa = imsg->data;
		if (lsa->hdr.type != lasttype)
			show_database_head(area_id, ifname, lsa->hdr.type);
		show_db_hdr_msg_detail(&lsa->hdr);
		/* XXX should we hexdump the data? */
		lasttype = lsa->hdr.type;
		break;
	case IMSG_CTL_AREA:
		area = imsg->data;
		area_id = area->id;
		lasttype = 0;
		break;
	case IMSG_CTL_IFACE:
		iface = imsg->data;
		strlcpy(ifname, iface->name, sizeof(ifname));
		lasttype = 0;
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
		if (asprintf(&state, "%s/%s", nbr_state_name(nbr->nbr_state),
		    if_state_name(nbr->iface_state)) == -1)
			err(1, NULL);
		printf("%-15s %-3d %-12s %-9s", inet_ntoa(nbr->id),
		    nbr->priority, state, fmt_timeframe_core(nbr->dead_timer));
		printf("%-15s %-9s %s\n", inet_ntoa(nbr->addr), nbr->name,
		    nbr->uptime == 0 ? "-" : fmt_timeframe_core(nbr->uptime));
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

	snprintf(optbuf, sizeof(optbuf), "%s|%s|%s|%s|%s|%s|%s|%s",
	    opts & OSPF_OPTION_DN ? "DN" : "-",
	    opts & OSPF_OPTION_O ? "O" : "-",
	    opts & OSPF_OPTION_DC ? "DC" : "-",
	    opts & OSPF_OPTION_EA ? "EA" : "-",
	    opts & OSPF_OPTION_NP ? "N/P" : "-",
	    opts & OSPF_OPTION_MC ? "MC" : "-",
	    opts & OSPF_OPTION_E ? "E" : "-",
	    opts & OSPF_OPTION_MT ? "MT" : "-");
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
		printf("  Area %s, interface %s\n", inet_ntoa(nbr->area),
		    nbr->name);
		printf("  Neighbor priority is %d, "
		    "State is %s, %d state changes\n",
		    nbr->priority, nbr_state_name(nbr->nbr_state),
		    nbr->state_chng_cnt);
		printf("  DR is %s, ", inet_ntoa(nbr->dr));
		printf("BDR is %s\n", inet_ntoa(nbr->bdr));
		printf("  Options %s\n", print_ospf_options(nbr->options));
		printf("  Dead timer due in %s\n",
		    fmt_timeframe_core(nbr->dead_timer));
		printf("  Uptime %s\n", fmt_timeframe_core(nbr->uptime));
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

int
show_rib_msg(struct imsg *imsg)
{
	struct ctl_rt	*rt;
	char		*dstnet;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB:
		rt = imsg->data;
		switch (rt->d_type) {
		case DT_NET:
			if (asprintf(&dstnet, "%s/%d", inet_ntoa(rt->prefix),
			    rt->prefixlen) == -1)
				err(1, NULL);
			break;
		case DT_RTR:
			if (asprintf(&dstnet, "%s",
			    inet_ntoa(rt->prefix)) == -1)
				err(1, NULL);
			break;
		default:
			errx(1, "Invalid route type");
		}

		printf("%-20s %-16s%s %-12s %-9s %-7d %s\n", dstnet,
		    inet_ntoa(rt->nexthop), rt->connected ? "C" : " ",
		    path_type_name(rt->p_type),
		    dst_type_name(rt->d_type), rt->cost,
		    rt->uptime == 0 ? "-" : fmt_timeframe_core(rt->uptime));
		free(dstnet);
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

void
show_rib_head(struct in_addr aid, u_int8_t d_type, u_int8_t p_type)
{
	char	*header, *format, *format2;

	switch (p_type) {
	case PT_INTRA_AREA:
	case PT_INTER_AREA:
		switch (d_type) {
		case DT_NET:
			format = "Network Routing Table";
			format2 = "";
			break;
		case DT_RTR:
			format = "Router Routing Table";
			format2 = "Type";
			break;
		default:
			errx(1, "unknown route type");
		}
		break;
	case PT_TYPE1_EXT:
	case PT_TYPE2_EXT:
		format = NULL;
		format2 = "Cost 2";
		if ((header = strdup("External Routing Table")) == NULL)
			err(1, NULL);
		break;
	default:
		errx(1, "unknown route type");
	}

	if (p_type != PT_TYPE1_EXT && p_type != PT_TYPE2_EXT)
		if (asprintf(&header, "%s (Area %s)", format,
		    inet_ntoa(aid)) == -1)
			err(1, NULL);

	printf("\n%-18s %s\n", "", header);
	free(header);

	printf("\n%-18s %-15s %-15s %-12s %-7s %-7s\n", "Destination",
	    "Nexthop", "Adv Router", "Path type", "Cost", format2);
}

const char *
print_ospf_rtr_flags(u_int8_t opts)
{
	static char	optbuf[32];

	snprintf(optbuf, sizeof(optbuf), "%s%s%s",
	    opts & OSPF_RTR_E ? "AS" : "",
	    opts & OSPF_RTR_E && opts & OSPF_RTR_B ? "+" : "",
	    opts & OSPF_RTR_B ? "ABR" : "");
	return (optbuf);
}

int
show_rib_detail_msg(struct imsg *imsg)
{
	static struct in_addr	 area_id;
	struct ctl_rt		*rt;
	struct area		*area;
	char			*dstnet;
	static u_int8_t		 lasttype;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_RIB:
		rt = imsg->data;

		switch (rt->p_type) {
		case PT_INTRA_AREA:
		case PT_INTER_AREA:
			switch (rt->d_type) {
			case DT_NET:
				if (lasttype != RIB_NET)
					show_rib_head(rt->area, rt->d_type,
					     rt->p_type);
				if (asprintf(&dstnet, "%s/%d",
				    inet_ntoa(rt->prefix), rt->prefixlen) == -1)
					err(1, NULL);
				lasttype = RIB_NET;
				break;
			case DT_RTR:
				if (lasttype != RIB_RTR)
					show_rib_head(rt->area, rt->d_type,
					     rt->p_type);
				if (asprintf(&dstnet, "%s",
				    inet_ntoa(rt->prefix)) == -1)
					err(1, NULL);
				lasttype = RIB_RTR;
				break;
			default:
				errx(1, "unknown route type");
			}
			printf("%-18s %-15s ", dstnet, inet_ntoa(rt->nexthop));
			printf("%-15s %-12s %-7d", inet_ntoa(rt->adv_rtr),
			    path_type_name(rt->p_type), rt->cost);
			free(dstnet);

			if (rt->d_type == DT_RTR)
				printf(" %-7s",
				    print_ospf_rtr_flags(rt->flags));

			printf("\n");
			break;
		case PT_TYPE1_EXT:
		case PT_TYPE2_EXT:
			if (lasttype != RIB_EXT)
				show_rib_head(rt->area, rt->d_type, rt->p_type);

			if (asprintf(&dstnet, "%s/%d",
			    inet_ntoa(rt->prefix), rt->prefixlen) == -1)
				err(1, NULL);

			printf("%-18s %-15s ", dstnet, inet_ntoa(rt->nexthop));
			printf("%-15s %-12s %-7d %-7d\n",
			    inet_ntoa(rt->adv_rtr), path_type_name(rt->p_type),
			    rt->cost, rt->cost2);
			free(dstnet);

			lasttype = RIB_EXT;
			break;
		default:
			errx(1, "unknown route type");
		}
		break;
	case IMSG_CTL_AREA:
		area = imsg->data;
		area_id = area->id;
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

void
show_fib_head(void)
{
	printf("flags: * = valid, O = OSPF, C = Connected, S = Static\n");
	printf("%-6s %-4s %-20s %-17s\n", "Flags", "Prio", "Destination", "Nexthop");
}

int
show_fib_msg(struct imsg *imsg)
{
	struct kroute		*k;
	char			*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(struct kroute))
			errx(1, "wrong imsg len");
		k = imsg->data;

		if (k->flags & F_DOWN)
			printf(" ");
		else
			printf("*");

		if (!(k->flags & F_KERNEL))
			printf("O");
		else if (k->flags & F_CONNECTED)
			printf("C");
		else if (k->flags & F_STATIC)
			printf("S");
		else
			printf(" ");

		printf("     ");
		printf("%4d ", k->priority);
		if (asprintf(&p, "%s/%u", inet_ntoa(k->prefix), k->prefixlen) ==
		    -1)
			err(1, NULL);
		printf("%-20s ", p);
		free(p);

		if (k->nexthop.s_addr)
			printf("%s", inet_ntoa(k->nexthop));
		else if (k->flags & F_CONNECTED)
			printf("link#%u", k->ifindex);
		printf("\n");

		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}

void
show_interface_head(void)
{
	printf("%-15s%-15s%s\n", "Interface", "Flags",
	    "Link state");
}

const struct if_status_description
		if_status_descriptions[] = LINK_STATE_DESCRIPTIONS;
const struct ifmedia_description
		ifm_type_descriptions[] = IFM_TYPE_DESCRIPTIONS;

const char *
get_media_descr(uint64_t media_type)
{
	const struct ifmedia_description	*p;

	for (p = ifm_type_descriptions; p->ifmt_string != NULL; p++)
		if (media_type == p->ifmt_word)
			return (p->ifmt_string);

	return ("unknown");
}

const char *
get_linkstate(uint8_t if_type, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, if_type, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return (buf);
}

void
print_baudrate(u_int64_t baudrate)
{
	if (baudrate > IF_Gbps(1))
		printf("%llu GBit/s", baudrate / IF_Gbps(1));
	else if (baudrate > IF_Mbps(1))
		printf("%llu MBit/s", baudrate / IF_Mbps(1));
	else if (baudrate > IF_Kbps(1))
		printf("%llu KBit/s", baudrate / IF_Kbps(1));
	else
		printf("%llu Bit/s", baudrate);
}

int
show_fib_interface_msg(struct imsg *imsg)
{
	struct kif	*k;
	uint64_t	 ifms_type;

	switch (imsg->hdr.type) {
	case IMSG_CTL_IFINFO:
		k = imsg->data;
		printf("%-15s", k->ifname);
		printf("%-15s", k->flags & IFF_UP ? "UP" : "");
		ifms_type = get_ifms_type(k->if_type);
		if (ifms_type)
			printf("%s, ", get_media_descr(ifms_type));

		printf("%s", get_linkstate(k->if_type, k->link_state));

		if (k->link_state != LINK_STATE_DOWN && k->baudrate > 0) {
			printf(", ");
			print_baudrate(k->baudrate);
		}
		printf("\n");
		break;
	case IMSG_CTL_END:
		printf("\n");
		return (1);
	default:
		break;
	}

	return (0);
}
