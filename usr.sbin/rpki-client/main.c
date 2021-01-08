/*	$OpenBSD: main.c,v 1.90 2021/01/08 08:45:55 claudio Exp $ */
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

/*-
 * Copyright (C) 2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2012 Oleg Moskalenko <mom040267@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <fts.h>
#include <inttypes.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>
#include <unistd.h>
#include <imsg.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

/*
 * Maximum number of TAL files we'll load.
 */
#define	TALSZ_MAX	8

/*
 * An rsync repository.
 */
struct	repo {
	char	*host; /* hostname */
	char	*module; /* module name */
	int	 loaded; /* whether loaded or not */
	size_t	 id; /* identifier (array index) */
};

size_t	entity_queue;
int	timeout = 60*60;
volatile sig_atomic_t killme;
void	suicide(int sig);

/*
 * Table of all known repositories.
 */
static struct	repotab {
	struct repo	*repos; /* repositories */
	size_t		 reposz; /* number of repos */
} rt;

/*
 * An entity (MFT, ROA, certificate, etc.) that needs to be downloaded
 * and parsed.
 */
struct	entity {
	enum rtype	 type; /* type of entity (not RTYPE_EOF) */
	char		*uri; /* file or rsync:// URI */
	int		 has_dgst; /* whether dgst is specified */
	unsigned char	 dgst[SHA256_DIGEST_LENGTH]; /* optional */
	ssize_t		 repo; /* repo index or <0 if w/o repo */
	int		 has_pkey; /* whether pkey/sz is specified */
	unsigned char	*pkey; /* public key (optional) */
	size_t		 pkeysz; /* public key length (optional) */
	char		*descr; /* tal description */
	TAILQ_ENTRY(entity) entries;
};

TAILQ_HEAD(entityq, entity);

/*
 * Database of all file path accessed during a run.
 */
struct filepath {
	RB_ENTRY(filepath)	entry;
	char			*file;
};

static inline int
filepathcmp(struct filepath *a, struct filepath *b)
{
	return strcasecmp(a->file, b->file);
}

RB_HEAD(filepath_tree, filepath);
RB_PROTOTYPE(filepath_tree, filepath, entry, filepathcmp);
struct filepath_tree  fpt = RB_INITIALIZER(&fpt);

/*
 * Mark that our subprocesses will never return.
 */
static void	entityq_flush(struct msgbuf *, struct entityq *,
		    const struct repo *);
static void	proc_parser(int) __attribute__((noreturn));
static void	build_chain(const struct auth *, STACK_OF(X509) **);
static void	build_crls(const struct auth *, struct crl_tree *,
		    STACK_OF(X509_CRL) **);

const char	*bird_tablename = "ROAS";

int	verbose;
int	noop;

struct stats	 stats;

/*
 * Log a message to stderr if and only if "verbose" is non-zero.
 * This uses the err(3) functionality.
 */
void
logx(const char *fmt, ...)
{
	va_list		 ap;

	if (verbose && fmt != NULL) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
}

/*
 * Functions to lookup which files have been accessed during computation.
 */
static void
filepath_add(char *file)
{
	struct filepath *fp;

	if ((fp = malloc(sizeof(*fp))) == NULL)
		err(1, NULL);
	if ((fp->file = strdup(file)) == NULL)
		err(1, NULL);

	if (RB_INSERT(filepath_tree, &fpt, fp) != NULL) {
		/* already in the tree */
		free(fp->file);
		free(fp);
	}
}

static int
filepath_exists(char *file)
{
	struct filepath needle;

	needle.file = file;
	return RB_FIND(filepath_tree, &fpt, &needle) != NULL;
}

RB_GENERATE(filepath_tree, filepath, entry, filepathcmp);

static void
entity_free(struct entity *ent)
{

	if (ent == NULL)
		return;

	free(ent->pkey);
	free(ent->uri);
	free(ent->descr);
	free(ent);
}

/*
 * Read a queue entity from the descriptor.
 * Matched by entity_buffer_req().
 * The pointer must be passed entity_free().
 */
