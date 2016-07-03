/*    locale.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *    2002, 2003, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *      A Elbereth Gilthoniel,
 *      silivren penna míriel
 *      o menel aglar elenath!
 *      Na-chaered palan-díriel
 *      o galadhremmin ennorath,
 *      Fanuilos, le linnathon
 *      nef aear, si nef aearon!
 *
 *     [p.238 of _The Lord of the Rings_, II/i: "Many Meetings"]
 */

/* utility functions for handling locale-specific stuff like what
 * character represents the decimal point.
 *
 * All C programs have an underlying locale.  Perl generally doesn't pay any
 * attention to it except within the scope of a 'use locale'.  For most
 * categories, it accomplishes this by just using different operations if it is
 * in such scope than if not.  However, various libc functions called by Perl
 * are affected by the LC_NUMERIC category, so there are macros in perl.h that
 * are used to toggle between the current locale and the C locale depending on
 * the desired behavior of those functions at the moment.
 */

#include "EXTERN.h"
#define PERL_IN_LOCALE_C
#include "perl.h"

#ifdef I_LANGINFO
#   include <langinfo.h>
#endif

#include "reentr.h"

#ifdef USE_LOCALE

/*
 * Standardize the locale name from a string returned by 'setlocale', possibly
 * modifying that string.
 *
 * The typical return value of setlocale() is either
 * (1) "xx_YY" if the first argument of setlocale() is not LC_ALL
 * (2) "xa_YY xb_YY ..." if the first argument of setlocale() is LC_ALL
 *     (the space-separated values represent the various sublocales,
 *      in some unspecified order).  This is not handled by this function.
 *
 * In some platforms it has a form like "LC_SOMETHING=Lang_Country.866\n",
 * which is harmful for further use of the string in setlocale().  This
 * function removes the trailing new line and everything up through the '='
 *
 */
STATIC char *
S_stdize_locale(pTHX_ char *locs)
{
    const char * const s = strchr(locs, '=');
    bool okay = TRUE;

    PERL_ARGS_ASSERT_STDIZE_LOCALE;

    if (s) {
	const char * const t = strchr(s, '.');
	okay = FALSE;
	if (t) {
	    const char * const u = strchr(t, '\n');
	    if (u && (u[1] == 0)) {
		const STRLEN len = u - s;
		Move(s + 1, locs, len, char);
		locs[len] = 0;
		okay = TRUE;
	    }
	}
    }

    if (!okay)
	Perl_croak(aTHX_ "Can't fix broken locale name \"%s\"", locs);

    return locs;
}

#endif

void
Perl_set_numeric_radix(pTHX)
{
#ifdef USE_LOCALE_NUMERIC
    dVAR;
# ifdef HAS_LOCALECONV
    const struct lconv* const lc = localeconv();

    if (lc && lc->decimal_point) {
	if (lc->decimal_point[0] == '.' && lc->decimal_point[1] == 0) {
	    SvREFCNT_dec(PL_numeric_radix_sv);
	    PL_numeric_radix_sv = NULL;
	}
	else {
	    if (PL_numeric_radix_sv)
		sv_setpv(PL_numeric_radix_sv, lc->decimal_point);
	    else
		PL_numeric_radix_sv = newSVpv(lc->decimal_point, 0);
            if (! is_ascii_string((U8 *) lc->decimal_point, 0)
                && is_utf8_string((U8 *) lc->decimal_point, 0)
                && is_cur_LC_category_utf8(LC_NUMERIC))
            {
		SvUTF8_on(PL_numeric_radix_sv);
            }
	}
    }
    else
	PL_numeric_radix_sv = NULL;

    DEBUG_L(PerlIO_printf(Perl_debug_log, "Locale radix is %s\n",
                                          (PL_numeric_radix_sv)
                                          ? lc->decimal_point
                                          : "NULL"));

# endif /* HAS_LOCALECONV */
#endif /* USE_LOCALE_NUMERIC */
}

void
Perl_new_numeric(pTHX_ const char *newnum)
{
#ifdef USE_LOCALE_NUMERIC

    /* Called after all libc setlocale() calls affecting LC_NUMERIC, to tell
     * core Perl this and that 'newnum' is the name of the new locale.
     * It installs this locale as the current underlying default.
     *
     * The default locale and the C locale can be toggled between by use of the
     * set_numeric_local() and set_numeric_standard() functions, which should
     * probably not be called directly, but only via macros like
     * SET_NUMERIC_STANDARD() in perl.h.
     *
     * The toggling is necessary mainly so that a non-dot radix decimal point
     * character can be output, while allowing internal calculations to use a
     * dot.
     *
     * This sets several interpreter-level variables:
     * PL_numeric_name  The default locale's name: a copy of 'newnum'
     * PL_numeric_local A boolean indicating if the toggled state is such
     *                  that the current locale is the default locale
     * PL_numeric_standard A boolean indicating if the toggled state is such
     *                  that the current locale is the C locale
     * Note that both of the last two variables can be true at the same time,
     * if the underlying locale is C.  (Toggling is a no-op under these
     * circumstances.)
     *
     * Any code changing the locale (outside this file) should use
     * POSIX::setlocale, which calls this function.  Therefore this function
     * should be called directly only from this file and from
     * POSIX::setlocale() */

    char *save_newnum;
    dVAR;

    if (! newnum) {
	Safefree(PL_numeric_name);
	PL_numeric_name = NULL;
	PL_numeric_standard = TRUE;
	PL_numeric_local = TRUE;
	return;
    }

    save_newnum = stdize_locale(savepv(newnum));
    if (! PL_numeric_name || strNE(PL_numeric_name, save_newnum)) {
	Safefree(PL_numeric_name);
	PL_numeric_name = save_newnum;
    }

    PL_numeric_standard = ((*save_newnum == 'C' && save_newnum[1] == '\0')
                            || strEQ(save_newnum, "POSIX"));
    PL_numeric_local = TRUE;
    set_numeric_radix();

#endif /* USE_LOCALE_NUMERIC */
}

