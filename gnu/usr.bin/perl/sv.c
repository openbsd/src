/*    sv.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "I wonder what the Entish is for 'yes' and 'no'," he thought.
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef OVR_DBL_DIG
/* Use an overridden DBL_DIG */
# ifdef DBL_DIG
#  undef DBL_DIG
# endif
# define DBL_DIG OVR_DBL_DIG
#else
/* The following is all to get DBL_DIG, in order to pick a nice
   default value for printing floating point numbers in Gconvert.
   (see config.h)
*/
#ifdef I_LIMITS
#include <limits.h>
#endif
#ifdef I_FLOAT
#include <float.h>
#endif
#ifndef HAS_DBL_DIG
#define DBL_DIG	15   /* A guess that works lots of places */
#endif
#endif

#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
#  define FAST_SV_GETS
#endif

static SV *more_sv _((void));
static XPVIV *more_xiv _((void));
static XPVNV *more_xnv _((void));
static XPV *more_xpv _((void));
static XRV *more_xrv _((void));
static SV *new_sv _((void));
static XPVIV *new_xiv _((void));
static XPVNV *new_xnv _((void));
static XPV *new_xpv _((void));
static XRV *new_xrv _((void));
static void del_xiv _((XPVIV* p));
static void del_xnv _((XPVNV* p));
static void del_xpv _((XPV* p));
static void del_xrv _((XRV* p));
static void sv_mortalgrow _((void));

static void sv_unglob _((SV* sv));

#ifdef PURIFY

#define new_SV() sv = (SV*)safemalloc(sizeof(SV))
#define del_SV(p) free((char*)p)

void
sv_add_arena(ptr, size, flags)
char* ptr;
U32 size;
U32 flags;
{
    if (!(flags & SVf_FAKE))
	free(ptr);
}

#else

#define new_SV()			\
    if (sv_root) {			\
	sv = sv_root;			\
	sv_root = (SV*)SvANY(sv);	\
	++sv_count;			\
    }					\
    else				\
	sv = more_sv();

static SV*
new_sv()
{
    SV* sv;
    if (sv_root) {
	sv = sv_root;
	sv_root = (SV*)SvANY(sv);
	++sv_count;
	return sv;
    }
    return more_sv();
}

#ifdef DEBUGGING
#define del_SV(p)			\
    if (debug & 32768)			\
	del_sv(p);			\
    else {				\
	SvANY(p) = (void *)sv_root;	\
	sv_root = p;			\
	--sv_count;			\
    }

static void
del_sv(p)
SV* p;
{
    if (debug & 32768) {
	SV* sva;
	SV* sv;
	SV* svend;
	int ok = 0;
	for (sva = sv_arenaroot; sva; sva = (SV *) SvANY(sva)) {
	    sv = sva + 1;
	    svend = &sva[SvREFCNT(sva)];
	    if (p >= sv && p < svend)
		ok = 1;
	}
	if (!ok) {
	    warn("Attempt to free non-arena SV: 0x%lx", (unsigned long)p);
	    return;
	}
    }
    SvANY(p) = (void *) sv_root;
    sv_root = p;
    --sv_count;
}
#else
#define del_SV(p)			\
    SvANY(p) = (void *)sv_root;		\
    sv_root = p;			\
    --sv_count;

#endif

void
sv_add_arena(ptr, size, flags)
char* ptr;
U32 size;
U32 flags;
{
    SV* sva = (SV*)ptr;
    register SV* sv;
    register SV* svend;
    Zero(sva, size, char);

    /* The first SV in an arena isn't an SV. */
    SvANY(sva) = (void *) sv_arenaroot;		/* ptr to next arena */
    SvREFCNT(sva) = size / sizeof(SV);		/* number of SV slots */
    SvFLAGS(sva) = flags;			/* FAKE if not to be freed */

    sv_arenaroot = sva;
    sv_root = sva + 1;

    svend = &sva[SvREFCNT(sva) - 1];
    sv = sva + 1;
    while (sv < svend) {
	SvANY(sv) = (void *)(SV*)(sv + 1);
	SvFLAGS(sv) = SVTYPEMASK;
	sv++;
    }
    SvANY(sv) = 0;
    SvFLAGS(sv) = SVTYPEMASK;
}

static SV*
more_sv()
{
    if (nice_chunk) {
	sv_add_arena(nice_chunk, nice_chunk_size, 0);
	nice_chunk = Nullch;
    }
    else
	sv_add_arena(safemalloc(1008), 1008, 0);
    return new_sv();
}
#endif

void
sv_report_used()
{
    SV* sva;
    SV* sv;
    register SV* svend;

    for (sva = sv_arenaroot; sva; sva = (SV *) SvANY(sva)) {
	sv = sva + 1;
	svend = &sva[SvREFCNT(sva)];
	while (sv < svend) {
	    if (SvTYPE(sv) != SVTYPEMASK) {
		fprintf(stderr, "****\n");
		sv_dump(sv);
	    }
	    ++sv;
	}
    }
}

void
sv_clean_objs()
{
    SV* sva;
    register SV* sv;
    register SV* svend;
    SV* rv;

#ifndef DISABLE_DESTRUCTOR_KLUDGE
    register GV* gv;
    for (sva = sv_arenaroot; sva; sva = (SV *) SvANY(sva)) {
	gv = sva + 1;
	svend = &sva[SvREFCNT(sva)];
	while (gv < svend) {
	    if (SvTYPE(gv) == SVt_PVGV && (sv = GvSV(gv)) &&
		SvROK(sv) && SvOBJECT(rv = SvRV(sv)))
	    {
		DEBUG_D((fprintf(stderr, "Cleaning object ref:\n "),
			 sv_dump(sv));)
		SvROK_off(sv);
		SvRV(sv) = 0;
		SvREFCNT_dec(rv);
	    }
	    ++gv;
	}
    }
    if (!sv_objcount)
	return;
#endif
    for (sva = sv_arenaroot; sva; sva = (SV *) SvANY(sva)) {
	sv = sva + 1;
	svend = &sva[SvREFCNT(sva)];
	while (sv < svend) {
	    if (SvROK(sv) && SvOBJECT(rv = SvRV(sv))) {
		DEBUG_D((fprintf(stderr, "Cleaning object ref:\n "),
			 sv_dump(sv));)
		SvROK_off(sv);
		SvRV(sv) = 0;
		SvREFCNT_dec(rv);
	    }
	    /* XXX Might want to check arrays, etc. */
	    ++sv;
	}
    }
}

void
sv_clean_all()
{
    SV* sva;
    register SV* sv;
    register SV* svend;

    for (sva = sv_arenaroot; sva; sva = (SV*) SvANY(sva)) {
	sv = sva + 1;
	svend = &sva[SvREFCNT(sva)];
	while (sv < svend) {
	    if (SvTYPE(sv) != SVTYPEMASK) {
		DEBUG_D((fprintf(stderr, "Cleaning loops:\n "), sv_dump(sv));)
		SvFLAGS(sv) |= SVf_BREAK;
		SvREFCNT_dec(sv);
	    }
	    ++sv;
	}
    }
}

void
sv_free_arenas()
{
    SV* sva;
    SV* svanext;

    /* Free arenas here, but be careful about fake ones.  (We assume
       contiguity of the fake ones with the corresponding real ones.) */

    for (sva = sv_arenaroot; sva; sva = svanext) {
	svanext = (SV*) SvANY(sva);
	while (svanext && SvFAKE(svanext))
	    svanext = (SV*) SvANY(svanext);

	if (!SvFAKE(sva))
	    Safefree(sva);
    }
}

static XPVIV*
new_xiv()
{
    IV** xiv;
    if (xiv_root) {
	xiv = xiv_root;
	/*
	 * See comment in more_xiv() -- RAM.
	 */
	xiv_root = (IV**)*xiv;
	return (XPVIV*)((char*)xiv - sizeof(XPV));
    }
    return more_xiv();
}

static void
del_xiv(p)
XPVIV* p;
{
    IV** xiv = (IV**)((char*)(p) + sizeof(XPV));
    *xiv = (IV *)xiv_root;
    xiv_root = xiv;
}

static XPVIV*
more_xiv()
{
    register IV** xiv;
    register IV** xivend;
    XPV* ptr = (XPV*)safemalloc(1008);
    ptr->xpv_pv = (char*)xiv_arenaroot;		/* linked list of xiv arenas */
    xiv_arenaroot = ptr;			/* to keep Purify happy */

    xiv = (IV**) ptr;
    xivend = &xiv[1008 / sizeof(IV *) - 1];
    xiv += (sizeof(XPV) - 1) / sizeof(IV *) + 1;   /* fudge by size of XPV */
    xiv_root = xiv;
    while (xiv < xivend) {
	*xiv = (IV *)(xiv + 1);
	xiv++;
    }
    *xiv = 0;
    return new_xiv();
}

static XPVNV*
new_xnv()
{
    double* xnv;
    if (xnv_root) {
	xnv = xnv_root;
	xnv_root = *(double**)xnv;
	return (XPVNV*)((char*)xnv - sizeof(XPVIV));
    }
    return more_xnv();
}

static void
del_xnv(p)
XPVNV* p;
{
    double* xnv = (double*)((char*)(p) + sizeof(XPVIV));
    *(double**)xnv = xnv_root;
    xnv_root = xnv;
}

static XPVNV*
more_xnv()
{
    register double* xnv;
    register double* xnvend;
    xnv = (double*)safemalloc(1008);
    xnvend = &xnv[1008 / sizeof(double) - 1];
    xnv += (sizeof(XPVIV) - 1) / sizeof(double) + 1; /* fudge by sizeof XPVIV */
    xnv_root = xnv;
    while (xnv < xnvend) {
	*(double**)xnv = (double*)(xnv + 1);
	xnv++;
    }
    *(double**)xnv = 0;
    return new_xnv();
}

static XRV*
new_xrv()
{
    XRV* xrv;
    if (xrv_root) {
	xrv = xrv_root;
	xrv_root = (XRV*)xrv->xrv_rv;
	return xrv;
    }
    return more_xrv();
}

static void
del_xrv(p)
XRV* p;
{
    p->xrv_rv = (SV*)xrv_root;
    xrv_root = p;
}

static XRV*
more_xrv()
{
    register XRV* xrv;
    register XRV* xrvend;
    xrv_root = (XRV*)safemalloc(1008);
    xrv = xrv_root;
    xrvend = &xrv[1008 / sizeof(XRV) - 1];
    while (xrv < xrvend) {
	xrv->xrv_rv = (SV*)(xrv + 1);
	xrv++;
    }
    xrv->xrv_rv = 0;
    return new_xrv();
}

static XPV*
new_xpv()
{
    XPV* xpv;
    if (xpv_root) {
	xpv = xpv_root;
	xpv_root = (XPV*)xpv->xpv_pv;
	return xpv;
    }
    return more_xpv();
}

static void
del_xpv(p)
XPV* p;
{
    p->xpv_pv = (char*)xpv_root;
    xpv_root = p;
}

static XPV*
more_xpv()
{
    register XPV* xpv;
    register XPV* xpvend;
    xpv_root = (XPV*)safemalloc(1008);
    xpv = xpv_root;
    xpvend = &xpv[1008 / sizeof(XPV) - 1];
    while (xpv < xpvend) {
	xpv->xpv_pv = (char*)(xpv + 1);
	xpv++;
    }
    xpv->xpv_pv = 0;
    return new_xpv();
}

#ifdef PURIFY
#define new_XIV() (void*)safemalloc(sizeof(XPVIV))
#define del_XIV(p) free((char*)p)
#else
#define new_XIV() (void*)new_xiv()
#define del_XIV(p) del_xiv(p)
#endif

#ifdef PURIFY
#define new_XNV() (void*)safemalloc(sizeof(XPVNV))
#define del_XNV(p) free((char*)p)
#else
#define new_XNV() (void*)new_xnv()
#define del_XNV(p) del_xnv(p)
#endif

#ifdef PURIFY
#define new_XRV() (void*)safemalloc(sizeof(XRV))
#define del_XRV(p) free((char*)p)
#else
#define new_XRV() (void*)new_xrv()
#define del_XRV(p) del_xrv(p)
#endif

#ifdef PURIFY
#define new_XPV() (void*)safemalloc(sizeof(XPV))
#define del_XPV(p) free((char*)p)
#else
#define new_XPV() (void*)new_xpv()
#define del_XPV(p) del_xpv(p)
#endif

#define new_XPVIV() (void*)safemalloc(sizeof(XPVIV))
#define del_XPVIV(p) free((char*)p)

#define new_XPVNV() (void*)safemalloc(sizeof(XPVNV))
#define del_XPVNV(p) free((char*)p)