static void
entity_read_req(int fd, struct entity *ent)
{

	io_simple_read(fd, &ent->type, sizeof(enum rtype));
	io_str_read(fd, &ent->uri);
	io_simple_read(fd, &ent->has_dgst, sizeof(int));
	if (ent->has_dgst)
		io_simple_read(fd, ent->dgst, sizeof(ent->dgst));
	io_simple_read(fd, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_read_alloc(fd, (void **)&ent->pkey, &ent->pkeysz);
	io_str_read(fd, &ent->descr);
}

/*
 * Like entity_write_req() but into a buffer.
 * Matched by entity_read_req().
 */
static void
entity_buffer_req(struct ibuf *b, const struct entity *ent)
{

	io_simple_buffer(b, &ent->type, sizeof(ent->type));
	io_str_buffer(b, ent->uri);
	io_simple_buffer(b, &ent->has_dgst, sizeof(int));
	if (ent->has_dgst)
		io_simple_buffer(b, ent->dgst, sizeof(ent->dgst));
	io_simple_buffer(b, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_buffer(b, ent->pkey, ent->pkeysz);
	io_str_buffer(b, ent->descr);
}

/*
 * Write the queue entity.
 * Simply a wrapper around entity_buffer_req().
 */
static void
entity_write_req(struct msgbuf *msgq, const struct entity *ent)
{
	struct ibuf *b;

	if ((b = ibuf_dynamic(sizeof(*ent), UINT_MAX)) == NULL)
		err(1, NULL);
	entity_buffer_req(b, ent);
	ibuf_close(msgq, b);
}

/*
 * Scan through all queued requests and see which ones are in the given
 * repo, then flush those into the parser process.
 */
static void
entityq_flush(struct msgbuf *msgq, struct entityq *q, const struct repo *repo)
{
	struct entity	*p, *np;

	TAILQ_FOREACH_SAFE(p, q, entries, np) {
		if (p->repo < 0 || repo->id != (size_t)p->repo)
			continue;
		entity_write_req(msgq, p);
		TAILQ_REMOVE(q, p, entries);
		entity_free(p);
	}
}

/*
 * Look up a repository, queueing it for discovery if not found.
 */
static const struct repo *
repo_lookup(struct msgbuf *msgq, const char *uri)
{
	const char	*host, *mod;
	size_t		 hostsz, modsz, i;
	struct repo	*rp;
	struct ibuf	*b;

	if (!rsync_uri_parse(&host, &hostsz,
	    &mod, &modsz, NULL, NULL, NULL, uri))
		errx(1, "%s: malformed", uri);

	/* Look up in repository table. */

	for (i = 0; i < rt.reposz; i++) {
		if (strlen(rt.repos[i].host) != hostsz)
			continue;
		if (strlen(rt.repos[i].module) != modsz)
			continue;
		if (strncasecmp(rt.repos[i].host, host, hostsz))
			continue;
		if (strncasecmp(rt.repos[i].module, mod, modsz))
			continue;
		return &rt.repos[i];
	}

	rt.repos = reallocarray(rt.repos,
		rt.reposz + 1, sizeof(struct repo));
	if (rt.repos == NULL)
		err(1, "reallocarray");

	rp = &rt.repos[rt.reposz++];
	memset(rp, 0, sizeof(struct repo));
	rp->id = rt.reposz - 1;

	if ((rp->host = strndup(host, hostsz)) == NULL ||
	    (rp->module = strndup(mod, modsz)) == NULL)
		err(1, "strndup");

	i = rt.reposz - 1;

	if (!noop) {
		logx("%s/%s: pulling from network", rp->host, rp->module);
		if ((b = ibuf_dynamic(128, UINT_MAX)) == NULL)
			err(1, NULL);
		io_simple_buffer(b, &i, sizeof(i));
		io_str_buffer(b, rp->host);
		io_str_buffer(b, rp->module);

		ibuf_close(msgq, b);
	} else {
		rp->loaded = 1;
		logx("%s/%s: using cache", rp->host, rp->module);
		stats.repos++;
		/* there is nothing in the queue so no need to flush */
	}
	return rp;
}

/*
 * Build local file name base on the URI and the repo info.
 */
static char *
repo_filename(const struct repo *repo, const char *uri)
{
	char *nfile;

	uri += 8 + strlen(repo->host) + 1 + strlen(repo->module) + 1;

	if (asprintf(&nfile, "%s/%s/%s", repo->host, repo->module, uri) == -1)
		err(1, "asprintf");
	return nfile;
}

/*
 * Add the heap-allocated file to the queue for processing.
 */
static void
entityq_add(struct msgbuf *msgq, struct entityq *q, char *file, enum rtype type,
    const struct repo *rp, const unsigned char *dgst,
    const unsigned char *pkey, size_t pkeysz, char *descr)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(1, "calloc");

	p->type = type;
	p->uri = file;
	p->repo = (rp != NULL) ? (ssize_t)rp->id : -1;
	p->has_dgst = dgst != NULL;
	p->has_pkey = pkey != NULL;
	if (p->has_dgst)
		memcpy(p->dgst, dgst, sizeof(p->dgst));
	if (p->has_pkey) {
		p->pkeysz = pkeysz;
		if ((p->pkey = malloc(pkeysz)) == NULL)
			err(1, "malloc");
		memcpy(p->pkey, pkey, pkeysz);
	}
	if (descr != NULL)
		if ((p->descr = strdup(descr)) == NULL)
			err(1, "strdup");

	filepath_add(file);

	entity_queue++;

	/*
	 * Write to the queue if there's no repo or the repo has already
	 * been loaded else enqueue it for later.
	 */

	if (rp == NULL || rp->loaded) {
		entity_write_req(msgq, p);
		entity_free(p);
	} else
		TAILQ_INSERT_TAIL(q, p, entries);
}

/*
 * Add a file (CER, ROA, CRL) from an MFT file, RFC 6486.
 * These are always relative to the directory in which "mft" sits.
 */
static void
queue_add_from_mft(struct msgbuf *msgq, struct entityq *q, const char *mft,
    const struct mftfile *file, enum rtype type)
{
	char		*cp, *nfile;

	/* Construct local path from filename. */
	/* We know this is host/module/... */

	cp = strrchr(mft, '/');
	assert(cp != NULL);
	assert(cp - mft < INT_MAX);
	if (asprintf(&nfile, "%.*s/%s", (int)(cp - mft), mft, file->file) == -1)
		err(1, "asprintf");

	/*
	 * Since we're from the same directory as the MFT file, we know
	 * that the repository has already been loaded.
	 */

	entityq_add(msgq, q, nfile, type, NULL, file->hash, NULL, 0, NULL);
}

/*
 * Loops over queue_add_from_mft() for all files.
 * The order here is important: we want to parse the revocation
 * list *before* we parse anything else.
 * FIXME: set the type of file in the mftfile so that we don't need to
 * keep doing the check (this should be done in the parser, where we
 * check the suffix anyway).
 */
static void
queue_add_from_mft_set(struct msgbuf *msgq, struct entityq *q,
    const struct mft *mft)
{
	size_t			 i, sz;
	const struct mftfile	*f;

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_CRL);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".cer"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_CER);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".roa"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_ROA);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".gbr"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_GBR);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl") == 0 ||
		    strcasecmp(f->file + sz - 4, ".cer") == 0 ||
		    strcasecmp(f->file + sz - 4, ".roa") == 0 ||
		    strcasecmp(f->file + sz - 4, ".gbr") == 0)
			continue;
		logx("%s: unsupported file type: %s", mft->file, f->file);
	}
}

