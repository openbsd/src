/*	$OpenBSD: rarpd.c,v 1.19 1998/05/03 22:49:09 d Exp $ */
/*	$NetBSD: rarpd.c,v 1.25 1998/04/23 02:48:33 mrg Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
char    copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif				/* not lint */

#ifndef lint
static char rcsid[] = "$OpenBSD: rarpd.c,v 1.19 1998/05/03 22:49:09 d Exp $";
#endif


/*
 * rarpd - Reverse ARP Daemon
 *
 * Usage:	rarpd -a [ -d ] [ -f ] [ -l ]
 *		rarpd [ -d ] [ -f ] [ -l ] interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "pathnames.h"

#define FATAL		1	/* fatal error occurred */
#define NONFATAL	0	/* non fatal error occurred */

/*
 * The structure for each interface.
 */
struct if_info {
	int     ii_fd;			/* BPF file descriptor */
	char	ii_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	u_char  ii_eaddr[6];		/* Ethernet address of this interface */
	u_int32_t ii_ipaddr;		/* IP address of this interface */
	u_int32_t ii_netmask;		/* subnet or net mask */
	struct if_info *ii_next;
};
/*
 * The list of all interfaces that are being listened to.  rarp_loop()
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

int    rarp_open     __P((char *));
void   init_one      __P((char *));
void   init_all      __P((void));
void   rarp_loop     __P((void));
void   lookup_eaddr  __P((char *, u_char *));
void   lookup_ipaddr __P((char *, u_int32_t *, u_int32_t *));
void   usage         __P((void));
void   rarp_process  __P((struct if_info *, u_char *));
void   rarp_reply    __P((struct if_info *, struct ether_header *, u_int32_t,
	struct hostent *));
void   update_arptab __P((u_char *, u_int32_t));
void   err           __P((int, const char *,...));
void   debug         __P((const char *,...));
u_int32_t ipaddrtonetmask __P((u_int32_t));

#ifdef REQUIRE_TFTPBOOT
int    rarp_bootable __P((u_int32_t));
#endif

int     aflag = 0;		/* listen on "all" interfaces  */
int     dflag = 0;		/* print debugging messages */
int     fflag = 0;		/* don't fork */
int     lflag = 0;		/* log all replies */

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int     op, pid, devnull, f;
	char   *ifname, *hostname;
	extern char *__progname;
	extern char *optarg;
	extern int optind, opterr;

	/* All error reporting is done through syslogs. */
	openlog(__progname, LOG_PID | LOG_CONS, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "adfl")) != -1) {
		switch (op) {
		case 'a':
			++aflag;
			break;

		case 'd':
			++dflag;
			break;

		case 'f':
			++fflag;
			break;

		case 'l':
			++lflag;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}
	ifname = argv[optind++];
	hostname = ifname ? argv[optind] : 0;
	if ((aflag && ifname) || (!aflag && ifname == 0))
		usage();

	if (aflag)
		init_all();
	else
		init_one(ifname);

	if ((!fflag) && (!dflag)) {
		FILE *fp;

		pid = fork();
		if (pid > 0)
			/* Parent exits, leaving child in background. */
			exit(0);
		else
			if (pid == -1) {
				err(FATAL, "cannot fork");
				/* NOTREACHED */
			}

		/* write pid file */
		if ((fp = fopen(_PATH_RARPDPID, "w")) != NULL) {
			fprintf(fp, "%u\n", getpid());
			(void)fclose(fp);
		}

		/* Fade into the background */
		f = open("/dev/tty", O_RDWR);
		if (f >= 0) {
			if (ioctl(f, TIOCNOTTY, 0) < 0) {
				err(FATAL, "TIOCNOTTY: %s", strerror(errno));
				/* NOTREACHED */
			}
			(void) close(f);
		}
		(void) chdir("/");
		(void) setpgrp(0, getpid());
		devnull = open("/dev/null", O_RDWR);
		if (devnull >= 0) {
			(void) dup2(devnull, 0);
			(void) dup2(devnull, 1);
			(void) dup2(devnull, 2);
			if (devnull > 2)
				(void) close(devnull);
		}
	}
	rarp_loop();
	exit(0);
}

/*
 * Add 'ifname' to the interface list.  Lookup its IP address and network
 * mask and Ethernet address, and open a BPF file for it.
 */
