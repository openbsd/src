/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Hash table functions
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$arla: hash.c,v 1.18 2002/05/23 15:21:41 lha Exp $");
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <bool.h>
#include <hash.h>
#include <arlamath.h>

struct ht_bucket {
     Hashentry *e;
     int len;
};


struct hashtab {		/* Hash table */
     int (*cmp)(void *, void *); /* Compare function */
     unsigned (*hash)(void *);	/* hash function */
     int flags;			/* flags */
     int sz;			/* Size */
     int maxbucketlen;		/* max bucket length */
     struct ht_bucket *tab;	/* The table */
};

struct hashentry {		/* Entry in bucket */
     struct hashentry **prev;
     struct hashentry *next;
     void *ptr;
};

#define HASHTAB_INITAL_SIZE	17
#define HASHTAB_MAX_BUCKET_LEN	20

static Hashentry *_search(Hashtab * htab,	/* The hash table */
			  void *ptr);	/* And key */
static void *_add(Hashtab * htab,
		  void *ptr,
		  Bool unique);

static void _might_resize(Hashtab *htab,	/* The hash table */
			  int bucket_len);	/* Hash bucket len */

Hashtab *
hashtabnew(int sz,
	   int (*cmp) (void *, void *),
	   unsigned (*hash) (void *))
{
     return hashtabnewf(sz, cmp, hash, 0);
}

Hashtab *
hashtabnewf(int sz,
	    int (*cmp) (void *, void *),
	    unsigned (*hash) (void *),
	    int flags) 
{
    Hashtab *htab;
    int i;

    assert(sz > 0 || (flags & HASHTAB_GROW));

    htab = (Hashtab *) malloc(sizeof(Hashtab));

    if (htab == NULL)
	return NULL;

    if (sz == 0 && (flags & HASHTAB_GROW))
	 sz = HASHTAB_INITAL_SIZE;

    htab->tab = malloc(sz * sizeof(struct ht_bucket));
    if (htab->tab == NULL){
	 free(htab);
	 return NULL;
    }

    for (i = 0; i < sz; ++i) {
	htab->tab[i].e = NULL;
	htab->tab[i].len = 0;
    }

    htab->cmp = cmp;
    htab->hash = hash;
    htab->sz = sz;
    htab->maxbucketlen = HASHTAB_MAX_BUCKET_LEN;
    htab->flags = flags;
	
    return htab;
}

/* Intern search function */

static Hashentry *
_search(Hashtab * htab, void *ptr)
{
    Hashentry *hptr;

    assert(htab && ptr);

    for (hptr = htab->tab[(*htab->hash) (ptr) % htab->sz].e;
	 hptr;
	 hptr = hptr->next)
	if ((*htab->cmp) (ptr, hptr->ptr) == 0)
	    break;
    return hptr;
}

/* Interal resize function */

static Bool
_resize(void *ptr, void *arg)
{
     Hashtab *h = arg;
     _add(h, ptr, TRUE);
     return TRUE;
}

static void
_might_resize(Hashtab *htab, int bucket_len)
{
     if (bucket_len > htab->maxbucketlen) {
	  Hashtab *nhtab;
	  struct ht_bucket *tab;
	  int new_size, old_size;

	  new_size = arlautil_findprime(htab->sz * 2);
	  assert (new_size > htab->sz);

	  nhtab = hashtabnewf(new_size, htab->cmp,
			      htab->hash, htab->flags);
	  if (nhtab == NULL)
	       return;

	  hashtabcleantab(htab, _resize, nhtab);

	  /* switch place between the tab and size */
	  tab = htab->tab;
	  htab->tab = nhtab->tab;
	  nhtab->tab = tab;

	  old_size = htab->sz;
	  htab->sz = nhtab->sz;
	  nhtab->sz = old_size;
	  
	  hashtabrelease(nhtab);
     }
}


/* Search for element in hash table */

void *
hashtabsearch(Hashtab * htab, void *ptr)
{
    Hashentry *tmp;

    tmp = _search(htab, ptr);
    return tmp ? tmp->ptr : tmp;
}

