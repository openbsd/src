/*	$OpenBSD: validate.c,v 1.40 2022/06/10 10:36:43 tb Exp $ */
/*
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

#include <sys/socket.h>

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

extern ASN1_OBJECT	*certpol_oid;

/*
 * Walk up the chain of certificates trying to match our AS number to
 * one of the allocations in that chain.
 * Returns 1 if covered or 0 if not.
 */
static int
valid_as(struct auth *a, uint32_t min, uint32_t max)
{
	int	 c;

	if (a == NULL)
		return 0;

	/* Does this certificate cover our AS number? */
	c = as_check_covered(min, max, a->cert->as, a->cert->asz);
	if (c > 0)
		return 1;
	else if (c < 0)
		return 0;

	/* If it inherits, walk up the chain. */
	return valid_as(a->parent, min, max);
}

/*
 * Walk up the chain of certificates (really just the last one, but in
 * the case of inheritance, the ones before) making sure that our IP
 * prefix is covered in the first non-inheriting specification.
 * Returns 1 if covered or 0 if not.
 */
static int
valid_ip(struct auth *a, enum afi afi,
    const unsigned char *min, const unsigned char *max)
{
	int	 c;

	if (a == NULL)
		return 0;

	/* Does this certificate cover our IP prefix? */
	c = ip_addr_check_covered(afi, min, max, a->cert->ips, a->cert->ipsz);
	if (c > 0)
		return 1;
	else if (c < 0)
		return 0;

	/* If it inherits, walk up the chain. */
	return valid_ip(a->parent, afi, min, max);
}

/*
 * Make sure that the SKI doesn't already exist and return the parent by
 * its AKI.
 * Returns the parent auth or NULL on failure.
 */
struct auth *
valid_ski_aki(const char *fn, struct auth_tree *auths,
    const char *ski, const char *aki)
{
	struct auth *a;

	if (auth_find(auths, ski) != NULL) {
		warnx("%s: RFC 6487: duplicate SKI", fn);
		return NULL;
	}

	a = auth_find(auths, aki);
	if (a == NULL)
		warnx("%s: RFC 6487: unknown AKI", fn);

	return a;
}

/*
 * Authenticate a trust anchor by making sure its resources are not
 * inheriting and that the SKI is unique.
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_ta(const char *fn, struct auth_tree *auths, const struct cert *cert)
{
	size_t	 i;

	/* AS and IP resources must not inherit. */
	if (cert->asz && cert->as[0].type == CERT_AS_INHERIT) {
		warnx("%s: RFC 6487 (trust anchor): "
		    "inheriting AS resources", fn);
		return 0;
	}
	for (i = 0; i < cert->ipsz; i++)
		if (cert->ips[i].type == CERT_IP_INHERIT) {
			warnx("%s: RFC 6487 (trust anchor): "
			    "inheriting IP resources", fn);
			return 0;
		}

	/* SKI must not be a dupe. */
	if (auth_find(auths, cert->ski) != NULL) {
		warnx("%s: RFC 6487: duplicate SKI", fn);
		return 0;
	}

	return 1;
}

/*
 * Validate a non-TA certificate: make sure its IP and AS resources are
 * fully covered by those in the authority key (which must exist).
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_cert(const char *fn, struct auth *a, const struct cert *cert)
{
	size_t		 i;
	uint32_t	 min, max;
	char		 buf1[64], buf2[64];

	for (i = 0; i < cert->asz; i++) {
		if (cert->as[i].type == CERT_AS_INHERIT) {
			if (cert->purpose == CERT_PURPOSE_BGPSEC_ROUTER)
				return 0; /* BGPsec doesn't permit inheriting */
			continue;
		}
		min = cert->as[i].type == CERT_AS_ID ?
		    cert->as[i].id : cert->as[i].range.min;
		max = cert->as[i].type == CERT_AS_ID ?
		    cert->as[i].id : cert->as[i].range.max;
		if (valid_as(a, min, max))
			continue;
		warnx("%s: RFC 6487: uncovered AS: "
		    "%u--%u", fn, min, max);
		return 0;
	}

	for (i = 0; i < cert->ipsz; i++) {
		if (valid_ip(a, cert->ips[i].afi, cert->ips[i].min,
		    cert->ips[i].max))
			continue;
		switch (cert->ips[i].type) {
		case CERT_IP_RANGE:
			ip_addr_print(&cert->ips[i].range.min,
			    cert->ips[i].afi, buf1, sizeof(buf1));
			ip_addr_print(&cert->ips[i].range.max,
			    cert->ips[i].afi, buf2, sizeof(buf2));
			warnx("%s: RFC 6487: uncovered IP: "
			    "%s--%s", fn, buf1, buf2);
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&cert->ips[i].ip,
			    cert->ips[i].afi, buf1, sizeof(buf1));
			warnx("%s: RFC 6487: uncovered IP: "
			    "%s", fn, buf1);
			break;
		case CERT_IP_INHERIT:
			warnx("%s: RFC 6487: uncovered IP: "
			    "(inherit)", fn);
			break;
		}
		return 0;
	}

	return 1;
}

