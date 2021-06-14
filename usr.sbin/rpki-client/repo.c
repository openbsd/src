/*	$OpenBSD: repo.c,v 1.8 2021/06/14 10:01:23 claudio Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <imsg.h>

#include "extern.h"

extern struct stats	stats;
extern int		noop;

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
	char			*temp;
	struct filepath_tree	 added;
	struct filepath_tree	 deleted;
	size_t			 id;
	enum repo_state		 state;
};
SLIST_HEAD(, rrdprepo)	rrdprepos = SLIST_HEAD_INITIALIZER(rrdprepos);

struct rsyncrepo {
	SLIST_ENTRY(rsyncrepo)	 entry;
	char			*repouri;
	char			*basedir;
	size_t			 id;
	enum repo_state		 state;
};
SLIST_HEAD(, rsyncrepo)	rsyncrepos = SLIST_HEAD_INITIALIZER(rsyncrepos);

struct tarepo {
	SLIST_ENTRY(tarepo)	 entry;
	char			*descr;
	char			*basedir;
	char			*temp;
	char			**uri;
	size_t			 urisz;
	size_t			 uriidx;
	size_t			 id;
	enum repo_state		 state;
};
SLIST_HEAD(, tarepo)	tarepos = SLIST_HEAD_INITIALIZER(tarepos);

struct	repo {
	SLIST_ENTRY(repo)	 entry;
	char			*repouri;	/* CA repository base URI */
	const struct rrdprepo	*rrdp;
	const struct rsyncrepo	*rsync;
	const struct tarepo	*ta;
	struct entityq		 queue;		/* files waiting for repo */
	size_t			 id;		/* identifier */
};
SLIST_HEAD(, repo)	repos = SLIST_HEAD_INITIALIZER(repos);

/* counter for unique repo id */
size_t			repoid;

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
	struct filepath needle;

	needle.file = file;
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
 * Return true if a filepath entry exists that starts with path.
 */
static int
filepath_dir_exists(struct filepath_tree *tree, char *path)
{
	struct filepath needle;
	struct filepath *res;

	needle.file = path;
	res = RB_NFIND(filepath_tree, tree, &needle);
	while (res != NULL && strstr(res->file, path) == res->file) {
		/* make sure that filepath actually is in that path */
		if (res->file[strlen(path)] == '/')
			return 1;
		res = RB_NEXT(filepath_tree, tree, res);
	}
	return 0;
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
 * prefixed with dir.
 */
static char *
hash_dir(const char *uri, const char *dir)
{
	const char hex[] = "0123456789abcdef";
	unsigned char m[SHA256_DIGEST_LENGTH];
	char hash[SHA256_DIGEST_LENGTH * 2 + 1];
	char *out;
	size_t i;

	SHA256(uri, strlen(uri), m);
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		hash[i * 2] = hex[m[i] >> 4];
		hash[i * 2 + 1] = hex[m[i] & 0xf];
	}
	hash[SHA256_DIGEST_LENGTH * 2] = '\0';

	asprintf(&out, "%s/%s", dir, hash);
	return out;
}

/*
 * Function to build the directory name based on URI and a directory
 * as prefix. Skip the proto:// in URI but keep everything else.
 */
static char *
rsync_dir(const char *uri, const char *dir)
{
	char *local, *out;

	local = strchr(uri, ':') + strlen("://");

	asprintf(&out, "%s/%s", dir, local);
	return out;
}

/*
 * Function to create all missing directories to a path.
 * This functions alters the path temporarily.
 */
static int
repo_mkpath(char *file)
{
	char *slash;

	/* build directory hierarchy */
	slash = strrchr(file, '/');
	assert(slash != NULL);
	*slash = '\0';
	if (mkpath(file) == -1) {
		warn("mkpath %s", file);
		return -1;
	}
	*slash = '/';
	return 0;
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
	    temp ? ".XXXXXXXX": "") == -1)
		err(1, NULL);

	return nfile;
}

