#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef PERLIO_LAYERS

#include "perliol.h"

typedef struct {
    struct _PerlIO base;	/* Base "class" info */
    SV *var;
    Off_t posn;
} PerlIOScalar;

IV
PerlIOScalar_pushed(pTHX_ PerlIO * f, const char *mode, SV * arg,
		    PerlIO_funcs * tab)
{
    IV code;
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    /* If called (normally) via open() then arg is ref to scalar we are
     * using, otherwise arg (from binmode presumably) is either NULL
     * or the _name_ of the scalar
     */
    if (arg) {
	if (SvROK(arg)) {
	    s->var = SvREFCNT_inc(SvRV(arg));
	    if (!SvPOK(s->var) && SvTYPE(SvRV(arg)) > SVt_NULL)
		(void)SvPV_nolen(s->var);
	}
	else {
	    s->var =
		SvREFCNT_inc(perl_get_sv
			     (SvPV_nolen(arg), GV_ADD | GV_ADDMULTI));
	}
    }
    else {
	s->var = newSVpvn("", 0);
    }
    SvUPGRADE(s->var, SVt_PV);
    code = PerlIOBase_pushed(aTHX_ f, mode, Nullsv, tab);
    if ((PerlIOBase(f)->flags) & PERLIO_F_TRUNCATE)
	SvCUR(s->var) = 0;
    if ((PerlIOBase(f)->flags) & PERLIO_F_APPEND)
	s->posn = SvCUR(s->var);
    else
	s->posn = 0;
    return code;
}

IV
PerlIOScalar_popped(pTHX_ PerlIO * f)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    if (s->var) {
	SvREFCNT_dec(s->var);
	s->var = Nullsv;
    }
    return 0;
}

IV
PerlIOScalar_close(pTHX_ PerlIO * f)
{
    IV code = PerlIOBase_close(aTHX_ f);
    PerlIOBase(f)->flags &= ~(PERLIO_F_RDBUF | PERLIO_F_WRBUF);
    return code;
}

IV
PerlIOScalar_fileno(pTHX_ PerlIO * f)
{
    return -1;
}

IV
PerlIOScalar_seek(pTHX_ PerlIO * f, Off_t offset, int whence)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    switch (whence) {
    case 0:
	s->posn = offset;
	break;
    case 1:
	s->posn = offset + s->posn;
	break;
    case 2:
	s->posn = offset + SvCUR(s->var);
	break;
    }
    if ((STRLEN) s->posn > SvCUR(s->var)) {
	(void) SvGROW(s->var, (STRLEN) s->posn);
    }
    return 0;
}

Off_t
PerlIOScalar_tell(pTHX_ PerlIO * f)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    return s->posn;
}

SSize_t
PerlIOScalar_unread(pTHX_ PerlIO * f, const void *vbuf, Size_t count)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    char *dst = SvGROW(s->var, (STRLEN)s->posn + count);
    Move(vbuf, dst + s->posn, count, char);
    s->posn += count;
    SvCUR_set(s->var, (STRLEN)s->posn);
    SvPOK_on(s->var);
    return count;
}

SSize_t
PerlIOScalar_write(pTHX_ PerlIO * f, const void *vbuf, Size_t count)
{
    if (PerlIOBase(f)->flags & PERLIO_F_CANWRITE) {
	Off_t offset;
	PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
	SV *sv = s->var;
	char *dst;
	if ((PerlIOBase(f)->flags) & PERLIO_F_APPEND) {
	    dst = SvGROW(sv, SvCUR(sv) + count);
	    offset = SvCUR(sv);
	    s->posn = offset + count;
	}
	else {
	    if ((s->posn + count) > SvCUR(sv))
		dst = SvGROW(sv, (STRLEN)s->posn + count);
	    else
		dst = SvPV_nolen(sv);
	    offset = s->posn;
	    s->posn += count;
	}
	Move(vbuf, dst + offset, count, char);
	if ((STRLEN) s->posn > SvCUR(sv))
	    SvCUR_set(sv, (STRLEN)s->posn);
	SvPOK_on(s->var);
	return count;
    }
    else
	return 0;
}

IV
PerlIOScalar_fill(pTHX_ PerlIO * f)
{
    return -1;
}

IV
PerlIOScalar_flush(pTHX_ PerlIO * f)
{
    return 0;
}

STDCHAR *
PerlIOScalar_get_base(pTHX_ PerlIO * f)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    if (PerlIOBase(f)->flags & PERLIO_F_CANREAD) {
	return (STDCHAR *) SvPV_nolen(s->var);
    }
    return (STDCHAR *) Nullch;
}