void
Perl_set_numeric_standard(pTHX)
{
#ifdef USE_LOCALE_NUMERIC
    dVAR;

    /* Toggle the LC_NUMERIC locale to C, if not already there.  Probably
     * should use the macros like SET_NUMERIC_STANDARD() in perl.h instead of
     * calling this directly. */

    if (! PL_numeric_standard) {
	setlocale(LC_NUMERIC, "C");
	PL_numeric_standard = TRUE;
	PL_numeric_local = FALSE;
	set_numeric_radix();
    }
    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Underlying LC_NUMERIC locale now is C\n"));

#endif /* USE_LOCALE_NUMERIC */
}

void
Perl_set_numeric_local(pTHX)
{
#ifdef USE_LOCALE_NUMERIC
    dVAR;

    /* Toggle the LC_NUMERIC locale to the current underlying default, if not
     * already there.  Probably should use the macros like SET_NUMERIC_LOCAL()
     * in perl.h instead of calling this directly. */

    if (! PL_numeric_local) {
	setlocale(LC_NUMERIC, PL_numeric_name);
	PL_numeric_standard = FALSE;
	PL_numeric_local = TRUE;
	set_numeric_radix();
    }
    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Underlying LC_NUMERIC locale now is %s\n",
                          PL_numeric_name));

#endif /* USE_LOCALE_NUMERIC */
}

/*
 * Set up for a new ctype locale.
 */
void
Perl_new_ctype(pTHX_ const char *newctype)
{
#ifdef USE_LOCALE_CTYPE

    /* Called after all libc setlocale() calls affecting LC_CTYPE, to tell
     * core Perl this and that 'newctype' is the name of the new locale.
     *
     * This function sets up the folding arrays for all 256 bytes, assuming
     * that tofold() is tolc() since fold case is not a concept in POSIX,
     *
     * Any code changing the locale (outside this file) should use
     * POSIX::setlocale, which calls this function.  Therefore this function
     * should be called directly only from this file and from
     * POSIX::setlocale() */

    dVAR;
    UV i;

    PERL_ARGS_ASSERT_NEW_CTYPE;

    PL_in_utf8_CTYPE_locale = is_cur_LC_category_utf8(LC_CTYPE);

    /* A UTF-8 locale gets standard rules.  But note that code still has to
     * handle this specially because of the three problematic code points */
    if (PL_in_utf8_CTYPE_locale) {
        Copy(PL_fold_latin1, PL_fold_locale, 256, U8);
    }
    else {
        for (i = 0; i < 256; i++) {
            if (isUPPER_LC((U8) i))
                PL_fold_locale[i] = (U8) toLOWER_LC((U8) i);
            else if (isLOWER_LC((U8) i))
                PL_fold_locale[i] = (U8) toUPPER_LC((U8) i);
            else
                PL_fold_locale[i] = (U8) i;
        }
    }

#endif /* USE_LOCALE_CTYPE */
    PERL_ARGS_ASSERT_NEW_CTYPE;
    PERL_UNUSED_ARG(newctype);
    PERL_UNUSED_CONTEXT;
}

void
Perl_new_collate(pTHX_ const char *newcoll)
{
#ifdef USE_LOCALE_COLLATE

    /* Called after all libc setlocale() calls affecting LC_COLLATE, to tell
     * core Perl this and that 'newcoll' is the name of the new locale.
     *
     * Any code changing the locale (outside this file) should use
     * POSIX::setlocale, which calls this function.  Therefore this function
     * should be called directly only from this file and from
     * POSIX::setlocale() */

    dVAR;

    if (! newcoll) {
	if (PL_collation_name) {
	    ++PL_collation_ix;
	    Safefree(PL_collation_name);
	    PL_collation_name = NULL;
	}
	PL_collation_standard = TRUE;
	PL_collxfrm_base = 0;
	PL_collxfrm_mult = 2;
	return;
    }

    if (! PL_collation_name || strNE(PL_collation_name, newcoll)) {
	++PL_collation_ix;
	Safefree(PL_collation_name);
	PL_collation_name = stdize_locale(savepv(newcoll));
	PL_collation_standard = ((*newcoll == 'C' && newcoll[1] == '\0')
				 || strEQ(newcoll, "POSIX"));

	{
	  /*  2: at most so many chars ('a', 'b'). */
	  /* 50: surely no system expands a char more. */
#define XFRMBUFSIZE  (2 * 50)
	  char xbuf[XFRMBUFSIZE];
	  const Size_t fa = strxfrm(xbuf, "a",  XFRMBUFSIZE);
	  const Size_t fb = strxfrm(xbuf, "ab", XFRMBUFSIZE);
	  const SSize_t mult = fb - fa;
	  if (mult < 1 && !(fa == 0 && fb == 0))
	      Perl_croak(aTHX_ "panic: strxfrm() gets absurd - a => %"UVuf", ab => %"UVuf,
			 (UV) fa, (UV) fb);
	  PL_collxfrm_base = (fa > (Size_t)mult) ? (fa - mult) : 0;
	  PL_collxfrm_mult = mult;
	}
    }

#endif /* USE_LOCALE_COLLATE */
}

#ifdef WIN32

