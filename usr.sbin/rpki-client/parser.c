/*	$OpenBSD: parser.c,v 1.136 2024/05/20 15:51:43 claudio Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <imsg.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "extern.h"

static X509_STORE_CTX	*ctx;
static struct auth_tree	 auths = RB_INITIALIZER(&auths);
static struct crl_tree	 crlt = RB_INITIALIZER(&crlt);

struct parse_repo {
	RB_ENTRY(parse_repo)	 entry;
	char			*path;
	char			*validpath;
	unsigned int		 id;
};

static RB_HEAD(repo_tree, parse_repo)	repos = RB_INITIALIZER(&repos);

static inline int
repocmp(struct parse_repo *a, struct parse_repo *b)
{
	return a->id - b->id;
}

RB_GENERATE_STATIC(repo_tree, parse_repo, entry, repocmp);

static struct parse_repo *
repo_get(unsigned int id)
{
	struct parse_repo needle = { .id = id };

	return RB_FIND(repo_tree, &repos, &needle);
}

static void
repo_add(unsigned int id, char *path, char *validpath)
{
	struct parse_repo *rp;

	if ((rp = calloc(1, sizeof(*rp))) == NULL)
		err(1, NULL);
	rp->id = id;
	if (path != NULL)
		if ((rp->path = strdup(path)) == NULL)
			err(1, NULL);
	if (validpath != NULL)
		if ((rp->validpath = strdup(validpath)) == NULL)
			err(1, NULL);

	if (RB_INSERT(repo_tree, &repos, rp) != NULL)
		errx(1, "repository already added: id %d, %s", id, path);
}

/*
 * Return the issuer by its certificate id, or NULL on failure.
 * Make sure the AKI is the same as the AKI listed on the Manifest,
 * and that the SKI of the cert matches with the AKI.
 */
static struct auth *
find_issuer(const char *fn, int id, const char *aki, const char *mftaki)
{
	struct auth *a;

	a = auth_find(&auths, id);
	if (a == NULL) {
		warnx("%s: RFC 6487: unknown cert with SKI %s", fn, aki);
		return NULL;
	}

	if (mftaki != NULL) {
		if (strcmp(aki, mftaki) != 0) {
			warnx("%s: AKI %s doesn't match Manifest AKI %s", fn,
			    aki, mftaki);
			return NULL;
		}
	}

	if (strcmp(aki, a->cert->ski) != 0) {
		warnx("%s: AKI %s doesn't match issuer SKI %s", fn,
		    aki, a->cert->ski);
		return NULL;
	}

	return a;
}

/*
 * Build access path to file based on repoid, path, location and file values.
 */
static char *
parse_filepath(unsigned int repoid, const char *path, const char *file,
    enum location loc)
{
	struct parse_repo	*rp;
	char			*fn, *repopath;

	/* build file path based on repoid, entity path and filename */
	rp = repo_get(repoid);
	if (rp == NULL)
		errx(1, "build file path: repository %u missing", repoid);

	if (loc == DIR_VALID)
		repopath = rp->validpath;
	else
		repopath = rp->path;

	if (repopath == NULL)
		return NULL;

	if (path == NULL) {
		if (asprintf(&fn, "%s/%s", repopath, file) == -1)
			err(1, NULL);
	} else {
		if (asprintf(&fn, "%s/%s/%s", repopath, path, file) == -1)
			err(1, NULL);
	}
	return fn;
}

/*
 * Parse and validate a ROA.
 * This is standard stuff.
 * Returns the roa on success, NULL on failure.
 */
static struct roa *
proc_parser_roa(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct roa		*roa;
	struct auth		*a;
	struct crl		*crl;
	X509			*x509;
	const char		*errstr;

	if ((roa = roa_parse(&x509, file, entp->talid, der, len)) == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, roa->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, x509, a, crl, &errstr)) {
		warnx("%s: %s", file, errstr);
		X509_free(x509);
		roa_free(roa);
		return NULL;
	}
	X509_free(x509);

	roa->talid = a->cert->talid;

	roa->expires = x509_find_expires(roa->notafter, a, &crlt);

	return roa;
}

/*
 * Parse and validate a draft-ietf-sidrops-rpki-prefixlist SPL.
 * Returns the spl on success, NULL on failure.
 */
