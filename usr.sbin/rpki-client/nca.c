/*	$OpenBSD: nca.c,v 1.11 2026/07/09 12:02:23 job Exp $ */
/*
 * Copyright (c) 2026 Job Snijders <job@bsd.nl>
 * Copyright (c) 2025 Theo Buehler <tb@openbsd.org>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

extern int rrdpon;

/*
 * Add a given CA cert into the non-functional CA tree.
 * Return 1 if a synchronization attempt is to be made, 0 otherwise.
 */
static int
nca_tree_insert_cert(struct nca_tree *tree, const struct cert *cert,
    const struct nca_hist *nca_hist)
{
	struct nonfunc_ca *nca;
	time_t now = get_current_time();

	if ((nca = calloc(1, sizeof(*nca))) == NULL)
		err(1, NULL);
	if ((nca->aki = strdup(cert->aki)) == NULL)
		err(1, NULL);
	if ((nca->ski = strdup(cert->ski)) == NULL)
		err(1, NULL);
	if ((nca->location = strdup(cert->path)) == NULL)
		err(1, NULL);
	if ((nca->carepo = strdup(cert->repo)) == NULL)
		err(1, NULL);
	if ((nca->mfturi = strdup(cert->mft)) == NULL)
		err(1, NULL);
	if (cert->notify != NULL) {
		if ((nca->notify = strdup(cert->notify)) == NULL)
			err(1, NULL);
	}
	nca->repoid = cert->repoid;
	nca->certid = cert->certid;
	nca->talid = cert->talid;

	/* default initial state */
	nca->since = now;
	nca->last_attempt = now;
	nca->attempts = 1;
	nca->defer = 0;

	if (nca_hist != NULL) {
		nca->since = nca_hist->since;
		nca->last_attempt = nca_hist->last_attempt;
		nca->attempts = nca_hist->attempts;
		nca->defer = nca_hist->defer;

		if (retry_all_ncas || nca->defer == 0) {
			if (verbose) {
				warnx("%s: retrying, non-functional since %s",
				    cert->path, time2str(nca->since));
			}
			nca->attempts++;
			nca->last_attempt = now;
		} else if (verbose > 1) {
			warnx("%s: deferring sync, non-functional since"
			    " %s", cert->path, time2str(nca->since));
		}
	}

	if (RB_INSERT(nca_tree, tree, nca) != NULL)
		errx(1, "non-functional CA tree corrupted");

	return nca->defer;
}

void
nca_tree_remove_cert(struct nca_tree *tree, int cid)
{
	struct nonfunc_ca *found, needle = { .certid = cid };

	if ((found = RB_FIND(nca_tree, tree, &needle)) != NULL) {
		RB_REMOVE(nca_tree, tree, found);
		free(found->aki);
		free(found->ski);
		free(found->location);
		free(found->carepo);
		free(found->mfturi);
		free(found->notify);
		free(found);
	}
}

static inline int
certidcmp(const struct nonfunc_ca *a, const struct nonfunc_ca *b)
{
	if (a->certid < b->certid)
		return -1;
	if (a->certid > b->certid)
		return 1;

	return 0;
}

RB_GENERATE(nca_tree, nonfunc_ca, entry, certidcmp);

static struct strlist batchlist = LIST_HEAD_INITIALIZER(batchlist);

static RB_HEAD(nca_hist_tree, nca_hist) ncas_hist = RB_INITIALIZER(&ncas_hist);

static inline int
nca_hist_cmp(const struct nca_hist *a, const struct nca_hist *b)
{
	int cmp;

	cmp = strcmp(a->aki, b->aki);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(a->ski, b->ski);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(a->location, b->location);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(a->mfturi, b->mfturi);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	return 0;
}

RB_GENERATE_STATIC(nca_hist_tree, nca_hist, entry, nca_hist_cmp);

static void
nca_hist_free(struct nca_hist *nca_hist)
{
	if (nca_hist == NULL)
		return;

	free(nca_hist->aki);
	free(nca_hist->ski);
	free(nca_hist->location);
	free(nca_hist->mfturi);
	free(nca_hist->baseuri);
	free(nca_hist->notify);
	free(nca_hist);
}

static void
nca_hist_tree_free(void)
{
	struct nca_hist *nca_hist, *nca_hist_tmp;

	RB_FOREACH_SAFE(nca_hist, nca_hist_tree, &ncas_hist, nca_hist_tmp) {
		RB_REMOVE(nca_hist_tree, &ncas_hist, nca_hist);
		nca_hist_free(nca_hist);
	}
}

/*
 * To control the rate of synchronization attempts for non-functional CAs
 * decide whether to schedule a retry or defer it.
 * Return 1 to schedule a retry, 0 otherwise.
 */