void
init_one(ifname)
	char   *ifname;
{
	struct if_info *p;
	int fd;

	/* first check to see if this "if" was already opened? */
	for(p = iflist; p; p = p->ii_next)
		if (!strncmp(p->ii_name, ifname, IFNAMSIZ))
			return;

	fd = rarp_open(ifname);
	if (fd < 0)
		return;

	p = (struct if_info *)malloc(sizeof(*p));
	if (p == 0) {
		err(FATAL, "malloc: %s", strerror(errno));
		/* NOTREACHED */
	}

	p->ii_next = iflist;
	iflist = p;

	p->ii_fd = fd;
	strncpy(p->ii_name, ifname, IFNAMSIZ);
	lookup_eaddr(ifname, p->ii_eaddr);
	lookup_ipaddr(ifname, &p->ii_ipaddr, &p->ii_netmask);
}
/*
 * Initialize all "candidate" interfaces that are in the system
 * configuration list.  A "candidate" is up, not loopback and not
 * point to point.
 */
void
init_all()
{
	char *inbuf = NULL;
	struct ifconf ifc;
	struct ifreq *ifr;
	int fd, inlen = 8192;
	int i, len;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		err(FATAL, "socket: %s", strerror(errno));
		/* NOTREACHED */
	}

	while (1) {
		ifc.ifc_len = inlen;
		ifc.ifc_buf = inbuf = realloc(inbuf, inlen);
		if (inbuf == NULL) {
			close(fd);
			err(FATAL, "init_all: malloc: %s", strerror(errno));
		}
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
			(void) close(fd);
			free(inbuf);
			err(FATAL, "init_all: SIOCGIFCONF: %s", strerror(errno));
			/* NOTREACHED */
		}
		if (ifc.ifc_len + sizeof(*ifr) < inlen)
			break;
		inlen *= 2;
	}

	ifr = ifc.ifc_req;
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifr = (struct ifreq *)((caddr_t)ifr + len)) {
		len = sizeof(ifr->ifr_name) +
		      (ifr->ifr_addr.sa_len > sizeof(struct sockaddr) ?
		       ifr->ifr_addr.sa_len : sizeof(struct sockaddr));

		if (ioctl(fd, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
			free(inbuf);
			err(FATAL, "init_all: SIOCGIFFLAGS %s: %s",
			    ifr->ifr_name, strerror(errno));
			/* NOTREACHED */
		}
		if ((ifr->ifr_flags &
		    (IFF_UP | IFF_LOOPBACK | IFF_POINTOPOINT)) != IFF_UP)
			continue;
		init_one(ifr->ifr_name);
	}
	free(inbuf);
	(void) close(fd);
}

void
usage()
{
	(void) fprintf(stderr, "usage: rarpd -a [ -d -f ]\n");
	(void) fprintf(stderr, "       rarpd [ -d -f ] interface\n");
	exit(1);
}

static int
bpf_open()
{
	int     fd;
	int     n = 0;
	char    device[sizeof "/dev/bpf000"];

	/* Go through all the minors and find one that isn't in use. */
	do {
		(void) sprintf(device, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
		err(FATAL, "%s: %s", device, strerror(errno));
		/* NOTREACHED */
	}
	return fd;
}
/*
 * Open a BPF file and attach it to the interface named 'device'.
 * Set immediate mode, and set a filter that accepts only RARP requests.
 */
int
rarp_open(device)
	char   *device;
{
	int     fd;
	struct ifreq ifr;
	u_int   dlt;
	int     immediate;

	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ETHERTYPE_REVARP, 0, 3),
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 20),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ARPOP_REVREQUEST, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, sizeof(struct ether_arp) +
		    sizeof(struct ether_header)),
		BPF_STMT(BPF_RET | BPF_K, 0),
	};
	static struct bpf_program filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};

	fd = bpf_open();

	/* Set immediate mode so packets are processed as they arrive. */
	immediate = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &immediate) < 0) {
		err(FATAL, "BIOCIMMEDIATE: %s", strerror(errno));
		/* NOTREACHED */
	}
	(void) strncpy(ifr.ifr_name, device, sizeof ifr.ifr_name);
	if (ioctl(fd, BIOCSETIF, (caddr_t) & ifr) < 0) {
		if (aflag) {	/* for -a skip not ethernet interfaces */
			close(fd);
			return -1;
		}
		err(FATAL, "BIOCSETIF: %s", strerror(errno));
		/* NOTREACHED */
	}
	/* Check that the data link layer is an Ethernet; this code won't work
	 * with anything else. */
	if (ioctl(fd, BIOCGDLT, (caddr_t) & dlt) < 0) {
		err(FATAL, "BIOCGDLT: %s", strerror(errno));
		/* NOTREACHED */
	}
	if (dlt != DLT_EN10MB) {
		if (aflag) {	/* for -a skip not ethernet interfaces */
			close(fd);
			return -1;
		}
		err(FATAL, "%s is not an ethernet", device);
		/* NOTREACHED */
	}
	/* Set filter program. */
	if (ioctl(fd, BIOCSETF, (caddr_t) & filter) < 0) {
		err(FATAL, "BIOCSETF: %s", strerror(errno));
		/* NOTREACHED */
	}
	return fd;
}
/*
 * Perform various sanity checks on the RARP request packet.  Return
 * false on failure and log the reason.
 */