/*
 * Add a local TAL file (RFC 7730) to the queue of files to fetch.
 */
static void
queue_add_tal(struct msgbuf *msgq, struct entityq *q, const char *file)
{
	char	*nfile, *buf;

	if ((nfile = strdup(file)) == NULL)
		err(1, "strdup");
	buf = tal_read_file(file);

	/* Record tal for later reporting */
	if (stats.talnames == NULL)
		stats.talnames = strdup(file);
	else {
		char *tmp;
		if (asprintf(&tmp, "%s %s", stats.talnames, file) == -1)
			err(1, "asprintf");
		free(stats.talnames);
		stats.talnames = tmp;
	}

	/* Not in a repository, so directly add to queue. */
	entityq_add(msgq, q, nfile, RTYPE_TAL, NULL, NULL, NULL, 0, buf);
	/* entityq_add makes a copy of buf */
	free(buf);
}

/*
 * Add URIs (CER) from a TAL file, RFC 8630.
 */
static void
queue_add_from_tal(struct msgbuf *procq, struct msgbuf *rsyncq,
    struct entityq *q, const struct tal *tal)
{
	char			*nfile;
	const struct repo	*repo;
	const char		*uri = NULL;
	size_t			 i;

	assert(tal->urisz);

	for (i = 0; i < tal->urisz; i++) {
		uri = tal->uri[i];
		if (strncasecmp(uri, "rsync://", 8) == 0)
			break;
	}
	if (uri == NULL)
		errx(1, "TAL file has no rsync:// URI");

	/* Look up the repository. */
	repo = repo_lookup(rsyncq, uri);
	nfile = repo_filename(repo, uri);

	entityq_add(procq, q, nfile, RTYPE_CER, repo, NULL, tal->pkey,
	    tal->pkeysz, tal->descr);
}

/*
 * Add a manifest (MFT) found in an X509 certificate, RFC 6487.
 */
static void
queue_add_from_cert(struct msgbuf *procq, struct msgbuf *rsyncq,
    struct entityq *q, const char *rsyncuri, const char *rrdpuri)
{
	char			*nfile;
	const struct repo	*repo;

	if (rsyncuri == NULL)
		return;

	/* Look up the repository. */
	repo = repo_lookup(rsyncq, rsyncuri);
	nfile = repo_filename(repo, rsyncuri);

	entityq_add(procq, q, nfile, RTYPE_MFT, repo, NULL, NULL, 0, NULL);
}

/*
 * Parse and validate a ROA.
 * This is standard stuff.
 * Returns the roa on success, NULL on failure.
 */
static struct roa *
proc_parser_roa(struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	struct roa		*roa;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	assert(entp->has_dgst);
	if ((roa = roa_parse(&x509, entp->uri, entp->dgst)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->uri, auths, roa->ski, roa->aki);

	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		if (verbose > 0 || c != X509_V_ERR_UNABLE_TO_GET_CRL)
			warnx("%s: %s", entp->uri,
			    X509_verify_cert_error_string(c));
		X509_free(x509);
		roa_free(roa);
		sk_X509_free(chain);
		sk_X509_CRL_free(crls);
		return NULL;
	}
	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);

	/*
	 * If the ROA isn't valid, we accept it anyway and depend upon
	 * the code around roa_read() to check the "valid" field itself.
	 */

	if (valid_roa(entp->uri, auths, roa))
		roa->valid = 1;

	return roa;
}

/*
 * Parse and validate a manifest file.
 * Here we *don't* validate against the list of CRLs, because the
 * certificate used to sign the manifest may specify a CRL that the root
 * certificate didn't, and we haven't scanned for it yet.
 * This chicken-and-egg isn't important, however, because we'll catch
 * the revocation list by the time we scan for any contained resources
 * (ROA, CER) and will see it then.
 * Return the mft on success or NULL on failure.
 */
static struct mft *
proc_parser_mft(struct entity *entp, X509_STORE *store, X509_STORE_CTX *ctx,
	struct auth_tree *auths, struct crl_tree *crlt)
{
	struct mft		*mft;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;

	assert(!entp->has_dgst);
	if ((mft = mft_parse(&x509, entp->uri)) == NULL)
		return NULL;

	a = valid_ski_aki(entp->uri, auths, mft->ski, mft->aki);
	build_chain(a, &chain);

	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");

	/* CRL checked disabled here because CRL is referenced from mft */
	X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_IGNORE_CRITICAL);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		warnx("%s: %s", entp->uri, X509_verify_cert_error_string(c));
		mft_free(mft);
		X509_free(x509);
		sk_X509_free(chain);
		return NULL;
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	X509_free(x509);

	if (!mft_check(entp->uri, mft)) {
		mft_free(mft);
		return NULL;
	}

	return mft;
}

