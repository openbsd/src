/*	$OpenBSD: print.c,v 1.22 2022/12/01 12:41:34 job Exp $ */
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

#include <openssl/evp.h>

#include "extern.h"

static const char *
pretty_key_id(const char *hex)
{
	static char buf[128];	/* bigger than SHA_DIGEST_LENGTH * 3 */
	size_t i;

	for (i = 0; i < sizeof(buf) && *hex != '\0'; i++) {
		if (i % 3 == 2)
			buf[i] = ':';
		else
			buf[i] = *hex++;
	}
	if (i == sizeof(buf))
		memcpy(buf + sizeof(buf) - 4, "...", 4);
	else
		buf[i] = '\0';
	return buf;
}

char *
time2str(time_t t)
{
	static char buf[64];
	struct tm tm;

	if (gmtime_r(&t, &tm) == NULL)
		return "could not convert time";

	strftime(buf, sizeof(buf), "%h %d %T %Y %Z", &tm);
	return buf;
}

void
tal_print(const struct tal *p)
{
	char			*ski;
	EVP_PKEY		*pk;
	RSA			*r;
	const unsigned char	*der;
	unsigned char		*rder = NULL;
	unsigned char		 md[SHA_DIGEST_LENGTH];
	int			 rder_len;
	size_t			 i;

	der = p->pkey;
	pk = d2i_PUBKEY(NULL, &der, p->pkeysz);
	if (pk == NULL)
		errx(1, "d2i_PUBKEY failed in %s", __func__);

	r = EVP_PKEY_get0_RSA(pk);
	if (r == NULL)
		errx(1, "EVP_PKEY_get0_RSA failed in %s", __func__);
	if ((rder_len = i2d_RSAPublicKey(r, &rder)) <= 0)
		errx(1, "i2d_RSAPublicKey failed in %s", __func__);

	if (!EVP_Digest(rder, rder_len, md, NULL, EVP_sha1(), NULL))
		errx(1, "EVP_Digest failed in %s", __func__);

	ski = hex_encode(md, SHA_DIGEST_LENGTH);

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"tal\",\n");
		printf("\t\"name\": \"%s\",\n", p->descr);
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(ski));
		printf("\t\"trust_anchor_locations\": [");
		for (i = 0; i < p->urisz; i++) {
			printf("\"%s\"", p->uri[i]);
			if (i + 1 < p->urisz)
				printf(", ");
		}
		printf("],\n");
	} else {
		printf("Trust anchor name:        %s\n", p->descr);
		printf("Subject key identifier:   %s\n", pretty_key_id(ski));
		printf("Trust anchor locations:\n");
		for (i = 0; i < p->urisz; i++)
			printf("%5zu: %s\n", i + 1, p->uri[i]);
	}

	EVP_PKEY_free(pk);
	free(rder);
	free(ski);
}

void
x509_print(const X509 *x)
{
	const ASN1_INTEGER	*xserial;
	char			*serial = NULL;

	xserial = X509_get0_serialNumber(x);
	if (xserial == NULL) {
		warnx("X509_get0_serialNumber failed in %s", __func__);
		goto out;
	}

	serial = x509_convert_seqnum(__func__, xserial);
	if (serial == NULL) {
		warnx("x509_convert_seqnum failed in %s", __func__);
		goto out;
	}

	if (outformats & FORMAT_JSON) {
		printf("\t\"cert_serial\": \"%s\",\n", serial);
	} else {
		printf("Certificate serial:       %s\n", serial);
	}

 out:
	free(serial);
}

