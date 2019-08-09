/*	$OpenBSD: main.c,v 1.13 2019/08/09 09:50:44 claudio Exp $ */
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

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <fts.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "extern.h"

/*
 * Base directory for where we'll look for all media.
 */
#define	BASE_DIR "/var/cache/rpki-client"

/*
 * Statistics collected during run-time.
 */
struct	stats {
	size_t	 tals; /* total number of locators */
	size_t	 mfts; /* total number of manifests */
	size_t	 mfts_fail; /* failing syntactic parse */
	size_t	 mfts_stale; /* stale manifests */
	size_t	 certs; /* certificates */
	size_t	 certs_fail; /* failing syntactic parse */
	size_t	 certs_invalid; /* invalid resources */
	size_t	 roas; /* route origin authorizations */
	size_t	 roas_fail; /* failing syntactic parse */
	size_t	 roas_invalid; /* invalid resources */
	size_t	 repos; /* repositories */
	size_t	 crls; /* revocation lists */
};

/*
 * An rsync repository.
 */
struct	repo {
	char	*host; /* hostname */
	char	*module; /* module name */
	int	 loaded; /* whether loaded or not */
	size_t	 id; /* identifier (array index) */
};

/*
 * A running rsync process.
 * We can have multiple of these simultaneously and need to keep track
 * of which process maps to which request.
 */
struct	rsyncproc {
	char	*uri; /* uri of this rsync proc */
	size_t	 id; /* identity of request */
	pid_t	 pid; /* pid of process or 0 if unassociated */
};

/*
 * Table of all known repositories.
 */
struct	repotab {
	struct repo	*repos; /* repositories */
	size_t		 reposz; /* number of repos */
};

/*
 * An entity (MFT, ROA, certificate, etc.) that needs to be downloaded
 * and parsed.
 */
struct	entity {
	size_t		 id; /* unique identifier */
	enum rtype	 type; /* type of entity (not RTYPE_EOF) */
	char		*uri; /* file or rsync:// URI */
	int		 has_dgst; /* whether dgst is specified */
	unsigned char	 dgst[SHA256_DIGEST_LENGTH]; /* optional */
	ssize_t		 repo; /* repo index or <0 if w/o repo */
	int		 has_pkey; /* whether pkey/sz is specified */
	unsigned char	*pkey; /* public key (optional) */
	size_t		 pkeysz; /* public key length (optional) */
	TAILQ_ENTRY(entity) entries;
};

TAILQ_HEAD(entityq, entity);

/*
 * Mark that our subprocesses will never return.
 */
static void	 proc_parser(int, int, int)
			__attribute__((noreturn));
static void	 proc_rsync(const char *, const char *, int, int)
			__attribute__((noreturn));
static void	 logx(const char *fmt, ...)
			__attribute__((format(printf, 1, 2)));

static int	 verbose;

/*
 * Log a message to stderr if and only if "verbose" is non-zero.
 * This uses the err(3) functionality.
 */
static void
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
 * Resolve the media type of a resource by looking at its suffice.
 * Returns the type of RTYPE_EOF if not found.
 */
static enum rtype
rtype_resolve(const char *uri)
{
	enum rtype	 rp;

	rsync_uri_parse(NULL, NULL, NULL, NULL, NULL, NULL, &rp, uri);
	return rp;
}

