/* $OpenBSD: rebound-ns.c,v 1.2 2018/05/22 06:58:57 anton Exp $ */
/*
 * Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
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

#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct dnsheader {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
};

union dnsmsg {
	struct dnsheader *hdr;
	uint8_t *u8;
	uint16_t *u16;
	uint32_t *u32;
};

struct dnsrecord {
	unsigned char req[256];
	size_t reqlen;
	unsigned char resp[256];
	size_t resplen;
};

static const struct dnsrecord *lookup(struct dnsrecord *, size_t, union dnsmsg,
    size_t);
static const unsigned char *lowercase(union dnsmsg, size_t *);
static void newdnsrecord(struct dnsrecord *, const char *, const char *,
    const char *);
static __dead void usage(void);

int
main(int argc, char *argv[])
{
#define NRECORDS	32
	static struct dnsrecord records[NRECORDS];
	unsigned char recvbuf[65536];
	union {
		struct sockaddr a;
		struct sockaddr_in i;
	} bindaddr, fromaddr;
	const char *bindname = "127.0.0.1";
	const struct dnsrecord *resp;
	union dnsmsg req;
	socklen_t fromlen;
	ssize_t n;
	size_t nrecords = 0;
	int c, fd;

	if (pledge("stdio inet dns proc", NULL) == -1)
		err(1, "pledge");

	while ((c = getopt(argc, argv, "l:")) != -1)
		switch (c) {
		case 'l':
			bindname = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 0 || argc % 3 > 0)
		usage();

	for (; argc > 0; argc -= 3, argv += 3) {
		if (nrecords == NRECORDS)
			errx(1, "too many arguments");
		newdnsrecord(records + nrecords, argv[0], argv[1], argv[2]);
		nrecords++;
	}

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.i.sin_len = sizeof(bindaddr.i);
	bindaddr.i.sin_family = AF_INET;
	bindaddr.i.sin_port = htons(53);
	inet_aton(bindname, &bindaddr.i.sin_addr);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		err(1, "socket");
	if (bind(fd, &bindaddr.a, bindaddr.a.sa_len) == -1)
		err(1, "bind");

	if (daemon(1, 1) == -1)
		err(1, "daemon");

	if (pledge("stdio inet", NULL) == -1)
		err(1, "pledge");

	req.u8 = recvbuf;
	for (;;) {
		fromlen = sizeof(fromaddr.a);
		n = recvfrom(fd, req.u8, sizeof(recvbuf), 0, &fromaddr.a,
		    &fromlen);
		if (n == -1) {
			warn("recvfrom");
			break;
		}
		resp = lookup(records, nrecords, req, n);
		sendto(fd, resp->resp, resp->resplen, 0, &fromaddr.a, fromlen);
	}
	close(fd);

	return 0;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: ns [-l addr] type name rddata ...\n");
	exit(1);
}

static const struct dnsrecord *
lookup(struct dnsrecord *records, size_t nrecords, union dnsmsg req,
    size_t reqlen)
{
	static struct dnsrecord resp;
	union dnsmsg dns;
	const unsigned char *qname;
	size_t i, qnamelen;
	uint16_t id;

	qname = lowercase(req, &qnamelen);
	id = req.hdr->id;
	req.hdr->id = 0;

	for (i = 0; i < nrecords; i++) {
		if (records[i].reqlen != reqlen)
			continue;
		dns.u8 = records[i].req;
		if (memcmp(dns.hdr, req.hdr, reqlen) == 0) {
			memcpy(resp.resp, records[i].resp, records[i].resplen);
			resp.resplen = records[i].resplen;
			dns.u8 = resp.resp;
			dns.hdr->id = id;
			memcpy(dns.hdr + 1, qname, qnamelen);
			return &resp;
		}
	}

	resp.resplen = sizeof(struct dnsheader);
	memset(resp.resp, 0, sizeof(resp.resp));
	dns.u8 = resp.resp;
	dns.hdr->id = id;
	dns.hdr->flags = htons(0x8000 | 0x3); /* QR | NXDOMAIN */
	return &resp;
}

static const unsigned char *
lowercase(union dnsmsg dns, size_t *retlen)
{
	static unsigned char buf[256];
	size_t i;
	size_t len = 0;

	dns.hdr++; /* move to qname */
	for (;;) {
		if (*dns.u8 == '\0')
			break;
		i = buf[len++] = *dns.u8++; /* label length */
		for (; i > 0; i--) {
			buf[len++] = *dns.u8;
			*dns.u8 = tolower(*dns.u8);
			dns.u8++;
		}
	}
	*retlen = len;
	return buf;
}

static void
newdnsrecord(struct dnsrecord *record, const char *type, const char *qname,
    const char *rddata)
{
	union dnsmsg beg, dns;
	const char *label;
	int qtype;

	if (strcmp(type, "A") == 0)
		qtype = 1;
	else
		errx(1, "%s: unknown type", type);

	/* question header */
	dns.u8 = beg.u8 = record->req;
	dns.hdr->flags = htons(0x100); /* RD */
	dns.hdr->qdcount = htons(1);

	/* question message */
	dns.hdr++;

	/* question name */
	for (;;) {
		label = strchr(qname, '.');
		if (label == NULL)
			break;
		*dns.u8++ = label - qname;
		for (; qname < label; qname++)
			*dns.u8++ = *qname;
		qname = label + 1;
	}
	*dns.u8++ = '\0';

	/* question type */
	*dns.u16++ = htons(qtype);

	/* question class */
	*dns.u16++ = htons(1);

	record->reqlen = dns.u8 - beg.u8;

	/* response header */
	memcpy(record->resp, record->req, record->reqlen);
	dns.u8 = beg.u8 = record->resp;
	dns.hdr->flags = htons(0x8000 | 0x100); /* QR | RD */
	dns.hdr->ancount = htons(1);

	/* response message */
	dns.u8 += record->reqlen;

	/* response name with compression */
	*dns.u16++ = htons(0xc000 | sizeof(struct dnsheader));

	/* response type */
	*dns.u16++ = htons(qtype);

	/* response class */
	*dns.u16++ = htons(1);

	/* response ttl */
	*dns.u32++ = htonl(3600);

	/* response rdlength */
	*dns.u16++ = htons(4);

	/* response rddata */
	for (;;) {
		for (; isdigit(*rddata); rddata++)
			*dns.u8 = *dns.u8 * 10 + (*rddata - '0');
		dns.u8++;
		if (*rddata++ == '\0')
			break;
	}

	record->resplen = dns.u8 - beg.u8;
}
