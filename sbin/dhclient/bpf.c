/*	$OpenBSD: bpf.c,v 1.74 2019/01/05 21:40:44 krw Exp $	*/

/* BPF socket interface code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1995, 1996, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/bpf.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"

int
get_bpf_sock(char *name)
{
	struct ifreq	 ifr;
	int		sock;

	if ((sock = open("/dev/bpf", O_RDWR | O_CLOEXEC)) == -1)
		fatal("open(/dev/bpf)");

	/* Set the BPF device to point at this interface. */
	strlcpy(ifr.ifr_name, name, IFNAMSIZ);
	if (ioctl(sock, BIOCSETIF, &ifr) == -1)
		fatal("BIOCSETIF");

	return sock;
}

int
get_udp_sock(int rdomain)
{
	int	 sock, on = 1;

	/*
	 * Use raw socket for unicast send.
	 */
	if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) == -1)
		fatal("socket(AF_INET, SOCK_RAW)");
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on,
	    sizeof(on)) == -1)
		fatal("setsockopt(IP_HDRINCL)");
	if (setsockopt(sock, IPPROTO_IP, SO_RTABLE, &rdomain,
	    sizeof(rdomain)) == -1)
		fatal("setsockopt(SO_RTABLE)");

	return sock;
}

/*
 * Packet filter program.
 *
 * N.B.: Changes to the filter program may require changes to the
 * constant offsets used in if_register_receive to patch the BPF program!
 */
