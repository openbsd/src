#if defined(PERL_EXT_RE_DEBUG) && !defined(DEBUGGING)
#  define DEBUGGING
#endif

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

START_EXTERN_C

extern regexp*	my_regcomp (pTHX_ char* exp, char* xend, PMOP* pm);
extern I32	my_regexec (pTHX_ regexp* prog, char* stringarg, char* strend,
			    char* strbeg, I32 minend, SV* screamer,
			    void* data, U32 flags);
extern void	my_regfree (pTHX_ struct regexp* r);
extern char*	my_re_intuit_start (pTHX_ regexp *prog, SV *sv, char *strpos,
				    char *strend, U32 flags,
				    struct re_scream_pos_data_s *data);
extern SV*	my_re_intuit_string (pTHX_ regexp *prog);

END_EXTERN_C

#define MY_CXT_KEY "re::_guts" XS_VERSION

typedef struct {
    int		x_oldflag;		/* debug flag */
} my_cxt_t;

START_MY_CXT

#define oldflag		(MY_CXT.x_oldflag)

static void
uninstall(pTHX)
{
    dMY_CXT;
    PL_regexecp = Perl_regexec_flags;
    PL_regcompp = Perl_pregcomp;
    PL_regint_start = Perl_re_intuit_start;
    PL_regint_string = Perl_re_intuit_string;
    PL_regfree = Perl_pregfree;

    if (!oldflag)
	PL_debug &= ~DEBUG_r_FLAG;
}

static void
install(pTHX)
{
    dMY_CXT;
    PL_colorset = 0;			/* Allow reinspection of ENV. */
    PL_regexecp = &my_regexec;
    PL_regcompp = &my_regcomp;
    PL_regint_start = &my_re_intuit_start;
    PL_regint_string = &my_re_intuit_string;
    PL_regfree = &my_regfree;
    oldflag = PL_debug & DEBUG_r_FLAG;
    PL_debug |= DEBUG_r_FLAG;
}

MODULE = re	PACKAGE = re

BOOT:
{
   MY_CXT_INIT;
}


void
install()
  CODE:
    install(aTHX);

void
uninstall()
  CODE:
    uninstall(aTHX);