static int
nca_decide_retry(const struct nca_hist *nca_hist)
{
	time_t now = get_current_time();

	/* First, just retry a few times consecutively. */
	if (nca_hist->attempts < 3)
		return 1;

	/* Then, insert 90 minute pauses between the retries. */
	if ((now - nca_hist->since < 24 * 60 * 60) &&
	    (now > nca_hist->last_attempt + 90 * 60))
		return 1;

	/* Add jitter to spread the retries around in time. */
	if (arc4random() & 0x1)
		return 0;

	/*
	 * After 24 hours, settle on retrying only once per day (modulo any
	 * RRDP rpkiNotify-based batching, as arranged by the caller).
	 */
	if ((now - nca_hist->since > 24 * 60 * 60) &&
	    (now - nca_hist->last_attempt > 24 * 60 * 60))
		return 1;

	return 0;
}

/*
 * Determine which non-functioncal CAs are eligible for retry.
 * If an NCA is eligible for retry, batch it together with all NCAs sharing
 * its rsync baseURI or RRDP rpkiNotify URI.
 */
static void
ncas_plan_retries(void)
{
	struct nca_hist *nca_hist;
	struct strlistentry *sle, *sle_tmp;

	RB_FOREACH(nca_hist, nca_hist_tree, &ncas_hist) {
		if (nca_decide_retry(nca_hist) == 0) {
			nca_hist->defer = 1;
			continue;
		}

		strlist_insert(&batchlist, nca_hist->baseuri);

		if (nca_hist->notify != NULL && rrdpon)
			strlist_insert(&batchlist, nca_hist->notify);
	}

	RB_FOREACH(nca_hist, nca_hist_tree, &ncas_hist) {
		if (strlist_find(&batchlist, nca_hist->baseuri,
		    strlen(nca_hist->baseuri))) {
			nca_hist->defer = 0;
			continue;
		}

		if (nca_hist->notify != NULL && rrdpon) {
			if (strlist_find(&batchlist, nca_hist->notify,
			    strlen(nca_hist->notify))) {
				nca_hist->defer = 0;
			}
		}
	}

	LIST_FOREACH_SAFE(sle, &batchlist, entry, sle_tmp) {
		LIST_REMOVE(sle, entry);
		free(sle->str);
		free(sle);
	}
}

void
nca_history_load(void)
{
	FILE *f;
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;
	const char *errstr;
	struct nca_hist *nca_hist = NULL;
	time_t now;

	now = get_current_time();

	if ((f = fopen(NCA_HISTORY, "r")) == NULL) {
		if (errno == ENOENT)
			return;
		err(1, "failed to open %s", NCA_HISTORY);
	}

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		char *l, *aki, *ski, *loc, *mfturi, *notify,
		    *since, *last_attempt, *attempts;
		size_t loc_len, mfturi_len, notify_len;

		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		if ((nca_hist = calloc(1, sizeof(*nca_hist))) == NULL)
			err(1, NULL);

		l = line;

		if ((aki = strsep(&l, " ")) == NULL)
			goto err;
		if ((nca_hist->aki = strdup(aki)) == NULL)
			err(1, NULL);

		if ((ski = strsep(&l, " ")) == NULL)
			goto err;
		if ((nca_hist->ski = strdup(ski)) == NULL)
			err(1, NULL);

		if ((since = strsep(&l, " ")) == NULL)
			goto err;
		nca_hist->since = strtonum(since, 1, LLONG_MAX, &errstr);
		if (errstr != NULL)
			goto err;
		if (nca_hist->since > now)
			goto err;

		if ((last_attempt = strsep(&l, " ")) == NULL)
			goto err;
		nca_hist->last_attempt = strtonum(last_attempt, 1,
		    LLONG_MAX, &errstr);
		if (errstr != NULL)
			goto err;
		if (nca_hist->last_attempt > now)
			goto err;

		if ((attempts = strsep(&l, " ")) == NULL)
			goto err;
		nca_hist->attempts = strtonum(attempts, 1, LLONG_MAX,
		    &errstr);
		if (errstr != NULL)
			goto err;

		if ((loc = strsep(&l, " ")) == NULL)
			goto err;

		/* minimal example cert location: ab.cd/a/b.cer */
		if ((loc_len = strlen(loc)) < 13)
			goto err;
		if (strcmp(loc + loc_len - 4, ".cer") != 0)
			goto err;
		if (!valid_uri(loc, strlen(loc), NULL))
			goto err;
		if ((nca_hist->location = strdup(loc)) == NULL)
			err(1, NULL);

		if ((mfturi = strsep(&l, " ")) == NULL)
			goto err;

		/* minimal example mft location: rsync://a.bc/d/e.mft */
		if ((mfturi_len = strlen(mfturi)) < 20)
			goto err;
		if (strcmp(mfturi + mfturi_len - 4, ".mft") != 0)
			goto err;
		if (!valid_uri(mfturi, strlen(mfturi), RSYNC_PROTO))
			goto err;
		if (!rsync_base_uri(mfturi, &nca_hist->baseuri))
			goto err;
		if ((nca_hist->mfturi = strdup(mfturi)) == NULL)
			err(1, NULL);

		notify = l;
		if (notify == NULL)
			goto err;

		/* example rpkiNotify: '-' or 'https://a.bc/d.xml' */
		if (strcmp("-", notify) != 0) {
			if ((notify_len = strlen(notify)) < 18)
				goto err;
			if (strcmp(notify + notify_len - 4, ".xml") != 0)
				goto err;
			if (!valid_uri(notify, strlen(notify), HTTPS_PROTO))
				goto err;
			if ((nca_hist->notify = strdup(notify)) == NULL)
				err(1, NULL);
		}

		if (RB_INSERT(nca_hist_tree, &ncas_hist, nca_hist) != NULL)
			err(1, "ncas_hist_tree corrupted");
	}

	if (ferror(f))
		goto err;

	fclose(f);
	free(line);

	ncas_plan_retries();

	return;

 err:
	warnx("error reading %s", NCA_HISTORY);
	fclose(f);
	unlink(NCA_HISTORY);

	free(line);

	nca_hist_free(nca_hist);

	nca_hist_tree_free();
}

