/*	$OpenBSD: if.c,v 1.1 2004/11/16 09:52:33 markus Exp $ */
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <stdlib.h>

#include "systat.h"
#include "extern.h"

static  enum state { BOOT, TIME, RUN } state = TIME;

struct ifcount {
	u_long		ifc_ib;			/* input bytes */
	u_long		ifc_ip;			/* input packets */
	u_long		ifc_ie;			/* input errors */
	u_long		ifc_ob;			/* output bytes */
	u_long		ifc_op;			/* output packets */
	u_long		ifc_oe;			/* output errors */
	u_long		ifc_co;			/* collisions */
} sum;

struct ifstat {
	char		ifs_name[IFNAMSIZ];	/* interface name */
	struct ifcount	ifs_cur;
	struct ifcount	ifs_old;
	struct ifcount	ifs_now;
} *ifstats;

static	int nifs = 0;

WINDOW *
openifstat(void)
{

	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

void
closeifstat(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

int
initifstat(void)
{

	fetchifstat();
	return(1);
}

#define UPDATE(x, y) do { \
		ifs->ifs_now.x = ifm->y; \
		ifs->ifs_cur.x = ifs->ifs_now.x - ifs->ifs_old.x; \
		sum.x += ifs->ifs_cur.x; \
		if (state == TIME) \
			ifs->ifs_old.x = ifs->ifs_now.x; \
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

void
fetchifstat(void)
{
	struct ifstat *newstats, *ifs;
	struct if_msghdr *ifm;
	struct sockaddr *info[RTAX_MAX];
	struct sockaddr_dl *sdl;
	char *buf, *next, *lim;
	int mib[6], i;
	size_t need;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
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

	lim = buf + need;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type != RTM_IFINFO ||
		   !(ifm->ifm_addrs & RTA_IFP))
			continue;
		if (ifm->ifm_index >= nifs) {
			if ((newstats = realloc(ifstats, (ifm->ifm_index + 4) *
			    sizeof(struct ifstat))) == NULL)
				continue;
			ifstats = newstats;
			for (; nifs < ifm->ifm_index + 4; nifs++)
				ifstats[nifs].ifs_name[0] = '\0';
		}
		ifs = &ifstats[ifm->ifm_index];
		if (ifs->ifs_name[0] == '\0') {
			bzero(&info, sizeof(info));
			rt_getaddrinfo((struct sockaddr *)(ifm + 1),
			    ifm->ifm_addrs, info);
			if ((sdl = (struct sockaddr_dl *)info[RTAX_IFP])) {
				if (sdl->sdl_family == AF_LINK &&
				    sdl->sdl_nlen > 0)
					strlcpy(ifs->ifs_name,
					    sdl->sdl_data,
					    sizeof(ifs->ifs_name));
			}
			if (ifs->ifs_name[0] == '\0')
				continue;
		}
		UPDATE(ifc_ip, ifm_data.ifi_ipackets);
		UPDATE(ifc_ib, ifm_data.ifi_ibytes);
		UPDATE(ifc_ie, ifm_data.ifi_ierrors);
		UPDATE(ifc_op, ifm_data.ifi_opackets);
		UPDATE(ifc_ob, ifm_data.ifi_obytes);
		UPDATE(ifc_oe, ifm_data.ifi_oerrors);
		UPDATE(ifc_co, ifm_data.ifi_collisions);
	}
	free(buf);
}

#define INSET 0

void
labelifstat(void)
{

	wmove(wnd, 0, 0); wclrtobot(wnd);

	mvwaddstr(wnd, 1, INSET, "Interfaces");
	mvwaddstr(wnd, 1, INSET+15, "Ibytes");
	mvwaddstr(wnd, 1, INSET+27, "Ipkts");
	mvwaddstr(wnd, 1, INSET+34, "Ierrs");
	mvwaddstr(wnd, 1, INSET+46, "Obytes");
	mvwaddstr(wnd, 1, INSET+58, "Opkts");
	mvwaddstr(wnd, 1, INSET+65, "Oerrs");
	mvwaddstr(wnd, 1, INSET+74, "Colls");
}

#define FMT "%-10.10s %10lu %10lu %6lu   %10lu %10lu %6lu   %6lu "
	
void
showifstat(void)
{
	int row;
	struct ifstat *ifs;

	row = 2;
	wmove(wnd, 0, 0); wclrtoeol(wnd);
	for (ifs = ifstats; ifs < ifstats + nifs; ifs++) {
		if (ifs->ifs_name[0] == '\0')
			continue;
		mvwprintw(wnd, row++, INSET, FMT,
		    ifs->ifs_name,
		    ifs->ifs_cur.ifc_ib,
		    ifs->ifs_cur.ifc_ip,
		    ifs->ifs_cur.ifc_ie,
		    ifs->ifs_cur.ifc_ob,
		    ifs->ifs_cur.ifc_op,
		    ifs->ifs_cur.ifc_oe,
		    ifs->ifs_cur.ifc_co);
	}
	mvwprintw(wnd, row++, INSET, FMT,
	    "Totals",
	    sum.ifc_ib,
	    sum.ifc_ip,
	    sum.ifc_ie,
	    sum.ifc_ob,
	    sum.ifc_op,
	    sum.ifc_oe,
	    sum.ifc_co);
}

int
cmdifstat(char *cmd, char *args)
{
	struct ifstat *ifs;

	if (prefix(cmd, "run")) {
		if (state != RUN)
			for (ifs = ifstats; ifs < ifstats + nifs; ifs++)
				ifs->ifs_old = ifs->ifs_now;
		state = RUN;
		return (1);
	}
	if (prefix(cmd, "boot")) {
		state = BOOT;
		for (ifs = ifstats; ifs < ifstats + nifs; ifs++)
			bzero(&ifs->ifs_old, sizeof(ifs->ifs_old));
		return (1);
	}
	if (prefix(cmd, "time")) {
		state = TIME;
		return (1);
	}
	if (prefix(cmd, "zero")) {
		if (state == RUN)
			for (ifs = ifstats; ifs < ifstats + nifs; ifs++)
				ifs->ifs_old = ifs->ifs_now;
		return (1);
	}
	return (1);
}
