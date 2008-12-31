/*	$OpenBSD: pool.c,v 1.5 2008/12/31 05:37:24 canacar Exp $	*/
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
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/pool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"

void print_pool(void);
int  read_pool(void);
void  sort_pool(void);
int  select_pool(void);
void showpool(int k);

/* qsort callbacks */
int sort_name_callback(const void *s1, const void *s2);
int sort_req_callback(const void *s1, const void *s2);
int sort_psize_callback(const void *s1, const void *s2);
int sort_npage_callback(const void *s1, const void *s2);

struct pool_info {
	char name[32];
	struct pool pool;
};


int num_pools = 0;
struct pool_info *pools = NULL;


field_def fields_pool[] = {
	{"NAME", 11, 32, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"SIZE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"REQUESTS", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"FAIL", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"INUSE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"PGREQ", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"PGREL", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"NPAGE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"HIWAT", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"MINPG", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"MAXPG", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"IDLE", 8, 24, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0}
};


#define FIELD_ADDR(x) (&fields_pool[x])

#define FLD_POOL_NAME	FIELD_ADDR(0)
#define FLD_POOL_SIZE	FIELD_ADDR(1)
#define FLD_POOL_REQS	FIELD_ADDR(2)
#define FLD_POOL_FAIL	FIELD_ADDR(3)
#define FLD_POOL_INUSE	FIELD_ADDR(4)
#define FLD_POOL_PGREQ	FIELD_ADDR(5)
#define FLD_POOL_PGREL	FIELD_ADDR(6)
#define FLD_POOL_NPAGE	FIELD_ADDR(7)
#define FLD_POOL_HIWAT	FIELD_ADDR(8)
#define FLD_POOL_MINPG	FIELD_ADDR(9)
#define FLD_POOL_MAXPG	FIELD_ADDR(10)
#define FLD_POOL_IDLE	FIELD_ADDR(11)

/* Define views */
field_def *view_pool_0[] = {
	FLD_POOL_NAME, FLD_POOL_SIZE, FLD_POOL_REQS, FLD_POOL_FAIL,
	FLD_POOL_INUSE, FLD_POOL_PGREQ, FLD_POOL_PGREL, FLD_POOL_NPAGE,
	FLD_POOL_HIWAT, FLD_POOL_MINPG, FLD_POOL_MAXPG, FLD_POOL_IDLE, NULL
};

order_type pool_order_list[] = {
	{"name", "name", 'N', sort_name_callback},
	{"requests", "requests", 'Q', sort_req_callback},
	{"size", "size", 'Z', sort_psize_callback},
	{"npages", "npages", 'P', sort_npage_callback},
	{NULL, NULL, 0, NULL}
};

/* Define view managers */
struct view_manager pool_mgr = {
	"Pool", select_pool, read_pool, sort_pool, print_header,
	print_pool, keyboard_callback, pool_order_list, pool_order_list
};

field_view views_pool[] = {
	{view_pool_0, "pool", '5', &pool_mgr},
	{NULL, NULL, 0, NULL}
};


int
sort_name_callback(const void *s1, const void *s2)
{
	struct pool_info *p1, *p2;
	p1 = (struct pool_info *)s1;
	p2 = (struct pool_info *)s2;

	return strcmp(p1->name, p2->name) * sortdir;
}

int
sort_req_callback(const void *s1, const void *s2)
{
	struct pool_info *p1, *p2;
	p1 = (struct pool_info *)s1;
	p2 = (struct pool_info *)s2;

	if (p1->pool.pr_nget <  p2->pool.pr_nget)
		return sortdir;
	if (p1->pool.pr_nget >  p2->pool.pr_nget)
		return -sortdir;

	return sort_name_callback(s1, s2);
}

int
sort_npage_callback(const void *s1, const void *s2)
{
	struct pool_info *p1, *p2;
	p1 = (struct pool_info *)s1;
	p2 = (struct pool_info *)s2;

	if (p1->pool.pr_npages <  p2->pool.pr_npages)
		return sortdir;
	if (p1->pool.pr_npages >  p2->pool.pr_npages)
		return -sortdir;

	return sort_name_callback(s1, s2);
}

int
sort_psize_callback(const void *s1, const void *s2)
{
	struct pool_info *p1, *p2;
	size_t ps1, ps2;

	p1 = (struct pool_info *)s1;
	p2 = (struct pool_info *)s2;

	ps1  = (size_t)(p1->pool.pr_nget - p1->pool.pr_nput) *
	    (size_t)p1->pool.pr_size;
	ps2  = (size_t)(p2->pool.pr_nget - p2->pool.pr_nput) *
	    (size_t)p2->pool.pr_size;

	if (ps1 <  ps2)
		return sortdir;
	if (ps1 >  ps2)
		return -sortdir;

	return sort_npage_callback(s1, s2);
}

void
sort_pool(void)
{
	order_type *ordering;

	if (curr_mgr == NULL)
		return;

	ordering = curr_mgr->order_curr;

	if (ordering == NULL)
		return;
	if (ordering->func == NULL)
		return;
	if (pools == NULL)
		return;
	if (num_pools <= 0)
		return;

	mergesort(pools, num_pools, sizeof(struct pool_info), ordering->func);
}

int
select_pool(void)
{
	num_disp = num_pools;
	return (0);
}

int
read_pool(void)
{
	int mib[4], np, i;
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_POOL;
	mib[2] = KERN_POOL_NPOOLS;
	size = sizeof(np);

	if (sysctl(mib, 3, &np, &size, NULL, 0) < 0) {
		error("sysctl(npools): %s", strerror(errno));
		return (-1);
	}

	if (np <= 0) {
		num_pools = 0;
		return (0);
	}

	if (np > num_pools || pools == NULL) {
		struct pool_info *p = realloc(pools, sizeof(*pools) * np);
		if (p == NULL) {
			error("realloc: %s", strerror(errno));
			return (-1);
		}
		pools = p;
		num_pools = np;
	}

	num_disp = num_pools;

	for (i = 0; i < num_pools; i++) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_POOL;
		mib[2] = KERN_POOL_POOL;
		mib[3] = i + 1;
		size = sizeof(struct pool);
		if (sysctl(mib, 4, &pools[i].pool, &size, NULL, 0) < 0) {
			memset(&pools[i], 0, sizeof(pools[i]));
			num_disp--;
			continue;
		}
		mib[2] = KERN_POOL_NAME;
		size = sizeof(pools[i].name);
		if (sysctl(mib, 4, &pools[i].name, &size, NULL, 0) < 0) {
			snprintf(pools[i].name, size, "#%d#", mib[3]);
		}
	}

	if (i != num_pools) {
		memset(pools, 0, sizeof(*pools) * num_pools);
		return (-1);
	}

	return 0;
}


