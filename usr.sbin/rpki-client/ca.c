/*	$OpenBSD: ca.c,v 1.12 2026/10/27 06:15:48 tb Exp $ */
/*
 * Copyright (c) 2026 Job Snijders <job@bsd.nl>
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
#include <sys/tree.h>

#include <assert.h>
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

static void ca_free(struct ca *);
static void ca_set_state_from_last_run(struct ca *, const struct cert *);

static inline int
certidcmp(const struct ca *a, const struct ca *b)
{
	if (a->certid < b->certid)
		return -1;
	if (a->certid > b->certid)
		return 1;
	return 0;
}

RB_GENERATE(ca_tree, ca, entry, certidcmp);

static struct ca *
ca_lookup_by_certid(struct ca_tree *ca_tree, int certid)
{
	struct ca needle = { .certid = certid };

	return RB_FIND(ca_tree, ca_tree, &needle);
}

/*
 * Add metadata for a given CA cert into the CA tree.
 */
struct ca *
ca_tree_insert_cert(struct ca_tree *ca_tree, const struct cert *cert)
{
	struct ca *ca, *old_ca;
	time_t now = get_current_time();

	if ((ca = calloc(1, sizeof(*ca))) == NULL)
		err(1, NULL);

	if (cert->purpose != CERT_PURPOSE_TA) {
		if ((ca->aki = strdup(cert->aki)) == NULL)
			err(1, NULL);
	}

	if ((ca->ski = strdup(cert->ski)) == NULL)
		err(1, NULL);
	if ((ca->location = strdup(cert->path)) == NULL)
		err(1, NULL);
	if ((ca->carepo = strdup(cert->repo)) == NULL)
		err(1, NULL);
	if ((ca->mfturi = strdup(cert->mft)) == NULL)
		err(1, NULL);
	if (cert->notify != NULL) {
		if ((ca->notify = strdup(cert->notify)) == NULL)
			err(1, NULL);
	}
	ca->repoid = cert->repoid;
	ca->certid = cert->certid;
	ca->talid = cert->talid;
	ca->purpose = cert->purpose;

	ca->since = now;
	ca->last_attempt = now;
	ca->attempts = 1;
	ca->defer = 0;
	if (cert->purpose != CERT_PURPOSE_TA)
		ca->nonfunc = 1;

	ca->confirmed = 0;

	if (cert->purpose == CERT_PURPOSE_TA) {
		ca->confirmed_in_last_run = 1;
		ca->issuer = NULL;
	} else {
		ca_set_state_from_last_run(ca, cert);
		ca->issuer = ca_lookup_by_certid(ca_tree, cert->issuerid);
		if (ca->issuer == NULL)
			errx(1, "ca tree corrupted");
	}

	if ((old_ca = RB_INSERT(ca_tree, ca_tree, ca)) != NULL) {
		warnx("duplicate CA at %s", ca->location);
		ca_free(ca);
		return old_ca;
	}

	return ca;
}

static void
ca_free(struct ca *ca)
{
	if (ca == NULL)
		return;

	free(ca->aki);
	free(ca->ski);
	free(ca->location);
	free(ca->carepo);
	free(ca->mfturi);
	free(ca->baseuri);
	free(ca->notify);
	free(ca);
}

void
ca_set_confirmed(struct ca_tree *ca_tree, int certid)
{
	struct ca *ca;

	if ((ca = ca_lookup_by_certid(ca_tree, certid)) == NULL)
		errx(1, "ca_tree corrupted");

	do {
		if (ca->confirmed)
			return;

		ca->confirmed = 1;

		if (!ca->confirmed_in_last_run)
			ca->confirmed_since = get_current_time();

	} while ((ca = ca->issuer) != NULL);
}

int
ca_confirmed_in_last_run(struct ca_tree *ca_tree, const struct cert *cert)
{
	struct ca *ca;

	if ((ca = ca_lookup_by_certid(ca_tree, cert->certid)) != NULL)
		return ca->confirmed_in_last_run;

	return 0;
}

void
ca_clear_nonfunc(struct ca_tree *ca_tree, int cid)
{
	struct ca *found;

	if ((found = ca_lookup_by_certid(ca_tree, cid)) != NULL)
		found->nonfunc = 0;
}

static inline int
ca_cmp(const struct ca *a, const struct ca *b)
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

static RB_HEAD(ca_state_tree, ca) ca_state_tree \
    = RB_INITIALIZER(&ca_state_tree);

RB_GENERATE_STATIC(ca_state_tree, ca, entry, ca_cmp);