#define new_XPVMG() (void*)safemalloc(sizeof(XPVMG))
#define del_XPVMG(p) free((char*)p)

#define new_XPVLV() (void*)safemalloc(sizeof(XPVLV))
#define del_XPVLV(p) free((char*)p)

#define new_XPVAV() (void*)safemalloc(sizeof(XPVAV))
#define del_XPVAV(p) free((char*)p)

#define new_XPVHV() (void*)safemalloc(sizeof(XPVHV))
#define del_XPVHV(p) free((char*)p)

#define new_XPVCV() (void*)safemalloc(sizeof(XPVCV))
#define del_XPVCV(p) free((char*)p)

#define new_XPVGV() (void*)safemalloc(sizeof(XPVGV))
#define del_XPVGV(p) free((char*)p)

#define new_XPVBM() (void*)safemalloc(sizeof(XPVBM))
#define del_XPVBM(p) free((char*)p)

#define new_XPVFM() (void*)safemalloc(sizeof(XPVFM))
#define del_XPVFM(p) free((char*)p)

#define new_XPVIO() (void*)safemalloc(sizeof(XPVIO))
#define del_XPVIO(p) free((char*)p)

bool
sv_upgrade(sv, mt)
register SV* sv;
U32 mt;
{
    char*	pv;
    U32		cur;
    U32		len;
    IV		iv;
    double	nv;
    MAGIC*	magic;
    HV*		stash;

    if (SvTYPE(sv) == mt)
	return TRUE;

    if (mt < SVt_PVIV)
	(void)SvOOK_off(sv);

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	pv	= 0;
	cur	= 0;
	len	= 0;
	iv	= 0;
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	break;
    case SVt_IV:
	pv	= 0;
	cur	= 0;
	len	= 0;
	iv	= SvIVX(sv);
	nv	= (double)SvIVX(sv);
	del_XIV(SvANY(sv));
	magic	= 0;
	stash	= 0;
	if (mt == SVt_NV)
	    mt = SVt_PVNV;
	else if (mt < SVt_PVIV)
	    mt = SVt_PVIV;
	break;
    case SVt_NV:
	pv	= 0;
	cur	= 0;
	len	= 0;
	nv	= SvNVX(sv);
	iv	= I_32(nv);
	magic	= 0;
	stash	= 0;
	del_XNV(SvANY(sv));
	SvANY(sv) = 0;
	if (mt < SVt_PVNV)
	    mt = SVt_PVNV;
	break;
    case SVt_RV:
	pv	= (char*)SvRV(sv);
	cur	= 0;
	len	= 0;
	iv	= (IV)pv;
	nv	= (double)(unsigned long)pv;
	del_XRV(SvANY(sv));
	magic	= 0;
	stash	= 0;
	break;
    case SVt_PV:
	nv = 0.0;
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= 0;
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	del_XPV(SvANY(sv));
	if (mt <= SVt_IV)
	    mt = SVt_PVIV;
	else if (mt == SVt_NV)
	    mt = SVt_PVNV;
	break;
    case SVt_PVIV:
	nv = 0.0;
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	del_XPVIV(SvANY(sv));
	break;
    case SVt_PVNV:
	nv = SvNVX(sv);
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= SvNVX(sv);
	magic	= 0;
	stash	= 0;
	del_XPVNV(SvANY(sv));
	break;
    case SVt_PVMG:
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= SvNVX(sv);
	magic	= SvMAGIC(sv);
	stash	= SvSTASH(sv);
	del_XPVMG(SvANY(sv));
	break;
    default:
	croak("Can't upgrade that kind of scalar");
    }

    switch (mt) {
    case SVt_NULL:
	croak("Can't upgrade to undef");
    case SVt_IV:
	SvANY(sv) = new_XIV();
	SvIVX(sv)	= iv;
	break;
    case SVt_NV:
	SvANY(sv) = new_XNV();
	SvNVX(sv)	= nv;
	break;
    case SVt_RV:
	SvANY(sv) = new_XRV();
	SvRV(sv) = (SV*)pv;
	break;
    case SVt_PV:
	SvANY(sv) = new_XPV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	break;
    case SVt_PVIV:
	SvANY(sv) = new_XPVIV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	if (SvNIOK(sv))
	    (void)SvIOK_on(sv);
	SvNOK_off(sv);
	break;
    case SVt_PVNV:
	SvANY(sv) = new_XPVNV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	break;
    case SVt_PVMG:
	SvANY(sv) = new_XPVMG();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVLV:
	SvANY(sv) = new_XPVLV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	LvTARGOFF(sv)	= 0;
	LvTARGLEN(sv)	= 0;
	LvTARG(sv)	= 0;
	LvTYPE(sv)	= 0;
	break;
    case SVt_PVAV:
	SvANY(sv) = new_XPVAV();
	if (pv)
	    Safefree(pv);
	SvPVX(sv)	= 0;
	AvMAX(sv)	= 0;
	AvFILL(sv)	= 0;
	SvIVX(sv)	= 0;
	SvNVX(sv)	= 0.0;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	AvALLOC(sv)	= 0;
	AvARYLEN(sv)	= 0;
	AvFLAGS(sv)	= 0;
	break;
    case SVt_PVHV:
	SvANY(sv) = new_XPVHV();
	if (pv)
	    Safefree(pv);
	SvPVX(sv)	= 0;
	HvFILL(sv)	= 0;
	HvMAX(sv)	= 0;
	HvKEYS(sv)	= 0;
	SvNVX(sv)	= 0.0;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	HvRITER(sv)	= 0;
	HvEITER(sv)	= 0;
	HvPMROOT(sv)	= 0;
	HvNAME(sv)	= 0;
	break;
    case SVt_PVCV:
	SvANY(sv) = new_XPVCV();
	Zero(SvANY(sv), 1, XPVCV);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVGV:
	SvANY(sv) = new_XPVGV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	GvGP(sv)	= 0;
	GvNAME(sv)	= 0;
	GvNAMELEN(sv)	= 0;
	GvSTASH(sv)	= 0;
	GvFLAGS(sv)	= 0;
	break;
    case SVt_PVBM:
	SvANY(sv) = new_XPVBM();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	BmRARE(sv)	= 0;
	BmUSEFUL(sv)	= 0;
	BmPREVIOUS(sv)	= 0;
	break;
    case SVt_PVFM:
	SvANY(sv) = new_XPVFM();
	Zero(SvANY(sv), 1, XPVFM);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVIO:
	SvANY(sv) = new_XPVIO();
	Zero(SvANY(sv), 1, XPVIO);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	IoPAGE_LEN(sv)	= 60;
	break;
    }
    SvFLAGS(sv) &= ~SVTYPEMASK;
    SvFLAGS(sv) |= mt;
    return TRUE;
}

#ifdef DEBUGGING
char *
sv_peek(sv)
register SV *sv;
{
    char *t = tokenbuf;
    int unref = 0;

  retry:
    if (!sv) {
	strcpy(t, "VOID");
	goto finish;
    }
    else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
	strcpy(t, "WILD");
	goto finish;
    }
    else if (sv == &sv_undef || sv == &sv_no || sv == &sv_yes) {
	if (sv == &sv_undef) {
	    strcpy(t, "SV_UNDEF");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	else if (sv == &sv_no) {
	    strcpy(t, "SV_NO");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 0 &&
		SvNVX(sv) == 0.0)
		goto finish;
	}
	else {
	    strcpy(t, "SV_YES");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 1 &&
		SvPVX(sv) && *SvPVX(sv) == '1' &&
		SvNVX(sv) == 1.0)
		goto finish;
	}
	t += strlen(t);
	*t++ = ':';
    }
    else if (SvREFCNT(sv) == 0) {
	*t++ = '(';
	unref++;
    }
    if (SvROK(sv)) {
	*t++ = '\\';
	if (t - tokenbuf + unref > 10) {
	    strcpy(tokenbuf + unref + 3,"...");
	    goto finish;
	}
	sv = (SV*)SvRV(sv);
	goto retry;
    }
    switch (SvTYPE(sv)) {
    default:
	strcpy(t,"FREED");
	goto finish;

    case SVt_NULL:
	strcpy(t,"UNDEF");
	return tokenbuf;
    case SVt_IV:
	strcpy(t,"IV");
	break;
    case SVt_NV:
	strcpy(t,"NV");
	break;
    case SVt_RV:
	strcpy(t,"RV");
	break;
    case SVt_PV:
	strcpy(t,"PV");
	break;
    case SVt_PVIV:
	strcpy(t,"PVIV");
	break;
    case SVt_PVNV:
	strcpy(t,"PVNV");
	break;
    case SVt_PVMG:
	strcpy(t,"PVMG");
	break;
    case SVt_PVLV:
	strcpy(t,"PVLV");
	break;
    case SVt_PVAV:
	strcpy(t,"AV");
	break;
    case SVt_PVHV:
	strcpy(t,"HV");
	break;
    case SVt_PVCV:
	if (CvGV(sv))
	    sprintf(t, "CV(%s)", GvNAME(CvGV(sv)));
	else
	    strcpy(t, "CV()");
	goto finish;
    case SVt_PVGV:
	strcpy(t,"GV");
	break;
    case SVt_PVBM:
	strcpy(t,"BM");
	break;
    case SVt_PVFM:
	strcpy(t,"FM");
	break;
    case SVt_PVIO:
	strcpy(t,"IO");
	break;
    }
    t += strlen(t);

    if (SvPOKp(sv)) {
	if (!SvPVX(sv))
	    strcpy(t, "(null)");
	if (SvOOK(sv))
	    sprintf(t,"(%ld+\"%.127s\")",(long)SvIVX(sv),SvPVX(sv));
	else
	    sprintf(t,"(\"%.127s\")",SvPVX(sv));
    }
    else if (SvNOKp(sv))
	sprintf(t,"(%g)",SvNVX(sv));
    else if (SvIOKp(sv))
	sprintf(t,"(%ld)",(long)SvIVX(sv));
    else
	strcpy(t,"()");
    
  finish:
    if (unref) {
	t += strlen(t);
	while (unref--)
	    *t++ = ')';
	*t = '\0';
    }
    return tokenbuf;
}
#endif

int
sv_backoff(sv)
register SV *sv;
{
    assert(SvOOK(sv));
    if (SvIVX(sv)) {
	char *s = SvPVX(sv);
	SvLEN(sv) += SvIVX(sv);
	SvPVX(sv) -= SvIVX(sv);
	SvIV_set(sv, 0);
	Move(s, SvPVX(sv), SvCUR(sv)+1, char);
    }
    SvFLAGS(sv) &= ~SVf_OOK;
    return 0;
}

char *
sv_grow(sv,newlen)
register SV *sv;
#ifndef DOSISH
register I32 newlen;
#else
unsigned long newlen;
#endif
{
    register char *s;

#ifdef MSDOS
    if (newlen >= 0x10000) {
	fprintf(stderr, "Allocation too large: %lx\n", newlen);
	my_exit(1);
    }
#endif /* MSDOS */
    if (SvROK(sv))
	sv_unref(sv);
    if (SvTYPE(sv) < SVt_PV) {
	sv_upgrade(sv, SVt_PV);
	s = SvPVX(sv);
    }
    else if (SvOOK(sv)) {	/* pv is offset? */
	sv_backoff(sv);
	s = SvPVX(sv);
	if (newlen > SvLEN(sv))
	    newlen += 10 * (newlen - SvCUR(sv)); /* avoid copy each time */
    }
    else
	s = SvPVX(sv);
    if (newlen > SvLEN(sv)) {		/* need more room? */
        if (SvLEN(sv) && s)
	    Renew(s,newlen,char);
        else
	    New(703,s,newlen,char);
	SvPV_set(sv, s);
        SvLEN_set(sv, newlen);
    }
    return s;
}

void
sv_setiv(sv,i)
register SV *sv;
IV i;
{
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	sv_upgrade(sv, SVt_IV);
	break;
    case SVt_NV:
	sv_upgrade(sv, SVt_PVNV);
	break;
    case SVt_RV:
    case SVt_PV:
	sv_upgrade(sv, SVt_PVIV);
	break;

    case SVt_PVGV:
	if (SvFAKE(sv)) {
	    sv_unglob(sv);
	    break;
	}
	/* FALL THROUGH */
    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVFM:
    case SVt_PVIO:
	croak("Can't coerce %s to integer in %s", sv_reftype(sv,0),
	    op_name[op->op_type]);
    }
    (void)SvIOK_only(sv);			/* validate number */
    SvIVX(sv) = i;
    SvTAINT(sv);
}

