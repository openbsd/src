#if defined(PERL_EXT_RE_DEBUG) && !defined(DEBUGGING)
#  define DEBUGGING
#endif

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "re_comp.h"


START_EXTERN_C

extern REGEXP*	my_re_compile (pTHX_ SV * const pattern, const U32 pm_flags);
extern I32	my_regexec (pTHX_ REGEXP * const prog, char* stringarg, char* strend,
			    char* strbeg, I32 minend, SV* screamer,
			    void* data, U32 flags);

extern char*	my_re_intuit_start (pTHX_ REGEXP * const prog, SV *sv, char *strpos,
				    char *strend, const U32 flags,
				    struct re_scream_pos_data_s *data);
extern SV*	my_re_intuit_string (pTHX_ REGEXP * const prog);

extern void	my_regfree (pTHX_ REGEXP * const r);

extern void	my_reg_numbered_buff_fetch(pTHX_ REGEXP * const rx, const I32 paren,
					   SV * const usesv);
extern void	my_reg_numbered_buff_store(pTHX_ REGEXP * const rx, const I32 paren,
					   SV const * const value);
extern I32	my_reg_numbered_buff_length(pTHX_ REGEXP * const rx,
					    const SV * const sv, const I32 paren);

extern SV*	my_reg_named_buff(pTHX_ REGEXP * const, SV * const, SV * const,
                              const U32);
extern SV*	my_reg_named_buff_iter(pTHX_ REGEXP * const rx,
                                   const SV * const lastkey, const U32 flags);

extern SV*      my_reg_qr_package(pTHX_ REGEXP * const rx);
#if defined(USE_ITHREADS)
extern void*	my_regdupe (pTHX_ REGEXP * const r, CLONE_PARAMS *param);
#endif

EXTERN_C const struct regexp_engine my_reg_engine;

END_EXTERN_C

const struct regexp_engine my_reg_engine = { 
        my_re_compile, 
        my_regexec, 
        my_re_intuit_start, 
        my_re_intuit_string, 
        my_regfree, 
        my_reg_numbered_buff_fetch,
        my_reg_numbered_buff_store,
        my_reg_numbered_buff_length,
        my_reg_named_buff,
        my_reg_named_buff_iter,
        my_reg_qr_package,
#if defined(USE_ITHREADS)
        my_regdupe 
#endif
};

MODULE = re	PACKAGE = re

void
install()
    PPCODE:
        PL_colorset = 0;	/* Allow reinspection of ENV. */
        /* PL_debug |= DEBUG_r_FLAG; */
	XPUSHs(sv_2mortal(newSViv(PTR2IV(&my_reg_engine))));

void
regmust(sv)
    SV * sv
PROTOTYPE: $
PREINIT:
    REGEXP *re;
PPCODE:
{
    if ((re = SvRX(sv))) /* assign deliberate */
    {
        SV *an = &PL_sv_no;
        SV *fl = &PL_sv_no;
        if (RX_ANCHORED_SUBSTR(re)) {
            an = newSVsv(RX_ANCHORED_SUBSTR(re));
        } else if (RX_ANCHORED_UTF8(re)) {
            an = newSVsv(RX_ANCHORED_UTF8(re));
        }
        if (RX_FLOAT_SUBSTR(re)) {
            fl = newSVsv(RX_FLOAT_SUBSTR(re));
        } else if (RX_FLOAT_UTF8(re)) {
            fl = newSVsv(RX_FLOAT_UTF8(re));
        }
        XPUSHs(an);
        XPUSHs(fl);
        XSRETURN(2);
    }
    XSRETURN_UNDEF;
}