static int
rarp_check(p, len)
	u_char *p;
	int     len;
{
	struct ether_header *ep = (struct ether_header *) p;
	struct ether_arp *ap = (struct ether_arp *) (p + sizeof(*ep));

	(void) debug("got a packet");

	if (len < sizeof(*ep) + sizeof(*ap)) {
		err(NONFATAL, "truncated request");
		return 0;
	}
	/* XXX This test might be better off broken out... */
	if (ntohs (ep->ether_type) != ETHERTYPE_REVARP ||
	    ntohs (ap->arp_hrd) != ARPHRD_ETHER ||
	    ntohs (ap->arp_op) != ARPOP_REVREQUEST ||
	    ntohs (ap->arp_pro) != ETHERTYPE_IP ||
	    ap->arp_hln != 6 || ap->arp_pln != 4) {
		err(NONFATAL, "request fails sanity check");
		return 0;
	}
	if (memcmp((char *) &ep->ether_shost, (char *) &ap->arp_sha, 6) != 0) {
		err(NONFATAL, "ether/arp sender address mismatch");
		return 0;
	}
	if (memcmp((char *) &ap->arp_sha, (char *) &ap->arp_tha, 6) != 0) {
		err(NONFATAL, "ether/arp target address mismatch");
		return 0;
	}
	return 1;
}

/*
 * Loop indefinitely listening for RARP requests on the
 * interfaces in 'iflist'.
 */
void
rarp_loop()
{
	u_char *buf, *bp, *ep;
	int     cc, fd;
	fd_set  *fdsp, *lfdsp;
	int	fdsn;
	int     bufsize, maxfd = 0;
	struct if_info *ii;

	if (iflist == 0) {
		err(FATAL, "no interfaces");
		/* NOTREACHED */
	}
	if (ioctl(iflist->ii_fd, BIOCGBLEN, (caddr_t) & bufsize) < 0) {
		err(FATAL, "BIOCGBLEN: %s", strerror(errno));
		/* NOTREACHED */
	}
	buf = (u_char *) malloc((unsigned) bufsize);
	if (buf == 0) {
		err(FATAL, "malloc: %s", strerror(errno));
		/* NOTREACHED */
	}
	/*
         * Find the highest numbered file descriptor for select().
         * Initialize the set of descriptors to listen to.
         */
	for (ii = iflist; ii; ii = ii->ii_next)
		if (ii->ii_fd > maxfd)
			maxfd = ii->ii_fd;

	fdsn = howmany(maxfd+1, NFDBITS) * sizeof(fd_mask);
	if ((fdsp = (fd_set *)malloc(fdsn)) == NULL)
		err(1, "malloc");
	if ((lfdsp = (fd_set *)malloc(fdsn)) == NULL)
		err(1, "malloc");

	memset(fdsp, 0, fdsn);
	for (ii = iflist; ii; ii = ii->ii_next)
		FD_SET(ii->ii_fd, fdsp);

	while (1) {
		memcpy(lfdsp, fdsp, fdsn);
		if (select(maxfd + 1, lfdsp, (struct fd_set *) 0,
			(struct fd_set *) 0, (struct timeval *) 0) < 0) {
			err(FATAL, "select: %s", strerror(errno));
			/* NOTREACHED */
		}
		for (ii = iflist; ii; ii = ii->ii_next) {
			fd = ii->ii_fd;
			if (!FD_ISSET(fd, lfdsp))
				continue;
		again:
			cc = read(fd, (char *) buf, bufsize);
			/* Don't choke when we get ptraced */
			if (cc < 0 && errno == EINTR)
				goto again;
			/* Due to a SunOS bug, after 2^31 bytes, the file
			 * offset overflows and read fails with EINVAL.  The
			 * lseek() to 0 will fix things. */
			if (cc < 0) {
				if (errno == EINVAL &&
				    (lseek(fd, 0, SEEK_CUR) + bufsize) < 0) {
					(void) lseek(fd, 0, 0);
					goto again;
				}
				err(FATAL, "read: %s", strerror(errno));
				/* NOTREACHED */
			}
			/* Loop through the packet(s) */
#define bhp ((struct bpf_hdr *)bp)
			bp = buf;
			ep = bp + cc;
			while (bp < ep) {
				register int caplen, hdrlen;

				caplen = bhp->bh_caplen;
				hdrlen = bhp->bh_hdrlen;
				if (rarp_check(bp + hdrlen, caplen))
					rarp_process(ii, bp + hdrlen);
				bp += BPF_WORDALIGN(hdrlen + caplen);
			}
		}
	}
	free(fdsp);
	free(lfdsp);
}