static void
ca_set_state_from_last_run(struct ca *ca, const struct cert *cert)
{
	struct ca *found;

	assert(ca->purpose == CERT_PURPOSE_CA);

	if ((found = RB_FIND(ca_state_tree, &ca_state_tree, ca)) == NULL)
		return;

	ca->confirmed_since = found->confirmed_since;
	ca->confirmed_in_last_run = found->confirmed_in_last_run;
}

static void
ca_state_tree_free(void)
{
	struct ca *ca, *ca_tmp;

	RB_FOREACH_SAFE(ca, ca_state_tree, &ca_state_tree, ca_tmp) {
		RB_REMOVE(ca_state_tree, &ca_state_tree, ca);
		ca_free(ca);
	}
}

static RB_HEAD(nca_tree, ca) nca_tree = RB_INITIALIZER(&nca_tree);

RB_GENERATE_STATIC(nca_tree, ca, entry, ca_cmp);

static void
nca_tree_free(void)
{
	struct ca *nca, *nca_tmp;

	RB_FOREACH_SAFE(nca, nca_tree, &nca_tree, nca_tmp) {
		RB_REMOVE(nca_tree, &nca_tree, nca);
		ca_free(nca);
	}
}

/*
 * To control the rate of synchronization attempts for non-functional CAs
 * decide whether to schedule a retry or defer it.
 * Return 1 to schedule a retry, 0 otherwise.
 */
static int
nca_decide_retry(const struct ca *nca)
{
	time_t now = get_current_time();

	/* First, just retry a few times consecutively. */
	if (nca->attempts < 3)
		return 1;

	/* Then, insert 90 minute pauses between the retries. */
	if ((now - nca->since < 24 * 60 * 60) &&
	    (now > nca->last_attempt + 90 * 60))
		return 1;

	/* Add jitter to spread the retries around in time. */
	if (arc4random() & 0x1)
		return 0;

	/*
	 * After 24 hours, settle on retrying only once per day (modulo any
	 * RRDP rpkiNotify-based batching, as arranged by the caller).
	 */
	if ((now - nca->since > 24 * 60 * 60) &&
	    (now - nca->last_attempt > 24 * 60 * 60))
		return 1;

	return 0;
}

static struct strlist batchlist = LIST_HEAD_INITIALIZER(batchlist);

/*
 * Determine which non-functioncal CAs are eligible for retry.
 * If an NCA is eligible for retry, batch it together with all NCAs sharing
 * its rsync baseURI or RRDP rpkiNotify URI.
 */
