/*	$OpenBSD: tbrconfig.c,v 1.3 2002/02/15 03:31:16 deraadt Exp $	*/
/*	$KAME: tbrconfig.c,v 1.3 2001/05/08 04:36:39 itojun Exp $	*/
/*
 * Copyright (C) 2000
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include <altq/altq.h>

#define	ALTQ_DEVICE	"/dev/altq/altq"

static void usage(void);
static u_long atobps(const char *s);
static u_long atobytes(const char *s);
static u_int size_bucket(const char *ifname, const u_int rate);
static u_int autosize_bucket(const char *ifname, const u_int rate);
static int get_clockfreq(void);
static int get_ifmtu(const char *ifname);
static void list_all(void);

static void
usage(void)
{
	fprintf(stderr, "usage: tbrconfig interface [tokenrate [bucketsize]\n");
	fprintf(stderr, "       tbrconfig -d interface\n");
	fprintf(stderr, "       tbrconfig -a\n");
	exit(1);
}

int 
main(int argc, char **argv)
{
	struct tbrreq req;
	u_int rate, depth;
	int fd, ch, delete;

	delete = 0;
	rate = 0;
	depth = 0;

	while ((ch = getopt(argc, argv, "ad")) != -1) {
		switch (ch) {
		case 'a':
			list_all();
			return (0);
		case 'd':
			delete = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	req.ifname[IFNAMSIZ-1] = '\0';
	strncpy(req.ifname, argv[0], IFNAMSIZ-1);
	if (argc > 1)
		rate = (u_int)atobps(argv[1]);
	if (argc > 2) {
		if (strncmp(argv[2], "auto", strlen("auto")) == 0)
			depth = autosize_bucket(req.ifname, rate);
		else
			depth = (u_int)atobytes(argv[2]);
	}
	if (argc > 3)
		usage();

	if (delete || rate > 0) {
		/* set token bucket regulator */
		if (delete)
			rate = 0;
		else if (depth == 0)
			depth = size_bucket(req.ifname, rate);

		req.tb_prof.rate = rate;
		req.tb_prof.depth = depth;

		if ((fd = open(ALTQ_DEVICE, O_RDWR)) < 0)
			err(1, "can't open altq device");

		if (ioctl(fd, ALTQTBRSET, &req) < 0)
			err(1, "ALTQTBRSET for interface %s", req.ifname);

		close(fd);

		if (delete) {
			printf("deleted token bucket regulator on %s\n",
			       req.ifname);
			return (0);
		}
	}

	/* get token bucket regulator */
	if ((fd = open(ALTQ_DEVICE, O_RDONLY)) < 0)
		err(1, "can't open altq device");
	if (ioctl(fd, ALTQTBRGET, &req) < 0)
		err(1, "ALTQTBRGET for interface %s", req.ifname);
	if (req.tb_prof.rate == 0)
		printf("no token bucket regulater found on %s\n", req.ifname);
	else {
		char rate_str[64], size_str[64];

		if (req.tb_prof.rate < 999999)
			snprintf(rate_str, sizeof rate_str, "%.2fK",
				(double)req.tb_prof.rate/1000.0);
		else
			snprintf(rate_str, sizeof rate_str, "%.2fM",
				(double)req.tb_prof.rate/1000000.0);
		if (req.tb_prof.depth < 10240)
			snprintf(size_str, sizeof size_str, "%u", req.tb_prof.depth);
		else
			snprintf(size_str, sizeof size_str, "%.2fK",
				(double)req.tb_prof.depth/1024.0);
		printf("%s: tokenrate %s(bps)  bucketsize %s(bytes)\n",
		       req.ifname, rate_str, size_str);
	}
	close(fd);
	return (0);
}

