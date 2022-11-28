/*	$OpenBSD: geofeed.c,v 1.7 2022/11/28 15:22:13 tb Exp $ */
/*
 * Copyright (c) 2022 Job Snijders <job@fastly.com>
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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#include "extern.h"

struct	parse {
	const char	*fn;
	struct geofeed	*res;
};

extern ASN1_OBJECT	*geofeed_oid;

/*
 * Take a CIDR prefix (in presentation format) and add it to parse results.
 * Returns 1 on success, 0 on failure.
 */
static int
geofeed_parse_geoip(struct geofeed *res, char *cidr, char *loc)
{
	struct geoip	*geoip;
	struct ip_addr	*ipaddr;
	enum afi	 afi;
	int		 plen;

	if ((ipaddr = calloc(1, sizeof(struct ip_addr))) == NULL)
		err(1, NULL);

	if ((plen = inet_net_pton(AF_INET, cidr, ipaddr->addr,
	    sizeof(ipaddr->addr))) != -1)
		afi = AFI_IPV4;
	else if ((plen = inet_net_pton(AF_INET6, cidr, ipaddr->addr,
	    sizeof(ipaddr->addr))) != -1)
		afi = AFI_IPV6;
	else {
		static char buf[80];

		if (strnvis(buf, cidr, sizeof(buf), VIS_SAFE)
		    >= (int)sizeof(buf)) {
			memcpy(buf + sizeof(buf) - 4, "...", 4);
		}
		warnx("invalid address: %s", buf);
		free(ipaddr);
		return 0;
	}

	ipaddr->prefixlen = plen;

	res->geoips = recallocarray(res->geoips, res->geoipsz,
	    res->geoipsz + 1, sizeof(struct geoip));
	if (res->geoips == NULL)
		err(1, NULL);
	geoip = &res->geoips[res->geoipsz++];

	if ((geoip->ip = calloc(1, sizeof(struct cert_ip))) == NULL)
		err(1, NULL);

	geoip->ip->type = CERT_IP_ADDR;
	geoip->ip->ip = *ipaddr;
	geoip->ip->afi = afi;

	if ((geoip->loc = strdup(loc)) == NULL)
		err(1, NULL);

	if (!ip_cert_compose_ranges(geoip->ip))
		return 0;

	return 1;
}

/*
 * Parse a full RFC 9092 file.
 * Returns the Geofeed, or NULL if the object was malformed.
 */
struct geofeed *
geofeed_parse(X509 **x509, const char *fn, char *buf, size_t len)
{
	struct parse	 p;
	char		*delim, *line, *loc, *nl;
	ssize_t		 linelen;
	BIO		*bio;
	char		*b64 = NULL;
	size_t		 b64sz;
	unsigned char	*der = NULL;
	size_t		 dersz;
	const ASN1_TIME	*at;
	struct cert	*cert = NULL;
	int		 rpki_signature_seen = 0, end_signature_seen = 0;
	int		 rc = 0;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		errx(1, "BIO_new");

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	if ((p.res = calloc(1, sizeof(struct geofeed))) == NULL)
		err(1, NULL);

	while ((nl = memchr(buf, '\n', len)) != NULL) {
		line = buf;

		/* advance buffer to next line */
		len -= nl + 1 - buf;
		buf = nl + 1;

		/* replace LF and CR with NUL, point nl at first NUL */
		*nl = '\0';
		if (nl > line && nl[-1] == '\r') {
			nl[-1] = '\0';
			nl--;
			linelen = nl - line;
		} else {
			warnx("%s: malformed file, expected CRLF line"
			    " endings", fn);
			goto out;
		}

		if (end_signature_seen) {
			warnx("%s: trailing data after signature section", fn);
			goto out;
		}

		if (rpki_signature_seen) {
			if (strncmp(line, "# End Signature:",
			    strlen("# End Signature:")) == 0) {
				end_signature_seen = 1;
				continue;
			}

			if (linelen > 74) {
				warnx("%s: line in signature section too long",
				    fn);
				goto out;
			}
			if (strncmp(line, "# ", strlen("# ")) != 0) {
				warnx("%s: line in signature section too "
				    "short", fn);
				goto out;
			}

			/* skip over "# " */
			line += 2;
			strlcat(b64, line, b64sz);
			continue;
		}

		if (strncmp(line, "# RPKI Signature:",
		    strlen("# RPKI Signature:")) == 0) {
			rpki_signature_seen = 1;

			if ((b64 = calloc(1, len)) == NULL)
				err(1, NULL);
			b64sz = len;

			continue;
		}

		/*
		 * Read the Geofeed CSV records into a BIO to later on
		 * calculate the message digest and compare with the one
		 * in the detached CMS signature.
		 */
		if (BIO_puts(bio, line) != linelen ||
		    BIO_puts(bio, "\r\n") != 2) {
			warnx("%s: BIO_puts failed", fn);
			goto out;
		}

		/* Skip empty lines or commented lines. */
		if (linelen == 0 || line[0] == '#')
			continue;

		/* zap comments */
		delim = memchr(line, '#', linelen);
		if (delim != NULL)
			*delim = '\0';

		/* Split prefix and location info */
		delim = memchr(line, ',', linelen);
		if (delim != NULL) {
			*delim = '\0';
			loc = delim + 1;
		} else
			loc = "";

		/* read each prefix  */
		if (!geofeed_parse_geoip(p.res, line, loc))
			goto out;
	}

	if (!rpki_signature_seen || !end_signature_seen) {
		warnx("%s: absent or invalid signature", fn);
		goto out;
	}

	if ((base64_decode(b64, strlen(b64), &der, &dersz)) == -1) {
		warnx("%s: base64_decode failed", fn);
		goto out;
	}

	if (!cms_parse_validate_detached(x509, fn, der, dersz, geofeed_oid,
	    bio))
		goto out;

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;

	if (p.res->aia == NULL || p.res->aki == NULL || p.res->ski == NULL) {
		warnx("%s: missing AIA, AKI, or SKI X509 extension", fn);
		goto out;
	}

	at = X509_get0_notAfter(*x509);
	if (at == NULL) {
		warnx("%s: X509_get0_notAfter failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &p.res->expires)) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}

	if ((cert = cert_parse_ee_cert(fn, *x509)) == NULL)
		goto out;

	if (x509_any_inherits(*x509)) {
		warnx("%s: inherit elements not allowed in EE cert", fn);
		goto out;
	}

	if (cert->asz > 0) {
		warnx("%s: superfluous AS Resources extension present", fn);
		goto out;
	}

	p.res->valid = valid_geofeed(fn, cert, p.res);

	rc = 1;
 out:
	if (rc == 0) {
		geofeed_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	cert_free(cert);
	BIO_free(bio);
	free(b64);
	free(der);

	return p.res;
}

/*
 * Free what follows a pointer to a geofeed structure.
 * Safe to call with NULL.
 */
void
geofeed_free(struct geofeed *p)
{
	size_t i;

	if (p == NULL)
		return;

	for (i = 0; i < p->geoipsz; i++) {
		free(p->geoips[i].ip);
		free(p->geoips[i].loc);
	}

	free(p->geoips);
	free(p->aia);
	free(p->aki);
	free(p->ski);
	free(p);
}