char *
Perl_my_setlocale(pTHX_ int category, const char* locale)
{
    /* This, for Windows, emulates POSIX setlocale() behavior.  There is no
     * difference unless the input locale is "", which means on Windows to get
     * the machine default, which is set via the computer's "Regional and
     * Language Options" (or its current equivalent).  In POSIX, it instead
     * means to find the locale from the user's environment.  This routine
     * looks in the environment, and, if anything is found, uses that instead
     * of going to the machine default.  If there is no environment override,
     * the machine default is used, as normal, by calling the real setlocale()
     * with "".  The POSIX behavior is to use the LC_ALL variable if set;
     * otherwise to use the particular category's variable if set; otherwise to
     * use the LANG variable. */

    bool override_LC_ALL = FALSE;
    char * result;

    if (locale && strEQ(locale, "")) {
#   ifdef LC_ALL
        locale = PerlEnv_getenv("LC_ALL");
        if (! locale) {
#endif
            switch (category) {
#   ifdef LC_ALL
                case LC_ALL:
                    override_LC_ALL = TRUE;
                    break;  /* We already know its variable isn't set */
#   endif
#   ifdef USE_LOCALE_TIME
                case LC_TIME:
                    locale = PerlEnv_getenv("LC_TIME");
                    break;
#   endif
#   ifdef USE_LOCALE_CTYPE
                case LC_CTYPE:
                    locale = PerlEnv_getenv("LC_CTYPE");
                    break;
#   endif
#   ifdef USE_LOCALE_COLLATE
                case LC_COLLATE:
                    locale = PerlEnv_getenv("LC_COLLATE");
                    break;
#   endif
#   ifdef USE_LOCALE_MONETARY
                case LC_MONETARY:
                    locale = PerlEnv_getenv("LC_MONETARY");
                    break;
#   endif
#   ifdef USE_LOCALE_NUMERIC
                case LC_NUMERIC:
                    locale = PerlEnv_getenv("LC_NUMERIC");
                    break;
#   endif
#   ifdef USE_LOCALE_MESSAGES
                case LC_MESSAGES:
                    locale = PerlEnv_getenv("LC_MESSAGES");
                    break;
#   endif
                default:
                    /* This is a category, like PAPER_SIZE that we don't
                     * know about; and so can't provide a wrapper. */
                    break;
            }
            if (! locale) {
                locale = PerlEnv_getenv("LANG");
                if (! locale) {
                    locale = "";
                }
            }
#   ifdef LC_ALL
        }
#   endif
    }

    result = setlocale(category, locale);

    if (! override_LC_ALL)  {
        return result;
    }

    /* Here the input locale was LC_ALL, and we have set it to what is in the
     * LANG variable or the system default if there is no LANG.  But these have
     * lower priority than the other LC_foo variables, so override it for each
     * one that is set.  (If they are set to "", it means to use the same thing
     * we just set LC_ALL to, so can skip) */
#   ifdef USE_LOCALE_TIME
    result = PerlEnv_getenv("LC_TIME");
    if (result && strNE(result, "")) {
        setlocale(LC_TIME, result);
    }
#   endif
#   ifdef USE_LOCALE_CTYPE
    result = PerlEnv_getenv("LC_CTYPE");
    if (result && strNE(result, "")) {
        setlocale(LC_CTYPE, result);
    }
#   endif
#   ifdef USE_LOCALE_COLLATE
    result = PerlEnv_getenv("LC_COLLATE");
    if (result && strNE(result, "")) {
        setlocale(LC_COLLATE, result);
    }
#   endif
#   ifdef USE_LOCALE_MONETARY
    result = PerlEnv_getenv("LC_MONETARY");
    if (result && strNE(result, "")) {
        setlocale(LC_MONETARY, result);
    }
#   endif
#   ifdef USE_LOCALE_NUMERIC
    result = PerlEnv_getenv("LC_NUMERIC");
    if (result && strNE(result, "")) {
        setlocale(LC_NUMERIC, result);
    }
#   endif
#   ifdef USE_LOCALE_MESSAGES
    result = PerlEnv_getenv("LC_MESSAGES");
    if (result && strNE(result, "")) {
        setlocale(LC_MESSAGES, result);
    }
#   endif

    return setlocale(LC_ALL, NULL);

}

#endif


/*
 * Initialize locale awareness.
 */
