/*	$OpenBSD: table.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 * dynamic hashed associative table for commands and variables
 */

#include "sh.h"

#define	INIT_TBLS	8	/* initial table size (power of 2) */

static void     texpand     ARGS((struct table *tp, int nsize));
static int      tnamecmp    ARGS((void *p1, void *p2));


unsigned int
hash(n)
	register const char * n;
{
	register unsigned int h = 0;

	while (*n != '\0')
		h = 2*h + *n++;
	return h * 32821;	/* scatter bits */
}

void
tinit(tp, ap, tsize)
	register struct table *tp;
	register Area *ap;
	int tsize;
{
	tp->areap = ap;
	tp->tbls = NULL;
	tp->size = tp->nfree = 0;
	if (tsize)
		texpand(tp, tsize);
}

static void
texpand(tp, nsize)
	register struct table *tp;
	int nsize;
{
	register int i;
	register struct tbl *tblp, **p;
	register struct tbl **ntblp, **otblp = tp->tbls;
	int osize = tp->size;

	ntblp = (struct tbl**) alloc(sizeofN(struct tbl *, nsize), tp->areap);
	for (i = 0; i < nsize; i++)
		ntblp[i] = NULL;
	tp->size = nsize;
	tp->nfree = 8*nsize/10;	/* table can get 80% full */
	tp->tbls = ntblp;
	if (otblp == NULL)
		return;
	for (i = 0; i < osize; i++)
		if ((tblp = otblp[i]) != NULL)
			if ((tblp->flag&DEFINED)) {
				for (p = &ntblp[hash(tblp->name)
					  & (tp->size-1)];
				     *p != NULL; p--)
					if (p == ntblp) /* wrap */
						p += tp->size;
				*p = tblp;
				tp->nfree--;
			} else {
				afree((void*)tblp, tp->areap);
			}
	afree((void*)otblp, tp->areap);
}

struct tbl *
tsearch(tp, n, h)
	register struct table *tp;	/* table */
	register const char *n;		/* name to enter */
	unsigned int h;			/* hash(n) */
{
	register struct tbl **pp, *p;

	if (tp->size == 0)
		return NULL;

	/* search for name in hashed table */
	for (pp = &tp->tbls[h & (tp->size-1)]; (p = *pp) != NULL; pp--) {
		if (*p->name == *n && strcmp(p->name, n) == 0
		    && (p->flag&DEFINED))
			return p;
		if (pp == tp->tbls) /* wrap */
			pp += tp->size;
	}

	return NULL;
}

struct tbl *
tenter(tp, n, h)
	register struct table *tp;	/* table */
	register const char *n;		/* name to enter */
	unsigned int h;			/* hash(n) */
{
	register struct tbl **pp, *p;
	register int len;

	if (tp->size == 0)
		texpand(tp, INIT_TBLS);
  Search:
	/* search for name in hashed table */
	for (pp = &tp->tbls[h & (tp->size-1)]; (p = *pp) != NULL; pp--) {
		if (*p->name == *n && strcmp(p->name, n) == 0)
			return p; 	/* found */
		if (pp == tp->tbls) /* wrap */
			pp += tp->size;
	}

	if (tp->nfree <= 0) {	/* too full */
		texpand(tp, 2*tp->size);
		goto Search;
	}

	/* create new tbl entry */
	len = strlen(n) + 1;
	p = (struct tbl *) alloc(offsetof(struct tbl, name[0]) + len,
				 tp->areap);
	p->flag = 0;
	p->type = 0;
	p->areap = tp->areap;
	p->field = 0;
	p->u.array = (struct tbl *)0;
	memcpy(p->name, n, len);

	/* enter in tp->tbls */
	tp->nfree--;
	*pp = p;
	return p;
}

void
tdelete(p)
	register struct tbl *p;
{
	p->flag = 0;
}

void
twalk(ts, tp)
	struct tstate *ts;
	struct table *tp;
{
	ts->left = tp->size;
	ts->next = tp->tbls;
}

struct tbl *
tnext(ts)
	struct tstate *ts;
{
	while (--ts->left >= 0) {
		struct tbl *p = *ts->next++;
		if (p != NULL && (p->flag&DEFINED))
			return p;
	}
	return NULL;
}

static int
tnamecmp(p1, p2)
	void *p1, *p2;
{
	return strcmp(((struct tbl *)p1)->name, ((struct tbl *)p2)->name);
}

struct tbl **
tsort(tp)
	register struct table *tp;
{
	register int i;
	register struct tbl **p, **sp, **dp;

	p = (struct tbl **)alloc(sizeofN(struct tbl *, tp->size+1), ATEMP);
	sp = tp->tbls;		/* source */
	dp = p;			/* dest */
	for (i = 0; i < tp->size; i++)
		if ((*dp = *sp++) != NULL && (((*dp)->flag&DEFINED) ||
					      ((*dp)->flag&ARRAY)))
			dp++;
	i = dp - p;
	qsortp((void**)p, (size_t)i, tnamecmp);
	p[i] = NULL;
	return p;
}

#ifdef PERF_DEBUG /* performance debugging */

void tprintinfo ARGS((struct table *tp));

void
tprintinfo(tp)
	struct table *tp;
{
	struct tbl *te;
	char *n;
	unsigned int h;
	int ncmp;
	int totncmp = 0, maxncmp = 0;
	int nentries = 0;
	struct tstate ts;

	shellf("table size %d, nfree %d\n", tp->size, tp->nfree);
	shellf("    Ncmp name\n");
	twalk(&ts, tp);
	while ((te = tnext(&ts))) {
		register struct tbl **pp, *p;

		h = hash(n = te->name);
		ncmp = 0;

		/* taken from tsearch() and added counter */
		for (pp = &tp->tbls[h & (tp->size-1)]; (p = *pp); pp--) {
			ncmp++;
			if (*p->name == *n && strcmp(p->name, n) == 0
			    && (p->flag&DEFINED))
				break; /* return p; */
			if (pp == tp->tbls) /* wrap */
				pp += tp->size;
		}
		shellf("    %4d %s\n", ncmp, n);
		totncmp += ncmp;
		nentries++;
		if (ncmp > maxncmp)
			maxncmp = ncmp;
	}
	if (nentries)
		shellf("  %d entries, worst ncmp %d, avg ncmp %d.%02d\n",
			nentries, maxncmp,
			totncmp / nentries,
			(totncmp % nentries) * 100 / nentries);
}
#endif /* PERF_DEBUG */