static void
ncas_plan_retries(void)
{
	struct ca *nca;
	struct strlistentry *sle, *sle_tmp;

	RB_FOREACH(nca, nca_tree, &nca_tree) {
		if (nca_decide_retry(nca) == 0) {
			nca->defer = 1;
			continue;
		}

		strlist_insert(&batchlist, nca->baseuri);

		if (nca->notify != NULL && rrdpon)
			strlist_insert(&batchlist, nca->notify);
	}

	RB_FOREACH(nca, nca_tree, &nca_tree) {
		if (strlist_find(&batchlist, nca->baseuri,
		    strlen(nca->baseuri))) {
			nca->defer = 0;
			continue;
		}

		if (nca->notify != NULL && rrdpon) {
			if (strlist_find(&batchlist, nca->notify,
			    strlen(nca->notify))) {
				nca->defer = 0;
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
	struct ca *nca = NULL;
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

		if ((nca = calloc(1, sizeof(*nca))) == NULL)
			err(1, NULL);

		l = line;

		if ((aki = strsep(&l, " ")) == NULL)
			goto err;
		if ((nca->aki = strdup(aki)) == NULL)
			err(1, NULL);

		if ((ski = strsep(&l, " ")) == NULL)
			goto err;
		if ((nca->ski = strdup(ski)) == NULL)
			err(1, NULL);

		if ((since = strsep(&l, " ")) == NULL)
			goto err;
		nca->since = strtonum(since, 1, LLONG_MAX, &errstr);
		if (errstr != NULL)
			goto err;
		if (nca->since > now)
			goto err;

		if ((last_attempt = strsep(&l, " ")) == NULL)
			goto err;
		nca->last_attempt = strtonum(last_attempt, 1, LLONG_MAX,
		    &errstr);
		if (errstr != NULL)
			goto err;
		if (nca->last_attempt > now)
			goto err;

		if ((attempts = strsep(&l, " ")) == NULL)
			goto err;
		nca->attempts = strtonum(attempts, 1, LLONG_MAX, &errstr);
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
		if ((nca->location = strdup(loc)) == NULL)
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
		if (!rsync_base_uri(mfturi, &nca->baseuri))
			goto err;
		if ((nca->mfturi = strdup(mfturi)) == NULL)
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
			if ((nca->notify = strdup(notify)) == NULL)
				err(1, NULL);
		}

		if (RB_INSERT(nca_tree, &nca_tree, nca) != NULL) {
			warnx("duplicate nca_tree entry for %s", nca->location);
			ca_free(nca);
			nca = NULL;
		}
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

	ca_free(nca);

	nca_tree_free();
}

/*
 * Look up history and synchronization eligibility for a given CA, if any.
 * Return 1 to skip the synchronization attempt, otherwise 0.
 */
int
nca_skip_sync(struct ca *ca)
{
	struct ca *nca, needle;

	if (ca->purpose == CERT_PURPOSE_TA)
		return 0;

	needle.aki = ca->aki;
	needle.ski = ca->ski;
	needle.location = ca->location;
	needle.mfturi = ca->mfturi;

	if ((nca = RB_FIND(nca_tree, &nca_tree, &needle)) == NULL)
		return 0;

	/* determined by ncas_plan_retries */
	ca->defer = nca->defer;

	/* state from the previous run */
	ca->since = nca->since;
	ca->last_attempt = nca->last_attempt;
	ca->attempts = nca->attempts;

	if (retry_all_ncas || ca->defer == 0) {
		if (verbose) {
			warnx("%s: retrying, non-functional since %s",
			    ca->location, time2str(ca->since));
		}
		ca->attempts++;
		ca->last_attempt = get_current_time();
	} else if (verbose > 1) {
		warnx("%s: deferring sync, non-functional since %s",
		    ca->location, time2str(ca->since));
	}

	return ca->defer;
}

static int
ca_state_write(const char *fn, time_t buildtime, struct ca **cas,
    size_t num_cas, int (*ca_print)(FILE *, const struct ca *))
{
	char *temp = NULL;
	FILE *f = NULL;
	size_t idx;
	int fd, ret = 0;
	struct timespec ts[2];

	if (asprintf(&temp, "%s.XXXXXXXX", fn) == -1)
		err(1, NULL);

	if ((fd = mkostemp(temp, O_CLOEXEC)) == -1)
		goto err;
	(void)fchmod(fd, 0644);

	if ((f = fdopen(fd, "w")) == NULL)
		err(1, "fdopen");

	for (idx = 0; idx < num_cas; idx++) {
		if (ca_print(f, cas[idx]) < 0)
			goto err;
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

	if (rename(temp, fn) == -1)
		goto err;

	ret = 1;

 err:
	free(temp);
	if (f != NULL)
		fclose(f);
	unlink(temp);

	return ret;
}

static int
ncas_sorted_cmp(const void *ca_a, const void *ca_b)
{
	int cmp;
	const struct ca *a = *(const struct ca **)ca_a;
	const struct ca *b = *(const struct ca **)ca_b;

	assert(a->purpose == CERT_PURPOSE_CA);
	assert(b->purpose == CERT_PURPOSE_CA);

	if (a->since > b->since)
		return 1;
	if (a->since < b->since)
		return -1;

	cmp = strcmp(a->location, b->location);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	return 0;
}

static int
ca_print_nonfunctional(FILE *f, const struct ca *ca)
{
	assert(ca->nonfunc);

	return fprintf(f, "%s %s %lld %lld %d %s %s %s\n", ca->aki, ca->ski,
	    (long long)ca->since, (long long)ca->last_attempt, ca->attempts,
	    ca->location, ca->mfturi, ca->notify == NULL ? "-" : ca->notify);
}

void
nca_history_save(struct ca_tree *ca_tree, time_t buildtime)
{
	struct ca *ca, **ncas_sorted = NULL;
	size_t ncas_num = 0, idx = 0;

	RB_FOREACH(ca, ca_tree, ca_tree) {
		if (ca->nonfunc && ca->purpose == CERT_PURPOSE_CA)
			ncas_num++;
	}

	if (ncas_num == 0) {
		unlink(NCA_HISTORY);
		return;
	}

	if ((ncas_sorted = calloc(ncas_num, sizeof(ncas_sorted[0]))) == NULL)
		err(1, NULL);

	RB_FOREACH(ca, ca_tree, ca_tree) {
		if (ca->nonfunc && ca->purpose == CERT_PURPOSE_CA)
			ncas_sorted[idx++] = ca;
	}

	qsort(ncas_sorted, ncas_num, sizeof(ncas_sorted[0]), ncas_sorted_cmp);

	if (!ca_state_write(NCA_HISTORY, buildtime, ncas_sorted, ncas_num,
	    ca_print_nonfunctional))
		warn("error saving non-functional CA history to %s",
		    NCA_HISTORY);

	free(ncas_sorted);
	nca_tree_free();

	return;
}

void
ca_state_load(void)
{
	FILE *f;
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;
	const char *errstr;
	struct ca *ca = NULL;
	time_t now;

	now = get_current_time();

	if ((f = fopen(CA_STATE, "r")) == NULL) {
		if (errno == ENOENT)
			return;
		err(1, "failed to open %s", CA_STATE);
	}

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		char *l, *aki, *ski, *loc, *mfturi, *since;
		size_t loc_len, mfturi_len;

		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';

		if ((ca = calloc(1, sizeof(*ca))) == NULL)
			err(1, NULL);

		ca->confirmed_in_last_run = 1;

		l = line;

		if ((aki = strsep(&l, " ")) == NULL)
			goto err;
		if ((ca->aki = strdup(aki)) == NULL)
			err(1, NULL);

		if ((ski = strsep(&l, " ")) == NULL)
			goto err;
		if ((ca->ski = strdup(ski)) == NULL)
			err(1, NULL);

		if ((since = strsep(&l, " ")) == NULL)
			goto err;
		ca->confirmed_since = strtonum(since, 1, LLONG_MAX, &errstr);
		if (errstr != NULL)
			goto err;
		if (ca->confirmed_since > now)
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
		if ((ca->location = strdup(loc)) == NULL)
			err(1, NULL);

		mfturi = l;
		if (mfturi == NULL)
			goto err;

		/* minimal example mft location: rsync://a.bc/d/e.mft */
		if ((mfturi_len = strlen(mfturi)) < 20)
			goto err;
		if (strcmp(mfturi + mfturi_len - 4, ".mft") != 0)
			goto err;
		if (!valid_uri(mfturi, strlen(mfturi), RSYNC_PROTO))
			goto err;
		if (!rsync_base_uri(mfturi, NULL))
			goto err;
		if ((ca->mfturi = strdup(mfturi)) == NULL)
			err(1, NULL);

		if (RB_INSERT(ca_state_tree, &ca_state_tree, ca) != NULL) {
			warnx("duplicate entry for ca state tree at %s",
			    ca->location);
			ca_free(ca);
			ca = NULL;
		}
	}

	if (ferror(f))
		goto err;

	fclose(f);
	free(line);

	return;

 err:
	warnx("error reading %s", CA_STATE);
	fclose(f);
	unlink(CA_STATE);

	free(line);

	ca_free(ca);

	ca_state_tree_free();
}

static inline int
cas_sorted_cmp(const void *ca_a, const void *ca_b)
{
	int cmp;
	const struct ca *a = *(const struct ca **)ca_a;
	const struct ca *b = *(const struct ca **)ca_b;

	assert(a->purpose == CERT_PURPOSE_CA);
	assert(b->purpose == CERT_PURPOSE_CA);

	cmp = strcmp(a->location, b->location);
	if (cmp > 0)
		return 1;
	if (cmp < 0)
		return -1;

	return 0;
}

static int
ca_print_confirmed(FILE *f, const struct ca *ca)
{
	assert(ca->confirmed);

	return fprintf(f, "%s %s %lld %s %s\n", ca->aki, ca->ski,
	    (long long)ca->confirmed_since, ca->location, ca->mfturi);
}

void
ca_state_save(struct ca_tree *ca_tree, time_t buildtime)
{
	struct ca *ca, **cas_sorted = NULL;
	size_t cas_num = 0, idx = 0;

	RB_FOREACH(ca, ca_tree, ca_tree) {
		if (ca->confirmed && ca->purpose == CERT_PURPOSE_CA)
			cas_num++;
	}

	if (cas_num == 0) {
		unlink(CA_STATE);
		return;
	}

	if ((cas_sorted = calloc(cas_num, sizeof(cas_sorted[0]))) == NULL)
		err(1, NULL);

	RB_FOREACH(ca, ca_tree, ca_tree) {
		if (ca->confirmed && ca->purpose == CERT_PURPOSE_CA)
			cas_sorted[idx++] = ca;
	}

	qsort(cas_sorted, cas_num, sizeof(cas_sorted[0]), cas_sorted_cmp);

	if (!ca_state_write(CA_STATE, buildtime, cas_sorted, cas_num,
	    ca_print_confirmed))
		warn("error saving CA state to %s", CA_STATE);

	free(cas_sorted);
	ca_state_tree_free();

	return;
}