int
Perl_init_i18nl10n(pTHX_ int printwarn)
{
    /* printwarn is
     *
     *    0 if not to output warning when setup locale is bad
     *    1 if to output warning based on value of PERL_BADLANG
     *    >1 if to output regardless of PERL_BADLANG
     *
     * returns
     *    1 = set ok or not applicable,
     *    0 = fallback to a locale of lower priority
     *   -1 = fallback to all locales failed, not even to the C locale
     */

    int ok = 1;

#if defined(USE_LOCALE)
    dVAR;

#ifdef USE_LOCALE_CTYPE
    char *curctype   = NULL;
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    char *curcoll    = NULL;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    char *curnum     = NULL;
#endif /* USE_LOCALE_NUMERIC */
#ifdef __GLIBC__
    const char * const language   = savepv(PerlEnv_getenv("LANGUAGE"));
#endif

    /* NULL uses the existing already set up locale */
    const char * const setlocale_init = (PerlEnv_getenv("PERL_SKIP_LOCALE_INIT"))
                                        ? NULL
                                        : "";
    const char* trial_locales[5];   /* 5 = 1 each for "", LC_ALL, LANG, "", C */
    unsigned int trial_locales_count;
    const char * const lc_all     = savepv(PerlEnv_getenv("LC_ALL"));
    const char * const lang       = savepv(PerlEnv_getenv("LANG"));
    bool setlocale_failure = FALSE;
    unsigned int i;
    char *p;

    /* A later getenv() could zap this, so only use here */
    const char * const bad_lang_use_once = PerlEnv_getenv("PERL_BADLANG");

    const bool locwarn = (printwarn > 1
                          || (printwarn
                              && (! bad_lang_use_once
                                  || atoi(bad_lang_use_once))));
    bool done = FALSE;
    const char *system_default_locale = NULL;


#ifndef LOCALE_ENVIRON_REQUIRED
    PERL_UNUSED_VAR(done);
#else

    /*
     * Ultrix setlocale(..., "") fails if there are no environment
     * variables from which to get a locale name.
     */

#   ifdef LC_ALL
    if (lang) {
	if (my_setlocale(LC_ALL, setlocale_init))
	    done = TRUE;
	else
	    setlocale_failure = TRUE;
    }
    if (!setlocale_failure) {
#       ifdef USE_LOCALE_CTYPE
	Safefree(curctype);
	if (! (curctype =
	       my_setlocale(LC_CTYPE,
			 (!done && (lang || PerlEnv_getenv("LC_CTYPE")))
				    ? setlocale_init : NULL)))
	    setlocale_failure = TRUE;
	else
	    curctype = savepv(curctype);
#       endif /* USE_LOCALE_CTYPE */
#       ifdef USE_LOCALE_COLLATE
	Safefree(curcoll);
	if (! (curcoll =
	       my_setlocale(LC_COLLATE,
			 (!done && (lang || PerlEnv_getenv("LC_COLLATE")))
				   ? setlocale_init : NULL)))
	    setlocale_failure = TRUE;
	else
	    curcoll = savepv(curcoll);
#       endif /* USE_LOCALE_COLLATE */
#       ifdef USE_LOCALE_NUMERIC
	Safefree(curnum);
	if (! (curnum =
	       my_setlocale(LC_NUMERIC,
			 (!done && (lang || PerlEnv_getenv("LC_NUMERIC")))
				  ? setlocale_init : NULL)))
	    setlocale_failure = TRUE;
	else
	    curnum = savepv(curnum);
#       endif /* USE_LOCALE_NUMERIC */
#       ifdef USE_LOCALE_MESSAGES
	if (! my_setlocale(LC_MESSAGES,
			 (!done && (lang || PerlEnv_getenv("LC_MESSAGES")))
				  ? setlocale_init : NULL))
        {
	    setlocale_failure = TRUE;
        }
#       endif /* USE_LOCALE_MESSAGES */
#       ifdef USE_LOCALE_MONETARY
	if (! my_setlocale(LC_MONETARY,
			 (!done && (lang || PerlEnv_getenv("LC_MONETARY")))
				  ? setlocale_init : NULL))
        {
	    setlocale_failure = TRUE;
        }
#       endif /* USE_LOCALE_MONETARY */
    }

#   endif /* LC_ALL */

#endif /* !LOCALE_ENVIRON_REQUIRED */

    /* We try each locale in the list until we get one that works, or exhaust
     * the list */
    trial_locales[0] = setlocale_init;
    trial_locales_count = 1;
    for (i= 0; i < trial_locales_count; i++) {
        const char * trial_locale = trial_locales[i];

        if (i > 0) {

            /* XXX This is to preserve old behavior for LOCALE_ENVIRON_REQUIRED
             * when i==0, but I (khw) don't think that behavior makes much
             * sense */
            setlocale_failure = FALSE;

#ifdef WIN32

            /* On Windows machines, an entry of "" after the 0th means to use
             * the system default locale, which we now proceed to get. */
            if (strEQ(trial_locale, "")) {
                unsigned int j;

                /* Note that this may change the locale, but we are going to do
                 * that anyway just below */
                system_default_locale = setlocale(LC_ALL, "");

                /* Skip if invalid or it's already on the list of locales to
                 * try */
                if (! system_default_locale) {
                    goto next_iteration;
                }
                for (j = 0; j < trial_locales_count; j++) {
                    if (strEQ(system_default_locale, trial_locales[j])) {
                        goto next_iteration;
                    }
                }

                trial_locale = system_default_locale;
            }
#endif
        }

#ifdef LC_ALL
        if (! my_setlocale(LC_ALL, trial_locale)) {
            setlocale_failure = TRUE;
        }
        else {
            /* Since LC_ALL succeeded, it should have changed all the other
             * categories it can to its value; so we massage things so that the
             * setlocales below just return their category's current values.
             * This adequately handles the case in NetBSD where LC_COLLATE may
             * not be defined for a locale, and setting it individually will
             * fail, whereas setting LC_ALL suceeds, leaving LC_COLLATE set to
             * the POSIX locale. */
            trial_locale = NULL;
        }
#endif /* LC_ALL */

        if (!setlocale_failure) {
#ifdef USE_LOCALE_CTYPE
            Safefree(curctype);
            if (! (curctype = my_setlocale(LC_CTYPE, trial_locale)))
                setlocale_failure = TRUE;
            else
                curctype = savepv(curctype);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
            Safefree(curcoll);
            if (! (curcoll = my_setlocale(LC_COLLATE, trial_locale)))
                setlocale_failure = TRUE;
            else
                curcoll = savepv(curcoll);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
            Safefree(curnum);
            if (! (curnum = my_setlocale(LC_NUMERIC, trial_locale)))
                setlocale_failure = TRUE;
            else
                curnum = savepv(curnum);
#endif /* USE_LOCALE_NUMERIC */
#ifdef USE_LOCALE_MESSAGES
            if (! (my_setlocale(LC_MESSAGES, trial_locale)))
                setlocale_failure = TRUE;
#endif /* USE_LOCALE_MESSAGES */
#ifdef USE_LOCALE_MONETARY
            if (! (my_setlocale(LC_MONETARY, trial_locale)))
                setlocale_failure = TRUE;
#endif /* USE_LOCALE_MONETARY */

            if (! setlocale_failure) {  /* Success */
                break;
            }
        }

        /* Here, something failed; will need to try a fallback. */
        ok = 0;

        if (i == 0) {
            unsigned int j;

            if (locwarn) { /* Output failure info only on the first one */
#ifdef LC_ALL

                PerlIO_printf(Perl_error_log,
                "perl: warning: Setting locale failed.\n");

#else /* !LC_ALL */

                PerlIO_printf(Perl_error_log,
                "perl: warning: Setting locale failed for the categories:\n\t");
#ifdef USE_LOCALE_CTYPE
                if (! curctype)
                    PerlIO_printf(Perl_error_log, "LC_CTYPE ");
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
                if (! curcoll)
                    PerlIO_printf(Perl_error_log, "LC_COLLATE ");
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
                if (! curnum)
                    PerlIO_printf(Perl_error_log, "LC_NUMERIC ");
#endif /* USE_LOCALE_NUMERIC */
                PerlIO_printf(Perl_error_log, "and possibly others\n");

#endif /* LC_ALL */

                PerlIO_printf(Perl_error_log,
                    "perl: warning: Please check that your locale settings:\n");

#ifdef __GLIBC__
                PerlIO_printf(Perl_error_log,
                            "\tLANGUAGE = %c%s%c,\n",
                            language ? '"' : '(',
                            language ? language : "unset",
                            language ? '"' : ')');
#endif

                PerlIO_printf(Perl_error_log,
                            "\tLC_ALL = %c%s%c,\n",
                            lc_all ? '"' : '(',
                            lc_all ? lc_all : "unset",
                            lc_all ? '"' : ')');

#if defined(USE_ENVIRON_ARRAY)
                {
                char **e;
                for (e = environ; *e; e++) {
                    if (strnEQ(*e, "LC_", 3)
                            && strnNE(*e, "LC_ALL=", 7)
                            && (p = strchr(*e, '=')))
                        PerlIO_printf(Perl_error_log, "\t%.*s = \"%s\",\n",
                                        (int)(p - *e), *e, p + 1);
                }
                }
#else
                PerlIO_printf(Perl_error_log,
                            "\t(possibly more locale environment variables)\n");
#endif

                PerlIO_printf(Perl_error_log,
                            "\tLANG = %c%s%c\n",
                            lang ? '"' : '(',
                            lang ? lang : "unset",
                            lang ? '"' : ')');

                PerlIO_printf(Perl_error_log,
                            "    are supported and installed on your system.\n");
            }

            /* Calculate what fallback locales to try.  We have avoided this
             * until we have to, becuase failure is quite unlikely.  This will
             * usually change the upper bound of the loop we are in.
             *
             * Since the system's default way of setting the locale has not
             * found one that works, We use Perl's defined ordering: LC_ALL,
             * LANG, and the C locale.  We don't try the same locale twice, so
             * don't add to the list if already there.  (On POSIX systems, the
             * LC_ALL element will likely be a repeat of the 0th element "",
             * but there's no harm done by doing it explicitly */
            if (lc_all) {
                for (j = 0; j < trial_locales_count; j++) {
                    if (strEQ(lc_all, trial_locales[j])) {
                        goto done_lc_all;
                    }
                }
                trial_locales[trial_locales_count++] = lc_all;
            }
          done_lc_all:

            if (lang) {
                for (j = 0; j < trial_locales_count; j++) {
                    if (strEQ(lang, trial_locales[j])) {
                        goto done_lang;
                    }
                }
                trial_locales[trial_locales_count++] = lang;
            }
          done_lang:

#if defined(WIN32) && defined(LC_ALL)
            /* For Windows, we also try the system default locale before "C".
             * (If there exists a Windows without LC_ALL we skip this because
             * it gets too complicated.  For those, the "C" is the next
             * fallback possibility).  The "" is the same as the 0th element of
             * the array, but the code at the loop above knows to treat it
             * differently when not the 0th */
            trial_locales[trial_locales_count++] = "";
#endif

            for (j = 0; j < trial_locales_count; j++) {
                if (strEQ("C", trial_locales[j])) {
                    goto done_C;
                }
            }
            trial_locales[trial_locales_count++] = "C";

          done_C: ;
        }   /* end of first time through the loop */

#ifdef WIN32
      next_iteration: ;
#endif

    }   /* end of looping through the trial locales */

    if (ok < 1) {   /* If we tried to fallback */
        const char* msg;
        if (! setlocale_failure) {  /* fallback succeeded */
           msg = "Falling back to";
        }
        else {  /* fallback failed */

            /* We dropped off the end of the loop, so have to decrement i to
             * get back to the value the last time through */
            i--;

            ok = -1;
            msg = "Failed to fall back to";

            /* To continue, we should use whatever values we've got */
#ifdef USE_LOCALE_CTYPE
            Safefree(curctype);
            curctype = savepv(setlocale(LC_CTYPE, NULL));
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
            Safefree(curcoll);
            curcoll = savepv(setlocale(LC_COLLATE, NULL));
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
            Safefree(curnum);
            curnum = savepv(setlocale(LC_NUMERIC, NULL));
#endif /* USE_LOCALE_NUMERIC */
        }

        if (locwarn) {
            const char * description;
            const char * name = "";
            if (strEQ(trial_locales[i], "C")) {
                description = "the standard locale";
                name = "C";
            }
            else if (strEQ(trial_locales[i], "")) {
                description = "the system default locale";
                if (system_default_locale) {
                    name = system_default_locale;
                }
            }
            else {
                description = "a fallback locale";
                name = trial_locales[i];
            }
            if (name && strNE(name, "")) {
                PerlIO_printf(Perl_error_log,
                    "perl: warning: %s %s (\"%s\").\n", msg, description, name);
            }
            else {
                PerlIO_printf(Perl_error_log,
                                   "perl: warning: %s %s.\n", msg, description);
            }
        }
    } /* End of tried to fallback */

#ifdef USE_LOCALE_CTYPE
    new_ctype(curctype);
#endif /* USE_LOCALE_CTYPE */

#ifdef USE_LOCALE_COLLATE
    new_collate(curcoll);
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC
    new_numeric(curnum);
#endif /* USE_LOCALE_NUMERIC */

#if defined(USE_PERLIO) && defined(USE_LOCALE_CTYPE)
    /* Set PL_utf8locale to TRUE if using PerlIO _and_ the current LC_CTYPE
     * locale is UTF-8.  If PL_utf8locale and PL_unicode (set by -C or by
     * $ENV{PERL_UNICODE}) are true, perl.c:S_parse_body() will turn on the
     * PerlIO :utf8 layer on STDIN, STDOUT, STDERR, _and_ the default open
     * discipline.  */
    PL_utf8locale = is_cur_LC_category_utf8(LC_CTYPE);

    /* Set PL_unicode to $ENV{PERL_UNICODE} if using PerlIO.
       This is an alternative to using the -C command line switch
       (the -C if present will override this). */
    {
	 const char *p = PerlEnv_getenv("PERL_UNICODE");
	 PL_unicode = p ? parse_unicode_opts(&p) : 0;
	 if (PL_unicode & PERL_UNICODE_UTF8CACHEASSERT_FLAG)
	     PL_utf8cache = -1;
    }
#endif

#ifdef USE_LOCALE_CTYPE
    Safefree(curctype);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    Safefree(curcoll);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    Safefree(curnum);
#endif /* USE_LOCALE_NUMERIC */

#endif /* USE_LOCALE */

#ifdef __GLIBC__
    Safefree(language);
#endif

    Safefree(lc_all);
    Safefree(lang);

    return ok;
}


