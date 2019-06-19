/*	$OpenBSD: if.c,v 1.24 2019/03/04 21:27:35 dlg Exp $ */
/*
 * Copyright (c) 2004 Markus Friedl <markus@openbsd.org>
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

#include <sys/param.h>	/* roundup */
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "systat.h"

static  enum state { BOOT, TIME, RUN } state = TIME;

struct ifstat {
	char		ifs_name[IFNAMSIZ];	/* interface name */
	char		ifs_description[IFDESCRSIZE];
	struct ifcount	ifs_cur;
	struct ifcount	ifs_old;
	struct ifcount	ifs_now;
	char		ifs_flag;
} *ifstats;

static	int nifs = 0;
static int num_ifs = 0;
static int show_bits = 0;

void print_if(void);
int read_if(void);
int select_if(void);
int if_keyboard_callback(int);

void fetchifstat(void);
static void showifstat(struct ifstat *);
static void showtotal(void);
static void rt_getaddrinfo(struct sockaddr *, int, struct sockaddr **);

const char ifails[] = "IFAILS";
const char ofails[] = "OFAILS";

#define IF_ERR_SUM	0
#define IF_ERR_ERRORS	1
#define IF_ERR_QDROPS	2

struct if_err_view {
	const char *iname;
	const char *oname;
	uint64_t (*icount)(const struct ifcount *);
	uint64_t (*ocount)(const struct ifcount *);
};

static uint64_t if_err_ifails(const struct ifcount *);
static uint64_t if_err_ofails(const struct ifcount *);
static uint64_t if_err_ierrors(const struct ifcount *);
static uint64_t if_err_oerrors(const struct ifcount *);
static uint64_t if_err_iqdrops(const struct ifcount *);
static uint64_t if_err_oqdrops(const struct ifcount *);

static const struct if_err_view if_err_views[] = {
	[IF_ERR_SUM] =    {
		.iname = ifails,
		.oname = ofails,
		.icount = if_err_ifails,
		.ocount = if_err_ofails,
	},
	[IF_ERR_ERRORS] = {
		.iname = "IERRS",
		.oname = "OERRS",
		.icount = if_err_ierrors,
		.ocount = if_err_oerrors,
	},
	[IF_ERR_QDROPS] = {
		.iname = "IQDROPS",
		.oname = "OQDROPS",
		.icount = if_err_iqdrops,
		.ocount = if_err_oqdrops,
	},
};

static const struct if_err_view *if_err_view = &if_err_views[IF_ERR_SUM];

/* Define fields */
field_def fields_if[] = {
	{"IFACE", 8, 16, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"STATE", 4, 6, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"IPKTS", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"IBYTES", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{ifails, 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"OPKTS", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"OBYTES", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{ofails, 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"COLLS", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"DESC", 14, 64, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
};


#define FLD_IF_IFACE	FIELD_ADDR(fields_if,0)
#define FLD_IF_STATE	FIELD_ADDR(fields_if,1)
#define FLD_IF_IPKTS	FIELD_ADDR(fields_if,2)
#define FLD_IF_IBYTES	FIELD_ADDR(fields_if,3)
#define FLD_IF_IERRS	FIELD_ADDR(fields_if,4)
#define FLD_IF_OPKTS	FIELD_ADDR(fields_if,5)
#define FLD_IF_OBYTES	FIELD_ADDR(fields_if,6)
#define FLD_IF_OERRS	FIELD_ADDR(fields_if,7)
#define FLD_IF_COLLS	FIELD_ADDR(fields_if,8)
#define FLD_IF_DESC	FIELD_ADDR(fields_if,9)


/* Define views */
field_def *view_if_0[] = {
	FLD_IF_IFACE, FLD_IF_STATE, FLD_IF_DESC, FLD_IF_IPKTS,
	FLD_IF_IBYTES, FLD_IF_IERRS, FLD_IF_OPKTS, FLD_IF_OBYTES,
	FLD_IF_OERRS, FLD_IF_COLLS, NULL
};

/* Define view managers */

struct view_manager ifstat_mgr = {
	"Ifstat", select_if, read_if, NULL, print_header,
	print_if, if_keyboard_callback, NULL, NULL
};

field_view views_if[] = {
	{view_if_0, "ifstat", '1', &ifstat_mgr},
	{NULL, NULL, 0, NULL}
};


int
initifstat(void)
{
	field_view *v;
	read_if();
	for (v = views_if; v->name != NULL; v++)
		add_view(v);

	return(1);
}

#define UPDATE(x, y) do { \
		ifs->ifs_now.x = ifm.y; \
		ifs->ifs_cur.x = ifs->ifs_now.x - ifs->ifs_old.x; \
		if (state == TIME) {\
			ifs->ifs_old.x = ifs->ifs_now.x; \
			ifs->ifs_cur.x /= naptime; \
		} \
		sum.x += ifs->ifs_cur.x; \
	} while(0)


void
rt_getaddrinfo(struct sockaddr *sa, int addrs, struct sockaddr **info)
{
	int i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			info[i] = sa;
			sa = (struct sockaddr *) ((char *)(sa) +
			    roundup(sa->sa_len, sizeof(long)));
		} else
			info[i] = NULL;
	}
}