#ifdef REQUIRE_TFTPBOOT

#ifndef TFTP_DIR
#define TFTP_DIR "/tftpboot"
#endif

/*
 * True if this server can boot the host whose IP address is 'addr'.
 * This check is made by looking in the tftp directory for the
 * configuration file.
 */
int
rarp_bootable(addr)
	u_int32_t  addr;
{
	register struct dirent *dent;
	register DIR *d;
	char    ipname[9];
	static DIR *dd = 0;

	(void) sprintf(ipname, "%08X", addr);
	/* If directory is already open, rewind it.  Otherwise, open it. */
	if (d = dd)
		rewinddir(d);
	else {
		if (chdir(TFTP_DIR) == -1) {
			err(FATAL, "chdir: %s", strerror(errno));
			/* NOTREACHED */
		}
		d = opendir(".");
		if (d == 0) {
			err(FATAL, "opendir: %s", strerror(errno));
			/* NOTREACHED */
		}
		dd = d;
	}
	while (dent = readdir(d))
		if (strncmp(dent->d_name, ipname, 8) == 0)
			return 1;
	return 0;
}
#endif /* REQUIRE_TFTPBOOT */

/*
 * Given a list of IP addresses, 'alist', return the first address that
 * is on network 'net'; 'netmask' is a mask indicating the network portion
 * of the address.
 */
u_int32_t
choose_ipaddr(alist, net, netmask)
	u_int32_t **alist;
	u_int32_t  net;
	u_int32_t  netmask;
{
	for (; *alist; ++alist) {
		if ((**alist & netmask) == net)
			return **alist;
	}
	return 0;
}
/*
 * Answer the RARP request in 'pkt', on the interface 'ii'.  'pkt' has
 * already been checked for validity.  The reply is overlaid on the request.
 */
void
rarp_process(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
	struct ether_header *ep;
	struct hostent *hp;
	u_int32_t  target_ipaddr;
	char    ename[MAXHOSTNAMELEN];
	struct	in_addr in;

	ep = (struct ether_header *) pkt;

	if (ether_ntohost(ename, (struct ether_addr *)&ep->ether_shost) != 0 ||
	    (hp = gethostbyname(ename)) == 0)
		return;

	/* Choose correct address from list. */
	if (hp->h_addrtype != AF_INET) {
		err(FATAL, "cannot handle non IP addresses");
		/* NOTREACHED */
	}
	target_ipaddr = choose_ipaddr((u_int32_t **) hp->h_addr_list,
	    ii->ii_ipaddr & ii->ii_netmask, ii->ii_netmask);

	if (target_ipaddr == 0) {
		in.s_addr = ii->ii_ipaddr & ii->ii_netmask;
		err(NONFATAL, "cannot find %s on net %s",
		    ename, inet_ntoa(in));
		return;
	}
#ifdef REQUIRE_TFTPBOOT
	if (rarp_bootable(htonl(target_ipaddr)))
#endif
		rarp_reply(ii, ep, target_ipaddr, hp);
}
/*
 * Lookup the ethernet address of the interface attached to the BPF
 * file descriptor 'fd'; return it in 'eaddr'.
 */
