/*    av.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct xpvav {
    char*	xav_array;	/* pointer to malloced string */
    SSize_t	xav_fill;
    SSize_t	xav_max;
    IV		xof_off;	/* ptr is incremented by offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    SV**	xav_alloc;
    SV*		xav_arylen;
    U8		xav_flags;
};

#define AVf_REAL 1	/* free old entries */
#define AVf_REIFY 2	/* can become real */
#define AVf_REUSED 4	/* got undeffed--don't turn old memory into SVs now */

#define Nullav Null(AV*)

#define AvARRAY(av)	((SV**)((XPVAV*)  SvANY(av))->xav_array)
#define AvALLOC(av)	((XPVAV*)  SvANY(av))->xav_alloc
#define AvMAX(av)	((XPVAV*)  SvANY(av))->xav_max
#define AvFILL(av)	((XPVAV*)  SvANY(av))->xav_fill
#define AvARYLEN(av)	((XPVAV*)  SvANY(av))->xav_arylen
#define AvFLAGS(av)	((XPVAV*)  SvANY(av))->xav_flags

#define AvREAL(av)	(AvFLAGS(av) & AVf_REAL)
#define AvREAL_on(av)	(AvFLAGS(av) |= AVf_REAL)
#define AvREAL_off(av)	(AvFLAGS(av) &= ~AVf_REAL)
#define AvREIFY(av)	(AvFLAGS(av) & AVf_REIFY)
#define AvREIFY_on(av)	(AvFLAGS(av) |= AVf_REIFY)
#define AvREIFY_off(av)	(AvFLAGS(av) &= ~AVf_REIFY)
#define AvREUSED(av)	(AvFLAGS(av) & AVf_REUSED)
#define AvREUSED_on(av)	(AvFLAGS(av) |= AVf_REUSED)
#define AvREUSED_off(av) (AvFLAGS(av) &= ~AVf_REUSED)

#define AvREALISH(av)	AvFLAGS(av)	/* REAL or REIFY -- shortcut */

