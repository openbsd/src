/*    av.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "...for the Entwives desired order, and plenty, and peace (by which they
 * meant that things should remain where they had set them)." --Treebeard
 */

#include "EXTERN.h"
#include "perl.h"

static void	av_reify _((AV* av));

static void
av_reify(av)
AV* av;
{
    I32 key;
    SV* sv;
    
    key = AvMAX(av) + 1;
    while (key > AvFILL(av) + 1)
	AvARRAY(av)[--key] = &sv_undef;
    while (key) {
	sv = AvARRAY(av)[--key];
	assert(sv);
	if (sv != &sv_undef)
	    (void)SvREFCNT_inc(sv);
    }
    AvREAL_on(av);
}

void
av_extend(av,key)
AV *av;
I32 key;
{
    if (key > AvMAX(av)) {
	SV** ary;
	I32 tmp;
	I32 newmax;

	if (AvALLOC(av) != AvARRAY(av)) {
	    ary = AvALLOC(av) + AvFILL(av) + 1;
	    tmp = AvARRAY(av) - AvALLOC(av);
	    Move(AvARRAY(av), AvALLOC(av), AvFILL(av)+1, SV*);
	    AvMAX(av) += tmp;
	    SvPVX(av) = (char*)AvALLOC(av);
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &sv_undef;
	    }
	    
	    if (key > AvMAX(av) - 10) {
		newmax = key + AvMAX(av);
		goto resize;
	    }
	}
	else {
	    if (AvALLOC(av)) {
#ifndef STRANGE_MALLOC
		U32 bytes;
#endif

		newmax = key + AvMAX(av) / 5;
	      resize:
#ifdef STRANGE_MALLOC
		Renew(AvALLOC(av),newmax+1, SV*);
#else
		bytes = (newmax + 1) * sizeof(SV*);
#define MALLOC_OVERHEAD 16
		tmp = MALLOC_OVERHEAD;
		while (tmp - MALLOC_OVERHEAD < bytes)
		    tmp += tmp;
		tmp -= MALLOC_OVERHEAD;
		tmp /= sizeof(SV*);
		assert(tmp > newmax);
		newmax = tmp - 1;
		New(2,ary, newmax+1, SV*);
		Copy(AvALLOC(av), ary, AvMAX(av)+1, SV*);
		if (AvMAX(av) > 64 && !nice_chunk) {
		    nice_chunk = (char*)AvALLOC(av);
		    nice_chunk_size = (AvMAX(av) + 1) * sizeof(SV*);
		}
		else
		    Safefree(AvALLOC(av));
		AvALLOC(av) = ary;
#endif
		ary = AvALLOC(av) + AvMAX(av) + 1;
		tmp = newmax - AvMAX(av);
		if (av == stack) {	/* Oops, grew stack (via av_store()?) */
		    stack_sp = AvALLOC(av) + (stack_sp - stack_base);
		    stack_base = AvALLOC(av);
		    stack_max = stack_base + newmax;
		}
	    }
	    else {
		newmax = key < 4 ? 4 : key;
		New(2,AvALLOC(av), newmax+1, SV*);
		ary = AvALLOC(av) + 1;
		tmp = newmax;
		AvALLOC(av)[0] = &sv_undef;	/* For the stacks */
	    }
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &sv_undef;
	    }
	    
	    SvPVX(av) = (char*)AvALLOC(av);
	    AvMAX(av) = newmax;
	}
    }
}

SV**
av_fetch(av,key,lval)
register AV *av;
I32 key;
I32 lval;
{
    SV *sv;

    if (!av)
	return 0;

    if (SvRMAGICAL(av)) {
	if (mg_find((SV*)av,'P')) {
	    sv = sv_newmortal();
	    mg_copy((SV*)av, sv, 0, key);
	    Sv = sv;
	    return &Sv;
	}
    }

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return 0;
    }
    else if (key > AvFILL(av)) {
	if (!lval)
	    return 0;
	if (AvREALISH(av))
	    sv = NEWSV(5,0);
	else
	    sv = sv_newmortal();
	return av_store(av,key,sv);
    }
    if (AvARRAY(av)[key] == &sv_undef) {
	if (lval) {
	    sv = NEWSV(6,0);
	    return av_store(av,key,sv);
	}
	return 0;
    }
    return &AvARRAY(av)[key];
}

