/*	$OpenBSD: repo.c,v 1.39 2022/09/02 21:56:45 claudio Exp $ */
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

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <imsg.h>

#include "extern.h"

extern struct stats	stats;
extern int		noop;
extern int		rrdpon;
extern int		repo_timeout;
extern time_t		deadline;
int			nofetch;

enum repo_state {
	REPO_LOADING = 0,
	REPO_DONE = 1,
	REPO_FAILED = -1,
};

/*
 * A ta, rsync or rrdp repository.
 * Depending on what is needed the generic repository is backed by
 * a ta, rsync or rrdp repository. Multiple repositories can use the
 * same backend.
 */
struct rrdprepo {
	SLIST_ENTRY(rrdprepo)	 entry;
	char			*notifyuri;
	char			*basedir;
	struct filepath_tree	 deleted;
	unsigned int		 id;
	enum repo_state		 state;
};
static SLIST_HEAD(, rrdprepo)	rrdprepos = SLIST_HEAD_INITIALIZER(rrdprepos);

struct rsyncrepo {
	SLIST_ENTRY(rsyncrepo)	 entry;
	char			*repouri;
	char			*basedir;
	unsigned int		 id;
	enum repo_state		 state;
};
static SLIST_HEAD(, rsyncrepo)	rsyncrepos = SLIST_HEAD_INITIALIZER(rsyncrepos);

struct tarepo {
	SLIST_ENTRY(tarepo)	 entry;
	char			*descr;
	char			*basedir;
	char			*temp;
	char			**uri;
	size_t			 urisz;
	size_t			 uriidx;
	unsigned int		 id;
	enum repo_state		 state;
};
static SLIST_HEAD(, tarepo)	tarepos = SLIST_HEAD_INITIALIZER(tarepos);

struct repo {
	SLIST_ENTRY(repo)	 entry;
	char			*repouri;
	char			*notifyuri;
	char			*basedir;
	const struct rrdprepo	*rrdp;
	const struct rsyncrepo	*rsync;
	const struct tarepo	*ta;
	struct entityq		 queue;		/* files waiting for repo */
	time_t			 alarm;		/* sync timeout */
	int			 talid;
	unsigned int		 id;		/* identifier */
};
static SLIST_HEAD(, repo)	repos = SLIST_HEAD_INITIALIZER(repos);

/* counter for unique repo id */
unsigned int		repoid;

static struct rsyncrepo	*rsync_get(const char *, const char *);
static void		 remove_contents(char *);

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
	return strcmp(a->file, b->file);
}

RB_PROTOTYPE(filepath_tree, filepath, entry, filepathcmp);

/*
 * Functions to lookup which files have been accessed during computation.
 */
int
filepath_add(struct filepath_tree *tree, char *file)
{
	struct filepath *fp;

	if ((fp = malloc(sizeof(*fp))) == NULL)
		err(1, NULL);
	if ((fp->file = strdup(file)) == NULL)
		err(1, NULL);

	if (RB_INSERT(filepath_tree, tree, fp) != NULL) {
		/* already in the tree */
		free(fp->file);
		free(fp);
		return 0;
	}

	return 1;
}

/*
 * Lookup a file path in the tree and return the object if found or NULL.
 */
static struct filepath *
filepath_find(struct filepath_tree *tree, char *file)
{
	struct filepath needle = { .file = file };

	return RB_FIND(filepath_tree, tree, &needle);
}

/*
 * Returns true if file exists in the tree.
 */
static int
filepath_exists(struct filepath_tree *tree, char *file)
{
	return filepath_find(tree, file) != NULL;
}

/*
 * Remove entry from tree and free it.
 */
static void
filepath_put(struct filepath_tree *tree, struct filepath *fp)
{
	RB_REMOVE(filepath_tree, tree, fp);
	free((void *)fp->file);
	free(fp);
}

/*
 * Free all elements of a filepath tree.
 */
static void
filepath_free(struct filepath_tree *tree)
{
	struct filepath *fp, *nfp;

	RB_FOREACH_SAFE(fp, filepath_tree, tree, nfp)
		filepath_put(tree, fp);
}

RB_GENERATE(filepath_tree, filepath, entry, filepathcmp);

/*
 * Function to hash a string into a unique directory name.
 * Returned hash needs to be freed.
 */
static char *
hash_dir(const char *uri)
{
	unsigned char m[SHA256_DIGEST_LENGTH];

	SHA256(uri, strlen(uri), m);
	return hex_encode(m, sizeof(m));
}

/*
 * Function to build the directory name based on URI and a directory
 * as prefix. Skip the proto:// in URI but keep everything else.
 */
static char *
repo_dir(const char *uri, const char *dir, int hash)
{
	const char *local;
	char *out, *hdir = NULL;

	if (hash) {
		local = hdir = hash_dir(uri);
	} else {
		local = strchr(uri, ':');
		if (local != NULL)
			local += strlen("://");
		else
			local = uri;
	}

	if (dir == NULL) {
		if ((out = strdup(local)) == NULL)
			err(1, NULL);
	} else {
		if (asprintf(&out, "%s/%s", dir, local) == -1)
			err(1, NULL);
	}

	free(hdir);
	return out;
}