static struct spl *
proc_parser_spl(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct spl		*spl;
	struct auth		*a;
	struct crl		*crl;
	X509			*x509;
	const char		*errstr;

	if ((spl = spl_parse(&x509, file, entp->talid, der, len)) == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, spl->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, x509, a, crl, &errstr)) {
		warnx("%s: %s", file, errstr);
		X509_free(x509);
		spl_free(spl);
		return NULL;
	}
	X509_free(x509);

	spl->talid = a->cert->talid;

	spl->expires = x509_find_expires(spl->notafter, a, &crlt);

	return spl;
}

/*
 * Check all files and their hashes in a MFT structure.
 * Return zero on failure, non-zero on success.
 */
static int
proc_parser_mft_check(const char *fn, struct mft *p)
{
	const enum location loc[2] = { DIR_TEMP, DIR_VALID };
	size_t	 i;
	int	 rc = 1;
	char	*path;

	if (p == NULL)
		return 0;

	for (i = 0; i < p->filesz; i++) {
		struct mftfile *m = &p->files[i];
		int try, fd = -1, noent = 0, valid = 0;
		for (try = 0; try < 2 && !valid; try++) {
			if ((path = parse_filepath(p->repoid, p->path, m->file,
			    loc[try])) == NULL)
				continue;
			fd = open(path, O_RDONLY);
			if (fd == -1 && errno == ENOENT)
				noent++;
			free(path);

			/* remember which path was checked */
			m->location = loc[try];
			valid = valid_filehash(fd, m->hash, sizeof(m->hash));
		}

		if (!valid) {
			/* silently skip not-existing unknown files */
			if (m->type == RTYPE_INVALID && noent == 2)
				continue;
			warnx("%s#%s: bad message digest for %s", fn,
			    p->seqnum, m->file);
			rc = 0;
			continue;
		}
	}

	return rc;
}

/*
 * Load the CRL from loc using the info from the MFT.
 */
static struct crl *
parse_load_crl_from_mft(struct entity *entp, struct mft *mft, enum location loc,
    char **crlfile)
{
	struct crl	*crl = NULL;
	unsigned char	*f = NULL;
	char		*fn = NULL;
	size_t		 flen;

	*crlfile = NULL;

	fn = parse_filepath(entp->repoid, entp->path, mft->crl, loc);
	if (fn == NULL)
		goto out;

	f = load_file(fn, &flen);
	if (f == NULL) {
		if (errno != ENOENT)
			warn("parse file %s", fn);
		goto out;
	}

	if (!valid_hash(f, flen, mft->crlhash, sizeof(mft->crlhash)))
		goto out;

	crl = crl_parse(fn, f, flen);
	if (crl == NULL)
		goto out;

	if (strcmp(crl->aki, mft->aki) != 0) {
		warnx("%s: AKI doesn't match Manifest AKI", fn);
		goto out;
	}

	if ((crl->mftpath = strdup(mft->sia)) == NULL)
		err(1, NULL);

	*crlfile = fn;
	free(f);

	return crl;

 out:
	crl_free(crl);
	free(f);
	free(fn);

	return NULL;
}

/*
 * Parse and validate a manifest file.
 * Don't check the fileandhash, this is done later on.
 * Return the mft on success, or NULL on failure.
 */
