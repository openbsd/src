/*	$OpenBSD: malloc.c,v 1.3 2015/01/16 00:03:37 deraadt Exp $	*/
/*
 * Copyright (c) 2008 Can Erkin Acar <canacar@openbsd.org>
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
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "systat.h"

void print_types(void);
void print_buckets(void);
int  read_types(void);
int  read_buckets(void);
void sort_types(void);
int  select_types(void);
int  select_buckets(void);
void showtype(int k);
void showbucket(int k);


/* qsort callbacks */
int sort_tname_callback(const void *s1, const void *s2);
int sort_treq_callback(const void *s1, const void *s2);
int sort_inuse_callback(const void *s1, const void *s2);
int sort_memuse_callback(const void *s1, const void *s2);

#define MAX_BUCKETS 16

struct type_info {
	const char *name;
	struct kmemstats stats;
	char buckets[MAX_BUCKETS];
};


struct type_info types[M_LAST];

struct kmembuckets buckets[MAX_BUCKETS];
int bucket_sizes[MAX_BUCKETS];

int num_types = 0;
int num_buckets = 0;

/*
 * These names are defined in <sys/malloc.h>.
 */
const char *kmemnames[] = INITKMEMNAMES;

field_def fields_malloc[] = {
	{"TYPE", 14, 32, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"INUSE", 6, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"MEMUSE", 6, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"HIGHUSE", 6, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"LIMIT", 6, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"REQUESTS", 8, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"TYPE LIMIT", 5, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"KERN LIMIT", 5, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"BUCKETS", MAX_BUCKETS, MAX_BUCKETS, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},

	{"BUCKET", 8, 8, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"REQUESTS", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"INUSE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"FREE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"HIWAT", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"COULDFREE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
};


#define FLD_TYPE_NAME		FIELD_ADDR(fields_malloc,0)
#define FLD_TYPE_INUSE		FIELD_ADDR(fields_malloc,1)
#define FLD_TYPE_MEMUSE		FIELD_ADDR(fields_malloc,2)
#define FLD_TYPE_HIGHUSE	FIELD_ADDR(fields_malloc,3)
#define FLD_TYPE_LIMIT		FIELD_ADDR(fields_malloc,4)
#define FLD_TYPE_REQUESTS	FIELD_ADDR(fields_malloc,5)
#define FLD_TYPE_TLIMIT		FIELD_ADDR(fields_malloc,6)
#define FLD_TYPE_KLIMIT		FIELD_ADDR(fields_malloc,7)
#define FLD_TYPE_SIZES		FIELD_ADDR(fields_malloc,8)

#define FLD_BUCKET_SIZE		FIELD_ADDR(fields_malloc,9)
#define FLD_BUCKET_REQUESTS	FIELD_ADDR(fields_malloc,10)
#define FLD_BUCKET_INUSE	FIELD_ADDR(fields_malloc,11)
#define FLD_BUCKET_FREE		FIELD_ADDR(fields_malloc,12)
#define FLD_BUCKET_HIWAT	FIELD_ADDR(fields_malloc,13)
#define FLD_BUCKET_COULDFREE	FIELD_ADDR(fields_malloc,14)



/* Define views */
field_def *view_malloc_0[] = {
	FLD_TYPE_NAME, FLD_TYPE_INUSE, FLD_TYPE_MEMUSE,
	FLD_TYPE_HIGHUSE, FLD_TYPE_LIMIT, FLD_TYPE_REQUESTS,
	FLD_TYPE_TLIMIT, FLD_TYPE_KLIMIT, FLD_TYPE_SIZES, NULL
};

field_def *view_malloc_1[] = {
	FLD_BUCKET_SIZE, FLD_BUCKET_REQUESTS, FLD_BUCKET_INUSE,
	FLD_BUCKET_FREE, FLD_BUCKET_HIWAT, FLD_BUCKET_COULDFREE, NULL
};

order_type type_order_list[] = {
	{"name", "name", 'N', sort_tname_callback},
	{"inuse", "in use", 'U', sort_inuse_callback},
	{"memuse", "mem use", 'S', sort_memuse_callback},
	{"requests", "requests", 'Q', sort_treq_callback},
	{NULL, NULL, 0, NULL}
};

/* Define view managers */
struct view_manager types_mgr = {
	"Types", select_types, read_types, sort_types, print_header,
	print_types, keyboard_callback, type_order_list, type_order_list
};

struct view_manager buckets_mgr = {
	"Buckets", select_buckets, read_buckets, NULL, print_header,
	print_buckets, keyboard_callback, NULL, NULL
};

field_view views_malloc[] = {
	{view_malloc_0, "malloc", '6', &types_mgr},
	{view_malloc_1, "buckets", '7', &buckets_mgr},
	{NULL, NULL, 0, NULL}
};


int
sort_tname_callback(const void *s1, const void *s2)
{
	struct type_info *t1, *t2;
	t1 = (struct type_info *)s1;
	t2 = (struct type_info *)s2;

	return strcmp(t1->name, t2->name) * sortdir;
}

int
sort_treq_callback(const void *s1, const void *s2)
{
	struct type_info *t1, *t2;
	t1 = (struct type_info *)s1;
	t2 = (struct type_info *)s2;

	if (t1->stats.ks_calls <  t2->stats.ks_calls)
		return sortdir;
	if (t1->stats.ks_calls >  t2->stats.ks_calls)
		return -sortdir;

	return sort_tname_callback(s1, s2);
}

int
sort_inuse_callback(const void *s1, const void *s2)
{
	struct type_info *t1, *t2;
	t1 = (struct type_info *)s1;
	t2 = (struct type_info *)s2;

	if (t1->stats.ks_inuse <  t2->stats.ks_inuse)
		return sortdir;
	if (t1->stats.ks_inuse >  t2->stats.ks_inuse)
		return -sortdir;

	return sort_tname_callback(s1, s2);
}

int
sort_memuse_callback(const void *s1, const void *s2)
{
	struct type_info *t1, *t2;
	t1 = (struct type_info *)s1;
	t2 = (struct type_info *)s2;

	if (t1->stats.ks_memuse <  t2->stats.ks_memuse)
		return sortdir;
	if (t1->stats.ks_memuse >  t2->stats.ks_memuse)
		return -sortdir;

	return sort_tname_callback(s1, s2);
}


void
sort_types(void)
{
	order_type *ordering;

	if (curr_mgr == NULL)
		return;

	ordering = curr_mgr->order_curr;

	if (ordering == NULL)
		return;
	if (ordering->func == NULL)
		return;
	if (num_types <= 0)
		return;

	mergesort(types, num_types, sizeof(struct type_info), ordering->func);
}

int
select_types(void)
{
	num_disp = num_types;
	return (0);
}

int
select_buckets(void)
{
	num_disp = num_buckets;
	return (0);
}

int
read_buckets(void)
{
	int mib[4];
	char buf[BUFSIZ], *bufp, *ap;
	const char *errstr;
	size_t siz;

	mib[0] = CTL_KERN;
	mib[1] = KERN_MALLOCSTATS;
	mib[2] = KERN_MALLOC_BUCKETS;

	siz = sizeof(buf);
	num_buckets = 0;

	if (sysctl(mib, 3, buf, &siz, NULL, 0) < 0) {
		error("sysctl(kern.malloc.buckets): %s", strerror(errno));
		return (-1);
	}

	bufp = buf;
	mib[2] = KERN_MALLOC_BUCKET;
	siz = sizeof(struct kmembuckets);

	while ((ap = strsep(&bufp, ",")) != NULL) {
		if (num_buckets >= MAX_BUCKETS)
			break;
		bucket_sizes[num_buckets] = strtonum(ap, 0, INT_MAX, &errstr);
		if (errstr) {
			error("strtonum(%s): %s", ap, errstr);
			return (-1);
		}
		mib[3] = bucket_sizes[num_buckets];
		if (sysctl(mib, 4, &buckets[num_buckets], &siz,
			   NULL, 0) < 0) {
			error("sysctl(kern.malloc.bucket.%d): %s",
			    mib[3], strerror(errno));
			return (-1);
		}
		num_buckets++;
	}

	return (0);
}

int
read_types(void)
{
	struct type_info *ti;
	int i, j, k, mib[4];
	size_t siz;

	bzero(types, sizeof(types));
	ti = types;
	siz = sizeof(struct kmemstats);

	num_types = 0;
	
	for (i = 0; i < M_LAST; i++) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MALLOCSTATS;
		mib[2] = KERN_MALLOC_KMEMSTATS;
		mib[3] = i;
		
		/*
		 * Skip errors -- these are presumed to be unallocated
		 * entries.
		 */
		if (sysctl(mib, 4, &ti->stats, &siz, NULL, 0) < 0)
			continue;

		if (ti->stats.ks_calls == 0)
			continue;

		ti->name = kmemnames[i];
		j = 1 << MINBUCKET;

		for (k = 0; k < MAX_BUCKETS; k++, j <<= 1)
			ti->buckets[k] = (ti->stats.ks_size & j) ? '|' : '.';

		ti++;
		num_types++;
	}

	return (0);
}


