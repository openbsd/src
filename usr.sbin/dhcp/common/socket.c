/* socket.c

   BSD socket interface code... */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
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

#include "dhcpd.h"

/* Generic interface registration routine... */
int if_register_socket (info)
	struct interface_info *info;
{
	struct sockaddr_in name;
	int sock;
	int flag;

	/* Set up the address we're going to bind to. */
	memset(&name, 0, sizeof name);
	name.sin_family = AF_INET;
	name.sin_port = local_port;
	name.sin_addr.s_addr = INADDR_ANY;

	/* Make a socket... */
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		error ("Can't create dhcp socket: %m");

	/* Set the REUSEADDR option so that we don't fail to start if
	   we're being restarted. */
	flag = 1;
	if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)&flag, sizeof flag) < 0)
		error ("Can't set SO_REUSEADDR option on dhcp socket: %m");

	flag = 1;
	if (setsockopt (sock, SOL_SOCKET, SO_REUSEPORT,
			(char *)&flag, sizeof flag) < 0)
		error ("Can't set SO_REUSEPORT option on dhcp socket: %m");

	/* Set the BROADCAST option so that we can broadcast DHCP responses. */
	if (setsockopt (sock, SOL_SOCKET, SO_BROADCAST,
			(char *)&flag, sizeof flag) < 0)
		error ("Can't set SO_BROADCAST option on dhcp socket: %m");

	/* Bind the socket to this interface's IP address. */
	if (bind (sock, (struct sockaddr *)&name, sizeof name) < 0)
		error ("Can't bind to dhcp address: %m");

	flag = IPSEC_LEVEL_BYPASS;
	if (setsockopt (sock, IPPROTO_IP, IP_AUTH_LEVEL,
			(char *)&flag, sizeof flag) == -1)
		if (errno != EOPNOTSUPP)
			error ("Can't bypass auth IPsec on dhcp socket: %m");
	if (setsockopt (sock, IPPROTO_IP, IP_ESP_TRANS_LEVEL,
			(char *)&flag, sizeof flag) == -1)
		if (errno != EOPNOTSUPP)
			error ("Can't bypass ESP transport on dhcp socket: %m");
	if (setsockopt (sock, IPPROTO_IP, IP_ESP_NETWORK_LEVEL,
			(char *)&flag, sizeof flag) == -1)
		if (errno != EOPNOTSUPP)
			error ("Can't bypass ESP network on dhcp socket: %m");

	return sock;
}

void if_register_fallback (info)
	struct interface_info *info;
{

	info -> wfdesc = if_register_socket (info);

	if (!quiet_interface_discovery)
		note ("Sending on   Socket/%s%s%s",
		      info -> name,
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

ssize_t send_fallback (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	int result;

	result = sendto (interface -> wfdesc, (char *)raw, len, 0,
	  (struct sockaddr *)to, sizeof *to);

	if (result == -1) {
		warn ("send_fallback: %m");
		if (errno == ENETUNREACH)
			warn ("send_fallback: please consult README file %s",
			  "regarding broadcast address.");
	}
	return result;
}

/* This just reads in a packet and silently discards it. */

void fallback_discard (protocol)
	struct protocol *protocol;
{
	char buf [1540];
	struct sockaddr_in from;
	socklen_t flen = sizeof from;
	int status;
	struct interface_info *interface = protocol -> local;

	status = recvfrom (interface -> wfdesc, buf, sizeof buf, 0,
			   (struct sockaddr *)&from, &flen);
	if (status == 0)
		warn ("fallback_discard: %m");
}
