/*	$OpenBSD: mft.c,v 1.82 2022/12/01 10:24:28 claudio Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/safestack.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	*fn; /* manifest file name */
	struct mft	*res; /* result object */
	int		 found_crl;
};

extern ASN1_OBJECT	*mft_oid;

/*
 * Types and templates for the Manifest eContent, RFC 6486, section 4.2.
 */

typedef struct {
	ASN1_IA5STRING	*file;
	ASN1_BIT_STRING	*hash;
} FileAndHash;

DECLARE_STACK_OF(FileAndHash);

#ifndef DEFINE_STACK_OF
#define sk_FileAndHash_num(sk)		SKM_sk_num(FileAndHash, (sk))
#define sk_FileAndHash_value(sk, i)	SKM_sk_value(FileAndHash, (sk), (i))
#endif

typedef struct {
	ASN1_INTEGER		*version;
	ASN1_INTEGER		*manifestNumber;
	ASN1_GENERALIZEDTIME	*thisUpdate;
	ASN1_GENERALIZEDTIME	*nextUpdate;
	ASN1_OBJECT		*fileHashAlg;
	STACK_OF(FileAndHash)	*fileList;
} Manifest;

ASN1_SEQUENCE(FileAndHash) = {
	ASN1_SIMPLE(FileAndHash, file, ASN1_IA5STRING),
	ASN1_SIMPLE(FileAndHash, hash, ASN1_BIT_STRING),
} ASN1_SEQUENCE_END(FileAndHash);

