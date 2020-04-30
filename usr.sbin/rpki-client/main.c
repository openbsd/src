/*	$OpenBSD: main.c,v 1.68 2020/04/30 16:08:04 job Exp $ */
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

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
	int		 has_descr; /* whether descr is specified */
	char		*descr; /* tal description */
	TAILQ_ENTRY(entity) entries;
};

TAILQ_HEAD(entityq, entity);

/*
 * Mark that our subprocesses will never return.
 */
static void	proc_parser(int, int) __attribute__((noreturn));
static void	proc_rsync(char *, char *, int, int)
		    __attribute__((noreturn));
static void	build_chain(const struct auth *, STACK_OF(X509) **);
static void	build_crls(const struct auth *, struct crl_tree *,
		    STACK_OF(X509_CRL) **);

const char	*bird_tablename = "ROAS";

int	 verbose;

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

	io_simple_read(fd, &ent->id, sizeof(size_t));
	io_simple_read(fd, &ent->type, sizeof(enum rtype));
	io_str_read(fd, &ent->uri);
	io_simple_read(fd, &ent->has_dgst, sizeof(int));
	if (ent->has_dgst)
		io_simple_read(fd, ent->dgst, sizeof(ent->dgst));
	io_simple_read(fd, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_read_alloc(fd, (void **)&ent->pkey, &ent->pkeysz);
	io_simple_read(fd, &ent->has_descr, sizeof(int));
	if (ent->has_descr)
		io_str_read(fd, &ent->descr);
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
		errx(1, "%s: malformed", uri);

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
		err(1, "reallocarray");

	rp = &rt->repos[rt->reposz++];
	memset(rp, 0, sizeof(struct repo));
	rp->id = rt->reposz - 1;

	if ((rp->host = strndup(host, hostsz)) == NULL ||
	    (rp->module = strndup(mod, modsz)) == NULL)
		err(1, "strndup");

	i = rt->reposz - 1;

	logx("%s/%s: pulling from network", rp->host, rp->module);
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
	io_simple_buffer(b, bsz, bmax, &ent->has_descr, sizeof(int));
	if (ent->has_descr)
		io_str_buffer(b, bsz, bmax, ent->descr);
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
    const unsigned char *pkey, size_t pkeysz, char *descr, size_t *eid)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(1, "calloc");

	p->id = (*eid)++;
	p->type = type;
	p->uri = file;
	p->repo = (rp != NULL) ? (ssize_t)rp->id : -1;
	p->has_dgst = dgst != NULL;
	p->has_pkey = pkey != NULL;
	p->has_descr = descr != NULL;
	if (p->has_dgst)
		memcpy(p->dgst, dgst, sizeof(p->dgst));
	if (p->has_pkey) {
		p->pkeysz = pkeysz;
		if ((p->pkey = malloc(pkeysz)) == NULL)
			err(1, "malloc");
		memcpy(p->pkey, pkey, pkeysz);
	}
	if (p->has_descr)
		if ((p->descr = strdup(descr)) == NULL)
			err(1, "strdup");

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

	/* Construct local path from filename. */

	sz = strlen(file->file) + strlen(mft);
	if ((nfile = calloc(sz + 1, 1)) == NULL)
		err(1, "calloc");

	/* We know this is host/module/... */

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

	entityq_add(fd, q, nfile, type, NULL, file->hash, NULL, 0, NULL, eid);
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
	char	*nfile, *buf;

	if ((nfile = strdup(file)) == NULL)
		err(1, "strdup");
	buf = tal_read_file(file);

	/* Record tal for later reporting */
	if (stats.talnames == NULL)
		stats.talnames = strdup(file);
	else {
		char *tmp;
		asprintf(&tmp, "%s %s", stats.talnames, file);
		free(stats.talnames);
		stats.talnames = tmp;
	}

	/* Not in a repository, so directly add to queue. */
	entityq_add(fd, q, nfile, RTYPE_TAL, NULL, NULL, NULL, 0, buf, eid);
	/* entityq_add makes a copy of buf */
	free(buf);
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

	if (asprintf(&nfile, "%s/%s/%s", repo->host, repo->module, uri) == -1)
		err(1, "asprintf");

	entityq_add(proc, q, nfile, RTYPE_CER, repo, NULL, tal->pkey,
	    tal->pkeysz, tal->descr, eid);
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
		errx(1, "%s: unknown file type", uri);
	if (type != RTYPE_MFT && type != RTYPE_CRL)
		errx(1, "%s: invalid file type", uri);

	/* ignore the CRL since it is already loaded via the MFT */
	if (type == RTYPE_CRL)
		return;

	/* Look up the repository. */

	repo = repo_lookup(rsync, rt, uri);
	uri += 8 + strlen(repo->host) + 1 + strlen(repo->module) + 1;

	if (asprintf(&nfile, "%s/%s/%s", repo->host, repo->module, uri) == -1)
		err(1, "asprintf");

	entityq_add(proc, q, nfile, type, repo, NULL, NULL, 0, NULL, eid);
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
proc_rsync(char *prog, char *bind_addr, int fd, int noop)
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
				errx(1, "PATH is unset");
			if ((path = strdup(getenv("PATH"))) == NULL)
				err(1, "strdup");
			save = path;
			while ((pp = strsep(&path, ":")) != NULL) {
				if (*pp == '\0')
					continue;
				if (asprintf(&cmd, "%s/%s", pp, prog) == -1)
					err(1, "asprintf");
				if (lstat(cmd, &stt) == -1) {
					free(cmd);
					continue;
				} else if (unveil(cmd, "x") == -1)
					err(1, "%s: unveil", cmd);
				free(cmd);
				break;
			}
			free(save);
		} else if (unveil(prog, "x") == -1)
			err(1, "%s: unveil", prog);

		/* Unveil the repository directory and terminate unveiling. */

		if (unveil(".", "c") == -1)
			err(1, "unveil");
		if (unveil(NULL, NULL) == -1)
			err(1, "unveil");
	}

	/* Initialise retriever for children exiting. */

	if (sigemptyset(&mask) == -1)
		err(1, NULL);
	if (signal(SIGCHLD, proc_child) == SIG_ERR)
		err(1, NULL);
	if (sigaddset(&mask, SIGCHLD) == -1)
		err(1, NULL);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1)
		err(1, NULL);

	for (;;) {
		if (ppoll(&pfd, 1, NULL, &oldmask) == -1) {
			if (errno != EINTR)
				err(1, "ppoll");

			/*
			 * If we've received an EINTR, it means that one
			 * of our children has exited and we can reap it
			 * and look up its identifier.
			 * Then we respond to the parent.
			 */

			if ((pid = waitpid(WAIT_ANY, &st, 0)) == -1)
				err(1, "waitpid");

			for (i = 0; i < idsz; i++)
				if (ids[i].pid == pid)
					break;
			assert(i < idsz);

			if (!WIFEXITED(st)) {
				warnx("rsync %s terminated abnormally",
				    ids[i].uri);
				rc = 1;
			} else if (WEXITSTATUS(st) != 0) {
				warnx("rsync %s failed", ids[i].uri);
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
			err(1, "read");
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

		if (mkdir(host, 0700) == -1 && EEXIST != errno)
			err(1, "%s", host);

		if (asprintf(&dst, "%s/%s", host, mod) == -1)
			err(1, NULL);
		if (mkdir(dst, 0700) == -1 && EEXIST != errno)
			err(1, "%s", dst);

		if (asprintf(&uri, "rsync://%s/%s", host, mod) == -1)
			err(1, NULL);

		/* Run process itself, wait for exit, check error. */

		if ((pid = fork()) == -1)
			err(1, "fork");

		if (pid == 0) {
			if (pledge("stdio exec", NULL) == -1)
				err(1, "pledge");
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
			err(1, "%s: execvp", prog);
		}

		/* Augment the list of running processes. */

		for (i = 0; i < idsz; i++)
			if (ids[i].pid == 0)
				break;
		if (i == idsz) {
			ids = reallocarray(ids, idsz + 1, sizeof(*ids));
			if (ids == NULL)
				err(1, NULL);
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

	/* No need for these to be hanging around. */
	for (i = 0; i < idsz; i++)
		if (ids[i].pid > 0) {
			kill(ids[i].pid, SIGTERM);
			free(ids[i].uri);
		}

	free(ids);
	exit(rc);
	/* NOTREACHED */
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
proc_parser_mft(struct entity *entp, int force, X509_STORE *store,
    X509_STORE_CTX *ctx, struct auth_tree *auths, struct crl_tree *crlt)
{
	struct mft		*mft;
	X509			*x509;
	int			 c;
	struct auth		*a;
	STACK_OF(X509)		*chain;

	assert(!entp->has_dgst);
	if ((mft = mft_parse(&x509, entp->uri, force)) == NULL)
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
 * Certificates are from manifests (has a digest and is signed with another
 * certificate) or TALs (has a pkey and is self-signed).  Parse the certificate,
 * make sure its signatures are valid (with CRLs), then validate the RPKI
 * content.  This returns a certificate (which must not be freed) or NULL on
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

	assert(!entp->has_dgst != !entp->has_pkey);

	/* Extract certificate data and X509. */

	cert = entp->has_dgst ? cert_parse(&x509, entp->uri, entp->dgst) :
	    ta_parse(&x509, entp->uri, entp->pkey, entp->pkeysz);
	if (cert == NULL)
		return NULL;

	if (entp->has_dgst)
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
			cert_free(cert);
			sk_X509_free(chain);
			sk_X509_CRL_free(crls);
			X509_free(x509);
			return NULL;
		}
	}
	X509_STORE_CTX_cleanup(ctx);
	sk_X509_free(chain);
	sk_X509_CRL_free(crls);

	/* Validate the cert to get the parent */
	if (!(entp->has_pkey ?
		valid_ta(entp->uri, auths, cert) :
		valid_cert(entp->uri, auths, cert))) {
		X509_free(x509); // needed? XXX
		return cert;
	}

	/*
	 * Only on success of all do we add the certificate to the store
	 * of trusted certificates, both X509 and RPKI semantic.
	 */

	cert->valid = 1;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	if (entp->has_pkey) {
		if ((tal = strdup(entp->descr)) == NULL)
			err(1, NULL);
	} else
		tal = a->tal;

	na->parent = a;
	na->cert = cert;
	na->tal = tal;
	na->fn = strdup(entp->uri);
	if (na->fn == NULL)
		err(1, NULL);

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	/* only a ta goes into the store */
	if (a == NULL)
		X509_STORE_add_cert(store, x509);

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
	char			*t;

	dgst = entp->has_dgst ? entp->dgst : NULL;
	if ((x509_crl = crl_parse(entp->uri, dgst)) != NULL) {
		if ((crl = malloc(sizeof(*crl))) == NULL)
			err(1, NULL);
		if ((t = strdup(entp->uri)) == NULL)
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
proc_parser(int fd, int force)
{
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	struct entity	*entp;
	struct entityq	 q;
	int		 c, rc = 1;
	struct pollfd	 pfd;
	char		*b = NULL;
	size_t		 bsz = 0, bmax = 0, bpos = 0;
	ssize_t		 ssz;
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

	pfd.fd = fd;
	pfd.events = POLLIN;

	io_socket_nonblocking(pfd.fd);

	for (;;) {
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
				err(1, "write");
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

		entity_buffer_resp(&b, &bsz, &bmax, entp);

		switch (entp->type) {
		case RTYPE_TAL:
			assert(!entp->has_dgst);
			if ((tal = tal_parse(entp->uri, entp->descr)) == NULL)
				goto out;
			tal_buffer(&b, &bsz, &bmax, tal);
			tal_free(tal);
			break;
		case RTYPE_CER:
			cert = proc_parser_cert(entp, store, ctx,
			    &auths, &crlt);
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
			    store, ctx, &auths, &crlt);
			c = (mft != NULL);
			io_simple_buffer(&b, &bsz, &bmax, &c, sizeof(int));
			if (mft != NULL)
				mft_buffer(&b, &bsz, &bmax, mft);
			mft_free(mft);
			break;
		case RTYPE_CRL:
			proc_parser_crl(entp, store, ctx, &crlt);
			break;
		case RTYPE_ROA:
			assert(entp->has_dgst);
			roa = proc_parser_roa(entp, store, ctx, &auths, &crlt);
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

	rc = 0;
out:
	while ((entp = TAILQ_FIRST(&q)) != NULL) {
		TAILQ_REMOVE(&q, entp, entries);
		entity_free(entp);
	}

	/* XXX free auths and crl tree */

	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);

	free(b);

	exit(rc);
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
    size_t *eid, struct vrp_tree *tree)
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
		if (roa->valid)
			roa_insert_vrps(tree, roa, &st->vrps, &st->uniqs);
		else
			st->roas_invalid++;
		roa_free(roa);
		break;
	default:
		abort();
	}
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

int
main(int argc, char *argv[])
{
	int		 rc = 1, c, proc, st, rsync,
			 fl = SOCK_STREAM | SOCK_CLOEXEC, noop = 0,
			 force = 0;
	size_t		 i, j, eid = 1, outsz = 0, talsz = 0;
	pid_t		 procpid, rsyncpid;
	int		 fd[2];
	struct entityq	 q;
	struct entity	*ent;
	struct pollfd	 pfd[2];
	struct repotab	 rt;
	struct roa	**out = NULL;
	char		*rsync_prog = "openrsync";
	char		*bind_addr = NULL;
	const char	*cachedir = NULL;
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

	while ((c = getopt(argc, argv, "b:Bcd:e:fjnot:T:v")) != -1)
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
		case 'f':
			force = 1;
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

	memset(&rt, 0, sizeof(struct repotab));
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
		proc_parser(fd[0], force);
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
		err(1, "socketpair");
	if ((rsyncpid = fork()) == -1)
		err(1, "fork");

	if (rsyncpid == 0) {
		close(proc);
		close(fd[1]);

		/* change working directory to the cache directory */
		if (chdir(cachedir) == -1)
			err(1, "%s: chdir", cachedir);

		if (pledge("stdio rpath cpath proc exec unveil", NULL) == -1)
			err(1, "pledge");

		/* If -n, we don't exec or mkdir. */

		if (noop && pledge("stdio", NULL) == -1)
			err(1, "pledge");
		proc_rsync(rsync_prog, bind_addr, fd[0], noop);
		/* NOTREACHED */
	}

	close(fd[0]);
	rsync = fd[1];

	assert(rsync != proc);

	if (pledge("stdio rpath wpath cpath fattr", NULL) == -1)
		err(1, "pledge");

	/*
	 * Prime the process with our TAL file.
	 * This will contain (hopefully) links to our manifest and we
	 * can get the ball rolling.
	 */

	for (i = 0; i < talsz; i++)
		queue_add_tal(proc, &q, tals[i], &eid);

	/*
	 * The main process drives the top-down scan to leaf ROAs using
	 * data downloaded by the rsync process and parsed by the
	 * parsing process.
	 */

	pfd[0].fd = rsync;
	pfd[1].fd = proc;
	pfd[0].events = pfd[1].events = POLLIN;

	while (!TAILQ_EMPTY(&q)) {
		if ((c = poll(pfd, 2, verbose ? 10000 : INFTIM)) == -1)
			err(1, "poll");

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
			errx(1, "poll: bad fd");
		if ((pfd[0].revents & POLLHUP) ||
		    (pfd[1].revents & POLLHUP))
			errx(1, "poll: hangup");

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
			logx("%s/%s: loaded from cache", rt.repos[i].host,
			    rt.repos[i].module);
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
			    &q, ent, &rt, &eid, &v);
			if (verbose > 1)
				fprintf(stderr, "%s\n", ent->uri);
			entity_free(ent);
		}
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
	if (waitpid(rsyncpid, &st, 0) == -1)
		err(1, "waitpid");
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		warnx("rsync process exited abnormally");
		rc = 1;
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

	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("Certificates: %zu (%zu failed parse, %zu invalid)",
	    stats.certs, stats.certs_fail, stats.certs_invalid);
	logx("Trust Anchor Locators: %zu", stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Repositories: %zu", stats.repos);
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
	    "usage: rpki-client [-Bcfjnov] [-b sourceaddr] [-d cachedir]"
	    " [-e rsync_prog]\n"
	    "                   [-T table] [-t tal] [outputdir]\n");
	return 1;
}