void
lookup_eaddr(ifname, eaddr)
	char *ifname;
	u_char *eaddr;
{
	char *inbuf = NULL;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_dl *sdl;
	int fd;
	int i, len, inlen = 8192;

	/* We cannot use SIOCGIFADDR on the BPF descriptor.
	   We must instead get all the interfaces with SIOCGIFCONF
	   and find the right one.  */

	/* Use datagram socket to get Ethernet address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		err(FATAL, "socket: %s", strerror(errno));
		/* NOTREACHED */
	}

	while (1) {
		ifc.ifc_len = inlen;
		ifc.ifc_buf = inbuf = realloc(inbuf, inlen);
		if (inbuf == NULL) {
			close(fd);
			err(FATAL, "init_all: malloc: %s", strerror(errno));
		}
		if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
			(void) close(fd);
			free(inbuf);
			err(FATAL, "init_all: SIOCGIFCONF: %s", strerror(errno));
			/* NOTREACHED */
		}
		if (ifc.ifc_len + sizeof(*ifr) < inlen)
			break;
		inlen *= 2;
	}

	ifr = ifc.ifc_req;
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifr = (struct ifreq *)((caddr_t)ifr + len)) {
		len = sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
		sdl = (struct sockaddr_dl *)&ifr->ifr_addr;
		if (sdl->sdl_family != AF_LINK || sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != 6)
			continue;
		if (!strncmp(ifr->ifr_name, ifname, sizeof(ifr->ifr_name))) {
			memcpy((caddr_t)eaddr, (caddr_t)LLADDR(sdl), 6);
			if (dflag)
				fprintf(stderr, "%s: %x:%x:%x:%x:%x:%x\n",
				    ifr->ifr_name, eaddr[0], eaddr[1],
				    eaddr[2], eaddr[3], eaddr[4], eaddr[5]);
			free(inbuf);
			return;
		}
	}

	free(inbuf);
	err(FATAL, "lookup_eaddr: Never saw interface `%s'!", ifname);
}
/*
 * Lookup the IP address and network mask of the interface named 'ifname'.
 */
void
lookup_ipaddr(ifname, addrp, netmaskp)
	char   *ifname;
	u_int32_t *addrp;
	u_int32_t *netmaskp;
{
	int     fd;
	struct ifreq ifr;

	/* Use datagram socket to get IP address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		err(FATAL, "socket: %s", strerror(errno));
		/* NOTREACHED */
	}
	(void) strncpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
	if (ioctl(fd, SIOCGIFADDR, (char *) &ifr) < 0) {
		err(FATAL, "SIOCGIFADDR: %s", strerror(errno));
		/* NOTREACHED */
	}
	*addrp = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
	if (ioctl(fd, SIOCGIFNETMASK, (char *) &ifr) < 0) {
		perror("SIOCGIFNETMASK");
		unlink(_PATH_RARPDPID);
		exit(1);
	}
	*netmaskp = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
	/* If SIOCGIFNETMASK didn't work, figure out a mask from the IP
	 * address class. */
	if (*netmaskp == 0)
		*netmaskp = ipaddrtonetmask(*addrp);

	(void) close(fd);
}

int arptab_set __P((u_char *eaddr, u_int32_t host));

/*
 * Poke the kernel arp tables with the ethernet/ip address combinataion
 * given.  When processing a reply, we must do this so that the booting
 * host (i.e. the guy running rarpd), won't try to ARP for the hardware
 * address of the guy being booted (he cannot answer the ARP).
 */
void
update_arptab(ep, ipaddr)
	u_char *ep;
	u_int32_t  ipaddr;
{
#ifdef SIOCSARP
	int     s;
	struct arpreq request;
	struct sockaddr_in *sin;

	u_char *eaddr;
	u_int32_t host;

	request.arp_flags = 0;
	sin = (struct sockaddr_in *) & request.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	request.arp_ha.sa_family = AF_UNSPEC;
	/* This is needed #if defined(COMPAT_43) && BYTE_ORDER != BIG_ENDIAN,
	   because AF_UNSPEC is zero and the kernel assumes that a zero
	   sa_family means that the real sa_family value is in sa_len.  */
	request.arp_ha.sa_len = 16; /* XXX */
	memcpy((char *) request.arp_ha.sa_data, (char *) ep, 6);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		err(NONFATAL, "socket: %s", strerror(errno));
	} else {
		if (ioctl(s, SIOCSARP, (caddr_t) & request) < 0) {
		    err(NONFATAL, "SIOCSARP: %s", strerror(errno));
		}
		(void) close(s);
	}
#else
	if (arptab_set(ep, ipaddr) > 0)
		syslog(LOG_ERR, "couldn't update arp table");