void
sv_setnv(sv,num)
register SV *sv;
double num;
{
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
    case SVt_IV:
	sv_upgrade(sv, SVt_NV);
	break;
    case SVt_NV:
    case SVt_RV:
    case SVt_PV:
    case SVt_PVIV:
	sv_upgrade(sv, SVt_PVNV);
	/* FALL THROUGH */
    case SVt_PVNV:
    case SVt_PVMG:
    case SVt_PVBM:
    case SVt_PVLV:
	if (SvOOK(sv))
	    (void)SvOOK_off(sv);
	break;
    case SVt_PVGV:
	if (SvFAKE(sv)) {
	    sv_unglob(sv);
	    break;
	}
	/* FALL THROUGH */
    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVFM:
    case SVt_PVIO:
	croak("Can't coerce %s to number in %s", sv_reftype(sv,0),
	    op_name[op->op_type]);
    }
    SvNVX(sv) = num;
    (void)SvNOK_only(sv);			/* validate number */
    SvTAINT(sv);
}

static void
not_a_number(sv)
SV *sv;
{
    char tmpbuf[64];
    char *d = tmpbuf;
    char *s;
    int i;

    for (s = SvPVX(sv), i = 50; *s && i; s++,i--) {
	int ch = *s;
	if (ch & 128 && !isprint(ch)) {
	    *d++ = 'M';
	    *d++ = '-';
	    ch &= 127;
	}
	if (isprint(ch))
	    *d++ = ch;
	else {
	    *d++ = '^';
	    *d++ = ch ^ 64;
	}
    }
    if (*s) {
	*d++ = '.';
	*d++ = '.';
	*d++ = '.';
    }
    *d = '\0';

    if (op)
	warn("Argument \"%s\" isn't numeric in %s", tmpbuf,
		op_name[op->op_type]);
    else
	warn("Argument \"%s\" isn't numeric", tmpbuf);
}

IV
sv_2iv(sv)
register SV *sv;
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvIOKp(sv))
	    return SvIVX(sv);
	if (SvNOKp(sv)) {
	    if (SvNVX(sv) < 0.0)
		return I_V(SvNVX(sv));
	    else
		return (IV) U_V(SvNVX(sv));
	}
	if (SvPOKp(sv) && SvLEN(sv)) {
	    if (dowarn && !looks_like_number(sv))
		not_a_number(sv);
	    return (IV)atol(SvPVX(sv));
	}
        if (!SvROK(sv)) {
            return 0;
        }
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
#ifdef OVERLOAD
	  SV* tmpstr;
	  if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv, numer)))
	    return SvIV(tmpstr);
#endif /* OVERLOAD */
	  return (IV)SvRV(sv);
	}
	if (SvREADONLY(sv)) {
	    if (SvNOKp(sv)) {
		if (SvNVX(sv) < 0.0)
		    return I_V(SvNVX(sv));
		else
		    return (IV) U_V(SvNVX(sv));
	    }
	    if (SvPOKp(sv) && SvLEN(sv)) {
		if (dowarn && !looks_like_number(sv))
		    not_a_number(sv);
		return (IV)atol(SvPVX(sv));
	    }
	    if (dowarn)
		warn(warn_uninit);
	    return 0;
	}
    }
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	sv_upgrade(sv, SVt_IV);
	return SvIVX(sv);
    case SVt_PV:
	sv_upgrade(sv, SVt_PVIV);
	break;
    case SVt_NV:
	sv_upgrade(sv, SVt_PVNV);
	break;
    }
    if (SvNOKp(sv)) {
	(void)SvIOK_on(sv);
	if (SvNVX(sv) < 0.0)
	    SvIVX(sv) = I_V(SvNVX(sv));
	else
	    SvIVX(sv) = (IV) U_V(SvNVX(sv));
    }
    else if (SvPOKp(sv) && SvLEN(sv)) {
	if (dowarn && !looks_like_number(sv))
	    not_a_number(sv);
	(void)SvIOK_on(sv);
	SvIVX(sv) = (IV)atol(SvPVX(sv));
    }
    else  {
	if (dowarn && !localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    warn(warn_uninit);
	return 0;
    }
    DEBUG_c(fprintf(stderr,"0x%lx 2iv(%ld)\n",
	(unsigned long)sv,(long)SvIVX(sv)));
    return SvIVX(sv);
}

double
sv_2nv(sv)
register SV *sv;
{
    if (!sv)
	return 0.0;
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvNOKp(sv))
	    return SvNVX(sv);
	if (SvPOKp(sv) && SvLEN(sv)) {
	    if (dowarn && !SvIOKp(sv) && !looks_like_number(sv))
		not_a_number(sv);
	    return atof(SvPVX(sv));
	}
	if (SvIOKp(sv))
	    return (double)SvIVX(sv);
        if (!SvROK(sv)) {
            return 0;
        }
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
#ifdef OVERLOAD
	  SV* tmpstr;
	  if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,numer)))
	    return SvNV(tmpstr);
#endif /* OVERLOAD */
	  return (double)(unsigned long)SvRV(sv);
	}
	if (SvREADONLY(sv)) {
	    if (SvPOKp(sv) && SvLEN(sv)) {
		if (dowarn && !SvIOKp(sv) && !looks_like_number(sv))
		    not_a_number(sv);
		return atof(SvPVX(sv));
	    }
	    if (SvIOKp(sv))
		return (double)SvIVX(sv);
	    if (dowarn)
		warn(warn_uninit);
	    return 0.0;
	}
    }
    if (SvTYPE(sv) < SVt_NV) {
	if (SvTYPE(sv) == SVt_IV)
	    sv_upgrade(sv, SVt_PVNV);
	else
	    sv_upgrade(sv, SVt_NV);
	DEBUG_c(fprintf(stderr,"0x%lx num(%g)\n",(unsigned long)sv,SvNVX(sv)));
    }
    else if (SvTYPE(sv) < SVt_PVNV)
	sv_upgrade(sv, SVt_PVNV);
    if (SvIOKp(sv) &&
	    (!SvPOKp(sv) || !strchr(SvPVX(sv),'.') || !looks_like_number(sv)))
    {
	SvNVX(sv) = (double)SvIVX(sv);
    }
    else if (SvPOKp(sv) && SvLEN(sv)) {
	if (dowarn && !SvIOKp(sv) && !looks_like_number(sv))
	    not_a_number(sv);
	SvNVX(sv) = atof(SvPVX(sv));
    }
    else  {
	if (dowarn && !localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    warn(warn_uninit);
	return 0.0;
    }
    SvNOK_on(sv);
    DEBUG_c(fprintf(stderr,"0x%lx 2nv(%g)\n",(unsigned long)sv,SvNVX(sv)));
    return SvNVX(sv);
}

char *
sv_2pv(sv, lp)
register SV *sv;
STRLEN *lp;
{
    register char *s;
    int olderrno;

    if (!sv) {
	*lp = 0;
	return "";
    }
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvPOKp(sv)) {
	    *lp = SvCUR(sv);
	    return SvPVX(sv);
	}
	if (SvIOKp(sv)) {
	    (void)sprintf(tokenbuf,"%ld",(long)SvIVX(sv));
	    goto tokensave;
	}
	if (SvNOKp(sv)) {
	    Gconvert(SvNVX(sv), DBL_DIG, 0, tokenbuf);
	    goto tokensave;
	}
        if (!SvROK(sv)) {
            *lp = 0;
            return "";
        }
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
#ifdef OVERLOAD
	    SV* tmpstr;
	    if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,string)))
	      return SvPV(tmpstr,*lp);
#endif /* OVERLOAD */
	    sv = (SV*)SvRV(sv);
	    if (!sv)
		s = "NULLREF";
	    else {
		switch (SvTYPE(sv)) {
		case SVt_NULL:
		case SVt_IV:
		case SVt_NV:
		case SVt_RV:
		case SVt_PV:
		case SVt_PVIV:
		case SVt_PVNV:
		case SVt_PVBM:
		case SVt_PVMG:	s = "SCALAR";			break;
		case SVt_PVLV:	s = "LVALUE";			break;
		case SVt_PVAV:	s = "ARRAY";			break;
		case SVt_PVHV:	s = "HASH";			break;
		case SVt_PVCV:	s = "CODE";			break;
		case SVt_PVGV:	s = "GLOB";			break;
		case SVt_PVFM:	s = "FORMATLINE";		break;
		case SVt_PVIO:	s = "FILEHANDLE";		break;
		default:	s = "UNKNOWN";			break;
		}
		if (SvOBJECT(sv))
		    sprintf(tokenbuf, "%s=%s(0x%lx)",
				HvNAME(SvSTASH(sv)), s, (unsigned long)sv);
		else
		    sprintf(tokenbuf, "%s(0x%lx)", s, (unsigned long)sv);
		goto tokensaveref;
	    }
	    *lp = strlen(s);
	    return s;
	}
	if (SvREADONLY(sv)) {
	    if (SvNOKp(sv)) {
		Gconvert(SvNVX(sv), DBL_DIG, 0, tokenbuf);
		goto tokensave;
	    }
	    if (SvIOKp(sv)) {
		(void)sprintf(tokenbuf,"%ld",(long)SvIVX(sv));
		goto tokensave;
	    }
	    if (dowarn)
		warn(warn_uninit);
	    *lp = 0;
	    return "";
	}
    }
    if (!SvUPGRADE(sv, SVt_PV))
	return 0;
    if (SvNOKp(sv)) {
	if (SvTYPE(sv) < SVt_PVNV)
	    sv_upgrade(sv, SVt_PVNV);
	SvGROW(sv, 28);
	s = SvPVX(sv);
	olderrno = errno;	/* some Xenix systems wipe out errno here */
#ifdef apollo
	if (SvNVX(sv) == 0.0)
	    (void)strcpy(s,"0");
	else
#endif /*apollo*/
	    Gconvert(SvNVX(sv), DBL_DIG, 0, s);
	errno = olderrno;
#ifdef FIXNEGATIVEZERO
        if (*s == '-' && s[1] == '0' && !s[2])
	    strcpy(s,"0");
#endif
	while (*s) s++;
#ifdef hcx
	if (s[-1] == '.')
	    s--;
#endif
    }
    else if (SvIOKp(sv)) {
	if (SvTYPE(sv) < SVt_PVIV)
	    sv_upgrade(sv, SVt_PVIV);
	SvGROW(sv, 11);
	s = SvPVX(sv);
	olderrno = errno;	/* some Xenix systems wipe out errno here */
	(void)sprintf(s,"%ld",(long)SvIVX(sv));
	errno = olderrno;
	while (*s) s++;
    }
    else {
	if (dowarn && !localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    warn(warn_uninit);
	*lp = 0;
	return "";
    }
    *s = '\0';
    *lp = s - SvPVX(sv);
    SvCUR_set(sv, *lp);
    SvPOK_on(sv);
    DEBUG_c(fprintf(stderr,"0x%lx 2pv(%s)\n",(unsigned long)sv,SvPVX(sv)));
    return SvPVX(sv);

  tokensave:
    if (SvROK(sv)) {	/* XXX Skip this when sv_pvn_force calls */
	/* Sneaky stuff here */

      tokensaveref:
	sv = sv_newmortal();
	*lp = strlen(tokenbuf);
	sv_setpvn(sv, tokenbuf, *lp);
	return SvPVX(sv);
    }
    else {
	STRLEN len;
	
#ifdef FIXNEGATIVEZERO
	if (*tokenbuf == '-' && tokenbuf[1] == '0' && !tokenbuf[2])
	    strcpy(tokenbuf,"0");
#endif
	(void)SvUPGRADE(sv, SVt_PV);
	len = *lp = strlen(tokenbuf);
	s = SvGROW(sv, len + 1);
	SvCUR_set(sv, len);
	(void)strcpy(s, tokenbuf);
	/* NO SvPOK_on(sv) here! */
	return s;
    }
}

/* This function is only called on magical items */
bool
sv_2bool(sv)
register SV *sv;
{
    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv))
	return 0;
    if (SvROK(sv)) {
#ifdef OVERLOAD
      {
	SV* tmpsv;
	if (SvAMAGIC(sv) && (tmpsv = AMG_CALLun(sv,bool_)))
	  return SvTRUE(tmpsv);
      }
#endif /* OVERLOAD */
      return SvRV(sv) != 0;
    }
    if (SvPOKp(sv)) {
	register XPV* Xpv;
	if ((Xpv = (XPV*)SvANY(sv)) &&
		(*Xpv->xpv_pv > '0' ||
		Xpv->xpv_cur > 1 ||
		(Xpv->xpv_cur && *Xpv->xpv_pv != '0')))
	    return 1;
	else
	    return 0;
    }
    else {
	if (SvIOKp(sv))
	    return SvIVX(sv) != 0;
	else {
	    if (SvNOKp(sv))
		return SvNVX(sv) != 0.0;
	    else
		return FALSE;
	}
    }
}