void
cert_print(const struct cert *p)
{
	size_t			 i, j;
	char			 buf1[64], buf2[64];
	int			 sockt;
	char			 tbuf[21];

	strftime(tbuf, sizeof(tbuf), "%FT%TZ", gmtime(&p->expires));

	if (outformats & FORMAT_JSON) {
		if (p->pubkey != NULL)
			printf("\t\"type\": \"router_key\",\n");
		else
			printf("\t\"type\": \"ca_cert\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		if (p->aki != NULL)
			printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		x509_print(p->x509);
		if (p->aia != NULL)
			printf("\t\"aia\": \"%s\",\n", p->aia);
		if (p->mft != NULL)
			printf("\t\"manifest\": \"%s\",\n", p->mft);
		if (p->repo != NULL)
			printf("\t\"carepository\": \"%s\",\n", p->repo);
		if (p->notify != NULL)
			printf("\t\"notify_url\": \"%s\",\n", p->notify);
		if (p->pubkey != NULL)
			printf("\t\"router_key\": \"%s\",\n", p->pubkey);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
		printf("\t\"subordinate_resources\": [\n");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		if (p->aki != NULL)
			printf("Authority key identifier: %s\n",
			    pretty_key_id(p->aki));
		x509_print(p->x509);
		if (p->aia != NULL)
			printf("Authority info access:    %s\n", p->aia);
		if (p->mft != NULL)
			printf("Manifest:                 %s\n", p->mft);
		if (p->repo != NULL)
			printf("caRepository:             %s\n", p->repo);
		if (p->notify != NULL)
			printf("Notify URL:               %s\n", p->notify);
		if (p->pubkey != NULL) {
			printf("BGPsec ECDSA public key:  %s\n",
			    p->pubkey);
			printf("Router key valid until:   %s\n", tbuf);
		} else
			printf("Certificate valid until:  %s\n", tbuf);
		printf("Subordinate resources:\n");
	}

	for (i = 0; i < p->asz; i++) {
		switch (p->as[i].type) {
		case CERT_AS_ID:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"asid\": %u }", p->as[i].id);
			else
				printf("%5zu: AS: %u", i + 1, p->as[i].id);
			break;
		case CERT_AS_INHERIT:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"asid_inherit\": \"true\" }");
			else
				printf("%5zu: AS: inherit", i + 1);
			break;
		case CERT_AS_RANGE:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"asrange\": { \"min\": %u, "
				    "\"max\": %u }}", p->as[i].range.min,
				    p->as[i].range.max);
			else
				printf("%5zu: AS: %u -- %u", i + 1,
				    p->as[i].range.min, p->as[i].range.max);
			break;
		}
		if (outformats & FORMAT_JSON && i + 1 < p->asz + p->ipsz)
			printf(",\n");
		else
			printf("\n");
	}

	for (j = 0; j < p->ipsz; j++) {
		switch (p->ips[j].type) {
		case CERT_IP_INHERIT:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"ip_inherit\": \"true\" }");
			else
				printf("%5zu: IP: inherit", i + j + 1);
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&p->ips[j].ip,
			    p->ips[j].afi, buf1, sizeof(buf1));
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"ip_prefix\": \"%s\" }", buf1);
			else
				printf("%5zu: IP: %s", i + j + 1, buf1);
			break;
		case CERT_IP_RANGE:
			sockt = (p->ips[j].afi == AFI_IPV4) ?
			    AF_INET : AF_INET6;
			inet_ntop(sockt, p->ips[j].min, buf1, sizeof(buf1));
			inet_ntop(sockt, p->ips[j].max, buf2, sizeof(buf2));
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"ip_range\": { \"min\": \"%s\""
				    ", \"max\": \"%s\" }}", buf1, buf2);
			else
				printf("%5zu: IP: %s -- %s", i + j + 1, buf1,
				    buf2);
			break;
		}
		if (outformats & FORMAT_JSON && i + j + 1 < p->asz + p->ipsz)
			printf(",\n");
		else
			printf("\n");
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
}