/*
 * Certificates are from manifests (has a digest and is signed with
 * another certificate) Parse the certificate, make sure its
 * signatures are valid (with CRLs), then validate the RPKI content.
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_cert(const struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	struct cert		*cert;
	X509			*x509;
	int			 c;
	struct auth		*a = NULL, *na;
	char			*tal;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	assert(entp->has_dgst);
	assert(!entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = cert_parse(&x509, entp->uri, entp->dgst);
	if (cert == NULL)
		return NULL;

	a = valid_ski_aki(entp->uri, auths, cert->ski, cert->aki);
	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	/*
	 * Validate certificate chain w/CRLs.
	 * Only check the CRLs if specifically asked.
	 */

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");

	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		warnx("%s: %s", entp->uri,
		    X509_verify_cert_error_string(c));
		X509_STORE_CTX_cleanup(ctx);
		cert_free(cert);
		sk_X509_free(chain);
		sk_X509_CRL_free(crls);
		X509_free(x509);
		return NULL;
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);

	/* Validate the cert to get the parent */
	if (!valid_cert(entp->uri, auths, cert)) {
		X509_free(x509); // needed? XXX
		return cert;
	}

	/*
	 * Add validated certs to the RPKI auth tree.
	 */

	cert->valid = 1;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	tal = a->tal;

	na->parent = a;
	na->cert = cert;
	na->tal = tal;
	na->fn = strdup(entp->uri);
	if (na->fn == NULL)
		err(1, NULL);

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	return cert;
}


/*
 * Root certificates come from TALs (has a pkey and is self-signed).
 * Parse the certificate, ensure that it's public key matches the
 * known public key from the TAL, and then validate the RPKI
 * content. If valid, we add it as a trusted root (trust anchor) to
 * "store".
 *
 * This returns a certificate (which must not be freed) or NULL on
 * parse failure.
 */
static struct cert *
proc_parser_root_cert(const struct entity *entp,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth_tree *auths, struct crl_tree *crlt)
{
	char			subject[256];
	ASN1_TIME		*notBefore, *notAfter;
	X509_NAME		*name;
	struct cert		*cert;
	X509			*x509;
	struct auth		*na;
	char			*tal;

	assert(!entp->has_dgst);
	assert(entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = ta_parse(&x509, entp->uri, entp->pkey, entp->pkeysz);
	if (cert == NULL)
		return NULL;

	if ((name = X509_get_subject_name(x509)) == NULL) {
		warnx("%s Unable to get certificate subject", entp->uri);
		goto badcert;
	}
	if (X509_NAME_oneline(name, subject, sizeof(subject)) == NULL) {
		warnx("%s: Unable to parse certificate subject name",
		    entp->uri);
		goto badcert;
	}
	if ((notBefore = X509_get_notBefore(x509)) == NULL) {
		warnx("%s: certificate has invalid notBefore, subject='%s'",
		    entp->uri, subject);
		goto badcert;
	}
	if ((notAfter = X509_get_notAfter(x509)) == NULL) {
		warnx("%s: certificate has invalid notAfter, subject='%s'",
		    entp->uri, subject);
		goto badcert;
	}
	if (X509_cmp_current_time(notBefore) != -1) {
		warnx("%s: certificate not yet valid, subject='%s'", entp->uri,
		    subject);
		goto badcert;
	}
	if (X509_cmp_current_time(notAfter) != 1)  {
		warnx("%s: certificate has expired, subject='%s'", entp->uri,
		    subject);
		goto badcert;
	}
	if (!valid_ta(entp->uri, auths, cert)) {
		warnx("%s: certificate not a valid ta, subject='%s'",
		    entp->uri, subject);
		goto badcert;
	}

	/*
	 * Add valid roots to the RPKI auth tree and as a trusted root
	 * for chain validation to the X509_STORE.
	 */

	cert->valid = 1;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	if ((tal = strdup(entp->descr)) == NULL)
		err(1, NULL);

	na->parent = NULL;
	na->cert = cert;
	na->tal = tal;
	na->fn = strdup(entp->uri);
	if (na->fn == NULL)
		err(1, NULL);

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	X509_STORE_add_cert(store, x509);

	return cert;
 badcert:
	X509_free(x509); // needed? XXX
	return cert;
}

/*
 * Parse a certificate revocation list
 * This simply parses the CRL content itself, optionally validating it
 * within the digest if it comes from a manifest, then adds it to the
 * store of CRLs.
 */
static void
proc_parser_crl(struct entity *entp, X509_STORE *store,
    X509_STORE_CTX *ctx, struct crl_tree *crlt)
{
	X509_CRL		*x509_crl;
	struct crl		*crl;
	const unsigned char	*dgst;

	dgst = entp->has_dgst ? entp->dgst : NULL;
	if ((x509_crl = crl_parse(entp->uri, dgst)) != NULL) {
		if ((crl = malloc(sizeof(*crl))) == NULL)
			err(1, NULL);
		if ((crl->aki = x509_crl_get_aki(x509_crl)) == NULL)
			errx(1, "x509_crl_get_aki failed");
		crl->x509_crl = x509_crl;

		if (RB_INSERT(crl_tree, crlt, crl) != NULL) {
			warnx("%s: duplicate AKI %s", entp->uri, crl->aki);
			free_crl(crl);
		}
	}
}

/*
 * Parse a ghostbuster record
 */
static void
proc_parser_gbr(struct entity *entp, X509_STORE *store,
    X509_STORE_CTX *ctx, struct auth_tree *auths, struct crl_tree *crlt)
{
	struct gbr		*gbr;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;
	STACK_OF(X509_CRL)	*crls;

	if ((gbr = gbr_parse(&x509, entp->uri)) == NULL)
		return;

	a = valid_ski_aki(entp->uri, auths, gbr->ski, gbr->aki);

	build_chain(a, &chain);
	build_crls(a, crlt, &crls);

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, chain))
		cryptoerrx("X509_STORE_CTX_init");
	X509_STORE_CTX_set_flags(ctx,
	    X509_V_FLAG_IGNORE_CRITICAL | X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set0_crls(ctx, crls);

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		if (verbose > 0 || c != X509_V_ERR_UNABLE_TO_GET_CRL)
			warnx("%s: %s", entp->uri,
			    X509_verify_cert_error_string(c));
	}

	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);
	X509_free(x509);
	gbr_free(gbr);
}

