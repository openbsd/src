/*	$OpenBSD: pf.c,v 1.4 2000/02/20 17:45:33 bitblt Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is partly derived from rarpd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: pf.c,v 1.4 2000/02/20 17:45:33 bitblt Exp $";
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>

#include <net/bpf.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netdb.h>
#include <ctype.h>
#include <string.h>

#include <syslog.h>
#include <varargs.h>

#include "common/mopdef.h"

/*
 * Variables
 */

extern int errno;
extern int promisc;

/*
 * Return information to device.c how to open device.
 * In this case the driver can handle both Ethernet type II and
 * IEEE 802.3 frames (SNAP) in a single pfOpen.
 */

int
pfTrans(interface)
	char *interface;
{
	return TRANS_ETHER+TRANS_8023+TRANS_AND;
}

/*
 * Open and initialize packet filter.
 */

int
pfInit(interface, mode, protocol, typ)
	char *interface;
	u_short protocol;
	int typ, mode;
{
	int	fd;
	int	n = 0;
	char	device[sizeof "/dev/bpf000"];
	struct ifreq ifr;
	u_int	dlt;
	int	immediate;

	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x4711, 4, 0),
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 20),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x4711, 0, 3),
		BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 14),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0xaaaa, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, 1520),
		BPF_STMT(BPF_RET | BPF_K, 0),
	};
	static struct bpf_program filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};
	
  	/* Go through all the minors and find one that isn't in use. */
	do {
		(void) sprintf(device, "/dev/bpf%d", n++);
		fd = open(device, mode);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
      		syslog(LOG_ERR,"pfInit: open bpf %m");
		return(-1);
	}
  
	/* Set immediate mode so packets are processed as they arrive. */
	immediate = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &immediate) < 0) {
      		syslog(LOG_ERR,"pfInit: BIOCIMMEDIATE: %m");
		return(-1);
	}
	(void) strncpy(ifr.ifr_name, interface, sizeof ifr.ifr_name);
	if (ioctl(fd, BIOCSETIF, (caddr_t) & ifr) < 0) {
      		syslog(LOG_ERR,"pfInit: BIOCSETIF: %m");
		return(-1);
	}
	/* Check that the data link layer is an Ethernet; this code won't work
	 * with anything else. */
	if (ioctl(fd, BIOCGDLT, (caddr_t) & dlt) < 0) {
      		syslog(LOG_ERR,"pfInit: BIOCGDLT: %m");
		return(-1);
	}
	if (dlt != DLT_EN10MB) {
      		syslog(LOG_ERR,"pfInit: %s is not ethernet", device);
		return(-1);
	}
	if (promisc) {
		/* Set promiscuous mode. */
		if (ioctl(fd, BIOCPROMISC, (caddr_t)0) < 0) {
      			syslog(LOG_ERR,"pfInit: BIOCPROMISC: %m");
			return(-1);
		}
	}
	/* Set filter program. */
	insns[1].k = protocol;
	insns[3].k = protocol;

	if (ioctl(fd, BIOCSETF, (caddr_t) & filter) < 0) {
      		syslog(LOG_ERR,"pfInit: BIOCSETF: %m");
		return(-1);
	}
	return(fd);
}

/*
 * Add a Multicast address to the interface
 */

int
pfAddMulti(s, interface, addr)
	int s;
	char *interface, *addr;
{
	struct ifreq ifr;
	int	fd;
	
	strncpy(ifr.ifr_name, interface,sizeof(ifr.ifr_name) - 1);
	ifr.ifr_name[sizeof(ifr.ifr_name)] = 0;

	ifr.ifr_addr.sa_family = AF_UNSPEC;
	bcopy(addr, ifr.ifr_addr.sa_data, 6);
	
	/*
	 * open a socket, temporarily, to use for SIOC* ioctls
	 *
	 */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "pfAddMulti: socket: %m");
		return(-1);
	}
	if (ioctl(fd, SIOCADDMULTI, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "pfAddMulti: SIOCADDMULTI: %m");
		close(fd);
		return(-1);
	}
	close(fd);
	
	return(0);
}

/*
 * Delete a Multicast address from the interface
 */

int
pfDelMulti(s, interface, addr)
	int s;
	char *interface, *addr;
{
	struct ifreq ifr;
	int	fd;
	
	strncpy(ifr.ifr_name, interface, sizeof (ifr.ifr_name) - 1);
	ifr.ifr_name[sizeof(ifr.ifr_name)] = 0;
	
	ifr.ifr_addr.sa_family = AF_UNSPEC;
	bcopy(addr, ifr.ifr_addr.sa_data, 6);
	
	/*
	 * open a socket, temporarily, to use for SIOC* ioctls
	 *
	 */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "pfDelMulti: socket: %m");
		return(-1);
	}
	if (ioctl(fd, SIOCDELMULTI, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "pfAddMulti: SIOCDELMULTI: %m");
		close(fd);
		return(-1);
	}
	close(fd);
	
	return(0);
}

/*
 * read a packet
 */

int
pfRead(fd, buf, len)
	int	fd, len;
	u_char *buf;
{
	return(read(fd, buf, len));
}

/*
 * write a packet
 */

int
pfWrite(fd, buf, len, trans)
	int fd, len, trans;
	u_char *buf;
{
	
	struct iovec iov[2];
	
	switch (trans) {
	case TRANS_8023:
		iov[0].iov_base = (caddr_t)buf;
		iov[0].iov_len = 22;
		iov[1].iov_base = (caddr_t)buf+22;
		iov[1].iov_len = len-22;
		break;
	default:
		iov[0].iov_base = (caddr_t)buf;
		iov[0].iov_len = 14;
		iov[1].iov_base = (caddr_t)buf+14;
		iov[1].iov_len = len-14;
		break;
	}

	if (writev(fd, iov, 2) == len)
		return(len);
	
	return(-1);
}