static struct mft *
proc_parser_mft_pre(struct entity *entp, char *file, struct crl **crl,
    char **crlfile, struct mft *cached_mft, const char **errstr)
{
	struct mft	*mft;
	X509		*x509;
	struct auth	*a;
	unsigned char	*der;
	size_t		 len;
	time_t		 now;
	int		 issued_cmp, seqnum_cmp;

	*crl = NULL;
	*crlfile = NULL;
	*errstr = NULL;

	if (file == NULL)
		return NULL;

	der = load_file(file, &len);
	if (der == NULL && errno != ENOENT)
		warn("parse file %s", file);

	if ((mft = mft_parse(&x509, file, entp->talid, der, len)) == NULL) {
		free(der);
		return NULL;
	}

	if (entp->path != NULL) {
		if ((mft->path = strdup(entp->path)) == NULL)
			err(1, NULL);
	}

	if (!EVP_Digest(der, len, mft->mfthash, NULL, EVP_sha256(), NULL))
		errx(1, "EVP_Digest failed");

	free(der);

	*crl = parse_load_crl_from_mft(entp, mft, DIR_TEMP, crlfile);
	if (*crl == NULL)
		*crl = parse_load_crl_from_mft(entp, mft, DIR_VALID, crlfile);

	a = find_issuer(file, entp->certid, mft->aki, NULL);
	if (!valid_x509(file, ctx, x509, a, *crl, errstr))
		goto err;
	X509_free(x509);
	x509 = NULL;

	mft->repoid = entp->repoid;
	mft->talid = a->cert->talid;
	mft->certid = entp->certid;

	now = get_current_time();
	/* check that now is not before from */
	if (now < mft->thisupdate) {
		warnx("%s: manifest not yet valid %s", file,
		    time2str(mft->thisupdate));
		goto err;
	}
	/* check that now is not after until */
	if (now > mft->nextupdate) {
		warnx("%s: manifest expired on %s", file,
		    time2str(mft->nextupdate));
		goto err;
	}

	/* if there is nothing to compare to, return now */
	if (cached_mft == NULL)
		return mft;

	/*
	 * Check that the cached manifest is older in the sense that it was
	 * issued earlier and that it has a smaller sequence number.
	 */

	if ((issued_cmp = mft_compare_issued(mft, cached_mft)) < 0) {
		warnx("%s: unexpected manifest issuance date (want >= %lld, "
		    "got %lld)", file, (long long)cached_mft->thisupdate,
		    (long long)mft->thisupdate);
		goto err;
	}
	if ((seqnum_cmp = mft_compare_seqnum(mft, cached_mft)) < 0) {
		warnx("%s: unexpected manifest number (want >= #%s, got #%s)",
		    file, cached_mft->seqnum, mft->seqnum);
		goto err;
	}
	if (issued_cmp > 0 && seqnum_cmp == 0) {
		warnx("%s: manifest issued at %lld and %lld with same "
		    "manifest number #%s", file, (long long)mft->thisupdate,
		    (long long)cached_mft->thisupdate, cached_mft->seqnum);
		goto err;
	}
	if (issued_cmp == 0 && seqnum_cmp > 0) {
		warnx("%s: #%s and #%s were issued at same issuance date %lld",
		    file, mft->seqnum, cached_mft->seqnum,
		    (long long)mft->thisupdate);
		goto err;
	}
	if (issued_cmp == 0 && seqnum_cmp == 0 && memcmp(mft->mfthash,
	    cached_mft->mfthash, SHA256_DIGEST_LENGTH) != 0) {
		warnx("%s: misissuance, issuance date %lld and manifest number "
		    "#%s were recycled", file, (long long)mft->thisupdate,
		    mft->seqnum);
		goto err;
	}

	return mft;

 err:
	X509_free(x509);
	mft_free(mft);
	crl_free(*crl);
	*crl = NULL;
	free(*crlfile);
	*crlfile = NULL;
	return NULL;
}

/*
 * Load the most recent MFT by opening both options and comparing the two.
 */
static char *
proc_parser_mft(struct entity *entp, struct mft **mp, char **crlfile,
    time_t *crlmtime)
{
	struct mft	*mft1 = NULL, *mft2 = NULL;
	struct crl	*crl, *crl1 = NULL, *crl2 = NULL;
	char		*file, *file1 = NULL, *file2 = NULL;
	char		*crl1file = NULL, *crl2file = NULL;
	const char	*err1 = NULL, *err2 = NULL;

	*mp = NULL;
	*crlmtime = 0;

	file2 = parse_filepath(entp->repoid, entp->path, entp->file, DIR_VALID);
	mft2 = proc_parser_mft_pre(entp, file2, &crl2, &crl2file, NULL, &err2);

	if (!noop) {
		file1 = parse_filepath(entp->repoid, entp->path, entp->file,
		    DIR_TEMP);
		mft1 = proc_parser_mft_pre(entp, file1, &crl1, &crl1file, mft2,
		    &err1);
	}

	if (proc_parser_mft_check(file1, mft1)) {
		mft_free(mft2);
		crl_free(crl2);
		free(crl2file);
		free(file2);

		*mp = mft1;
		crl = crl1;
		file = file1;
		*crlfile = crl1file;
	} else {
		if (mft1 != NULL && mft2 != NULL)
			warnx("%s: failed fetch, continuing with #%s "
			    "from cache", file2, mft2->seqnum);

		if (!proc_parser_mft_check(file2, mft2)) {
			mft_free(mft2);
			mft2 = NULL;

			if (err2 == NULL)
				err2 = err1;
			if (err2 == NULL)
				err2 = "no valid manifest available";
			warnx("%s: %s", file2, err2);
		}

		mft_free(mft1);
		crl_free(crl1);
		free(crl1file);
		free(file1);

		*mp = mft2;
		crl = crl2;
		file = file2;
		*crlfile = crl2file;
	}

	if (*mp != NULL) {
		*crlmtime = crl->thisupdate;
		if (crl_insert(&crlt, crl))
			crl = NULL;
	}
	crl_free(crl);

	return file;
}