/*
 * Function to create all missing directories to a path.
 * This functions alters the path temporarily.
 */
static int
repo_mkpath(int fd, char *file)
{
	char *slash;

	/* build directory hierarchy */
	slash = strrchr(file, '/');
	assert(slash != NULL);
	*slash = '\0';
	if (mkpathat(fd, file) == -1) {
		warn("mkpath %s", file);
		return -1;
	}
	*slash = '/';
	return 0;
}

/*
 * Return the state of a repository.
 */
static enum repo_state
repo_state(struct repo *rp)
{
	if (rp->ta)
		return rp->ta->state;
	if (rp->rsync)
		return rp->rsync->state;
	if (rp->rrdp)
		return rp->rrdp->state;
	/* No backend so sync is by definition done. */
	return REPO_DONE;
}

/*
 * Function called once a repository is done with the sync. Either
 * successfully or after failure.
 */
static void
repo_done(const void *vp, int ok)
{
	struct repo *rp;

	SLIST_FOREACH(rp, &repos, entry) {
		if (vp == rp->ta)
			entityq_flush(&rp->queue, rp);
		if (vp == rp->rsync)
			entityq_flush(&rp->queue, rp);
		if (vp == rp->rrdp) {
			if (!ok && !nofetch) {
				/* try to fall back to rsync */
				rp->rrdp = NULL;
				rp->rsync = rsync_get(rp->repouri,
				    rp->basedir);
				/* need to check if it was already loaded */
				if (repo_state(rp) != REPO_LOADING)
					entityq_flush(&rp->queue, rp);
			} else
				entityq_flush(&rp->queue, rp);
		}
	}
}

/*
 * Build TA file name based on the repo info.
 * If temp is set add Xs for mkostemp.
 */
static char *
ta_filename(const struct tarepo *tr, int temp)
{
	const char *file;
	char *nfile;

	/* does not matter which URI, all end with same filename */
	file = strrchr(tr->uri[0], '/');
	assert(file);

	if (asprintf(&nfile, "%s%s%s", tr->basedir, file,
	    temp ? ".XXXXXXXX" : "") == -1)
		err(1, NULL);

	return nfile;
}