/* Note: sv_setsv() should not be called with a source string that needs
 * to be reused, since it may destroy the source string if it is marked
 * as temporary.
 */

void
sv_setsv(dstr,sstr)
SV *dstr;
register SV *sstr;
{
    register U32 sflags;
    register int dtype;
    register int stype;

    if (sstr == dstr)
	return;
    if (SvTHINKFIRST(dstr)) {
	if (SvREADONLY(dstr) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(dstr))
	    sv_unref(dstr);
    }
    if (!sstr)
	sstr = &sv_undef;
    stype = SvTYPE(sstr);
    dtype = SvTYPE(dstr);

    if (dtype == SVt_PVGV && (SvFLAGS(dstr) & SVf_FAKE)) {
        sv_unglob(dstr);     /* so fake GLOB won't perpetuate */
	sv_setpvn(dstr, "", 0);
        (void)SvPOK_only(dstr);
        dtype = SvTYPE(dstr);
    }

#ifdef OVERLOAD
    SvAMAGIC_off(dstr);
#endif /* OVERLOAD */
    /* There's a lot of redundancy below but we're going for speed here */

    switch (stype) {
    case SVt_NULL:
	(void)SvOK_off(dstr);
	return;
    case SVt_IV:
	if (dtype <= SVt_PV) {
	    if (dtype < SVt_IV)
		sv_upgrade(dstr, SVt_IV);
	    else if (dtype == SVt_NV)
		sv_upgrade(dstr, SVt_PVNV);
	    else if (dtype <= SVt_PV)
		sv_upgrade(dstr, SVt_PVIV);
	}
	break;
    case SVt_NV:
	if (dtype <= SVt_PVIV) {
	    if (dtype < SVt_NV)
		sv_upgrade(dstr, SVt_NV);
	    else if (dtype == SVt_PVIV)
		sv_upgrade(dstr, SVt_PVNV);
	    else if (dtype <= SVt_PV)
		sv_upgrade(dstr, SVt_PVNV);
	}
	break;
    case SVt_RV:
	if (dtype < SVt_RV)
	    sv_upgrade(dstr, SVt_RV);
	else if (dtype == SVt_PVGV &&
		 SvTYPE(SvRV(sstr)) == SVt_PVGV) {
	    sstr = SvRV(sstr);
	    if (sstr == dstr) {
		if (curcop->cop_stash != GvSTASH(dstr))
		    GvIMPORTED_on(dstr);
		GvMULTI_on(dstr);
		return;
	    }
	    goto glob_assign;
	}
	break;
    case SVt_PV:
	if (dtype < SVt_PV)
	    sv_upgrade(dstr, SVt_PV);
	break;
    case SVt_PVIV:
	if (dtype < SVt_PVIV)
	    sv_upgrade(dstr, SVt_PVIV);
	break;
    case SVt_PVNV:
	if (dtype < SVt_PVNV)
	    sv_upgrade(dstr, SVt_PVNV);
	break;

    case SVt_PVLV:
	sv_upgrade(dstr, SVt_PVNV);
	break;

    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVIO:
	if (op)
	    croak("Bizarre copy of %s in %s", sv_reftype(sstr, 0),
		op_name[op->op_type]);
	else
	    croak("Bizarre copy of %s", sv_reftype(sstr, 0));
	break;

    case SVt_PVGV:
	if (dtype <= SVt_PVGV) {
  glob_assign:
	    if (dtype != SVt_PVGV) {
		char *name = GvNAME(sstr);
		STRLEN len = GvNAMELEN(sstr);
		sv_upgrade(dstr, SVt_PVGV);
		sv_magic(dstr, dstr, '*', name, len);
		GvSTASH(dstr) = GvSTASH(sstr);
		GvNAME(dstr) = savepvn(name, len);
		GvNAMELEN(dstr) = len;
		SvFAKE_on(dstr);	/* can coerce to non-glob */
	    }
	    (void)SvOK_off(dstr);
	    GvINTRO_off(dstr);		/* one-shot flag */
	    gp_free(dstr);
	    GvGP(dstr) = gp_ref(GvGP(sstr));
	    SvTAINT(dstr);
	    if (curcop->cop_stash != GvSTASH(dstr))
		GvIMPORTED_on(dstr);
	    GvMULTI_on(dstr);
	    return;
	}
	/* FALL THROUGH */

    default:
	if (dtype < stype)
	    sv_upgrade(dstr, stype);
	if (SvGMAGICAL(sstr))
	    mg_get(sstr);
    }

    sflags = SvFLAGS(sstr);

    if (sflags & SVf_ROK) {
	if (dtype >= SVt_PV) {
	    if (dtype == SVt_PVGV) {
		SV *sref = SvREFCNT_inc(SvRV(sstr));
		SV *dref = 0;
		int intro = GvINTRO(dstr);

		if (intro) {
		    GP *gp;
		    GvGP(dstr)->gp_refcnt--;
		    GvINTRO_off(dstr);	/* one-shot flag */
		    Newz(602,gp, 1, GP);
		    GvGP(dstr) = gp;
		    GvREFCNT(dstr) = 1;
		    GvSV(dstr) = NEWSV(72,0);
		    GvLINE(dstr) = curcop->cop_line;
		    GvEGV(dstr) = dstr;
		}
		GvMULTI_on(dstr);
		switch (SvTYPE(sref)) {
		case SVt_PVAV:
		    if (intro)
			SAVESPTR(GvAV(dstr));
		    else
			dref = (SV*)GvAV(dstr);
		    GvAV(dstr) = (AV*)sref;
		    if (curcop->cop_stash != GvSTASH(dstr))
			GvIMPORTED_AV_on(dstr);
		    break;
		case SVt_PVHV:
		    if (intro)
			SAVESPTR(GvHV(dstr));
		    else
			dref = (SV*)GvHV(dstr);
		    GvHV(dstr) = (HV*)sref;
		    if (curcop->cop_stash != GvSTASH(dstr))
			GvIMPORTED_HV_on(dstr);
		    break;
		case SVt_PVCV:
		    if (intro)
			SAVESPTR(GvCV(dstr));
		    else {
			CV* cv = GvCV(dstr);
			if (cv) {
			    dref = (SV*)cv;
			    if (dowarn && sref != dref &&
				    !GvCVGEN((GV*)dstr) &&
				    (CvROOT(cv) || CvXSUB(cv)) )
				warn("Subroutine %s redefined",
				    GvENAME((GV*)dstr));
			    SvFAKE_on(cv);
			}
		    }
		    if (GvCV(dstr) != (CV*)sref) {
			GvCV(dstr) = (CV*)sref;
			GvASSUMECV_on(dstr);
		    }
		    if (curcop->cop_stash != GvSTASH(dstr))
			GvIMPORTED_CV_on(dstr);
		    break;
		case SVt_PVIO:
		    if (intro)
			SAVESPTR(GvIOp(dstr));
		    else
			dref = (SV*)GvIOp(dstr);
		    GvIOp(dstr) = (IO*)sref;
		    break;
		default:
		    if (intro)
			SAVESPTR(GvSV(dstr));
		    else
			dref = (SV*)GvSV(dstr);
		    GvSV(dstr) = sref;
		    if (curcop->cop_stash != GvSTASH(dstr))
			GvIMPORTED_SV_on(dstr);
		    break;
		}
		if (dref)
		    SvREFCNT_dec(dref);
		if (intro)
		    SAVEFREESV(sref);
		SvTAINT(dstr);
		return;
	    }
	    if (SvPVX(dstr)) {
		Safefree(SvPVX(dstr));
		SvLEN(dstr)=SvCUR(dstr)=0;
	    }
	}
	(void)SvOK_off(dstr);
	SvRV(dstr) = SvREFCNT_inc(SvRV(sstr));
	SvROK_on(dstr);
	if (sflags & SVp_NOK) {
	    SvNOK_on(dstr);
	    SvNVX(dstr) = SvNVX(sstr);
	}
	if (sflags & SVp_IOK) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	}
#ifdef OVERLOAD
	if (SvAMAGIC(sstr)) {
	    SvAMAGIC_on(dstr);
	}
#endif /* OVERLOAD */
    }
    else if (sflags & SVp_POK) {

	/*
	 * Check to see if we can just swipe the string.  If so, it's a
	 * possible small lose on short strings, but a big win on long ones.
	 * It might even be a win on short strings if SvPVX(dstr)
	 * has to be allocated and SvPVX(sstr) has to be freed.
	 */

	if (SvTEMP(sstr) &&		/* slated for free anyway? */
	    !(sflags & SVf_OOK)) 	/* and not involved in OOK hack? */
	{
	    if (SvPVX(dstr)) {		/* we know that dtype >= SVt_PV */
		if (SvOOK(dstr)) {
		    SvFLAGS(dstr) &= ~SVf_OOK;
		    Safefree(SvPVX(dstr) - SvIVX(dstr));
		}
		else
		    Safefree(SvPVX(dstr));
	    }
	    (void)SvPOK_only(dstr);
	    SvPV_set(dstr, SvPVX(sstr));
	    SvLEN_set(dstr, SvLEN(sstr));
	    SvCUR_set(dstr, SvCUR(sstr));
	    SvTEMP_off(dstr);
	    (void)SvOK_off(sstr);
	    SvPV_set(sstr, Nullch);
	    SvLEN_set(sstr, 0);
	    SvCUR_set(sstr, 0);
	    SvTEMP_off(sstr);
	}
	else {					/* have to copy actual string */
	    STRLEN len = SvCUR(sstr);

	    SvGROW(dstr, len + 1);		/* inlined from sv_setpvn */
	    Move(SvPVX(sstr),SvPVX(dstr),len,char);
	    SvCUR_set(dstr, len);
	    *SvEND(dstr) = '\0';
	    (void)SvPOK_only(dstr);
	}
	/*SUPPRESS 560*/
	if (sflags & SVp_NOK) {
	    SvNOK_on(dstr);
	    SvNVX(dstr) = SvNVX(sstr);
	}
	if (sflags & SVp_IOK) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	}
    }
    else if (sflags & SVp_NOK) {
	SvNVX(dstr) = SvNVX(sstr);
	(void)SvNOK_only(dstr);
	if (SvIOK(sstr)) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	}
    }
    else if (sflags & SVp_IOK) {
	(void)SvIOK_only(dstr);
	SvIVX(dstr) = SvIVX(sstr);
    }
    else {
	(void)SvOK_off(dstr);
    }
    SvTAINT(dstr);
}