SV**
av_store(av,key,val)
register AV *av;
I32 key;
SV *val;
{
    SV** ary;

    if (!av)
	return 0;

    if (SvRMAGICAL(av)) {
	if (mg_find((SV*)av,'P')) {
	    mg_copy((SV*)av, val, 0, key);
	    return 0;
	}
    }

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return 0;
    }
    if (!val)
	val = &sv_undef;

    if (key > AvMAX(av))
	av_extend(av,key);
    if (AvREIFY(av))
	av_reify(av);

    ary = AvARRAY(av);
    if (AvFILL(av) < key) {
	if (!AvREAL(av)) {
	    if (av == stack && key > stack_sp - stack_base)
		stack_sp = stack_base + key;	/* XPUSH in disguise */
	    do
		ary[++AvFILL(av)] = &sv_undef;
	    while (AvFILL(av) < key);
	}
	AvFILL(av) = key;
    }
    else if (AvREAL(av))
	SvREFCNT_dec(ary[key]);
    ary[key] = val;
    if (SvSMAGICAL(av)) {
	if (val != &sv_undef) {
	    MAGIC* mg = SvMAGIC(av);
	    sv_magic(val, (SV*)av, toLOWER(mg->mg_type), 0, key);
	}
	mg_set((SV*)av);
    }
    return &ary[key];
}

AV *
newAV()
{
    register AV *av;

    av = (AV*)NEWSV(3,0);
    sv_upgrade((SV *)av, SVt_PVAV);
    AvREAL_on(av);
    AvALLOC(av) = 0;
    SvPVX(av) = 0;
    AvMAX(av) = AvFILL(av) = -1;
    return av;
}

AV *
av_make(size,strp)
register I32 size;
register SV **strp;
{
    register AV *av;
    register I32 i;
    register SV** ary;

    av = (AV*)NEWSV(8,0);
    sv_upgrade((SV *) av,SVt_PVAV);
    New(4,ary,size+1,SV*);
    AvALLOC(av) = ary;
    AvFLAGS(av) = AVf_REAL;
    SvPVX(av) = (char*)ary;
    AvFILL(av) = size - 1;
    AvMAX(av) = size - 1;
    for (i = 0; i < size; i++) {
	assert (*strp);
	ary[i] = NEWSV(7,0);
	sv_setsv(ary[i], *strp);
	strp++;
    }
    return av;
}

AV *
av_fake(size,strp)
register I32 size;
register SV **strp;
{
    register AV *av;
    register SV** ary;

    av = (AV*)NEWSV(9,0);
    sv_upgrade((SV *)av, SVt_PVAV);
    New(4,ary,size+1,SV*);
    AvALLOC(av) = ary;
    Copy(strp,ary,size,SV*);
    AvFLAGS(av) = AVf_REIFY;
    SvPVX(av) = (char*)ary;
    AvFILL(av) = size - 1;
    AvMAX(av) = size - 1;
    while (size--) {
	assert (*strp);
	SvTEMP_off(*strp);
	strp++;
    }
    return av;
}

void
av_clear(av)
register AV *av;
{
    register I32 key;
    SV** ary;

    if (!av || AvMAX(av) < 0)
	return;
    /*SUPPRESS 560*/

    if (AvREAL(av)) {
	ary = AvARRAY(av);
	key = AvFILL(av) + 1;
	while (key) {
	    SvREFCNT_dec(ary[--key]);
	    ary[key] = &sv_undef;
	}
    }
    if (key = AvARRAY(av) - AvALLOC(av)) {
	AvMAX(av) += key;
	SvPVX(av) = (char*)AvALLOC(av);
    }
    AvFILL(av) = -1;
}