static void
ta_fetch(struct tarepo *tr)
{
	if (!rrdpon) {
		for (; tr->uriidx < tr->urisz; tr->uriidx++) {
			if (strncasecmp(tr->uri[tr->uriidx],
			    "rsync://", 8) == 0)
				break;
		}
	}

	if (tr->uriidx >= tr->urisz) {
		tr->state = REPO_FAILED;
		logx("ta/%s: fallback to cache", tr->descr);

		repo_done(tr, 0);
		return;
	}

	logx("ta/%s: pulling from %s", tr->descr, tr->uri[tr->uriidx]);

	if (strncasecmp(tr->uri[tr->uriidx], "rsync://", 8) == 0) {
		/*
		 * Create destination location.
		 * Build up the tree to this point.
		 */
		rsync_fetch(tr->id, tr->uri[tr->uriidx], tr->basedir, NULL);
	} else {
		int fd;

		tr->temp = ta_filename(tr, 1);
		fd = mkostemp(tr->temp, O_CLOEXEC);
		if (fd == -1) {
			warn("mkostemp: %s", tr->temp);
			http_finish(tr->id, HTTP_FAILED, NULL);
			return;
		}
		if (fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
			warn("fchmod: %s", tr->temp);

		http_fetch(tr->id, tr->uri[tr->uriidx], NULL, fd);
	}
}

static struct tarepo *
ta_get(struct tal *tal)
{
	struct tarepo *tr;

	/* no need to look for possible other repo */

	if ((tr = calloc(1, sizeof(*tr))) == NULL)
		err(1, NULL);
	tr->id = ++repoid;
	SLIST_INSERT_HEAD(&tarepos, tr, entry);

	if ((tr->descr = strdup(tal->descr)) == NULL)
		err(1, NULL);
	tr->basedir = repo_dir(tal->descr, "ta", 0);

	/* steal URI information from TAL */
	tr->urisz = tal->urisz;
	tr->uri = tal->uri;
	tal->urisz = 0;
	tal->uri = NULL;

	ta_fetch(tr);

	return tr;
}

static struct tarepo *
ta_find(unsigned int id)
{
	struct tarepo *tr;

	SLIST_FOREACH(tr, &tarepos, entry)
		if (id == tr->id)
			break;
	return tr;
}

static void
ta_free(void)
{
	struct tarepo *tr;

	while ((tr = SLIST_FIRST(&tarepos)) != NULL) {
		SLIST_REMOVE_HEAD(&tarepos, entry);
		free(tr->descr);
		free(tr->basedir);
		free(tr->temp);
		free(tr->uri);
		free(tr);
	}
}

static struct rsyncrepo *
rsync_get(const char *uri, const char *validdir)
{
	struct rsyncrepo *rr;
	char *repo;

	if ((repo = rsync_base_uri(uri)) == NULL)
		errx(1, "bad caRepository URI: %s", uri);

	SLIST_FOREACH(rr, &rsyncrepos, entry)
		if (strcmp(rr->repouri, repo) == 0) {
			free(repo);
			return rr;
		}

	if ((rr = calloc(1, sizeof(*rr))) == NULL)
		err(1, NULL);

	rr->id = ++repoid;
	SLIST_INSERT_HEAD(&rsyncrepos, rr, entry);

	rr->repouri = repo;
	rr->basedir = repo_dir(repo, ".rsync", 0);

	/* create base directory */
	if (mkpath(rr->basedir) == -1) {
		warn("mkpath %s", rr->basedir);
		rsync_finish(rr->id, 0);
		return rr;
	}

	logx("%s: pulling from %s", rr->basedir, rr->repouri);
	rsync_fetch(rr->id, rr->repouri, rr->basedir, validdir);

	return rr;
}

static struct rsyncrepo *
rsync_find(unsigned int id)
{
	struct rsyncrepo *rr;

	SLIST_FOREACH(rr, &rsyncrepos, entry)
		if (id == rr->id)
			break;
	return rr;
}

static void
rsync_free(void)
{
	struct rsyncrepo *rr;

	while ((rr = SLIST_FIRST(&rsyncrepos)) != NULL) {
		SLIST_REMOVE_HEAD(&rsyncrepos, entry);
		free(rr->repouri);
		free(rr->basedir);
		free(rr);
	}
}

/*
 * Build local file name base on the URI and the rrdprepo info.
 */
static char *
rrdp_filename(const struct rrdprepo *rr, const char *uri, int valid)
{
	char *nfile;
	const char *dir = rr->basedir;

	if (!valid_uri(uri, strlen(uri), "rsync://"))
		errx(1, "%s: bad URI %s", rr->basedir, uri);
	uri += strlen("rsync://");	/* skip proto */
	if (valid) {
		if ((nfile = strdup(uri)) == NULL)
			err(1, NULL);
	} else {
		if (asprintf(&nfile, "%s/%s", dir, uri) == -1)
			err(1, NULL);
	}
	return nfile;
}

/*
 * Build RRDP state file name based on the repo info.
 * If temp is set add Xs for mkostemp.
 */
static char *
rrdp_state_filename(const struct rrdprepo *rr, int temp)
{
	char *nfile;

	if (asprintf(&nfile, "%s/.state%s", rr->basedir,
	    temp ? ".XXXXXXXX" : "") == -1)
		err(1, NULL);

	return nfile;
}

static struct rrdprepo *
rrdp_find(unsigned int id)
{
	struct rrdprepo *rr;

	SLIST_FOREACH(rr, &rrdprepos, entry)
		if (id == rr->id)
			break;
	return rr;
}

static void
rrdp_free(void)
{
	struct rrdprepo *rr;

	while ((rr = SLIST_FIRST(&rrdprepos)) != NULL) {
		SLIST_REMOVE_HEAD(&rrdprepos, entry);

		free(rr->notifyuri);
		free(rr->basedir);

		filepath_free(&rr->deleted);

		free(rr);
	}
}

/*
 * Check if a directory is an active rrdp repository.
 * Returns 1 if found else 0.
 */
static int
rrdp_is_active(const char *dir)
{
	struct rrdprepo *rr;

	SLIST_FOREACH(rr, &rrdprepos, entry)
		if (strcmp(dir, rr->basedir) == 0)
			return rr->state != REPO_FAILED;

	return 0;
}

/*
 * Check if the URI is actually covered by one of the repositories
 * that depend on this RRDP repository.
 * Returns 1 if the URI is valid, 0 if no repouri matches the URI.
 */
static int
rrdp_uri_valid(struct rrdprepo *rr, const char *uri)
{
	struct repo *rp;

	SLIST_FOREACH(rp, &repos, entry) {
		if (rp->rrdp != rr)
			continue;
		if (strncmp(uri, rp->repouri, strlen(rp->repouri)) == 0)
			return 1;
	}
	return 0;
}

/*
 * Allocate and insert a new repository.
 */
static struct repo *
repo_alloc(int talid)
{
	struct repo *rp;

	if ((rp = calloc(1, sizeof(*rp))) == NULL)
		err(1, NULL);

	rp->id = ++repoid;
	rp->talid = talid;
	rp->alarm = getmonotime() + repo_timeout;
	TAILQ_INIT(&rp->queue);
	SLIST_INSERT_HEAD(&repos, rp, entry);

	stats.repos++;
	return rp;
}

/*
 * Parse the RRDP state file if it exists and set the session struct
 * based on that information.
 */
static void
rrdp_parse_state(const struct rrdprepo *rr, struct rrdp_session *state)
{
	FILE *f;
	int fd, ln = 0;
	const char *errstr;
	char *line = NULL, *file;
	size_t len = 0;
	ssize_t n;

	file = rrdp_state_filename(rr, 0);
	if ((fd = open(file, O_RDONLY)) == -1) {
		if (errno != ENOENT)
			warn("%s: open state file", rr->basedir);
		free(file);
		return;
	}
	free(file);
	f = fdopen(fd, "r");
	if (f == NULL)
		err(1, "fdopen");

	while ((n = getline(&line, &len, f)) != -1) {
		if (line[n - 1] == '\n')
			line[n - 1] = '\0';
		switch (ln) {
		case 0:
			if ((state->session_id = strdup(line)) == NULL)
				err(1, NULL);
			break;
		case 1:
			state->serial = strtonum(line, 1, LLONG_MAX, &errstr);
			if (errstr)
				goto fail;
			break;
		case 2:
			if ((state->last_mod = strdup(line)) == NULL)
				err(1, NULL);
			break;
		default:
			goto fail;
		}
		ln++;
	}

	free(line);
	if (ferror(f))
		goto fail;
	fclose(f);
	return;

fail:
	warnx("%s: troubles reading state file", rr->basedir);
	fclose(f);
	free(state->session_id);
	free(state->last_mod);
	memset(state, 0, sizeof(*state));
}

/*
 * Carefully write the RRDP session state file back.
 */
void
rrdp_save_state(unsigned int id, struct rrdp_session *state)
{
	struct rrdprepo *rr;
	char *temp, *file;
	FILE *f;
	int fd;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "non-existant rrdp repo %u", id);

	file = rrdp_state_filename(rr, 0);
	temp = rrdp_state_filename(rr, 1);

	if ((fd = mkostemp(temp, O_CLOEXEC)) == -1) {
		warn("mkostemp %s", temp);
		goto fail;
	}
	(void)fchmod(fd, 0644);
	f = fdopen(fd, "w");
	if (f == NULL)
		err(1, "fdopen");

	/* write session state file out */
	if (fprintf(f, "%s\n%lld\n", state->session_id,
	    state->serial) < 0) {
		fclose(f);
		goto fail;
	}
	if (state->last_mod != NULL) {
		if (fprintf(f, "%s\n", state->last_mod) < 0) {
			fclose(f);
			goto fail;
		}
	}
	if (fclose(f) != 0)
		goto fail;

	if (rename(temp, file) == -1)
		warn("%s: rename state file", rr->basedir);

	free(temp);
	free(file);
	return;

fail:
	warnx("%s: failed to save state", rr->basedir);
	unlink(temp);
	free(temp);
	free(file);
}