#endif
}
/*
 * Build a reverse ARP packet and sent it out on the interface.
 * 'ep' points to a valid ARPOP_REVREQUEST.  The ARPOP_REVREPLY is built
 * on top of the request, then written to the network.
 *
 * RFC 903 defines the ether_arp fields as follows.  The following comments
 * are taken (more or less) straight from this document.
 *
 * ARPOP_REVREQUEST
 *
 * arp_sha is the hardware address of the sender of the packet.
 * arp_spa is undefined.
 * arp_tha is the 'target' hardware address.
 *   In the case where the sender wishes to determine his own
 *   protocol address, this, like arp_sha, will be the hardware
 *   address of the sender.
 * arp_tpa is undefined.
 *
 * ARPOP_REVREPLY
 *
 * arp_sha is the hardware address of the responder (the sender of the
 *   reply packet).
 * arp_spa is the protocol address of the responder (see the note below).
 * arp_tha is the hardware address of the target, and should be the same as
 *   that which was given in the request.
 * arp_tpa is the protocol address of the target, that is, the desired address.
 *
 * Note that the requirement that arp_spa be filled in with the responder's
 * protocol is purely for convenience.  For instance, if a system were to use
 * both ARP and RARP, then the inclusion of the valid protocol-hardware
 * address pair (arp_spa, arp_sha) may eliminate the need for a subsequent
 * ARP request.
 */
void
rarp_reply(ii, ep, ipaddr, hp)
	struct if_info *ii;
	struct ether_header *ep;
	u_int32_t  ipaddr;
	struct hostent *hp;
{
	int     n;
	struct ether_arp *ap = (struct ether_arp *) (ep + 1);
	int     len;

	update_arptab((u_char *) & ap->arp_sha, ipaddr);

	/* Build the rarp reply by modifying the rarp request in place. */
	ep->ether_type = htons(ETHERTYPE_REVARP);
	ap->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
	ap->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
	ap->arp_op = htons(ARPOP_REVREPLY);

	memcpy((char *) &ep->ether_dhost, (char *) &ap->arp_sha, 6);
	memcpy((char *) &ep->ether_shost, (char *) ii->ii_eaddr, 6);
	memcpy((char *) &ap->arp_sha, (char *) ii->ii_eaddr, 6);

	memcpy((char *) ap->arp_tpa, (char *) &ipaddr, 4);
	/* Target hardware is unchanged. */
	memcpy((char *) ap->arp_spa, (char *) &ii->ii_ipaddr, 4);

	if (lflag) {
		struct ether_addr ea;		

		memcpy(&ea.ether_addr_octet, &ap->arp_sha, 6);
		syslog(LOG_INFO, "%s asked; %s replied", hp->h_name, 
		    ether_ntoa(&ea));
	}

	len = sizeof(*ep) + sizeof(*ap);
	n = write(ii->ii_fd, (char *) ep, len);
	if (n != len) {
		err(NONFATAL, "write: only %d of %d bytes written", n, len);
	}
}
/*
 * Get the netmask of an IP address.  This routine is used if
 * SIOCGIFNETMASK doesn't work.
 */
u_int32_t
ipaddrtonetmask(addr)
	u_int32_t  addr;
{
	if (IN_CLASSA(addr))
		return IN_CLASSA_NET;
	if (IN_CLASSB(addr))
		return IN_CLASSB_NET;
	if (IN_CLASSC(addr))
		return IN_CLASSC_NET;
	err(FATAL, "unknown IP address class: %08X", addr);
	/* NOTREACHED */
}

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
err(int fatal, const char *fmt,...)
#else
err(fmt, va_alist)
	int     fatal;
	char   *fmt;
va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (dflag) {
		if (fatal)
			(void) fprintf(stderr, "rarpd: error: ");
		else
			(void) fprintf(stderr, "rarpd: warning: ");
		(void) vfprintf(stderr, fmt, ap);
		(void) fprintf(stderr, "\n");
	}
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	if (fatal) {
		unlink(_PATH_RARPDPID);
		exit(1);
	}
	/* NOTREACHED */
}

void
#ifdef __STDC__
debug(const char *fmt,...)
#else
debug(fmt, va_alist)
	char   *fmt;
va_dcl
#endif
{
	va_list ap;

	if (dflag) {
#ifdef __STDC__
		va_start(ap, fmt);
#else
		va_start(ap);
#endif
		(void) fprintf(stderr, "rarpd: ");
		(void) vfprintf(stderr, fmt, ap);
		va_end(ap);
		(void) fprintf(stderr, "\n");
	}
}
