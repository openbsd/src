/*	$OpenBSD: mft.c,v 1.53 2022/02/10 17:33:28 claudio Exp $ */
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	*fn; /* manifest file name */
	struct mft	*res; /* result object */
};

extern ASN1_OBJECT    *mft_oid;

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

	if ((p->res->valid_from = mktime(&tm_from)) == -1 ||
	    (p->res->valid_until = mktime(&tm_until)) == -1)
		errx(1, "%s: mktime failed", p->fn);

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

	return RTYPE_INVALID;
}

/*
 * Validate that a filename listed on a Manifest only contains characters
 * permitted in draft-ietf-sidrops-6486bis section 4.2.2
 */
static int
valid_filename(const char *fn, size_t len)
{
	const unsigned char *c;
	size_t i;

	for (c = fn, i = 0; i < len; i++, c++)
		if (!isalnum(*c) && *c != '-' && *c != '_' && *c != '.')
			return 0;

	c = memchr(fn, '.', len);
	if (c == NULL || c != memrchr(fn, '.', len))
		return 0;

	return 1;
}

/*
 * Check that the file is a CER, CRL, GBR or a ROA.
 * Returns corresponding rtype or RTYPE_INVALID on error.
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
mft_parse_filehash(struct parse *p, const ASN1_OCTET_STRING *os)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*file, *hash;
	char			*fn = NULL;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 rc = 0;
	struct mftfile		*fent;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6486 section 4.2.1: FileAndHash: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	} else if (sk_ASN1_TYPE_num(seq) != 2) {
		warnx("%s: RFC 6486 section 4.2.1: FileAndHash: "
		    "want 2 elements, have %d", p->fn,
		    sk_ASN1_TYPE_num(seq));
		goto out;
	}

	/* First is the filename itself. */

	file = sk_ASN1_TYPE_value(seq, 0);
	if (file->type != V_ASN1_IA5STRING) {
		warnx("%s: RFC 6486 section 4.2.1: FileAndHash: "
		    "want ASN.1 IA5 string, have %s (NID %d)",
		    p->fn, ASN1_tag2str(file->type), file->type);
		goto out;
	}
	if (!valid_filename(file->value.ia5string->data,
	    file->value.ia5string->length)) {
		warnx("%s: RFC 6486 section 4.2.2: bad filename", p->fn);
		goto out;
	}
	fn = strndup((const char *)file->value.ia5string->data,
	    file->value.ia5string->length);
	if (fn == NULL)
		err(1, NULL);

	/* Now hash value. */

	hash = sk_ASN1_TYPE_value(seq, 1);
	if (hash->type != V_ASN1_BIT_STRING) {
		warnx("%s: RFC 6486 section 4.2.1: FileAndHash: "
		    "want ASN.1 bit string, have %s (NID %d)",
		    p->fn, ASN1_tag2str(hash->type), hash->type);
		goto out;
	}

	if (hash->value.bit_string->length != SHA256_DIGEST_LENGTH) {
		warnx("%s: RFC 6486 section 4.2.1: hash: "
		    "invalid SHA256 length, have %d",
		    p->fn, hash->value.bit_string->length);
		goto out;
	}

	/* Insert the filename and hash value. */
	fent = &p->res->files[p->res->filesz++];

	fent->type = rtype_from_mftfile(fn);
	fent->file = fn;
	fn = NULL;
	memcpy(fent->hash, hash->value.bit_string->data, SHA256_DIGEST_LENGTH);

	rc = 1;
out:
	free(fn);
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse the "FileAndHash" sequence, RFC 6486, sec. 4.2.
 * Return zero on failure, non-zero on success.
 */
