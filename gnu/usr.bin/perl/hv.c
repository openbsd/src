/*    hv.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "I sit beside the fire and think of all that I have seen."  --Bilbo
 */

#include "EXTERN.h"
#include "perl.h"

static void hsplit _((HV *hv));
static void hfreeentries _((HV *hv));

static HE* more_he();

static HE*
new_he()
{
    HE* he;
    if (he_root) {
        he = he_root;
        he_root = (HE*)he->hent_next;
        return he;
    }
    return more_he();
}

static void
del_he(p)
HE* p;
{
    p->hent_next = (HE*)he_root;
    he_root = p;
}

static HE*
more_he()
{
    register HE* he;
    register HE* heend;
    he_root = (HE*)safemalloc(1008);
    he = he_root;
    heend = &he[1008 / sizeof(HE) - 1];
    while (he < heend) {
        he->hent_next = (HE*)(he + 1);
        he++;
    }
    he->hent_next = 0;
    return new_he();
}

SV**
hv_fetch(hv,key,klen,lval)
HV *hv;
char *key;
U32 klen;
I32 lval;
{
    register XPVHV* xhv;
    register char *s;
    register I32 i;
    register I32 hash;
    register HE *entry;
    SV *sv;

    if (!hv)
	return 0;

    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv,'P')) {
	    sv = sv_newmortal();
	    mg_copy((SV*)hv, sv, key, klen);
	    Sv = sv;
	    return &Sv;
	}
    }

    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array) {
	if (lval 
#ifdef DYNAMIC_ENV_FETCH  /* if it's an %ENV lookup, we may get it on the fly */
	         || (HvNAME(hv) && strEQ(HvNAME(hv),ENV_HV_NAME))
#endif
	                                                          )
	    Newz(503,xhv->xhv_array, sizeof(HE*) * (xhv->xhv_max + 1), char);
	else
	    return 0;
    }

    i = klen;
    hash = 0;
    s = key;
    while (i--)
	hash = hash * 33 + *s++;

    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (entry->hent_klen != klen)
	    continue;
	if (bcmp(entry->hent_key,key,klen))	/* is this it? */
	    continue;
	return &entry->hent_val;
    }
#ifdef DYNAMIC_ENV_FETCH  /* %ENV lookup?  If so, try to fetch the value now */
    if (HvNAME(hv) && strEQ(HvNAME(hv),ENV_HV_NAME)) {
      char *gotenv;

      gotenv = my_getenv(key);
      if (gotenv != NULL) {
        sv = newSVpv(gotenv,strlen(gotenv));
        return hv_store(hv,key,klen,sv,hash);
      }
    }
#endif
    if (lval) {		/* gonna assign to this, so it better be there */
	sv = NEWSV(61,0);
	return hv_store(hv,key,klen,sv,hash);
    }
    return 0;
}

SV**
hv_store(hv,key,klen,val,hash)
HV *hv;
char *key;
U32 klen;
SV *val;
register U32 hash;
{
    register XPVHV* xhv;
    register char *s;
    register I32 i;
    register HE *entry;
    register HE **oentry;

    if (!hv)
	return 0;

    xhv = (XPVHV*)SvANY(hv);
    if (SvMAGICAL(hv)) {
	mg_copy((SV*)hv, val, key, klen);
#ifndef OVERLOAD
	if (!xhv->xhv_array)
	    return 0;
#else
	if (!xhv->xhv_array && (SvMAGIC(hv)->mg_type != 'A'
				|| SvMAGIC(hv)->mg_moremagic))
	  return 0;
#endif /* OVERLOAD */
    }
    if (!hash) {
    i = klen;
    s = key;
    while (i--)
	hash = hash * 33 + *s++;
    }

    if (!xhv->xhv_array)
	Newz(505, xhv->xhv_array, sizeof(HE**) * (xhv->xhv_max + 1), char);

    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    i = 1;

    for (entry = *oentry; entry; i=0, entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (entry->hent_klen != klen)
	    continue;
	if (bcmp(entry->hent_key,key,klen))	/* is this it? */
	    continue;
	SvREFCNT_dec(entry->hent_val);
	entry->hent_val = val;
	return &entry->hent_val;
    }

    entry = new_he();
    entry->hent_klen = klen;
    entry->hent_key = savepvn(key,klen);
    entry->hent_val = val;
    entry->hent_hash = hash;
    entry->hent_next = *oentry;
    *oentry = entry;

    xhv->xhv_keys++;
    if (i) {				/* initial entry? */
	++xhv->xhv_fill;
	if (xhv->xhv_keys > xhv->xhv_max)
	    hsplit(hv);
    }

    return &entry->hent_val;
}