/*
 * Validate our ROA: check that the prefixes (ipAddrBlocks) are contained.
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_roa(const char *fn, struct auth *a, struct roa *roa)
{
	size_t	 i;
	char	 buf[64];

	for (i = 0; i < roa->ipsz; i++) {
		if (valid_ip(a, roa->ips[i].afi, roa->ips[i].min,
		    roa->ips[i].max))
			continue;
		ip_addr_print(&roa->ips[i].addr,
		    roa->ips[i].afi, buf, sizeof(buf));
		warnx("%s: RFC 6482: uncovered IP: "
		    "%s", fn, buf);
		return 0;
	}

	return 1;
}

/*
 * Validate a file by verifying the SHA256 hash of that file.
 * The file to check is passed as a file descriptor.
 * Returns 1 if hash matched, 0 otherwise. Closes fd when done.
 */
int
valid_filehash(int fd, const char *hash, size_t hlen)
{
	SHA256_CTX	ctx;
	char		filehash[SHA256_DIGEST_LENGTH];
	char		buffer[8192];
	ssize_t		nr;

	if (hlen != sizeof(filehash))
		errx(1, "bad hash size");

	if (fd == -1)
		return 0;

	SHA256_Init(&ctx);
	while ((nr = read(fd, buffer, sizeof(buffer))) > 0)
		SHA256_Update(&ctx, buffer, nr);
	close(fd);
	SHA256_Final(filehash, &ctx);

	if (memcmp(hash, filehash, sizeof(filehash)) != 0)
		return 0;
	return 1;
}

/*
 * Same as above but with a buffer instead of a fd.
 */
int
valid_hash(unsigned char *buf, size_t len, const char *hash, size_t hlen)
{
	char	filehash[SHA256_DIGEST_LENGTH];

	if (hlen != sizeof(filehash))
		errx(1, "bad hash size");

	if (buf == NULL || len == 0)
		return 0;

	if (!EVP_Digest(buf, len, filehash, NULL, EVP_sha256(), NULL))
		errx(1, "EVP_Digest failed");

	if (memcmp(hash, filehash, sizeof(filehash)) != 0)
		return 0;
	return 1;
}

/*
 * Validate that a filename only contains characters from the POSIX portable
 * filename character set [A-Za-z0-9._-], see IEEE Std 1003.1-2013, 3.278.
 */
int
valid_filename(const char *fn, size_t len)
{
	const unsigned char *c;
	size_t i;

	for (c = fn, i = 0; i < len; i++, c++)
		if (!isalnum(*c) && *c != '-' && *c != '_' && *c != '.')
			return 0;
	return 1;
}

/*
 * Validate a URI to make sure it is pure ASCII and does not point backwards
 * or doing some other silly tricks. To enforce the protocol pass either
 * https:// or rsync:// as proto, if NULL is passed no protocol is enforced.
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_uri(const char *uri, size_t usz, const char *proto)
{
	size_t s;

	if (usz > MAX_URI_LENGTH)
		return 0;

	for (s = 0; s < usz; s++)
		if (!isalnum((unsigned char)uri[s]) &&
		    !ispunct((unsigned char)uri[s]))
			return 0;

	if (proto != NULL) {
		s = strlen(proto);
		if (strncasecmp(uri, proto, s) != 0)
			return 0;
	}

	/* do not allow files or directories to start with a '.' */
	if (strstr(uri, "/.") != NULL)
		return 0;

	return 1;
}

/*
 * Validate that a URI has the same host as the URI passed in proto.
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_origin(const char *uri, const char *proto)
{
	const char *to;

	/* extract end of host from proto URI */
	to = strstr(proto, "://");
	if (to == NULL)
		return 0;
	to += strlen("://");
	if ((to = strchr(to, '/')) == NULL)
		return 0;

	/* compare hosts including the / for the start of the path section */
	if (strncasecmp(uri, proto, to - proto + 1) != 0)
		return 0;

	return 1;
}

/*
 * Walk the certificate tree to the root and build a certificate
 * chain from cert->x509. All certs in the tree are validated and
 * can be loaded as trusted stack into the validator.
 */
static void
build_chain(const struct auth *a, STACK_OF(X509) **chain)
{
	*chain = NULL;

	if (a == NULL)
		return;

	if ((*chain = sk_X509_new_null()) == NULL)
		err(1, "sk_X509_new_null");
	for (; a != NULL; a = a->parent) {
		assert(a->cert->x509 != NULL);
		if (!sk_X509_push(*chain, a->cert->x509))
			errx(1, "sk_X509_push");
	}
}

/*
 * Add the CRL based on the certs SKI value.
 * No need to insert any other CRL since those were already checked.
 */
static void
build_crls(const struct crl *crl, STACK_OF(X509_CRL) **crls)
{
	*crls = NULL;

	if (crl == NULL)
		return;
	if ((*crls = sk_X509_CRL_new_null()) == NULL)
		errx(1, "sk_X509_CRL_new_null");
	if (!sk_X509_CRL_push(*crls, crl->x509_crl))
		err(1, "sk_X509_CRL_push");
}