/* use the parent (id) to walk the tree to the root and
   build a certificate chain from cert->x509 */
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

/* use the parent (id) to walk the tree to the root and
   build a stack of CRLs */
static void
build_crls(const struct auth *a, struct crl_tree *crlt,
    STACK_OF(X509_CRL) **crls)
{
	struct crl	find, *found;

	if ((*crls = sk_X509_CRL_new_null()) == NULL)
		errx(1, "sk_X509_CRL_new_null");

	if (a == NULL)
		return;

	find.aki = a->cert->ski;
	found = RB_FIND(crl_tree, crlt, &find);
	if (found && !sk_X509_CRL_push(*crls, found->x509_crl))
		err(1, "sk_X509_CRL_push");
}

/*
 * Process responsible for parsing and validating content.
 * All this process does is wait to be told about a file to parse, then
 * it parses it and makes sure that the data being returned is fully
 * validated and verified.
 * The process will exit cleanly only when fd is closed.
 */
static void
proc_parser(int fd)
{
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct entity	*entp;
	struct entityq	 q;
	int		 c, rc = 1;
	struct msgbuf	 msgq;
	struct pollfd	 pfd;
	struct ibuf	*b;
	X509_STORE	*store;
	X509_STORE_CTX	*ctx;
	struct auth_tree auths = RB_INITIALIZER(&auths);
	struct crl_tree	 crlt = RB_INITIALIZER(&crlt);

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	if ((store = X509_STORE_new()) == NULL)
		cryptoerrx("X509_STORE_new");
	if ((ctx = X509_STORE_CTX_new()) == NULL)
		cryptoerrx("X509_STORE_CTX_new");

	TAILQ_INIT(&q);

	msgbuf_init(&msgq);
	msgq.fd = fd;

	pfd.fd = fd;

	io_socket_nonblocking(pfd.fd);

	for (;;) {
		pfd.events = POLLIN;
		if (msgq.queued)
			pfd.events |= POLLOUT;

		if (poll(&pfd, 1, INFTIM) == -1)
			err(1, "poll");
		if ((pfd.revents & (POLLERR|POLLNVAL)))
			errx(1, "poll: bad descriptor");

		/* If the parent closes, return immediately. */

		if ((pfd.revents & POLLHUP))
			break;

		/*
		 * Start with read events.
		 * This means that the parent process is sending us
		 * something we need to parse.
		 * We don't actually parse it til we have space in our
		 * outgoing buffer for responding, though.
		 */

		if ((pfd.revents & POLLIN)) {
			io_socket_blocking(fd);
			entp = calloc(1, sizeof(struct entity));
			if (entp == NULL)
				err(1, NULL);
			entity_read_req(fd, entp);
			TAILQ_INSERT_TAIL(&q, entp, entries);
			io_socket_nonblocking(fd);
		}

		if (pfd.revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		/*
		 * If there's nothing to parse, then stop waiting for
		 * the write signal.
		 */

		if (TAILQ_EMPTY(&q)) {
			pfd.events &= ~POLLOUT;
			continue;
		}

		entp = TAILQ_FIRST(&q);
		assert(entp != NULL);

		if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
			err(1, NULL);
		io_simple_buffer(b, &entp->type, sizeof(entp->type));

		switch (entp->type) {
		case RTYPE_TAL:
			assert(!entp->has_dgst);
			if ((tal = tal_parse(entp->uri, entp->descr)) == NULL)
				goto out;
			tal_buffer(b, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			if (entp->has_dgst)
				cert = proc_parser_cert(entp, store, ctx,
				    &auths, &crlt);
			else
				cert = proc_parser_root_cert(entp, store, ctx,
				    &auths, &crlt);
			c = (cert != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (cert != NULL)
				cert_buffer(b, cert);
			/*
			 * The parsed certificate data "cert" is now
			 * managed in the "auths" table, so don't free
			 * it here (see the loop after "out").
			 */
			break;
		case RTYPE_MFT:
			mft = proc_parser_mft(entp, store, ctx, &auths, &crlt);
			c = (mft != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(b, mft);
			mft_free(mft);
			break;
		case RTYPE_CRL:
			proc_parser_crl(entp, store, ctx, &crlt);
			break;
		case RTYPE_ROA:
			assert(entp->has_dgst);
			roa = proc_parser_roa(entp, store, ctx, &auths, &crlt);
			c = (roa != NULL);
			io_simple_buffer(b, &c, sizeof(int));
			if (roa != NULL)
				roa_buffer(b, roa);
			roa_free(roa);
			break;
		case RTYPE_GBR:
			proc_parser_gbr(entp, store, ctx, &auths, &crlt);
			break;
		default:
			abort();
		}

		ibuf_close(&msgq, b);
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	rc = 0;
out:
	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	/* XXX free auths and crl tree */

	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);

	msgbuf_clear(&msgq);

	exit(rc);
}

/*
 * Process parsed content.
 * For non-ROAs, we grok for more data.
 * For ROAs, we want to extract the valid info.
 * In all cases, we gather statistics.
 */
static void
entity_process(int proc, struct msgbuf *procq, struct msgbuf *rsyncq,
    struct stats *st, struct entityq *q, struct vrp_tree *tree)
{
	enum rtype	type;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	int		 c;

	/*
	 * For most of these, we first read whether there's any content
	 * at all---this means that the syntactic parse failed (X509
	 * certificate, for example).
	 * We follow that up with whether the resources didn't parse.
	 */
	io_simple_read(proc, &type, sizeof(type));

	switch (type) {
	case RTYPE_TAL:
		st->tals++;
		tal = tal_read(proc);
		queue_add_from_tal(procq, rsyncq, q, tal);
		tal_free(tal);
		break;
	case RTYPE_CER:
		st->certs++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->certs_fail++;
			break;
		}
		cert = cert_read(proc);
		if (cert->valid) {
			/*
			 * Process the revocation list from the
			 * certificate *first*, since it might mark that
			 * we're revoked and then we don't want to
			 * process the MFT.
			 */
			queue_add_from_cert(procq, rsyncq,
			    q, cert->mft, cert->notify);
		} else
			st->certs_invalid++;
		cert_free(cert);
		break;
	case RTYPE_MFT:
		st->mfts++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->mfts_fail++;
			break;
		}
		mft = mft_read(proc);
		if (mft->stale)
			st->mfts_stale++;
		queue_add_from_mft_set(procq, q, mft);
		mft_free(mft);
		break;
	case RTYPE_CRL:
		st->crls++;
		break;
	case RTYPE_ROA:
		st->roas++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->roas_fail++;
			break;
		}
		roa = roa_read(proc);
		if (roa->valid)
			roa_insert_vrps(tree, roa, &st->vrps, &st->uniqs);
		else
			st->roas_invalid++;
		roa_free(roa);
		break;
	case RTYPE_GBR:
		st->gbrs++;
		break;
	default:
		abort();
	}

	entity_queue--;
}

/*
 * Assign filenames ending in ".tal" in "/etc/rpki" into "tals",
 * returning the number of files found and filled-in.
 * This may be zero.
 * Don't exceded "max" filenames.
 */
static size_t
tal_load_default(const char *tals[], size_t max)
{
	static const char *confdir = "/etc/rpki";
	size_t s = 0;
	char *path;
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(confdir);
	if (dirp == NULL)
		err(1, "open %s", confdir);
	while ((dp = readdir(dirp)) != NULL) {
		if (fnmatch("*.tal", dp->d_name, FNM_PERIOD) == FNM_NOMATCH)
			continue;
		if (s >= max)
			err(1, "too many tal files found in %s",
			    confdir);
		if (asprintf(&path, "%s/%s", confdir, dp->d_name) == -1)
			err(1, "asprintf");
		tals[s++] = path;
	}
	closedir (dirp);
	return (s);
}

static char **
add_to_del(char **del, size_t *dsz, char *file)
{
	size_t i = *dsz;

	del = reallocarray(del, i + 1, sizeof(*del));
	if (del == NULL)
		err(1, "reallocarray");
	del[i] = strdup(file);
	if (del[i] == NULL)
		err(1, "strdup");
	*dsz = i + 1;
	return del;
}

static size_t
repo_cleanup(const char *cachedir)
{
	size_t i, delsz = 0;
	char *argv[2], **del = NULL;
	FTS *fts;
	FTSENT *e;

	/* change working directory to the cache directory */
	if (chdir(cachedir) == -1)
		err(1, "%s: chdir", cachedir);

	for (i = 0; i < rt.reposz; i++) {
		if (asprintf(&argv[0], "%s/%s", rt.repos[i].host,
		    rt.repos[i].module) == -1)
			err(1, NULL);
		argv[1] = NULL;
		if ((fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT,
		    NULL)) == NULL)
			err(1, "fts_open");
		errno = 0;
		while ((e = fts_read(fts)) != NULL) {
			switch (e->fts_info) {
			case FTS_NSOK:
				if (!filepath_exists(e->fts_path))
					del = add_to_del(del, &delsz,
					    e->fts_path);
				break;
			case FTS_D:
			case FTS_DP:
				/* TODO empty directory pruning */
				break;
			case FTS_SL:
			case FTS_SLNONE:
				warnx("symlink %s", e->fts_path);
				del = add_to_del(del, &delsz, e->fts_path);
				break;
			case FTS_NS:
			case FTS_ERR:
				warnx("fts_read %s: %s", e->fts_path,
				    strerror(e->fts_errno));
				break;
			default:
				warnx("unhandled[%x] %s", e->fts_info,
				    e->fts_path);
				break;
			}

			errno = 0;
		}
		if (errno)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close");
	}

	for (i = 0; i < delsz; i++) {
		if (unlink(del[i]) == -1)
			warn("unlink %s", del[i]);
		if (verbose > 1)
			logx("deleted %s", del[i]);
		free(del[i]);
	}
	free(del);

	return delsz;
}