#ifdef USE_LOCALE_COLLATE

/*
 * mem_collxfrm() is a bit like strxfrm() but with two important
 * differences. First, it handles embedded NULs. Second, it allocates
 * a bit more memory than needed for the transformed data itself.
 * The real transformed data begins at offset sizeof(collationix).
 * Please see sv_collxfrm() to see how this is used.
 */

char *
Perl_mem_collxfrm(pTHX_ const char *s, STRLEN len, STRLEN *xlen)
{
    dVAR;
    char *xbuf;
    STRLEN xAlloc, xin, xout; /* xalloc is a reserved word in VC */

    PERL_ARGS_ASSERT_MEM_COLLXFRM;

    /* the first sizeof(collationix) bytes are used by sv_collxfrm(). */
    /* the +1 is for the terminating NUL. */

    xAlloc = sizeof(PL_collation_ix) + PL_collxfrm_base + (PL_collxfrm_mult * len) + 1;
    Newx(xbuf, xAlloc, char);
    if (! xbuf)
	goto bad;

    *(U32*)xbuf = PL_collation_ix;
    xout = sizeof(PL_collation_ix);
    for (xin = 0; xin < len; ) {
	Size_t xused;

	for (;;) {
	    xused = strxfrm(xbuf + xout, s + xin, xAlloc - xout);
	    if (xused >= PERL_INT_MAX)
		goto bad;
	    if ((STRLEN)xused < xAlloc - xout)
		break;
	    xAlloc = (2 * xAlloc) + 1;
	    Renew(xbuf, xAlloc, char);
	    if (! xbuf)
		goto bad;
	}

	xin += strlen(s + xin) + 1;
	xout += xused;

	/* Embedded NULs are understood but silently skipped
	 * because they make no sense in locale collation. */
    }

    xbuf[xout] = '\0';
    *xlen = xout - sizeof(PL_collation_ix);
    return xbuf;

  bad:
    Safefree(xbuf);
    *xlen = 0;
    return NULL;
}