/* add element to hash table */
/* if already there, set new value */
/* !NULL if succesful */

static void *
_add(Hashtab * htab, void *ptr, Bool unique)
{
    Hashentry *h = _search(htab, ptr);
    Hashentry **tabptr;
    struct ht_bucket *hb;

    assert(htab && ptr);

    if (h) {
	if (unique)
	    return NULL;
	free((void *) h->ptr);
	h->ptr = ptr;
    } else {
	h = (Hashentry *) malloc(sizeof(Hashentry));
	if (h == NULL) {
	    return NULL;
	}
	hb = &htab->tab[(*htab->hash) (ptr) % htab->sz];
	hb->len++;
	tabptr = &hb->e;
	h->next = *tabptr;
	*tabptr = h;
	h->prev = tabptr;
	if (h->next)
	    h->next->prev = &h->next;
	h->ptr = ptr;
	if (htab->flags & HASHTAB_GROW)
	     _might_resize(htab, hb->len);
    }
    return h;
}

void *
hashtabaddreplace (Hashtab *htab, void *ptr)
{
    return _add (htab, ptr, FALSE);
}

void *
hashtabadd (Hashtab *htab, void *ptr)
{
    return _add (htab, ptr, TRUE);
}

/* delete element with key key. Iff freep, free Hashentry->ptr */

int
_hashtabdel(Hashtab * htab, void *ptr, int freep)
{
    Hashentry *h;

    assert(htab && ptr);

    h = _search(htab, ptr);
    if (h) {
	if (freep)
	    free(h->ptr);
	if ((*(h->prev) = h->next))
	    h->next->prev = h->prev;
	free(h);
	return 0;
    } else
	return -1;
}

/* Do something for each element */

void
hashtabforeach(Hashtab * htab, Bool(*func) (void *ptr, void *arg),
	       void *arg)
{
    struct ht_bucket *h;
    Hashentry *g, *next;

    assert(htab);

    for (h = htab->tab; h < &htab->tab[htab->sz]; ++h)
	for (g = h->e; g; g = next) {
	    next = g->next;
	    if ((*func) (g->ptr, arg))
		return;
	}
}


/* Clean out all elements that meet condition */

void
hashtabcleantab(Hashtab * htab, Bool(*cond) (void *ptr, void *arg),
	       void *arg)
{
    struct ht_bucket *h;
    Hashentry *g, *f;

    assert(htab);

    for (h = htab->tab; h < &htab->tab[htab->sz]; ++h) {
	for (g = h->e; g;) {
	    if ((*cond) (g->ptr, arg)) {
		f = g ; 
		g = g->next ;
		if ((*(f->prev) = f->next))
		    f->next->prev = f->prev;
		free(f);
		assert(h->len > 0);
		h->len--;
	    } else {
		 g = g->next;
	    }
	}
    }
}

static Bool
true_cond(void *ptr, void *arg)
{
    return TRUE;
}

/* Free the hashtab and all items in it */

void
hashtabrelease(Hashtab *htab)
{
    hashtabcleantab(htab, true_cond, NULL);
    free(htab->tab);
    free(htab);
}


/* standard hash-functions for strings */

unsigned
hashadd(const char *s)
{				/* Standard hash function */
    unsigned i;

    assert(s);

    for (i = 0; *s; ++s)
	i += *s;
    return i;
}

unsigned
hashcaseadd(const char *s)
{				/* Standard hash function */
    unsigned i;

    assert(s);

    for (i = 0; *s; ++s)
	i += toupper((unsigned char)*s);
    return i;
}

#define TWELVE (sizeof(unsigned))
#define SEVENTYFIVE (6*sizeof(unsigned))
#define HIGH_BITS (~((unsigned)(~0) >> TWELVE))

unsigned
hashjpw(const char *ss)
{				/* another hash function */
    unsigned h = 0;
    unsigned g;
    unsigned const char *s = (unsigned const char *) ss;

    for (; *s; ++s) {
	h = (h << TWELVE) + *s;
	if ((g = h & HIGH_BITS))
	    h = (h ^ (g >> SEVENTYFIVE)) & ~HIGH_BITS;
    }
    return h;
}