static struct rrdprepo *
rrdp_get(const char *uri)
{
	struct rrdp_session state = { 0 };
	struct rrdprepo *rr;

	SLIST_FOREACH(rr, &rrdprepos, entry)
		if (strcmp(rr->notifyuri, uri) == 0) {
			if (rr->state == REPO_FAILED)
				return NULL;
			return rr;
		}

	if ((rr = calloc(1, sizeof(*rr))) == NULL)
		err(1, NULL);

	rr->id = ++repoid;
	SLIST_INSERT_HEAD(&rrdprepos, rr, entry);

	if ((rr->notifyuri = strdup(uri)) == NULL)
		err(1, NULL);
	rr->basedir = repo_dir(uri, ".rrdp", 1);

	RB_INIT(&rr->deleted);

	/* create base directory */
	if (mkpath(rr->basedir) == -1) {
		warn("mkpath %s", rr->basedir);
		rrdp_finish(rr->id, 0);
		return rr;
	}

	/* parse state and start the sync */
	rrdp_parse_state(rr, &state);
	rrdp_fetch(rr->id, rr->notifyuri, rr->notifyuri, &state);
	free(state.session_id);
	free(state.last_mod);

	logx("%s: pulling from %s", rr->notifyuri, "network");

	return rr;
}

/*
 * Remove RRDP repo and start over.
 */
void
rrdp_clear(unsigned int id)
{
	struct rrdprepo *rr;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "non-existant rrdp repo %u", id);

	/* remove rrdp repository contents */
	remove_contents(rr->basedir);
}

/*
 * Write a file into the temporary RRDP dir but only after checking
 * its hash (if required). The function also makes sure that the file
 * tracking is properly adjusted.
 * Returns 1 on success, 0 if the repo is corrupt, -1 on IO error
 */
int
rrdp_handle_file(unsigned int id, enum publish_type pt, char *uri,
    char *hash, size_t hlen, char *data, size_t dlen)
{
	struct rrdprepo *rr;
	struct filepath *fp;
	ssize_t s;
	char *fn = NULL;
	int fd = -1, try = 0;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "non-existant rrdp repo %u", id);
	if (rr->state == REPO_FAILED)
		return -1;

	/* check hash of original file for updates and deletes */
	if (pt == PUB_UPD || pt == PUB_DEL) {
		if (filepath_exists(&rr->deleted, uri)) {
			warnx("%s: already deleted", uri);
			return 0;
		}
		/* try to open file first in rrdp then in valid repo */
		do {
			free(fn);
			if ((fn = rrdp_filename(rr, uri, try++)) == NULL)
				return 0;
			fd = open(fn, O_RDONLY);
		} while (fd == -1 && try < 2);

		if (!valid_filehash(fd, hash, hlen)) {
			warnx("%s: bad file digest for %s", rr->notifyuri, fn);
			free(fn);
			return 0;
		}
		free(fn);
	}

	/* write new content or mark uri as deleted. */
	if (pt == PUB_DEL) {
		filepath_add(&rr->deleted, uri);
	} else {
		fp = filepath_find(&rr->deleted, uri);
		if (fp != NULL)
			filepath_put(&rr->deleted, fp);

		/* add new file to rrdp dir */
		if ((fn = rrdp_filename(rr, uri, 0)) == NULL)
			return 0;

		if (repo_mkpath(AT_FDCWD, fn) == -1)
			goto fail;

		fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if (fd == -1) {
			warn("open %s", fn);
			goto fail;
		}

		if ((s = write(fd, data, dlen)) == -1) {
			warn("write %s", fn);
			goto fail;
		}
		close(fd);
		if ((size_t)s != dlen)	/* impossible */
			errx(1, "short write %s", fn);
		free(fn);
	}

	return 1;