ASN1_SEQUENCE(Manifest) = {
	ASN1_EXP_OPT(Manifest, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(Manifest, manifestNumber, ASN1_INTEGER),
	ASN1_SIMPLE(Manifest, thisUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(Manifest, nextUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(Manifest, fileHashAlg, ASN1_OBJECT),
	ASN1_SEQUENCE_OF(Manifest, fileList, FileAndHash),
} ASN1_SEQUENCE_END(Manifest);

DECLARE_ASN1_FUNCTIONS(Manifest);
IMPLEMENT_ASN1_FUNCTIONS(Manifest);

/*
 * Convert an ASN1_GENERALIZEDTIME to a struct tm.
 * Returns 1 on success, 0 on failure.
 */
static int
generalizedtime_to_tm(const ASN1_GENERALIZEDTIME *gtime, struct tm *tm)
{
	const char *data;
	size_t len;

	data = ASN1_STRING_get0_data(gtime);
	len = ASN1_STRING_length(gtime);

	memset(tm, 0, sizeof(*tm));
	return ASN1_time_parse(data, len, tm, V_ASN1_GENERALIZEDTIME) ==
	    V_ASN1_GENERALIZEDTIME;
}

/*
 * Validate and verify the time validity of the mft.
 * Returns 1 if all is good and for any other case 0.
 */
static int
mft_parse_time(const ASN1_GENERALIZEDTIME *from,
    const ASN1_GENERALIZEDTIME *until, struct parse *p)
{
	struct tm tm_from, tm_until;

	if (!generalizedtime_to_tm(from, &tm_from)) {
		warnx("%s: embedded from time format invalid", p->fn);
		return 0;
	}
	if (!generalizedtime_to_tm(until, &tm_until)) {
		warnx("%s: embedded until time format invalid", p->fn);
		return 0;
	}

	/* check that until is not before from */
	if (ASN1_time_tm_cmp(&tm_until, &tm_from) < 0) {
		warnx("%s: bad update interval", p->fn);
		return 0;
	}

	if ((p->res->valid_since = timegm(&tm_from)) == -1 ||
	    (p->res->valid_until = timegm(&tm_until)) == -1)
		errx(1, "%s: timegm failed", p->fn);

	return 1;
}

/*
 * Determine rtype corresponding to file extension. Returns RTYPE_INVALID
 * on error or unkown extension.
 */
enum rtype
rtype_from_file_extension(const char *fn)
{
	size_t	 sz;

	sz = strlen(fn);
	if (sz < 5)
		return RTYPE_INVALID;

	if (strcasecmp(fn + sz - 4, ".tal") == 0)
		return RTYPE_TAL;
	if (strcasecmp(fn + sz - 4, ".cer") == 0)
		return RTYPE_CER;
	if (strcasecmp(fn + sz - 4, ".crl") == 0)
		return RTYPE_CRL;
	if (strcasecmp(fn + sz - 4, ".mft") == 0)
		return RTYPE_MFT;
	if (strcasecmp(fn + sz - 4, ".roa") == 0)
		return RTYPE_ROA;
	if (strcasecmp(fn + sz - 4, ".gbr") == 0)
		return RTYPE_GBR;
	if (strcasecmp(fn + sz - 4, ".sig") == 0)
		return RTYPE_RSC;
	if (strcasecmp(fn + sz - 4, ".asa") == 0)
		return RTYPE_ASPA;
	if (strcasecmp(fn + sz - 4, ".tak") == 0)
		return RTYPE_TAK;
	if (strcasecmp(fn + sz - 4, ".csv") == 0)
		return RTYPE_GEOFEED;

	return RTYPE_INVALID;
}

/*
 * Validate that a filename listed on a Manifest only contains characters
 * permitted in draft-ietf-sidrops-6486bis section 4.2.2
 * Also ensure that there is exactly one '.'.
 */
static int
valid_mft_filename(const char *fn, size_t len)
{
	const unsigned char *c;

	if (!valid_filename(fn, len))
		return 0;

	c = memchr(fn, '.', len);
	if (c == NULL || c != memrchr(fn, '.', len))
		return 0;

	return 1;
}

/*
 * Check that the file is allowed to be part of a manifest and the parser
 * for this type is implemented in rpki-client.
 * Returns corresponding rtype or RTYPE_INVALID to mark the file as unknown.
 */
static enum rtype
rtype_from_mftfile(const char *fn)
{
	enum rtype		 type;

	type = rtype_from_file_extension(fn);
	switch (type) {
	case RTYPE_CER:
	case RTYPE_CRL:
	case RTYPE_GBR:
	case RTYPE_ROA:
	case RTYPE_ASPA:
	case RTYPE_TAK:
		return type;
	default:
		return RTYPE_INVALID;
	}
}

/*
 * Parse an individual "FileAndHash", RFC 6486, sec. 4.2.
 * Return zero on failure, non-zero on success.
 */
static int
mft_parse_filehash(struct parse *p, const FileAndHash *fh)
{
	char			*fn = NULL;
	int			 rc = 0;
	struct mftfile		*fent;
	enum rtype		 type;

	if (!valid_mft_filename(fh->file->data, fh->file->length)) {
		warnx("%s: RFC 6486 section 4.2.2: bad filename", p->fn);
		goto out;
	}
	fn = strndup(fh->file->data, fh->file->length);
	if (fn == NULL)
		err(1, NULL);

	if (fh->hash->length != SHA256_DIGEST_LENGTH) {
		warnx("%s: RFC 6486 section 4.2.1: hash: "
		    "invalid SHA256 length, have %d",
		    p->fn, fh->hash->length);
		goto out;
	}

	type = rtype_from_mftfile(fn);
	/* remember the filehash for the CRL in struct mft */
	if (type == RTYPE_CRL && strcmp(fn, p->res->crl) == 0) {
		memcpy(p->res->crlhash, fh->hash->data, SHA256_DIGEST_LENGTH);
		p->found_crl = 1;
	}

	/* Insert the filename and hash value. */
	fent = &p->res->files[p->res->filesz++];
	fent->type = type;
	fent->file = fn;
	fn = NULL;
	memcpy(fent->hash, fh->hash->data, SHA256_DIGEST_LENGTH);

	rc = 1;
 out:
	free(fn);
	return rc;
}

/*
 * Handle the eContent of the manifest object, RFC 6486 sec. 4.2.
 * Returns 0 on failure and 1 on success.
 */
static int
mft_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	Manifest		*mft;
	FileAndHash		*fh;
	int			 i, rc = 0;

	if ((mft = d2i_Manifest(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6486 section 4: failed to parse Manifest",
		    p->fn);
		goto out;
	}

	if (!valid_econtent_version(p->fn, mft->version))
		goto out;

	p->res->seqnum = x509_convert_seqnum(p->fn, mft->manifestNumber);
	if (p->res->seqnum == NULL)
		goto out;

	/*
	 * Timestamps: this and next update time.
	 * Validate that the current date falls into this interval.
	 * This is required by section 4.4, (3).
	 * If we're after the given date, then the MFT is stale.
	 * This is made super complicated because it uses OpenSSL's
	 * ASN1_GENERALIZEDTIME instead of ASN1_TIME, which we could
	 * compare against the current time trivially.
	 */

	if (!mft_parse_time(mft->thisUpdate, mft->nextUpdate, p))
		goto out;

	if (OBJ_obj2nid(mft->fileHashAlg) != NID_sha256) {
		warnx("%s: RFC 6486 section 4.2.1: fileHashAlg: "
		    "want SHA256 object, have %s (NID %d)", p->fn,
		    ASN1_tag2str(OBJ_obj2nid(mft->fileHashAlg)),
		    OBJ_obj2nid(mft->fileHashAlg));
		goto out;
	}

	if (sk_FileAndHash_num(mft->fileList) >= MAX_MANIFEST_ENTRIES) {
		warnx("%s: %d exceeds manifest entry limit (%d)", p->fn,
		    sk_FileAndHash_num(mft->fileList), MAX_MANIFEST_ENTRIES);
		goto out;
	}

	p->res->files = calloc(sk_FileAndHash_num(mft->fileList),
	    sizeof(struct mftfile));
	if (p->res->files == NULL)
		err(1, NULL);

	for (i = 0; i < sk_FileAndHash_num(mft->fileList); i++) {
		fh = sk_FileAndHash_value(mft->fileList, i);
		if (!mft_parse_filehash(p, fh))
			goto out;
	}

	if (!p->found_crl) {
		warnx("%s: CRL not part of MFT fileList", p->fn);
		goto out;
	}

	rc = 1;
 out:
	Manifest_free(mft);
	return rc;
}

/*
 * Parse the objects that have been published in the manifest.
 * This conforms to RFC 6486.
 * Note that if the MFT is stale, all referenced objects are stripped
 * from the parsed content.
 * The MFT content is otherwise returned.
 */
struct mft *
mft_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse	 p;
	int		 rc = 0;
	size_t		 cmsz;
	unsigned char	*cms;
	char		*crldp = NULL, *crlfile;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, mft_oid, &cmsz);
	if (cms == NULL)
		return NULL;
	assert(*x509 != NULL);

	if ((p.res = calloc(1, sizeof(struct mft))) == NULL)
		err(1, NULL);

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &p.res->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->sia == NULL ||
	    p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA, or SKI X509 extension", fn);
		goto out;
	}

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	/* get CRL info for later */
	if (!x509_get_crl(*x509, fn, &crldp))
		goto out;
	if (crldp == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "missing CRL distribution point extension", fn);
		goto out;
	}
	crlfile = strrchr(crldp, '/');
	if (crlfile == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: "
		    "invalid CRL distribution point", fn);
		goto out;
	}
	crlfile++;
	if (!valid_mft_filename(crlfile, strlen(crlfile)) ||
	    rtype_from_file_extension(crlfile) != RTYPE_CRL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "bad CRL distribution point extension", fn);
		goto out;
	}
	if ((p.res->crl = strdup(crlfile)) == NULL)
		err(1, NULL);

	if (mft_parse_econtent(cms, cmsz, &p) == 0)
		goto out;

	rc = 1;