void
suicide(int sig __attribute__((unused)))
{
	killme = 1;

}

int
main(int argc, char *argv[])
{
	int		 rc = 1, c, proc, st, rsync,
			 fl = SOCK_STREAM | SOCK_CLOEXEC;
	size_t		 i, j, outsz = 0, talsz = 0;
	pid_t		 procpid, rsyncpid;
	int		 fd[2];
	struct entityq	 q;
	struct msgbuf	 procq, rsyncq;
	struct pollfd	 pfd[2];
	struct roa	**out = NULL;
	char		*rsync_prog = "openrsync";
	char		*bind_addr = NULL;
	const char	*cachedir = NULL, *errs;
	const char	*tals[TALSZ_MAX];
	struct vrp_tree	 v = RB_INITIALIZER(&v);
	struct rusage	ru;
	struct timeval	start_time, now_time;

	gettimeofday(&start_time, NULL);

	/* If started as root, priv-drop to _rpki-client */
	if (getuid() == 0) {
		struct passwd *pw;

		pw = getpwnam("_rpki-client");
		if (!pw)
			errx(1, "no _rpki-client user to revoke to");
		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			err(1, "unable to revoke privs");

	}
	cachedir = RPKI_PATH_BASE_DIR;
	outputdir = RPKI_PATH_OUT_DIR;

	if (pledge("stdio rpath wpath cpath fattr proc exec unveil", NULL) == -1)
		err(1, "pledge");

	while ((c = getopt(argc, argv, "b:Bcd:e:jnos:t:T:v")) != -1)
		switch (c) {
		case 'b':
			bind_addr = optarg;
			break;
		case 'B':
			outformats |= FORMAT_BIRD;
			break;
		case 'c':
			outformats |= FORMAT_CSV;
			break;
		case 'd':
			cachedir = optarg;
			break;
		case 'e':
			rsync_prog = optarg;
			break;
		case 'j':
			outformats |= FORMAT_JSON;
			break;
		case 'n':
			noop = 1;
			break;
		case 'o':
			outformats |= FORMAT_OPENBGPD;
			break;
		case 's':
			timeout = strtonum(optarg, 0, 24*60*60, &errs);
			if (errs)
				errx(1, "-s: %s", errs);
			break;
		case 't':
			if (talsz >= TALSZ_MAX)
				err(1,
				    "too many tal files specified");
			tals[talsz++] = optarg;
			break;
		case 'T':
			bird_tablename = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			goto usage;
		}

	argv += optind;
	argc -= optind;
	if (argc == 1)
		outputdir = argv[0];
	else if (argc > 1)
		goto usage;

	if (timeout) {
		signal(SIGALRM, suicide);
		/* Commit suicide eventually - cron will normally start a new one */
		alarm(timeout);
	}

	if (cachedir == NULL) {
		warnx("cache directory required");
		goto usage;
	}
	if (outputdir == NULL) {
		warnx("output directory required");
		goto usage;
	}

	if (outformats == 0)
		outformats = FORMAT_OPENBGPD;

	if (talsz == 0)
		talsz = tal_load_default(tals, TALSZ_MAX);
	if (talsz == 0)
		err(1, "no TAL files found in %s", "/etc/rpki");

	TAILQ_INIT(&q);

	/*
	 * Create the file reader as a jailed child process.
	 * It will be responsible for reading all of the files (ROAs,
	 * manifests, certificates, etc.) and returning contents.
	 */

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(1, "socketpair");
	if ((procpid = fork()) == -1)
		err(1, "fork");

	if (procpid == 0) {
		close(fd[1]);

		/* change working directory to the cache directory */
		if (chdir(cachedir) == -1)
			err(1, "%s: chdir", cachedir);

		/* Only allow access to the cache directory. */
		if (unveil(cachedir, "r") == -1)
			err(1, "%s: unveil", cachedir);
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
		proc_parser(fd[0]);
		/* NOTREACHED */
	}

	close(fd[0]);
	proc = fd[1];

	/*
	 * Create a process that will do the rsync'ing.
	 * This process is responsible for making sure that all the
	 * repositories referenced by a certificate manifest (or the
	 * TAL) exists and has been downloaded.
	 */

	if (!noop) {
		if (socketpair(AF_UNIX, fl, 0, fd) == -1)
			err(1, "socketpair");
		if ((rsyncpid = fork()) == -1)
			err(1, "fork");

		if (rsyncpid == 0) {
			close(proc);
			close(fd[1]);

			/* change working directory to the cache directory */
			if (chdir(cachedir) == -1)
				err(1, "%s: chdir", cachedir);

			if (pledge("stdio rpath cpath proc exec unveil", NULL)
			    == -1)
				err(1, "pledge");

			proc_rsync(rsync_prog, bind_addr, fd[0]);
			/* NOTREACHED */
		}

		close(fd[0]);
		rsync = fd[1];
	} else
		rsync = -1;

	assert(rsync != proc);

	if (pledge("stdio rpath wpath cpath fattr", NULL) == -1)
		err(1, "pledge");

	msgbuf_init(&procq);
	msgbuf_init(&rsyncq);
	procq.fd = proc;
	rsyncq.fd = rsync;

	/*
	 * The main process drives the top-down scan to leaf ROAs using
	 * data downloaded by the rsync process and parsed by the
	 * parsing process.
	 */

	pfd[0].fd = rsync;
	pfd[1].fd = proc;

	/*
	 * Prime the process with our TAL file.
	 * This will contain (hopefully) links to our manifest and we
	 * can get the ball rolling.
	 */

	for (i = 0; i < talsz; i++)
		queue_add_tal(&procq, &q, tals[i]);

	while (entity_queue > 0 && !killme) {
		pfd[0].events = POLLIN;
		if (rsyncq.queued)
			pfd[0].events = POLLOUT;
		pfd[1].events = POLLIN;
		if (procq.queued)
			pfd[1].events = POLLOUT;

		if ((c = poll(pfd, 2, verbose ? 10000 : INFTIM)) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}

		/* Debugging: print some statistics if we stall. */

		if (c == 0) {
			for (i = j = 0; i < rt.reposz; i++)
				if (!rt.repos[i].loaded)
					j++;
			logx("period stats: %zu pending repos", j);
			logx("period stats: %zu pending entries", entity_queue);
			continue;
		}

		if ((pfd[0].revents & (POLLERR|POLLNVAL)) ||
		    (pfd[1].revents & (POLLERR|POLLNVAL)))
			errx(1, "poll: bad fd");
		if ((pfd[0].revents & POLLHUP) ||
		    (pfd[1].revents & POLLHUP))
			errx(1, "poll: hangup");

		if (pfd[0].revents & POLLOUT) {
			switch (msgbuf_write(&rsyncq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}
		if (pfd[1].revents & POLLOUT) {
			switch (msgbuf_write(&procq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		/*
		 * Check the rsync process.
		 * This means that one of our modules has completed
		 * downloading and we can flush the module requests into
		 * the parser process.
		 */

		if ((pfd[0].revents & POLLIN)) {
			int ok;
			io_simple_read(rsync, &i, sizeof(size_t));
			io_simple_read(rsync, &ok, sizeof(ok));
			assert(i < rt.reposz);
			assert(!rt.repos[i].loaded);
			rt.repos[i].loaded = 1;
			if (ok)
				logx("%s/%s: loaded from network",
				    rt.repos[i].host, rt.repos[i].module);
			else
				logx("%s/%s: load from network failed, "
				    "fallback to cache",
				    rt.repos[i].host, rt.repos[i].module);
			stats.repos++;
			entityq_flush(&procq, &q, &rt.repos[i]);
		}

		/*
		 * The parser has finished something for us.
		 * Dequeue these one by one.
		 */

		if ((pfd[1].revents & POLLIN)) {
			entity_process(proc, &procq, &rsyncq, &stats, &q, &v);
		}
	}

	if (killme) {
		syslog(LOG_CRIT|LOG_DAEMON,
		    "excessive runtime (%d seconds), giving up", timeout);
		errx(1, "excessive runtime (%d seconds), giving up", timeout);
	}

	assert(TAILQ_EMPTY(&q));
	logx("all files parsed: generating output");
	rc = 0;

	/*
	 * For clean-up, close the input for the parser and rsync
	 * process.
	 * This will cause them to exit, then we reap them.
	 */

	close(proc);
	close(rsync);

	if (waitpid(procpid, &st, 0) == -1)
		err(1, "waitpid");
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		warnx("parser process exited abnormally");
		rc = 1;
	}
	if (!noop) {
		if (waitpid(rsyncpid, &st, 0) == -1)
			err(1, "waitpid");
		if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
			warnx("rsync process exited abnormally");
			rc = 1;
		}
	}
	gettimeofday(&now_time, NULL);
	timersub(&now_time, &start_time, &stats.elapsed_time);
	if (getrusage(RUSAGE_SELF, &ru) == 0) {
		stats.user_time = ru.ru_utime;
		stats.system_time = ru.ru_stime;
	}
	if (getrusage(RUSAGE_CHILDREN, &ru) == 0) {
		timeradd(&stats.user_time, &ru.ru_utime, &stats.user_time);
		timeradd(&stats.system_time, &ru.ru_stime, &stats.system_time);
	}

	if (outputfiles(&v, &stats))
		rc = 1;

	stats.del_files = repo_cleanup(cachedir);

	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("Certificates: %zu (%zu failed parse, %zu invalid)",
	    stats.certs, stats.certs_fail, stats.certs_invalid);
	logx("Trust Anchor Locators: %zu", stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Ghostbuster records: %zu", stats.gbrs);
	logx("Repositories: %zu", stats.repos);
	logx("Files removed: %zu", stats.del_files);
	logx("VRP Entries: %zu (%zu unique)", stats.vrps, stats.uniqs);

	/* Memory cleanup. */
	for (i = 0; i < rt.reposz; i++) {
		free(rt.repos[i].host);
		free(rt.repos[i].module);
	}
	free(rt.repos);

	for (i = 0; i < outsz; i++)
		roa_free(out[i]);
	free(out);

	return rc;

usage:
	fprintf(stderr,
	    "usage: rpki-client [-Bcjnov] [-b sourceaddr] [-d cachedir]"
	    " [-e rsync_prog]\n"
	    "                   [-s timeout] [-T table] [-t tal] [outputdir]\n");
	return 1;
}