void
crl_print(const struct crl *p)
{
	STACK_OF(X509_REVOKED)	*revlist;
	X509_REVOKED *rev;
	ASN1_INTEGER *crlnum;
	int i;
	char *serial;
	time_t t;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"crl\",\n");
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
	} else
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));

	crlnum = X509_CRL_get_ext_d2i(p->x509_crl, NID_crl_number, NULL, NULL);
	serial = x509_convert_seqnum(__func__, crlnum);
	if (serial != NULL) {
		if (outformats & FORMAT_JSON)
			printf("\t\"crl_serial\": \"%s\",\n", serial);
		else
			printf("CRL Serial Number:        %s\n", serial);
	}
	free(serial);
	ASN1_INTEGER_free(crlnum);

	if (outformats & FORMAT_JSON) {
		printf("\t\"valid_since\": %lld,\n", (long long)p->issued);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
		printf("\t\"revoked_certs\": [\n");
	} else {
		printf("CRL valid since:          %s\n", time2str(p->issued));
		printf("CRL valid until:          %s\n", time2str(p->expires));
		printf("Revoked Certificates:\n");
	}

	revlist = X509_CRL_get_REVOKED(p->x509_crl);
	for (i = 0; i < sk_X509_REVOKED_num(revlist); i++) {
		rev = sk_X509_REVOKED_value(revlist, i);
		serial = x509_convert_seqnum(__func__,
		    X509_REVOKED_get0_serialNumber(rev));
		x509_get_time(X509_REVOKED_get0_revocationDate(rev), &t);
		if (serial != NULL) {
			if (outformats & FORMAT_JSON) {
				printf("\t\t{ \"serial\": \"%s\"", serial);
				printf(", \"date\": \"%s\" }", time2str(t));
				if (i + 1 < sk_X509_REVOKED_num(revlist))
					printf(",");
				printf("\n");
			} else
				printf("    Serial: %8s   Revocation Date: %s"
				    "\n", serial, time2str(t));
		}
		free(serial);
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
	else if (i == 0)
		printf("No Revoked Certificates\n");
}

void
mft_print(const X509 *x, const struct mft *p)
{
	size_t i;
	char *hash;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"manifest\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"sia\": \"%s\",\n", p->sia);
		printf("\t\"manifest_number\": \"%s\",\n", p->seqnum);
		printf("\t\"valid_since\": %lld,\n", (long long)p->valid_since);
		printf("\t\"valid_until\": %lld,\n", (long long)p->valid_until);
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		x509_print(x);
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("Manifest Number:          %s\n", p->seqnum);
		printf("Manifest valid since:     %s\n", time2str(p->valid_since));
		printf("Manifest valid until:     %s\n", time2str(p->valid_until));
		printf("Files and hashes:\n");
	}

	for (i = 0; i < p->filesz; i++) {
		if (i == 0 && outformats & FORMAT_JSON)
			printf("\t\"filesandhashes\": [\n");

		if (base64_encode(p->files[i].hash, sizeof(p->files[i].hash),
		    &hash) == -1)
			errx(1, "base64_encode failure");

		if (outformats & FORMAT_JSON) {
			printf("\t\t{ \"filename\": \"%s\",", p->files[i].file);
			printf(" \"hash\": \"%s\" }", hash);
			if (i + 1 < p->filesz)
				printf(",");
			printf("\n");
		} else {
			printf("%5zu: %s\n", i + 1, p->files[i].file);
			printf("\thash %s\n", hash);
		}

		free(hash);
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
}

void
roa_print(const X509 *x, const struct roa *p)
{
	char	 buf[128];
	size_t	 i;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"roa\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"sia\": \"%s\",\n", p->sia);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("ROA valid until:          %s\n", time2str(p->expires));
		printf("asID:                     %u\n", p->asid);
		printf("IP address blocks:\n");
	}

	for (i = 0; i < p->ipsz; i++) {
		if (i == 0 && outformats & FORMAT_JSON)
			printf("\t\"vrps\": [\n");

		ip_addr_print(&p->ips[i].addr,
		    p->ips[i].afi, buf, sizeof(buf));

		if (outformats & FORMAT_JSON) {
			printf("\t\t{ \"prefix\": \"%s\",", buf);
			printf(" \"asid\": %u,", p->asid);
			printf(" \"maxlen\": %hhu }", p->ips[i].maxlength);
			if (i + 1 < p->ipsz)
				printf(",");
			printf("\n");
		} else
			printf("%5zu: %s maxlen: %hhu\n", i + 1, buf,
			    p->ips[i].maxlength);
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
}

void
gbr_print(const X509 *x, const struct gbr *p)
{
	size_t	i;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"gbr\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"sia\": \"%s\",\n", p->sia);
		printf("\t\"vcard\": \"");
		for (i = 0; i < strlen(p->vcard); i++) {
			if (p->vcard[i] == '"')
				printf("\\\"");
			if (p->vcard[i] == '\r')
				continue;
			if (p->vcard[i] == '\n')
				printf("\\r\\n");
			else
				putchar(p->vcard[i]);
		}
		printf("\",\n");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("vcard:\n%s", p->vcard);
	}
}