fail:
	rr->state = REPO_FAILED;
	if (fd != -1)
		close(fd);
	free(fn);
	return -1;
}

/*
 * RSYNC sync finished, either with or without success.
 */
void
rsync_finish(unsigned int id, int ok)
{
	struct rsyncrepo *rr;
	struct tarepo *tr;

	tr = ta_find(id);
	if (tr != NULL) {
		/* repository changed state already, ignore request */
		if (tr->state != REPO_LOADING)
			return;
		if (ok) {
			logx("ta/%s: loaded from network", tr->descr);
			stats.rsync_repos++;
			tr->state = REPO_DONE;
			repo_done(tr, 1);
		} else {
			warnx("ta/%s: load from network failed", tr->descr);
			stats.rsync_fails++;
			tr->uriidx++;
			ta_fetch(tr);
		}
		return;
	}

	rr = rsync_find(id);
	if (rr == NULL)
		errx(1, "unknown rsync repo %u", id);
	/* repository changed state already, ignore request */
	if (rr->state != REPO_LOADING)
		return;

	if (ok) {
		logx("%s: loaded from network", rr->basedir);
		stats.rsync_repos++;
		rr->state = REPO_DONE;
	} else {
		warnx("%s: load from network failed, fallback to cache",
		    rr->basedir);
		stats.rsync_fails++;
		rr->state = REPO_FAILED;
		/* clear rsync repo since it failed */
		remove_contents(rr->basedir);
	}

	repo_done(rr, ok);
}

/*
 * RRDP sync finshed, either with or without success.
 */
void
rrdp_finish(unsigned int id, int ok)
{
	struct rrdprepo *rr;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "unknown RRDP repo %u", id);
	/* repository changed state already, ignore request */
	if (rr->state != REPO_LOADING)
		return;

	if (ok) {
		logx("%s: loaded from network", rr->notifyuri);
		stats.rrdp_repos++;
		rr->state = REPO_DONE;
	} else {
		warnx("%s: load from network failed, fallback to %s",
		    rr->notifyuri, nofetch ? "cache" : "rsync");
		stats.rrdp_fails++;
		rr->state = REPO_FAILED;
		/* clear the RRDP repo since it failed */
		remove_contents(rr->basedir);
		/* also clear the list of deleted files */
		filepath_free(&rr->deleted);
	}

	repo_done(rr, ok);
}

/*
 * Handle responses from the http process. For TA file, either rename
 * or delete the temporary file. For RRDP requests relay the request
 * over to the rrdp process.
 */
void
http_finish(unsigned int id, enum http_result res, const char *last_mod)
{
	struct tarepo *tr;

	tr = ta_find(id);
	if (tr == NULL) {
		/* not a TA fetch therefor RRDP */
		rrdp_http_done(id, res, last_mod);
		return;
	}

	/* repository changed state already, ignore request */
	if (tr->state != REPO_LOADING)
		return;

	/* Move downloaded TA file into place, or unlink on failure. */
	if (res == HTTP_OK) {
		char *file;

		file = ta_filename(tr, 0);
		if (rename(tr->temp, file) == -1)
			warn("rename to %s", file);
		free(file);

		logx("ta/%s: loaded from network", tr->descr);
		tr->state = REPO_DONE;
		stats.http_repos++;
		repo_done(tr, 1);
	} else {
		if (unlink(tr->temp) == -1 && errno != ENOENT)
			warn("unlink %s", tr->temp);

		tr->uriidx++;
		warnx("ta/%s: load from network failed", tr->descr);
		ta_fetch(tr);
	}
}

/*
 * Look up a trust anchor, queueing it for download if not found.
 */
struct repo *
ta_lookup(int id, struct tal *tal)
{
	struct repo	*rp;

	if (tal->urisz == 0)
		errx(1, "TAL %s has no URI", tal->descr);

	/* Look up in repository table. (Lookup should actually fail here) */
	SLIST_FOREACH(rp, &repos, entry) {
		if (strcmp(rp->repouri, tal->descr) == 0)
			return rp;
	}

	rp = repo_alloc(id);
	rp->basedir = repo_dir(tal->descr, "ta", 0);
	if ((rp->repouri = strdup(tal->descr)) == NULL)
		err(1, NULL);

	/* check if sync disabled ... */
	if (noop) {
		logx("ta/%s: using cache", rp->repouri);
		entityq_flush(&rp->queue, rp);
		return rp;
	}

	/* try to create base directory */
	if (mkpath(rp->basedir) == -1)
		warn("mkpath %s", rp->basedir);

	rp->ta = ta_get(tal);

	/* need to check if it was already loaded */
	if (repo_state(rp) != REPO_LOADING)
		entityq_flush(&rp->queue, rp);

	return rp;
}