static void
entity_free(struct entity *ent)
{

	if (ent == NULL)
		return;

	free(ent->pkey);
	free(ent->uri);
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

	io_simple_read(fd, &ent->id, sizeof(size_t));
	io_simple_read(fd, &ent->type, sizeof(enum rtype));
	io_str_read(fd, &ent->uri);
	io_simple_read(fd, &ent->has_dgst, sizeof(int));
	if (ent->has_dgst)
		io_simple_read(fd, ent->dgst, sizeof(ent->dgst));
	io_simple_read(fd, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_read_alloc(fd, (void **)&ent->pkey, &ent->pkeysz);
}

/*
 * Look up a repository, queueing it for discovery if not found.
 */
static const struct repo *
repo_lookup(int fd, struct repotab *rt, const char *uri)
{
	const char	*host, *mod;
	size_t		 hostsz, modsz, i;
	struct repo	*rp;

	if (!rsync_uri_parse(&host, &hostsz,
	    &mod, &modsz, NULL, NULL, NULL, uri))
		errx(EXIT_FAILURE, "%s: malformed", uri);

	/* Look up in repository table. */

	for (i = 0; i < rt->reposz; i++) {
		if (strlen(rt->repos[i].host) != hostsz)
			continue;
		if (strlen(rt->repos[i].module) != modsz)
			continue;
		if (strncasecmp(rt->repos[i].host, host, hostsz))
			continue;
		if (strncasecmp(rt->repos[i].module, mod, modsz))
			continue;
		return &rt->repos[i];
	}

	rt->repos = reallocarray(rt->repos,
		rt->reposz + 1, sizeof(struct repo));
	if (rt->repos == NULL)
		err(EXIT_FAILURE, "reallocarray");

	rp = &rt->repos[rt->reposz++];
	memset(rp, 0, sizeof(struct repo));
	rp->id = rt->reposz - 1;

	if ((rp->host = strndup(host, hostsz)) == NULL ||
	    (rp->module = strndup(mod, modsz)) == NULL)
		err(EXIT_FAILURE, "strndup");

	i = rt->reposz - 1;

	logx("%s/%s: loading", rp->host, rp->module);
	io_simple_write(fd, &i, sizeof(size_t));
	io_str_write(fd, rp->host);
	io_str_write(fd, rp->module);
	return rp;
}

/*
 * Read the next entity from the parser process, removing it from the
 * queue of pending requests in the process.
 * This always returns a valid entity.
 */
static struct entity *
entityq_next(int fd, struct entityq *q)
{
	size_t		 id;
	struct entity	*entp;

	io_simple_read(fd, &id, sizeof(size_t));

	TAILQ_FOREACH(entp, q, entries)
		if (entp->id == id)
			break;

	assert(entp != NULL);
	TAILQ_REMOVE(q, entp, entries);
	return entp;
}

static void
entity_buffer_resp(char **b, size_t *bsz, size_t *bmax,
    const struct entity *ent)
{

	io_simple_buffer(b, bsz, bmax, &ent->id, sizeof(size_t));
}

/*
 * Like entity_write_req() but into a buffer.
 * Matched by entity_read_req().
 */
static void
entity_buffer_req(char **b, size_t *bsz, size_t *bmax,
    const struct entity *ent)
{

	io_simple_buffer(b, bsz, bmax, &ent->id, sizeof(size_t));
	io_simple_buffer(b, bsz, bmax, &ent->type, sizeof(enum rtype));
	io_str_buffer(b, bsz, bmax, ent->uri);
	io_simple_buffer(b, bsz, bmax, &ent->has_dgst, sizeof(int));
	if (ent->has_dgst)
		io_simple_buffer(b, bsz, bmax, ent->dgst, sizeof(ent->dgst));
	io_simple_buffer(b, bsz, bmax, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_buffer(b, bsz, bmax, ent->pkey, ent->pkeysz);
}

/*
 * Write the queue entity.
 * Simply a wrapper around entity_buffer_req().
 */
static void
entity_write_req(int fd, const struct entity *ent)
{
	char	*b = NULL;
	size_t	 bsz = 0, bmax = 0;

	entity_buffer_req(&b, &bsz, &bmax, ent);
	io_simple_write(fd, b, bsz);
	free(b);
}

/*
 * Scan through all queued requests and see which ones are in the given
 * repo, then flush those into the parser process.
 */
static void
entityq_flush(int fd, struct entityq *q, const struct repo *repo)
{
	struct entity	*p;

	TAILQ_FOREACH(p, q, entries) {
		if (p->repo < 0 || repo->id != (size_t)p->repo)
			continue;
		entity_write_req(fd, p);
	}
}

/*
 * Add the heap-allocated file to the queue for processing.
 */
static void
entityq_add(int fd, struct entityq *q, char *file, enum rtype type,
    const struct repo *rp, const unsigned char *dgst,
    const unsigned char *pkey, size_t pkeysz, size_t *eid)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(EXIT_FAILURE, "calloc");

	p->id = (*eid)++;
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
			err(EXIT_FAILURE, "malloc");
		memcpy(p->pkey, pkey, pkeysz);
	}
	TAILQ_INSERT_TAIL(q, p, entries);

	/*
	 * Write to the queue if there's no repo or the repo has already
	 * been loaded.
	 */

	if (rp == NULL || rp->loaded)
		entity_write_req(fd, p);
}

/*
 * Add a file (CER, ROA, CRL) from an MFT file, RFC 6486.
 * These are always relative to the directory in which "mft" sits.
 */
static void
queue_add_from_mft(int fd, struct entityq *q, const char *mft,
    const struct mftfile *file, enum rtype type, size_t *eid)
{
	size_t		 sz;
	char		*cp, *nfile;

	assert(strncmp(mft, BASE_DIR, strlen(BASE_DIR)) == 0);

	/* Construct local path from filename. */

	sz = strlen(file->file) + strlen(mft);
	if ((nfile = calloc(sz + 1, 1)) == NULL)
		err(EXIT_FAILURE, "calloc");

	/* We know this is BASE_DIR/host/module/... */

	strlcpy(nfile, mft, sz + 1);
	cp = strrchr(nfile, '/');
	assert(cp != NULL);
	cp++;
	*cp = '\0';
	strlcat(nfile, file->file, sz + 1);

	/*
	 * Since we're from the same directory as the MFT file, we know
	 * that the repository has already been loaded.
	 */

	entityq_add(fd, q, nfile, type, NULL, file->hash, NULL, 0, eid);
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
queue_add_from_mft_set(int fd, struct entityq *q, const struct mft *mft,
    size_t *eid)
{
	size_t			 i, sz;
	const struct mftfile	*f;

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl"))
			continue;
		queue_add_from_mft(fd, q, mft->file, f, RTYPE_CRL, eid);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".cer"))
			continue;
		queue_add_from_mft(fd, q, mft->file, f, RTYPE_CER, eid);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".roa"))
			continue;
		queue_add_from_mft(fd, q, mft->file, f, RTYPE_ROA, eid);
	}
}