#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE

STATIC bool
S_is_cur_LC_category_utf8(pTHX_ int category)
{
    /* Returns TRUE if the current locale for 'category' is UTF-8; FALSE
     * otherwise. 'category' may not be LC_ALL.  If the platform doesn't have
     * nl_langinfo(), nor MB_CUR_MAX, this employs a heuristic, which hence
     * could give the wrong result.  It errs on the side of not being a UTF-8
     * locale. */

    char *save_input_locale = NULL;
    STRLEN final_pos;

#ifdef LC_ALL
    assert(category != LC_ALL);
#endif

    /* First dispose of the trivial cases */
    save_input_locale = setlocale(category, NULL);
    if (! save_input_locale) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                              "Could not find current locale for category %d\n",
                              category));
        return FALSE;   /* XXX maybe should croak */
    }
    save_input_locale = stdize_locale(savepv(save_input_locale));
    if ((*save_input_locale == 'C' && save_input_locale[1] == '\0')
        || strEQ(save_input_locale, "POSIX"))
    {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                              "Current locale for category %d is %s\n",
                              category, save_input_locale));
        Safefree(save_input_locale);
        return FALSE;
    }

#if defined(USE_LOCALE_CTYPE)    \
    && (defined(MB_CUR_MAX) || (defined(HAS_NL_LANGINFO) && defined(CODESET)))

    { /* Next try nl_langinfo or MB_CUR_MAX if available */

        char *save_ctype_locale = NULL;
        bool is_utf8;

        if (category != LC_CTYPE) { /* These work only on LC_CTYPE */

            /* Get the current LC_CTYPE locale */
            save_ctype_locale = stdize_locale(savepv(setlocale(LC_CTYPE, NULL)));
            if (! save_ctype_locale) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                               "Could not find current locale for LC_CTYPE\n"));
                goto cant_use_nllanginfo;
            }

            /* If LC_CTYPE and the desired category use the same locale, this
             * means that finding the value for LC_CTYPE is the same as finding
             * the value for the desired category.  Otherwise, switch LC_CTYPE
             * to the desired category's locale */
            if (strEQ(save_ctype_locale, save_input_locale)) {
                Safefree(save_ctype_locale);
                save_ctype_locale = NULL;
            }
            else if (! setlocale(LC_CTYPE, save_input_locale)) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                                    "Could not change LC_CTYPE locale to %s\n",
                                    save_input_locale));
                Safefree(save_ctype_locale);
                goto cant_use_nllanginfo;
            }
        }

        DEBUG_L(PerlIO_printf(Perl_debug_log, "Current LC_CTYPE locale=%s\n",
                                              save_input_locale));

        /* Here the current LC_CTYPE is set to the locale of the category whose
         * information is desired.  This means that nl_langinfo() and MB_CUR_MAX
         * should give the correct results */