void
print_types(void)
{
	int n, count = 0;

	for (n = dispstart; n < num_disp; n++) {
		showtype(n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;	}
}

void
print_buckets(void)
{
	int n, count = 0;

	for (n = dispstart; n < num_disp; n++) {
		showbucket(n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

int
initmalloc(void)
{
	field_view *v;

	for (v = views_malloc; v->name != NULL; v++)
		add_view(v);

	read_buckets();
	read_types();

	return(0);
}

void
showbucket(int k)
{
	struct kmembuckets *kp = buckets + k;

	if (k < 0 || k >= num_buckets)
		return;

	print_fld_size(FLD_BUCKET_SIZE, bucket_sizes[k]);
	print_fld_size(FLD_BUCKET_INUSE, kp->kb_total - kp->kb_totalfree);
	print_fld_size(FLD_BUCKET_FREE, kp->kb_totalfree);
	print_fld_size(FLD_BUCKET_REQUESTS, kp->kb_calls);
	print_fld_size(FLD_BUCKET_HIWAT, kp->kb_highwat);
	print_fld_size(FLD_BUCKET_COULDFREE, kp->kb_couldfree);

	end_line();
}


void
showtype(int k)
{
	struct type_info *t = types + k;

	if (k < 0 || k >= num_types)
		return;


	print_fld_str(FLD_TYPE_NAME, t->name ? t->name : "undefined");
	print_fld_size(FLD_TYPE_INUSE, t->stats.ks_inuse);
	print_fld_size(FLD_TYPE_MEMUSE, t->stats.ks_memuse);
	print_fld_size(FLD_TYPE_HIGHUSE, t->stats.ks_maxused);
	print_fld_size(FLD_TYPE_LIMIT, t->stats.ks_limit);
	print_fld_size(FLD_TYPE_REQUESTS, t->stats.ks_calls);
	print_fld_size(FLD_TYPE_TLIMIT, t->stats.ks_limblocks);
	print_fld_size(FLD_TYPE_KLIMIT, t->stats.ks_mapblocks);
	print_fld_str(FLD_TYPE_SIZES, t->buckets);

	end_line();
}