/*
 * Certificates are from manifests (has a digest and is signed with
 * another certificate) Parse the certificate, make sure its
 * signatures are valid (with CRLs), then validate the RPKI content.
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_cert(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct cert	*cert;
	struct crl	*crl;
	struct auth	*a;
	const char	*errstr = NULL;

	/* Extract certificate data. */

	cert = cert_parse_pre(file, der, len);
	cert = cert_parse(file, cert);
	if (cert == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, cert->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, cert->x509, a, crl, &errstr) ||
	    !valid_cert(file, a, cert)) {
		if (errstr != NULL)
			warnx("%s: %s", file, errstr);
		cert_free(cert);
		return NULL;
	}

	cert->talid = a->cert->talid;

	if (cert->purpose == CERT_PURPOSE_BGPSEC_ROUTER) {
		if (!constraints_validate(file, cert)) {
			cert_free(cert);
			return NULL;
		}
	}

	/*
	 * Add validated CA certs to the RPKI auth tree.
	 */
	if (cert->purpose == CERT_PURPOSE_CA)
		auth_insert(file, &auths, cert, a);

	return cert;
}

/*
 * Root certificates come from TALs (has a pkey and is self-signed).
 * Parse the certificate, ensure that its public key matches the
 * known public key from the TAL, and then validate the RPKI
 * content.
 *
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_root_cert(char *file, const unsigned char *der, size_t len,
    unsigned char *pkey, size_t pkeysz, int talid)
{
	struct cert		*cert;

	/* Extract certificate data. */

	cert = cert_parse_pre(file, der, len);
	cert = ta_parse(file, cert, pkey, pkeysz);
	if (cert == NULL)
		return NULL;
	cert->talid = talid;

	/*
	 * Add valid roots to the RPKI auth tree.
	 */
	auth_insert(file, &auths, cert, NULL);

	return cert;
}

/*
 * Parse a ghostbuster record
 */
static struct gbr *
proc_parser_gbr(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct gbr	*gbr;
	X509		*x509;
	struct crl	*crl;
	struct auth	*a;
	const char	*errstr;

	if ((gbr = gbr_parse(&x509, file, entp->talid, der, len)) == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, gbr->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	/* return value can be ignored since nothing happens here */
	if (!valid_x509(file, ctx, x509, a, crl, &errstr)) {
		warnx("%s: %s", file, errstr);
		X509_free(x509);
		gbr_free(gbr);
		return NULL;
	}
	X509_free(x509);

	gbr->talid = a->cert->talid;

	return gbr;
}

/*
 * Parse an ASPA object
 */
static struct aspa *
proc_parser_aspa(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct aspa	*aspa;
	struct auth	*a;
	struct crl	*crl;
	X509		*x509;
	const char	*errstr;

	if ((aspa = aspa_parse(&x509, file, entp->talid, der, len)) == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, aspa->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, x509, a, crl, &errstr)) {
		warnx("%s: %s", file, errstr);
		X509_free(x509);
		aspa_free(aspa);
		return NULL;
	}
	X509_free(x509);

	aspa->talid = a->cert->talid;

	aspa->expires = x509_find_expires(aspa->notafter, a, &crlt);

	return aspa;
}

