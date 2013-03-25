#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "bsd_glob.h"

#define MY_CXT_KEY "File::Glob::_guts" XS_VERSION

typedef struct {
    int		x_GLOB_ERROR;
    HV *	x_GLOB_ENTRIES;
} my_cxt_t;

START_MY_CXT

#define GLOB_ERROR	(MY_CXT.x_GLOB_ERROR)

#include "const-c.inc"

#ifdef WIN32
#define errfunc		NULL
#else
static int
errfunc(const char *foo, int bar) {
  PERL_UNUSED_ARG(foo);
  return !(bar == EACCES || bar == ENOENT || bar == ENOTDIR);
}
#endif

static void
doglob(pTHX_ const char *pattern, int flags)
{
    dSP;
    glob_t pglob;
    int i;
    int retval;
    SV *tmp;
    {
	dMY_CXT;

	/* call glob */
	memset(&pglob, 0, sizeof(glob_t));
	retval = bsd_glob(pattern, flags, errfunc, &pglob);
	GLOB_ERROR = retval;

	/* return any matches found */
	EXTEND(sp, pglob.gl_pathc);
	for (i = 0; i < pglob.gl_pathc; i++) {
	    /* printf("# bsd_glob: %s\n", pglob.gl_pathv[i]); */
	    tmp = newSVpvn_flags(pglob.gl_pathv[i], strlen(pglob.gl_pathv[i]),
				 SVs_TEMP);
	    TAINT;
	    SvTAINT(tmp);
	    PUSHs(tmp);
	}
	PUTBACK;

	bsd_globfree(&pglob);
    }
}

static void
iterate(pTHX_ bool(*globber)(pTHX_ AV *entries, SV *patsv))
{
    dSP;
    dMY_CXT;

    SV * const cxixsv = POPs;
    const char *cxixpv;
    STRLEN cxixlen;
    AV *entries;
    U32 const gimme = GIMME_V;
    SV *patsv = POPs;
    bool on_stack = FALSE;

    /* assume global context if not provided one */
    SvGETMAGIC(cxixsv);
    if (SvOK(cxixsv)) cxixpv = SvPV_nomg(cxixsv, cxixlen);
    else cxixpv = "_G_", cxixlen = 3;

    if (!MY_CXT.x_GLOB_ENTRIES) MY_CXT.x_GLOB_ENTRIES = newHV();
    entries = (AV *)*(hv_fetch(MY_CXT.x_GLOB_ENTRIES, cxixpv, cxixlen, 1));

    /* if we're just beginning, do it all first */
    if (SvTYPE(entries) != SVt_PVAV) {
	PUTBACK;
	on_stack = globber(aTHX_ entries, patsv);
	SPAGAIN;
    }

    /* chuck it all out, quick or slow */
    if (gimme == G_ARRAY) {
	if (!on_stack) {
	    Copy(AvARRAY(entries), SP+1, AvFILLp(entries)+1, SV *);
	    SP += AvFILLp(entries)+1;
	}
	/* No G_DISCARD here!  It will free the stack items. */
	hv_delete(MY_CXT.x_GLOB_ENTRIES, cxixpv, cxixlen, 0);
    }
    else {
	if (AvFILLp(entries) + 1) {
	    mPUSHs(av_shift(entries));
	}
	else {
	    /* return undef for EOL */
	    hv_delete(MY_CXT.x_GLOB_ENTRIES, cxixpv, cxixlen, G_DISCARD);
	    PUSHs(&PL_sv_undef);
	}
    }
    PUTBACK;
}

/* returns true if the items are on the stack already, but only in
   list context */