int
select_if(void)
{
	num_disp = num_ifs + 1;
	return (0);
}

int
read_if(void)
{
	fetchifstat();
	num_disp = num_ifs + 1;

	return 0;
}

void
print_if(void)
{
	int n, i, count = 0;

	for (n = 0, i = 0; n < nifs; n++) {
		if (ifstats[n].ifs_name[0] == '\0')
			continue;
		if (i++ < dispstart)
			continue;
		if (i == num_disp)
			break;
		showifstat(ifstats + n);
		if (maxprint > 0 && ++count >= maxprint)
			return;
	}
	showtotal();
}


void
fetchifstat(void)
{
	struct ifstat *newstats, *ifs;
	struct if_msghdr ifm;
	struct sockaddr *info[RTAX_MAX];
	struct sockaddr_dl *sdl;
	char *buf, *next, *lim;
	int mib[6], i;
	size_t need;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &need, NULL, 0) == -1)
		return;
	if ((buf = malloc(need)) == NULL)
		return;
	if (sysctl(mib, 6, buf, &need, NULL, 0) == -1) {
		free(buf);
		return;
	}

	bzero(&sum, sizeof(sum));
	num_ifs = 0;

	lim = buf + need;
	for (next = buf; next < lim; next += ifm.ifm_msglen) {
		bcopy(next, &ifm, sizeof ifm);
		if (ifm.ifm_version != RTM_VERSION ||
		    ifm.ifm_type != RTM_IFINFO ||
		    !(ifm.ifm_addrs & RTA_IFP))
			continue;
		if (ifm.ifm_index >= nifs) {
			if ((newstats = reallocarray(ifstats, ifm.ifm_index + 4,
			    sizeof(struct ifstat))) == NULL)
				continue;
			ifstats = newstats;
			for (; nifs < ifm.ifm_index + 4; nifs++)
				bzero(&ifstats[nifs], sizeof(*ifstats));
		}
		ifs = &ifstats[ifm.ifm_index];
		if (ifs->ifs_name[0] == '\0') {
			bzero(&info, sizeof(info));
			rt_getaddrinfo(
			    (struct sockaddr *)((struct if_msghdr *)next + 1),
			    ifm.ifm_addrs, info);
			sdl = (struct sockaddr_dl *)info[RTAX_IFP];

			if (sdl && sdl->sdl_family == AF_LINK &&
			    sdl->sdl_nlen > 0) {
				struct ifreq ifrdesc;
				char ifdescr[IFDESCRSIZE];
				int s;

				bcopy(sdl->sdl_data, ifs->ifs_name,
				      sdl->sdl_nlen);
				ifs->ifs_name[sdl->sdl_nlen] = '\0';

				/* Get the interface description */
				memset(&ifrdesc, 0, sizeof(ifrdesc));
				strlcpy(ifrdesc.ifr_name, ifs->ifs_name,
					sizeof(ifrdesc.ifr_name));
				ifrdesc.ifr_data = (caddr_t)&ifdescr;

				s = socket(AF_INET, SOCK_DGRAM, 0);
				if (s != -1) {
					if (ioctl(s, SIOCGIFDESCR, &ifrdesc) == 0)
						strlcpy(ifs->ifs_description,
						    ifrdesc.ifr_data,
						    sizeof(ifs->ifs_description));
					close(s);
				}
			}
			if (ifs->ifs_name[0] == '\0')
				continue;
		}
		num_ifs++;
		UPDATE(ifc_ip, ifm_data.ifi_ipackets);
		UPDATE(ifc_ib, ifm_data.ifi_ibytes);
		UPDATE(ifc_ie, ifm_data.ifi_ierrors);
		UPDATE(ifc_iq, ifm_data.ifi_iqdrops);
		UPDATE(ifc_op, ifm_data.ifi_opackets);
		UPDATE(ifc_ob, ifm_data.ifi_obytes);
		UPDATE(ifc_oe, ifm_data.ifi_oerrors);
		UPDATE(ifc_oq, ifm_data.ifi_oqdrops);
		UPDATE(ifc_co, ifm_data.ifi_collisions);
		ifs->ifs_cur.ifc_flags = ifm.ifm_flags;
		ifs->ifs_cur.ifc_state = ifm.ifm_data.ifi_link_state;
		ifs->ifs_flag++;
	}

	/* remove unreferenced interfaces */
	for (i = 0; i < nifs; i++) {
		ifs = &ifstats[i];
		if (ifs->ifs_flag)
			ifs->ifs_flag = 0;
		else
			ifs->ifs_name[0] = '\0';
	}

	free(buf);
}


