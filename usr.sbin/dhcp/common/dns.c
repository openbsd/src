/* dns.c

   Domain Name Service subroutines. */

/*
 * Copyright (C) 1992 by Ted Lemon.
 * Copyright (c) 1997 The Internet Software Consortium.
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
 * This file is based on software written in 1992 by Ted Lemon for
 * a portable network boot loader.   That original code base has been
 * substantially modified for use in the Internet Software Consortium
 * DHCP suite.
 *
 * These later modifications were done on behalf of the Internet
 * Software Consortium by Ted Lemon <mellon@fugue.com> in cooperation
 * with Vixie Enterprises.  To learn more about the Internet Software
 * Consortium, see ``http://www.vix.com/isc''.  To learn more about
 * Vixie Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: dns.c,v 1.1 1998/08/18 03:43:25 deraadt Exp $ Copyright (c) 1997 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include "arpa/nameser.h"

int dns_protocol_initialized;
int dns_protocol_fd;

static int addlabel PROTO ((u_int8_t *, char *));
static int skipname PROTO ((u_int8_t *));
static int copy_out_name PROTO ((u_int8_t *, u_int8_t *, char *));
static int nslookup PROTO ((u_int8_t, char *, int, u_int16_t, u_int16_t));
static int zonelookup PROTO ((u_int8_t, char *, int, u_int16_t));
u_int16_t dns_port;

/* Initialize the DNS protocol. */

void dns_startup ()
{
	struct servent *srv;
	struct sockaddr_in from;

	/* Only initialize icmp once. */
	if (dns_protocol_initialized)
		error ("attempted to reinitialize dns protocol");
	dns_protocol_initialized = 1;

	/* Get the protocol number (should be 1). */
	srv = getservbyname ("domain", "tcp");
	if (srv)
		dns_port = srv -> s_port;
	else
		dns_port = htons (53);

	/* Get a socket for the DNS protocol. */
	dns_protocol_fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!dns_protocol_fd)
		error ("unable to create dns socket: %m");

	pick_name_server ();

	add_protocol ("dns", dns_protocol_fd, dns_packet, 0);
}

/* Label manipulation stuff; see RFC1035, page 28 section 4.1.2 and
   page 30, section 4.1.4. */

/* addlabel copies a label into the specified buffer, putting the length of
   the label in the first character, the contents of the label in subsequent
   characters, and returning the length of the conglomeration. */

static int addlabel (buf, label)
	u_int8_t *buf;
	char *label;
{
	*buf = strlen (label);
	memcpy (buf + 1, label, *buf);
	return *buf + 1;
}

/* skipname skips over all of the labels in a single domain name,
   returning the length of the domain name. */

static int skipname (label)
     u_int8_t *label;
{
	if (*label & INDIR_MASK)
		return 2;
	if (*label == 0)
		return 1;
	return *label + 1 + skipname (label + *label + 1);
}

/* copy_out_name copies out the name appearing at the specified location
   into a string, stored as fields seperated by dots rather than lengths
   and labels.   The length of the label-formatted name is returned. */

static int copy_out_name (base, name, buf)
     u_int8_t *base;
     u_int8_t *name;
     char *buf;
{
	if (*name & INDIR_MASK) {
		int offset = (*name & ~INDIR_MASK) + (*name + 1);
		return copy_out_name (base, base + offset, buf);
	}
	if (!*name) {
		*buf = 0;
		return 1;
	}
	memcpy (buf, name + 1, *name);
	*(buf + *name) = '.';
	return (*name + 1
		+ copy_out_name (base, name + *name + 1, buf + *name + 1));
}

/* ns_inaddr_lookup constructs a PTR lookup query for an internet address -
   e.g., 1.200.9.192.in-addr.arpa.   If the specified timeout period passes
   before the query is satisfied, or if the query fails, the callback is
   called with a null pointer.   Otherwise, the callback is called with the
   address of the string returned by the name server. */

int ns_inaddr_lookup (id, inaddr)
	u_int16_t id;
	struct iaddr inaddr;
{
	unsigned char namebuf [512];
	unsigned char *s = namebuf;
	unsigned char *label;
	int i;
	unsigned char c;

	for (i = 3; i >= 0; --i) {
		label = s++;
		*label = 1;
		c = inaddr.iabuf [i];
		if (c > 100) {
			++*label;
			*s++ = '0' + c / 100;
		}
		if (c > 10) {
			++*label;
			*s++ = '0' + ((c / 10) % 10);
		}
		*s++ = '0' + (c % 10);
	}
	s += addlabel (s, "in-addr");
	s += addlabel (s, "arpa");
	*s++ = 0;
/*	return nslookup (id, namebuf, s - namebuf, T_PTR, C_IN); */
	return zonelookup (id, namebuf, s - namebuf, C_IN);
}

/* Construct and transmit a name server query. */