/*
 * Build local file name base on the URI and the rrdprepo info.
 */
static char *
rrdp_filename(const struct rrdprepo *rr, const char *uri, int temp)
{
	char *nfile;
	char *dir = rr->basedir;

	if (temp)
		dir = rr->temp;

	if (!valid_uri(uri, strlen(uri), "rsync://")) {
		warnx("%s: bad URI %s", rr->basedir, uri);
		return NULL;
	}

	uri += strlen("rsync://");	/* skip proto */
	if (asprintf(&nfile, "%s/%s", dir, uri) == -1)
		err(1, NULL);
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
	    temp ? ".XXXXXXXX": "") == -1)
		err(1, NULL);

	return nfile;
}



static void
ta_fetch(struct tarepo *tr)
{
	logx("ta/%s: pulling from %s", tr->descr, tr->uri[tr->uriidx]);

	if (strncasecmp(tr->uri[tr->uriidx], "rsync://", 8) == 0) {
		/*
		 * Create destination location.
		 * Build up the tree to this point.
		 */
		rsync_fetch(tr->id, tr->uri[tr->uriidx], tr->basedir);
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

	if (tal->urisz == 0)
		errx(1, "TAL %s has no URI", tal->descr);

	if ((tr = calloc(1, sizeof(*tr))) == NULL)
		err(1, NULL);
	tr->id = ++repoid;
	SLIST_INSERT_HEAD(&tarepos, tr, entry);

	if ((tr->descr = strdup(tal->descr)) == NULL)
		err(1, NULL);
	if (asprintf(&tr->basedir, "ta/%s", tal->descr) == -1)
		err(1, NULL);

	/* steal URI infromation from TAL */
	tr->urisz = tal->urisz;
	tr->uri = tal->uri;
	tal->urisz = 0;
	tal->uri = NULL;

	if (noop) {
		tr->state = REPO_DONE;
		logx("ta/%s: using cache", tr->descr);
		/* there is nothing in the queue so no need to flush */
	} else {
		/* try to create base directory */
		if (mkpath(tr->basedir) == -1)
			warn("mkpath %s", tr->basedir);

		ta_fetch(tr);
	}

	return tr;
}

static struct tarepo *
ta_find(size_t id)
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
rsync_get(const char *uri)
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
	rr->basedir = rsync_dir(repo, "rsync");

	if (noop) {
		rr->state = REPO_DONE;
		logx("%s: using cache", rr->basedir);
		/* there is nothing in the queue so no need to flush */
	} else {
		/* create base directory */
		if (mkpath(rr->basedir) == -1) {
			warn("mkpath %s", rr->basedir);
			rsync_finish(rr->id, 0);
			return rr;
		}

		logx("%s: pulling from %s", rr->basedir, rr->repouri);
		rsync_fetch(rr->id, rr->repouri, rr->basedir);
	}

	return rr;
}

static struct rsyncrepo *
rsync_find(size_t id)
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

static int rrdprepo_fetch(struct rrdprepo *);

static struct rrdprepo *
rrdp_get(const char *uri)
{
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
	rr->basedir = hash_dir(uri, "rrdp");

	RB_INIT(&rr->added);
	RB_INIT(&rr->deleted);

	if (noop) {
		rr->state = REPO_DONE;
		logx("%s: using cache", rr->notifyuri);
		/* there is nothing in the queue so no need to flush */
	} else {
		/* create base directory */
		if (mkpath(rr->basedir) == -1) {
			warn("mkpath %s", rr->basedir);
			rrdp_finish(rr->id, 0);
			return rr;
		}
		if (rrdprepo_fetch(rr) == -1) {
			rrdp_finish(rr->id, 0);
			return rr;
		}

		logx("%s: pulling from %s", rr->notifyuri, "network");
	}

	return rr;
}