void
sv_setpvn(sv,ptr,len)
register SV *sv;
register char *ptr;
register STRLEN len;
{
    assert(len >= 0);
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    if (SvTYPE(sv) >= SVt_PV) {
	if (SvFAKE(sv) && SvTYPE(sv) == SVt_PVGV)
	    sv_unglob(sv);
    }
    else if (!sv_upgrade(sv, SVt_PV))
	return;
    SvGROW(sv, len + 1);
    Move(ptr,SvPVX(sv),len,char);
    SvCUR_set(sv, len);
    *SvEND(sv) = '\0';
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

void
sv_setpv(sv,ptr)
register SV *sv;
register char *ptr;
{
    register STRLEN len;

    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    len = strlen(ptr);
    if (SvTYPE(sv) >= SVt_PV) {
	if (SvFAKE(sv) && SvTYPE(sv) == SVt_PVGV)
	    sv_unglob(sv);
    }
    else if (!sv_upgrade(sv, SVt_PV))
	return;
    SvGROW(sv, len + 1);
    Move(ptr,SvPVX(sv),len+1,char);
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

void
sv_usepvn(sv,ptr,len)
register SV *sv;
register char *ptr;
register STRLEN len;
{
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (!SvUPGRADE(sv, SVt_PV))
	return;
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    if (SvPVX(sv))
	Safefree(SvPVX(sv));
    Renew(ptr, len+1, char);
    SvPVX(sv) = ptr;
    SvCUR_set(sv, len);
    SvLEN_set(sv, len+1);
    *SvEND(sv) = '\0';
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

void
sv_chop(sv,ptr)	/* like set but assuming ptr is in sv */
register SV *sv;
register char *ptr;
{
    register STRLEN delta;

    if (!ptr || !SvPOKp(sv))
	return;
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (SvTYPE(sv) < SVt_PVIV)
	sv_upgrade(sv,SVt_PVIV);

    if (!SvOOK(sv)) {
	SvIVX(sv) = 0;
	SvFLAGS(sv) |= SVf_OOK;
    }
    SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK|SVp_IOK|SVp_NOK);
    delta = ptr - SvPVX(sv);
    SvLEN(sv) -= delta;
    SvCUR(sv) -= delta;
    SvPVX(sv) += delta;
    SvIVX(sv) += delta;
}

void
sv_catpvn(sv,ptr,len)
register SV *sv;
register char *ptr;
register STRLEN len;
{
    STRLEN tlen;
    char *junk;

    junk = SvPV_force(sv, tlen);
    SvGROW(sv, tlen + len + 1);
    if (ptr == junk)
	ptr = SvPVX(sv);
    Move(ptr,SvPVX(sv)+tlen,len,char);
    SvCUR(sv) += len;
    *SvEND(sv) = '\0';
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

void
sv_catsv(dstr,sstr)
SV *dstr;
register SV *sstr;
{
    char *s;
    STRLEN len;
    if (!sstr)
	return;
    if (s = SvPV(sstr, len))
	sv_catpvn(dstr,s,len);
}

void
sv_catpv(sv,ptr)
register SV *sv;
register char *ptr;
{
    register STRLEN len;
    STRLEN tlen;
    char *junk;

    if (!ptr)
	return;
    junk = SvPV_force(sv, tlen);
    len = strlen(ptr);
    SvGROW(sv, tlen + len + 1);
    if (ptr == junk)
	ptr = SvPVX(sv);
    Move(ptr,SvPVX(sv)+tlen,len+1,char);
    SvCUR(sv) += len;
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

SV *
#ifdef LEAKTEST
newSV(x,len)
I32 x;
#else
newSV(len)
#endif
STRLEN len;
{
    register SV *sv;
    
    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    if (len) {
	sv_upgrade(sv, SVt_PV);
	SvGROW(sv, len + 1);
    }
    return sv;
}

void
sv_magic(sv, obj, how, name, namlen)
register SV *sv;
SV *obj;
int how;
char *name;
I32 namlen;
{
    MAGIC* mg;
    
    if (SvREADONLY(sv) && curcop != &compiling && !strchr("gB", how))
	croak(no_modify);
    if (SvMAGICAL(sv) || (how == 't' && SvTYPE(sv) >= SVt_PVMG)) {
	if (SvMAGIC(sv) && (mg = mg_find(sv, how))) {
	    if (how == 't')
		mg->mg_len |= 1;
	    return;
	}
    }
    else {
	if (!SvUPGRADE(sv, SVt_PVMG))
	    return;
    }
    Newz(702,mg, 1, MAGIC);
    mg->mg_moremagic = SvMAGIC(sv);

    SvMAGIC(sv) = mg;
    if (!obj || obj == sv || how == '#')
	mg->mg_obj = obj;
    else {
	mg->mg_obj = SvREFCNT_inc(obj);
	mg->mg_flags |= MGf_REFCOUNTED;
    }
    mg->mg_type = how;
    mg->mg_len = namlen;
    if (name && namlen >= 0)
	mg->mg_ptr = savepvn(name, namlen);
    switch (how) {
    case 0:
	mg->mg_virtual = &vtbl_sv;
	break;
#ifdef OVERLOAD
    case 'A':
        mg->mg_virtual = &vtbl_amagic;
        break;
    case 'a':
        mg->mg_virtual = &vtbl_amagicelem;
        break;
    case 'c':
        mg->mg_virtual = 0;
        break;
#endif /* OVERLOAD */
    case 'B':
	mg->mg_virtual = &vtbl_bm;
	break;
    case 'E':
	mg->mg_virtual = &vtbl_env;
	break;
    case 'e':
	mg->mg_virtual = &vtbl_envelem;
	break;
    case 'g':
	mg->mg_virtual = &vtbl_mglob;
	break;
    case 'I':
	mg->mg_virtual = &vtbl_isa;
	break;
    case 'i':
	mg->mg_virtual = &vtbl_isaelem;
	break;
    case 'L':
	SvRMAGICAL_on(sv);
	mg->mg_virtual = 0;
	break;
    case 'l':
	mg->mg_virtual = &vtbl_dbline;
	break;
    case 'P':
	mg->mg_virtual = &vtbl_pack;
	break;
    case 'p':
    case 'q':
	mg->mg_virtual = &vtbl_packelem;
	break;
    case 'S':
	mg->mg_virtual = &vtbl_sig;
	break;
    case 's':
	mg->mg_virtual = &vtbl_sigelem;
	break;
    case 't':
	mg->mg_virtual = &vtbl_taint;
	mg->mg_len = 1;
	break;
    case 'U':
	mg->mg_virtual = &vtbl_uvar;
	break;
    case 'v':
	mg->mg_virtual = &vtbl_vec;
	break;
    case 'x':
	mg->mg_virtual = &vtbl_substr;
	break;
    case '*':
	mg->mg_virtual = &vtbl_glob;
	break;
    case '#':
	mg->mg_virtual = &vtbl_arylen;
	break;
    case '.':
	mg->mg_virtual = &vtbl_pos;
	break;
    case '~':	/* Reserved for use by extensions not perl internals.	*/
	/* Useful for attaching extension internal data to perl vars.	*/
	/* Note that multiple extensions may clash if magical scalars	*/
	/* etc holding private data from one are passed to another.	*/
	SvRMAGICAL_on(sv);
	break;
    default:
	croak("Don't know how to handle magic of type '%c'", how);
    }
    mg_magical(sv);
    if (SvGMAGICAL(sv))
	SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK|SVf_POK);
}

int
sv_unmagic(sv, type)
SV* sv;
int type;
{
    MAGIC* mg;
    MAGIC** mgp;
    if (SvTYPE(sv) < SVt_PVMG || !SvMAGIC(sv))
	return 0;
    mgp = &SvMAGIC(sv);
    for (mg = *mgp; mg; mg = *mgp) {
	if (mg->mg_type == type) {
	    MGVTBL* vtbl = mg->mg_virtual;
	    *mgp = mg->mg_moremagic;
	    if (vtbl && vtbl->svt_free)
		(*vtbl->svt_free)(sv, mg);
	    if (mg->mg_ptr && mg->mg_type != 'g')
		Safefree(mg->mg_ptr);
	    if (mg->mg_flags & MGf_REFCOUNTED)
		SvREFCNT_dec(mg->mg_obj);
	    Safefree(mg);
	}
	else
	    mgp = &mg->mg_moremagic;
    }
    if (!SvMAGIC(sv)) {
	SvMAGICAL_off(sv);
	SvFLAGS(sv) |= (SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
    }

    return 0;
}

void
sv_insert(bigstr,offset,len,little,littlelen)
SV *bigstr;
STRLEN offset;
STRLEN len;
char *little;
STRLEN littlelen;
{
    register char *big;
    register char *mid;
    register char *midend;
    register char *bigend;
    register I32 i;

    if (!bigstr)
	croak("Can't modify non-existent substring");
    SvPV_force(bigstr, na);

    i = littlelen - len;
    if (i > 0) {			/* string might grow */
	big = SvGROW(bigstr, SvCUR(bigstr) + i + 1);
	mid = big + offset + len;
	midend = bigend = big + SvCUR(bigstr);
	bigend += i;
	*bigend = '\0';
	while (midend > mid)		/* shove everything down */
	    *--bigend = *--midend;
	Move(little,big+offset,littlelen,char);
	SvCUR(bigstr) += i;
	SvSETMAGIC(bigstr);
	return;
    }
    else if (i == 0) {
	Move(little,SvPVX(bigstr)+offset,len,char);
	SvSETMAGIC(bigstr);
	return;
    }

    big = SvPVX(bigstr);
    mid = big + offset;
    midend = mid + len;
    bigend = big + SvCUR(bigstr);

    if (midend > bigend)
	croak("panic: sv_insert");

    if (mid - big > bigend - midend) {	/* faster to shorten from end */
	if (littlelen) {
	    Move(little, mid, littlelen,char);
	    mid += littlelen;
	}
	i = bigend - midend;
	if (i > 0) {
	    Move(midend, mid, i,char);
	    mid += i;
	}
	*mid = '\0';
	SvCUR_set(bigstr, mid - big);
    }
    /*SUPPRESS 560*/
    else if (i = mid - big) {	/* faster from front */
	midend -= littlelen;
	mid = midend;
	sv_chop(bigstr,midend-i);
	big += i;
	while (i--)
	    *--midend = *--big;
	if (littlelen)
	    Move(little, mid, littlelen,char);
    }
    else if (littlelen) {
	midend -= littlelen;
	sv_chop(bigstr,midend);
	Move(little,midend,littlelen,char);
    }
    else {
	sv_chop(bigstr,midend);
    }
    SvSETMAGIC(bigstr);
}

/* make sv point to what nstr did */

void
sv_replace(sv,nsv)
register SV *sv;
register SV *nsv;
{
    U32 refcnt = SvREFCNT(sv);
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (SvREFCNT(nsv) != 1)
	warn("Reference miscount in sv_replace()");
    if (SvMAGICAL(sv)) {
	if (SvMAGICAL(nsv))
	    mg_free(nsv);
	else
	    sv_upgrade(nsv, SVt_PVMG);
	SvMAGIC(nsv) = SvMAGIC(sv);
	SvFLAGS(nsv) |= SvMAGICAL(sv);
	SvMAGICAL_off(sv);
	SvMAGIC(sv) = 0;
    }
    SvREFCNT(sv) = 0;
    sv_clear(sv);
    StructCopy(nsv,sv,SV);
    SvREFCNT(sv) = refcnt;
    del_SV(nsv);
}

void
sv_clear(sv)
register SV *sv;
{
    assert(sv);
    assert(SvREFCNT(sv) == 0);

    if (SvOBJECT(sv)) {
	dSP;
	GV* destructor;

	if (defstash) {		/* Still have a symbol table? */
	    destructor = gv_fetchmethod(SvSTASH(sv), "DESTROY");

	    ENTER;
	    SAVEFREESV(SvSTASH(sv));
	    if (destructor && GvCV(destructor)) {
		SV ref;

		Zero(&ref, 1, SV);
		sv_upgrade(&ref, SVt_RV);
		SAVEI32(SvREFCNT(sv));
		SvRV(&ref) = SvREFCNT_inc(sv);
		SvROK_on(&ref);

		EXTEND(SP, 2);
		PUSHMARK(SP);
		PUSHs(&ref);
		PUTBACK;
		perl_call_sv((SV*)destructor, G_DISCARD|G_EVAL|G_KEEPERR);
		del_XRV(SvANY(&ref));
	    }
	    LEAVE;
	}
	else
	    SvREFCNT_dec(SvSTASH(sv));
	if (SvOBJECT(sv)) {
	    SvOBJECT_off(sv);	/* Curse the object. */
	    if (SvTYPE(sv) != SVt_PVIO)
		--sv_objcount;	/* XXX Might want something more general */
	}
    }
    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv))
	mg_free(sv);
    switch (SvTYPE(sv)) {
    case SVt_PVIO:
	io_close((IO*)sv);
	Safefree(IoTOP_NAME(sv));
	Safefree(IoFMT_NAME(sv));
	Safefree(IoBOTTOM_NAME(sv));
	/* FALL THROUGH */
    case SVt_PVBM:
	goto freescalar;
    case SVt_PVCV:
    case SVt_PVFM:
	cv_undef((CV*)sv);
	goto freescalar;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVGV:
	gp_free(sv);
	Safefree(GvNAME(sv));
	/* FALL THROUGH */
    case SVt_PVLV:
    case SVt_PVMG:
    case SVt_PVNV:
    case SVt_PVIV:
      freescalar:
	(void)SvOOK_off(sv);
	/* FALL THROUGH */
    case SVt_PV:
    case SVt_RV:
	if (SvROK(sv))
	    SvREFCNT_dec(SvRV(sv));
	else if (SvPVX(sv))
	    Safefree(SvPVX(sv));
	break;
/*
    case SVt_NV:
    case SVt_IV:
    case SVt_NULL:
	break;
*/
    }

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_IV:
	del_XIV(SvANY(sv));
	break;
    case SVt_NV:
	del_XNV(SvANY(sv));
	break;
    case SVt_RV:
	del_XRV(SvANY(sv));
	break;
    case SVt_PV:
	del_XPV(SvANY(sv));
	break;
    case SVt_PVIV:
	del_XPVIV(SvANY(sv));
	break;
    case SVt_PVNV:
	del_XPVNV(SvANY(sv));
	break;
    case SVt_PVMG:
	del_XPVMG(SvANY(sv));
	break;
    case SVt_PVLV:
	del_XPVLV(SvANY(sv));
	break;
    case SVt_PVAV:
	del_XPVAV(SvANY(sv));
	break;
    case SVt_PVHV:
	del_XPVHV(SvANY(sv));
	break;
    case SVt_PVCV:
	del_XPVCV(SvANY(sv));
	break;
    case SVt_PVGV:
	del_XPVGV(SvANY(sv));
	break;
    case SVt_PVBM:
	del_XPVBM(SvANY(sv));
	break;
    case SVt_PVFM:
	del_XPVFM(SvANY(sv));
	break;
    case SVt_PVIO:
	del_XPVIO(SvANY(sv));
	break;
    }
    SvFLAGS(sv) &= SVf_BREAK;
    SvFLAGS(sv) |= SVTYPEMASK;
}

SV *
sv_newref(sv)
SV* sv;
{
    if (sv)
	SvREFCNT(sv)++;
    return sv;
}

void
sv_free(sv)
SV *sv;
{
    if (!sv)
	return;
    if (SvREADONLY(sv)) {
	if (sv == &sv_undef || sv == &sv_yes || sv == &sv_no)
	    return;
    }
    if (SvREFCNT(sv) == 0) {
	if (SvFLAGS(sv) & SVf_BREAK)
	    return;
	warn("Attempt to free unreferenced scalar");
	return;
    }
    if (--SvREFCNT(sv) > 0)
	return;
#ifdef DEBUGGING
    if (SvTEMP(sv)) {
	warn("Attempt to free temp prematurely");
	return;
    }
#endif
    sv_clear(sv);
    del_SV(sv);
}

STRLEN
sv_len(sv)
register SV *sv;
{
    char *junk;
    STRLEN len;

    if (!sv)
	return 0;

    if (SvGMAGICAL(sv))
	len = mg_len(sv);
    else
	junk = SvPV(sv, len);
    return len;
}

I32
sv_eq(str1,str2)
register SV *str1;
register SV *str2;
{
    char *pv1;
    STRLEN cur1;
    char *pv2;
    STRLEN cur2;

    if (!str1) {
	pv1 = "";
	cur1 = 0;
    }
    else
	pv1 = SvPV(str1, cur1);

    if (!str2)
	return !cur1;
    else
	pv2 = SvPV(str2, cur2);

    if (cur1 != cur2)
	return 0;

    return !bcmp(pv1, pv2, cur1);
}

I32
sv_cmp(str1,str2)
register SV *str1;
register SV *str2;
{
    I32 retval;
    char *pv1;
    STRLEN cur1;
    char *pv2;
    STRLEN cur2;

    if (!str1) {
	pv1 = "";
	cur1 = 0;
    }
    else
	pv1 = SvPV(str1, cur1);

    if (!str2) {
	pv2 = "";
	cur2 = 0;
    }
    else
	pv2 = SvPV(str2, cur2);

    if (!cur1)
	return cur2 ? -1 : 0;
    if (!cur2)
	return 1;

    if (cur1 < cur2) {
	/*SUPPRESS 560*/
	if (retval = memcmp((void*)pv1, (void*)pv2, cur1))
	    return retval < 0 ? -1 : 1;
	else
	    return -1;
    }
    /*SUPPRESS 560*/
    else if (retval = memcmp((void*)pv1, (void*)pv2, cur2))
	return retval < 0 ? -1 : 1;
    else if (cur1 == cur2)
	return 0;
    else
	return 1;
}

char *
sv_gets(sv,fp,append)
register SV *sv;
register FILE *fp;
I32 append;
{
    char *rsptr;
    STRLEN rslen;
    register STDCHAR rslast;
    register STDCHAR *bp;
    register I32 cnt;
    I32 i;

#ifdef FAST_SV_GETS
    /*
     * We're going to steal some values from the stdio struct
     * and put EVERYTHING in the innermost loop into registers.
     */
    register STDCHAR *ptr;
    STRLEN bpx;
    I32 shortbuffered;
#endif

    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv))
	    sv_unref(sv);
    }
    if (!SvUPGRADE(sv, SVt_PV))
	return 0;

    if (RsSNARF(rs)) {
	rsptr = NULL;
	rslen = 0;
    }
    else if (RsPARA(rs)) {
	rsptr = "\n\n";
	rslen = 2;
    }
    else
	rsptr = SvPV(rs, rslen);
    rslast = rslen ? rsptr[rslen - 1] : '\0';

    if (RsPARA(rs)) {		/* have to do this both before and after */
	do {			/* to make sure file boundaries work right */
	    if (feof(fp))
		return 0;
	    i = getc(fp);
	    if (i != '\n') {
		if (i == -1)
		    return 0;
		ungetc(i,fp);
		break;
	    }
	} while (i != EOF);
    }

