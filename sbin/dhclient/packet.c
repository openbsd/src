/*	$OpenBSD: packet.c,v 1.44 2018/02/28 22:16:56 krw Exp $	*/

/* Packet assembly code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1995, 1996, 1999 The Internet Software Consortium.
 * All rights reserved.
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

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"

uint32_t
checksum(unsigned char *buf, uint32_t nbytes, uint32_t sum)
{
	unsigned int	 i;

	/* Checksum all the pairs of bytes first. */
	for (i = 0; i < (nbytes & ~1U); i += 2) {
		sum += (uint16_t)ntohs(*((uint16_t *)(buf + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < nbytes) {
		sum += buf[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	return sum;
}

uint32_t
wrapsum(uint32_t sum)
{
	sum = ~sum & 0xFFFF;
	return htons(sum);
}

void
assemble_eh_header(struct ether_addr shost, struct ether_header *eh)
{
	memset(eh->ether_dhost, 0xff, sizeof(eh->ether_dhost));

	memcpy(eh->ether_shost, shost.ether_addr_octet,
	    sizeof(eh->ether_shost));

	eh->ether_type = htons(ETHERTYPE_IP);
}

ssize_t
decode_hw_header(unsigned char *buf, uint32_t buflen, struct ether_addr *from)
{
	struct ether_header	 eh;

	if (buflen < sizeof(eh))
		return -1;

	memcpy(&eh, buf, sizeof(eh));

	memcpy(from->ether_addr_octet, eh.ether_shost, ETHER_ADDR_LEN);

	return sizeof(eh);
}

ssize_t
decode_udp_ip_header(unsigned char *buf, uint32_t buflen,
    struct sockaddr_in *from)
{
	static int	 ip_packets_seen;
	static int	 ip_packets_bad_checksum;
	static int	 udp_packets_seen;
	static int	 udp_packets_bad_checksum;
	static int	 udp_packets_length_checked;
	static int	 udp_packets_length_overflow;
	struct ip	*ip;
	struct udphdr	*udp;
	unsigned char	*data;
	int		 len;
	uint32_t	 ip_len;
	uint32_t	 sum, usum;

	/* Assure that an entire IP header is within the buffer. */
	if (sizeof(*ip) > buflen)
		return -1;
	ip_len = (*buf & 0xf) << 2;
	if (ip_len > buflen)
		return -1;
	ip = (struct ip *)(buf);
	ip_packets_seen++;

	/* Check the IP header checksum - it should be zero. */
	if (wrapsum(checksum((unsigned char *)ip, ip_len, 0)) != 0) {
		ip_packets_bad_checksum++;
		if (ip_packets_seen > 4 && ip_packets_bad_checksum != 0 &&
		    (ip_packets_seen / ip_packets_bad_checksum) < 2) {
			log_debug("%s: %d bad IP checksums seen in %d packets",
			    log_procname, ip_packets_bad_checksum,
			    ip_packets_seen);
			ip_packets_seen = ip_packets_bad_checksum = 0;
		}
		return -1;
	}

	memcpy(&from->sin_addr, &ip->ip_src, sizeof(from->sin_addr));

	if (ntohs(ip->ip_len) != buflen)
		log_debug("%s: ip length %hu disagrees with bytes received %d",
		    log_procname, ntohs(ip->ip_len), buflen);

	/* Assure that the entire IP packet is within the buffer. */
	if (ntohs(ip->ip_len) > buflen)
		return -1;

	/* Assure that the UDP header is within the buffer. */
	if (ip_len + sizeof(*udp) > buflen)
		return -1;
	udp = (struct udphdr *)(buf + ip_len);
	udp_packets_seen++;

	/* Assure that the entire UDP packet is within the buffer. */
	if (ip_len + ntohs(udp->uh_ulen) > buflen)
		return -1;
	data = buf + ip_len + sizeof(*udp);

	/*
	 * Compute UDP checksums, including the ``pseudo-header'', the
	 * UDP header and the data. If the UDP checksum field is zero,
	 * we're not supposed to do a checksum.
	 */
	udp_packets_length_checked++;
	len = ntohs(udp->uh_ulen) - sizeof(*udp);
	if ((len < 0) || (len + data > buf + buflen)) {
		udp_packets_length_overflow++;
		if (udp_packets_length_checked > 4 &&
		    udp_packets_length_overflow != 0 &&
		    (udp_packets_length_checked /
		    udp_packets_length_overflow) < 2) {
			log_debug("%s: %d udp packets in %d too long - dropped",
			    log_procname, udp_packets_length_overflow,
			    udp_packets_length_checked);
			udp_packets_length_overflow =
			    udp_packets_length_checked = 0;
		}
		return -1;
	}
	if (len + data != buf + buflen)
		log_debug("%s: accepting packet with data after udp payload",
		    log_procname);

	usum = udp->uh_sum;
	udp->uh_sum = 0;

	sum = wrapsum(checksum((unsigned char *)udp, sizeof(*udp),
	    checksum(data, len, checksum((unsigned char *)&ip->ip_src,
	    2 * sizeof(ip->ip_src),
	    IPPROTO_UDP + (uint32_t)ntohs(udp->uh_ulen)))));

	udp_packets_seen++;
	if (usum != 0 && usum != sum) {
		udp_packets_bad_checksum++;
		if (udp_packets_seen > 4 && udp_packets_bad_checksum != 0 &&
		    (udp_packets_seen / udp_packets_bad_checksum) < 2) {
			log_debug("%s: %d bad udp checksums in %d packets",
			    log_procname, udp_packets_bad_checksum,
			    udp_packets_seen);
			udp_packets_seen = udp_packets_bad_checksum = 0;
		}
		return -1;
	}

	memcpy(&from->sin_port, &udp->uh_sport, sizeof(udp->uh_sport));

	return ip_len + sizeof(*udp);
}