#   if defined(HAS_NL_LANGINFO) && defined(CODESET)
        {
            char *codeset = savepv(nl_langinfo(CODESET));
            if (codeset && strNE(codeset, "")) {

                /* If we switched LC_CTYPE, switch back */
                if (save_ctype_locale) {
                    setlocale(LC_CTYPE, save_ctype_locale);
                    Safefree(save_ctype_locale);
                }

                is_utf8 = foldEQ(codeset, STR_WITH_LEN("UTF-8"))
                        || foldEQ(codeset, STR_WITH_LEN("UTF8"));

                DEBUG_L(PerlIO_printf(Perl_debug_log,
                       "\tnllanginfo returned CODESET '%s'; ?UTF8 locale=%d\n",
                                                     codeset,         is_utf8));
                Safefree(codeset);
                Safefree(save_input_locale);
                return is_utf8;
            }
            Safefree(codeset);
        }

#   endif
#   ifdef MB_CUR_MAX

        /* Here, either we don't have nl_langinfo, or it didn't return a
         * codeset.  Try MB_CUR_MAX */

        /* Standard UTF-8 needs at least 4 bytes to represent the maximum
         * Unicode code point.  Since UTF-8 is the only non-single byte
         * encoding we handle, we just say any such encoding is UTF-8, and if
         * turns out to be wrong, other things will fail */
        is_utf8 = MB_CUR_MAX >= 4;

        DEBUG_L(PerlIO_printf(Perl_debug_log,
                              "\tMB_CUR_MAX=%d; ?UTF8 locale=%d\n",
                                   (int) MB_CUR_MAX,      is_utf8));

        Safefree(save_input_locale);

#       ifdef HAS_MBTOWC

        /* ... But, most system that have MB_CUR_MAX will also have mbtowc(),
         * since they are both in the C99 standard.  We can feed a known byte
         * string to the latter function, and check that it gives the expected
         * result */
        if (is_utf8) {
            wchar_t wc;
            PERL_UNUSED_RESULT(mbtowc(&wc, NULL, 0));/* Reset any shift state */
            errno = 0;
            if (mbtowc(&wc, HYPHEN_UTF8, strlen(HYPHEN_UTF8))
                                                        != strlen(HYPHEN_UTF8)
                || wc != (wchar_t) 0x2010)
            {
                is_utf8 = FALSE;
                DEBUG_L(PerlIO_printf(Perl_debug_log, "\thyphen=U+%x\n", wc));
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                        "\treturn from mbtowc=%d; errno=%d; ?UTF8 locale=0\n",
                        mbtowc(&wc, HYPHEN_UTF8, strlen(HYPHEN_UTF8)), errno));
            }
        }
#       endif

        /* If we switched LC_CTYPE, switch back */
        if (save_ctype_locale) {
            setlocale(LC_CTYPE, save_ctype_locale);
            Safefree(save_ctype_locale);
        }

        return is_utf8;
#   endif
    }

  cant_use_nllanginfo:

#endif /* HAS_NL_LANGINFO etc */

    /* nl_langinfo not available or failed somehow.  Look at the locale name to
     * see if it matches qr/UTF -? 8 /ix  */

    final_pos = strlen(save_input_locale) - 1;
    if (final_pos >= 3) {
        char *name = save_input_locale;

        /* Find next 'U' or 'u' and look from there */
        while ((name += strcspn(name, "Uu") + 1)
                                            <= save_input_locale + final_pos - 2)
        {
            if (toFOLD(*(name)) != 't'
                || toFOLD(*(name + 1)) != 'f')
            {
                continue;
            }
            name += 2;
            if (*(name) == '-') {
                if ((name > save_input_locale + final_pos - 1)) {
                    break;
                }
                name++;
            }
            if (*(name) == '8') {
                Safefree(save_input_locale);
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                                      "Locale %s ends with UTF-8 in name\n",
                                      save_input_locale));
                return TRUE;
            }
        }
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                              "Locale %s doesn't end with UTF-8 in name\n",
                                save_input_locale));
    }

#ifdef WIN32
    /* http://msdn.microsoft.com/en-us/library/windows/desktop/dd317756.aspx */
    if (final_pos >= 4
        && *(save_input_locale + final_pos - 0) == '1'
        && *(save_input_locale + final_pos - 1) == '0'
        && *(save_input_locale + final_pos - 2) == '0'
        && *(save_input_locale + final_pos - 3) == '5'
        && *(save_input_locale + final_pos - 4) == '6')
    {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                        "Locale %s ends with 10056 in name, is UTF-8 locale\n",
                        save_input_locale));
        Safefree(save_input_locale);
        return TRUE;
    }
#endif

    /* Other common encodings are the ISO 8859 series, which aren't UTF-8 */
    if (instr(save_input_locale, "8859")) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                             "Locale %s has 8859 in name, not UTF-8 locale\n",
                             save_input_locale));
        Safefree(save_input_locale);
        return FALSE;
    }

#ifdef HAS_LOCALECONV