/*
 * Add a local TAL file (RFC 7730) to the queue of files to fetch.
 */
static void
queue_add_tal(int fd, struct entityq *q, const char *file, size_t *eid)
{
	char		*nfile;

	if ((nfile = strdup(file)) == NULL)
		err(EXIT_FAILURE, "strdup");

	/* Not in a repository, so directly add to queue. */

	entityq_add(fd, q, nfile, RTYPE_TAL, NULL, NULL, NULL, 0, eid);
}

/*
 * Add rsync URIs (CER) from a TAL file, RFC 7730.
 * Only use the first URI of the set.
 */
static void
queue_add_from_tal(int proc, int rsync, struct entityq *q,
    const struct tal *tal, struct repotab *rt, size_t *eid)
{
	char			*nfile;
	const struct repo	*repo;
	const char		*uri;

	assert(tal->urisz);
	uri = tal->uri[0];

	/* Look up the repository. */

	assert(rtype_resolve(uri) == RTYPE_CER);
	repo = repo_lookup(rsync, rt, uri);
	uri += 8 + strlen(repo->host) + 1 + strlen(repo->module) + 1;

	if (asprintf(&nfile, "%s/%s/%s/%s",
	    BASE_DIR, repo->host, repo->module, uri) == -1)
		err(EXIT_FAILURE, "asprintf");

	entityq_add(proc, q, nfile, RTYPE_CER, repo, NULL, tal->pkey,
	    tal->pkeysz, eid);
}

/*
 * Add a manifest (MFT) or CRL found in an X509 certificate, RFC 6487.
 */
static void
queue_add_from_cert(int proc, int rsync, struct entityq *q,
    const char *uri, struct repotab *rt, size_t *eid)
{
	char			*nfile;
	enum rtype		 type;
	const struct repo	*repo;

	if ((type = rtype_resolve(uri)) == RTYPE_EOF)
		errx(EXIT_FAILURE, "%s: unknown file type", uri);
	if (type != RTYPE_MFT && type != RTYPE_CRL)
		errx(EXIT_FAILURE, "%s: invalid file type", uri);

	/* Look up the repository. */

	repo = repo_lookup(rsync, rt, uri);
	uri += 8 + strlen(repo->host) + 1 + strlen(repo->module) + 1;

	if (asprintf(&nfile, "%s/%s/%s/%s",
	    BASE_DIR, repo->host, repo->module, uri) == -1)
		err(EXIT_FAILURE, "asprintf");

	entityq_add(proc, q, nfile, type, repo, NULL, NULL, 0, eid);
}

static void
proc_child(int signal)
{

	/* Nothing: just discard. */
}

/*
 * Process used for synchronising repositories.
 * This simply waits to be told which repository to synchronise, then
 * does so.
 * It then responds with the identifier of the repo that it updated.
 * It only exits cleanly when fd is closed.
 * FIXME: this should use buffered output to prevent deadlocks, but it's
 * very unlikely that we're going to fill our buffer, so whatever.
 * FIXME: limit the number of simultaneous process.
 * Currently, an attacker can trivially specify thousands of different
 * repositories and saturate our system.
 */
