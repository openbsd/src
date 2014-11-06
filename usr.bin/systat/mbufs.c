/*	$OpenBSD: mbufs.c,v 1.37 2014/11/06 12:50:55 dlg Exp $ */
/*
 * Copyright (c) 2008 Can Erkin Acar <canacar@openbsd.org>
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
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"

/* pool info */
int mbpool_index = -1;
int mclpools_index[MCLPOOLS];
int mclpool_count = 0;
struct kinfo_pool mbpool;
u_int mcllivelocks, mcllivelocks_cur, mcllivelocks_diff;

/* interfaces */
static int num_ifs = 0;
struct if_info {
	char name[16];
	struct if_rxrinfo data;
} *interfaces = NULL;

static int sock;

void print_mb(void);
int read_mb(void);
int select_mb(void);
static void showmbuf(struct if_info *, int, int);

/* Define fields */
field_def fields_mbuf[] = {
	{"IFACE", 8, 16, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"RXDELAY", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"TXDELAY", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"LIVELOCKS", 5, 10, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"SIZE", 3, 5, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"ALIVE", 3, 5, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"LWM", 3, 5, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"HWM", 3, 5, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"CWM", 3, 5, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
};


#define FLD_MB_IFACE	FIELD_ADDR(fields_mbuf,0)
#define FLD_MB_RXDELAY	FIELD_ADDR(fields_mbuf,1)
#define FLD_MB_TXDELAY	FIELD_ADDR(fields_mbuf,2)
#define FLD_MB_LLOCKS	FIELD_ADDR(fields_mbuf,3)
#define FLD_MB_MSIZE	FIELD_ADDR(fields_mbuf,4)
#define FLD_MB_MALIVE	FIELD_ADDR(fields_mbuf,5)
#define FLD_MB_MLWM	FIELD_ADDR(fields_mbuf,6)
#define FLD_MB_MHWM	FIELD_ADDR(fields_mbuf,7)
#define FLD_MB_MCWM	FIELD_ADDR(fields_mbuf,8)


/* Define views */
field_def *view_mbuf[] = {
	FLD_MB_IFACE,
	FLD_MB_LLOCKS, FLD_MB_MSIZE, FLD_MB_MALIVE, FLD_MB_MLWM, FLD_MB_MHWM,
	FLD_MB_MCWM, NULL
};

/* Define view managers */

struct view_manager mbuf_mgr = {
	"Mbufs", select_mb, read_mb, NULL, print_header,
	print_mb, keyboard_callback, NULL, NULL
};

field_view views_mb[] = {
	{view_mbuf, "mbufs", '4', &mbuf_mgr},
	{NULL, NULL, 0, NULL}
};


int
initmembufs(void)
{
	struct if_rxring_info *ifr;
	field_view *v;
	int i, mib[4], npools;
	struct kinfo_pool pool;
	char pname[32];
	size_t size;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		err(1, "socket()");
		/* NOTREACHED */
	}

	/* set up the "System" interface */

	interfaces = calloc(1, sizeof(*interfaces));
	if (interfaces == NULL)
		err(1, "calloc: interfaces");

	ifr = calloc(MCLPOOLS, sizeof(*ifr));
	if (ifr == NULL)
		err(1, "calloc: system pools");

	strlcpy(interfaces[0].name, "System", sizeof(interfaces[0].name));
	interfaces[0].data.ifri_total = MCLPOOLS;
	interfaces[0].data.ifri_entries = ifr;
	num_ifs = 1;

	/* go through all pools to identify mbuf and cluster pools */

	mib[0] = CTL_KERN;
	mib[1] = KERN_POOL;
	mib[2] = KERN_POOL_NPOOLS;
	size = sizeof(npools);

	if (sysctl(mib, 3, &npools, &size, NULL, 0) < 0) {
		err(1, "sysctl(KERN_POOL_NPOOLS)");
		/* NOTREACHED */
	}

	for (i = 1; i <= npools; i++) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_POOL;
		mib[2] = KERN_POOL_NAME;
		mib[3] = i;
		size = sizeof(pname);
		if (sysctl(mib, 4, &pname, &size, NULL, 0) < 0) {
			continue;
		}

		if (strcmp(pname, "mbufpl") == 0) {
			mbpool_index = i;
			continue;
		}

		if (strncmp(pname, "mcl", 3) != 0)
			continue;

		if (mclpool_count == MCLPOOLS) {
			warnx("mbufs: Too many mcl* pools");
			break;
		}

		mib[2] = KERN_POOL_POOL;
		size = sizeof(pool);

		if (sysctl(mib, 4, &pool, &size, NULL, 0) < 0) {
			err(1, "sysctl(KERN_POOL_POOL, %d)", i);
			/* NOTREACHED */
		}

		snprintf(ifr[mclpool_count].ifr_name,
		    sizeof(ifr[mclpool_count].ifr_name), "%dk",
		    pool.pr_size / 1024);
		ifr[mclpool_count].ifr_size = pool.pr_size;

		mclpools_index[mclpool_count++] = i;
	}

	if (mclpool_count != MCLPOOLS)
		warnx("mbufs: Unable to read all %d mcl* pools", MCLPOOLS);

	/* add view to the engine */
	for (v = views_mb; v->name != NULL; v++)
		add_view(v);

	/* finally read it once */
	read_mb();

	return(1);
}

