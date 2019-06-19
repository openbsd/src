/*	$OpenBSD: pool.c,v 1.18 2018/06/20 13:09:08 krw Exp $	*/
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
#include <sys/pool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "systat.h"

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

static int sysctl_rdint(const int *, unsigned int);
static int hw_ncpusfound(void);

static int pool_get_npools(void);
static int pool_get_name(int, char *, size_t);
static int pool_get_cache(int, struct kinfo_pool_cache *);
static int pool_get_cache_cpus(int, struct kinfo_pool_cache_cpu *,
    unsigned int);

void print_pool(void);
int  read_pool(void);
void  sort_pool(void);
int  select_pool(void);
void showpool(int k);
int pool_keyboard_callback(int);

/* qsort callbacks */
int sort_name_callback(const void *s1, const void *s2);
int sort_req_callback(const void *s1, const void *s2);
int sort_psize_callback(const void *s1, const void *s2);
int sort_npage_callback(const void *s1, const void *s2);

struct pool_info {
	char name[32];
	struct kinfo_pool pool;
};

/*
 * the kernel gives an array of ncpusfound * kinfo_pool_cache structs, but
 * it's idea of how big that struct is may differ from here. we fetch both
 * ncpusfound and the size it thinks kinfo_pool_cache is from sysctl, and
 * then allocate the memory for this here.
 */
struct pool_cache_info {
	char name[32];
	struct kinfo_pool_cache cache;
	struct kinfo_pool_cache_cpu *cache_cpus;
};

int print_all = 0;
int num_pools = 0;
struct pool_info *pools = NULL;
int num_pool_caches = 0;
struct pool_cache_info *pool_caches = NULL;
size_t pool_caches_size = 0;

int ncpusfound = 0;