/*
 * Look up a repository, queueing it for discovery if not found.
 */
struct repo *
repo_lookup(int talid, const char *uri, const char *notify)
{
	struct repo	*rp;
	char		*repouri;

	if ((repouri = rsync_base_uri(uri)) == NULL)
		errx(1, "bad caRepository URI: %s", uri);

	/* Look up in repository table. */
	SLIST_FOREACH(rp, &repos, entry) {
		if (strcmp(rp->repouri, repouri) != 0)
			continue;
		if (rp->notifyuri != NULL) {
			if (notify == NULL)
				continue;
			if (strcmp(rp->notifyuri, notify) != 0)
				continue;
		} else if (notify != NULL)
			continue;
		/* found matching repo */
		free(repouri);
		return rp;
	}

	rp = repo_alloc(talid);
	rp->basedir = repo_dir(repouri, NULL, 0);
	rp->repouri = repouri;
	if (notify != NULL)
		if ((rp->notifyuri = strdup(notify)) == NULL)
			err(1, NULL);

	if (++talrepocnt[talid] >= MAX_REPO_PER_TAL) {
		if (talrepocnt[talid] == MAX_REPO_PER_TAL)
			warnx("too many repositories under %s", tals[talid]);
		nofetch = 1;
	}

	/* check if sync disabled ... */
	if (noop || nofetch) {
		logx("%s: using cache", rp->basedir);
		entityq_flush(&rp->queue, rp);
		return rp;
	}

	/* try to create base directory */
	if (mkpath(rp->basedir) == -1)
		warn("mkpath %s", rp->basedir);

	/* ... else try RRDP first if available then rsync */
	if (notify != NULL)
		rp->rrdp = rrdp_get(notify);
	if (rp->rrdp == NULL)
		rp->rsync = rsync_get(uri, rp->basedir);

	/* need to check if it was already loaded */
	if (repo_state(rp) != REPO_LOADING)
		entityq_flush(&rp->queue, rp);

	return rp;
}

/*
 * Find repository by identifier.
 */
struct repo *
repo_byid(unsigned int id)
{
	struct repo	*rp;

	SLIST_FOREACH(rp, &repos, entry) {
		if (rp->id == id)
			return rp;
	}
	return NULL;
}

/*
 * Find repository by base path.
 */
static struct repo *
repo_bypath(const char *path)
{
	struct repo	*rp;

	SLIST_FOREACH(rp, &repos, entry) {
		if (strcmp(rp->basedir, path) == 0)
			return rp;
	}
	return NULL;
}

/*
 * Return the repository base or alternate directory.
 * Returned string must be freed by caller.
 */
char *
repo_basedir(const struct repo *rp, int wantvalid)
{
	char *path = NULL;

	if (!wantvalid) {
		if (rp->ta) {
			if ((path = strdup(rp->ta->basedir)) == NULL)
				err(1, NULL);
		} else if (rp->rsync) {
			if ((path = strdup(rp->rsync->basedir)) == NULL)
				err(1, NULL);
		} else if (rp->rrdp) {
			path = rrdp_filename(rp->rrdp, rp->repouri, 0);
		} else
			path = NULL;	/* only valid repo available */
	} else if (rp->basedir != NULL) {
		if ((path = strdup(rp->basedir)) == NULL)
			err(1, NULL);
	}

	return path;
}

/*
 * Return the repository identifier.
 */
unsigned int
repo_id(const struct repo *rp)
{
	return rp->id;
}

/*
 * Return the repository URI.
 */
const char *
repo_uri(const struct repo *rp)
{
	return rp->repouri;
}

int
repo_queued(struct repo *rp, struct entity *p)
{
	if (repo_state(rp) == REPO_LOADING) {
		TAILQ_INSERT_TAIL(&rp->queue, p, entries);
		return 1;
	}
	return 0;
}

static void
repo_fail(struct repo *rp)
{
	/* reset the alarm since code may fallback to rsync */
	rp->alarm = getmonotime() + repo_timeout;

	if (rp->ta)
		http_finish(rp->ta->id, HTTP_FAILED, NULL);
	else if (rp->rsync)
		rsync_finish(rp->rsync->id, 0);
	else if (rp->rrdp)
		rrdp_finish(rp->rrdp->id, 0);
	else
		errx(1, "%s: bad repo", rp->repouri);
}

static void
repo_abort(struct repo *rp)
{
	/* reset the alarm */
	rp->alarm = getmonotime() + repo_timeout;

	if (rp->rsync)
		rsync_abort(rp->rsync->id);
	else if (rp->rrdp)
		rrdp_abort(rp->rrdp->id);
	else
		repo_fail(rp);
}