#ifdef FAST_SV_GETS

    /* Here is some breathtakingly efficient cheating */

    cnt = FILE_cnt(fp);			/* get count into register */
    (void)SvPOK_only(sv);		/* validate pointer */
    if (SvLEN(sv) - append <= cnt + 1) { /* make sure we have the room */
	if (cnt > 80 && SvLEN(sv) > append) {
	    shortbuffered = cnt - SvLEN(sv) + append + 1;
	    cnt -= shortbuffered;
	}
	else {
	    shortbuffered = 0;
	    SvGROW(sv, append+cnt+2);/* (remembering cnt can be -1) */
	}
    }
    else
	shortbuffered = 0;
    bp = (STDCHAR*)SvPVX(sv) + append;  /* move these two too to registers */
    ptr = FILE_ptr(fp);
    for (;;) {
      screamer:
	if (cnt > 0) {
	    if (rslen) {
		while (--cnt >= 0) {		     /* this     |  eat */
		    if ((*bp++ = *ptr++) == rslast)  /* really   |  dust */
			goto thats_all_folks;	     /* screams  |  sed :-) */
		}
	    }
	    else {
	        memcpy((char*)bp, (char*)ptr, cnt);  /* this     |  eat */    
		bp += cnt;			     /* screams  |  dust */   
		ptr += cnt;			     /* louder   |  sed :-) */
		cnt = 0;
	    }
	}
	
	if (shortbuffered) {		/* oh well, must extend */
	    cnt = shortbuffered;
	    shortbuffered = 0;
	    bpx = bp - (STDCHAR*)SvPVX(sv); /* box up before relocation */
	    SvCUR_set(sv, bpx);
	    SvGROW(sv, SvLEN(sv) + append + cnt + 2);
	    bp = (STDCHAR*)SvPVX(sv) + bpx; /* unbox after relocation */
	    continue;
	}

	FILE_cnt(fp) = cnt;		/* deregisterize cnt and ptr */
	FILE_ptr(fp) = ptr;
	i = _filbuf(fp);		/* get more characters */
	cnt = FILE_cnt(fp);
	ptr = FILE_ptr(fp);		/* reregisterize cnt and ptr */

	if (i == EOF)			/* all done for ever? */
	    goto thats_really_all_folks;

	bpx = bp - (STDCHAR*)SvPVX(sv);	/* box up before relocation */
	SvCUR_set(sv, bpx);
	SvGROW(sv, bpx + cnt + 2);
	bp = (STDCHAR*)SvPVX(sv) + bpx;	/* unbox after relocation */

	*bp++ = i;			/* store character from _filbuf */

	if (rslen && (STDCHAR)i == rslast)  /* all done for now? */
	    goto thats_all_folks;
    }

thats_all_folks:
    if ((rslen > 1 && (bp - (STDCHAR*)SvPVX(sv) < rslen)) ||
	  bcmp((char*)bp - rslen, rsptr, rslen))
	goto screamer;			/* go back to the fray */
thats_really_all_folks:
    if (shortbuffered)
	cnt += shortbuffered;
    FILE_cnt(fp) = cnt;			/* put these back or we're in trouble */
    FILE_ptr(fp) = ptr;
    *bp = '\0';
    SvCUR_set(sv, bp - (STDCHAR*)SvPVX(sv));  /* set length */

#else /* SV_FAST_GETS */

    /*The big, slow, and stupid way */

    {
	STDCHAR buf[8192];

screamer:
	if (rslen) {
	    register STDCHAR *bpe = buf + sizeof(buf);
	    bp = buf;
	    while ((i = getc(fp)) != EOF && (*bp++ = i) != rslast && bp < bpe)
		; /* keep reading */
	    cnt = bp - buf;
	}
	else {
	    cnt = fread((char*)buf, 1, sizeof(buf), fp);
	    i = cnt ? (U8)buf[cnt - 1] : EOF;
	}

	if (append)
	    sv_catpvn(sv, buf, cnt);
	else
	    sv_setpvn(sv, buf, cnt);

	if (i != EOF &&			/* joy */
	    (!rslen ||
	     SvCUR(sv) < rslen ||
	     bcmp(SvPVX(sv) + SvCUR(sv) - rslen, rsptr, rslen)))
	{
	    append = -1;
	    goto screamer;
	}
    }

#endif /* SV_FAST_GETS */

    if (RsPARA(rs)) {		/* have to do this both before and after */  
        while (i != EOF) {	/* to make sure file boundaries work right */
	    i = getc(fp);
	    if (i != '\n') {
		ungetc(i,fp);
		break;
	    }
	}
    }

    return (SvCUR(sv) - append) ? SvPVX(sv) : Nullch;
}

void
sv_inc(sv)
register SV *sv;
{
    register char *d;
    int flags;

    if (!sv)
	return;
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv)) {
#ifdef OVERLOAD
	  if (SvAMAGIC(sv) && AMG_CALLun(sv,inc)) return;
#endif /* OVERLOAD */
	  sv_unref(sv);
	}
    }
    if (SvGMAGICAL(sv))
	mg_get(sv);
    flags = SvFLAGS(sv);
    if (flags & SVp_IOK) {
	(void)SvIOK_only(sv);
	++SvIVX(sv);
	return;
    }
    if (flags & SVp_NOK) {
	SvNVX(sv) += 1.0;
	(void)SvNOK_only(sv);
	return;
    }
    if (!(flags & SVp_POK) || !*SvPVX(sv)) {
	if ((flags & SVTYPEMASK) < SVt_PVNV)
	    sv_upgrade(sv, SVt_NV);
	SvNVX(sv) = 1.0;
	(void)SvNOK_only(sv);
	return;
    }
    d = SvPVX(sv);
    while (isALPHA(*d)) d++;
    while (isDIGIT(*d)) d++;
    if (*d) {
        sv_setnv(sv,atof(SvPVX(sv)) + 1.0);  /* punt */
	return;
    }
    d--;
    while (d >= SvPVX(sv)) {
	if (isDIGIT(*d)) {
	    if (++*d <= '9')
		return;
	    *(d--) = '0';
	}
	else {
	    ++*d;
	    if (isALPHA(*d))
		return;
	    *(d--) -= 'z' - 'a' + 1;
	}
    }
    /* oh,oh, the number grew */
    SvGROW(sv, SvCUR(sv) + 2);
    SvCUR(sv)++;
    for (d = SvPVX(sv) + SvCUR(sv); d > SvPVX(sv); d--)
	*d = d[-1];
    if (isDIGIT(d[1]))
	*d = '1';
    else
	*d = d[1];
}

void
sv_dec(sv)
register SV *sv;
{
    int flags;

    if (!sv)
	return;
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv) && curcop != &compiling)
	    croak(no_modify);
	if (SvROK(sv)) {
#ifdef OVERLOAD
	  if (SvAMAGIC(sv) && AMG_CALLun(sv,dec)) return;
#endif /* OVERLOAD */
	  sv_unref(sv);
	}
    }
    if (SvGMAGICAL(sv))
	mg_get(sv);
    flags = SvFLAGS(sv);
    if (flags & SVp_IOK) {
	(void)SvIOK_only(sv);
	--SvIVX(sv);
	return;
    }
    if (flags & SVp_NOK) {
	SvNVX(sv) -= 1.0;
	(void)SvNOK_only(sv);
	return;
    }
    if (!(flags & SVp_POK)) {
	if ((flags & SVTYPEMASK) < SVt_PVNV)
	    sv_upgrade(sv, SVt_NV);
	SvNVX(sv) = -1.0;
	(void)SvNOK_only(sv);
	return;
    }
    sv_setnv(sv,atof(SvPVX(sv)) - 1.0);
}

/* Make a string that will exist for the duration of the expression
 * evaluation.  Actually, it may have to last longer than that, but
 * hopefully we won't free it until it has been assigned to a
 * permanent location. */

static void
sv_mortalgrow()
{
    tmps_max += 128;
    Renew(tmps_stack, tmps_max, SV*);
}

SV *
sv_mortalcopy(oldstr)
SV *oldstr;
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    sv_setsv(sv,oldstr);
    if (++tmps_ix >= tmps_max)
	sv_mortalgrow();
    tmps_stack[tmps_ix] = sv;
    SvTEMP_on(sv);
    return sv;
}