#   ifdef USE_LOCALE_MONETARY

    /* Here, there is nothing in the locale name to indicate whether the locale
     * is UTF-8 or not.  This "name", the return of setlocale(), is actually
     * defined to be opaque, so we can't really rely on the absence of various
     * substrings in the name to indicate its UTF-8ness.  Look at the locale's
     * currency symbol.  Often that will be in the native script, and if the
     * symbol isn't in UTF-8, we know that the locale isn't.  If it is
     * non-ASCII UTF-8, we infer that the locale is too.
     * To do this, like above for LC_CTYPE, we first set LC_MONETARY to the
     * locale of the desired category, if it isn't that locale already */

    {
        char *save_monetary_locale = NULL;
        bool illegal_utf8 = FALSE;
        bool only_ascii = FALSE;
        const struct lconv* const lc = localeconv();

        if (category != LC_MONETARY) {

            save_monetary_locale = stdize_locale(savepv(setlocale(LC_MONETARY,
                                                                  NULL)));
            if (! save_monetary_locale) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "Could not find current locale for LC_MONETARY\n"));
                goto cant_use_monetary;
            }

            if (strNE(save_monetary_locale, save_input_locale)) {
                if (! setlocale(LC_MONETARY, save_input_locale)) {
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                "Could not change LC_MONETARY locale to %s\n",
                                                            save_input_locale));
                    Safefree(save_monetary_locale);
                    goto cant_use_monetary;
                }
            }
        }

        /* Here the current LC_MONETARY is set to the locale of the category
         * whose information is desired. */

        if (lc && lc->currency_symbol) {
            if (! is_utf8_string((U8 *) lc->currency_symbol, 0)) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "Currency symbol for %s is not legal UTF-8\n",
                                        save_input_locale));
                illegal_utf8 = TRUE;
            }
            else if (is_ascii_string((U8 *) lc->currency_symbol, 0)) {
                DEBUG_L(PerlIO_printf(Perl_debug_log, "Currency symbol for %s contains only ASCII; can't use for determining if UTF-8 locale\n", save_input_locale));
                only_ascii = TRUE;
            }
        }

        /* If we changed it, restore LC_MONETARY to its original locale */
        if (save_monetary_locale) {
            setlocale(LC_MONETARY, save_monetary_locale);
            Safefree(save_monetary_locale);
        }

        Safefree(save_input_locale);

        /* It isn't a UTF-8 locale if the symbol is not legal UTF-8; otherwise
         * assume the locale is UTF-8 if and only if the symbol is non-ascii
         * UTF-8.  (We can't really tell if the locale is UTF-8 or not if the
         * symbol is just a '$', so we err on the side of it not being UTF-8)
         * */
        DEBUG_L(PerlIO_printf(Perl_debug_log, "\tis_utf8=%d\n", (illegal_utf8)
                                                               ? FALSE
                                                               : ! only_ascii));
        return (illegal_utf8)
                ? FALSE
                : ! only_ascii;

    }
  cant_use_monetary:

#   endif /* USE_LOCALE_MONETARY */
#endif /* HAS_LOCALECONV */

#if 0 && defined(HAS_STRERROR) && defined(USE_LOCALE_MESSAGES)

/* This code is ifdefd out because it was found to not be necessary in testing
 * on our dromedary test machine, which has over 700 locales.  There, looking
 * at just the currency symbol gave essentially the same results as doing this
 * extra work.  Executing this also caused segfaults in miniperl.  I left it in
 * so as to avoid rewriting it if real-world experience indicates that
 * dromedary is an outlier.  Essentially, instead of returning abpve if we
 * haven't found illegal utf8, we continue on and examine all the strerror()
 * messages on the platform for utf8ness.  If all are ASCII, we still don't
 * know the answer; but otherwise we have a pretty good indication of the
 * utf8ness.  The reason this doesn't necessarily help much is that the
 * messages may not have been translated into the locale.  The currency symbol
 * is much more likely to have been translated.  The code below would need to
 * be altered somewhat to just be a continuation of testing the currency
 * symbol. */
        int e;
        unsigned int failures = 0, non_ascii = 0;
        char *save_messages_locale = NULL;

        /* Like above for LC_CTYPE, we set LC_MESSAGES to the locale of the
         * desired category, if it isn't that locale already */

        if (category != LC_MESSAGES) {

            save_messages_locale = stdize_locale(savepv(setlocale(LC_MESSAGES,
                                                                  NULL)));
            if (! save_messages_locale) {
                goto cant_use_messages;
            }

            if (strEQ(save_messages_locale, save_input_locale)) {
                Safefree(save_input_locale);
            }
            else if (! setlocale(LC_MESSAGES, save_input_locale)) {
                Safefree(save_messages_locale);
                goto cant_use_messages;
            }
        }

        /* Here the current LC_MESSAGES is set to the locale of the category
         * whose information is desired.  Look through all the messages */

        for (e = 0;
#ifdef HAS_SYS_ERRLIST
             e <= sys_nerr
#endif
             ; e++)
        {
            const U8* const errmsg = (U8 *) Strerror(e) ;
            if (!errmsg)
                break;
            if (! is_utf8_string(errmsg, 0)) {
                failures++;
                break;
            }
            else if (! is_ascii_string(errmsg, 0)) {
                non_ascii++;
            }
        }

        /* And, if we changed it, restore LC_MESSAGES to its original locale */
        if (save_messages_locale) {
            setlocale(LC_MESSAGES, save_messages_locale);
            Safefree(save_messages_locale);
        }

        /* Any non-UTF-8 message means not a UTF-8 locale; if all are valid,
         * any non-ascii means it is one; otherwise we assume it isn't */
        return (failures) ? FALSE : non_ascii;

    }
  cant_use_messages:

#endif

    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Assuming locale %s is not a UTF-8 locale\n",
                                    save_input_locale));
    Safefree(save_input_locale);
    return FALSE;
}

#endif

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