void
print_pool(void)
{
	int i, n, count = 0;

	if (pools == NULL)
		return;

	for (n = i = 0; i < num_pools; i++) {
		if (pools[i].name[0] == 0)
			continue;
		if (n++ < dispstart)
			continue;
		showpool(i);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

int
initpool(void)
{
	field_view *v;

	for (v = views_pool; v->name != NULL; v++)
		add_view(v);

	read_pool();

	return(0);
}

void
showpool(int k)
{
	struct pool_info *p = pools + k;

	if (k < 0 || k >= num_pools)
		return;

	print_fld_str(FLD_POOL_NAME, p->name);
	print_fld_uint(FLD_POOL_SIZE, p->pool.pr_size);

	print_fld_size(FLD_POOL_REQS, p->pool.pr_nget);
	print_fld_size(FLD_POOL_FAIL, p->pool.pr_nfail);
	print_fld_ssize(FLD_POOL_INUSE, p->pool.pr_nget - p->pool.pr_nput);
	print_fld_size(FLD_POOL_PGREQ, p->pool.pr_npagealloc);
	print_fld_size(FLD_POOL_PGREL, p->pool.pr_npagefree);

	print_fld_size(FLD_POOL_NPAGE, p->pool.pr_npages);
	print_fld_size(FLD_POOL_HIWAT, p->pool.pr_hiwat);
	print_fld_size(FLD_POOL_MINPG, p->pool.pr_minpages);

	if (p->pool.pr_maxpages == UINT_MAX)
		print_fld_str(FLD_POOL_MAXPG, "inf");
	else
		print_fld_size(FLD_POOL_MAXPG, p->pool.pr_maxpages);

	print_fld_size(FLD_POOL_IDLE, p->pool.pr_nidle);

	end_line();
}