static void
list_all(void)
{
	struct if_nameindex *ifn_list, *ifnp;
	struct tbrreq req;
	char rate_str[64], size_str[64];
	int fd, ntbr;

	if ((ifn_list = if_nameindex()) == NULL)
		err(1, "if_nameindex failed");

	if ((fd = open(ALTQ_DEVICE, O_RDONLY)) < 0)
		err(1, "can't open altq device");

	ntbr = 0;
	for (ifnp = ifn_list; ifnp->if_name != NULL; ifnp++) {
		req.ifname[IFNAMSIZ-1] = '\0';
		strncpy(req.ifname, ifnp->if_name, IFNAMSIZ-1);
		if (ioctl(fd, ALTQTBRGET, &req) < 0)
			err(1, "ALTQTBRGET");
		if (req.tb_prof.rate == 0)
			continue;

		if (req.tb_prof.rate < 999999)
			snprintf(rate_str, sizeof rate_str, "%.2fK",
				(double)req.tb_prof.rate/1000.0);
		else
			snprintf(rate_str, sizeof rate_str, "%.2fM",
				(double)req.tb_prof.rate/1000000.0);
		if (req.tb_prof.depth < 10240)
			snprintf(size_str, sizeof size_str,
				"%u", req.tb_prof.depth);
		else
			snprintf(size_str, sizeof size_str, "%.2fK",
				(double)req.tb_prof.depth/1024.0);
		printf("%s: tokenrate %s(bps)  bucketsize %s(bytes)\n",
		       req.ifname, rate_str, size_str);
		ntbr++;
	}
	if (ntbr == 0)
		printf("no active token bucket regulator\n");

	close(fd);
	if_freenameindex(ifn_list);
}

static u_long
atobps(const char *s)
{
	double bandwidth;
	char *cp;
			
	bandwidth = strtod(s, &cp);
	if (cp != NULL) {
		if (*cp == 'K' || *cp == 'k')
			bandwidth *= 1000;
		else if (*cp == 'M' || *cp == 'm')
			bandwidth *= 1000000;
		else if (*cp == 'G' || *cp == 'g')
			bandwidth *= 1000000000;
	}
	if (bandwidth < 0)
		bandwidth = 0;
	return ((u_long)bandwidth);
}

static u_long
atobytes(const char *s)
{
	double bytes;
	char *cp;
			
	bytes = strtod(s, &cp);
	if (cp != NULL) {
		if (*cp == 'K' || *cp == 'k')
			bytes *= 1024;
		else if (*cp == 'M' || *cp == 'm')
			bytes *= 1024 * 1024;
		else if (*cp == 'G' || *cp == 'g')
			bytes *= 1024 * 1024 * 1024;
	}
	if (bytes < 0)
		bytes = 0;
	return ((u_long)bytes);
}

/*
 * use heuristics to determin the bucket size
 */
static u_int
size_bucket(const char *ifname, const u_int rate)
{
	u_int size, mtu;

	mtu = get_ifmtu(ifname);
	if (mtu > 1500)
		mtu = 1500;	/* assume that the path mtu is still 1500 */

	if (rate <= 1*1000*1000)
		size = 1;
	else if (rate <= 10*1000*1000)
		size = 4;
	else if (rate <= 200*1000*1000)
		size = 8;
	else
		size = 24;

	size = size * mtu;
	return (size);
}

/*
 * compute the bucket size to be required to fill the rate
 * even when the rate is controlled only by the kernel timer.
 */
static u_int
autosize_bucket(const char *ifname, const u_int rate)
{
	u_int size, freq, mtu;

	mtu = get_ifmtu(ifname);
	freq = get_clockfreq();
	size = rate / 8 / freq;
	if (size < mtu)
		size = mtu;
	return (size);
}

static int
get_clockfreq(void)
{
	struct clockinfo clkinfo;
	int mib[2];
	size_t len;

	clkinfo.hz = 100; /* default Hz */

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	len = sizeof(struct clockinfo);
	if (sysctl(mib, 2, &clkinfo, &len, NULL, 0) == -1)
		warnx("can't get clockrate via sysctl! use %dHz", clkinfo.hz);
	return (clkinfo.hz);
}

static int 
get_ifmtu(const char *ifname)
{
	int s, mtu;
	struct ifreq ifr;
#ifdef __OpenBSD__
	struct if_data ifdata;
#endif

	mtu = 512; /* default MTU */

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return (mtu);
	strncpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
#ifdef __OpenBSD__
	ifr.ifr_data = (caddr_t)&ifdata;
	if (ioctl(s, SIOCGIFDATA, (caddr_t)&ifr) == 0)
		mtu = ifdata.ifi_mtu;
#else
	if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) == 0)
		mtu = ifr.ifr_mtu;
#endif
	close(s);
	return (mtu);
}