static int
mft_parse_flist(struct parse *p, const ASN1_OCTET_STRING *os)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i, rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6486 section 4.2: fileList: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	if (sk_ASN1_TYPE_num(seq) > MAX_MANIFEST_ENTRIES) {
		warnx("%s: %d exceeds manifest entry limit (%d)", p->fn,
		    sk_ASN1_TYPE_num(seq), MAX_MANIFEST_ENTRIES);
		goto out;
	}

	p->res->files = calloc(sk_ASN1_TYPE_num(seq), sizeof(struct mftfile));
	if (p->res->files == NULL)
		err(1, NULL);

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);
		if (t->type != V_ASN1_SEQUENCE) {
			warnx("%s: RFC 6486 section 4.2: fileList: "
			    "want ASN.1 sequence, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		} else if (!mft_parse_filehash(p, t->value.octet_string))
			goto out;
	}

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Handle the eContent of the manifest object, RFC 6486 sec. 4.2.
 * Returns 0 on failure and 1 on success.
 */
static int
mft_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	const ASN1_GENERALIZEDTIME *from, *until;
	long			 mft_version;
	int			 i = 0, rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6486 section 4.2: Manifest: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	/* Test if the optional profile version field is present. */
	if (sk_ASN1_TYPE_num(seq) != 5 &&
	    sk_ASN1_TYPE_num(seq) != 6) {
		warnx("%s: RFC 6486 section 4.2: Manifest: "
		    "want 5 or 6 elements, have %d", p->fn,
		    sk_ASN1_TYPE_num(seq));
		goto out;
	}

	/* Parse the optional version field */
	if (sk_ASN1_TYPE_num(seq) == 6) {
		t = sk_ASN1_TYPE_value(seq, i++);
		d = t->value.asn1_string->data;
		dsz = t->value.asn1_string->length;

		if (cms_econtent_version(p->fn, &d, dsz, &mft_version) == -1)
			goto out;

		switch (mft_version) {
		case 0:
			warnx("%s: incorrect encoding for version 0", p->fn);
			goto out;
		default:
			warnx("%s: version %ld not supported (yet)", p->fn,
			    mft_version);
			goto out;
		}
	}

	/* Now the manifest sequence number. */

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_INTEGER) {
		warnx("%s: RFC 6486 section 4.2.1: manifestNumber: "
		    "want ASN.1 integer, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}

	p->res->seqnum = x509_convert_seqnum(p->fn, t->value.integer);
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

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_GENERALIZEDTIME) {
		warnx("%s: RFC 6486 section 4.2.1: thisUpdate: "
		    "want ASN.1 generalised time, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	from = t->value.generalizedtime;

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_GENERALIZEDTIME) {
		warnx("%s: RFC 6486 section 4.2.1: nextUpdate: "
		    "want ASN.1 generalised time, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	until = t->value.generalizedtime;

	if (!mft_parse_time(from, until, p))
		goto out;

	/* File list algorithm. */

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_OBJECT) {
		warnx("%s: RFC 6486 section 4.2.1: fileHashAlg: "
		    "want ASN.1 object, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (OBJ_obj2nid(t->value.object) != NID_sha256) {
		warnx("%s: RFC 6486 section 4.2.1: fileHashAlg: "
		    "want SHA256 object, have %s (NID %d)", p->fn,
		    ASN1_tag2str(OBJ_obj2nid(t->value.object)),
		    OBJ_obj2nid(t->value.object));
		goto out;
	}

	/* Now the sequence. */

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RFC 6486 section 4.2.1: fileList: "
		    "want ASN.1 sequence, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}

	if (!mft_parse_flist(p, t->value.octet_string))
		goto out;

	rc = 1;
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
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

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, mft_oid, &cmsz);
	if (cms == NULL)
		return NULL;
	assert(*x509 != NULL);

	if ((p.res = calloc(1, sizeof(struct mft))) == NULL)
		err(1, NULL);

	p.res->aia = x509_get_aia(*x509, fn);
	p.res->aki = x509_get_aki(*x509, 0, fn);
	p.res->ski = x509_get_ski(*x509, fn);
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI or SKI X509 extension", fn);
		goto out;
	}

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