void
av_undef(av)
register AV *av;
{
    register I32 key;

    if (!av)
	return;
    /*SUPPRESS 560*/
    if (AvREAL(av)) {
	key = AvFILL(av) + 1;
	while (key)
	    SvREFCNT_dec(AvARRAY(av)[--key]);
    }
    if (key = AvARRAY(av) - AvALLOC(av)) {
	AvMAX(av) += key;
	SvPVX(av) = (char*)AvALLOC(av);
    }
    Safefree(AvALLOC(av));
    AvALLOC(av) = 0;
    SvPVX(av) = 0;
    AvMAX(av) = AvFILL(av) = -1;
    if (AvARYLEN(av)) {
	SvREFCNT_dec(AvARYLEN(av));
	AvARYLEN(av) = 0;
    }
}

void
av_push(av,val)
register AV *av;
SV *val;
{
    if (!av)
	return;
    av_store(av,AvFILL(av)+1,val);
}

SV *
av_pop(av)
register AV *av;
{
    SV *retval;

    if (!av || AvFILL(av) < 0)
	return &sv_undef;
    retval = AvARRAY(av)[AvFILL(av)];
    AvARRAY(av)[AvFILL(av)--] = &sv_undef;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

void
av_unshift(av,num)
register AV *av;
register I32 num;
{
    register I32 i;
    register SV **sstr,**dstr;

    if (!av || num <= 0)
	return;
    if (!AvREAL(av)) {
	if (AvREIFY(av))
	    av_reify(av);
	else
	    croak("Can't unshift");
    }
    i = AvARRAY(av) - AvALLOC(av);
    if (i) {
	if (i > num)
	    i = num;
	num -= i;
    
	AvMAX(av) += i;
	AvFILL(av) += i;
	SvPVX(av) = (char*)(AvARRAY(av) - i);
    }
    if (num) {
	av_extend(av,AvFILL(av)+num);
	AvFILL(av) += num;
	dstr = AvARRAY(av) + AvFILL(av);
	sstr = dstr - num;
#ifdef BUGGY_MSC5
 # pragma loop_opt(off)	/* don't loop-optimize the following code */
#endif /* BUGGY_MSC5 */
	for (i = AvFILL(av) - num; i >= 0; --i) {
	    *dstr-- = *sstr--;
#ifdef BUGGY_MSC5
 # pragma loop_opt()	/* loop-optimization back to command-line setting */
#endif /* BUGGY_MSC5 */
	}
	while (num)
	    AvARRAY(av)[--num] = &sv_undef;
    }
}

SV *
av_shift(av)
register AV *av;
{
    SV *retval;

    if (!av || AvFILL(av) < 0)
	return &sv_undef;
    retval = *AvARRAY(av);
    if (AvREAL(av))
	*AvARRAY(av) = &sv_undef;
    SvPVX(av) = (char*)(AvARRAY(av) + 1);
    AvMAX(av)--;
    AvFILL(av)--;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

I32
av_len(av)
register AV *av;
{
    return AvFILL(av);
}

void
av_fill(av, fill)
register AV *av;
I32 fill;
{
    if (!av)
	croak("panic: null array");
    if (fill < 0)
	fill = -1;
    if (fill <= AvMAX(av)) {
	I32 key = AvFILL(av);
	SV** ary = AvARRAY(av);

	if (AvREAL(av)) {
	    while (key > fill) {
		SvREFCNT_dec(ary[key]);
		ary[key--] = &sv_undef;
	    }
	}
	else {
	    while (key < fill)
		ary[++key] = &sv_undef;
	}
	    
	AvFILL(av) = fill;
	if (SvSMAGICAL(av))
	    mg_set((SV*)av);
    }
    else
	(void)av_store(av,fill,&sv_undef);
}