SV *
hv_delete(hv,key,klen,flags)
HV *hv;
char *key;
U32 klen;
I32 flags;
{
    register XPVHV* xhv;
    register char *s;
    register I32 i;
    register I32 hash;
    register HE *entry;
    register HE **oentry;
    SV *sv;

    if (!hv)
	return Nullsv;
    if (SvRMAGICAL(hv)) {
	sv = *hv_fetch(hv, key, klen, TRUE);
	mg_clear(sv);
	if (mg_find(sv, 'p')) {
	    sv_unmagic(sv, 'p');	/* No longer an element */
	    return sv;
	}
    }
    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array)
	return Nullsv;
    i = klen;
    hash = 0;
    s = key;
    while (i--)
	hash = hash * 33 + *s++;

    oentry = &((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    entry = *oentry;
    i = 1;
    for (; entry; i=0, oentry = &entry->hent_next, entry = *oentry) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (entry->hent_klen != klen)
	    continue;
	if (bcmp(entry->hent_key,key,klen))	/* is this it? */
	    continue;
	*oentry = entry->hent_next;
	if (i && !*oentry)
	    xhv->xhv_fill--;
	if (flags & G_DISCARD)
	    sv = Nullsv;
	else
	    sv = sv_mortalcopy(entry->hent_val);
	if (entry == xhv->xhv_eiter)
	    entry->hent_klen = -1;
	else
	    he_free(entry);
	--xhv->xhv_keys;
	return sv;
    }
    return Nullsv;
}

bool
hv_exists(hv,key,klen)
HV *hv;
char *key;
U32 klen;
{
    register XPVHV* xhv;
    register char *s;
    register I32 i;
    register I32 hash;
    register HE *entry;
    SV *sv;

    if (!hv)
	return 0;

    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv,'P')) {
	    sv = sv_newmortal();
	    mg_copy((SV*)hv, sv, key, klen); 
	    magic_existspack(sv, mg_find(sv, 'p'));
	    return SvTRUE(sv);
	}
    }

    xhv = (XPVHV*)SvANY(hv);
    if (!xhv->xhv_array)
	return 0; 

    i = klen;
    hash = 0;
    s = key;
    while (i--)
	hash = hash * 33 + *s++;

    entry = ((HE**)xhv->xhv_array)[hash & (I32) xhv->xhv_max];
    for (; entry; entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (entry->hent_klen != klen)
	    continue;
	if (bcmp(entry->hent_key,key,klen))	/* is this it? */
	    continue;
	return TRUE;
    }
    return FALSE;
}

static void
hsplit(hv)
HV *hv;
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    I32 oldsize = (I32) xhv->xhv_max + 1; /* sic(k) */
    register I32 newsize = oldsize * 2;
    register I32 i;
    register HE **a;
    register HE **b;
    register HE *entry;
    register HE **oentry;
#ifndef STRANGE_MALLOC
    I32 tmp;
#endif

    a = (HE**)xhv->xhv_array;
    nomemok = TRUE;
#ifdef STRANGE_MALLOC
    Renew(a, newsize, HE*);
#else
    i = newsize * sizeof(HE*);
#define MALLOC_OVERHEAD 16
    tmp = MALLOC_OVERHEAD;
    while (tmp - MALLOC_OVERHEAD < i)
	tmp += tmp;
    tmp -= MALLOC_OVERHEAD;
    tmp /= sizeof(HE*);
    assert(tmp >= newsize);
    New(2,a, tmp, HE*);
    Copy(xhv->xhv_array, a, oldsize, HE*);
    if (oldsize >= 64 && !nice_chunk) {
	nice_chunk = (char*)xhv->xhv_array;
	nice_chunk_size = oldsize * sizeof(HE*) * 2 - MALLOC_OVERHEAD;
    }
    else
	Safefree(xhv->xhv_array);
#endif

    nomemok = FALSE;
    Zero(&a[oldsize], oldsize, HE*);		/* zero 2nd half*/
    xhv->xhv_max = --newsize;
    xhv->xhv_array = (char*)a;

    for (i=0; i<oldsize; i++,a++) {
	if (!*a)				/* non-existent */
	    continue;
	b = a+oldsize;
	for (oentry = a, entry = *a; entry; entry = *oentry) {
	    if ((entry->hent_hash & newsize) != i) {
		*oentry = entry->hent_next;
		entry->hent_next = *b;
		if (!*b)
		    xhv->xhv_fill++;
		*b = entry;
		continue;
	    }
	    else
		oentry = &entry->hent_next;
	}
	if (!*a)				/* everything moved */
	    xhv->xhv_fill--;
    }
}

HV *
newHV()
{
    register HV *hv;
    register XPVHV* xhv;

    hv = (HV*)NEWSV(502,0);
    sv_upgrade((SV *)hv, SVt_PVHV);
    xhv = (XPVHV*)SvANY(hv);
    SvPOK_off(hv);
    SvNOK_off(hv);
    xhv->xhv_max = 7;		/* start with 8 buckets */
    xhv->xhv_fill = 0;
    xhv->xhv_pmroot = 0;
    (void)hv_iterinit(hv);	/* so each() will start off right */
    return hv;
}

void
he_free(hent)
register HE *hent;
{
    if (!hent)
	return;
    SvREFCNT_dec(hent->hent_val);
    Safefree(hent->hent_key);
    del_he(hent);
}

void
he_delayfree(hent)
register HE *hent;
{
    if (!hent)
	return;
    sv_2mortal(hent->hent_val);	/* free between statements */
    Safefree(hent->hent_key);
    del_he(hent);
}