/*
 * Parse a TAK object.
 */
static struct tak *
proc_parser_tak(char *file, const unsigned char *der, size_t len,
    const struct entity *entp)
{
	struct tak	*tak;
	X509		*x509;
	struct crl	*crl;
	struct auth	*a;
	const char	*errstr;
	int		 rc = 0;

	if ((tak = tak_parse(&x509, file, entp->talid, der, len)) == NULL)
		return NULL;

	a = find_issuer(file, entp->certid, tak->aki, entp->mftaki);
	crl = crl_get(&crlt, a);

	if (!valid_x509(file, ctx, x509, a, crl, &errstr)) {
		warnx("%s: %s", file, errstr);
		goto out;
	}

	/* TAK EE must be signed by self-signed CA */
	if (a->issuer != NULL)
		goto out;

	tak->talid = a->cert->talid;
	rc = 1;
 out:
	if (rc == 0) {
		tak_free(tak);
		tak = NULL;
	}
	X509_free(x509);
	return tak;
}

/*
 * Load the file specified by the entity information.
 */
static char *
parse_load_file(struct entity *entp, unsigned char **f, size_t *flen)
{
	char *file;

	file = parse_filepath(entp->repoid, entp->path, entp->file,
	    entp->location);
	if (file == NULL)
		errx(1, "no path to file");

	*f = load_file(file, flen);
	if (*f == NULL)
		warn("parse file %s", file);

	return file;
}

/*
 * Process an entity and respond to parent process.
 */
