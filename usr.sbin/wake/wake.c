/*	$OpenBSD: wake.c,v 1.7 2009/01/29 13:12:21 pyr Exp $ */

/*
 * Copyright (C) 2006-2008 Marc Balmer.
 * Copyright (C) 2000 Eugene M. Kim.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Author's name may not be used endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/bpf.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#ifndef BPF_PATH_FORMAT
#define BPF_PATH_FORMAT "/dev/bpf%u"
#endif

#ifndef SYNC_LEN
#define SYNC_LEN 6
#endif

#ifndef DESTADDR_COUNT
#define DESTADDR_COUNT 16
#endif

int	get_bpf(void);
int	bind_if_to_bpf(char const *ifname, int bpf);
int	get_ether(char const *text, struct ether_addr *addr);
void	send_wakeup(int bpf, struct ether_addr const *addr);
void	usage(void);

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s interface lladdr\n", __progname);
	exit(0);
}

int
wake(const char *iface, const char *host)
{
	int bpf;
	struct ether_addr macaddr;

	bpf = get_bpf();
	if (bpf == -1)
		return -1;
	if (bind_if_to_bpf(iface, bpf) == -1) {
		close(bpf);
		return -1;
	}
	if (get_ether(host, &macaddr) == -1) {
		close(bpf);
		return -1;
	}
	send_wakeup(bpf, &macaddr);
	close(bpf);
	return 0;
}

int
get_bpf(void)
{
	int i, fd;
	char *path;

	for (i = 0;; i++) {
		asprintf(&path, BPF_PATH_FORMAT, i);
		if (path == NULL)
			return -1;

		fd = open(path, O_RDWR);
		free(path);
		if (fd != -1)
			return fd;
		if (errno == EBUSY)
			continue;
		break;
	}
	return -1;
}

int
bind_if_to_bpf(char const *ifname, int bpf)
{
	struct ifreq ifr;
	u_int dlt;

	if (strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)) >=
	    sizeof(ifr.ifr_name))
		return -1;
	if (ioctl(bpf, BIOCSETIF, &ifr) == -1)
		return -1;
	if (ioctl(bpf, BIOCGDLT, &dlt) == -1)
		return -1;
	if (dlt != DLT_EN10MB)
		return -1;
	return 0;
}

int
get_ether(char const *text, struct ether_addr *addr)
{
	struct ether_addr *paddr;
	paddr = ether_aton(text);
	if (paddr != NULL) {
		*addr = *paddr;
		return 0;
	}
	if (ether_hostton(text, addr))
		return -1;
	return 0;
}

void
send_wakeup(int bpf, struct ether_addr const *addr)
{
	struct {
		struct ether_header hdr;
		u_char data[SYNC_LEN + ETHER_ADDR_LEN * DESTADDR_COUNT];
	} pkt;
	u_char *p;
	int i;
	ssize_t bw;
	ssize_t len;

	memset(pkt.hdr.ether_dhost, 0xff, sizeof(pkt.hdr.ether_dhost));
	pkt.hdr.ether_type = htons(0);
	memset(pkt.data, 0xff, SYNC_LEN);
	for (p = pkt.data + SYNC_LEN, i = 0; i < DESTADDR_COUNT;
	    p += ETHER_ADDR_LEN, i++)
		bcopy(addr->ether_addr_octet, p, ETHER_ADDR_LEN);
	p = (u_char *)&pkt;
	len = sizeof(pkt);
	bw = 0;
	while (len) {
		if ((bw = write(bpf, &pkt, sizeof(pkt))) == -1)
			err(1, "cannot send wake on lan frame");
		len -= bw;
		p += bw;
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 3)
		usage();

	if (wake(argv[1], argv[2]))
		err(1, "error sending Wake on LAN frame over %s to %s",
		    argv[1], argv[2]);
	return 0;
}