void
rsc_print(const X509 *x, const struct rsc *p)
{
	char	 buf1[64], buf2[64], tbuf[21];
	char	*hash;
	int	 sockt;
	size_t	 i, j;

	strftime(tbuf, sizeof(tbuf), "%FT%TZ", gmtime(&p->expires));

	if (outformats & FORMAT_JSON) {
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		x509_print(x);
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
		printf("\t\"signed_with_resources\": [\n");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		x509_print(x);
		printf("Authority info access:    %s\n", p->aia);
		printf("Valid until:              %s\n", tbuf);
		printf("Signed with resources:\n");
	}

	for (i = 0; i < p->asz; i++) {
		switch (p->as[i].type) {
		case CERT_AS_ID:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"asid\": %u }", p->as[i].id);
			else
				printf("%5zu: AS: %u", i + 1, p->as[i].id);
			break;
		case CERT_AS_RANGE:
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"asrange\": { \"min\": %u, "
				    "\"max\": %u }}", p->as[i].range.min,
				    p->as[i].range.max);
			else
				printf("%5zu: AS: %u -- %u", i + 1,
				    p->as[i].range.min, p->as[i].range.max);
			break;
		case CERT_AS_INHERIT:
			/* inheritance isn't possible in RSC */
			break;
		}
		if (outformats & FORMAT_JSON && i + 1 < p->asz + p->ipsz)
			printf(",\n");
		else
			printf("\n");
	}

	for (j = 0; j < p->ipsz; j++) {
		switch (p->ips[j].type) {
		case CERT_IP_ADDR:
			ip_addr_print(&p->ips[j].ip,
			    p->ips[j].afi, buf1, sizeof(buf1));
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"ip_prefix\": \"%s\" }", buf1);
			else
				printf("%5zu: IP: %s", i + j + 1, buf1);
			break;
		case CERT_IP_RANGE:
			sockt = (p->ips[j].afi == AFI_IPV4) ?
			    AF_INET : AF_INET6;
			inet_ntop(sockt, p->ips[j].min, buf1, sizeof(buf1));
			inet_ntop(sockt, p->ips[j].max, buf2, sizeof(buf2));
			if (outformats & FORMAT_JSON)
				printf("\t\t{ \"ip_range\": { \"min\": \"%s\""
				    ", \"max\": \"%s\" }}", buf1, buf2);
			else
				printf("%5zu: IP: %s -- %s", i + j + 1, buf1,
				    buf2);
			break;
		case CERT_IP_INHERIT:
			/* inheritance isn't possible in RSC */
			break;
		}
		if (outformats & FORMAT_JSON && i + j + 1 < p->asz + p->ipsz)
			printf(",\n");
		else
			printf("\n");
	}

	if (outformats & FORMAT_JSON) {
		printf("\t],\n");
		printf("\t\"filenamesandhashes\": [\n");
	} else
		printf("Filenames and hashes:\n");

	for (i = 0; i < p->filesz; i++) {
		if (base64_encode(p->files[i].hash, sizeof(p->files[i].hash),
		    &hash) == -1)
			errx(1, "base64_encode failure");

		if (outformats & FORMAT_JSON) {
			printf("\t\t{ \"filename\": \"%s\",",
			    p->files[i].filename ? p->files[i].filename : "");
			printf(" \"hash_digest\": \"%s\" }", hash);
			if (i + 1 < p->filesz)
				printf(",");
			printf("\n");
		} else {
			printf("%5zu: %s\n", i + 1, p->files[i].filename
			    ? p->files[i].filename : "no filename");
			printf("\thash %s\n", hash);
		}

		free(hash);
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
}

void
aspa_print(const X509 *x, const struct aspa *p)
{
	size_t	i;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"aspa\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"sia\": \"%s\",\n", p->sia);
		printf("\t\"customer_asid\": %u,\n", p->custasid);
		printf("\t\"provider_set\": [\n");
		for (i = 0; i < p->providersz; i++) {
			printf("\t\t{ \"asid\": %u", p->providers[i].as);
			if (p->providers[i].afi == AFI_IPV4)
				printf(", \"afi_limit\": \"ipv4\"");
			if (p->providers[i].afi == AFI_IPV6)
				printf(", \"afi_limit\": \"ipv6\"");
			printf(" }");
			if (i + 1 < p->providersz)
				printf(",");
			printf("\n");
		}
		printf("\t],\n");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("Customer AS:              %u\n", p->custasid);
		printf("Provider Set:\n");
		for (i = 0; i < p->providersz; i++) {
			printf("%5zu: AS: %d", i + 1, p->providers[i].as);
			switch (p->providers[i].afi) {
			case AFI_IPV4:
				printf(" (IPv4 only)");
				break;
			case AFI_IPV6:
				printf(" (IPv6 only)");
				break;
			default:
				break;
			}
			printf("\n");
		}
	}
}