void
hv_clear(hv)
HV *hv;
{
    register XPVHV* xhv;
    if (!hv)
	return;
    xhv = (XPVHV*)SvANY(hv);
    hfreeentries(hv);
    xhv->xhv_fill = 0;
    xhv->xhv_keys = 0;
    if (xhv->xhv_array)
	(void)memzero(xhv->xhv_array, (xhv->xhv_max + 1) * sizeof(HE*));

    if (SvRMAGICAL(hv))
	mg_clear((SV*)hv); 
}

static void
hfreeentries(hv)
HV *hv;
{
    register HE **array;
    register HE *hent;
    register HE *ohent = Null(HE*);
    I32 riter;
    I32 max;

    if (!hv)
	return;
    if (!HvARRAY(hv))
	return;

    riter = 0;
    max = HvMAX(hv);
    array = HvARRAY(hv);
    hent = array[0];
    for (;;) {
	if (hent) {
	    ohent = hent;
	    hent = hent->hent_next;
	    he_free(ohent);
	}
	if (!hent) {
	    if (++riter > max)
		break;
	    hent = array[riter];
	} 
    }
    (void)hv_iterinit(hv);
}

void
hv_undef(hv)
HV *hv;
{
    register XPVHV* xhv;
    if (!hv)
	return;
    xhv = (XPVHV*)SvANY(hv);
    hfreeentries(hv);
    Safefree(xhv->xhv_array);
    if (HvNAME(hv)) {
	Safefree(HvNAME(hv));
	HvNAME(hv) = 0;
    }
    xhv->xhv_array = 0;
    xhv->xhv_max = 7;		/* it's a normal associative array */
    xhv->xhv_fill = 0;
    xhv->xhv_keys = 0;

    if (SvRMAGICAL(hv))
	mg_clear((SV*)hv); 
}

I32
hv_iterinit(hv)
HV *hv;
{
    register XPVHV* xhv = (XPVHV*)SvANY(hv);
    HE *entry = xhv->xhv_eiter;
    if (entry && entry->hent_klen < 0)	/* was deleted earlier? */
	he_free(entry);
    xhv->xhv_riter = -1;
    xhv->xhv_eiter = Null(HE*);
    return xhv->xhv_fill;
}

HE *
hv_iternext(hv)
HV *hv;
{
    register XPVHV* xhv;
    register HE *entry;
    HE *oldentry;
    MAGIC* mg;

    if (!hv)
	croak("Bad associative array");
    xhv = (XPVHV*)SvANY(hv);
    oldentry = entry = xhv->xhv_eiter;

    if (SvRMAGICAL(hv) && (mg = mg_find((SV*)hv,'P'))) {
	SV *key = sv_newmortal();
	if (entry) {
	    sv_usepvn(key, entry->hent_key, entry->hent_klen);
	    entry->hent_key = 0;
	}
	else {
	    xhv->xhv_eiter = entry = new_he();
	    Zero(entry, 1, HE);
	}
	magic_nextpack((SV*) hv,mg,key);
        if (SvOK(key)) {
	    STRLEN len;
	    entry->hent_key = SvPV_force(key, len);
	    entry->hent_klen = len;
	    SvPOK_off(key);
	    SvPVX(key) = 0;
	    return entry;
        }
	if (entry->hent_val)
	    SvREFCNT_dec(entry->hent_val);
	del_he(entry);
	xhv->xhv_eiter = Null(HE*);
	return Null(HE*);
    }

    if (!xhv->xhv_array)
	Newz(506,xhv->xhv_array, sizeof(HE*) * (xhv->xhv_max + 1), char);
    do {
	if (entry)
	    entry = entry->hent_next;
	if (!entry) {
	    ++xhv->xhv_riter;
	    if (xhv->xhv_riter > xhv->xhv_max) {
		xhv->xhv_riter = -1;
		break;
	    }
	    entry = ((HE**)xhv->xhv_array)[xhv->xhv_riter];
	}
    } while (!entry);

    if (oldentry && oldentry->hent_klen < 0)	/* was deleted earlier? */
	he_free(oldentry);

    xhv->xhv_eiter = entry;
    return entry;
}

char *
hv_iterkey(entry,retlen)
register HE *entry;
I32 *retlen;
{
    *retlen = entry->hent_klen;
    return entry->hent_key;
}

SV *
hv_iterval(hv,entry)
HV *hv;
register HE *entry;
{
    if (SvRMAGICAL(hv)) {
	if (mg_find((SV*)hv,'P')) {
	    SV* sv = sv_newmortal();
	    mg_copy((SV*)hv, sv, entry->hent_key, entry->hent_klen);
	    return sv;
	}
    }
    return entry->hent_val;
}

SV *
hv_iternextsv(hv, key, retlen)
    HV *hv;
    char **key;
    I32 *retlen;
{
    HE *he;
    if ( (he = hv_iternext(hv)) == NULL)
	return NULL;
    *key = hv_iterkey(he, retlen);
    return hv_iterval(hv, he);
}

void
hv_magic(hv, gv, how)
HV* hv;
GV* gv;
int how;
{
    sv_magic((SV*)hv, (SV*)gv, how, Nullch, 0);
}