static bool
csh_glob(pTHX_ AV *entries, SV *patsv)
{
	dSP;
	const char *pat;
	AV *patav = NULL;
	const char *patend;
	const char *s = NULL;
	const char *piece = NULL;
	SV *word = NULL;
	int const flags =
	    (int)SvIV(get_sv("File::Glob::DEFAULT_FLAGS", GV_ADD));
	bool is_utf8;
	STRLEN len;
	U32 const gimme = GIMME_V;

	/* glob without args defaults to $_ */
	SvGETMAGIC(patsv);
	if (
	    !SvOK(patsv)
	 && (patsv = DEFSV, SvGETMAGIC(patsv), !SvOK(patsv))
	)
	     pat = "", len = 0, is_utf8 = 0;
	else pat = SvPV_nomg(patsv,len), is_utf8 = !!SvUTF8(patsv);
	patend = pat + len;

	/* extract patterns */
	s = pat-1;
	while (++s < patend) {
	    switch (*s) {
	    case '\'':
	    case '"' :
	      {
		bool found = FALSE;
		const char quote = *s;
		if (!word) {
		    word = newSVpvs("");
		    if (is_utf8) SvUTF8_on(word);
		}
		if (piece) sv_catpvn(word, piece, s-piece);
		piece = s+1;
		while (++s < patend)
		    if (*s == '\\') {
			s++;
			/* If the backslash is here to escape a quote,
			   obliterate it. */
			if (s < patend && *s == quote)
			    sv_catpvn(word, piece, s-piece-1), piece = s;
		    }
		    else if (*s == quote) {
			sv_catpvn(word, piece, s-piece);
			piece = NULL;
			found = TRUE;
			break;
		    }
		if (!found) { /* unmatched quote */
		    /* Give up on tokenisation and treat the whole string
		       as a single token, but with whitespace stripped. */
		    piece = pat;
		    while (isSPACE(*pat)) pat++;
		    while (isSPACE(*(patend-1))) patend--;
		    /* bsd_glob expects a trailing null, but we cannot mod-
		       ify the original */
		    if (patend < SvEND(patsv)) {
			if (word) sv_setpvn(word, pat, patend-pat);
			else
			    word = newSVpvn_flags(
				pat, patend-pat, SVf_UTF8*is_utf8
			    );
			piece = NULL;
		    }
		    else {
			if (word) SvREFCNT_dec(word), word=NULL;
			piece = pat;
			s = patend;
		    }
		    goto end_of_parsing;
		}
		break;
	      }
	    case '\\':
		if (!piece) piece = s;
		s++;
		/* If the backslash is here to escape a quote,
		   obliterate it. */
		if (s < patend && (*s == '"' || *s == '\'')) {
		    if (!word) {
			word = newSVpvn(piece,s-piece-1);
			if (is_utf8) SvUTF8_on(word);
		    }
		    else sv_catpvn(word, piece, s-piece-1);
		    piece = s;
		}
		break;
	    default:
		if (isSPACE(*s)) {
		    if (piece) {
			if (!word) {
			    word = newSVpvn(piece,s-piece);
			    if (is_utf8) SvUTF8_on(word);
			}
			else sv_catpvn(word, piece, s-piece);
		    }
		    if (!word) break;
		    if (!patav) patav = (AV *)sv_2mortal((SV *)newAV());
		    av_push(patav, word);
		    word = NULL;
		    piece = NULL;
		}
		else if (!piece) piece = s;
		break;
	    }
	}
      end_of_parsing:

	assert(SvTYPE(entries) != SVt_PVAV);
	sv_upgrade((SV *)entries, SVt_PVAV);
	
	if (patav) {
	    I32 items = AvFILLp(patav) + 1;
	    SV **svp = AvARRAY(patav);
	    while (items--) {
		PUSHMARK(SP);
		PUTBACK;
		doglob(aTHX_ SvPVXx(*svp++), flags);
		SPAGAIN;
		{
		    dMARK;
		    dORIGMARK;
		    while (++MARK <= SP)
			av_push(entries, SvREFCNT_inc_simple_NN(*MARK));
		    SP = ORIGMARK;
		}
	    }
	}
	/* piece is set at this point if there is no trailing whitespace.
	   It is the beginning of the last token or quote-delimited
	   piece thereof.  word is set at this point if the last token has
	   multiple quoted pieces. */
	if (piece || word) {
	    if (word) {
		if (piece) sv_catpvn(word, piece, s-piece);
		piece = SvPVX(word);
	    }
	    PUSHMARK(SP);
	    PUTBACK;
	    doglob(aTHX_ piece, flags);
	    if (word) SvREFCNT_dec(word);
	    SPAGAIN;
	    {
		dMARK;
		dORIGMARK;
		/* short-circuit here for a fairly common case */
		if (!patav && gimme == G_ARRAY) { PUTBACK; return TRUE; }
		while (++MARK <= SP)
		    av_push(entries, SvREFCNT_inc_simple_NN(*MARK));

		SP = ORIGMARK;
	    }
	}
	PUTBACK;
	return FALSE;
}