int
repo_check_timeout(int timeout)
{
	struct repo	*rp;
	time_t		 now;
	int		 diff;

	now = getmonotime();

	/* check against our runtime deadline first */
	if (deadline != 0) {
		if (deadline <= now) {
			warnx("deadline reached, giving up on repository sync");
			nofetch = 1;
			/* clear deadline since nofetch is set */
			deadline = 0;
			/* increase now enough so that all pending repos fail */
			now += repo_timeout;
		} else {
			diff = deadline - now;
			diff *= 1000;
			if (timeout == INFTIM || diff < timeout)
				timeout = diff;
		}
	}
	/* Look up in repository table. (Lookup should actually fail here) */
	SLIST_FOREACH(rp, &repos, entry) {
		if (repo_state(rp) == REPO_LOADING) {
			if (rp->alarm <= now) {
				warnx("%s: synchronisation timeout",
				    rp->repouri);
				repo_abort(rp);
			} else {
				diff = rp->alarm - now;
				diff *= 1000;
				if (timeout == INFTIM || diff < timeout)
					timeout = diff;
			}
		}
	}
	return timeout;
}

/*
 * Delayed delete of files from RRDP. Since RRDP has no security built-in
 * this code needs to check if this RRDP repository is actually allowed to
 * remove the file referenced by the URI.
 */
static void
repo_cleanup_rrdp(struct filepath_tree *tree)
{
	struct rrdprepo *rr;
	struct filepath *fp, *nfp;
	char *fn;

	SLIST_FOREACH(rr, &rrdprepos, entry) {
		RB_FOREACH_SAFE(fp, filepath_tree, &rr->deleted, nfp) {
			if (!rrdp_uri_valid(rr, fp->file)) {
				warnx("%s: external URI %s", rr->notifyuri,
				    fp->file);
				filepath_put(&rr->deleted, fp);
				continue;
			}
			/* try to remove file from rrdp repo ... */
			fn = rrdp_filename(rr, fp->file, 0);

			if (unlink(fn) == -1) {
				if (errno != ENOENT)
					warn("unlink %s", fn);
			} else {
				if (verbose > 1)
					logx("deleted %s", fn);
				stats.del_files++;
			}
			free(fn);

			/* ... and from the valid repository if unused. */
			fn = rrdp_filename(rr, fp->file, 1);
			if (!filepath_exists(tree, fn)) {
				if (unlink(fn) == -1) {
					if (errno != ENOENT)
						warn("unlink %s", fn);
				} else {
					if (verbose > 1)
						logx("deleted %s", fn);
					stats.del_files++;
				}
			} else
				warnx("%s: referenced file supposed to be "
				    "deleted", fn);

			free(fn);
			filepath_put(&rr->deleted, fp);
		}
	}
}

/*
 * All files in tree are valid and should be moved to the valid repository
 * if not already there. Rename the files to the new path and readd the
 * filepath entry with the new path if successful.
 */
static void
repo_move_valid(struct filepath_tree *tree)
{
	struct filepath *fp, *nfp;
	size_t rsyncsz = strlen(".rsync/");
	size_t rrdpsz = strlen(".rrdp/");
	char *fn, *base;

	RB_FOREACH_SAFE(fp, filepath_tree, tree, nfp) {
		if (strncmp(fp->file, ".rsync/", rsyncsz) != 0 &&
		    strncmp(fp->file, ".rrdp/", rrdpsz) != 0)
			continue; /* not a temporary file path */

		if (strncmp(fp->file, ".rsync/", rsyncsz) == 0) {
			fn = fp->file + rsyncsz;
		} else {
			base = strchr(fp->file + rrdpsz, '/');
			assert(base != NULL);
			fn = base + 1;
		}

		if (repo_mkpath(AT_FDCWD, fn) == -1)
			continue;

		if (rename(fp->file, fn) == -1) {
			warn("rename %s", fp->file);
			continue;
		}

		/* switch filepath node to new path */
		RB_REMOVE(filepath_tree, tree, fp);
		base = fp->file;
		if ((fp->file = strdup(fn)) == NULL)
			err(1, NULL);
		free(base);
		if (RB_INSERT(filepath_tree, tree, fp) != NULL)
			errx(1, "%s: both possibilities of file present",
			    fp->file);
	}
}

#define	BASE_DIR	(void *)0x01
#define	RSYNC_DIR	(void *)0x02
#define	RRDP_DIR	(void *)0x03

static const struct rrdprepo *
repo_is_rrdp(struct repo *rp)
{
	/* check for special pointers first these are not a repository */
	if (rp == NULL || rp == BASE_DIR || rp == RSYNC_DIR || rp == RRDP_DIR)
		return NULL;

	if (rp->rrdp)
		return rp->rrdp->state == REPO_DONE ? rp->rrdp : NULL;
	return NULL;
}

static inline char *
skip_dotslash(char *in)
{
	if (memcmp(in, "./", 2) == 0)
		return in + 2;
	return in;
}