int
select_mb(void)
{
	num_disp = 0;
	return (0);
}

int
read_mb(void)
{
	struct kinfo_pool pool;
	struct ifaddrs *ifap, *ifa;
	struct if_info *ifi;
	struct if_rxring_info *ifr;
	int mib[4];
	int i, p, nif, ret = 1, rv;
	u_int rings;
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_NETLIVELOCKS;
	size = sizeof(mcllivelocks_cur);
	if (sysctl(mib, 2, &mcllivelocks_cur, &size, NULL, 0) < 0 &&
	    errno != EOPNOTSUPP) {
		error("sysctl(KERN_NETLIVELOCKS)");
		goto exit;
	}
	mcllivelocks_diff = mcllivelocks_cur - mcllivelocks;
	mcllivelocks = mcllivelocks_cur;

	num_disp = 0;
	if (getifaddrs(&ifap)) {
		error("getifaddrs: %s", strerror(errno));
		return (1);
	}

	nif = 1;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		nif++;
	}

	if (num_ifs < nif) {
		ifi = reallocarray(interfaces, nif, sizeof(*interfaces));
		if (ifi == NULL) {
			error("reallocarray: %d interfaces", nif);
			goto exit;
		}

		interfaces = ifi;
		while (num_ifs < nif)
			memset(&interfaces[num_ifs++], 0, sizeof(*interfaces));
	}

	/* Fill in the "real" interfaces */
	ifi = interfaces + 1;

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_LINK)
			continue;
			
		strlcpy(ifi->name, ifa->ifa_name, sizeof(ifi->name));
		for (;;) {
			struct ifreq ifreq;
			rings = ifi->data.ifri_total;

			memset(&ifreq, 0, sizeof(ifreq));
			strlcpy(ifreq.ifr_name, ifa->ifa_name,
			    sizeof(ifreq.ifr_name));
			ifreq.ifr_data = (caddr_t)&ifi->data;

			rv = ioctl(sock, SIOCGIFRXR, &ifreq);
			if (rv == -1) {
				if (errno == ENOTTY) {
					free(ifi->data.ifri_entries);
					ifi->data.ifri_total = 0;
					ifi->data.ifri_entries = NULL;
					break;
				}

				error("ioctl(SIOCGIFRXR) %s", strerror(errno));
				break;
			}

			if (rings >= ifi->data.ifri_total)
				break;  

			ifr = reallocarray(ifi->data.ifri_entries,
			    ifi->data.ifri_total, sizeof(*ifr));
			if (ifr == NULL) {
				ifi->data.ifri_total = rings;
				error("reallocarray: %u rings",
				    ifi->data.ifri_total);
				goto exit;
			}

			ifi->data.ifri_entries = ifr;
		}

		ifi++;
	}

	/* Fill in the "System" entry from pools */

	mib[0] = CTL_KERN;
	mib[1] = KERN_POOL;
	mib[2] = KERN_POOL_POOL;
	mib[3] = mbpool_index;
	size = sizeof(mbpool);

	if (sysctl(mib, 4, &mbpool, &size, NULL, 0) < 0) {
		error("sysctl(KERN_POOL_POOL, %d)", mib[3]);
		goto exit;
	}

	for (i = 0; i < mclpool_count; i++) {
		ifr = &interfaces[0].data.ifri_entries[i];

		mib[3] = mclpools_index[i];
		size = sizeof(pool);

		if (sysctl(mib, 4, &pool, &size, NULL, 0) < 0) {
			error("sysctl(KERN_POOL_POOL, %d)", mib[3]);
			continue;
		}

		ifr->ifr_info.rxr_alive = pool.pr_nget - pool.pr_nput;
		ifr->ifr_info.rxr_hwm = pool.pr_hiwat;
	}

	num_disp = 1;
	ret = 0;

	for (i = 0; i < num_ifs; i++) {
		struct if_info *ifi = &interfaces[i];
		int pnd = num_disp;
		for (p = 0; p < ifi->data.ifri_total; p++) {
			ifr = &ifi->data.ifri_entries[p];
			if (ifr->ifr_info.rxr_alive == 0)
				continue;
			num_disp++;
		}
		if (i && pnd == num_disp)
			num_disp++;
	}

 exit:
	freeifaddrs(ifap);
	return (ret);
}

