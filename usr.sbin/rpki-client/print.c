/*	$OpenBSD: print.c,v 1.3 2021/12/22 09:35:14 claudio Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "extern.h"

static const char *
pretty_key_id(char *hex)
{
	static char buf[128];	/* bigger than SHA_DIGEST_LENGTH * 3 */
	size_t i;

	for (i = 0; i < sizeof(buf) && *hex != '\0'; i++) {
		if  (i % 3 == 2 && *hex != '\0')
			buf[i] = ':';
		else
			buf[i] = *hex++;
	}
	if (i == sizeof(buf))
		memcpy(buf + sizeof(buf) - 4, "...", 4);
	return buf;
}

void
tal_print(const struct tal *p)
{
	size_t	 i;

	for (i = 0; i < p->urisz; i++)
		printf("%5zu: URI: %s\n", i + 1, p->uri[i]);
}

void
cert_print(const struct cert *p)
{
	size_t	 i;
	char	 buf1[64], buf2[64];
	int	 sockt;
	char	 tbuf[21];

	printf("Subject key identifier: %s\n", pretty_key_id(p->ski));
	if (p->aki != NULL)
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
	if (p->aia != NULL)
		printf("Authority info access: %s\n", p->aia);
	if (p->mft != NULL)
		printf("Manifest: %s\n", p->mft);
	if (p->repo != NULL)
		printf("caRepository: %s\n", p->repo);
	if (p->notify != NULL)
		printf("Notify URL: %s\n", p->notify);
	if (p->pubkey != NULL)
		printf("BGPsec P-256 ECDSA public key: %s\n", p->pubkey);
	strftime(tbuf, sizeof(tbuf), "%FT%TZ", gmtime(&p->expires));
	printf("Valid until: %s\n", tbuf);

	printf("Subordinate Resources:\n");

	for (i = 0; i < p->asz; i++)
		switch (p->as[i].type) {
		case CERT_AS_ID:
			printf("%5zu: AS: %u\n", i + 1, p->as[i].id);
			break;
		case CERT_AS_INHERIT:
			printf("%5zu: AS: inherit\n", i + 1);
			break;
		case CERT_AS_RANGE:
			printf("%5zu: AS: %u -- %u\n", i + 1,
				p->as[i].range.min, p->as[i].range.max);
			break;
		}

	for (i = 0; i < p->ipsz; i++)
		switch (p->ips[i].type) {
		case CERT_IP_INHERIT:
			printf("%5zu: IP: inherit\n", i + 1);
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&p->ips[i].ip,
				p->ips[i].afi, buf1, sizeof(buf1));
			printf("%5zu: IP: %s\n", i + 1, buf1);
			break;
		case CERT_IP_RANGE:
			sockt = (p->ips[i].afi == AFI_IPV4) ?
				AF_INET : AF_INET6;
			inet_ntop(sockt, p->ips[i].min, buf1, sizeof(buf1));
			inet_ntop(sockt, p->ips[i].max, buf2, sizeof(buf2));
			printf("%5zu: IP: %s -- %s\n", i + 1, buf1, buf2);
			break;
		}

}

void
mft_print(const struct mft *p)
{
	size_t i;
	char *hash;

	printf("Subject key identifier: %s\n", pretty_key_id(p->ski));
	printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
	printf("Authority info access: %s\n", p->aia);
	printf("Manifest Number: %s\n", p->seqnum);
	for (i = 0; i < p->filesz; i++) {
		if (base64_encode(p->files[i].hash, sizeof(p->files[i].hash),
		    &hash) == -1)
			errx(1, "base64_encode failure");
		printf("%5zu: %s\n", i + 1, p->files[i].file);
		printf("\thash %s\n", hash);
		free(hash);
	}
}

void
roa_print(const struct roa *p)
{
	char	 buf[128];
	size_t	 i;
	char	 tbuf[21];

	printf("Subject key identifier: %s\n", pretty_key_id(p->ski));
	printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
	printf("Authority info access: %s\n", p->aia);
	strftime(tbuf, sizeof(tbuf), "%FT%TZ", gmtime(&p->expires));
	printf("ROA valid until: %s\n", tbuf);
	
	printf("asID: %u\n", p->asid);
	for (i = 0; i < p->ipsz; i++) {
		ip_addr_print(&p->ips[i].addr,
			p->ips[i].afi, buf, sizeof(buf));
		printf("%5zu: %s maxlen: %hhu\n", i + 1,
			buf, p->ips[i].maxlength);
	}
}

void
gbr_print(const struct gbr *p)
{
	printf("Subject key identifier: %s\n", pretty_key_id(p->ski));
	printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
	printf("Authority info access: %s\n", p->aia);
	printf("vcard:\n%s", p->vcard);
}