STDCHAR *
PerlIOScalar_get_ptr(pTHX_ PerlIO * f)
{
    if (PerlIOBase(f)->flags & PERLIO_F_CANREAD) {
	PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
	return PerlIOScalar_get_base(aTHX_ f) + s->posn;
    }
    return (STDCHAR *) Nullch;
}

SSize_t
PerlIOScalar_get_cnt(pTHX_ PerlIO * f)
{
    if (PerlIOBase(f)->flags & PERLIO_F_CANREAD) {
	PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
	if (SvCUR(s->var) > (STRLEN) s->posn)
	    return SvCUR(s->var) - (STRLEN)s->posn;
	else
	    return 0;
    }
    return 0;
}

Size_t
PerlIOScalar_bufsiz(pTHX_ PerlIO * f)
{
    if (PerlIOBase(f)->flags & PERLIO_F_CANREAD) {
	PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
	return SvCUR(s->var);
    }
    return 0;
}

void
PerlIOScalar_set_ptrcnt(pTHX_ PerlIO * f, STDCHAR * ptr, SSize_t cnt)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    s->posn = SvCUR(s->var) - cnt;
}

PerlIO *
PerlIOScalar_open(pTHX_ PerlIO_funcs * self, PerlIO_list_t * layers, IV n,
		  const char *mode, int fd, int imode, int perm,
		  PerlIO * f, int narg, SV ** args)
{
    SV *arg = (narg > 0) ? *args : PerlIOArg;
    if (SvROK(arg) || SvPOK(arg)) {
	if (!f) {
	    f = PerlIO_allocate(aTHX);
	}
	if ( (f = PerlIO_push(aTHX_ f, self, mode, arg)) ) {
	    PerlIOBase(f)->flags |= PERLIO_F_OPEN;
	}
	return f;
    }
    return NULL;
}

SV *
PerlIOScalar_arg(pTHX_ PerlIO * f, CLONE_PARAMS * param, int flags)
{
    PerlIOScalar *s = PerlIOSelf(f, PerlIOScalar);
    SV *var = s->var;
    if (flags & PERLIO_DUP_CLONE)
	var = PerlIO_sv_dup(aTHX_ var, param);
    else if (flags & PERLIO_DUP_FD) {
	/* Equivalent (guesses NI-S) of dup() is to create a new scalar */
	var = newSVsv(var);
    }
    else {
	var = SvREFCNT_inc(var);
    }
    return newRV_noinc(var);
}

PerlIO *
PerlIOScalar_dup(pTHX_ PerlIO * f, PerlIO * o, CLONE_PARAMS * param,
		 int flags)
{
    if ((f = PerlIOBase_dup(aTHX_ f, o, param, flags))) {
	PerlIOScalar *fs = PerlIOSelf(f, PerlIOScalar);
	PerlIOScalar *os = PerlIOSelf(o, PerlIOScalar);
	/* var has been set by implicit push */
	fs->posn = os->posn;
    }
    return f;
}

PerlIO_funcs PerlIO_scalar = {
    sizeof(PerlIO_funcs),
    "scalar",
    sizeof(PerlIOScalar),
    PERLIO_K_BUFFERED | PERLIO_K_RAW,
    PerlIOScalar_pushed,
    PerlIOScalar_popped,
    PerlIOScalar_open,
    PerlIOBase_binmode,
    PerlIOScalar_arg,
    PerlIOScalar_fileno,
    PerlIOScalar_dup,
    PerlIOBase_read,
    PerlIOScalar_unread,
    PerlIOScalar_write,
    PerlIOScalar_seek,
    PerlIOScalar_tell,
    PerlIOScalar_close,
    PerlIOScalar_flush,
    PerlIOScalar_fill,
    PerlIOBase_eof,
    PerlIOBase_error,
    PerlIOBase_clearerr,
    PerlIOBase_setlinebuf,
    PerlIOScalar_get_base,
    PerlIOScalar_bufsiz,
    PerlIOScalar_get_ptr,
    PerlIOScalar_get_cnt,
    PerlIOScalar_set_ptrcnt,
};


#endif /* Layers available */

MODULE = PerlIO::scalar	PACKAGE = PerlIO::scalar

PROTOTYPES: ENABLE

BOOT:
{
#ifdef PERLIO_LAYERS
 PerlIO_define_layer(aTHX_ &PerlIO_scalar);
#endif
}