void
print_mb(void)
{
	int i, p, n, count = 0;

	showmbuf(interfaces, -1, 1);

	for (n = i = 0; i < num_ifs; i++) {
		struct if_info *ifi = &interfaces[i];
		int pcnt = count;
		int showif = i;

		if (maxprint > 0 && count >= maxprint)
			return;

		for (p = 0; p < ifi->data.ifri_total; p++) {
			struct if_rxring_info *ifr = &ifi->data.ifri_entries[p];
			if (ifr->ifr_info.rxr_alive == 0)
				continue;
			if (n++ >= dispstart) {
				showmbuf(ifi, p, showif);
				showif = 0;
				count++;
			}
		}

		if (i && pcnt == count) {
			/* only print the first line */
			if (n++ >= dispstart) {
				showmbuf(ifi, -1, 1);
				count++;
			}
		}
	}
}


static void
showmbuf(struct if_info *ifi, int p, int showif)
{
	if (showif)
		print_fld_str(FLD_MB_IFACE, ifi->name);

	if (p == -1 && ifi == interfaces) {
		print_fld_uint(FLD_MB_LLOCKS, mcllivelocks_diff);
		print_fld_size(FLD_MB_MSIZE, mbpool.pr_size);
		print_fld_size(FLD_MB_MALIVE, mbpool.pr_nget - mbpool.pr_nput);
		print_fld_size(FLD_MB_MHWM, mbpool.pr_hiwat);
	}

	if (p >= 0 && p < mclpool_count) {
		struct if_rxring_info *ifr = &ifi->data.ifri_entries[p];
		struct if_rxring *rxr= &ifr->ifr_info;
		print_fld_uint(FLD_MB_MSIZE, ifr->ifr_size);
		print_fld_uint(FLD_MB_MALIVE, rxr->rxr_alive);
		if (rxr->rxr_lwm)
			print_fld_size(FLD_MB_MLWM, rxr->rxr_lwm);
		if (rxr->rxr_hwm)
			print_fld_size(FLD_MB_MHWM, rxr->rxr_hwm);
		if (rxr->rxr_cwm)
			print_fld_size(FLD_MB_MCWM, rxr->rxr_cwm);
	}

	end_line();
}