out:
	if (rc == 0) {
		mft_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	free(crldp);
	free(cms);
	return p.res;
}

/*
 * Free an MFT pointer.
 * Safe to call with NULL.
 */
void
mft_free(struct mft *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	if (p->files != NULL)
		for (i = 0; i < p->filesz; i++)
			free(p->files[i].file);

	free(p->aia);
	free(p->aki);
	free(p->sia);
	free(p->ski);
	free(p->path);
	free(p->files);
	free(p->seqnum);
	free(p);
}

/*
 * Serialise MFT parsed content into the given buffer.
 * See mft_read() for the other side of the pipe.
 */
void
mft_buffer(struct ibuf *b, const struct mft *p)
{
	size_t		 i;

	io_simple_buffer(b, &p->stale, sizeof(p->stale));
	io_simple_buffer(b, &p->repoid, sizeof(p->repoid));
	io_str_buffer(b, p->path);

	io_str_buffer(b, p->aia);
	io_str_buffer(b, p->aki);
	io_str_buffer(b, p->ski);

	io_simple_buffer(b, &p->filesz, sizeof(size_t));
	for (i = 0; i < p->filesz; i++) {
		io_str_buffer(b, p->files[i].file);
		io_simple_buffer(b, &p->files[i].type,
		    sizeof(p->files[i].type));
		io_simple_buffer(b, &p->files[i].location,
		    sizeof(p->files[i].location));
		io_simple_buffer(b, p->files[i].hash, SHA256_DIGEST_LENGTH);
	}
}

/*
 * Read an MFT structure from the file descriptor.
 * Result must be passed to mft_free().
 */
struct mft *
mft_read(struct ibuf *b)
{
	struct mft	*p = NULL;
	size_t		 i;

	if ((p = calloc(1, sizeof(struct mft))) == NULL)
		err(1, NULL);

	io_read_buf(b, &p->stale, sizeof(p->stale));
	io_read_buf(b, &p->repoid, sizeof(p->repoid));
	io_read_str(b, &p->path);

	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	assert(p->aia && p->aki && p->ski);

	io_read_buf(b, &p->filesz, sizeof(size_t));
	if ((p->files = calloc(p->filesz, sizeof(struct mftfile))) == NULL)
		err(1, NULL);

	for (i = 0; i < p->filesz; i++) {
		io_read_str(b, &p->files[i].file);
		io_read_buf(b, &p->files[i].type, sizeof(p->files[i].type));
		io_read_buf(b, &p->files[i].location,
		    sizeof(p->files[i].location));
		io_read_buf(b, p->files[i].hash, SHA256_DIGEST_LENGTH);
	}

	return p;
}

/*
 * Compare two MFT files, returns 1 if first MFT is preferred and 0 if second
 * MFT should be used.
 */
int
mft_compare(const struct mft *a, const struct mft *b)
{
	int r;

	if (b == NULL)
		return 1;
	if (a == NULL)
		return 0;

	r = strlen(a->seqnum) - strlen(b->seqnum);
	if (r > 0)	/* seqnum in a is longer -> higher */
		return 1;
	if (r < 0)	/* seqnum in a is shorter -> smaller */
		return 0;

	r = strcmp(a->seqnum, b->seqnum);
	if (r >= 0)	/* a is greater or equal, prefer a */
		return 1;
	return 0;
}