static void
parse_entity(struct entityq *q, struct msgbuf *msgq)
{
	struct entity	*entp;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct aspa	*aspa;
	struct gbr	*gbr;
	struct tak	*tak;
	struct spl	*spl;
	struct ibuf	*b;
	unsigned char	*f;
	time_t		 mtime, crlmtime;
	size_t		 flen;
	char		*file, *crlfile;
	int		 c;

	while ((entp = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, entp, entries);

		/* handle RTYPE_REPO first */
		if (entp->type == RTYPE_REPO) {
			repo_add(entp->repoid, entp->path, entp->file);
			entity_free(entp);
			continue;
		}

		/* pass back at least type, repoid and filename */
		b = io_new_buffer();
		io_simple_buffer(b, &entp->type, sizeof(entp->type));
		io_simple_buffer(b, &entp->repoid, sizeof(entp->repoid));
		io_simple_buffer(b, &entp->talid, sizeof(entp->talid));

		file = NULL;
		f = NULL;
		mtime = 0;
		crlmtime = 0;

		switch (entp->type) {
		case RTYPE_TAL:
			io_str_buffer(b, entp->file);
			io_simple_buffer(b, &mtime, sizeof(mtime));
			if ((tal = tal_parse(entp->file, entp->data,
			    entp->datasz)) == NULL)
				errx(1, "%s: could not parse tal file",
				    entp->file);
			tal->id = entp->talid;
			tal_buffer(b, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			if (entp->data != NULL)
				cert = proc_parser_root_cert(file,
				    f, flen, entp->data, entp->datasz,
				    entp->talid);
			else
				cert = proc_parser_cert(file, f, flen, entp);
			if (cert != NULL)
				mtime = cert->notbefore;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			c = (cert != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (cert != NULL) {
				cert->repoid = entp->repoid;
				cert_buffer(b, cert);
			}
			/*
			 * The parsed certificate data "cert" is now
			 * managed in the "auths" table, so don't free
			 * it here.
			 */
			break;
		case RTYPE_MFT:
			file = proc_parser_mft(entp, &mft, &crlfile, &crlmtime);
			io_str_buffer(b, file);
			if (mft != NULL)
				mtime = mft->signtime;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			c = (mft != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(b, mft);

			/* Push valid CRL together with the MFT. */
			if (crlfile != NULL) {
				enum rtype type;
				struct ibuf *b2;

				b2 = io_new_buffer();
				type = RTYPE_CRL;
				io_simple_buffer(b2, &type, sizeof(type));
				io_simple_buffer(b2, &entp->repoid,
				    sizeof(entp->repoid));
				io_simple_buffer(b2, &entp->talid,
				    sizeof(entp->talid));
				io_str_buffer(b2, crlfile);
				io_simple_buffer(b2, &crlmtime,
				    sizeof(crlmtime));
				free(crlfile);

				io_close_buffer(msgq, b2);
			}
			mft_free(mft);
			break;
		case RTYPE_ROA:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			roa = proc_parser_roa(file, f, flen, entp);
			if (roa != NULL)
				mtime = roa->signtime;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			c = (roa != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (roa != NULL)
				roa_buffer(b, roa);
			roa_free(roa);
			break;
		case RTYPE_GBR:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			gbr = proc_parser_gbr(file, f, flen, entp);
			if (gbr != NULL)
				mtime = gbr->signtime;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			gbr_free(gbr);
			break;
		case RTYPE_ASPA:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			aspa = proc_parser_aspa(file, f, flen, entp);
			if (aspa != NULL)
				mtime = aspa->signtime;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			c = (aspa != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (aspa != NULL)
				aspa_buffer(b, aspa);
			aspa_free(aspa);
			break;
		case RTYPE_TAK:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			tak = proc_parser_tak(file, f, flen, entp);
			if (tak != NULL)
				mtime = tak->signtime;
			io_simple_buffer(b, &mtime, sizeof(mtime));
			tak_free(tak);
			break;
		case RTYPE_SPL:
			file = parse_load_file(entp, &f, &flen);
			io_str_buffer(b, file);
			if (experimental) {
				spl = proc_parser_spl(file, f, flen, entp);
				if (spl != NULL)
					mtime = spl->signtime;
			} else {
				if (verbose > 0)
					warnx("%s: skipped", file);
				spl = NULL;
			}
			io_simple_buffer(b, &mtime, sizeof(mtime));
			c = (spl != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (spl != NULL)
				spl_buffer(b, spl);
			spl_free(spl);
			break;
		case RTYPE_CRL:
		default:
			file = parse_filepath(entp->repoid, entp->path,
			    entp->file, entp->location);
			io_str_buffer(b, file);
			io_simple_buffer(b, &mtime, sizeof(mtime));
			warnx("%s: unhandled type %d", file, entp->type);
			break;
		}

		free(f);
		free(file);
		io_close_buffer(msgq, b);
		entity_free(entp);
	}
}

/*
 * Process responsible for parsing and validating content.
 * All this process does is wait to be told about a file to parse, then
 * it parses it and makes sure that the data being returned is fully
 * validated and verified.
 * The process will exit cleanly only when fd is closed.
 */
void
proc_parser(int fd)
{
	struct entityq	 q;
	struct msgbuf	 msgq;
	struct pollfd	 pfd;
	struct entity	*entp;
	struct ibuf	*b, *inbuf = NULL;

	/* Only allow access to the cache directory. */
	if (unveil(".", "r") == -1)
		err(1, "unveil cachedir");
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	x509_init_oid();
	constraints_parse();

	if ((ctx = X509_STORE_CTX_new()) == NULL)
		err(1, "X509_STORE_CTX_new");

	TAILQ_INIT(&q);

	msgbuf_init(&msgq);
	msgq.fd = fd;

	pfd.fd = fd;

	for (;;) {
		pfd.events = POLLIN;
		if (msgq.queued)
			pfd.events |= POLLOUT;

		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		if ((pfd.revents & (POLLERR|POLLNVAL)))
			errx(1, "poll: bad descriptor");

		/* If the parent closes, return immediately. */

		if ((pfd.revents & POLLHUP))
			break;

		if ((pfd.revents & POLLIN)) {
			b = io_buf_read(fd, &inbuf);
			if (b != NULL) {
				entp = calloc(1, sizeof(struct entity));
				if (entp == NULL)
					err(1, NULL);
				entity_read_req(b, entp);
				TAILQ_INSERT_TAIL(&q, entp, entries);
				ibuf_free(b);
			}
		}

		if (pfd.revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		parse_entity(&q, &msgq);
	}

	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	auth_tree_free(&auths);
	crl_tree_free(&crlt);

	X509_STORE_CTX_free(ctx);
	msgbuf_clear(&msgq);

	ibuf_free(inbuf);

	exit(0);
}