static void
takey_print(char *name, const struct takey *t)
{
	char	*spki = NULL;
	size_t	 i, j = 0;

	if (base64_encode(t->pubkey, t->pubkeysz, &spki) != 0)
		errx(1, "base64_encode failed in %s", __func__);

	if (outformats & FORMAT_JSON) {
		printf("\t\t{\n\t\t\t\"name\": \"%s\",\n", name);
		printf("\t\t\t\"comments\": [");
		for (i = 0; i < t->commentsz; i++) {
			printf("\"%s\"", t->comments[i]);
			if (i + 1 < t->commentsz)
				printf(", ");
		}
		printf("],\n");
		printf("\t\t\t\"uris\": [");
		for (i = 0; i < t->urisz; i++) {
			printf("\"%s\"", t->uris[i]);
			if (i + 1 < t->urisz)
				printf(", ");
		}
		printf("],\n");
		printf("\t\t\t\"spki\": \"%s\"\n\t\t}", spki);
	} else {
		printf("TAL derived from the '%s' Trust Anchor Key:\n\n", name);

		for (i = 0; i < t->commentsz; i++) {
			printf("\t# %s\n", t->comments[i]);
		}

		for (i = 0; i < t->urisz; i++) {
			printf("\t%s\n\n\t", t->uris[i]);
		}

		for (i = 0; i < strlen(spki); i++) {
			printf("%c", spki[i]);
			j++;
			if (j == 64) {
				printf("\n\t");
				j = 0;
			}
		}

		printf("\n\n");
	}

	free(spki);
}

void
tak_print(const X509 *x, const struct tak *p)
{
	char	tbuf[21];

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"tak\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"sia\": \"%s\",\n", p->sia);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
		printf("\t\"takeys\": [\n");
	} else {
		strftime(tbuf, sizeof(tbuf), "%FT%TZ", gmtime(&p->expires));
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("TAK valid until:          %s\n", tbuf);
	}

	takey_print("current", p->current);

	if (p->predecessor != NULL) {
		if (outformats & FORMAT_JSON)
			printf(",\n");
		takey_print("predecessor", p->predecessor);
	}

	if (p->successor != NULL) {
		if (outformats & FORMAT_JSON)
			printf(",\n");
		takey_print("successor", p->successor);
	}

	if (outformats & FORMAT_JSON)
		printf("\n\t],\n");
}

void
geofeed_print(const X509 *x, const struct geofeed *p)
{
	char	 buf[128];
	size_t	 i;

	if (outformats & FORMAT_JSON) {
		printf("\t\"type\": \"geofeed\",\n");
		printf("\t\"ski\": \"%s\",\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("\t\"aki\": \"%s\",\n", pretty_key_id(p->aki));
		printf("\t\"aia\": \"%s\",\n", p->aia);
		printf("\t\"valid_until\": %lld,\n", (long long)p->expires);
		printf("\t\"records\": [\n");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Geofeed valid until:      %s\n", time2str(p->expires));
		printf("Geofeed CSV records:\n");
	}

	for (i = 0; i < p->geoipsz; i++) {
		if (p->geoips[i].ip->type != CERT_IP_ADDR)
			continue;

		ip_addr_print(&p->geoips[i].ip->ip, p->geoips[i].ip->afi, buf,
		    sizeof(buf));
		if (outformats & FORMAT_JSON)
			printf("\t\t{ \"prefix\": \"%s\", \"location\": \"%s\""
			    "}", buf, p->geoips[i].loc);
		else
			printf("%5zu: IP: %s (%s)", i + 1, buf,
			    p->geoips[i].loc);

		if (outformats & FORMAT_JSON && i + 1 < p->geoipsz)
			printf(",\n");
		else
			printf("\n");
	}

	if (outformats & FORMAT_JSON)
		printf("\t],\n");
}
