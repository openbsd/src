/*    hv.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

typedef struct he HE;

struct he {
    HE		*hent_next;
    char	*hent_key;
    SV		*hent_val;
    U32		hent_hash;
    I32		hent_klen;
};

struct xpvhv {
    char *	xhv_array;	/* pointer to malloced string */
    STRLEN	xhv_fill;	/* how full xhv_array currently is */
    STRLEN	xhv_max;	/* subscript of last element of xhv_array */
    I32		xhv_keys;	/* how many elements in the array */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    I32		xhv_riter;	/* current root of iterator */
    HE		*xhv_eiter;	/* current entry of iterator */
    PMOP	*xhv_pmroot;	/* list of pm's for this package */
    char	*xhv_name;	/* name, if a symbol table */
};

#define Nullhv Null(HV*)
#define HvARRAY(hv)	((HE**)((XPVHV*)  SvANY(hv))->xhv_array)
#define HvFILL(hv)	((XPVHV*)  SvANY(hv))->xhv_fill
#define HvMAX(hv)	((XPVHV*)  SvANY(hv))->xhv_max
#define HvKEYS(hv)	((XPVHV*)  SvANY(hv))->xhv_keys
#define HvRITER(hv)	((XPVHV*)  SvANY(hv))->xhv_riter
#define HvEITER(hv)	((XPVHV*)  SvANY(hv))->xhv_eiter
#define HvPMROOT(hv)	((XPVHV*)  SvANY(hv))->xhv_pmroot
#define HvNAME(hv)	((XPVHV*)  SvANY(hv))->xhv_name

#ifdef OVERLOAD

/* Maybe amagical: */
/* #define HV_AMAGICmb(hv)      (SvFLAGS(hv) & (SVpgv_badAM | SVpgv_AM)) */

#define HV_AMAGIC(hv)        (SvFLAGS(hv) &   SVpgv_AM)
#define HV_AMAGIC_on(hv)     (SvFLAGS(hv) |=  SVpgv_AM)
#define HV_AMAGIC_off(hv)    (SvFLAGS(hv) &= ~SVpgv_AM)

/*
#define HV_AMAGICbad(hv)     (SvFLAGS(hv) & SVpgv_badAM)
#define HV_badAMAGIC_on(hv)  (SvFLAGS(hv) |= SVpgv_badAM)
#define HV_badAMAGIC_off(hv) (SvFLAGS(hv) &= ~SVpgv_badAM)
*/

#endif /* OVERLOAD */