/*
 * Look up history and synchronization eligibility for a given CA, if any.
 * Return 1 to skip the synchronization attempt, otherwise 0.
 */
int
nca_skip_sync(struct nca_tree *ncas, const struct cert *cert)
{
	struct nca_hist *nca_hist, needle;

	if (cert->purpose == CERT_PURPOSE_TA)
		return 0;

	needle.aki = cert->aki;
	needle.ski = cert->ski;
	needle.location = cert->path;
	needle.mfturi = cert->mft;
	nca_hist = RB_FIND(nca_hist_tree, &ncas_hist, &needle);

	return nca_tree_insert_cert(ncas, cert, nca_hist);
}

static int
ncas_sorted_cmp(const void *a, const void *b)
{
	int cmp;
	const struct nonfunc_ca *na = *(const struct nonfunc_ca **)a;
	const struct nonfunc_ca *nb = *(const struct nonfunc_ca **)b;

	cmp = strcmp(na->aki, nb->aki);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(na->mfturi, nb->mfturi);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(na->location, nb->location);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	cmp = strcmp(na->ski, nb->ski);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	return 0;
}

void
nca_history_save(struct nca_tree *ncas, time_t buildtime)
{
	char temp[] = NCA_HISTORY ".XXXXXXXX";
	FILE *f = NULL;
	int fd;
	struct nonfunc_ca *nca, **ncas_sorted = NULL;
	size_t ncas_num = 0, idx = 0;
	struct timespec ts[2];

	if (RB_EMPTY(ncas)) {
		unlink(NCA_HISTORY);
		return;
	}

	if ((fd = mkostemp(temp, O_CLOEXEC)) == -1)
		goto err;
	(void)fchmod(fd, 0644);

	if ((f = fdopen(fd, "w")) == NULL)
		err(1, "fopen");

	RB_FOREACH(nca, nca_tree, ncas)
		ncas_num++;

	if ((ncas_sorted = calloc(ncas_num, sizeof(ncas_sorted[0]))) == NULL)
		err(1, NULL);

	RB_FOREACH(nca, nca_tree, ncas)
		ncas_sorted[idx++] = nca;

	qsort(ncas_sorted, ncas_num, sizeof(ncas_sorted[0]), ncas_sorted_cmp);

	for (idx = 0; idx < ncas_num; idx++) {
		nca = ncas_sorted[idx];

		if (fprintf(f, "%s %s %lld %lld %d %s %s", nca->aki, nca->ski,
		    (long long)nca->since, (long long)nca->last_attempt,
		    nca->attempts, nca->location, nca->mfturi) < 0)
			goto err;

		if (nca->notify == NULL) {
			if (fprintf(f, " -\n") < 0)
				goto err;
		} else {
			if (fprintf(f, " %s\n", nca->notify) < 0)
				goto err;
		}
	}

	if (fclose(f) != 0) {
		f = NULL;
		goto err;
	}
	f = NULL;

	ts[0].tv_nsec = UTIME_OMIT;
	ts[1].tv_sec = buildtime;
	ts[1].tv_nsec = 0;

	if (utimensat(AT_FDCWD, temp, ts, 0) == -1)
		goto err;

	if (rename(temp, NCA_HISTORY) == -1)
		goto err;

	free(ncas_sorted);
	nca_hist_tree_free();

	return;

 err:
	warn("error saving non-functional CA history to %s", temp);
	if (f != NULL)
		fclose(f);
	unlink(temp);
	free(ncas_sorted);
	nca_hist_tree_free();
}