SV *
sv_newmortal()
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = SVs_TEMP;
    if (++tmps_ix >= tmps_max)
	sv_mortalgrow();
    tmps_stack[tmps_ix] = sv;
    return sv;
}

/* same thing without the copying */

SV *
sv_2mortal(sv)
register SV *sv;
{
    if (!sv)
	return sv;
    if (SvREADONLY(sv) && curcop != &compiling)
	croak(no_modify);
    if (++tmps_ix >= tmps_max)
	sv_mortalgrow();
    tmps_stack[tmps_ix] = sv;
    SvTEMP_on(sv);
    return sv;
}

SV *
newSVpv(s,len)
char *s;
STRLEN len;
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    if (!len)
	len = strlen(s);
    sv_setpvn(sv,s,len);
    return sv;
}

SV *
newSVnv(n)
double n;
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    sv_setnv(sv,n);
    return sv;
}

SV *
newSViv(i)
IV i;
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    sv_setiv(sv,i);
    return sv;
}

SV *
newRV(ref)
SV *ref;
{
    register SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    sv_upgrade(sv, SVt_RV);
    SvTEMP_off(ref);
    SvRV(sv) = SvREFCNT_inc(ref);
    SvROK_on(sv);
    return sv;
}

/* make an exact duplicate of old */

SV *
newSVsv(old)
register SV *old;
{
    register SV *sv;

    if (!old)
	return Nullsv;
    if (SvTYPE(old) == SVTYPEMASK) {
	warn("semi-panic: attempt to dup freed string");
	return Nullsv;
    }
    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 1;
    SvFLAGS(sv) = 0;
    if (SvTEMP(old)) {
	SvTEMP_off(old);
	sv_setsv(sv,old);
	SvTEMP_on(old);
    }
    else
	sv_setsv(sv,old);
    return sv;
}

void
sv_reset(s,stash)
register char *s;
HV *stash;
{
    register HE *entry;
    register GV *gv;
    register SV *sv;
    register I32 i;
    register PMOP *pm;
    register I32 max;
    char todo[256];

    if (!*s) {		/* reset ?? searches */
	for (pm = HvPMROOT(stash); pm; pm = pm->op_pmnext) {
	    pm->op_pmflags &= ~PMf_USED;
	}
	return;
    }

    /* reset variables */

    if (!HvARRAY(stash))
	return;

    Zero(todo, 256, char);
    while (*s) {
	i = *s;
	if (s[1] == '-') {
	    s += 2;
	}
	max = *s++;
	for ( ; i <= max; i++) {
	    todo[i] = 1;
	}
	for (i = 0; i <= (I32) HvMAX(stash); i++) {
	    for (entry = HvARRAY(stash)[i];
	      entry;
	      entry = entry->hent_next) {
		if (!todo[(U8)*entry->hent_key])
		    continue;
		gv = (GV*)entry->hent_val;
		sv = GvSV(gv);
		(void)SvOK_off(sv);
		if (SvTYPE(sv) >= SVt_PV) {
		    SvCUR_set(sv, 0);
		    SvTAINT(sv);
		    if (SvPVX(sv) != Nullch)
			*SvPVX(sv) = '\0';
		}
		if (GvAV(gv)) {
		    av_clear(GvAV(gv));
		}
		if (GvHV(gv)) {
		    if (HvNAME(GvHV(gv)))
			continue;
		    hv_clear(GvHV(gv));
#ifndef VMS  /* VMS has no environ array */
		    if (gv == envgv)
			environ[0] = Nullch;
#endif
		}
	    }
	}
    }
}

CV *
sv_2cv(sv, st, gvp, lref)
SV *sv;
HV **st;
GV **gvp;
I32 lref;
{
    GV *gv;
    CV *cv;

    if (!sv)
	return *gvp = Nullgv, Nullcv;
    switch (SvTYPE(sv)) {
    case SVt_PVCV:
	*st = CvSTASH(sv);
	*gvp = Nullgv;
	return (CV*)sv;
    case SVt_PVHV:
    case SVt_PVAV:
	*gvp = Nullgv;
	return Nullcv;
    case SVt_PVGV:
	gv = (GV*)sv;
	*gvp = gv;
	*st = GvESTASH(gv);
	goto fix_gv;

    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvROK(sv)) {
	    cv = (CV*)SvRV(sv);
	    if (SvTYPE(cv) != SVt_PVCV)
		croak("Not a subroutine reference");
	    *gvp = Nullgv;
	    *st = CvSTASH(cv);
	    return cv;
	}
	if (isGV(sv))
	    gv = (GV*)sv;
	else
	    gv = gv_fetchpv(SvPV(sv, na), lref, SVt_PVCV);
	*gvp = gv;
	if (!gv)
	    return Nullcv;
	*st = GvESTASH(gv);
    fix_gv:
	if (lref && !GvCV(gv)) {
	    SV *tmpsv;
	    ENTER;
	    tmpsv = NEWSV(704,0);
	    gv_efullname(tmpsv, gv);
	    newSUB(start_subparse(),
		   newSVOP(OP_CONST, 0, tmpsv),
		   Nullop,
		   Nullop);
	    LEAVE;
	    if (!GvCV(gv))
		croak("Unable to create sub named \"%s\"", SvPV(sv,na));
	}
	return GvCV(gv);
    }
}

#ifndef SvTRUE
I32
SvTRUE(sv)
register SV *sv;
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (SvPOK(sv)) {
	register XPV* Xpv;
	if ((Xpv = (XPV*)SvANY(sv)) &&
		(*Xpv->xpv_pv > '0' ||
		Xpv->xpv_cur > 1 ||
		(Xpv->xpv_cur && *Xpv->xpv_pv != '0')))
	    return 1;
	else
	    return 0;
    }
    else {
	if (SvIOK(sv))
	    return SvIVX(sv) != 0;
	else {
	    if (SvNOK(sv))
		return SvNVX(sv) != 0.0;
	    else
		return sv_2bool(sv);
	}
    }
}
#endif /* SvTRUE */

#ifndef SvIV
IV SvIV(Sv)
register SV *Sv;
{
    if (SvIOK(Sv))
	return SvIVX(Sv);
    return sv_2iv(Sv);
}
#endif /* SvIV */


#ifndef SvNV
double SvNV(Sv)
register SV *Sv;
{
    if (SvNOK(Sv))
	return SvNVX(Sv);
    if (SvIOK(Sv))
	return (double)SvIVX(Sv);
    return sv_2nv(Sv);
}
#endif /* SvNV */

#ifdef CRIPPLED_CC
char *
sv_pvn(sv, lp)
SV *sv;
STRLEN *lp;
{
    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
	return SvPVX(sv);
    }
    return sv_2pv(sv, lp);
}
#endif

char *
sv_pvn_force(sv, lp)
SV *sv;
STRLEN *lp;
{
    char *s;

    if (SvREADONLY(sv) && curcop != &compiling)
	croak(no_modify);
    
    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
    }
    else {
	if (SvTYPE(sv) > SVt_PVLV && SvTYPE(sv) != SVt_PVFM) {
	    if (SvFAKE(sv) && SvTYPE(sv) == SVt_PVGV) {
		sv_unglob(sv);
		s = SvPVX(sv);
		*lp = SvCUR(sv);
	    }
	    else
		croak("Can't coerce %s to string in %s", sv_reftype(sv,0),
		    op_name[op->op_type]);
	}
	else
	    s = sv_2pv(sv, lp);
	if (s != SvPVX(sv)) {	/* Almost, but not quite, sv_setpvn() */
	    STRLEN len = *lp;
	    
	    if (SvROK(sv))
		sv_unref(sv);
	    (void)SvUPGRADE(sv, SVt_PV);		/* Never FALSE */
	    SvGROW(sv, len + 1);
	    Move(s,SvPVX(sv),len,char);
	    SvCUR_set(sv, len);
	    *SvEND(sv) = '\0';
	}
	if (!SvPOK(sv)) {
	    SvPOK_on(sv);		/* validate pointer */
	    SvTAINT(sv);
	    DEBUG_c(fprintf(stderr,"0x%lx 2pv(%s)\n",
		(unsigned long)sv,SvPVX(sv)));
	}
    }
    return SvPVX(sv);
}

char *
sv_reftype(sv, ob)
SV* sv;
int ob;
{
    if (ob && SvOBJECT(sv))
	return HvNAME(SvSTASH(sv));
    else {
	switch (SvTYPE(sv)) {
	case SVt_NULL:
	case SVt_IV:
	case SVt_NV:
	case SVt_RV:
	case SVt_PV:
	case SVt_PVIV:
	case SVt_PVNV:
	case SVt_PVMG:
	case SVt_PVBM:
				if (SvROK(sv))
				    return "REF";
				else
				    return "SCALAR";
	case SVt_PVLV:		return "LVALUE";
	case SVt_PVAV:		return "ARRAY";
	case SVt_PVHV:		return "HASH";
	case SVt_PVCV:		return "CODE";
	case SVt_PVGV:		return "GLOB";
	case SVt_PVFM:		return "FORMLINE";
	default:		return "UNKNOWN";
	}
    }
}

int
sv_isobject(sv)
SV *sv;
{
    if (!SvROK(sv))
	return 0;
    sv = (SV*)SvRV(sv);
    if (!SvOBJECT(sv))
	return 0;
    return 1;
}

int
sv_isa(sv, name)
SV *sv;
char *name;
{
    if (!SvROK(sv))
	return 0;
    sv = (SV*)SvRV(sv);
    if (!SvOBJECT(sv))
	return 0;

    return strEQ(HvNAME(SvSTASH(sv)), name);
}

SV*
newSVrv(rv, classname)
SV *rv;
char *classname;
{
    SV *sv;

    new_SV();
    SvANY(sv) = 0;
    SvREFCNT(sv) = 0;
    SvFLAGS(sv) = 0;
    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = SvREFCNT_inc(sv);
    SvROK_on(rv);

    if (classname) {
	HV* stash = gv_stashpv(classname, TRUE);
	(void)sv_bless(rv, stash);
    }
    return sv;
}

SV*
sv_setref_pv(rv, classname, pv)
SV *rv;
char *classname;
void* pv;
{
    if (!pv)
	sv_setsv(rv, &sv_undef);
    else
	sv_setiv(newSVrv(rv,classname), (IV)pv);
    return rv;
}

SV*
sv_setref_iv(rv, classname, iv)
SV *rv;
char *classname;
IV iv;
{
    sv_setiv(newSVrv(rv,classname), iv);
    return rv;
}

SV*
sv_setref_nv(rv, classname, nv)
SV *rv;
char *classname;
double nv;
{
    sv_setnv(newSVrv(rv,classname), nv);
    return rv;
}

SV*
sv_setref_pvn(rv, classname, pv, n)
SV *rv;
char *classname;
char* pv;
I32 n;
{
    sv_setpvn(newSVrv(rv,classname), pv, n);
    return rv;
}

SV*
sv_bless(sv,stash)
SV* sv;
HV* stash;
{
    SV *ref;
    if (!SvROK(sv))
        croak("Can't bless non-reference value");
    ref = SvRV(sv);
    if (SvFLAGS(ref) & (SVs_OBJECT|SVf_READONLY)) {
	if (SvREADONLY(ref))
	    croak(no_modify);
	if (SvOBJECT(ref) && SvTYPE(ref) != SVt_PVIO)
	    --sv_objcount;
    }
    SvOBJECT_on(ref);
    ++sv_objcount;
    (void)SvUPGRADE(ref, SVt_PVMG);
    SvSTASH(ref) = (HV*)SvREFCNT_inc(stash);

#ifdef OVERLOAD
    SvAMAGIC_off(sv);
    if (Gv_AMG(stash)) {
      SvAMAGIC_on(sv);
    }
#endif /* OVERLOAD */

    return sv;
}

static void
sv_unglob(sv)
SV* sv;
{
    assert(SvTYPE(sv) == SVt_PVGV);
    SvFAKE_off(sv);
    if (GvGP(sv))
	gp_free(sv);
    sv_unmagic(sv, '*');
    Safefree(GvNAME(sv));
    GvMULTI_off(sv);
    SvFLAGS(sv) &= ~SVTYPEMASK;
    SvFLAGS(sv) |= SVt_PVMG;
}

void
sv_unref(sv)
SV* sv;
{
    SV* rv = SvRV(sv);
    
    SvRV(sv) = 0;
    SvROK_off(sv);
    if (SvREFCNT(rv) != 1 || SvREADONLY(rv))
	SvREFCNT_dec(rv);
    else
	sv_2mortal(rv);		/* Schedule for freeing later */
}