static void
proc_rsync(const char *prog, const char *bind_addr, int fd, int noop)
{
	size_t			 id, i, idsz = 0;
	ssize_t			 ssz;
	char			*host = NULL, *mod = NULL, *uri = NULL,
				*dst = NULL, *path, *save, *cmd;
	const char		*pp;
	pid_t			 pid;
	char			*args[32];
	int			 st, rc = 0;
	struct stat		 stt;
	struct pollfd		 pfd;
	sigset_t		 mask, oldmask;
	struct rsyncproc	*ids = NULL;

	pfd.fd = fd;
	pfd.events = POLLIN;

	/*
	 * Unveil the command we want to run.
	 * If this has a pathname component in it, interpret as a file
	 * and unveil the file directly.
	 * Otherwise, look up the command in our PATH.
	 */

	if (!noop) {
		if (strchr(prog, '/') == NULL) {
			if (getenv("PATH") == NULL)
				errx(EXIT_FAILURE, "PATH is unset");
			if ((path = strdup(getenv("PATH"))) == NULL)
				err(EXIT_FAILURE, "strdup");
			save = path;
			while ((pp = strsep(&path, ":")) != NULL) {
				if (*pp == '\0')
					continue;
				if (asprintf(&cmd, "%s/%s", pp, prog) == -1)
					err(EXIT_FAILURE, "asprintf");
				if (lstat(cmd, &stt) == -1) {
					free(cmd);
					continue;
				} else if (unveil(cmd, "x") == -1)
					err(EXIT_FAILURE, "%s: unveil", cmd);
				free(cmd);
				break;
			}
			free(save);
		} else if (unveil(prog, "x") == -1)
			err(EXIT_FAILURE, "%s: unveil", prog);

		/* Unveil the repository directory and terminate unveiling. */

		if (unveil(BASE_DIR, "c") == -1)
			err(EXIT_FAILURE, "%s: unveil", BASE_DIR);
		if (unveil(NULL, NULL) == -1)
			err(EXIT_FAILURE, "unveil");
	}

	/* Initialise retriever for children exiting. */

	if (sigemptyset(&mask) == -1)
		err(EXIT_FAILURE, NULL);
	if (signal(SIGCHLD, proc_child) == SIG_ERR)
		err(EXIT_FAILURE, NULL);
	if (sigaddset(&mask, SIGCHLD) == -1)
		err(EXIT_FAILURE, NULL);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1)
		err(EXIT_FAILURE, NULL);

	for (;;) {
		if (ppoll(&pfd, 1, NULL, &oldmask) == -1) {
			if (errno != EINTR)
				err(EXIT_FAILURE, "ppoll");

			/*
			 * If we've received an EINTR, it means that one
			 * of our children has exited and we can reap it
			 * and look up its identifier.
			 * Then we respond to the parent.
			 */

			if ((pid = waitpid(WAIT_ANY, &st, 0)) == -1)
				err(EXIT_FAILURE, "waitpid");

			for (i = 0; i < idsz; i++)
				if (ids[i].pid == pid)
					break;
			assert(i < idsz);

			if (!WIFEXITED(st)) {
				warnx("rsync %s did not exit", ids[i].uri);
				goto out;
			} else if (WEXITSTATUS(st) != EXIT_SUCCESS) {
				warnx("rsync %s failed", ids[i].uri);
				goto out;
			}

			io_simple_write(fd, &ids[i].id, sizeof(size_t));
			free(ids[i].uri);
			ids[i].uri = NULL;
			ids[i].pid = 0;
			ids[i].id = 0;
			continue;
		}

		/*
		 * Read til the parent exits.
		 * That will mean that we can safely exit.
		 */

		if ((ssz = read(fd, &id, sizeof(size_t))) == -1)
			err(EXIT_FAILURE, "read");
		if (ssz == 0)
			break;

		/* Read host and module. */

		io_str_read(fd, &host);
		io_str_read(fd, &mod);

		if (noop) {
			io_simple_write(fd, &id, sizeof(size_t));
			free(host);
			free(mod);
			continue;
		}

		/*
		 * Create source and destination locations.
		 * Build up the tree to this point because GPL rsync(1)
		 * will not build the destination for us.
		 */

		if (asprintf(&dst, "%s/%s", BASE_DIR, host) == -1)
			err(EXIT_FAILURE, NULL);
		if (mkdir(dst, 0700) == -1 && EEXIST != errno)
			err(EXIT_FAILURE, "%s", dst);
		free(dst);

		if (asprintf(&dst, "%s/%s/%s", BASE_DIR, host, mod) == -1)
			err(EXIT_FAILURE, NULL);
		if (mkdir(dst, 0700) == -1 && EEXIST != errno)
			err(EXIT_FAILURE, "%s", dst);

		if (asprintf(&uri, "rsync://%s/%s", host, mod) == -1)
			err(EXIT_FAILURE, NULL);

		/* Run process itself, wait for exit, check error. */

		if ((pid = fork()) == -1)
			err(EXIT_FAILURE, "fork");

		if (pid == 0) {
			if (pledge("stdio exec", NULL) == -1)
				err(EXIT_FAILURE, "pledge");
			i = 0;
			args[i++] = (char *)prog;
			args[i++] = "-rlt";
			args[i++] = "--delete";
			if (bind_addr != NULL) {
				args[i++] = "--address";
				args[i++] = (char *)bind_addr;
			}
			args[i++] = uri;
			args[i++] = dst;
			args[i] = NULL;
			execvp(args[0], args);
			err(EXIT_FAILURE, "%s: execvp", prog);
		}

		/* Augment the list of running processes. */

		for (i = 0; i < idsz; i++)
			if (ids[i].pid == 0)
				break;
		if (i == idsz) {
			ids = reallocarray(ids, idsz + 1, sizeof(*ids));
			if (ids == NULL)
				err(EXIT_FAILURE, NULL);
			idsz++;
		}

		ids[i].id = id;
		ids[i].pid = pid;
		ids[i].uri = uri;

		/* Clean up temporary values. */

		free(mod);
		free(dst);
		free(host);
	}
	rc = 1;
out:

	/* No need for these to be hanging around. */

	for (i = 0; i < idsz; i++)
		if (ids[i].pid > 0) {
			kill(ids[i].pid, SIGTERM);
			free(ids[i].uri);
		}

	free(ids);
	exit(rc ? EXIT_SUCCESS : EXIT_FAILURE);
	/* NOTREACHED */
}