static int nslookup (id, qname, namelen, qtype, qclass)
	u_int8_t id;
	char *qname;
	int namelen;
	u_int16_t qtype;
	u_int16_t qclass;
{
	HEADER *hdr;
	unsigned char query [512];
	u_int8_t *s;
	int len;
	int i, status;
	struct sockaddr_in *server = pick_name_server ();
	
	if (!server)
		return 0;

	/* Construct a header... */
	hdr = (HEADER *)query;
	memset (hdr, 0, sizeof *hdr);
	hdr -> id = htons (id);
	hdr -> rd = 1;
	hdr -> opcode = QUERY;
	hdr -> qdcount = htons (1);

	/* Copy in the name we're looking up. */
	s = (u_int8_t *)(hdr + 1);
	memcpy (s, qname, namelen);
	s += namelen;
	
	/* Set the query type. */
	putUShort (s, qtype);
	s += sizeof (u_int16_t);

	/* Set the query class. */
	putUShort (s, qclass);
	s += sizeof (u_int16_t);

	/* Send the query. */
	status = sendto (dns_protocol_fd, query, s - query, 0,
			 (struct sockaddr *)server, sizeof *server);

	/* If the send failed, report the failure. */
	if (status < 0)
		return 0;
	return 1;
}

/* Construct a query for the SOA for a specified name.
   Try every possible SOA name starting from the name specified and going
   to the root name - e.g., for

   	215.5.5.192.in-addr.arpa, look for SOAs matching:

	215.5.5.5.192.in-addr.arpa
	5.5.192.in-addr.arpa
	5.192.in-addr.arpa
	192.in-addr.arpa
	in-addr.arpa
	arpa */

static int zonelookup (id, qname, namelen, qclass)
	u_int8_t id;
	char *qname;
	int namelen;
	u_int16_t qclass;
{
	HEADER *hdr;
	unsigned char query [512];
	u_int8_t *s, *nptr;
	int len;
	int i, status, count;
	struct sockaddr_in *server = pick_name_server ();
	
	if (!server)
		return 0;

	/* Construct a header... */
	hdr = (HEADER *)query;
	memset (hdr, 0, sizeof *hdr);
	hdr -> id = htons (id);
	hdr -> rd = 1;
	hdr -> opcode = QUERY;

	/* Copy in the name we're looking up. */
	s = (u_int8_t *)(hdr + 1);
	memcpy (s, qname, namelen);
	s += namelen;
	
	/* Set the query type. */
	putUShort (s, T_SOA);
	s += sizeof (u_int16_t);

	/* Set the query class. */
	putUShort (s, qclass);
	s += sizeof (u_int16_t);
	count = 1;

	/* Now query up the hierarchy. */
	nptr = (u_int8_t *)(hdr + 1);
	while (*(nptr += *nptr + 1)) {
		/* Store a compressed reference from the full name. */
		putUShort (s, ntohs (htons (0xC000) |
				     htons (nptr - &query [0])));
		s += sizeof (u_int16_t);

		/* Store the query type. */
		putUShort (s, T_SOA);
		s += sizeof (u_int16_t);

		putUShort (s, qclass);
		s += sizeof (u_int16_t);

		/* Increment the query count... */
		++count;
break;
	}
	hdr -> qdcount = htons (count);

dump_raw (query, s - query);
	/* Send the query. */
	status = sendto (dns_protocol_fd, query, s - query, 0,
			 (struct sockaddr *)server, sizeof *server);

	/* If the send failed, report the failure. */
	if (status < 0)
		return 0;
	return 1;
}

/* Process a reply from a name server. */

void dns_packet (protocol)
	struct protocol *protocol;
{
	HEADER *ns_header;
	struct sockaddr_in from;
	int fl;
	unsigned char buf [4096];
	unsigned char nbuf [512];
	unsigned char *base;
	unsigned char *dptr;
	u_int16_t type;
	u_int16_t class;
	TIME ttl;
	u_int16_t rdlength;
	int len, status;
	int i;

	len = sizeof from;
	status = recvfrom (protocol -> fd, buf, sizeof buf, 0,
			  (struct sockaddr *)&from, &len);
	if (status < 0) {
		warn ("icmp_echoreply: %m");
		return;
	}

	ns_header = (HEADER *)buf;
	base = (unsigned char *)(ns_header + 1);

#if 0
	/* Ignore invalid packets... */
	if (ntohs (ns_header -> id) > ns_query_max) {
		printf ("Out-of-range NS message; id = %d\n",
			ntohs (ns_header -> id));
		return;
	}
#endif

	/* Parse the response... */
	dptr = base;

	/* Skip over the queries... */
	for (i = 0; i < ntohs (ns_header -> qdcount); i++) {
		dptr += skipname (dptr);
		/* Skip over the query type and query class. */
		dptr += 2 * sizeof (u_int16_t);
	}

	/* Process the answers... */
	for (i = 0; i < ntohs (ns_header -> ancount); i++) {
		/* Skip over the name we looked up. */
		dptr += skipname (dptr);

		/* Get the type. */
		type = getUShort (dptr);
		dptr += sizeof type;

		/* Get the class. */
		class = getUShort (dptr);
		dptr += sizeof class;

		/* Get the time-to-live. */
		ttl = getULong (dptr);
		dptr += sizeof ttl;

		/* Get the length of the reply. */
		rdlength = getUShort (dptr);
		dptr += sizeof rdlength;

		switch (type) {
		      case T_A:
			note ("A record; value is %d.%d.%d.%d",
			      dptr [0], dptr [1], dptr [2], dptr [3]);
			break;

		      case T_CNAME:
		      case T_PTR:
			copy_out_name (base, dptr, nbuf);
			note ("Domain name; value is %s\n", nbuf);
			return;

		      default:
			note ("unhandled type: %x", type);
		}
	}
}