#ifdef DEBUGGING
void
sv_dump(sv)
SV* sv;
{
    char tmpbuf[1024];
    char *d = tmpbuf;
    U32 flags;
    U32 type;

    if (!sv) {
	fprintf(stderr, "SV = 0\n");
	return;
    }
    
    flags = SvFLAGS(sv);
    type = SvTYPE(sv);

    sprintf(d, "(0x%lx)\n  REFCNT = %ld\n  FLAGS = (",
	(unsigned long)SvANY(sv), (long)SvREFCNT(sv));
    d += strlen(d);
    if (flags & SVs_PADBUSY)	strcat(d, "PADBUSY,");
    if (flags & SVs_PADTMP)	strcat(d, "PADTMP,");
    if (flags & SVs_PADMY)	strcat(d, "PADMY,");
    if (flags & SVs_TEMP)	strcat(d, "TEMP,");
    if (flags & SVs_OBJECT)	strcat(d, "OBJECT,");
    if (flags & SVs_GMG)	strcat(d, "GMG,");
    if (flags & SVs_SMG)	strcat(d, "SMG,");
    if (flags & SVs_RMG)	strcat(d, "RMG,");
    d += strlen(d);

    if (flags & SVf_IOK)	strcat(d, "IOK,");
    if (flags & SVf_NOK)	strcat(d, "NOK,");
    if (flags & SVf_POK)	strcat(d, "POK,");
    if (flags & SVf_ROK)	strcat(d, "ROK,");
    if (flags & SVf_OOK)	strcat(d, "OOK,");
    if (flags & SVf_FAKE)	strcat(d, "FAKE,");
    if (flags & SVf_READONLY)	strcat(d, "READONLY,");
    d += strlen(d);

    if (flags & SVp_IOK)	strcat(d, "pIOK,");
    if (flags & SVp_NOK)	strcat(d, "pNOK,");
    if (flags & SVp_POK)	strcat(d, "pPOK,");
    if (flags & SVp_SCREAM)	strcat(d, "SCREAM,");
    d += strlen(d);
    if (d[-1] == ',')
	d--;
    *d++ = ')';
    *d = '\0';

    fprintf(stderr, "SV = ");
    switch (type) {
    case SVt_NULL:
	fprintf(stderr,"NULL%s\n", tmpbuf);
	return;
    case SVt_IV:
	fprintf(stderr,"IV%s\n", tmpbuf);
	break;
    case SVt_NV:
	fprintf(stderr,"NV%s\n", tmpbuf);
	break;
    case SVt_RV:
	fprintf(stderr,"RV%s\n", tmpbuf);
	break;
    case SVt_PV:
	fprintf(stderr,"PV%s\n", tmpbuf);
	break;
    case SVt_PVIV:
	fprintf(stderr,"PVIV%s\n", tmpbuf);
	break;
    case SVt_PVNV:
	fprintf(stderr,"PVNV%s\n", tmpbuf);
	break;
    case SVt_PVBM:
	fprintf(stderr,"PVBM%s\n", tmpbuf);
	break;
    case SVt_PVMG:
	fprintf(stderr,"PVMG%s\n", tmpbuf);
	break;
    case SVt_PVLV:
	fprintf(stderr,"PVLV%s\n", tmpbuf);
	break;
    case SVt_PVAV:
	fprintf(stderr,"PVAV%s\n", tmpbuf);
	break;
    case SVt_PVHV:
	fprintf(stderr,"PVHV%s\n", tmpbuf);
	break;
    case SVt_PVCV:
	fprintf(stderr,"PVCV%s\n", tmpbuf);
	break;
    case SVt_PVGV:
	fprintf(stderr,"PVGV%s\n", tmpbuf);
	break;
    case SVt_PVFM:
	fprintf(stderr,"PVFM%s\n", tmpbuf);
	break;
    case SVt_PVIO:
	fprintf(stderr,"PVIO%s\n", tmpbuf);
	break;
    default:
	fprintf(stderr,"UNKNOWN%s\n", tmpbuf);
	return;
    }
    if (type >= SVt_PVIV || type == SVt_IV)
	fprintf(stderr, "  IV = %ld\n", (long)SvIVX(sv));
    if (type >= SVt_PVNV || type == SVt_NV)
	fprintf(stderr, "  NV = %.*g\n", DBL_DIG, SvNVX(sv));
    if (SvROK(sv)) {
	fprintf(stderr, "  RV = 0x%lx\n", (long)SvRV(sv));
	sv_dump(SvRV(sv));
	return;
    }
    if (type < SVt_PV)
	return;
    if (type <= SVt_PVLV) {
	if (SvPVX(sv))
	    fprintf(stderr, "  PV = 0x%lx \"%s\"\n  CUR = %ld\n  LEN = %ld\n",
		(long)SvPVX(sv), SvPVX(sv), (long)SvCUR(sv), (long)SvLEN(sv));
	else
	    fprintf(stderr, "  PV = 0\n");
    }
    if (type >= SVt_PVMG) {
	if (SvMAGIC(sv)) {
	    fprintf(stderr, "  MAGIC = 0x%lx\n", (long)SvMAGIC(sv));
	}
	if (SvSTASH(sv))
	    fprintf(stderr, "  STASH = %s\n", HvNAME(SvSTASH(sv)));
    }
    switch (type) {
    case SVt_PVLV:
	fprintf(stderr, "  TYPE = %c\n", LvTYPE(sv));
	fprintf(stderr, "  TARGOFF = %ld\n", (long)LvTARGOFF(sv));
	fprintf(stderr, "  TARGLEN = %ld\n", (long)LvTARGLEN(sv));
	fprintf(stderr, "  TARG = 0x%lx\n", (long)LvTARG(sv));
	sv_dump(LvTARG(sv));
	break;
    case SVt_PVAV:
	fprintf(stderr, "  ARRAY = 0x%lx\n", (long)AvARRAY(sv));
	fprintf(stderr, "  ALLOC = 0x%lx\n", (long)AvALLOC(sv));
	fprintf(stderr, "  FILL = %ld\n", (long)AvFILL(sv));
	fprintf(stderr, "  MAX = %ld\n", (long)AvMAX(sv));
	fprintf(stderr, "  ARYLEN = 0x%lx\n", (long)AvARYLEN(sv));
	flags = AvFLAGS(sv);
	d = tmpbuf;
	if (flags & AVf_REAL)	strcat(d, "REAL,");
	if (flags & AVf_REIFY)	strcat(d, "REIFY,");
	if (flags & AVf_REUSED)	strcat(d, "REUSED,");
	if (*d)
	    d[strlen(d)-1] = '\0';
	fprintf(stderr, "  FLAGS = (%s)\n", d);
	break;
    case SVt_PVHV:
	fprintf(stderr, "  ARRAY = 0x%lx\n",(long)HvARRAY(sv));
	fprintf(stderr, "  KEYS = %ld\n", (long)HvKEYS(sv));
	fprintf(stderr, "  FILL = %ld\n", (long)HvFILL(sv));
	fprintf(stderr, "  MAX = %ld\n", (long)HvMAX(sv));
	fprintf(stderr, "  RITER = %ld\n", (long)HvRITER(sv));
	fprintf(stderr, "  EITER = 0x%lx\n",(long) HvEITER(sv));
	if (HvPMROOT(sv))
	    fprintf(stderr, "  PMROOT = 0x%lx\n",(long)HvPMROOT(sv));
	if (HvNAME(sv))
	    fprintf(stderr, "  NAME = \"%s\"\n", HvNAME(sv));
	break;
    case SVt_PVFM:
    case SVt_PVCV:
	fprintf(stderr, "  STASH = 0x%lx\n", (long)CvSTASH(sv));
	fprintf(stderr, "  START = 0x%lx\n", (long)CvSTART(sv));
	fprintf(stderr, "  ROOT = 0x%lx\n", (long)CvROOT(sv));
	fprintf(stderr, "  XSUB = 0x%lx\n", (long)CvXSUB(sv));
	fprintf(stderr, "  XSUBANY = %ld\n", (long)CvXSUBANY(sv).any_i32);
	fprintf(stderr, "  FILEGV = 0x%lx\n", (long)CvFILEGV(sv));
	fprintf(stderr, "  DEPTH = %ld\n", (long)CvDEPTH(sv));
	fprintf(stderr, "  PADLIST = 0x%lx\n", (long)CvPADLIST(sv));
	fprintf(stderr, "  OUTSIDE = 0x%lx\n", (long)CvOUTSIDE(sv));
	if (type == SVt_PVFM)
	    fprintf(stderr, "  LINES = %ld\n", (long)FmLINES(sv));
	break;
    case SVt_PVGV:
	fprintf(stderr, "  NAME = %s\n", GvNAME(sv));
	fprintf(stderr, "  NAMELEN = %ld\n", (long)GvNAMELEN(sv));
	fprintf(stderr, "  STASH = 0x%lx\n", (long)GvSTASH(sv));
	fprintf(stderr, "  GP = 0x%lx\n", (long)GvGP(sv));
	fprintf(stderr, "    SV = 0x%lx\n", (long)GvSV(sv));
	fprintf(stderr, "    REFCNT = %ld\n", (long)GvREFCNT(sv));
	fprintf(stderr, "    IO = 0x%lx\n", (long)GvIOp(sv));
	fprintf(stderr, "    FORM = 0x%lx\n", (long)GvFORM(sv));
	fprintf(stderr, "    AV = 0x%lx\n", (long)GvAV(sv));
	fprintf(stderr, "    HV = 0x%lx\n", (long)GvHV(sv));
	fprintf(stderr, "    CV = 0x%lx\n", (long)GvCV(sv));
	fprintf(stderr, "    CVGEN = 0x%lx\n", (long)GvCVGEN(sv));
	fprintf(stderr, "    LASTEXPR = %ld\n", (long)GvLASTEXPR(sv));
	fprintf(stderr, "    LINE = %ld\n", (long)GvLINE(sv));
	fprintf(stderr, "    FLAGS = 0x%x\n", (int)GvFLAGS(sv));
	fprintf(stderr, "    STASH = 0x%lx\n", (long)GvSTASH(sv));
	fprintf(stderr, "    EGV = 0x%lx\n", (long)GvEGV(sv));
	break;
    case SVt_PVIO:
	fprintf(stderr, "  IFP = 0x%lx\n", (long)IoIFP(sv));
	fprintf(stderr, "  OFP = 0x%lx\n", (long)IoOFP(sv));
	fprintf(stderr, "  DIRP = 0x%lx\n", (long)IoDIRP(sv));
	fprintf(stderr, "  LINES = %ld\n", (long)IoLINES(sv));
	fprintf(stderr, "  PAGE = %ld\n", (long)IoPAGE(sv));
	fprintf(stderr, "  PAGE_LEN = %ld\n", (long)IoPAGE_LEN(sv));
	fprintf(stderr, "  LINES_LEFT = %ld\n", (long)IoLINES_LEFT(sv));
	fprintf(stderr, "  TOP_NAME = %s\n", IoTOP_NAME(sv));
	fprintf(stderr, "  TOP_GV = 0x%lx\n", (long)IoTOP_GV(sv));
	fprintf(stderr, "  FMT_NAME = %s\n", IoFMT_NAME(sv));
	fprintf(stderr, "  FMT_GV = 0x%lx\n", (long)IoFMT_GV(sv));
	fprintf(stderr, "  BOTTOM_NAME = %s\n", IoBOTTOM_NAME(sv));
	fprintf(stderr, "  BOTTOM_GV = 0x%lx\n", (long)IoBOTTOM_GV(sv));
	fprintf(stderr, "  SUBPROCESS = %ld\n", (long)IoSUBPROCESS(sv));
	fprintf(stderr, "  TYPE = %c\n", IoTYPE(sv));
	fprintf(stderr, "  FLAGS = 0x%lx\n", (long)IoFLAGS(sv));
	break;
    }
}
#else
void
sv_dump(sv)
SV* sv;
{
}
#endif

IO*
sv_2io(sv)
SV *sv;
{
    IO* io;
    GV* gv;

    switch (SvTYPE(sv)) {
    case SVt_PVIO:
	io = (IO*)sv;
	break;
    case SVt_PVGV:
	gv = (GV*)sv;
	io = GvIO(gv);
	if (!io)
	    croak("Bad filehandle: %s", GvNAME(gv));
	break;
    default:
	if (!SvOK(sv))
	    croak(no_usym, "filehandle");
	if (SvROK(sv))
	    return sv_2io(SvRV(sv));
	gv = gv_fetchpv(SvPV(sv,na), FALSE, SVt_PVIO);
	if (gv)
	    io = GvIO(gv);
	else
	    io = 0;
	if (!io)
	    croak("Bad filehandle: %s", SvPV(sv,na));
	break;
    }
    return io;
}