void
repo_cleanup(struct filepath_tree *tree, int cachefd)
{
	char *argv[2] = { ".", NULL };
	FTS *fts;
	FTSENT *e;
	const struct rrdprepo *rr;

	/* first move temp files which have been used to valid dir */
	repo_move_valid(tree);
	/* then delete files requested by rrdp */
	repo_cleanup_rrdp(tree);

	if ((fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT, NULL)) == NULL)
		err(1, "fts_open");
	errno = 0;
	while ((e = fts_read(fts)) != NULL) {
		char *path = skip_dotslash(e->fts_path);
		switch (e->fts_info) {
		case FTS_NSOK:
			if (filepath_exists(tree, path)) {
				e->fts_parent->fts_number++;
				break;
			}
			if (e->fts_parent->fts_pointer == RRDP_DIR) {
				e->fts_parent->fts_number++;
				/* handle rrdp .state files explicitly */
				if (e->fts_level == 3 &&
				    strcmp(e->fts_name, ".state") == 0)
					break;
				/* can't delete these extra files */
				stats.extra_files++;
				if (verbose > 1)
					logx("superfluous %s", path);
				break;
			}
			if (e->fts_parent->fts_pointer == RSYNC_DIR) {
				/* no need to keep rsync files */
				if (verbose > 1)
					logx("superfluous %s", path);
			}
			rr = repo_is_rrdp(e->fts_parent->fts_pointer);
			if (rr != NULL) {
				struct stat st;
				char *fn;

				if (asprintf(&fn, "%s/%s", rr->basedir,
				    path) == -1)
					err(1, NULL);

				/*
				 * If the file exists in the rrdp dir
				 * that file is newer and needs to be kept
				 * so unlink this file instead of moving
				 * it over the file in the rrdp dir.
				 */
				if (fstatat(cachefd, fn, &st, 0) == 0 &&
				    S_ISREG(st.st_mode)) {
					free(fn);
					goto unlink;
				}
				if (repo_mkpath(cachefd, fn) == 0) {
					if (renameat(AT_FDCWD, e->fts_accpath,
					    cachefd, fn) == -1)
						warn("rename %s to %s", path,
						    fn);
					else if (verbose > 1)
						logx("moved %s", path);
					stats.extra_files++;
				}
				free(fn);
			} else {
 unlink:
				if (unlink(e->fts_accpath) == -1) {
					warn("unlink %s", path);
				} else {
					if (verbose > 1)
						logx("deleted %s", path);
					stats.del_files++;
				}
			}
			break;
		case FTS_D:
			if (e->fts_level == 1) {
				if (strcmp(".rsync", e->fts_name) == 0)
					e->fts_pointer = RSYNC_DIR;
				else if (strcmp(".rrdp", e->fts_name) == 0)
					e->fts_pointer = RRDP_DIR;
				else
					e->fts_pointer = BASE_DIR;
			} else
				e->fts_pointer = e->fts_parent->fts_pointer;

			/*
			 * special handling for rrdp directories,
			 * clear them if they are not used anymore but
			 * only if rrdp is active.
			 */
			if (e->fts_pointer == RRDP_DIR && e->fts_level == 2) {
				if (!rrdp_is_active(path))
					e->fts_pointer = NULL;
			}
			if (e->fts_pointer == BASE_DIR && e->fts_level > 1) {
				e->fts_pointer = repo_bypath(path);
				if (e->fts_pointer == NULL)
					e->fts_pointer = BASE_DIR;
			}
			break;
		case FTS_DP:
			if (e->fts_level == FTS_ROOTLEVEL)
				break;
			if (e->fts_level == 1)
				/* do not remove .rsync and .rrdp */
				if (e->fts_pointer == RRDP_DIR ||
				    e->fts_pointer == RSYNC_DIR)
					break;

			e->fts_parent->fts_number += e->fts_number;

			if (e->fts_number == 0) {
				if (rmdir(e->fts_accpath) == -1)
					warn("rmdir %s", path);
				else
					stats.del_dirs++;
			}
			break;
		case FTS_SL:
		case FTS_SLNONE:
			warnx("symlink %s", path);
			if (unlink(e->fts_accpath) == -1)
				warn("unlink %s", path);
			break;
		case FTS_NS:
		case FTS_ERR:
			if (e->fts_errno == ENOENT &&
			    e->fts_level == FTS_ROOTLEVEL)
				continue;
			warnx("fts_read %s: %s", path,
			    strerror(e->fts_errno));
			break;
		default:
			warnx("fts_read %s: unhandled[%x]", path,
			    e->fts_info);
			break;
		}

		errno = 0;
	}
	if (errno)
		err(1, "fts_read");
	if (fts_close(fts) == -1)
		err(1, "fts_close");
}

void
repo_free(void)
{
	struct repo *rp;

	while ((rp = SLIST_FIRST(&repos)) != NULL) {
		SLIST_REMOVE_HEAD(&repos, entry);
		free(rp->repouri);
		free(rp->notifyuri);
		free(rp->basedir);
		free(rp);
	}

	ta_free();
	rrdp_free();
	rsync_free();
}

/*
 * Remove all files and directories under base but do not remove base itself.
 */
static void
remove_contents(char *base)
{
	char *argv[2] = { base, NULL };
	FTS *fts;
	FTSENT *e;

	if ((fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT, NULL)) == NULL)
		err(1, "fts_open");
	errno = 0;
	while ((e = fts_read(fts)) != NULL) {
		switch (e->fts_info) {
		case FTS_NSOK:
		case FTS_SL:
		case FTS_SLNONE:
			if (unlink(e->fts_accpath) == -1)
				warn("unlink %s", e->fts_path);
			break;
		case FTS_D:
			break;
		case FTS_DP:
			/* keep root directory */
			if (e->fts_level == FTS_ROOTLEVEL)
				break;
			if (rmdir(e->fts_accpath) == -1)
				warn("rmdir %s", e->fts_path);
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