field_def fields_pool[] = {
	{"NAME", 12, 32, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
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

#define FLD_POOL_NAME	FIELD_ADDR(fields_pool,0)
#define FLD_POOL_SIZE	FIELD_ADDR(fields_pool,1)
#define FLD_POOL_REQS	FIELD_ADDR(fields_pool,2)
#define FLD_POOL_FAIL	FIELD_ADDR(fields_pool,3)
#define FLD_POOL_INUSE	FIELD_ADDR(fields_pool,4)
#define FLD_POOL_PGREQ	FIELD_ADDR(fields_pool,5)
#define FLD_POOL_PGREL	FIELD_ADDR(fields_pool,6)
#define FLD_POOL_NPAGE	FIELD_ADDR(fields_pool,7)
#define FLD_POOL_HIWAT	FIELD_ADDR(fields_pool,8)
#define FLD_POOL_MINPG	FIELD_ADDR(fields_pool,9)
#define FLD_POOL_MAXPG	FIELD_ADDR(fields_pool,10)
#define FLD_POOL_IDLE	FIELD_ADDR(fields_pool,11)

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
	print_pool, pool_keyboard_callback, pool_order_list, pool_order_list
};

field_view pool_view = {
	view_pool_0,
	"pool",
	'5',
	&pool_mgr
};

void	pool_cache_print(void);
int	pool_cache_read(void);
void	pool_cache_sort(void);
void	pool_cache_show(const struct pool_cache_info *);
int	pool_cache_sort_name_callback(const void *, const void *);
int	pool_cache_sort_len_callback(const void *, const void *);
int	pool_cache_sort_idle_callback(const void *, const void *);
int	pool_cache_sort_ngc_callback(const void *, const void *);
int	pool_cache_sort_req_callback(const void *, const void *);
int	pool_cache_sort_put_callback(const void *, const void *);
int	pool_cache_sort_lreq_callback(const void *, const void *);
int	pool_cache_sort_lput_callback(const void *, const void *);
int	pool_cache_kbd_cb(int);

field_def pool_cache_fields[] = {
	{"NAME", 12, 32, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"LEN", 4, 4, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"IDLE", 4, 4, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"NGC", 4, 4, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"CPU",  4, 4, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"REQ", 8, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"REL", 8, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"LREQ", 8, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"LREL", 8, 12, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
};

#define FLD_POOL_CACHE_NAME	FIELD_ADDR(pool_cache_fields, 0)
#define FLD_POOL_CACHE_LEN	FIELD_ADDR(pool_cache_fields, 1)
#define FLD_POOL_CACHE_IDLE	FIELD_ADDR(pool_cache_fields, 2)
#define FLD_POOL_CACHE_NGC	FIELD_ADDR(pool_cache_fields, 3)
#define FLD_POOL_CACHE_CPU	FIELD_ADDR(pool_cache_fields, 4)
#define FLD_POOL_CACHE_GET	FIELD_ADDR(pool_cache_fields, 5)
#define FLD_POOL_CACHE_PUT	FIELD_ADDR(pool_cache_fields, 6)
#define FLD_POOL_CACHE_LGET	FIELD_ADDR(pool_cache_fields, 7)
#define FLD_POOL_CACHE_LPUT	FIELD_ADDR(pool_cache_fields, 8)

field_def *view_pool_cache_0[] = {
	FLD_POOL_CACHE_NAME,
	FLD_POOL_CACHE_LEN,
	FLD_POOL_CACHE_IDLE,
	FLD_POOL_CACHE_NGC,
	FLD_POOL_CACHE_CPU,
	FLD_POOL_CACHE_GET,
	FLD_POOL_CACHE_PUT,
	FLD_POOL_CACHE_LGET,
	FLD_POOL_CACHE_LPUT,
	NULL,
};

order_type pool_cache_order_list[] = {
	{"name", "name", 'N', pool_cache_sort_name_callback},
	{"len", "len", 'L', pool_cache_sort_len_callback},
	{"idle", "idle", 'I', pool_cache_sort_idle_callback},
	{"ngc", "ngc", 'G', pool_cache_sort_ngc_callback},
	{"requests", "requests", 'Q', pool_cache_sort_req_callback},
	{"releases", "releases", 'P', pool_cache_sort_put_callback},
	{"listrequests", "listrequests", 'E', pool_cache_sort_req_callback},
	{"listreleases", "listreleases", 'U', pool_cache_sort_put_callback},
	{NULL, NULL, 0, NULL}
};

/* Define view managers */
struct view_manager pool_cache_mgr = {
	"PoolCache",
	NULL,
	pool_cache_read,
	pool_cache_sort,
	print_header,
	pool_cache_print,
	pool_keyboard_callback,
	pool_cache_order_list,
	pool_cache_order_list
};

field_view pool_cache_view = {
	view_pool_cache_0,
	"pcaches",
	'5',
	&pool_cache_mgr
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

	p1 = (struct pool_info *)s1;
	p2 = (struct pool_info *)s2;

	if (p1->pool.pr_size <  p2->pool.pr_size)
		return sortdir;
	if (p1->pool.pr_size >  p2->pool.pr_size)
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
	int mib[] = { CTL_KERN, KERN_POOL, KERN_POOL_POOL, 0 };
	struct pool_info *p;
	int np, i;
	size_t size;

	np = pool_get_npools();
	if (np == -1) {
		error("sysctl(npools): %s", strerror(errno));
		return (-1);
	}

	if (np == 0) {
		free(pools);
		pools = NULL;
		num_pools = 0;
		return (0);
	}

	if (np > num_pools || pools == NULL) {
		p = reallocarray(pools, np, sizeof(*pools));
		if (p == NULL) {
			error("realloc: %s", strerror(errno));
			return (-1);
		}
		/* commit */
		pools = p;
		num_pools = np;
	}

	num_disp = num_pools;

	for (i = 0; i < num_pools; i++) {
		p = &pools[i];
		np = i + 1;

		mib[3] = np;
		size = sizeof(pools[i].pool);
		if (sysctl(mib, nitems(mib), &p->pool, &size, NULL, 0) < 0) {
			p->name[0] = '\0';
			num_disp--;
			continue;
		}

		if (pool_get_name(np, p->name, sizeof(p->name)) < 0)
			snprintf(p->name, sizeof(p->name), "#%d#", i + 1);
	}

	return 0;
}


void
print_pool(void)
{
	struct pool_info *p;
	int i, n, count = 0;

	if (pools == NULL)
		return;

	for (n = i = 0; i < num_pools; i++) {
		p = &pools[i];
		if (p->name[0] == 0)
			continue;

		if (!print_all &&
		   (p->pool.pr_nget == 0 && p->pool.pr_npagealloc == 0))
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
	add_view(&pool_view);
	read_pool();

	ncpusfound = hw_ncpusfound();
	if (ncpusfound == -1) {
		error("sysctl(ncpusfound): %s", strerror(errno));
		exit(1);
	}

	add_view(&pool_cache_view);
	pool_cache_read();

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

int
pool_keyboard_callback(int ch)
{
	switch (ch) {
	case 'A':
		print_all ^= 1;
		gotsig_alarm = 1;
	default:
		return keyboard_callback(ch);
	};

	return (1);
}

int
pool_cache_read(void)
{
	struct pool_cache_info *pc;
	int np, i;

	np = pool_get_npools();
	if (np == -1) {
		error("sysctl(npools): %s", strerror(errno));
		return (-1);
	}

	if (np > pool_caches_size) {
		pc = reallocarray(pool_caches, np, sizeof(*pc));
		if (pc == NULL) {
			error("realloc: %s", strerror(errno));
			return (-1);
		}
		/* commit to using the new memory */
		pool_caches = pc;

		for (i = pool_caches_size; i < np; i++) {
			pc = &pool_caches[i];
			pc->name[0] = '\0';

			pc->cache_cpus = reallocarray(NULL, ncpusfound,
			    sizeof(*pc->cache_cpus));
			if (pc->cache_cpus == NULL) {
				error("malloc cache cpus: %s", strerror(errno));
				goto unalloc;
			}
		}

		/* commit to using the new cache_infos */
		pool_caches_size = np;
	}

	num_pool_caches = 0;
	for (i = 0; i < pool_caches_size; i++) {
		pc = &pool_caches[num_pool_caches];
		np = i + 1;

		if (pool_get_cache(np, &pc->cache) < 0 ||
		    pool_get_cache_cpus(np, pc->cache_cpus, ncpusfound) < 0) {
			pc->name[0] = '\0';
			continue;
		}

		if (pool_get_name(np, pc->name, sizeof(pc->name)) < 0)
			snprintf(pc->name, sizeof(pc->name), "#%d#", i + 1);
		num_pool_caches++;
	}

	return 0;

unalloc:
	while (i > pool_caches_size) {
		pc = &pool_caches[--i];
		free(pc->cache_cpus);
	}
	return (-1);
}

void
pool_cache_sort(void)
{
	order_type *ordering;

	if (curr_mgr == NULL)
		return;

	ordering = curr_mgr->order_curr;

	if (ordering == NULL)
		return;
	if (ordering->func == NULL)
		return;
	if (pool_caches == NULL)
		return;
	if (num_pool_caches <= 0)
		return;

	mergesort(pool_caches, num_pool_caches, sizeof(*pool_caches), ordering->func);
}

void
pool_cache_print(void)
{
	struct pool_cache_info *pc;
	int i, n, count = 0;

	if (pool_caches == NULL)
		return;

	for (n = i = 0; i < num_pool_caches; i++) {
		pc = &pool_caches[i];
		if (pc->name[0] == '\0')
			continue;

		if (n++ < dispstart)
			continue;

		pool_cache_show(pc);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

void
pool_cache_show(const struct pool_cache_info *pc)
{
	const struct kinfo_pool_cache *kpc;
	const struct kinfo_pool_cache_cpu *kpcc;
	int cpu;

	kpc = &pc->cache;

	print_fld_str(FLD_POOL_CACHE_NAME, pc->name);
	print_fld_uint(FLD_POOL_CACHE_LEN, kpc->pr_len);
	print_fld_uint(FLD_POOL_CACHE_IDLE, kpc->pr_nitems);
	print_fld_size(FLD_POOL_CACHE_NGC, kpc->pr_ngc);

	for (cpu = 0; cpu < ncpusfound; cpu++) {
		kpcc = &pc->cache_cpus[cpu];

		print_fld_uint(FLD_POOL_CACHE_CPU, kpcc->pr_cpu);

		print_fld_size(FLD_POOL_CACHE_GET, kpcc->pr_nget);
		print_fld_size(FLD_POOL_CACHE_PUT, kpcc->pr_nput);
		print_fld_size(FLD_POOL_CACHE_LGET, kpcc->pr_nlget);
		print_fld_size(FLD_POOL_CACHE_LPUT, kpcc->pr_nlput);
		end_line();
	}
}

int
pool_cache_sort_name_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	return strcmp(pc1->name, pc2->name) * sortdir;
}

int
pool_cache_sort_len_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	if (pc1->cache.pr_len <  pc2->cache.pr_len)
		return sortdir;
	if (pc1->cache.pr_len >  pc2->cache.pr_len)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_idle_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	if (pc1->cache.pr_nitems <  pc2->cache.pr_nitems)
		return sortdir;
	if (pc1->cache.pr_nitems >  pc2->cache.pr_nitems)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_ngc_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	if (pc1->cache.pr_ngc <  pc2->cache.pr_ngc)
		return sortdir;
	if (pc1->cache.pr_ngc >  pc2->cache.pr_ngc)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_req_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	uint64_t nget1 = 0, nget2 = 0;
	int oflow1 = 0, oflow2 = 0;
	int cpu;

	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	for (cpu = 0; cpu < ncpusfound; cpu++) {
		if (nget1 + pc1->cache_cpus->pr_nget < nget1)
			oflow1++;
		nget1 += pc1->cache_cpus->pr_nget;
		if (nget2 + pc2->cache_cpus->pr_nget < nget2)
			oflow2++;
		nget2 += pc2->cache_cpus->pr_nget;
	}

	if (oflow1 < oflow2)
		return sortdir;
	if (oflow1 > oflow2)
		return -sortdir;
	if (nget1 < nget2)
		return sortdir;
	if (nget1 > nget2)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_put_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	uint64_t nput1 = 0, nput2 = 0;
	int oflow1 = 0, oflow2 = 0;
	int cpu;

	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	for (cpu = 0; cpu < ncpusfound; cpu++) {
		if (nput1 + pc1->cache_cpus->pr_nput < nput1)
			oflow1++;
		nput1 += pc1->cache_cpus->pr_nput;
		if (nput2 + pc2->cache_cpus->pr_nput < nput2)
			oflow2++;
		nput2 += pc2->cache_cpus->pr_nput;
	}

	if (oflow1 < oflow2)
		return sortdir;
	if (oflow1 > oflow2)
		return -sortdir;
	if (nput1 < nput2)
		return sortdir;
	if (nput1 > nput2)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_lreq_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	uint64_t nlget1 = 0, nlget2 = 0;
	int oflow1 = 0, oflow2 = 0;
	int cpu;

	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	for (cpu = 0; cpu < ncpusfound; cpu++) {
		if (nlget1 + pc1->cache_cpus->pr_nlget < nlget1)
			oflow1++;
		nlget1 += pc1->cache_cpus->pr_nlget;
		if (nlget2 + pc2->cache_cpus->pr_nlget < nlget2)
			oflow2++;
		nlget2 += pc2->cache_cpus->pr_nlget;
	}

	if (oflow1 < oflow2)
		return sortdir;
	if (oflow1 > oflow2)
		return -sortdir;
	if (nlget1 < nlget2)
		return sortdir;
	if (nlget1 > nlget2)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_cache_sort_lput_callback(const void *s1, const void *s2)
{
	struct pool_cache_info *pc1, *pc2;
	uint64_t nlput1 = 0, nlput2 = 0;
	int oflow1 = 0, oflow2 = 0;
	int cpu;

	pc1 = (struct pool_cache_info *)s1;
	pc2 = (struct pool_cache_info *)s2;

	for (cpu = 0; cpu < ncpusfound; cpu++) {
		if (nlput1 + pc1->cache_cpus->pr_nlput < nlput1)
			oflow1++;
		nlput1 += pc1->cache_cpus->pr_nlput;
		if (nlput2 + pc2->cache_cpus->pr_nlput < nlput2)
			oflow2++;
		nlput2 += pc2->cache_cpus->pr_nlput;
	}

	if (oflow1 < oflow2)
		return sortdir;
	if (oflow1 > oflow2)
		return -sortdir;
	if (nlput1 < nlput2)
		return sortdir;
	if (nlput1 > nlput2)
		return -sortdir;

	return pool_cache_sort_name_callback(s1, s2);
}

int
pool_get_npools(void)
{
	int mib[] = { CTL_KERN, KERN_POOL, KERN_POOL_NPOOLS };

	return (sysctl_rdint(mib, nitems(mib)));
}

static int
pool_get_cache(int pool, struct kinfo_pool_cache *kpc)
{
	int mib[] = { CTL_KERN, KERN_POOL, KERN_POOL_CACHE, pool };
	size_t len = sizeof(*kpc);

	return (sysctl(mib, nitems(mib), kpc, &len, NULL, 0));
}

static int
pool_get_cache_cpus(int pool, struct kinfo_pool_cache_cpu *kpcc,
    unsigned int ncpus)
{
	int mib[] = { CTL_KERN, KERN_POOL, KERN_POOL_CACHE_CPUS, pool };
	size_t len = sizeof(*kpcc) * ncpus;

	return (sysctl(mib, nitems(mib), kpcc, &len, NULL, 0));
}

static int
pool_get_name(int pool, char *name, size_t len)
{
	int mib[] = { CTL_KERN, KERN_POOL, KERN_POOL_NAME, pool };

	return (sysctl(mib, nitems(mib), name, &len, NULL, 0));
}

static int
hw_ncpusfound(void)
{
	int mib[] = { CTL_HW, HW_NCPUFOUND };

	return (sysctl_rdint(mib, nitems(mib)));
}

static int
sysctl_rdint(const int *mib, unsigned int nmib)
{
	int i;
	size_t size = sizeof(i);

	if (sysctl(mib, nmib, &i, &size, NULL, 0) == -1)
		return (-1);

	return (i);
}