static struct rrdprepo *
rrdp_find(size_t id)
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
		free(rr->temp);

		filepath_free(&rr->added);
		filepath_free(&rr->deleted);

		free(rr);
	}
}

static struct rrdprepo *
rrdp_basedir(const char *dir)
{
	struct rrdprepo *rr;

	SLIST_FOREACH(rr, &rrdprepos, entry)
		if (strcmp(dir, rr->basedir) == 0) {
			if (rr->state == REPO_FAILED)
				return NULL;
			return rr;
		}

	return NULL;
}

/*
 * Allocate and insert a new repository.
 */
static struct repo *
repo_alloc(void)
{
	struct repo *rp;

	if ((rp = calloc(1, sizeof(*rp))) == NULL)
		err(1, NULL);

	rp->id = ++repoid;
	TAILQ_INIT(&rp->queue);
	SLIST_INSERT_HEAD(&repos, rp, entry);

	stats.repos++;
	return rp;
}

/*
 * Return the state of a repository.
 */
static enum repo_state
repo_state(struct repo *rp)
{
	if (rp->ta)
		return rp->ta->state;
	if (rp->rrdp)
		return rp->rrdp->state;
	if (rp->rsync)
		return rp->rsync->state;
	errx(1, "%s: bad repo", rp->repouri);
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
rrdp_save_state(size_t id, struct rrdp_session *state)
{
	struct rrdprepo *rr;
	char *temp, *file;
	FILE *f;
	int fd;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "non-existant rrdp repo %zu", id);

	file = rrdp_state_filename(rr, 0);
	temp = rrdp_state_filename(rr, 1);

	if ((fd = mkostemp(temp, O_CLOEXEC)) == -1) {
		warn("mkostemp %s", temp);
		goto fail;
	}
	(void) fchmod(fd, 0644);
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

/*
 * Write a file into the temporary RRDP dir but only after checking
 * its hash (if required). The function also makes sure that the file
 * tracking is properly adjusted.
 * Returns 1 on success, 0 if the repo is corrupt, -1 on IO error
 */
int
rrdp_handle_file(size_t id, enum publish_type pt, char *uri,
    char *hash, size_t hlen, char *data, size_t dlen)
{
	struct rrdprepo *rr;
	struct filepath *fp;
	ssize_t s;
	char *fn;
	int fd = -1;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "non-existant rrdp repo %zu", id);
	if (rr->state == REPO_FAILED)
		return -1;

	if (pt == PUB_UPD || pt == PUB_DEL) {
		if (filepath_exists(&rr->deleted, uri)) {
			warnx("%s: already deleted", uri);
			return 0;
		}
		fp = filepath_find(&rr->added, uri);
		if (fp == NULL) {
			if ((fn = rrdp_filename(rr, uri, 0)) == NULL)
				return 0;
		} else {
			filepath_put(&rr->added, fp);
			if ((fn = rrdp_filename(rr, uri, 1)) == NULL)
				return 0;
		}
		if (!valid_filehash(fn, hash, hlen)) {
			warnx("%s: bad message digest", fn);
			free(fn);
			return 0;
		}
		free(fn);
	}

	if (pt == PUB_DEL) {
		filepath_add(&rr->deleted, uri);
	} else {
		fp = filepath_find(&rr->deleted, uri);
		if (fp != NULL)
			filepath_put(&rr->deleted, fp);

		/* add new file to temp dir */
		if ((fn = rrdp_filename(rr, uri, 1)) == NULL)
			return 0;

		if (repo_mkpath(fn) == -1)
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
		filepath_add(&rr->added, uri);
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
 * Initiate a RRDP sync, create the required temporary directory and
 * parse a possible state file before sending the request to the RRDP process.
 */
static int
rrdprepo_fetch(struct rrdprepo *rr)
{
	struct rrdp_session state = { 0 };

	if (asprintf(&rr->temp, "%s.XXXXXXXX", rr->basedir) == -1)
		err(1, NULL);
	if (mkdtemp(rr->temp) == NULL) {
		warn("mkdtemp %s", rr->temp);
		return -1;
	}

	rrdp_parse_state(rr, &state);
	rrdp_fetch(rr->id, rr->notifyuri, rr->notifyuri, &state);

	free(state.session_id);
	free(state.last_mod);

	return 0;
}

static int
rrdp_merge_repo(struct rrdprepo *rr)
{
	struct filepath *fp, *nfp;
	char *fn, *rfn;

	RB_FOREACH_SAFE(fp, filepath_tree, &rr->added, nfp) {
		fn = rrdp_filename(rr, fp->file, 1);
		rfn = rrdp_filename(rr, fp->file, 0);

		if (fn == NULL || rfn == NULL)
			errx(1, "bad filepath");	/* should not happen */

		if (repo_mkpath(rfn) == -1) {
			goto fail;
		}

		if (rename(fn, rfn) == -1) {
			warn("rename %s", rfn);
			goto fail;
		}

		free(rfn);
		free(fn);
		filepath_put(&rr->added, fp);
	}

	return 1;

fail:
	free(rfn);
	free(fn);
	return 0;
}

static void
rrdp_clean_temp(struct rrdprepo *rr)
{
	struct filepath *fp, *nfp;
	char *fn;

	filepath_free(&rr->deleted);

	RB_FOREACH_SAFE(fp, filepath_tree, &rr->added, nfp) {
		if ((fn = rrdp_filename(rr, fp->file, 1)) != NULL) {
			if (unlink(fn) == -1)
				warn("unlink %s", fn);
			free(fn);
		}
		filepath_put(&rr->added, fp);
	}
}

/*
 * RSYNC sync finished, either with or without success.
 */
void
rsync_finish(size_t id, int ok)
{
	struct rsyncrepo *rr;
	struct tarepo *tr;
	struct repo *rp;

	tr = ta_find(id);
	if (tr != NULL) {
		if (ok) {
			logx("ta/%s: loaded from network", tr->descr);
			stats.rsync_repos++;
			tr->state = REPO_DONE;
		} else if (++tr->uriidx < tr->urisz) {
			logx("ta/%s: load from network failed, retry",
			    tr->descr);
			ta_fetch(tr);
			return;
		} else {
			logx("ta/%s: load from network failed, "
			    "fallback to cache", tr->descr);
			stats.rsync_fails++;
			tr->state = REPO_FAILED;
		}
		SLIST_FOREACH(rp, &repos, entry)
			if (rp->ta == tr)
				entityq_flush(&rp->queue, rp);

		return;
	}

	rr = rsync_find(id);
	if (rr == NULL)
		errx(1, "unknown rsync repo %zu", id);

	if (ok) {
		logx("%s: loaded from network", rr->basedir);
		stats.rsync_repos++;
		rr->state = REPO_DONE;
	} else {
		logx("%s: load from network failed, fallback to cache",
		    rr->basedir);
		stats.rsync_fails++;
		rr->state = REPO_FAILED;
	}

	SLIST_FOREACH(rp, &repos, entry)
		if (rp->rsync == rr)
			entityq_flush(&rp->queue, rp);
}

/*
 * RRDP sync finshed, either with or without success.
 */
void
rrdp_finish(size_t id, int ok)
{
	struct rrdprepo *rr;
	struct repo *rp;

	rr = rrdp_find(id);
	if (rr == NULL)
		errx(1, "unknown RRDP repo %zu", id);

	if (ok && rrdp_merge_repo(rr)) {
		logx("%s: loaded from network", rr->notifyuri);
		rr->state = REPO_DONE;
		stats.rrdp_repos++;
		SLIST_FOREACH(rp, &repos, entry)
			if (rp->rrdp == rr)
				entityq_flush(&rp->queue, rp);
	} else if (!ok) {
		rrdp_clean_temp(rr);
		stats.rrdp_fails++;
		rr->state = REPO_FAILED;
		logx("%s: load from network failed, fallback to rsync",
		    rr->notifyuri);
		SLIST_FOREACH(rp, &repos, entry)
			if (rp->rrdp == rr) {
				rp->rrdp = NULL;
				rp->rsync = rsync_get(rp->repouri);
				/* need to check if it was already loaded */
				if (repo_state(rp) != REPO_LOADING)
					entityq_flush(&rp->queue, rp);
			}
	} else {
		rrdp_clean_temp(rr);
		stats.rrdp_fails++;
		rr->state = REPO_FAILED;
		logx("%s: load from network failed", rr->notifyuri);
		SLIST_FOREACH(rp, &repos, entry)
			if (rp->rrdp == rr)
				entityq_flush(&rp->queue, rp);
	}
}

/*
 * Handle responses from the http process. For TA file, either rename
 * or delete the temporary file. For RRDP requests relay the request
 * over to the rrdp process.
 */
void
http_finish(size_t id, enum http_result res, const char *last_mod)
{
	struct tarepo *tr;
	struct repo *rp;

	tr = ta_find(id);
	if (tr == NULL) {
		/* not a TA fetch therefor RRDP */
		rrdp_http_done(id, res, last_mod);
		return;
	}

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
	} else {
		if (unlink(tr->temp) == -1 && errno != ENOENT)
			warn("unlink %s", tr->temp);

		if (++tr->uriidx < tr->urisz) {
			logx("ta/%s: load from network failed, retry",
			    tr->descr);
			ta_fetch(tr);
			return;
		}

		tr->state = REPO_FAILED;
		logx("ta/%s: load from network failed, "
		    "fallback to cache", tr->descr);
	}

	SLIST_FOREACH(rp, &repos, entry)
		if (rp->ta == tr)
			entityq_flush(&rp->queue, rp);
}



/*
 * Look up a trust anchor, queueing it for download if not found.
 */
struct repo *
ta_lookup(struct tal *tal)
{
	struct repo	*rp;

	/* Look up in repository table. (Lookup should actually fail here) */
	SLIST_FOREACH(rp, &repos, entry) {
		if (strcmp(rp->repouri, tal->descr) == 0)
			return rp;
	}

	rp = repo_alloc();
	if ((rp->repouri = strdup(tal->descr)) == NULL)
		err(1, NULL);
	rp->ta = ta_get(tal);

	return rp;
}

/*
 * Look up a repository, queueing it for discovery if not found.
 */
struct repo *
repo_lookup(const char *uri, const char *notify)
{
	struct repo *rp;

	/* Look up in repository table. */
	SLIST_FOREACH(rp, &repos, entry) {
		if (strcmp(rp->repouri, uri) != 0)
			continue;
		return rp;
	}

	rp = repo_alloc();
	if ((rp->repouri = strdup(uri)) == NULL)
		err(1, NULL);

	/* try RRDP first if available */
	if (notify != NULL)
		rp->rrdp = rrdp_get(notify);
	if (rp->rrdp == NULL)
		rp->rsync = rsync_get(uri);

	return rp;
}

/*
 * Build local file name base on the URI and the repo info.
 */
char *
repo_filename(const struct repo *rp, const char *uri)
{
	char *nfile;
	char *dir, *repouri;

	if (uri == NULL && rp->ta)
		return ta_filename(rp->ta, 0);

	assert(uri != NULL);
	if (rp->rrdp)
		return rrdp_filename(rp->rrdp, uri, 0);

	/* must be rsync */
	dir = rp->rsync->basedir;
	repouri = rp->rsync->repouri;

	if (strstr(uri, repouri) != uri) {
		warnx("%s: URI %s outside of repository", repouri, uri);
		return NULL;
	}

	uri += strlen(repouri) + 1;	/* skip base and '/' */

	if (asprintf(&nfile, "%s/%s", dir, uri) == -1)
		err(1, NULL);
	return nfile;
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

static char **
add_to_del(char **del, size_t *dsz, char *file)
{
	size_t i = *dsz;

	del = reallocarray(del, i + 1, sizeof(*del));
	if (del == NULL)
		err(1, NULL);
	if ((del[i] = strdup(file)) == NULL)
		err(1, NULL);
	*dsz = i + 1;
	return del;
}

static char **
repo_rrdp_cleanup(struct filepath_tree *tree, struct rrdprepo *rr,
    char **del, size_t *delsz)
{
	struct filepath *fp, *nfp;
	char *fn;

	RB_FOREACH_SAFE(fp, filepath_tree, &rr->deleted, nfp) {
		fn = rrdp_filename(rr, fp->file, 0);
		/* temp dir will be cleaned up by repo_cleanup() */

		if (fn == NULL)
			errx(1, "bad filepath");	/* should not happen */

		if (!filepath_exists(tree, fn))
			del = add_to_del(del, delsz, fn);
		else
			warnx("%s: referenced file supposed to be deleted", fn);

		free(fn);
		filepath_put(&rr->deleted, fp);
	}

	return del;
}

void
repo_cleanup(struct filepath_tree *tree)
{
	size_t i, cnt, delsz = 0, dirsz = 0;
	char **del = NULL, **dir = NULL;
	char *argv[4] = { "ta", "rsync", "rrdp", NULL };
	struct rrdprepo *rr;
	FTS *fts;
	FTSENT *e;

	if ((fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT, NULL)) == NULL)
		err(1, "fts_open");
	errno = 0;
	while ((e = fts_read(fts)) != NULL) {
		switch (e->fts_info) {
		case FTS_NSOK:
			if (!filepath_exists(tree, e->fts_path))
				del = add_to_del(del, &delsz,
				    e->fts_path);
			break;
		case FTS_D:
			/* special cleanup for rrdp directories */
			if ((rr = rrdp_basedir(e->fts_path)) != NULL) {
				del = repo_rrdp_cleanup(tree, rr, del, &delsz);
				if (fts_set(fts, e, FTS_SKIP) == -1)
					err(1, "fts_set");
			}
			break;
		case FTS_DP:
			if (!filepath_dir_exists(tree, e->fts_path))
				dir = add_to_del(dir, &dirsz,
				    e->fts_path);
			break;
		case FTS_SL:
		case FTS_SLNONE:
			warnx("symlink %s", e->fts_path);
			del = add_to_del(del, &delsz, e->fts_path);
			break;
		case FTS_NS:
		case FTS_ERR:
			if (e->fts_errno == ENOENT &&
			    (strcmp(e->fts_path, "rsync") == 0 ||
			    strcmp(e->fts_path, "rrdp") == 0))
				continue;
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

	cnt = 0;
	for (i = 0; i < delsz; i++) {
		if (unlink(del[i]) == -1) {
			if (errno != ENOENT)
				warn("unlink %s", del[i]);
		} else {
			if (verbose > 1)
				logx("deleted %s", del[i]);
			cnt++;
		}
		free(del[i]);
	}
	free(del);
	stats.del_files = cnt;

	cnt = 0;
	for (i = 0; i < dirsz; i++) {
		if (rmdir(dir[i]) == -1)
			warn("rmdir %s", dir[i]);
		else
			cnt++;
		free(dir[i]);
	}
	free(dir);
	stats.del_dirs = cnt;
}

void
repo_free(void)
{
	struct repo *rp;

	while ((rp = SLIST_FIRST(&repos)) != NULL) {
		SLIST_REMOVE_HEAD(&repos, entry);
		free(rp->repouri);
		free(rp);
	}

	ta_free();
	rrdp_free();
	rsync_free();
}