/*
 * Parse and validate a ROA, not parsing the CRL bits of "norev" has
 * been set.
 * This is standard stuff.
 * Returns the roa on success, NULL on failure.
 */
static struct roa *
proc_parser_roa(struct entity *entp, int norev,
    X509_STORE *store, X509_STORE_CTX *ctx,
    const struct auth *auths, size_t authsz)
{
	struct roa		*roa;
	X509			*x509;
	int			 c;
	X509_VERIFY_PARAM	*param;
	unsigned int		fl, nfl;

	assert(entp->has_dgst);
	if ((roa = roa_parse(&x509, entp->uri, entp->dgst)) == NULL)
		return NULL;

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");

	if ((param = X509_STORE_CTX_get0_param(ctx)) == NULL)
		cryptoerrx("X509_STORE_CTX_get0_param");
	fl = X509_VERIFY_PARAM_get_flags(param);
	nfl = X509_V_FLAG_IGNORE_CRITICAL;
	if (!norev)
		nfl |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;
	if (!X509_VERIFY_PARAM_set_flags(param, fl | nfl))
		cryptoerrx("X509_VERIFY_PARAM_set_flags");

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		warnx("%s: %s", entp->uri, X509_verify_cert_error_string(c));
		X509_free(x509);
		roa_free(roa);
		return NULL;
	}
	X509_STORE_CTX_cleanup(ctx);
	X509_free(x509);

	/*
	 * If the ROA isn't valid, we accept it anyway and depend upon
	 * the code around roa_read() to check the "valid" field itself.
	 */

	roa->valid = valid_roa(entp->uri, auths, authsz, roa);
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
proc_parser_mft(struct entity *entp, int force, X509_STORE *store,
    X509_STORE_CTX *ctx, const struct auth *auths, size_t authsz)
{
	struct mft		*mft;
	X509			*x509;
	int			 c;
	unsigned int		 fl, nfl;
	X509_VERIFY_PARAM	*param;

	assert(!entp->has_dgst);
	if ((mft = mft_parse(&x509, entp->uri, force)) == NULL)
		return NULL;

	if (!X509_STORE_CTX_init(ctx, store, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");

	if ((param = X509_STORE_CTX_get0_param(ctx)) == NULL)
		cryptoerrx("X509_STORE_CTX_get0_param");
	fl = X509_VERIFY_PARAM_get_flags(param);
	nfl = X509_V_FLAG_IGNORE_CRITICAL;
	if (!X509_VERIFY_PARAM_set_flags(param, fl | nfl))
		cryptoerrx("X509_VERIFY_PARAM_set_flags");

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		X509_STORE_CTX_cleanup(ctx);
		warnx("%s: %s", entp->uri, X509_verify_cert_error_string(c));
		mft_free(mft);
		X509_free(x509);
		return NULL;
	}

	X509_STORE_CTX_cleanup(ctx);
	X509_free(x509);
	return mft;
}

/*
 * Certificates are from manifests (has a digest and is signed with
 * another certificate) or TALs (has a pkey and is self-signed).
 * Parse the certificate, make sure its signatures are valid (with CRLs
 * unless "norev" has been specified), then validate the RPKI content.
 * This returns a certificate (which must not be freed) or NULL on parse
 * failure.
 */
static struct cert *
proc_parser_cert(const struct entity *entp, int norev,
    X509_STORE *store, X509_STORE_CTX *ctx,
    struct auth **auths, size_t *authsz)
{
	struct cert		*cert;
	X509			*x509;
	int			 c;
	X509_VERIFY_PARAM	*param;
	unsigned int		 fl, nfl;
	ssize_t			 id;

	assert(!entp->has_dgst != !entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = entp->has_dgst ? cert_parse(&x509, entp->uri, entp->dgst) :
	    ta_parse(&x509, entp->uri, entp->pkey, entp->pkeysz);
	if (cert == NULL)
		return NULL;

	/*
	 * Validate certificate chain w/CRLs.
	 * Only check the CRLs if specifically asked.
	 */

	assert(x509 != NULL);
	if (!X509_STORE_CTX_init(ctx, store, x509, NULL))
		cryptoerrx("X509_STORE_CTX_init");
	if ((param = X509_STORE_CTX_get0_param(ctx)) == NULL)
		cryptoerrx("X509_STORE_CTX_get0_param");
	fl = X509_VERIFY_PARAM_get_flags(param);
	nfl = X509_V_FLAG_IGNORE_CRITICAL;
	if (!norev)
		nfl |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;
	if (!X509_VERIFY_PARAM_set_flags(param, fl | nfl))
		cryptoerrx("X509_VERIFY_PARAM_set_flags");

	/*
	 * FIXME: can we pass any options to the verification that make
	 * the depth-zero self-signed bits verify properly?
	 */

	if (X509_verify_cert(ctx) <= 0) {
		c = X509_STORE_CTX_get_error(ctx);
		if (c != X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
		    !entp->has_pkey) {
			warnx("%s: %s", entp->uri,
			    X509_verify_cert_error_string(c));
			X509_STORE_CTX_cleanup(ctx);
			X509_free(x509);
			cert_free(cert);
			return NULL;
		}
	}
	X509_STORE_CTX_cleanup(ctx);

	/* Semantic validation of RPKI content. */

	id = entp->has_pkey ?
		valid_ta(entp->uri, *auths, *authsz, cert) :
		valid_cert(entp->uri, *auths, *authsz, cert);

	if (id < 0) {
		X509_free(x509);
		return cert;
	}

	/*
	 * Only on success of all do we add the certificate to the store
	 * of trusted certificates, both X509 and RPKI semantic.
	 */

	cert->valid = 1;
	*auths = reallocarray(*auths, *authsz + 1, sizeof(struct auth));
	if (*auths == NULL)
		err(EXIT_FAILURE, NULL);

	(*auths)[*authsz].id = *authsz;
	(*auths)[*authsz].parent = id;
	(*auths)[*authsz].cert = cert;
	(*auths)[*authsz].fn = strdup(entp->uri);
	if ((*auths)[*authsz].fn == NULL)
		err(EXIT_FAILURE, NULL);
	(*authsz)++;

	X509_STORE_add_cert(store, x509);
	X509_free(x509);
	return cert;
}

/*
 * Parse a certificate revocation list (unless "norev", in which case
 * this is a noop that returns success).
 * This simply parses the CRL content itself, optionally validating it
 * within the digest if it comes from a manifest, then adds it to the
 * store of CRLs.
 */
static void
proc_parser_crl(struct entity *entp, int norev, X509_STORE *store,
    X509_STORE_CTX *ctx, const struct auth *auths, size_t authsz)
{
	X509_CRL	    *x509;
	const unsigned char *dgst;

	if (norev)
		return;

	dgst = entp->has_dgst ? entp->dgst : NULL;
	if ((x509 = crl_parse(entp->uri, dgst)) != NULL) {
		X509_STORE_add_crl(store, x509);
		X509_CRL_free(x509);
	}
}

/*
 * Process responsible for parsing and validating content.
 * All this process does is wait to be told about a file to parse, then
 * it parses it and makes sure that the data being returned is fully
 * validated and verified.
 * The process will exit cleanly only when fd is closed.
 */
static void
proc_parser(int fd, int force, int norev)
{
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct entity	*entp;
	struct entityq	 q;
	int		 c, rc = 0;
	struct pollfd	 pfd;
	char		*b = NULL;
	size_t		 i, bsz = 0, bmax = 0, bpos = 0, authsz = 0;
	ssize_t		 ssz;
	X509_STORE	*store;
	X509_STORE_CTX	*ctx;
	struct auth	*auths = NULL;
	int		 first_tals = 1;

	SSL_library_init();
	SSL_load_error_strings();

	if ((store = X509_STORE_new()) == NULL)
		cryptoerrx("X509_STORE_new");
	if ((ctx = X509_STORE_CTX_new()) == NULL)
		cryptoerrx("X509_STORE_CTX_new");

	TAILQ_INIT(&q);

	pfd.fd = fd;
	pfd.events = POLLIN;

	io_socket_nonblocking(pfd.fd);

	for (;;) {
		if (poll(&pfd, 1, INFTIM) == -1)
			err(EXIT_FAILURE, "poll");
		if ((pfd.revents & (POLLERR|POLLNVAL)))
			errx(EXIT_FAILURE, "poll: bad descriptor");

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
				err(EXIT_FAILURE, NULL);
			entity_read_req(fd, entp);
			TAILQ_INSERT_TAIL(&q, entp, entries);
			pfd.events |= POLLOUT;
			io_socket_nonblocking(fd);
		}

		if (!(pfd.revents & POLLOUT))
			continue;

		/*
		 * If we have a write buffer, then continue trying to
		 * push it all out.
		 * When it's all pushed out, reset it and get ready to
		 * continue sucking down more data.
		 */

		if (bsz) {
			assert(bpos < bmax);
			if ((ssz = write(fd, b + bpos, bsz)) == -1)
				err(EXIT_FAILURE, "write");
			bpos += ssz;
			bsz -= ssz;
			if (bsz)
				continue;
			bpos = bsz = 0;
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

		/*
		 * Extra security.
		 * Our TAL files may be anywhere, but the repository
		 * resources may only be in BASE_DIR.
		 * When we've finished processing TAL files, make sure
		 * that we can only see what's under that.
		 */

		if (entp->type != RTYPE_TAL && first_tals) {
			if (unveil(BASE_DIR, "r") == -1)
				err(EXIT_FAILURE, "%s: unveil", BASE_DIR);
			if (unveil(NULL, NULL) == -1)
				err(EXIT_FAILURE, "unveil");
			first_tals = 0;
		} else if (entp->type != RTYPE_TAL) {
			assert(!first_tals);
		} else if (entp->type == RTYPE_TAL)
			assert(first_tals);

		entity_buffer_resp(&b, &bsz, &bmax, entp);

		switch (entp->type) {
		case RTYPE_TAL:
			assert(!entp->has_dgst);
			if ((tal = tal_parse(entp->uri)) == NULL)
				goto out;
			tal_buffer(&b, &bsz, &bmax, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			cert = proc_parser_cert(entp, norev,
				store, ctx, &auths, &authsz);
			c = (cert != NULL);
			io_simple_buffer(&b, &bsz, &bmax, &c, sizeof(int));
			if (cert != NULL)
				cert_buffer(&b, &bsz, &bmax, cert);
			/*
			 * The parsed certificate data "cert" is now
			 * managed in the "auths" table, so don't free
			 * it here (see the loop after "out").
			 */
			break;
		case RTYPE_MFT:
			mft = proc_parser_mft(entp, force,
			    store, ctx, auths, authsz);
			c = (mft != NULL);
			io_simple_buffer(&b, &bsz, &bmax, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(&b, &bsz, &bmax, mft);
			mft_free(mft);
			break;
		case RTYPE_CRL:
			proc_parser_crl(entp, norev,
			    store, ctx, auths, authsz);
			break;
		case RTYPE_ROA:
			assert(entp->has_dgst);
			roa = proc_parser_roa(entp, norev,
			    store, ctx, auths, authsz);
			c = (roa != NULL);
			io_simple_buffer(&b, &bsz, &bmax, &c, sizeof(int));
			if (roa != NULL)
				roa_buffer(&b, &bsz, &bmax, roa);
			roa_free(roa);
			break;
		default:
			abort();
		}

		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	rc = 1;
out:
	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	for (i = 0; i < authsz; i++) {
		free(auths[i].fn);
		cert_free(auths[i].cert);
	}

	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);
	free(auths);

	free(b);

	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	ERR_free_strings();

	exit(rc ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * Process parsed content.
 * For non-ROAs, we grok for more data.
 * For ROAs, we want to extract the valid info.
 * In all cases, we gather statistics.
 */
static void
entity_process(int proc, int rsync, struct stats *st,
    struct entityq *q, const struct entity *ent, struct repotab *rt,
    size_t *eid, struct roa ***out, size_t *outsz)
{
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

	switch (ent->type) {
	case RTYPE_TAL:
		st->tals++;
		tal = tal_read(proc);
		queue_add_from_tal(proc, rsync, q, tal, rt, eid);
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
			if (cert->crl != NULL)
				queue_add_from_cert(proc, rsync,
				    q, cert->crl, rt, eid);
			if (cert->mft != NULL)
				queue_add_from_cert(proc, rsync,
				    q, cert->mft, rt, eid);
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
		queue_add_from_mft_set(proc, q, mft, eid);
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
		if (roa->valid) {
			*out = reallocarray(*out,
			    *outsz + 1, sizeof(struct roa *));
			if (*out == NULL)
				err(EXIT_FAILURE, "reallocarray");
			(*out)[*outsz] = roa;
			(*outsz)++;
			/* We roa_free() on exit. */
		} else {
			st->roas_invalid++;
			roa_free(roa);
		}
		break;
	default:
		abort();
	}
}

int
main(int argc, char *argv[])
{
	int		 rc = 0, c, proc, st, rsync,
			 fl = SOCK_STREAM | SOCK_CLOEXEC, noop = 0,
			 force = 0, norev = 0, quiet = 0;
	size_t		 i, j, eid = 1, outsz = 0, vrps, uniqs;
	pid_t		 procpid, rsyncpid;
	int		 fd[2];
	struct entityq	 q;
	struct entity	*ent;
	struct pollfd	 pfd[2];
	struct repotab	 rt;
	struct stats	 stats;
	struct roa	**out = NULL;
	const char	*rsync_prog = "openrsync";
	const char	*bind_addr = NULL;

	if (pledge("stdio rpath proc exec cpath unveil", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	while ((c = getopt(argc, argv, "b:e:fnqrv")) != -1)
		switch (c) {
		case 'b':
			bind_addr = optarg;
			break;
		case 'e':
			rsync_prog = optarg;
			break;
		case 'f':
			force = 1;
			break;
		case 'n':
			noop = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			norev = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			goto usage;
		}

	argv += optind;
	if ((argc -= optind) == 0)
		goto usage;

	memset(&rt, 0, sizeof(struct repotab));
	memset(&stats, 0, sizeof(struct stats));
	TAILQ_INIT(&q);

	/*
	 * Create the file reader as a jailed child process.
	 * It will be responsible for reading all of the files (ROAs,
	 * manifests, certificates, etc.) and returning contents.
	 */

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(EXIT_FAILURE, "socketpair");
	if ((procpid = fork()) == -1)
		err(EXIT_FAILURE, "fork");

	if (procpid == 0) {
		close(fd[1]);
		if (pledge("stdio rpath unveil", NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		proc_parser(fd[0], force, norev);
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

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(EXIT_FAILURE, "socketpair");
	if ((rsyncpid = fork()) == -1)
		err(EXIT_FAILURE, "fork");

	if (rsyncpid == 0) {
		close(proc);
		close(fd[1]);
		if (pledge("stdio proc exec rpath cpath unveil", NULL) == -1)
			err(EXIT_FAILURE, "pledge");

		/* If -n, we don't exec or mkdir. */

		if (noop && pledge("stdio", NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		proc_rsync(rsync_prog, bind_addr, fd[0], noop);
		/* NOTREACHED */
	}

	close(fd[0]);
	rsync = fd[1];

	assert(rsync != proc);

	/*
	 * The main process drives the top-down scan to leaf ROAs using
	 * data downloaded by the rsync process and parsed by the
	 * parsing process.
	 */

	/* Initialise SSL, errors, and our structures. */

	SSL_library_init();
	SSL_load_error_strings();

	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	/*
	 * Prime the process with our TAL file.
	 * This will contain (hopefully) links to our manifest and we
	 * can get the ball rolling.
	 */

	for (i = 0; i < (size_t)argc; i++)
		queue_add_tal(proc, &q, argv[i], &eid);

	pfd[0].fd = rsync;
	pfd[1].fd = proc;
	pfd[0].events = pfd[1].events = POLLIN;

	while (!TAILQ_EMPTY(&q)) {
		if ((c = poll(pfd, 2, verbose ? 10000 : INFTIM)) == -1)
			err(EXIT_FAILURE, "poll");

		/* Debugging: print some statistics if we stall. */

		if (c == 0) {
			for (i = j = 0; i < rt.reposz; i++)
				if (!rt.repos[i].loaded)
					j++;
			logx("period stats: %zu pending repos", j);
			j = 0;
			TAILQ_FOREACH(ent, &q, entries)
				j++;
			logx("period stats: %zu pending entries", j);
			continue;
		}

		if ((pfd[0].revents & (POLLERR|POLLNVAL)) ||
		    (pfd[1].revents & (POLLERR|POLLNVAL)))
			errx(EXIT_FAILURE, "poll: bad fd");
		if ((pfd[0].revents & POLLHUP) ||
		    (pfd[1].revents & POLLHUP))
			errx(EXIT_FAILURE, "poll: hangup");

		/*
		 * Check the rsync process.
		 * This means that one of our modules has completed
		 * downloading and we can flush the module requests into
		 * the parser process.
		 */

		if ((pfd[0].revents & POLLIN)) {
			io_simple_read(rsync, &i, sizeof(size_t));
			assert(i < rt.reposz);
			assert(!rt.repos[i].loaded);
			rt.repos[i].loaded = 1;
			logx("%s/%s/%s: loaded", BASE_DIR,
			    rt.repos[i].host, rt.repos[i].module);
			stats.repos++;
			entityq_flush(proc, &q, &rt.repos[i]);
		}

		/*
		 * The parser has finished something for us.
		 * Dequeue these one by one.
		 */

		if ((pfd[1].revents & POLLIN)) {
			ent = entityq_next(proc, &q);
			entity_process(proc, rsync, &stats,
			    &q, ent, &rt, &eid, &out, &outsz);
			if (verbose > 1)
				fprintf(stderr, "%s\n", ent->uri);
			entity_free(ent);
		}
	}

	assert(TAILQ_EMPTY(&q));
	logx("all files parsed: exiting");
	rc = 1;

	/*
	 * For clean-up, close the input for the parser and rsync
	 * process.
	 * This will cause them to exit, then we reap them.
	 */

	close(proc);
	close(rsync);

	if (waitpid(procpid, &st, 0) == -1)
		err(EXIT_FAILURE, "waitpid");
	if (!WIFEXITED(st) || WEXITSTATUS(st) != EXIT_SUCCESS) {
		warnx("parser process exited abnormally");
		rc = 0;
	}
	if (waitpid(rsyncpid, &st, 0) == -1)
		err(EXIT_FAILURE, "waitpid");
	if (!WIFEXITED(st) || WEXITSTATUS(st) != EXIT_SUCCESS) {
		warnx("rsync process exited abnormally");
		rc = 0;
	}

	/* Output and statistics. */

	output_bgpd((const struct roa **)out,
	    outsz, quiet, &vrps, &uniqs);
	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("Certificates: %zu (%zu failed parse, %zu invalid)",
	    stats.certs, stats.certs_fail, stats.certs_invalid);
	logx("Trust Anchor Locators: %zu", stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Repositories: %zu", stats.repos);
	logx("VRP Entries: %zu (%zu unique)", vrps, uniqs);

	/* Memory cleanup. */

	for (i = 0; i < rt.reposz; i++) {
		free(rt.repos[i].host);
		free(rt.repos[i].module);
	}
	free(rt.repos);

	for (i = 0; i < outsz; i++)
		roa_free(out[i]);
	free(out);

	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	ERR_free_strings();
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;

usage:
	fprintf(stderr,
	    "usage: rpki-client [-fnqrv] [-b bind_addr] [-e rsync_prog] "
	    "tal ...\n");
	return EXIT_FAILURE;
}
