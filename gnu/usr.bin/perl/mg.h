/*    mg.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct mgvtbl {
    int		(*svt_get)	_((SV *sv, MAGIC* mg));
    int		(*svt_set)	_((SV *sv, MAGIC* mg));
    U32		(*svt_len)	_((SV *sv, MAGIC* mg));
    int		(*svt_clear)	_((SV *sv, MAGIC* mg));
    int		(*svt_free)	_((SV *sv, MAGIC* mg));
};

struct magic {
    MAGIC*	mg_moremagic;
    MGVTBL*	mg_virtual;	/* pointer to magic functions */
    U16		mg_private;
    char	mg_type;
    U8		mg_flags;
    SV*		mg_obj;
    char*	mg_ptr;
    I32		mg_len;
};

#define MGf_TAINTEDDIR 1
#define MGf_REFCOUNTED 2
#define MGf_GSKIP      4

#define MGf_MINMATCH   1

#define MgTAINTEDDIR(mg) (mg->mg_flags & MGf_TAINTEDDIR)
#define MgTAINTEDDIR_on(mg) (mg->mg_flags |= MGf_TAINTEDDIR)