/*
 * Validate the X509 certificate.  If crl is NULL don't check CRL.
 * Returns 1 for valid certificates, returns 0 if there is a verify error
 */
int
valid_x509(char *file, X509_STORE_CTX *store_ctx, X509 *x509, struct auth *a,
    struct crl *crl, int nowarn)
{
	X509_VERIFY_PARAM	*params;
	ASN1_OBJECT		*cp_oid;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls = NULL;
	unsigned long		 flags;
	int			 c;

	build_chain(a, &chain);
	build_crls(crl, &crls);

	assert(store_ctx != NULL);
	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(store_ctx, NULL, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");

	if ((params = X509_STORE_CTX_get0_param(store_ctx)) == NULL)
		cryptoerrx("X509_STORE_CTX_get0_param");
	if ((cp_oid = OBJ_dup(certpol_oid)) == NULL)
		cryptoerrx("OBJ_dup");
	if (!X509_VERIFY_PARAM_add0_policy(params, cp_oid))
		cryptoerrx("X509_VERIFY_PARAM_add0_policy");

	flags = X509_V_FLAG_CRL_CHECK;
	flags |= X509_V_FLAG_EXPLICIT_POLICY;
	flags |= X509_V_FLAG_INHIBIT_MAP;
	X509_STORE_CTX_set_flags(store_ctx, flags);
	X509_STORE_CTX_set_depth(store_ctx, MAX_CERT_DEPTH);
	X509_STORE_CTX_set0_trusted_stack(store_ctx, chain);
	X509_STORE_CTX_set0_crls(store_ctx, crls);

	if (X509_verify_cert(store_ctx) <= 0) {
		c = X509_STORE_CTX_get_error(store_ctx);
		if (!nowarn || verbose > 1)
			warnx("%s: %s", file, X509_verify_cert_error_string(c));
		X509_STORE_CTX_cleanup(store_ctx);
		sk_X509_free(chain);
		sk_X509_CRL_free(crls);
		return 0;
	}

	X509_STORE_CTX_cleanup(store_ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	return 1;
}

/*
 * Validate our RSC: check that all items in the ResourceBlock are contained.
 * Returns 1 if valid, 0 otherwise.
 */
int
valid_rsc(const char *fn, struct auth *a, struct rsc *rsc)
{
	size_t		i;
	uint32_t	min, max;
	char		buf1[64], buf2[64];

	for (i = 0; i < rsc->asz; i++) {
		if (rsc->as[i].type == CERT_AS_INHERIT) {
			warnx("%s: RSC ResourceBlock: illegal inherit", fn);
			return 0;
		}

		min = rsc->as[i].type == CERT_AS_RANGE ? rsc->as[i].range.min
		    : rsc->as[i].id;
		max = rsc->as[i].type == CERT_AS_RANGE ? rsc->as[i].range.max
		    : rsc->as[i].id;

		if (valid_as(a, min, max))
			continue;

		switch (rsc->as[i].type) {
		case CERT_AS_ID:
			warnx("%s: RSC resourceBlock: uncovered AS Identifier: "
			    "%u", fn, rsc->as[i].id);
			break;
		case CERT_AS_RANGE:
			warnx("%s: RSC resourceBlock: uncovered AS Range: "
			    "%u--%u", fn, min, max);
			break;
		default:
			break;
		}
		return 0;
	}

	for (i = 0; i < rsc->ipsz; i++) {
		if (rsc->ips[i].type == CERT_IP_INHERIT) {
			warnx("%s: RSC ResourceBlock: illegal inherit", fn);
			return 0;
		}

		if (valid_ip(a, rsc->ips[i].afi, rsc->ips[i].min,
		    rsc->ips[i].max))
			continue;

		switch (rsc->ips[i].type) {
		case CERT_IP_RANGE:
			ip_addr_print(&rsc->ips[i].range.min,
			    rsc->ips[i].afi, buf1, sizeof(buf1));
			ip_addr_print(&rsc->ips[i].range.max,
			    rsc->ips[i].afi, buf2, sizeof(buf2));
			warnx("%s: RSC ResourceBlock: uncovered IP Range: "
			    "%s--%s", fn, buf1, buf2);
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&rsc->ips[i].ip,
			    rsc->ips[i].afi, buf1, sizeof(buf1));
			warnx("%s: RSC ResourceBlock: uncovered IP: "
			    "%s", fn, buf1);
			break;
		default:
			break;
		}
		return 0;
	}

	return 1;
}

int
valid_econtent_version(const char *fn, const ASN1_INTEGER *aint)
{
	long version;

	if (aint == NULL)
		return 1;

	if ((version = ASN1_INTEGER_get(aint)) < 0) {
		warnx("%s: ASN1_INTEGER_get failed", fn);
		return 0;
	}

	switch (version) {
	case 0:
		warnx("%s: incorrect encoding for version 0", fn);
		return 0;
	default:
		warnx("%s: version %ld not supported (yet)", fn, version);
		return 0;
	}
}