struct bpf_insn dhcp_bpf_filter[] = {
	/* Make sure this is an IP packet. */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),

	/* Make sure it's a UDP packet. */
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),

	/* Make sure this isn't a fragment. */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),

	/* Get the IP header length. */
	BPF_STMT(BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's to the right port. */
	BPF_STMT(BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 67, 0, 1),		/* patch */

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (unsigned int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

int dhcp_bpf_filter_len = sizeof(dhcp_bpf_filter) / sizeof(struct bpf_insn);

/*
 * Packet write filter program:
 * 'ip and udp and src port bootps and dst port (bootps or bootpc)'
 */
struct bpf_insn dhcp_bpf_wfilter[] = {
	BPF_STMT(BPF_LD + BPF_B + BPF_IND, 14),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, (IPVERSION << 4) + 5, 0, 12),

	/* Make sure this is an IP packet. */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 10),

	/* Make sure it's a UDP packet. */
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 8),

	/* Make sure this isn't a fragment. */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 6, 0),	/* patched */

	/* Get the IP header length. */
	BPF_STMT(BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's from the right port. */
	BPF_STMT(BPF_LD + BPF_H + BPF_IND, 14),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 68, 0, 3),

	/* Make sure it is to the right ports. */
	BPF_STMT(BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 67, 0, 1),

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (unsigned int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

int dhcp_bpf_wfilter_len = sizeof(dhcp_bpf_wfilter) / sizeof(struct bpf_insn);

int
configure_bpf_sock(int bpffd)
{
	struct bpf_version	 v;
	struct bpf_program	 p;
	int			 flag = 1, sz;

	/* Make sure the BPF version is in range. */
	if (ioctl(bpffd, BIOCVERSION, &v) == -1)
		fatal("BIOCVERSION");

	if (v.bv_major != BPF_MAJOR_VERSION ||
	    v.bv_minor < BPF_MINOR_VERSION)
		fatalx("kernel BPF version out of range - recompile "
		    "dhclient");

	/*
	 * Set immediate mode so that reads return as soon as a packet
	 * comes in, rather than waiting for the input buffer to fill
	 * with packets.
	 */
	if (ioctl(bpffd, BIOCIMMEDIATE, &flag) == -1)
		fatal("BIOCIMMEDIATE");

	if (ioctl(bpffd, BIOCSFILDROP, &flag) == -1)
		fatal("BIOCSFILDROP");

	/* Get the required BPF buffer length from the kernel. */
	if (ioctl(bpffd, BIOCGBLEN, &sz) == -1)
		fatal("BIOCGBLEN");

	/* Set up the bpf filter program structure. */
	p.bf_len = dhcp_bpf_filter_len;
	p.bf_insns = dhcp_bpf_filter;

	/* Patch the server port into the BPF program.
	 *
	 * N.B.: changes to filter program may require changes to the
	 * insn number(s) used below!
	 */
	dhcp_bpf_filter[8].k = LOCAL_PORT;

	if (ioctl(bpffd, BIOCSETF, &p) == -1)
		fatal("BIOCSETF");

	/* Set up the bpf write filter program structure. */
	p.bf_len = dhcp_bpf_wfilter_len;
	p.bf_insns = dhcp_bpf_wfilter;

	if (dhcp_bpf_wfilter[7].k == 0x1fff)
		dhcp_bpf_wfilter[7].k = htons(IP_MF|IP_OFFMASK);

	if (ioctl(bpffd, BIOCSETWF, &p) == -1)
		fatal("BIOCSETWF");

	if (ioctl(bpffd, BIOCLOCK, NULL) == -1)
		fatal("BIOCLOCK");

	return sz;
}

ssize_t
send_packet(struct interface_info *ifi, struct in_addr from, struct in_addr to,
    const char *desc)
{
	struct iovec		 iov[4];
	struct sockaddr_in	 dest;
	struct ether_header	 eh;
	struct ip		 ip;
	struct udphdr		 udp;
	struct msghdr		 msg;
	struct dhcp_packet	*packet = &ifi->sent_packet;
	ssize_t			 result, total;
	unsigned int		 iovcnt = 0, i;
	int			 len = ifi->sent_packet_length;

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(REMOTE_PORT);
	dest.sin_addr.s_addr = to.s_addr;

	if (to.s_addr == INADDR_BROADCAST) {
		assemble_eh_header(ifi->hw_address, &eh);
		iov[0].iov_base = &eh;
		iov[0].iov_len = sizeof(eh);
		iovcnt++;
	}

	ip.ip_v = 4;
	ip.ip_hl = 5;
	ip.ip_tos = IPTOS_LOWDELAY;
	ip.ip_len = htons(sizeof(ip) + sizeof(udp) + len);
	ip.ip_id = 0;
	ip.ip_off = 0;
	ip.ip_ttl = 128;
	ip.ip_p = IPPROTO_UDP;
	ip.ip_sum = 0;
	ip.ip_src.s_addr = from.s_addr;
	ip.ip_dst.s_addr = to.s_addr;
	ip.ip_sum = wrapsum(checksum((unsigned char *)&ip, sizeof(ip), 0));
	iov[iovcnt].iov_base = &ip;
	iov[iovcnt].iov_len = sizeof(ip);
	iovcnt++;

	udp.uh_sport = htons(LOCAL_PORT);
	udp.uh_dport = htons(REMOTE_PORT);
	udp.uh_ulen = htons(sizeof(udp) + len);
	udp.uh_sum = 0;
	udp.uh_sum = wrapsum(checksum((unsigned char *)&udp, sizeof(udp),
	    checksum((unsigned char *)packet, len,
	    checksum((unsigned char *)&ip.ip_src,
	    2 * sizeof(ip.ip_src),
	    IPPROTO_UDP + (uint32_t)ntohs(udp.uh_ulen)))));
	iov[iovcnt].iov_base = &udp;
	iov[iovcnt].iov_len = sizeof(udp);
	iovcnt++;

	iov[iovcnt].iov_base = packet;
	iov[iovcnt].iov_len = len;
	iovcnt++;

	total = 0;
	for (i = 0; i < iovcnt; i++)
		total += iov[i].iov_len;

	if (to.s_addr == INADDR_BROADCAST) {
		result = writev(ifi->bpffd, iov, iovcnt);
		if (result == -1)
			log_warn("%s: writev(%s)", log_procname, desc);
		else if (result < total) {
			log_warnx("%s, writev(%s): %zd of %zd bytes",
			    log_procname, desc, result, total);
			result = -1;
		}
	} else {
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = (struct sockaddr *)&dest;
		msg.msg_namelen = sizeof(dest);
		msg.msg_iov = iov;
		msg.msg_iovlen = iovcnt;
		result = sendmsg(ifi->udpfd, &msg, 0);
		if (result == -1)
			log_warn("%s: sendmsg(%s)", log_procname, desc);
		else if (result < total) {
			result = -1;
			log_warnx("%s, sendmsg(%s): %zd of %zd bytes",
			    log_procname, desc, result, total);
		}
	}

	return result;
}

/*
 * Extract a DHCP packet from a bpf capture buffer.
 *
 * Each captured packet is
 *
 *	<BPF header>
 *	<padding to BPF_WORDALIGN>
 *	<captured DHCP packet>
 *	<padding to BPF_WORDALIGN>
 *
 * Return the number of bytes processed or 0 if there is
 * no valid DHCP packet in the buffer.
 */
ssize_t
receive_packet(unsigned char *buf, unsigned char *lim,
    struct sockaddr_in *from, struct ether_addr *hfrom,
    struct dhcp_packet *packet)
{
	struct bpf_hdr		 bh;
	struct ether_header	 eh;
	unsigned char		*pktlim, *data, *next;
	size_t			 datalen;
	int			 len;

	for (next = buf; next < lim; next = pktlim) {
		/* No bpf header means no more packets. */
		if (lim < next + sizeof(bh))
			return 0;

		memcpy(&bh, next, sizeof(bh));
		pktlim = next + BPF_WORDALIGN(bh.bh_hdrlen + bh.bh_caplen);

		/* Truncated bpf packet means no more packets. */
		if (lim < next + bh.bh_hdrlen + bh.bh_caplen)
			return 0;

		/* Drop incompletely captured DHCP packets. */
		if (bh.bh_caplen != bh.bh_datalen)
			continue;

		/*
		 * Drop packets with invalid ethernet/ip/udp headers.
		 */
		if (pktlim < next + bh.bh_hdrlen + sizeof(eh))
			continue;
		memcpy(&eh, next + bh.bh_hdrlen, sizeof(eh));
		memcpy(hfrom->ether_addr_octet, eh.ether_shost, ETHER_ADDR_LEN);

		len = decode_udp_ip_header(next + bh.bh_hdrlen + sizeof(eh),
		    bh.bh_caplen - sizeof(eh), from);
		if (len < 0)
			continue;

		/* Drop packets larger than sizeof(struct dhcp_packet). */
		datalen = bh.bh_caplen - (sizeof(eh) + len);
		if (datalen > sizeof(*packet))
			continue;

		/* Extract the DHCP packet for further processing. */
		data = next + bh.bh_hdrlen + sizeof(eh) + len;
		memset(packet, DHO_END, sizeof(*packet));
		memcpy(packet, data, datalen);

		return (pktlim - buf);
	}

	return 0;
}