static void
showifstat(struct ifstat *ifs)
{
	int conv = show_bits ? 8 : 1;
	int div = show_bits ? 1000 : 1024;

	print_fld_str(FLD_IF_IFACE, ifs->ifs_name);

	tb_start();
	tbprintf("%s", ifs->ifs_cur.ifc_flags & IFF_UP ?
		 "up" : "dn");

	switch (ifs->ifs_cur.ifc_state) {
	case LINK_STATE_UP:
	case LINK_STATE_HALF_DUPLEX:
	case LINK_STATE_FULL_DUPLEX:
		tbprintf(":U");
		break;
	case LINK_STATE_DOWN:
		tbprintf (":D");
		break;
	}

	print_fld_tb(FLD_IF_STATE);

	print_fld_str(FLD_IF_DESC, ifs->ifs_description);

	print_fld_sdiv(FLD_IF_IBYTES, ifs->ifs_cur.ifc_ib * conv, div);
	print_fld_size(FLD_IF_IPKTS, ifs->ifs_cur.ifc_ip);
	print_fld_size(FLD_IF_IERRS, if_err_view->icount(&ifs->ifs_cur));

	print_fld_sdiv(FLD_IF_OBYTES, ifs->ifs_cur.ifc_ob * conv, div);
	print_fld_size(FLD_IF_OPKTS, ifs->ifs_cur.ifc_op);
	print_fld_size(FLD_IF_OERRS, if_err_view->ocount(&ifs->ifs_cur));

	print_fld_size(FLD_IF_COLLS, ifs->ifs_cur.ifc_co);

	end_line();
}

static void
showtotal(void)
{
	int conv = show_bits ? 8 : 1;
	int div = show_bits ? 1000 : 1024;

	print_fld_str(FLD_IF_IFACE, "Totals");

	print_fld_sdiv(FLD_IF_IBYTES, sum.ifc_ib * conv, div);
	print_fld_size(FLD_IF_IPKTS, sum.ifc_ip);
	print_fld_size(FLD_IF_IERRS, if_err_view->icount(&sum));

	print_fld_sdiv(FLD_IF_OBYTES, sum.ifc_ob * conv, div);
	print_fld_size(FLD_IF_OPKTS, sum.ifc_op);
	print_fld_size(FLD_IF_OERRS, if_err_view->ocount(&sum));

	print_fld_size(FLD_IF_COLLS, sum.ifc_co);

	end_line();

}

static uint64_t
if_err_ifails(const struct ifcount *ifc)
{
	return (ifc->ifc_ie + ifc->ifc_iq);
}

static uint64_t
if_err_ofails(const struct ifcount *ifc)
{
	return (ifc->ifc_oe + ifc->ifc_oq);
}

static uint64_t
if_err_ierrors(const struct ifcount *ifc)
{
	return (ifc->ifc_ie);
}

static uint64_t
if_err_oerrors(const struct ifcount *ifc)
{
	return (ifc->ifc_oe);
}

static uint64_t
if_err_iqdrops(const struct ifcount *ifc)
{
	return (ifc->ifc_iq);
}

static uint64_t
if_err_oqdrops(const struct ifcount *ifc)
{
	return (ifc->ifc_oq);
}

static void
if_set_errs(unsigned int v)
{
	if_err_view = &if_err_views[v];
	FLD_IF_IERRS->title = if_err_view->iname;
	FLD_IF_IERRS->title = if_err_view->oname;
	gotsig_alarm = 1;
}

int
if_keyboard_callback(int ch)
{
	struct ifstat *ifs;

	switch (ch) {
	case 'd':
		if_set_errs(IF_ERR_QDROPS);
		break;
	case 'e':
		if_set_errs(IF_ERR_ERRORS);
		break;
	case 'f':
		if_set_errs(IF_ERR_SUM);
		break;

	case 'r':
		for (ifs = ifstats; ifs < ifstats + nifs; ifs++)
			ifs->ifs_old = ifs->ifs_now;
		state = RUN;
		gotsig_alarm = 1;

		break;
	case 'b':
		state = BOOT;
		for (ifs = ifstats; ifs < ifstats + nifs; ifs++)
			bzero(&ifs->ifs_old, sizeof(ifs->ifs_old));
		gotsig_alarm = 1;
		break;
	case 'B':
		show_bits = !show_bits;
		if (show_bits) {
			FLD_IF_IBYTES->title = "IBITS";
			FLD_IF_OBYTES->title = "OBITS";
		} else {
			FLD_IF_IBYTES->title = "IBYTES";
			FLD_IF_OBYTES->title = "OBYTES";
		}
		gotsig_alarm = 1;
		break;
	case 't':
		state = TIME;
		gotsig_alarm = 1;
		break;
	default:
		return keyboard_callback(ch);
	};

	return 1;
}