static void
csh_glob_iter(pTHX)
{
    iterate(aTHX_ csh_glob);
}

/* wrapper around doglob that can be passed to the iterator */
static bool
doglob_iter_wrapper(pTHX_ AV *entries, SV *patsv)
{
    dSP;
    const char *pattern;
    int const flags =
	    (int)SvIV(get_sv("File::Glob::DEFAULT_FLAGS", GV_ADD));

    SvGETMAGIC(patsv);
    if (
	    !SvOK(patsv)
	 && (patsv = DEFSV, SvGETMAGIC(patsv), !SvOK(patsv))
    )
	 pattern = "";
    else pattern = SvPV_nomg_nolen(patsv);

    PUSHMARK(SP);
    PUTBACK;
    doglob(aTHX_ pattern, flags);
    SPAGAIN;
    {
	dMARK;
	dORIGMARK;
	if (GIMME_V == G_ARRAY) { PUTBACK; return TRUE; }
	sv_upgrade((SV *)entries, SVt_PVAV);
	while (++MARK <= SP)
	    av_push(entries, SvREFCNT_inc_simple_NN(*MARK));
	SP = ORIGMARK;
    }
    return FALSE;
}

MODULE = File::Glob		PACKAGE = File::Glob

int
GLOB_ERROR()
    PREINIT:
	dMY_CXT;
    CODE:
	RETVAL = GLOB_ERROR;
    OUTPUT:
	RETVAL

void
bsd_glob(pattern,...)
    char *pattern
PREINIT:
    int flags = 0;
PPCODE:
    {
	/* allow for optional flags argument */
	if (items > 1) {
	    flags = (int) SvIV(ST(1));
	    /* remove unsupported flags */
	    flags &= ~(GLOB_APPEND | GLOB_DOOFFS | GLOB_ALTDIRFUNC | GLOB_MAGCHAR);
	} else {
	    flags = (int) SvIV(get_sv("File::Glob::DEFAULT_FLAGS", GV_ADD));
	}
	
	PUTBACK;
	doglob(aTHX_ pattern, flags);
	SPAGAIN;
    }

PROTOTYPES: DISABLE
void
csh_glob(...)
PPCODE:
    /* For backward-compatibility with the original Perl function, we sim-
     * ply take the first two arguments, regardless of how many there are.
     */
    if (items >= 2) SP += 2;
    else {
	SP += items;
	XPUSHs(&PL_sv_undef);
	if (!items) XPUSHs(&PL_sv_undef);
    }
    PUTBACK;
    csh_glob_iter(aTHX);
    SPAGAIN;

void
bsd_glob_override(...)
PPCODE:
    if (items >= 2) SP += 2;
    else {
	SP += items;
	XPUSHs(&PL_sv_undef);
	if (!items) XPUSHs(&PL_sv_undef);
    }
    PUTBACK;
    iterate(aTHX_ doglob_iter_wrapper);
    SPAGAIN;

BOOT:
{
#ifndef PERL_EXTERNAL_GLOB
    /* Don't do this at home! The globhook interface is highly volatile. */
    PL_globhook = csh_glob_iter;
#endif
}

BOOT:
{
    MY_CXT_INIT;
    {
	dMY_CXT;
	MY_CXT.x_GLOB_ENTRIES = NULL;
    }  
}

INCLUDE: const-xs.inc
