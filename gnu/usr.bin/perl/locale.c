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
 * All C programs have an underlying locale.  Perl code generally doesn't pay
 * any attention to it except within the scope of a 'use locale'.  For most
 * categories, it accomplishes this by just using different operations if it is
 * in such scope than if not.  However, various libc functions called by Perl
 * are affected by the LC_NUMERIC category, so there are macros in perl.h that
 * are used to toggle between the current locale and the C locale depending on
 * the desired behavior of those functions at the moment.  And, LC_MESSAGES is
 * switched to the C locale for outputting the message unless within the scope
 * of 'use locale'.
 *
 * This code now has multi-thread-safe locale handling on systems that support
 * that.  This is completely transparent to most XS code.  On earlier systems,
 * it would be possible to emulate thread-safe locales, but this likely would
 * involve a lot of locale switching, and would require XS code changes.
 * Macros could be written so that the code wouldn't have to know which type of
 * system is being used.  It's unlikely that we would ever do that, since most
 * modern systems support thread-safe locales, but there was code written to
 * this end, and is retained, #ifdef'd out.
 */

#include "EXTERN.h"
#define PERL_IN_LOCALE_C
#include "perl_langinfo.h"
#include "perl.h"

#include "reentr.h"

#ifdef I_WCHAR
#  include <wchar.h>
#endif
#ifdef I_WCTYPE
#  include <wctype.h>
#endif

/* If the environment says to, we can output debugging information during
 * initialization.  This is done before option parsing, and before any thread
 * creation, so can be a file-level static */
#if ! defined(DEBUGGING) || defined(PERL_GLOBAL_STRUCT)
#  define debug_initialization 0
#  define DEBUG_INITIALIZATION_set(v)
#else
static bool debug_initialization = FALSE;
#  define DEBUG_INITIALIZATION_set(v) (debug_initialization = v)
#endif


/* Returns the Unix errno portion; ignoring any others.  This is a macro here
 * instead of putting it into perl.h, because unclear to khw what should be
 * done generally. */
#define GET_ERRNO   saved_errno

/* strlen() of a literal string constant.  We might want this more general,
 * but using it in just this file for now.  A problem with more generality is
 * the compiler warnings about comparing unlike signs */
#define STRLENs(s)  (sizeof("" s "") - 1)

/* Is the C string input 'name' "C" or "POSIX"?  If so, and 'name' is the
 * return of setlocale(), then this is extremely likely to be the C or POSIX
 * locale.  However, the output of setlocale() is documented to be opaque, but
 * the odds are extremely small that it would return these two strings for some
 * other locale.  Note that VMS in these two locales includes many non-ASCII
 * characters as controls and punctuation (below are hex bytes):
 *   cntrl:  84-97 9B-9F
 *   punct:  A1-A3 A5 A7-AB B0-B3 B5-B7 B9-BD BF-CF D1-DD DF-EF F1-FD
 * Oddly, none there are listed as alphas, though some represent alphabetics
 * http://www.nntp.perl.org/group/perl.perl5.porters/2013/02/msg198753.html */
#define isNAME_C_OR_POSIX(name)                                              \
                             (   (name) != NULL                              \
                              && (( *(name) == 'C' && (*(name + 1)) == '\0') \
                                   || strEQ((name), "POSIX")))

#ifdef USE_LOCALE

/* This code keeps a LRU cache of the UTF-8ness of the locales it has so-far
 * looked up.  This is in the form of a C string:  */

#define UTF8NESS_SEP     "\v"
#define UTF8NESS_PREFIX  "\f"

/* So, the string looks like:
 *
 *      \vC\a0\vPOSIX\a0\vam_ET\a0\vaf_ZA.utf8\a1\ven_US.UTF-8\a1\0
 *
 * where the digit 0 after the \a indicates that the locale starting just
 * after the preceding \v is not UTF-8, and the digit 1 mean it is. */

STATIC_ASSERT_DECL(STRLENs(UTF8NESS_SEP) == 1);
STATIC_ASSERT_DECL(STRLENs(UTF8NESS_PREFIX) == 1);

#define C_and_POSIX_utf8ness    UTF8NESS_SEP "C"     UTF8NESS_PREFIX "0"    \
                                UTF8NESS_SEP "POSIX" UTF8NESS_PREFIX "0"

/* The cache is initialized to C_and_POSIX_utf8ness at start up.  These are
 * kept there always.  The remining portion of the cache is LRU, with the
 * oldest looked-up locale at the tail end */

STATIC char *
S_stdize_locale(pTHX_ char *locs)
{
    /* Standardize the locale name from a string returned by 'setlocale',
     * possibly modifying that string.
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
     * */

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

/* Two parallel arrays; first the locale categories Perl uses on this system;
 * the second array is their names.  These arrays are in mostly arbitrary
 * order. */

const int categories[] = {

#    ifdef USE_LOCALE_NUMERIC
                             LC_NUMERIC,
#    endif
#    ifdef USE_LOCALE_CTYPE
                             LC_CTYPE,
#    endif
#    ifdef USE_LOCALE_COLLATE
                             LC_COLLATE,
#    endif
#    ifdef USE_LOCALE_TIME
                             LC_TIME,
#    endif
#    ifdef USE_LOCALE_MESSAGES
                             LC_MESSAGES,
#    endif
#    ifdef USE_LOCALE_MONETARY
                             LC_MONETARY,
#    endif
#    ifdef USE_LOCALE_ADDRESS
                             LC_ADDRESS,
#    endif
#    ifdef USE_LOCALE_IDENTIFICATION
                             LC_IDENTIFICATION,
#    endif
#    ifdef USE_LOCALE_MEASUREMENT
                             LC_MEASUREMENT,
#    endif
#    ifdef USE_LOCALE_PAPER
                             LC_PAPER,
#    endif
#    ifdef USE_LOCALE_TELEPHONE
                             LC_TELEPHONE,
#    endif
#    ifdef LC_ALL
                             LC_ALL,
#    endif
                            -1  /* Placeholder because C doesn't allow a
                                   trailing comma, and it would get complicated
                                   with all the #ifdef's */
};

/* The top-most real element is LC_ALL */

const char * const category_names[] = {

#    ifdef USE_LOCALE_NUMERIC
                                 "LC_NUMERIC",
#    endif
#    ifdef USE_LOCALE_CTYPE
                                 "LC_CTYPE",
#    endif
#    ifdef USE_LOCALE_COLLATE
                                 "LC_COLLATE",
#    endif
#    ifdef USE_LOCALE_TIME
                                 "LC_TIME",
#    endif
#    ifdef USE_LOCALE_MESSAGES
                                 "LC_MESSAGES",
#    endif
#    ifdef USE_LOCALE_MONETARY
                                 "LC_MONETARY",
#    endif
#    ifdef USE_LOCALE_ADDRESS
                                 "LC_ADDRESS",
#    endif
#    ifdef USE_LOCALE_IDENTIFICATION
                                 "LC_IDENTIFICATION",
#    endif
#    ifdef USE_LOCALE_MEASUREMENT
                                 "LC_MEASUREMENT",
#    endif
#    ifdef USE_LOCALE_PAPER
                                 "LC_PAPER",
#    endif
#    ifdef USE_LOCALE_TELEPHONE
                                 "LC_TELEPHONE",
#    endif
#    ifdef LC_ALL
                                 "LC_ALL",
#    endif
                                 NULL  /* Placeholder */
                            };

#  ifdef LC_ALL

    /* On systems with LC_ALL, it is kept in the highest index position.  (-2
     * to account for the final unused placeholder element.) */
#    define NOMINAL_LC_ALL_INDEX (C_ARRAY_LENGTH(categories) - 2)

#  else

    /* On systems without LC_ALL, we pretend it is there, one beyond the real
     * top element, hence in the unused placeholder element. */
#    define NOMINAL_LC_ALL_INDEX (C_ARRAY_LENGTH(categories) - 1)

#  endif

/* Pretending there is an LC_ALL element just above allows us to avoid most
 * special cases.  Most loops through these arrays in the code below are
 * written like 'for (i = 0; i < NOMINAL_LC_ALL_INDEX; i++)'.  They will work
 * on either type of system.  But the code must be written to not access the
 * element at 'LC_ALL_INDEX' except on platforms that have it.  This can be
 * checked for at compile time by using the #define LC_ALL_INDEX which is only
 * defined if we do have LC_ALL. */

STATIC const char *
S_category_name(const int category)
{
    unsigned int i;

#ifdef LC_ALL

    if (category == LC_ALL) {
        return "LC_ALL";
    }

#endif

    for (i = 0; i < NOMINAL_LC_ALL_INDEX; i++) {
        if (category == categories[i]) {
            return category_names[i];
        }
    }

    {
        const char suffix[] = " (unknown)";
        int temp = category;
        Size_t length = sizeof(suffix) + 1;
        char * unknown;
        dTHX;

        if (temp < 0) {
            length++;
            temp = - temp;
        }

        /* Calculate the number of digits */
        while (temp >= 10) {
            temp /= 10;
            length++;
        }

        Newx(unknown, length, char);
        my_snprintf(unknown, length, "%d%s", category, suffix);
        SAVEFREEPV(unknown);
        return unknown;
    }
}

/* Now create LC_foo_INDEX #defines for just those categories on this system */
#  ifdef USE_LOCALE_NUMERIC
#    define LC_NUMERIC_INDEX            0
#    define _DUMMY_NUMERIC              LC_NUMERIC_INDEX
#  else
#    define _DUMMY_NUMERIC              -1
#  endif
#  ifdef USE_LOCALE_CTYPE
#    define LC_CTYPE_INDEX              _DUMMY_NUMERIC + 1
#    define _DUMMY_CTYPE                LC_CTYPE_INDEX
#  else
#    define _DUMMY_CTYPE                _DUMMY_NUMERIC
#  endif
#  ifdef USE_LOCALE_COLLATE
#    define LC_COLLATE_INDEX            _DUMMY_CTYPE + 1
#    define _DUMMY_COLLATE              LC_COLLATE_INDEX
#  else
#    define _DUMMY_COLLATE              _DUMMY_CTYPE
#  endif
#  ifdef USE_LOCALE_TIME
#    define LC_TIME_INDEX               _DUMMY_COLLATE + 1
#    define _DUMMY_TIME                 LC_TIME_INDEX
#  else
#    define _DUMMY_TIME                 _DUMMY_COLLATE
#  endif
#  ifdef USE_LOCALE_MESSAGES
#    define LC_MESSAGES_INDEX           _DUMMY_TIME + 1
#    define _DUMMY_MESSAGES             LC_MESSAGES_INDEX
#  else
#    define _DUMMY_MESSAGES             _DUMMY_TIME
#  endif
#  ifdef USE_LOCALE_MONETARY
#    define LC_MONETARY_INDEX           _DUMMY_MESSAGES + 1
#    define _DUMMY_MONETARY             LC_MONETARY_INDEX
#  else
#    define _DUMMY_MONETARY             _DUMMY_MESSAGES
#  endif
#  ifdef USE_LOCALE_ADDRESS
#    define LC_ADDRESS_INDEX            _DUMMY_MONETARY + 1
#    define _DUMMY_ADDRESS              LC_ADDRESS_INDEX
#  else
#    define _DUMMY_ADDRESS              _DUMMY_MONETARY
#  endif
#  ifdef USE_LOCALE_IDENTIFICATION
#    define LC_IDENTIFICATION_INDEX     _DUMMY_ADDRESS + 1
#    define _DUMMY_IDENTIFICATION       LC_IDENTIFICATION_INDEX
#  else
#    define _DUMMY_IDENTIFICATION       _DUMMY_ADDRESS
#  endif
#  ifdef USE_LOCALE_MEASUREMENT
#    define LC_MEASUREMENT_INDEX        _DUMMY_IDENTIFICATION + 1
#    define _DUMMY_MEASUREMENT          LC_MEASUREMENT_INDEX
#  else
#    define _DUMMY_MEASUREMENT          _DUMMY_IDENTIFICATION
#  endif
#  ifdef USE_LOCALE_PAPER
#    define LC_PAPER_INDEX              _DUMMY_MEASUREMENT + 1
#    define _DUMMY_PAPER                LC_PAPER_INDEX
#  else
#    define _DUMMY_PAPER                _DUMMY_MEASUREMENT
#  endif
#  ifdef USE_LOCALE_TELEPHONE
#    define LC_TELEPHONE_INDEX          _DUMMY_PAPER + 1
#    define _DUMMY_TELEPHONE            LC_TELEPHONE_INDEX
#  else
#    define _DUMMY_TELEPHONE            _DUMMY_PAPER
#  endif
#  ifdef LC_ALL
#    define LC_ALL_INDEX                _DUMMY_TELEPHONE + 1
#  endif
#endif /* ifdef USE_LOCALE */

/* Windows requres a customized base-level setlocale() */
#ifdef WIN32
#  define my_setlocale(cat, locale) win32_setlocale(cat, locale)
#else
#  define my_setlocale(cat, locale) setlocale(cat, locale)
#endif

#ifndef USE_POSIX_2008_LOCALE

/* "do_setlocale_c" is intended to be called when the category is a constant
 * known at compile time; "do_setlocale_r", not known until run time  */
#  define do_setlocale_c(cat, locale) my_setlocale(cat, locale)
#  define do_setlocale_r(cat, locale) my_setlocale(cat, locale)

#else   /* Below uses POSIX 2008 */

/* We emulate setlocale with our own function.  LC_foo is not valid for the
 * POSIX 2008 functions.  Instead LC_foo_MASK is used, which we use an array
 * lookup to convert to.  At compile time we have defined LC_foo_INDEX as the
 * proper offset into the array 'category_masks[]'.  At runtime, we have to
 * search through the array (as the actual numbers may not be small contiguous
 * positive integers which would lend themselves to array lookup). */
#  define do_setlocale_c(cat, locale)                                       \
                        emulate_setlocale(cat, locale, cat ## _INDEX, TRUE)
#  define do_setlocale_r(cat, locale) emulate_setlocale(cat, locale, 0, FALSE)

/* A third array, parallel to the ones above to map from category to its
 * equivalent mask */
const int category_masks[] = {
#  ifdef USE_LOCALE_NUMERIC
                                LC_NUMERIC_MASK,
#  endif
#  ifdef USE_LOCALE_CTYPE
                                LC_CTYPE_MASK,
#  endif
#  ifdef USE_LOCALE_COLLATE
                                LC_COLLATE_MASK,
#  endif
#  ifdef USE_LOCALE_TIME
                                LC_TIME_MASK,
#  endif
#  ifdef USE_LOCALE_MESSAGES
                                LC_MESSAGES_MASK,
#  endif
#  ifdef USE_LOCALE_MONETARY
                                LC_MONETARY_MASK,
#  endif
#  ifdef USE_LOCALE_ADDRESS
                                LC_ADDRESS_MASK,
#  endif
#  ifdef USE_LOCALE_IDENTIFICATION
                                LC_IDENTIFICATION_MASK,
#  endif
#  ifdef USE_LOCALE_MEASUREMENT
                                LC_MEASUREMENT_MASK,
#  endif
#  ifdef USE_LOCALE_PAPER
                                LC_PAPER_MASK,
#  endif
#  ifdef USE_LOCALE_TELEPHONE
                                LC_TELEPHONE_MASK,
#  endif
                                /* LC_ALL can't be turned off by a Configure
                                 * option, and in Posix 2008, should always be
                                 * here, so compile it in unconditionally.
                                 * This could catch some glitches at compile
                                 * time */
                                LC_ALL_MASK
                            };

STATIC const char *
S_emulate_setlocale(const int category,
                    const char * locale,
                    unsigned int index,
                    const bool is_index_valid
                   )
{
    /* This function effectively performs a setlocale() on just the current
     * thread; thus it is thread-safe.  It does this by using the POSIX 2008
     * locale functions to emulate the behavior of setlocale().  Similar to
     * regular setlocale(), the return from this function points to memory that
     * can be overwritten by other system calls, so needs to be copied
     * immediately if you need to retain it.  The difference here is that
     * system calls besides another setlocale() can overwrite it.
     *
     * By doing this, most locale-sensitive functions become thread-safe.  The
     * exceptions are mostly those that return a pointer to static memory.
     *
     * This function takes the same parameters, 'category' and 'locale', that
     * the regular setlocale() function does, but it also takes two additional
     * ones.  This is because the 2008 functions don't use a category; instead
     * they use a corresponding mask.  Because this function operates in both
     * worlds, it may need one or the other or both.  This function can
     * calculate the mask from the input category, but to avoid this
     * calculation, if the caller knows at compile time what the mask is, it
     * can pass it, setting 'is_index_valid' to TRUE; otherwise the mask
     * parameter is ignored.
     *
     * POSIX 2008, for some sick reason, chose not to provide a method to find
     * the category name of a locale.  Some vendors have created a
     * querylocale() function to do just that.  This function is a lot simpler
     * to implement on systems that have this.  Otherwise, we have to keep
     * track of what the locale has been set to, so that we can return its
     * name to emulate setlocale().  It's also possible for C code in some
     * library to change the locale without us knowing it, though as of
     * September 2017, there are no occurrences in CPAN of uselocale().  Some
     * libraries do use setlocale(), but that changes the global locale, and
     * threads using per-thread locales will just ignore those changes.
     * Another problem is that without querylocale(), we have to guess at what
     * was meant by setting a locale of "".  We handle this by not actually
     * ever setting to "" (unless querylocale exists), but to emulate what we
     * think should happen for "".
     */

    int mask;
    locale_t old_obj;
    locale_t new_obj;
    dTHX;

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale input=%d (%s), \"%s\", %d, %d\n", __FILE__, __LINE__, category, category_name(category), locale, index, is_index_valid);
    }

#  endif

    /* If the input mask might be incorrect, calculate the correct one */
    if (! is_index_valid) {
        unsigned int i;

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "%s:%d: finding index of category %d (%s)\n", __FILE__, __LINE__, category, category_name(category));
        }

#  endif

        for (i = 0; i <= LC_ALL_INDEX; i++) {
            if (category == categories[i]) {
                index = i;
                goto found_index;
            }
        }

        /* Here, we don't know about this category, so can't handle it.
         * Fallback to the early POSIX usages */
        Perl_warner(aTHX_ packWARN(WARN_LOCALE),
                            "Unknown locale category %d; can't set it to %s\n",
                                                     category, locale);
        return NULL;

      found_index: ;

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "%s:%d: index is %d for %s\n", __FILE__, __LINE__, index, category_name(category));
        }

#  endif

    }

    mask = category_masks[index];

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "%s:%d: category name is %s; mask is 0x%x\n", __FILE__, __LINE__, category_names[index], mask);
    }

#  endif

    /* If just querying what the existing locale is ... */
    if (locale == NULL) {
        locale_t cur_obj = uselocale((locale_t) 0);

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale querying %p\n", __FILE__, __LINE__, cur_obj);
        }

#  endif

        if (cur_obj == LC_GLOBAL_LOCALE) {
            return my_setlocale(category, NULL);
        }

#  ifdef HAS_QUERYLOCALE

        return (char *) querylocale(mask, cur_obj);

#  else

        /* If this assert fails, adjust the size of curlocales in intrpvar.h */
        STATIC_ASSERT_STMT(C_ARRAY_LENGTH(PL_curlocales) > LC_ALL_INDEX);

#    if   defined(_NL_LOCALE_NAME)                      \
     &&   defined(DEBUGGING)                            \
     && ! defined(SETLOCALE_ACCEPTS_ANY_LOCALE_NAME)
          /* On systems that accept any locale name, the real underlying locale
           * is often returned by this internal function, so we can't use it */
        {
            /* Internal glibc for querylocale(), but doesn't handle
             * empty-string ("") locale properly; who knows what other
             * glitches.  Check for it now, under debug. */

            char * temp_name = nl_langinfo_l(_NL_LOCALE_NAME(category),
                                             uselocale((locale_t) 0));
            /*
            PerlIO_printf(Perl_debug_log, "%s:%d: temp_name=%s\n", __FILE__, __LINE__, temp_name ? temp_name : "NULL");
            PerlIO_printf(Perl_debug_log, "%s:%d: index=%d\n", __FILE__, __LINE__, index);
            PerlIO_printf(Perl_debug_log, "%s:%d: PL_curlocales[index]=%s\n", __FILE__, __LINE__, PL_curlocales[index]);
            */
            if (temp_name && PL_curlocales[index] && strNE(temp_name, "")) {
                if (         strNE(PL_curlocales[index], temp_name)
                    && ! (   isNAME_C_OR_POSIX(temp_name)
                          && isNAME_C_OR_POSIX(PL_curlocales[index]))) {

#      ifdef USE_C_BACKTRACE

                    dump_c_backtrace(Perl_debug_log, 20, 1);

#      endif

                    Perl_croak(aTHX_ "panic: Mismatch between what Perl thinks %s is"
                                     " (%s) and what internal glibc thinks"
                                     " (%s)\n", category_names[index],
                                     PL_curlocales[index], temp_name);
                }

                return temp_name;
            }
        }

#    endif

        /* Without querylocale(), we have to use our record-keeping we've
         *  done. */

        if (category != LC_ALL) {

#    ifdef DEBUGGING

            if (DEBUG_Lv_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale returning %s\n", __FILE__, __LINE__, PL_curlocales[index]);
            }

#    endif

            return PL_curlocales[index];
        }
        else {  /* For LC_ALL */
            unsigned int i;
            Size_t names_len = 0;
            char * all_string;
            bool are_all_categories_the_same_locale = TRUE;

            /* If we have a valid LC_ALL value, just return it */
            if (PL_curlocales[LC_ALL_INDEX]) {

#    ifdef DEBUGGING

                if (DEBUG_Lv_TEST || debug_initialization) {
                    PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale returning %s\n", __FILE__, __LINE__, PL_curlocales[LC_ALL_INDEX]);
                }

#    endif

                return PL_curlocales[LC_ALL_INDEX];
            }

            /* Otherwise, we need to construct a string of name=value pairs.
             * We use the glibc syntax, like
             *      LC_NUMERIC=C;LC_TIME=en_US.UTF-8;...
             *  First calculate the needed size.  Along the way, check if all
             *  the locale names are the same */
            for (i = 0; i < LC_ALL_INDEX; i++) {

#    ifdef DEBUGGING

                if (DEBUG_Lv_TEST || debug_initialization) {
                    PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale i=%d, name=%s, locale=%s\n", __FILE__, __LINE__, i, category_names[i], PL_curlocales[i]);
                }

#    endif

                names_len += strlen(category_names[i])
                          + 1                       /* '=' */
                          + strlen(PL_curlocales[i])
                          + 1;                      /* ';' */

                if (i > 0 && strNE(PL_curlocales[i], PL_curlocales[i-1])) {
                    are_all_categories_the_same_locale = FALSE;
                }
            }

            /* If they are the same, we don't actually have to construct the
             * string; we just make the entry in LC_ALL_INDEX valid, and be
             * that single name */
            if (are_all_categories_the_same_locale) {
                PL_curlocales[LC_ALL_INDEX] = savepv(PL_curlocales[0]);
                return PL_curlocales[LC_ALL_INDEX];
            }

            names_len++;    /* Trailing '\0' */
            SAVEFREEPV(Newx(all_string, names_len, char));
            *all_string = '\0';

            /* Then fill in the string */
            for (i = 0; i < LC_ALL_INDEX; i++) {

#    ifdef DEBUGGING

                if (DEBUG_Lv_TEST || debug_initialization) {
                    PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale i=%d, name=%s, locale=%s\n", __FILE__, __LINE__, i, category_names[i], PL_curlocales[i]);
                }

#    endif

                my_strlcat(all_string, category_names[i], names_len);
                my_strlcat(all_string, "=", names_len);
                my_strlcat(all_string, PL_curlocales[i], names_len);
                my_strlcat(all_string, ";", names_len);
            }

#    ifdef DEBUGGING

            if (DEBUG_L_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale returning %s\n", __FILE__, __LINE__, all_string);
            }

    #endif

            return all_string;
        }

#    ifdef EINVAL

        SETERRNO(EINVAL, LIB_INVARG);

#    endif

        return NULL;

#  endif

    }

    /* Here, we are switching locales. */

#  ifndef HAS_QUERYLOCALE

    if (strEQ(locale, "")) {

        /* For non-querylocale() systems, we do the setting of "" ourselves to
         * be sure that we really know what's going on.  We follow the Linux
         * documented behavior (but if that differs from the actual behavior,
         * this won't work exactly as the OS implements).  We go out and
         * examine the environment based on our understanding of how the system
         * works, and use that to figure things out */

        const char * const lc_all = PerlEnv_getenv("LC_ALL");

        /* Use any "LC_ALL" environment variable, as it overrides everything
         * else. */
        if (lc_all && strNE(lc_all, "")) {
            locale = lc_all;
        }
        else {

            /* Otherwise, we need to dig deeper.  Unless overridden, the
             * default is the LANG environment variable; if it doesn't exist,
             * then "C" */

            const char * default_name;

            default_name = PerlEnv_getenv("LANG");

            if (! default_name || strEQ(default_name, "")) {
                default_name = "C";
            }
            else if (PL_scopestack_ix != 0) {
                /* To minimize other threads messing with the environment,
                 * we copy the variable, making it a temporary.  But this
                 * doesn't work upon program initialization before any
                 * scopes are created, and at this time, there's nothing
                 * else going on that would interfere.  So skip the copy
                 * in that case */
                default_name = savepv(default_name);
                SAVEFREEPV(default_name);
            }

            if (category != LC_ALL) {
                const char * const name = PerlEnv_getenv(category_names[index]);

                /* Here we are setting a single category.  Assume will have the
                 * default name */
                locale = default_name;

                /* But then look for an overriding environment variable */
                if (name && strNE(name, "")) {
                    locale = name;
                }
            }
            else {
                bool did_override = FALSE;
                unsigned int i;

                /* Here, we are getting LC_ALL.  Any categories that don't have
                 * a corresponding environment variable set should be set to
                 * LANG, or to "C" if there is no LANG.  If no individual
                 * categories differ from this, we can just set LC_ALL.  This
                 * is buggy on systems that have extra categories that we don't
                 * know about.  If there is an environment variable that sets
                 * that category, we won't know to look for it, and so our use
                 * of LANG or "C" improperly overrides it.  On the other hand,
                 * if we don't do what is done here, and there is no
                 * environment variable, the category's locale should be set to
                 * LANG or "C".  So there is no good solution.  khw thinks the
                 * best is to look at systems to see what categories they have,
                 * and include them, and then to assume that we know the
                 * complete set */

                for (i = 0; i < LC_ALL_INDEX; i++) {
                    const char * const env_override
                                    = savepv(PerlEnv_getenv(category_names[i]));
                    const char * this_locale = (   env_override
                                                && strNE(env_override, ""))
                                               ? env_override
                                               : default_name;
                    if (! emulate_setlocale(categories[i], this_locale, i, TRUE))
                    {
                        Safefree(env_override);
                        return NULL;
                    }

                    if (strNE(this_locale, default_name)) {
                        did_override = TRUE;
                    }

                    Safefree(env_override);
                }

                /* If all the categories are the same, we can set LC_ALL to
                 * that */
                if (! did_override) {
                    locale = default_name;
                }
                else {

                    /* Here, LC_ALL is no longer valid, as some individual
                     * categories don't match it.  We call ourselves
                     * recursively, as that will execute the code that
                     * generates the proper locale string for this situation.
                     * We don't do the remainder of this function, as that is
                     * to update our records, and we've just done that for the
                     * individual categories in the loop above, and doing so
                     * would cause LC_ALL to be done as well */
                    return emulate_setlocale(LC_ALL, NULL, LC_ALL_INDEX, TRUE);
                }
            }
        }
    }
    else if (strchr(locale, ';')) {

        /* LC_ALL may actually incude a conglomeration of various categories.
         * Without querylocale, this code uses the glibc (as of this writing)
         * syntax for representing that, but that is not a stable API, and
         * other platforms do it differently, so we have to handle all cases
         * ourselves */

        unsigned int i;
        const char * s = locale;
        const char * e = locale + strlen(locale);
        const char * p = s;
        const char * category_end;
        const char * name_start;
        const char * name_end;

        /* If the string that gives what to set doesn't include all categories,
         * the omitted ones get set to "C".  To get this behavior, first set
         * all the individual categories to "C", and override the furnished
         * ones below */
        for (i = 0; i < LC_ALL_INDEX; i++) {
            if (! emulate_setlocale(categories[i], "C", i, TRUE)) {
                return NULL;
            }
        }

        while (s < e) {

            /* Parse through the category */
            while (isWORDCHAR(*p)) {
                p++;
            }
            category_end = p;

            if (*p++ != '=') {
                Perl_croak(aTHX_
                    "panic: %s: %d: Unexpected character in locale name '%02X",
                    __FILE__, __LINE__, *(p-1));
            }

            /* Parse through the locale name */
            name_start = p;
            while (p < e && *p != ';') {
                if (! isGRAPH(*p)) {
                    Perl_croak(aTHX_
                        "panic: %s: %d: Unexpected character in locale name '%02X",
                        __FILE__, __LINE__, *(p-1));
                }
                p++;
            }
            name_end = p;

            /* Space past the semi-colon */
            if (p < e) {
                p++;
            }

            /* Find the index of the category name in our lists */
            for (i = 0; i < LC_ALL_INDEX; i++) {
                char * individ_locale;

                /* Keep going if this isn't the index.  The strnNE() avoids a
                 * Perl_form(), but would fail if ever a category name could be
                 * a substring of another one, like if there were a
                 * "LC_TIME_DATE" */
                if strnNE(s, category_names[i], category_end - s) {
                    continue;
                }

                /* If this index is for the single category we're changing, we
                 * have found the locale to set it to. */
                if (category == categories[i]) {
                    locale = Perl_form(aTHX_ "%.*s",
                                             (int) (name_end - name_start),
                                             name_start);
                    goto ready_to_set;
                }

                assert(category == LC_ALL);
                individ_locale = Perl_form(aTHX_ "%.*s",
                                    (int) (name_end - name_start), name_start);
                if (! emulate_setlocale(categories[i], individ_locale, i, TRUE))
                {
                    return NULL;
                }
            }

            s = p;
        }

        /* Here we have set all the individual categories by recursive calls.
         * These collectively should have fixed up LC_ALL, so can just query
         * what that now is */
        assert(category == LC_ALL);

        return do_setlocale_c(LC_ALL, NULL);
    }

  ready_to_set: ;

    /* Here at the end of having to deal with the absence of querylocale().
     * Some cases have already been fully handled by recursive calls to this
     * function.  But at this point, we haven't dealt with those, but are now
     * prepared to, knowing what the locale name to set this category to is.
     * This would have come for free if this system had had querylocale() */

#  endif  /* end of ! querylocale */

    assert(PL_C_locale_obj);

    /* Switching locales generally entails freeing the current one's space (at
     * the C library's discretion).  We need to stop using that locale before
     * the switch.  So switch to a known locale object that we don't otherwise
     * mess with.  This returns the locale object in effect at the time of the
     * switch. */
    old_obj = uselocale(PL_C_locale_obj);

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale was using %p\n", __FILE__, __LINE__, old_obj);
    }

#  endif

    if (! old_obj) {

#  ifdef DEBUGGING

        if (DEBUG_L_TEST || debug_initialization) {
            dSAVE_ERRNO;
            PerlIO_printf(Perl_debug_log, "%s:%d: emulate_setlocale switching to C failed: %d\n", __FILE__, __LINE__, GET_ERRNO);
            RESTORE_ERRNO;
        }

#  endif

        return NULL;
    }

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log,
                      "%s:%d: emulate_setlocale now using %p\n",
                      __FILE__, __LINE__, PL_C_locale_obj);
    }

#  endif

    /* If we are switching to the LC_ALL C locale, it already exists.  Use
     * it instead of trying to create a new locale */
    if (mask == LC_ALL_MASK && isNAME_C_OR_POSIX(locale)) {

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log,
                          "%s:%d: will stay in C object\n", __FILE__, __LINE__);
        }

#  endif

        new_obj = PL_C_locale_obj;

        /* We already had switched to the C locale in preparation for freeing
         * 'old_obj' */
        if (old_obj != LC_GLOBAL_LOCALE && old_obj != PL_C_locale_obj) {
            freelocale(old_obj);
        }
    }
    else {
        /* If we weren't in a thread safe locale, set so that newlocale() below
         * which uses 'old_obj', uses an empty one.  Same for our reserved C
         * object.  The latter is defensive coding, so that, even if there is
         * some bug, we will never end up trying to modify either of these, as
         * if passed to newlocale(), they can be. */
        if (old_obj == LC_GLOBAL_LOCALE || old_obj == PL_C_locale_obj) {
            old_obj = (locale_t) 0;
        }

        /* Ready to create a new locale by modification of the exising one */
        new_obj = newlocale(mask, locale, old_obj);

        if (! new_obj) {
            dSAVE_ERRNO;

#  ifdef DEBUGGING

            if (DEBUG_L_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log,
                              "%s:%d: emulate_setlocale creating new object"
                              " failed: %d\n", __FILE__, __LINE__, GET_ERRNO);
            }

#  endif

            if (! uselocale(old_obj)) {

#  ifdef DEBUGGING

                if (DEBUG_L_TEST || debug_initialization) {
                    PerlIO_printf(Perl_debug_log,
                                  "%s:%d: switching back failed: %d\n",
                                  __FILE__, __LINE__, GET_ERRNO);
                }

#  endif

            }
            RESTORE_ERRNO;
            return NULL;
        }

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log,
                          "%s:%d: emulate_setlocale created %p",
                          __FILE__, __LINE__, new_obj);
            if (old_obj) {
                PerlIO_printf(Perl_debug_log,
                              "; should have freed %p", old_obj);
            }
            PerlIO_printf(Perl_debug_log, "\n");
        }

#  endif

        /* And switch into it */
        if (! uselocale(new_obj)) {
            dSAVE_ERRNO;

#  ifdef DEBUGGING

            if (DEBUG_L_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log,
                              "%s:%d: emulate_setlocale switching to new object"
                              " failed\n", __FILE__, __LINE__);
            }

#  endif

            if (! uselocale(old_obj)) {

#  ifdef DEBUGGING

                if (DEBUG_L_TEST || debug_initialization) {
                    PerlIO_printf(Perl_debug_log,
                                  "%s:%d: switching back failed: %d\n",
                                  __FILE__, __LINE__, GET_ERRNO);
                }

#  endif

            }
            freelocale(new_obj);
            RESTORE_ERRNO;
            return NULL;
        }
    }

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log,
                      "%s:%d: emulate_setlocale now using %p\n",
                      __FILE__, __LINE__, new_obj);
    }

#  endif

    /* We are done, except for updating our records (if the system doesn't keep
     * them) and in the case of locale "", we don't actually know what the
     * locale that got switched to is, as it came from the environment.  So
     * have to find it */

#  ifdef HAS_QUERYLOCALE

    if (strEQ(locale, "")) {
        locale = querylocale(mask, new_obj);
    }

#  else

    /* Here, 'locale' is the return value */

    /* Without querylocale(), we have to update our records */

    if (category == LC_ALL) {
        unsigned int i;

        /* For LC_ALL, we change all individual categories to correspond */
                              /* PL_curlocales is a parallel array, so has same
                               * length as 'categories' */
        for (i = 0; i <= LC_ALL_INDEX; i++) {
            Safefree(PL_curlocales[i]);
            PL_curlocales[i] = savepv(locale);
        }
    }
    else {

        /* For a single category, if it's not the same as the one in LC_ALL, we
         * nullify LC_ALL */

        if (PL_curlocales[LC_ALL_INDEX] && strNE(PL_curlocales[LC_ALL_INDEX], locale)) {
            Safefree(PL_curlocales[LC_ALL_INDEX]);
            PL_curlocales[LC_ALL_INDEX] = NULL;
        }

        /* Then update the category's record */
        Safefree(PL_curlocales[index]);
        PL_curlocales[index] = savepv(locale);
    }

#  endif

    return locale;
}

#endif /* USE_POSIX_2008_LOCALE */

#if 0   /* Code that was to emulate thread-safe locales on platforms that
           didn't natively support them */

/* The way this would work is that we would keep a per-thread list of the
 * correct locale for that thread.  Any operation that was locale-sensitive
 * would have to be changed so that it would look like this:
 *
 *      LOCALE_LOCK;
 *      setlocale to the correct locale for this operation
 *      do operation
 *      LOCALE_UNLOCK
 *
 * This leaves the global locale in the most recently used operation's, but it
 * was locked long enough to get the result.  If that result is static, it
 * needs to be copied before the unlock.
 *
 * Macros could be written like SETUP_LOCALE_DEPENDENT_OP(category) that did
 * the setup, but are no-ops when not needed, and similarly,
 * END_LOCALE_DEPENDENT_OP for the tear-down
 *
 * But every call to a locale-sensitive function would have to be changed, and
 * if a module didn't cooperate by using the mutex, things would break.
 *
 * This code was abandoned before being completed or tested, and is left as-is
*/

#  define do_setlocale_c(cat, locale) locking_setlocale(cat, locale, cat ## _INDEX, TRUE)
#  define do_setlocale_r(cat, locale) locking_setlocale(cat, locale, 0, FALSE)

STATIC char *
S_locking_setlocale(pTHX_
                    const int category,
                    const char * locale,
                    int index,
                    const bool is_index_valid
                   )
{
    /* This function kind of performs a setlocale() on just the current thread;
     * thus it is kind of thread-safe.  It does this by keeping a thread-level
     * array of the current locales for each category.  Every time a locale is
     * switched to, it does the switch globally, but updates the thread's
     * array.  A query as to what the current locale is just returns the
     * appropriate element from the array, and doesn't actually call the system
     * setlocale().  The saving into the array is done in an uninterruptible
     * section of code, so is unaffected by whatever any other threads might be
     * doing.
     *
     * All locale-sensitive operations must work by first starting a critical
     * section, then switching to the thread's locale as kept by this function,
     * and then doing the operation, then ending the critical section.  Thus,
     * each gets done in the appropriate locale. simulating thread-safety.
     *
     * This function takes the same parameters, 'category' and 'locale', that
     * the regular setlocale() function does, but it also takes two additional
     * ones.  This is because as described earlier.  If we know on input the
     * index corresponding to the category into the array where we store the
     * current locales, we don't have to calculate it.  If the caller knows at
     * compile time what the index is, it it can pass it, setting
     * 'is_index_valid' to TRUE; otherwise the index parameter is ignored.
     *
     */

    /* If the input index might be incorrect, calculate the correct one */
    if (! is_index_valid) {
        unsigned int i;

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "%s:%d: converting category %d to index\n", __FILE__, __LINE__, category);
        }

        for (i = 0; i <= LC_ALL_INDEX; i++) {
            if (category == categories[i]) {
                index = i;
                goto found_index;
            }
        }

        /* Here, we don't know about this category, so can't handle it.
         * XXX best we can do is to unsafely set this
         * XXX warning */

        return my_setlocale(category, locale);

      found_index: ;

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "%s:%d: index is 0x%x\n", __FILE__, __LINE__, index);
        }
    }

    /* For a query, just return what's in our records */
    if (new_locale == NULL) {
        return curlocales[index];
    }


    /* Otherwise, we need to do the switch, and save the result, all in a
     * critical section */

    Safefree(curlocales[[index]]);

    /* It might be that this is called from an already-locked section of code.
     * We would have to detect and skip the LOCK/UNLOCK if so */
    LOCALE_LOCK;

    curlocales[index] = savepv(my_setlocale(category, new_locale));

    if (strEQ(new_locale, "")) {

#ifdef LC_ALL

        /* The locale values come from the environment, and may not all be the
         * same, so for LC_ALL, we have to update all the others, while the
         * mutex is still locked */

        if (category == LC_ALL) {
            unsigned int i;
            for (i = 0; i < LC_ALL_INDEX) {
                curlocales[i] = my_setlocale(categories[i], NULL);
            }
        }
    }

#endif

    LOCALE_UNLOCK;

    return curlocales[index];
}

#endif
#ifdef USE_LOCALE

STATIC void
S_set_numeric_radix(pTHX_ const bool use_locale)
{
    /* If 'use_locale' is FALSE, set to use a dot for the radix character.  If
     * TRUE, use the radix character derived from the current locale */

#if defined(USE_LOCALE_NUMERIC) && (   defined(HAS_LOCALECONV)              \
                                    || defined(HAS_NL_LANGINFO))

    const char * radix = (use_locale)
                         ? my_nl_langinfo(RADIXCHAR, FALSE)
                                        /* FALSE => already in dest locale */
                         : ".";

        sv_setpv(PL_numeric_radix_sv, radix);

    /* If this is valid UTF-8 that isn't totally ASCII, and we are in
        * a UTF-8 locale, then mark the radix as being in UTF-8 */
    if (is_utf8_non_invariant_string((U8 *) SvPVX(PL_numeric_radix_sv),
                                            SvCUR(PL_numeric_radix_sv))
        && _is_cur_LC_category_utf8(LC_NUMERIC))
    {
        SvUTF8_on(PL_numeric_radix_sv);
    }

#  ifdef DEBUGGING

    if (DEBUG_L_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "Locale radix is '%s', ?UTF-8=%d\n",
                                           SvPVX(PL_numeric_radix_sv),
                                           cBOOL(SvUTF8(PL_numeric_radix_sv)));
    }

#  endif
#else

    PERL_UNUSED_ARG(use_locale);

#endif /* USE_LOCALE_NUMERIC and can find the radix char */

}

STATIC void
S_new_numeric(pTHX_ const char *newnum)
{

#ifndef USE_LOCALE_NUMERIC

    PERL_UNUSED_ARG(newnum);

#else

    /* Called after each libc setlocale() call affecting LC_NUMERIC, to tell
     * core Perl this and that 'newnum' is the name of the new locale.
     * It installs this locale as the current underlying default.
     *
     * The default locale and the C locale can be toggled between by use of the
     * set_numeric_underlying() and set_numeric_standard() functions, which
     * should probably not be called directly, but only via macros like
     * SET_NUMERIC_STANDARD() in perl.h.
     *
     * The toggling is necessary mainly so that a non-dot radix decimal point
     * character can be output, while allowing internal calculations to use a
     * dot.
     *
     * This sets several interpreter-level variables:
     * PL_numeric_name  The underlying locale's name: a copy of 'newnum'
     * PL_numeric_underlying  A boolean indicating if the toggled state is such
     *                  that the current locale is the program's underlying
     *                  locale
     * PL_numeric_standard An int indicating if the toggled state is such
     *                  that the current locale is the C locale or
     *                  indistinguishable from the C locale.  If non-zero, it
     *                  is in C; if > 1, it means it may not be toggled away
     *                  from C.
     * PL_numeric_underlying_is_standard   A bool kept by this function
     *                  indicating that the underlying locale and the standard
     *                  C locale are indistinguishable for the purposes of
     *                  LC_NUMERIC.  This happens when both of the above two
     *                  variables are true at the same time.  (Toggling is a
     *                  no-op under these circumstances.)  This variable is
     *                  used to avoid having to recalculate.
     */

    char *save_newnum;

    if (! newnum) {
	Safefree(PL_numeric_name);
	PL_numeric_name = NULL;
	PL_numeric_standard = TRUE;
	PL_numeric_underlying = TRUE;
	PL_numeric_underlying_is_standard = TRUE;
	return;
    }

    save_newnum = stdize_locale(savepv(newnum));
    PL_numeric_underlying = TRUE;
    PL_numeric_standard = isNAME_C_OR_POSIX(save_newnum);

#ifndef TS_W32_BROKEN_LOCALECONV

    /* If its name isn't C nor POSIX, it could still be indistinguishable from
     * them.  But on broken Windows systems calling my_nl_langinfo() for
     * THOUSEP can currently (but rarely) cause a race, so avoid doing that,
     * and just always change the locale if not C nor POSIX on those systems */
    if (! PL_numeric_standard) {
        PL_numeric_standard = cBOOL(strEQ(".", my_nl_langinfo(RADIXCHAR,
                                            FALSE /* Don't toggle locale */  ))
                                 && strEQ("",  my_nl_langinfo(THOUSEP, FALSE)));
    }

#endif

    /* Save the new name if it isn't the same as the previous one, if any */
    if (! PL_numeric_name || strNE(PL_numeric_name, save_newnum)) {
	Safefree(PL_numeric_name);
	PL_numeric_name = save_newnum;
    }
    else {
	Safefree(save_newnum);
    }

    PL_numeric_underlying_is_standard = PL_numeric_standard;

#  ifdef HAS_POSIX_2008_LOCALE

    PL_underlying_numeric_obj = newlocale(LC_NUMERIC_MASK,
                                          PL_numeric_name,
                                          PL_underlying_numeric_obj);

#endif

    if (DEBUG_L_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "Called new_numeric with %s, PL_numeric_name=%s\n", newnum, PL_numeric_name);
    }

    /* Keep LC_NUMERIC in the C locale.  This is for XS modules, so they don't
     * have to worry about the radix being a non-dot.  (Core operations that
     * need the underlying locale change to it temporarily). */
    if (PL_numeric_standard) {
        set_numeric_radix(0);
    }
    else {
        set_numeric_standard();
    }

#endif /* USE_LOCALE_NUMERIC */

}

void
Perl_set_numeric_standard(pTHX)
{

#ifdef USE_LOCALE_NUMERIC

    /* Toggle the LC_NUMERIC locale to C.  Most code should use the macros like
     * SET_NUMERIC_STANDARD() in perl.h instead of calling this directly.  The
     * macro avoids calling this routine if toggling isn't necessary according
     * to our records (which could be wrong if some XS code has changed the
     * locale behind our back) */

#  ifdef DEBUGGING

    if (DEBUG_L_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log,
                          "Setting LC_NUMERIC locale to standard C\n");
    }

#  endif

    do_setlocale_c(LC_NUMERIC, "C");
    PL_numeric_standard = TRUE;
    PL_numeric_underlying = PL_numeric_underlying_is_standard;
    set_numeric_radix(0);

#endif /* USE_LOCALE_NUMERIC */

}

void
Perl_set_numeric_underlying(pTHX)
{

#ifdef USE_LOCALE_NUMERIC

    /* Toggle the LC_NUMERIC locale to the current underlying default.  Most
     * code should use the macros like SET_NUMERIC_UNDERLYING() in perl.h
     * instead of calling this directly.  The macro avoids calling this routine
     * if toggling isn't necessary according to our records (which could be
     * wrong if some XS code has changed the locale behind our back) */

#  ifdef DEBUGGING

    if (DEBUG_L_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log,
                          "Setting LC_NUMERIC locale to %s\n",
                          PL_numeric_name);
    }

#  endif

    do_setlocale_c(LC_NUMERIC, PL_numeric_name);
    PL_numeric_standard = PL_numeric_underlying_is_standard;
    PL_numeric_underlying = TRUE;
    set_numeric_radix(! PL_numeric_standard);

#endif /* USE_LOCALE_NUMERIC */

}

/*
 * Set up for a new ctype locale.
 */
STATIC void
S_new_ctype(pTHX_ const char *newctype)
{

#ifndef USE_LOCALE_CTYPE

    PERL_UNUSED_ARG(newctype);
    PERL_UNUSED_CONTEXT;

#else

    /* Called after each libc setlocale() call affecting LC_CTYPE, to tell
     * core Perl this and that 'newctype' is the name of the new locale.
     *
     * This function sets up the folding arrays for all 256 bytes, assuming
     * that tofold() is tolc() since fold case is not a concept in POSIX,
     *
     * Any code changing the locale (outside this file) should use
     * Perl_setlocale or POSIX::setlocale, which call this function.  Therefore
     * this function should be called directly only from this file and from
     * POSIX::setlocale() */

    dVAR;
    unsigned int i;

    /* Don't check for problems if we are suppressing the warnings */
    bool check_for_problems = ckWARN_d(WARN_LOCALE) || UNLIKELY(DEBUG_L_TEST);
    bool maybe_utf8_turkic = FALSE;

    PERL_ARGS_ASSERT_NEW_CTYPE;

    /* We will replace any bad locale warning with 1) nothing if the new one is
     * ok; or 2) a new warning for the bad new locale */
    if (PL_warn_locale) {
        SvREFCNT_dec_NN(PL_warn_locale);
        PL_warn_locale = NULL;
    }

    PL_in_utf8_CTYPE_locale = _is_cur_LC_category_utf8(LC_CTYPE);

    /* A UTF-8 locale gets standard rules.  But note that code still has to
     * handle this specially because of the three problematic code points */
    if (PL_in_utf8_CTYPE_locale) {
        Copy(PL_fold_latin1, PL_fold_locale, 256, U8);

        /* UTF-8 locales can have special handling for 'I' and 'i' if they are
         * Turkic.  Make sure these two are the only anomalies.  (We don't use
         * towupper and towlower because they aren't in C89.) */

#if defined(HAS_TOWUPPER) && defined (HAS_TOWLOWER)

        if (towupper('i') == 0x130 && towlower('I') == 0x131) {

#else

        if (toupper('i') == 'i' && tolower('I') == 'I') {

#endif
            check_for_problems = TRUE;
            maybe_utf8_turkic = TRUE;
        }
    }

    /* We don't populate the other lists if a UTF-8 locale, but do check that
     * everything works as expected, unless checking turned off */
    if (check_for_problems || ! PL_in_utf8_CTYPE_locale) {
        /* Assume enough space for every character being bad.  4 spaces each
         * for the 94 printable characters that are output like "'x' "; and 5
         * spaces each for "'\\' ", "'\t' ", and "'\n' "; plus a terminating
         * NUL */
        char bad_chars_list[ (94 * 4) + (3 * 5) + 1 ] = { '\0' };
        bool multi_byte_locale = FALSE;     /* Assume is a single-byte locale
                                               to start */
        unsigned int bad_count = 0;         /* Count of bad characters */

        for (i = 0; i < 256; i++) {
            if (! PL_in_utf8_CTYPE_locale) {
                if (isupper(i))
                    PL_fold_locale[i] = (U8) tolower(i);
                else if (islower(i))
                    PL_fold_locale[i] = (U8) toupper(i);
                else
                    PL_fold_locale[i] = (U8) i;
            }

            /* If checking for locale problems, see if the native ASCII-range
             * printables plus \n and \t are in their expected categories in
             * the new locale.  If not, this could mean big trouble, upending
             * Perl's and most programs' assumptions, like having a
             * metacharacter with special meaning become a \w.  Fortunately,
             * it's very rare to find locales that aren't supersets of ASCII
             * nowadays.  It isn't a problem for most controls to be changed
             * into something else; we check only \n and \t, though perhaps \r
             * could be an issue as well. */
            if (    check_for_problems
                && (isGRAPH_A(i) || isBLANK_A(i) || i == '\n'))
            {
                bool is_bad = FALSE;
                char name[4] = { '\0' };

                /* Convert the name into a string */
                if (isGRAPH_A(i)) {
                    name[0] = i;
                    name[1] = '\0';
                }
                else if (i == '\n') {
                    my_strlcpy(name, "\\n", sizeof(name));
                }
                else if (i == '\t') {
                    my_strlcpy(name, "\\t", sizeof(name));
                }
                else {
                    assert(i == ' ');
                    my_strlcpy(name, "' '", sizeof(name));
                }

                /* Check each possibe class */
                if (UNLIKELY(cBOOL(isalnum(i)) != cBOOL(isALPHANUMERIC_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isalnum('%s') unexpectedly is %d\n",
                                          name, cBOOL(isalnum(i))));
                }
                if (UNLIKELY(cBOOL(isalpha(i)) != cBOOL(isALPHA_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isalpha('%s') unexpectedly is %d\n",
                                          name, cBOOL(isalpha(i))));
                }
                if (UNLIKELY(cBOOL(isdigit(i)) != cBOOL(isDIGIT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isdigit('%s') unexpectedly is %d\n",
                                          name, cBOOL(isdigit(i))));
                }
                if (UNLIKELY(cBOOL(isgraph(i)) != cBOOL(isGRAPH_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isgraph('%s') unexpectedly is %d\n",
                                          name, cBOOL(isgraph(i))));
                }
                if (UNLIKELY(cBOOL(islower(i)) != cBOOL(isLOWER_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "islower('%s') unexpectedly is %d\n",
                                          name, cBOOL(islower(i))));
                }
                if (UNLIKELY(cBOOL(isprint(i)) != cBOOL(isPRINT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isprint('%s') unexpectedly is %d\n",
                                          name, cBOOL(isprint(i))));
                }
                if (UNLIKELY(cBOOL(ispunct(i)) != cBOOL(isPUNCT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "ispunct('%s') unexpectedly is %d\n",
                                          name, cBOOL(ispunct(i))));
                }
                if (UNLIKELY(cBOOL(isspace(i)) != cBOOL(isSPACE_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isspace('%s') unexpectedly is %d\n",
                                          name, cBOOL(isspace(i))));
                }
                if (UNLIKELY(cBOOL(isupper(i)) != cBOOL(isUPPER_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isupper('%s') unexpectedly is %d\n",
                                          name, cBOOL(isupper(i))));
                }
                if (UNLIKELY(cBOOL(isxdigit(i))!= cBOOL(isXDIGIT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isxdigit('%s') unexpectedly is %d\n",
                                          name, cBOOL(isxdigit(i))));
                }
                if (UNLIKELY(tolower(i) != (int) toLOWER_A(i)))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "tolower('%s')=0x%x instead of the expected 0x%x\n",
                            name, tolower(i), (int) toLOWER_A(i)));
                }
                if (UNLIKELY(toupper(i) != (int) toUPPER_A(i)))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "toupper('%s')=0x%x instead of the expected 0x%x\n",
                            name, toupper(i), (int) toUPPER_A(i)));
                }
                if (UNLIKELY((i == '\n' && ! isCNTRL_LC(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                "'\\n' (=%02X) is not a control\n", (int) i));
                }

                /* Add to the list;  Separate multiple entries with a blank */
                if (is_bad) {
                    if (bad_count) {
                        my_strlcat(bad_chars_list, " ", sizeof(bad_chars_list));
                    }
                    my_strlcat(bad_chars_list, name, sizeof(bad_chars_list));
                    bad_count++;
                }
            }
        }

        if (bad_count == 2 && maybe_utf8_turkic) {
            bad_count = 0;
            *bad_chars_list = '\0';
            PL_fold_locale['I'] = 'I';
            PL_fold_locale['i'] = 'i';
            PL_in_utf8_turkic_locale = TRUE;
            DEBUG_L(PerlIO_printf(Perl_debug_log, "%s:%d: %s is turkic\n",
                                                 __FILE__, __LINE__, newctype));
        }
        else {
            PL_in_utf8_turkic_locale = FALSE;
        }

#  ifdef MB_CUR_MAX

        /* We only handle single-byte locales (outside of UTF-8 ones; so if
         * this locale requires more than one byte, there are going to be
         * problems. */
        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                 "%s:%d: check_for_problems=%d, MB_CUR_MAX=%d\n",
                 __FILE__, __LINE__, check_for_problems, (int) MB_CUR_MAX));

        if (   check_for_problems && MB_CUR_MAX > 1
            && ! PL_in_utf8_CTYPE_locale

               /* Some platforms return MB_CUR_MAX > 1 for even the "C"
                * locale.  Just assume that the implementation for them (plus
                * for POSIX) is correct and the > 1 value is spurious.  (Since
                * these are specially handled to never be considered UTF-8
                * locales, as long as this is the only problem, everything
                * should work fine */
            && strNE(newctype, "C") && strNE(newctype, "POSIX"))
        {
            multi_byte_locale = TRUE;
        }

#  endif

        /* If we found problems and we want them output, do so */
        if (   (UNLIKELY(bad_count) || UNLIKELY(multi_byte_locale))
            && (LIKELY(ckWARN_d(WARN_LOCALE)) || UNLIKELY(DEBUG_L_TEST)))
        {
            if (UNLIKELY(bad_count) && PL_in_utf8_CTYPE_locale) {
                PL_warn_locale = Perl_newSVpvf(aTHX_
                     "Locale '%s' contains (at least) the following characters"
                     " which have\nunexpected meanings: %s\nThe Perl program"
                     " will use the expected meanings",
                      newctype, bad_chars_list);
            }
            else {
                PL_warn_locale = Perl_newSVpvf(aTHX_
                             "Locale '%s' may not work well.%s%s%s\n",
                             newctype,
                             (multi_byte_locale)
                              ? "  Some characters in it are not recognized by"
                                " Perl."
                              : "",
                             (bad_count)
                              ? "\nThe following characters (and maybe others)"
                                " may not have the same meaning as the Perl"
                                " program expects:\n"
                              : "",
                             (bad_count)
                              ? bad_chars_list
                              : ""
                            );
            }

#  ifdef HAS_NL_LANGINFO

            Perl_sv_catpvf(aTHX_ PL_warn_locale, "; codeset=%s",
                                    /* parameter FALSE is a don't care here */
                                    my_nl_langinfo(CODESET, FALSE));

#  endif

            Perl_sv_catpvf(aTHX_ PL_warn_locale, "\n");

            /* If we are actually in the scope of the locale or are debugging,
             * output the message now.  If not in that scope, we save the
             * message to be output at the first operation using this locale,
             * if that actually happens.  Most programs don't use locales, so
             * they are immune to bad ones.  */
            if (IN_LC(LC_CTYPE) || UNLIKELY(DEBUG_L_TEST)) {

                /* The '0' below suppresses a bogus gcc compiler warning */
                Perl_warner(aTHX_ packWARN(WARN_LOCALE), SvPVX(PL_warn_locale), 0);

                if (IN_LC(LC_CTYPE)) {
                    SvREFCNT_dec_NN(PL_warn_locale);
                    PL_warn_locale = NULL;
                }
            }
        }
    }

#endif /* USE_LOCALE_CTYPE */

}

void
Perl__warn_problematic_locale()
{

#ifdef USE_LOCALE_CTYPE

    dTHX;

    /* Internal-to-core function that outputs the message in PL_warn_locale,
     * and then NULLS it.  Should be called only through the macro
     * _CHECK_AND_WARN_PROBLEMATIC_LOCALE */

    if (PL_warn_locale) {
        Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE),
                             SvPVX(PL_warn_locale),
                             0 /* dummy to avoid compiler warning */ );
        SvREFCNT_dec_NN(PL_warn_locale);
        PL_warn_locale = NULL;
    }

#endif

}

STATIC void
S_new_collate(pTHX_ const char *newcoll)
{

#ifndef USE_LOCALE_COLLATE

    PERL_UNUSED_ARG(newcoll);
    PERL_UNUSED_CONTEXT;

#else

    /* Called after each libc setlocale() call affecting LC_COLLATE, to tell
     * core Perl this and that 'newcoll' is the name of the new locale.
     *
     * The design of locale collation is that every locale change is given an
     * index 'PL_collation_ix'.  The first time a string particpates in an
     * operation that requires collation while locale collation is active, it
     * is given PERL_MAGIC_collxfrm magic (via sv_collxfrm_flags()).  That
     * magic includes the collation index, and the transformation of the string
     * by strxfrm(), q.v.  That transformation is used when doing comparisons,
     * instead of the string itself.  If a string changes, the magic is
     * cleared.  The next time the locale changes, the index is incremented,
     * and so we know during a comparison that the transformation is not
     * necessarily still valid, and so is recomputed.  Note that if the locale
     * changes enough times, the index could wrap (a U32), and it is possible
     * that a transformation would improperly be considered valid, leading to
     * an unlikely bug */

    if (! newcoll) {
	if (PL_collation_name) {
	    ++PL_collation_ix;
	    Safefree(PL_collation_name);
	    PL_collation_name = NULL;
	}
	PL_collation_standard = TRUE;
      is_standard_collation:
	PL_collxfrm_base = 0;
	PL_collxfrm_mult = 2;
        PL_in_utf8_COLLATE_locale = FALSE;
        PL_strxfrm_NUL_replacement = '\0';
        PL_strxfrm_max_cp = 0;
	return;
    }

    /* If this is not the same locale as currently, set the new one up */
    if (! PL_collation_name || strNE(PL_collation_name, newcoll)) {
	++PL_collation_ix;
	Safefree(PL_collation_name);
	PL_collation_name = stdize_locale(savepv(newcoll));
	PL_collation_standard = isNAME_C_OR_POSIX(newcoll);
        if (PL_collation_standard) {
            goto is_standard_collation;
        }

        PL_in_utf8_COLLATE_locale = _is_cur_LC_category_utf8(LC_COLLATE);
        PL_strxfrm_NUL_replacement = '\0';
        PL_strxfrm_max_cp = 0;

        /* A locale collation definition includes primary, secondary, tertiary,
         * etc. weights for each character.  To sort, the primary weights are
         * used, and only if they compare equal, then the secondary weights are
         * used, and only if they compare equal, then the tertiary, etc.
         *
         * strxfrm() works by taking the input string, say ABC, and creating an
         * output transformed string consisting of first the primary weights,
         * A¹B¹C¹ followed by the secondary ones, A²B²C²; and then the
         * tertiary, etc, yielding A¹B¹C¹ A²B²C² A³B³C³ ....  Some characters
         * may not have weights at every level.  In our example, let's say B
         * doesn't have a tertiary weight, and A doesn't have a secondary
         * weight.  The constructed string is then going to be
         *  A¹B¹C¹ B²C² A³C³ ....
         * This has the desired effect that strcmp() will look at the secondary
         * or tertiary weights only if the strings compare equal at all higher
         * priority weights.  The spaces shown here, like in
         *  "A¹B¹C¹ A²B²C² "
         * are not just for readability.  In the general case, these must
         * actually be bytes, which we will call here 'separator weights'; and
         * they must be smaller than any other weight value, but since these
         * are C strings, only the terminating one can be a NUL (some
         * implementations may include a non-NUL separator weight just before
         * the NUL).  Implementations tend to reserve 01 for the separator
         * weights.  They are needed so that a shorter string's secondary
         * weights won't be misconstrued as primary weights of a longer string,
         * etc.  By making them smaller than any other weight, the shorter
         * string will sort first.  (Actually, if all secondary weights are
         * smaller than all primary ones, there is no need for a separator
         * weight between those two levels, etc.)
         *
         * The length of the transformed string is roughly a linear function of
         * the input string.  It's not exactly linear because some characters
         * don't have weights at all levels.  When we call strxfrm() we have to
         * allocate some memory to hold the transformed string.  The
         * calculations below try to find coefficients 'm' and 'b' for this
         * locale so that m*x + b equals how much space we need, given the size
         * of the input string in 'x'.  If we calculate too small, we increase
         * the size as needed, and call strxfrm() again, but it is better to
         * get it right the first time to avoid wasted expensive string
         * transformations. */

	{
            /* We use the string below to find how long the tranformation of it
             * is.  Almost all locales are supersets of ASCII, or at least the
             * ASCII letters.  We use all of them, half upper half lower,
             * because if we used fewer, we might hit just the ones that are
             * outliers in a particular locale.  Most of the strings being
             * collated will contain a preponderance of letters, and even if
             * they are above-ASCII, they are likely to have the same number of
             * weight levels as the ASCII ones.  It turns out that digits tend
             * to have fewer levels, and some punctuation has more, but those
             * are relatively sparse in text, and khw believes this gives a
             * reasonable result, but it could be changed if experience so
             * dictates. */
            const char longer[] = "ABCDEFGHIJKLMnopqrstuvwxyz";
            char * x_longer;        /* Transformed 'longer' */
            Size_t x_len_longer;    /* Length of 'x_longer' */

            char * x_shorter;   /* We also transform a substring of 'longer' */
            Size_t x_len_shorter;

            /* _mem_collxfrm() is used get the transformation (though here we
             * are interested only in its length).  It is used because it has
             * the intelligence to handle all cases, but to work, it needs some
             * values of 'm' and 'b' to get it started.  For the purposes of
             * this calculation we use a very conservative estimate of 'm' and
             * 'b'.  This assumes a weight can be multiple bytes, enough to
             * hold any UV on the platform, and there are 5 levels, 4 weight
             * bytes, and a trailing NUL.  */
            PL_collxfrm_base = 5;
            PL_collxfrm_mult = 5 * sizeof(UV);

            /* Find out how long the transformation really is */
            x_longer = _mem_collxfrm(longer,
                                     sizeof(longer) - 1,
                                     &x_len_longer,

                                     /* We avoid converting to UTF-8 in the
                                      * called function by telling it the
                                      * string is in UTF-8 if the locale is a
                                      * UTF-8 one.  Since the string passed
                                      * here is invariant under UTF-8, we can
                                      * claim it's UTF-8 even though it isn't.
                                      * */
                                     PL_in_utf8_COLLATE_locale);
            Safefree(x_longer);

            /* Find out how long the transformation of a substring of 'longer'
             * is.  Together the lengths of these transformations are
             * sufficient to calculate 'm' and 'b'.  The substring is all of
             * 'longer' except the first character.  This minimizes the chances
             * of being swayed by outliers */
            x_shorter = _mem_collxfrm(longer + 1,
                                      sizeof(longer) - 2,
                                      &x_len_shorter,
                                      PL_in_utf8_COLLATE_locale);
            Safefree(x_shorter);

            /* If the results are nonsensical for this simple test, the whole
             * locale definition is suspect.  Mark it so that locale collation
             * is not active at all for it.  XXX Should we warn? */
            if (   x_len_shorter == 0
                || x_len_longer == 0
                || x_len_shorter >= x_len_longer)
            {
                PL_collxfrm_mult = 0;
                PL_collxfrm_base = 0;
            }
            else {
                SSize_t base;       /* Temporary */

                /* We have both:    m * strlen(longer)  + b = x_len_longer
                 *                  m * strlen(shorter) + b = x_len_shorter;
                 * subtracting yields:
                 *          m * (strlen(longer) - strlen(shorter))
                 *                             = x_len_longer - x_len_shorter
                 * But we have set things up so that 'shorter' is 1 byte smaller
                 * than 'longer'.  Hence:
                 *          m = x_len_longer - x_len_shorter
                 *
                 * But if something went wrong, make sure the multiplier is at
                 * least 1.
                 */
                if (x_len_longer > x_len_shorter) {
                    PL_collxfrm_mult = (STRLEN) x_len_longer - x_len_shorter;
                }
                else {
                    PL_collxfrm_mult = 1;
                }

                /*     mx + b = len
                 * so:      b = len - mx
                 * but in case something has gone wrong, make sure it is
                 * non-negative */
                base = x_len_longer - PL_collxfrm_mult * (sizeof(longer) - 1);
                if (base < 0) {
                    base = 0;
                }

                /* Add 1 for the trailing NUL */
                PL_collxfrm_base = base + 1;
            }

#  ifdef DEBUGGING

            if (DEBUG_L_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log,
                    "%s:%d: ?UTF-8 locale=%d; x_len_shorter=%zu, "
                    "x_len_longer=%zu,"
                    " collate multipler=%zu, collate base=%zu\n",
                    __FILE__, __LINE__,
                    PL_in_utf8_COLLATE_locale,
                    x_len_shorter, x_len_longer,
                    PL_collxfrm_mult, PL_collxfrm_base);
            }
#  endif

	}
    }

#endif /* USE_LOCALE_COLLATE */

}

#endif

#ifdef WIN32

#define USE_WSETLOCALE

#ifdef USE_WSETLOCALE

STATIC char *
S_wrap_wsetlocale(pTHX_ int category, const char *locale) {
    wchar_t *wlocale;
    wchar_t *wresult;
    char *result;

    if (locale) {
        int req_size =
            MultiByteToWideChar(CP_UTF8, 0, locale, -1, NULL, 0);

        if (!req_size) {
            errno = EINVAL;
            return NULL;
        }

        Newx(wlocale, req_size, wchar_t);
        if (!MultiByteToWideChar(CP_UTF8, 0, locale, -1, wlocale, req_size)) {
            Safefree(wlocale);
            errno = EINVAL;
            return NULL;
        }
    }
    else {
        wlocale = NULL;
    }
    wresult = _wsetlocale(category, wlocale);
    Safefree(wlocale);
    if (wresult) {
        int req_size =
            WideCharToMultiByte(CP_UTF8, 0, wresult, -1, NULL, 0, NULL, NULL);
        Newx(result, req_size, char);
        SAVEFREEPV(result); /* is there something better we can do here? */
        if (!WideCharToMultiByte(CP_UTF8, 0, wresult, -1,
                                 result, req_size, NULL, NULL)) {
            errno = EINVAL;
            return NULL;
        }
    }
    else {
        result = NULL;
    }

    return result;
}

#endif

STATIC char *
S_win32_setlocale(pTHX_ int category, const char* locale)
{
    /* This, for Windows, emulates POSIX setlocale() behavior.  There is no
     * difference between the two unless the input locale is "", which normally
     * means on Windows to get the machine default, which is set via the
     * computer's "Regional and Language Options" (or its current equivalent).
     * In POSIX, it instead means to find the locale from the user's
     * environment.  This routine changes the Windows behavior to first look in
     * the environment, and, if anything is found, use that instead of going to
     * the machine default.  If there is no environment override, the machine
     * default is used, by calling the real setlocale() with "".
     *
     * The POSIX behavior is to use the LC_ALL variable if set; otherwise to
     * use the particular category's variable if set; otherwise to use the LANG
     * variable. */

    bool override_LC_ALL = FALSE;
    char * result;
    unsigned int i;

    if (locale && strEQ(locale, "")) {

#  ifdef LC_ALL

        locale = PerlEnv_getenv("LC_ALL");
        if (! locale) {
            if (category ==  LC_ALL) {
                override_LC_ALL = TRUE;
            }
            else {

#  endif

                for (i = 0; i < NOMINAL_LC_ALL_INDEX; i++) {
                    if (category == categories[i]) {
                        locale = PerlEnv_getenv(category_names[i]);
                        goto found_locale;
                    }
                }

                locale = PerlEnv_getenv("LANG");
                if (! locale) {
                    locale = "";
                }

              found_locale: ;

#  ifdef LC_ALL

            }
        }

#  endif

    }

#ifdef USE_WSETLOCALE
    result = S_wrap_wsetlocale(aTHX_ category, locale);
#else
    result = setlocale(category, locale);
#endif
    DEBUG_L(STMT_START {
                dSAVE_ERRNO;
                PerlIO_printf(Perl_debug_log, "%s:%d: %s\n", __FILE__, __LINE__,
                            setlocale_debug_string(category, locale, result));
                RESTORE_ERRNO;
            } STMT_END);

    if (! override_LC_ALL)  {
        return result;
    }

    /* Here the input category was LC_ALL, and we have set it to what is in the
     * LANG variable or the system default if there is no LANG.  But these have
     * lower priority than the other LC_foo variables, so override it for each
     * one that is set.  (If they are set to "", it means to use the same thing
     * we just set LC_ALL to, so can skip) */

    for (i = 0; i < LC_ALL_INDEX; i++) {
        result = PerlEnv_getenv(category_names[i]);
        if (result && strNE(result, "")) {
#ifdef USE_WSETLOCALE
            S_wrap_wsetlocale(aTHX_ categories[i], result);
#else
            setlocale(categories[i], result);
#endif
            DEBUG_Lv(PerlIO_printf(Perl_debug_log, "%s:%d: %s\n",
                __FILE__, __LINE__,
                setlocale_debug_string(categories[i], result, "not captured")));
        }
    }

    result = setlocale(LC_ALL, NULL);
    DEBUG_L(STMT_START {
                dSAVE_ERRNO;
                PerlIO_printf(Perl_debug_log, "%s:%d: %s\n",
                               __FILE__, __LINE__,
                               setlocale_debug_string(LC_ALL, NULL, result));
                RESTORE_ERRNO;
            } STMT_END);

    return result;
}

#endif

/*

=head1 Locale-related functions and macros

=for apidoc Perl_setlocale

This is an (almost) drop-in replacement for the system L<C<setlocale(3)>>,
taking the same parameters, and returning the same information, except that it
returns the correct underlying C<LC_NUMERIC> locale.  Regular C<setlocale> will
instead return C<C> if the underlying locale has a non-dot decimal point
character, or a non-empty thousands separator for displaying floating point
numbers.  This is because perl keeps that locale category such that it has a
dot and empty separator, changing the locale briefly during the operations
where the underlying one is required. C<Perl_setlocale> knows about this, and
compensates; regular C<setlocale> doesn't.

Another reason it isn't completely a drop-in replacement is that it is
declared to return S<C<const char *>>, whereas the system setlocale omits the
C<const> (presumably because its API was specified long ago, and can't be
updated; it is illegal to change the information C<setlocale> returns; doing
so leads to segfaults.)

Finally, C<Perl_setlocale> works under all circumstances, whereas plain
C<setlocale> can be completely ineffective on some platforms under some
configurations.

C<Perl_setlocale> should not be used to change the locale except on systems
where the predefined variable C<${^SAFE_LOCALES}> is 1.  On some such systems,
the system C<setlocale()> is ineffective, returning the wrong information, and
failing to actually change the locale.  C<Perl_setlocale>, however works
properly in all circumstances.

The return points to a per-thread static buffer, which is overwritten the next
time C<Perl_setlocale> is called from the same thread.

=cut

*/

const char *
Perl_setlocale(const int category, const char * locale)
{
    /* This wraps POSIX::setlocale() */

#ifndef USE_LOCALE

    PERL_UNUSED_ARG(category);
    PERL_UNUSED_ARG(locale);

    return "C";

#else

    const char * retval;
    const char * newlocale;
    dSAVEDERRNO;
    dTHX;
    DECLARATION_FOR_LC_NUMERIC_MANIPULATION;

#ifdef USE_LOCALE_NUMERIC

    /* A NULL locale means only query what the current one is.  We have the
     * LC_NUMERIC name saved, because we are normally switched into the C
     * (or equivalent) locale for it.  For an LC_ALL query, switch back to get
     * the correct results.  All other categories don't require special
     * handling */
    if (locale == NULL) {
        if (category == LC_NUMERIC) {

            /* We don't have to copy this return value, as it is a per-thread
             * variable, and won't change until a future setlocale */
            return PL_numeric_name;
        }

#  ifdef LC_ALL

        else if (category == LC_ALL) {
            STORE_LC_NUMERIC_FORCE_TO_UNDERLYING();
        }

#  endif

    }

#endif

    retval = save_to_buffer(do_setlocale_r(category, locale),
                            &PL_setlocale_buf, &PL_setlocale_bufsize, 0);
    SAVE_ERRNO;

#if defined(USE_LOCALE_NUMERIC) && defined(LC_ALL)

    if (locale == NULL && category == LC_ALL) {
        RESTORE_LC_NUMERIC();
    }

#endif

    DEBUG_L(PerlIO_printf(Perl_debug_log,
        "%s:%d: %s\n", __FILE__, __LINE__,
            setlocale_debug_string(category, locale, retval)));

    RESTORE_ERRNO;

    if (! retval) {
        return NULL;
    }

    /* If locale == NULL, we are just querying the state */
    if (locale == NULL) {
        return retval;
    }

    /* Now that have switched locales, we have to update our records to
     * correspond. */

    switch (category) {

#ifdef USE_LOCALE_CTYPE

        case LC_CTYPE:
            new_ctype(retval);
            break;

#endif
#ifdef USE_LOCALE_COLLATE

        case LC_COLLATE:
            new_collate(retval);
            break;

#endif
#ifdef USE_LOCALE_NUMERIC

        case LC_NUMERIC:
            new_numeric(retval);
            break;

#endif
#ifdef LC_ALL

        case LC_ALL:

            /* LC_ALL updates all the things we care about.  The values may not
             * be the same as 'retval', as the locale "" may have set things
             * individually */

#  ifdef USE_LOCALE_CTYPE

            newlocale = savepv(do_setlocale_c(LC_CTYPE, NULL));
            new_ctype(newlocale);
            Safefree(newlocale);

#  endif /* USE_LOCALE_CTYPE */
#  ifdef USE_LOCALE_COLLATE

            newlocale = savepv(do_setlocale_c(LC_COLLATE, NULL));
            new_collate(newlocale);
            Safefree(newlocale);

#  endif
#  ifdef USE_LOCALE_NUMERIC

            newlocale = savepv(do_setlocale_c(LC_NUMERIC, NULL));
            new_numeric(newlocale);
            Safefree(newlocale);

#  endif /* USE_LOCALE_NUMERIC */
#endif /* LC_ALL */

        default:
            break;
    }

    return retval;

#endif

}

PERL_STATIC_INLINE const char *
S_save_to_buffer(const char * string, char **buf, Size_t *buf_size, const Size_t offset)
{
    /* Copy the NUL-terminated 'string' to 'buf' + 'offset'.  'buf' has size 'buf_size',
     * growing it if necessary */

    Size_t string_size;

    PERL_ARGS_ASSERT_SAVE_TO_BUFFER;

    if (! string) {
        return NULL;
    }

    string_size = strlen(string) + offset + 1;

    if (*buf_size == 0) {
        Newx(*buf, string_size, char);
        *buf_size = string_size;
    }
    else if (string_size > *buf_size) {
        Renew(*buf, string_size, char);
        *buf_size = string_size;
    }

    Copy(string, *buf + offset, string_size - offset, char);
    return *buf;
}

/*

=for apidoc Perl_langinfo

This is an (almost) drop-in replacement for the system C<L<nl_langinfo(3)>>,
taking the same C<item> parameter values, and returning the same information.
But it is more thread-safe than regular C<nl_langinfo()>, and hides the quirks
of Perl's locale handling from your code, and can be used on systems that lack
a native C<nl_langinfo>.

Expanding on these:

=over

=item *

The reason it isn't quite a drop-in replacement is actually an advantage.  The
only difference is that it returns S<C<const char *>>, whereas plain
C<nl_langinfo()> returns S<C<char *>>, but you are (only by documentation)
forbidden to write into the buffer.  By declaring this C<const>, the compiler
enforces this restriction, so if it is violated, you know at compilation time,
rather than getting segfaults at runtime.

=item *

It delivers the correct results for the C<RADIXCHAR> and C<THOUSEP> items,
without you having to write extra code.  The reason for the extra code would be
because these are from the C<LC_NUMERIC> locale category, which is normally
kept set by Perl so that the radix is a dot, and the separator is the empty
string, no matter what the underlying locale is supposed to be, and so to get
the expected results, you have to temporarily toggle into the underlying
locale, and later toggle back.  (You could use plain C<nl_langinfo> and
C<L</STORE_LC_NUMERIC_FORCE_TO_UNDERLYING>> for this but then you wouldn't get
the other advantages of C<Perl_langinfo()>; not keeping C<LC_NUMERIC> in the C
(or equivalent) locale would break a lot of CPAN, which is expecting the radix
(decimal point) character to be a dot.)

=item *

The system function it replaces can have its static return buffer trashed,
not only by a subesequent call to that function, but by a C<freelocale>,
C<setlocale>, or other locale change.  The returned buffer of this function is
not changed until the next call to it, so the buffer is never in a trashed
state.

=item *

Its return buffer is per-thread, so it also is never overwritten by a call to
this function from another thread;  unlike the function it replaces.

=item *

But most importantly, it works on systems that don't have C<nl_langinfo>, such
as Windows, hence makes your code more portable.  Of the fifty-some possible
items specified by the POSIX 2008 standard,
L<http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/langinfo.h.html>,
only one is completely unimplemented, though on non-Windows platforms, another
significant one is also not implemented).  It uses various techniques to
recover the other items, including calling C<L<localeconv(3)>>, and
C<L<strftime(3)>>, both of which are specified in C89, so should be always be
available.  Later C<strftime()> versions have additional capabilities; C<""> is
returned for those not available on your system.

It is important to note that when called with an item that is recovered by
using C<localeconv>, the buffer from any previous explicit call to
C<localeconv> will be overwritten.  This means you must save that buffer's
contents if you need to access them after a call to this function.  (But note
that you might not want to be using C<localeconv()> directly anyway, because of
issues like the ones listed in the second item of this list (above) for
C<RADIXCHAR> and C<THOUSEP>.  You can use the methods given in L<perlcall> to
call L<POSIX/localeconv> and avoid all the issues, but then you have a hash to
unpack).

The details for those items which may deviate from what this emulation returns
and what a native C<nl_langinfo()> would return are specified in
L<I18N::Langinfo>.

=back

When using C<Perl_langinfo> on systems that don't have a native
C<nl_langinfo()>, you must

 #include "perl_langinfo.h"

before the C<perl.h> C<#include>.  You can replace your C<langinfo.h>
C<#include> with this one.  (Doing it this way keeps out the symbols that plain
C<langinfo.h> would try to import into the namespace for code that doesn't need
it.)

The original impetus for C<Perl_langinfo()> was so that code that needs to
find out the current currency symbol, floating point radix character, or digit
grouping separator can use, on all systems, the simpler and more
thread-friendly C<nl_langinfo> API instead of C<L<localeconv(3)>> which is a
pain to make thread-friendly.  For other fields returned by C<localeconv>, it
is better to use the methods given in L<perlcall> to call
L<C<POSIX::localeconv()>|POSIX/localeconv>, which is thread-friendly.

=cut

*/

const char *
#ifdef HAS_NL_LANGINFO
Perl_langinfo(const nl_item item)
#else
Perl_langinfo(const int item)
#endif
{
    return my_nl_langinfo(item, TRUE);
}

STATIC const char *
#ifdef HAS_NL_LANGINFO
S_my_nl_langinfo(const nl_item item, bool toggle)
#else
S_my_nl_langinfo(const int item, bool toggle)
#endif
{
    dTHX;
    const char * retval;

#ifdef USE_LOCALE_NUMERIC

    /* We only need to toggle into the underlying LC_NUMERIC locale for these
     * two items, and only if not already there */
    if (toggle && ((   item != RADIXCHAR && item != THOUSEP)
                    || PL_numeric_underlying))

#endif  /* No toggling needed if not using LC_NUMERIC */

        toggle = FALSE;

#if defined(HAS_NL_LANGINFO) /* nl_langinfo() is available.  */
#  if   ! defined(HAS_THREAD_SAFE_NL_LANGINFO_L)      \
     || ! defined(HAS_POSIX_2008_LOCALE)              \
     || ! defined(DUPLOCALE)

    /* Here, use plain nl_langinfo(), switching to the underlying LC_NUMERIC
     * for those items dependent on it.  This must be copied to a buffer before
     * switching back, as some systems destroy the buffer when setlocale() is
     * called */

    {
        DECLARATION_FOR_LC_NUMERIC_MANIPULATION;

        if (toggle) {
            STORE_LC_NUMERIC_FORCE_TO_UNDERLYING();
        }

        LOCALE_LOCK;    /* Prevent interference from another thread executing
                           this code section (the only call to nl_langinfo in
                           the core) */


        /* Copy to a per-thread buffer, which is also one that won't be
         * destroyed by a subsequent setlocale(), such as the
         * RESTORE_LC_NUMERIC may do just below. */
        retval = save_to_buffer(nl_langinfo(item),
                                &PL_langinfo_buf, &PL_langinfo_bufsize, 0);

        LOCALE_UNLOCK;

        if (toggle) {
            RESTORE_LC_NUMERIC();
        }
    }

#  else /* Use nl_langinfo_l(), avoiding both a mutex and changing the locale */

    {
        bool do_free = FALSE;
        locale_t cur = uselocale((locale_t) 0);

        if (cur == LC_GLOBAL_LOCALE) {
            cur = duplocale(LC_GLOBAL_LOCALE);
            do_free = TRUE;
        }

#    ifdef USE_LOCALE_NUMERIC

        if (toggle) {
            if (PL_underlying_numeric_obj) {
                cur = PL_underlying_numeric_obj;
            }
            else {
                cur = newlocale(LC_NUMERIC_MASK, PL_numeric_name, cur);
                do_free = TRUE;
            }
        }

#    endif

        /* We have to save it to a buffer, because the freelocale() just below
         * can invalidate the internal one */
        retval = save_to_buffer(nl_langinfo_l(item, cur),
                                &PL_langinfo_buf, &PL_langinfo_bufsize, 0);

        if (do_free) {
            freelocale(cur);
        }
    }

#  endif

    if (strEQ(retval, "")) {
        if (item == YESSTR) {
            return "yes";
        }
        if (item == NOSTR) {
            return "no";
        }
    }

    return retval;

#else   /* Below, emulate nl_langinfo as best we can */

    {

#  ifdef HAS_LOCALECONV

        const struct lconv* lc;
        const char * temp;
        DECLARATION_FOR_LC_NUMERIC_MANIPULATION;

#    ifdef TS_W32_BROKEN_LOCALECONV

        const char * save_global;
        const char * save_thread;
        int needed_size;
        char * ptr;
        char * e;
        char * item_start;

#    endif
#  endif
#  ifdef HAS_STRFTIME

        struct tm tm;
        bool return_format = FALSE; /* Return the %format, not the value */
        const char * format;

#  endif

        /* We copy the results to a per-thread buffer, even if not
         * multi-threaded.  This is in part to simplify this code, and partly
         * because we need a buffer anyway for strftime(), and partly because a
         * call of localeconv() could otherwise wipe out the buffer, and the
         * programmer would not be expecting this, as this is a nl_langinfo()
         * substitute after all, so s/he might be thinking their localeconv()
         * is safe until another localeconv() call. */

        switch (item) {
            Size_t len;

            /* This is unimplemented */
            case ERA:      /* For use with strftime() %E modifier */

            default:
                return "";

            /* We use only an English set, since we don't know any more */
            case YESEXPR:   return "^[+1yY]";
            case YESSTR:    return "yes";
            case NOEXPR:    return "^[-0nN]";
            case NOSTR:     return "no";

            case CODESET:

#  ifndef WIN32

                /* On non-windows, this is unimplemented, in part because of
                 * inconsistencies between vendors.  The Darwin native
                 * nl_langinfo() implementation simply looks at everything past
                 * any dot in the name, but that doesn't work for other
                 * vendors.  Many Linux locales that don't have UTF-8 in their
                 * names really are UTF-8, for example; z/OS locales that do
                 * have UTF-8 in their names, aren't really UTF-8 */
                return "";

#  else

                {   /* But on Windows, the name does seem to be consistent, so
                       use that. */
                    const char * p;
                    const char * first;
                    Size_t offset = 0;
                    const char * name = my_setlocale(LC_CTYPE, NULL);

                    if (isNAME_C_OR_POSIX(name)) {
                        return "ANSI_X3.4-1968";
                    }

                    /* Find the dot in the locale name */
                    first = (const char *) strchr(name, '.');
                    if (! first) {
                        first = name;
                        goto has_nondigit;
                    }

                    /* Look at everything past the dot */
                    first++;
                    p = first;

                    while (*p) {
                        if (! isDIGIT(*p)) {
                            goto has_nondigit;
                        }

                        p++;
                    }

                    /* Here everything past the dot is a digit.  Treat it as a
                     * code page */
                    retval = save_to_buffer("CP", &PL_langinfo_buf,
                                                &PL_langinfo_bufsize, 0);
                    offset = STRLENs("CP");

                  has_nondigit:

                    retval = save_to_buffer(first, &PL_langinfo_buf,
                                            &PL_langinfo_bufsize, offset);
                }

                break;

#  endif
#  ifdef HAS_LOCALECONV

            case CRNCYSTR:

                /* We don't bother with localeconv_l() because any system that
                 * has it is likely to also have nl_langinfo() */

                LOCALE_LOCK_V;    /* Prevent interference with other threads
                                     using localeconv() */

#    ifdef TS_W32_BROKEN_LOCALECONV

                /* This is a workaround for a Windows bug prior to VS 15.
                 * What we do here is, while locked, switch to the global
                 * locale so localeconv() works; then switch back just before
                 * the unlock.  This can screw things up if some thread is
                 * already using the global locale while assuming no other is.
                 * A different workaround would be to call GetCurrencyFormat on
                 * a known value, and parse it; patches welcome
                 *
                 * We have to use LC_ALL instead of LC_MONETARY because of
                 * another bug in Windows */

                save_thread = savepv(my_setlocale(LC_ALL, NULL));
                _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
                save_global= savepv(my_setlocale(LC_ALL, NULL));
                my_setlocale(LC_ALL, save_thread);

#    endif

                lc = localeconv();
                if (   ! lc
                    || ! lc->currency_symbol
                    || strEQ("", lc->currency_symbol))
                {
                    LOCALE_UNLOCK_V;
                    return "";
                }

                /* Leave the first spot empty to be filled in below */
                retval = save_to_buffer(lc->currency_symbol, &PL_langinfo_buf,
                                        &PL_langinfo_bufsize, 1);
                if (lc->mon_decimal_point && strEQ(lc->mon_decimal_point, ""))
                { /*  khw couldn't figure out how the localedef specifications
                      would show that the $ should replace the radix; this is
                      just a guess as to how it might work.*/
                    PL_langinfo_buf[0] = '.';
                }
                else if (lc->p_cs_precedes) {
                    PL_langinfo_buf[0] = '-';
                }
                else {
                    PL_langinfo_buf[0] = '+';
                }

#    ifdef TS_W32_BROKEN_LOCALECONV

                my_setlocale(LC_ALL, save_global);
                _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
                my_setlocale(LC_ALL, save_thread);
                Safefree(save_global);
                Safefree(save_thread);

#    endif

                LOCALE_UNLOCK_V;
                break;

#    ifdef TS_W32_BROKEN_LOCALECONV

            case RADIXCHAR:

                /* For this, we output a known simple floating point number to
                 * a buffer, and parse it, looking for the radix */

                if (toggle) {
                    STORE_LC_NUMERIC_FORCE_TO_UNDERLYING();
                }

                if (PL_langinfo_bufsize < 10) {
                    PL_langinfo_bufsize = 10;
                    Renew(PL_langinfo_buf, PL_langinfo_bufsize, char);
                }

                needed_size = my_snprintf(PL_langinfo_buf, PL_langinfo_bufsize,
                                          "%.1f", 1.5);
                if (needed_size >= (int) PL_langinfo_bufsize) {
                    PL_langinfo_bufsize = needed_size + 1;
                    Renew(PL_langinfo_buf, PL_langinfo_bufsize, char);
                    needed_size = my_snprintf(PL_langinfo_buf, PL_langinfo_bufsize,
                                             "%.1f", 1.5);
                    assert(needed_size < (int) PL_langinfo_bufsize);
                }

                ptr = PL_langinfo_buf;
                e = PL_langinfo_buf + PL_langinfo_bufsize;

                /* Find the '1' */
                while (ptr < e && *ptr != '1') {
                    ptr++;
                }
                ptr++;

                /* Find the '5' */
                item_start = ptr;
                while (ptr < e && *ptr != '5') {
                    ptr++;
                }

                /* Everything in between is the radix string */
                if (ptr >= e) {
                    PL_langinfo_buf[0] = '?';
                    PL_langinfo_buf[1] = '\0';
                }
                else {
                    *ptr = '\0';
                    Move(item_start, PL_langinfo_buf, ptr - PL_langinfo_buf, char);
                }

                if (toggle) {
                    RESTORE_LC_NUMERIC();
                }

                retval = PL_langinfo_buf;
                break;

#    else

            case RADIXCHAR:     /* No special handling needed */

#    endif

            case THOUSEP:

                if (toggle) {
                    STORE_LC_NUMERIC_FORCE_TO_UNDERLYING();
                }

                LOCALE_LOCK_V;    /* Prevent interference with other threads
                                     using localeconv() */

#    ifdef TS_W32_BROKEN_LOCALECONV

                /* This should only be for the thousands separator.  A
                 * different work around would be to use GetNumberFormat on a
                 * known value and parse the result to find the separator */
                save_thread = savepv(my_setlocale(LC_ALL, NULL));
                _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
                save_global = savepv(my_setlocale(LC_ALL, NULL));
                my_setlocale(LC_ALL, save_thread);
#      if 0
                /* This is the start of code that for broken Windows replaces
                 * the above and below code, and instead calls
                 * GetNumberFormat() and then would parse that to find the
                 * thousands separator.  It needs to handle UTF-16 vs -8
                 * issues. */

                needed_size = GetNumberFormatEx(PL_numeric_name, 0, "1234.5", NULL, PL_langinfo_buf, PL_langinfo_bufsize);
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                    "%s: %d: return from GetNumber, count=%d, val=%s\n",
                    __FILE__, __LINE__, needed_size, PL_langinfo_buf));

#      endif
#    endif

                lc = localeconv();
                if (! lc) {
                    temp = "";
                }
                else {
                    temp = (item == RADIXCHAR)
                             ? lc->decimal_point
                             : lc->thousands_sep;
                    if (! temp) {
                        temp = "";
                    }
                }

                retval = save_to_buffer(temp, &PL_langinfo_buf,
                                        &PL_langinfo_bufsize, 0);

#    ifdef TS_W32_BROKEN_LOCALECONV

                my_setlocale(LC_ALL, save_global);
                _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
                my_setlocale(LC_ALL, save_thread);
                Safefree(save_global);
                Safefree(save_thread);

#    endif

                LOCALE_UNLOCK_V;

                if (toggle) {
                    RESTORE_LC_NUMERIC();
                }

                break;

#  endif
#  ifdef HAS_STRFTIME

            /* These are defined by C89, so we assume that strftime supports
             * them, and so are returned unconditionally; they may not be what
             * the locale actually says, but should give good enough results
             * for someone using them as formats (as opposed to trying to parse
             * them to figure out what the locale says).  The other format
             * items are actually tested to verify they work on the platform */
            case D_FMT:         return "%x";
            case T_FMT:         return "%X";
            case D_T_FMT:       return "%c";

            /* These formats are only available in later strfmtime's */
            case ERA_D_FMT: case ERA_T_FMT: case ERA_D_T_FMT: case T_FMT_AMPM:

            /* The rest can be gotten from most versions of strftime(). */
            case ABDAY_1: case ABDAY_2: case ABDAY_3:
            case ABDAY_4: case ABDAY_5: case ABDAY_6: case ABDAY_7:
            case ALT_DIGITS:
            case AM_STR: case PM_STR:
            case ABMON_1: case ABMON_2: case ABMON_3: case ABMON_4:
            case ABMON_5: case ABMON_6: case ABMON_7: case ABMON_8:
            case ABMON_9: case ABMON_10: case ABMON_11: case ABMON_12:
            case DAY_1: case DAY_2: case DAY_3: case DAY_4:
            case DAY_5: case DAY_6: case DAY_7:
            case MON_1: case MON_2: case MON_3: case MON_4:
            case MON_5: case MON_6: case MON_7: case MON_8:
            case MON_9: case MON_10: case MON_11: case MON_12:

                LOCALE_LOCK;

                init_tm(&tm);   /* Precaution against core dumps */
                tm.tm_sec = 30;
                tm.tm_min = 30;
                tm.tm_hour = 6;
                tm.tm_year = 2017 - 1900;
                tm.tm_wday = 0;
                tm.tm_mon = 0;
                switch (item) {
                    default:
                        LOCALE_UNLOCK;
                        Perl_croak(aTHX_
                                    "panic: %s: %d: switch case: %d problem",
                                       __FILE__, __LINE__, item);
                        NOT_REACHED; /* NOTREACHED */

                    case PM_STR: tm.tm_hour = 18;
                    case AM_STR:
                        format = "%p";
                        break;

                    case ABDAY_7: tm.tm_wday++;
                    case ABDAY_6: tm.tm_wday++;
                    case ABDAY_5: tm.tm_wday++;
                    case ABDAY_4: tm.tm_wday++;
                    case ABDAY_3: tm.tm_wday++;
                    case ABDAY_2: tm.tm_wday++;
                    case ABDAY_1:
                        format = "%a";
                        break;

                    case DAY_7: tm.tm_wday++;
                    case DAY_6: tm.tm_wday++;
                    case DAY_5: tm.tm_wday++;
                    case DAY_4: tm.tm_wday++;
                    case DAY_3: tm.tm_wday++;
                    case DAY_2: tm.tm_wday++;
                    case DAY_1:
                        format = "%A";
                        break;

                    case ABMON_12: tm.tm_mon++;
                    case ABMON_11: tm.tm_mon++;
                    case ABMON_10: tm.tm_mon++;
                    case ABMON_9: tm.tm_mon++;
                    case ABMON_8: tm.tm_mon++;
                    case ABMON_7: tm.tm_mon++;
                    case ABMON_6: tm.tm_mon++;
                    case ABMON_5: tm.tm_mon++;
                    case ABMON_4: tm.tm_mon++;
                    case ABMON_3: tm.tm_mon++;
                    case ABMON_2: tm.tm_mon++;
                    case ABMON_1:
                        format = "%b";
                        break;

                    case MON_12: tm.tm_mon++;
                    case MON_11: tm.tm_mon++;
                    case MON_10: tm.tm_mon++;
                    case MON_9: tm.tm_mon++;
                    case MON_8: tm.tm_mon++;
                    case MON_7: tm.tm_mon++;
                    case MON_6: tm.tm_mon++;
                    case MON_5: tm.tm_mon++;
                    case MON_4: tm.tm_mon++;
                    case MON_3: tm.tm_mon++;
                    case MON_2: tm.tm_mon++;
                    case MON_1:
                        format = "%B";
                        break;

                    case T_FMT_AMPM:
                        format = "%r";
                        return_format = TRUE;
                        break;

                    case ERA_D_FMT:
                        format = "%Ex";
                        return_format = TRUE;
                        break;

                    case ERA_T_FMT:
                        format = "%EX";
                        return_format = TRUE;
                        break;

                    case ERA_D_T_FMT:
                        format = "%Ec";
                        return_format = TRUE;
                        break;

                    case ALT_DIGITS:
                        tm.tm_wday = 0;
                        format = "%Ow";	/* Find the alternate digit for 0 */
                        break;
                }

                /* We can't use my_strftime() because it doesn't look at
                 * tm_wday  */
                while (0 == strftime(PL_langinfo_buf, PL_langinfo_bufsize,
                                     format, &tm))
                {
                    /* A zero return means one of:
                     *  a)  there wasn't enough space in PL_langinfo_buf
                     *  b)  the format, like a plain %p, returns empty
                     *  c)  it was an illegal format, though some
                     *      implementations of strftime will just return the
                     *      illegal format as a plain character sequence.
                     *
                     *  To quickly test for case 'b)', try again but precede
                     *  the format with a plain character.  If that result is
                     *  still empty, the problem is either 'a)' or 'c)' */

                    Size_t format_size = strlen(format) + 1;
                    Size_t mod_size = format_size + 1;
                    char * mod_format;
                    char * temp_result;

                    Newx(mod_format, mod_size, char);
                    Newx(temp_result, PL_langinfo_bufsize, char);
                    *mod_format = ' ';
                    my_strlcpy(mod_format + 1, format, mod_size);
                    len = strftime(temp_result,
                                   PL_langinfo_bufsize,
                                   mod_format, &tm);
                    Safefree(mod_format);
                    Safefree(temp_result);

                    /* If 'len' is non-zero, it means that we had a case like
                     * %p which means the current locale doesn't use a.m. or
                     * p.m., and that is valid */
                    if (len == 0) {

                        /* Here, still didn't work.  If we get well beyond a
                         * reasonable size, bail out to prevent an infinite
                         * loop. */

                        if (PL_langinfo_bufsize > 100 * format_size) {
                            *PL_langinfo_buf = '\0';
                        }
                        else {
                            /* Double the buffer size to retry;  Add 1 in case
                             * original was 0, so we aren't stuck at 0.  */
                            PL_langinfo_bufsize *= 2;
                            PL_langinfo_bufsize++;
                            Renew(PL_langinfo_buf, PL_langinfo_bufsize, char);
                            continue;
                        }
                    }

                    break;
                }

                /* Here, we got a result.
                 *
                 * If the item is 'ALT_DIGITS', PL_langinfo_buf contains the
                 * alternate format for wday 0.  If the value is the same as
                 * the normal 0, there isn't an alternate, so clear the buffer.
                 * */
                if (   item == ALT_DIGITS
                    && strEQ(PL_langinfo_buf, "0"))
                {
                    *PL_langinfo_buf = '\0';
                }

                /* ALT_DIGITS is problematic.  Experiments on it showed that
                 * strftime() did not always work properly when going from
                 * alt-9 to alt-10.  Only a few locales have this item defined,
                 * and in all of them on Linux that khw was able to find,
                 * nl_langinfo() merely returned the alt-0 character, possibly
                 * doubled.  Most Unicode digits are in blocks of 10
                 * consecutive code points, so that is sufficient information
                 * for those scripts, as we can infer alt-1, alt-2, ....  But
                 * for a Japanese locale, a CJK ideographic 0 is returned, and
                 * the CJK digits are not in code point order, so you can't
                 * really infer anything.  The localedef for this locale did
                 * specify the succeeding digits, so that strftime() works
                 * properly on them, without needing to infer anything.  But
                 * the nl_langinfo() return did not give sufficient information
                 * for the caller to understand what's going on.  So until
                 * there is evidence that it should work differently, this
                 * returns the alt-0 string for ALT_DIGITS.
                 *
                 * wday was chosen because its range is all a single digit.
                 * Things like tm_sec have two digits as the minimum: '00' */

                LOCALE_UNLOCK;

                retval = PL_langinfo_buf;

                /* If to return the format, not the value, overwrite the buffer
                 * with it.  But some strftime()s will keep the original format
                 * if illegal, so change those to "" */
                if (return_format) {
                    if (strEQ(PL_langinfo_buf, format)) {
                        *PL_langinfo_buf = '\0';
                    }
                    else {
                        retval = save_to_buffer(format, &PL_langinfo_buf,
                                                &PL_langinfo_bufsize, 0);
                    }
                }

                break;

#  endif

        }
    }

    return retval;

#endif

}

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
     *
     * Under -DDEBUGGING, if the environment variable PERL_DEBUG_LOCALE_INIT is
     * set, debugging information is output.
     *
     * This looks more complicated than it is, mainly due to the #ifdefs.
     *
     * We try to set LC_ALL to the value determined by the environment.  If
     * there is no LC_ALL on this platform, we try the individual categories we
     * know about.  If this works, we are done.
     *
     * But if it doesn't work, we have to do something else.  We search the
     * environment variables ourselves instead of relying on the system to do
     * it.  We look at, in order, LC_ALL, LANG, a system default locale (if we
     * think there is one), and the ultimate fallback "C".  This is all done in
     * the same loop as above to avoid duplicating code, but it makes things
     * more complex.  The 'trial_locales' array is initialized with just one
     * element; it causes the behavior described in the paragraph above this to
     * happen.  If that fails, we add elements to 'trial_locales', and do extra
     * loop iterations to cause the behavior described in this paragraph.
     *
     * On Ultrix, the locale MUST come from the environment, so there is
     * preliminary code to set it.  I (khw) am not sure that it is necessary,
     * and that this couldn't be folded into the loop, but barring any real
     * platforms to test on, it's staying as-is
     *
     * A slight complication is that in embedded Perls, the locale may already
     * be set-up, and we don't want to get it from the normal environment
     * variables.  This is handled by having a special environment variable
     * indicate we're in this situation.  We simply set setlocale's 2nd
     * parameter to be a NULL instead of "".  That indicates to setlocale that
     * it is not to change anything, but to return the current value,
     * effectively initializing perl's db to what the locale already is.
     *
     * We play the same trick with NULL if a LC_ALL succeeds.  We call
     * setlocale() on the individual categores with NULL to get their existing
     * values for our db, instead of trying to change them.
     * */

    dVAR;

    int ok = 1;

#ifndef USE_LOCALE

    PERL_UNUSED_ARG(printwarn);

#else  /* USE_LOCALE */
#  ifdef __GLIBC__

    const char * const language   = savepv(PerlEnv_getenv("LANGUAGE"));

#  endif

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

    /* A later getenv() could zap this, so only use here */
    const char * const bad_lang_use_once = PerlEnv_getenv("PERL_BADLANG");

    const bool locwarn = (printwarn > 1
                          || (          printwarn
                              && (    ! bad_lang_use_once
                                  || (
                                         /* disallow with "" or "0" */
                                         *bad_lang_use_once
                                       && strNE("0", bad_lang_use_once)))));

    /* setlocale() return vals; not copied so must be looked at immediately */
    const char * sl_result[NOMINAL_LC_ALL_INDEX + 1];

    /* current locale for given category; should have been copied so aren't
     * volatile */
    const char * curlocales[NOMINAL_LC_ALL_INDEX + 1];

#  ifdef WIN32

    /* In some systems you can find out the system default locale
     * and use that as the fallback locale. */
#    define SYSTEM_DEFAULT_LOCALE
#  endif
#  ifdef SYSTEM_DEFAULT_LOCALE

    const char *system_default_locale = NULL;

#  endif

#  ifndef DEBUGGING
#    define DEBUG_LOCALE_INIT(a,b,c)
#  else

    DEBUG_INITIALIZATION_set(cBOOL(PerlEnv_getenv("PERL_DEBUG_LOCALE_INIT")));

#    define DEBUG_LOCALE_INIT(category, locale, result)                     \
	STMT_START {                                                        \
		if (debug_initialization) {                                 \
                    PerlIO_printf(Perl_debug_log,                           \
                                  "%s:%d: %s\n",                            \
                                  __FILE__, __LINE__,                       \
                                  setlocale_debug_string(category,          \
                                                          locale,           \
                                                          result));         \
                }                                                           \
	} STMT_END

/* Make sure the parallel arrays are properly set up */
#    ifdef USE_LOCALE_NUMERIC
    assert(categories[LC_NUMERIC_INDEX] == LC_NUMERIC);
    assert(strEQ(category_names[LC_NUMERIC_INDEX], "LC_NUMERIC"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_NUMERIC_INDEX] == LC_NUMERIC_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_CTYPE
    assert(categories[LC_CTYPE_INDEX] == LC_CTYPE);
    assert(strEQ(category_names[LC_CTYPE_INDEX], "LC_CTYPE"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_CTYPE_INDEX] == LC_CTYPE_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_COLLATE
    assert(categories[LC_COLLATE_INDEX] == LC_COLLATE);
    assert(strEQ(category_names[LC_COLLATE_INDEX], "LC_COLLATE"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_COLLATE_INDEX] == LC_COLLATE_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_TIME
    assert(categories[LC_TIME_INDEX] == LC_TIME);
    assert(strEQ(category_names[LC_TIME_INDEX], "LC_TIME"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_TIME_INDEX] == LC_TIME_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_MESSAGES
    assert(categories[LC_MESSAGES_INDEX] == LC_MESSAGES);
    assert(strEQ(category_names[LC_MESSAGES_INDEX], "LC_MESSAGES"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_MESSAGES_INDEX] == LC_MESSAGES_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_MONETARY
    assert(categories[LC_MONETARY_INDEX] == LC_MONETARY);
    assert(strEQ(category_names[LC_MONETARY_INDEX], "LC_MONETARY"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_MONETARY_INDEX] == LC_MONETARY_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_ADDRESS
    assert(categories[LC_ADDRESS_INDEX] == LC_ADDRESS);
    assert(strEQ(category_names[LC_ADDRESS_INDEX], "LC_ADDRESS"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_ADDRESS_INDEX] == LC_ADDRESS_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_IDENTIFICATION
    assert(categories[LC_IDENTIFICATION_INDEX] == LC_IDENTIFICATION);
    assert(strEQ(category_names[LC_IDENTIFICATION_INDEX], "LC_IDENTIFICATION"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_IDENTIFICATION_INDEX] == LC_IDENTIFICATION_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_MEASUREMENT
    assert(categories[LC_MEASUREMENT_INDEX] == LC_MEASUREMENT);
    assert(strEQ(category_names[LC_MEASUREMENT_INDEX], "LC_MEASUREMENT"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_MEASUREMENT_INDEX] == LC_MEASUREMENT_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_PAPER
    assert(categories[LC_PAPER_INDEX] == LC_PAPER);
    assert(strEQ(category_names[LC_PAPER_INDEX], "LC_PAPER"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_PAPER_INDEX] == LC_PAPER_MASK);
#      endif
#    endif
#    ifdef USE_LOCALE_TELEPHONE
    assert(categories[LC_TELEPHONE_INDEX] == LC_TELEPHONE);
    assert(strEQ(category_names[LC_TELEPHONE_INDEX], "LC_TELEPHONE"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_TELEPHONE_INDEX] == LC_TELEPHONE_MASK);
#      endif
#    endif
#    ifdef LC_ALL
    assert(categories[LC_ALL_INDEX] == LC_ALL);
    assert(strEQ(category_names[LC_ALL_INDEX], "LC_ALL"));
    assert(NOMINAL_LC_ALL_INDEX == LC_ALL_INDEX);
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_ALL_INDEX] == LC_ALL_MASK);
#      endif
#    endif
#  endif    /* DEBUGGING */

    /* Initialize the cache of the program's UTF-8ness for the always known
     * locales C and POSIX */
    my_strlcpy(PL_locale_utf8ness, C_and_POSIX_utf8ness,
               sizeof(PL_locale_utf8ness));

#  ifdef USE_THREAD_SAFE_LOCALE
#    ifdef WIN32

    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);

#    endif
#  endif
#  ifdef USE_POSIX_2008_LOCALE

    PL_C_locale_obj = newlocale(LC_ALL_MASK, "C", (locale_t) 0);
    if (! PL_C_locale_obj) {
        Perl_croak_nocontext(
            "panic: Cannot create POSIX 2008 C locale object; errno=%d", errno);
    }
    if (DEBUG_Lv_TEST || debug_initialization) {
        PerlIO_printf(Perl_debug_log, "%s:%d: created C object %p\n", __FILE__, __LINE__, PL_C_locale_obj);
    }

#  endif

#  ifdef USE_LOCALE_NUMERIC

    PL_numeric_radix_sv = newSVpvs(".");

#  endif

#  if defined(USE_POSIX_2008_LOCALE) && ! defined(HAS_QUERYLOCALE)

    /* Initialize our records.  If we have POSIX 2008, we have LC_ALL */
    do_setlocale_c(LC_ALL, my_setlocale(LC_ALL, NULL));

#  endif
#  ifdef LOCALE_ENVIRON_REQUIRED

    /*
     * Ultrix setlocale(..., "") fails if there are no environment
     * variables from which to get a locale name.
     */

#    ifndef LC_ALL
#      error Ultrix without LC_ALL not implemented
#    else

    {
        bool done = FALSE;
        if (lang) {
            sl_result[LC_ALL_INDEX] = do_setlocale_c(LC_ALL, setlocale_init);
            DEBUG_LOCALE_INIT(LC_ALL, setlocale_init, sl_result[LC_ALL_INDEX]);
            if (sl_result[LC_ALL_INDEX])
                done = TRUE;
            else
                setlocale_failure = TRUE;
        }
        if (! setlocale_failure) {
            const char * locale_param;
            for (i = 0; i < LC_ALL_INDEX; i++) {
                locale_param = (! done && (lang || PerlEnv_getenv(category_names[i])))
                            ? setlocale_init
                            : NULL;
                sl_result[i] = do_setlocale_r(categories[i], locale_param);
                if (! sl_result[i]) {
                    setlocale_failure = TRUE;
                }
                DEBUG_LOCALE_INIT(categories[i], locale_param, sl_result[i]);
            }
        }
    }

#    endif /* LC_ALL */
#  endif /* LOCALE_ENVIRON_REQUIRED */

    /* We try each locale in the list until we get one that works, or exhaust
     * the list.  Normally the loop is executed just once.  But if setting the
     * locale fails, inside the loop we add fallback trials to the array and so
     * will execute the loop multiple times */
    trial_locales[0] = setlocale_init;
    trial_locales_count = 1;

    for (i= 0; i < trial_locales_count; i++) {
        const char * trial_locale = trial_locales[i];

        if (i > 0) {

            /* XXX This is to preserve old behavior for LOCALE_ENVIRON_REQUIRED
             * when i==0, but I (khw) don't think that behavior makes much
             * sense */
            setlocale_failure = FALSE;

#  ifdef SYSTEM_DEFAULT_LOCALE
#    ifdef WIN32    /* Note that assumes Win32 has LC_ALL */

            /* On Windows machines, an entry of "" after the 0th means to use
             * the system default locale, which we now proceed to get. */
            if (strEQ(trial_locale, "")) {
                unsigned int j;

                /* Note that this may change the locale, but we are going to do
                 * that anyway just below */
                system_default_locale = do_setlocale_c(LC_ALL, "");
                DEBUG_LOCALE_INIT(LC_ALL, "", system_default_locale);

                /* Skip if invalid or if it's already on the list of locales to
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
#    else
#      error SYSTEM_DEFAULT_LOCALE only implemented for Win32
#    endif
#  endif /* SYSTEM_DEFAULT_LOCALE */

        }   /* For i > 0 */

#  ifdef LC_ALL

        sl_result[LC_ALL_INDEX] = do_setlocale_c(LC_ALL, trial_locale);
        DEBUG_LOCALE_INIT(LC_ALL, trial_locale, sl_result[LC_ALL_INDEX]);
        if (! sl_result[LC_ALL_INDEX]) {
            setlocale_failure = TRUE;
        }
        else {
            /* Since LC_ALL succeeded, it should have changed all the other
             * categories it can to its value; so we massage things so that the
             * setlocales below just return their category's current values.
             * This adequately handles the case in NetBSD where LC_COLLATE may
             * not be defined for a locale, and setting it individually will
             * fail, whereas setting LC_ALL succeeds, leaving LC_COLLATE set to
             * the POSIX locale. */
            trial_locale = NULL;
        }

#  endif /* LC_ALL */

        if (! setlocale_failure) {
            unsigned int j;
            for (j = 0; j < NOMINAL_LC_ALL_INDEX; j++) {
                curlocales[j]
                        = savepv(do_setlocale_r(categories[j], trial_locale));
                if (! curlocales[j]) {
                    setlocale_failure = TRUE;
                }
                DEBUG_LOCALE_INIT(categories[j], trial_locale, curlocales[j]);
            }

            if (! setlocale_failure) {  /* All succeeded */
                break;  /* Exit trial_locales loop */
            }
        }

        /* Here, something failed; will need to try a fallback. */
        ok = 0;

        if (i == 0) {
            unsigned int j;

            if (locwarn) { /* Output failure info only on the first one */

#  ifdef LC_ALL

                PerlIO_printf(Perl_error_log,
                "perl: warning: Setting locale failed.\n");

#  else /* !LC_ALL */

                PerlIO_printf(Perl_error_log,
                "perl: warning: Setting locale failed for the categories:\n\t");

                for (j = 0; j < NOMINAL_LC_ALL_INDEX; j++) {
                    if (! curlocales[j]) {
                        PerlIO_printf(Perl_error_log, category_names[j]);
                    }
                    else {
                        Safefree(curlocales[j]);
                    }
                }

#  endif /* LC_ALL */

                PerlIO_printf(Perl_error_log,
                    "perl: warning: Please check that your locale settings:\n");

#  ifdef __GLIBC__

                PerlIO_printf(Perl_error_log,
                            "\tLANGUAGE = %c%s%c,\n",
                            language ? '"' : '(',
                            language ? language : "unset",
                            language ? '"' : ')');
#  endif

                PerlIO_printf(Perl_error_log,
                            "\tLC_ALL = %c%s%c,\n",
                            lc_all ? '"' : '(',
                            lc_all ? lc_all : "unset",
                            lc_all ? '"' : ')');

#  if defined(USE_ENVIRON_ARRAY)

                {
                    char **e;

                    /* Look through the environment for any variables of the
                     * form qr/ ^ LC_ [A-Z]+ = /x, except LC_ALL which was
                     * already handled above.  These are assumed to be locale
                     * settings.  Output them and their values. */
                    for (e = environ; *e; e++) {
                        const STRLEN prefix_len = sizeof("LC_") - 1;
                        STRLEN uppers_len;

                        if (     strBEGINs(*e, "LC_")
                            && ! strBEGINs(*e, "LC_ALL=")
                            && (uppers_len = strspn(*e + prefix_len,
                                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
                            && ((*e)[prefix_len + uppers_len] == '='))
                        {
                            PerlIO_printf(Perl_error_log, "\t%.*s = \"%s\",\n",
                                (int) (prefix_len + uppers_len), *e,
                                *e + prefix_len + uppers_len + 1);
                        }
                    }
                }

#  else

                PerlIO_printf(Perl_error_log,
                            "\t(possibly more locale environment variables)\n");

#  endif

                PerlIO_printf(Perl_error_log,
                            "\tLANG = %c%s%c\n",
                            lang ? '"' : '(',
                            lang ? lang : "unset",
                            lang ? '"' : ')');

                PerlIO_printf(Perl_error_log,
                            "    are supported and installed on your system.\n");
            }

            /* Calculate what fallback locales to try.  We have avoided this
             * until we have to, because failure is quite unlikely.  This will
             * usually change the upper bound of the loop we are in.
             *
             * Since the system's default way of setting the locale has not
             * found one that works, We use Perl's defined ordering: LC_ALL,
             * LANG, and the C locale.  We don't try the same locale twice, so
             * don't add to the list if already there.  (On POSIX systems, the
             * LC_ALL element will likely be a repeat of the 0th element "",
             * but there's no harm done by doing it explicitly.
             *
             * Note that this tries the LC_ALL environment variable even on
             * systems which have no LC_ALL locale setting.  This may or may
             * not have been originally intentional, but there's no real need
             * to change the behavior. */
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

#  if defined(WIN32) && defined(LC_ALL)

            /* For Windows, we also try the system default locale before "C".
             * (If there exists a Windows without LC_ALL we skip this because
             * it gets too complicated.  For those, the "C" is the next
             * fallback possibility).  The "" is the same as the 0th element of
             * the array, but the code at the loop above knows to treat it
             * differently when not the 0th */
            trial_locales[trial_locales_count++] = "";

#  endif

            for (j = 0; j < trial_locales_count; j++) {
                if (strEQ("C", trial_locales[j])) {
                    goto done_C;
                }
            }
            trial_locales[trial_locales_count++] = "C";

          done_C: ;
        }   /* end of first time through the loop */

#  ifdef WIN32

      next_iteration: ;

#  endif

    }   /* end of looping through the trial locales */

    if (ok < 1) {   /* If we tried to fallback */
        const char* msg;
        if (! setlocale_failure) {  /* fallback succeeded */
           msg = "Falling back to";
        }
        else {  /* fallback failed */
            unsigned int j;

            /* We dropped off the end of the loop, so have to decrement i to
             * get back to the value the last time through */
            i--;

            ok = -1;
            msg = "Failed to fall back to";

            /* To continue, we should use whatever values we've got */

            for (j = 0; j < NOMINAL_LC_ALL_INDEX; j++) {
                Safefree(curlocales[j]);
                curlocales[j] = savepv(do_setlocale_r(categories[j], NULL));
                DEBUG_LOCALE_INIT(categories[j], NULL, curlocales[j]);
            }
        }

        if (locwarn) {
            const char * description;
            const char * name = "";
            if (strEQ(trial_locales[i], "C")) {
                description = "the standard locale";
                name = "C";
            }

#  ifdef SYSTEM_DEFAULT_LOCALE

            else if (strEQ(trial_locales[i], "")) {
                description = "the system default locale";
                if (system_default_locale) {
                    name = system_default_locale;
                }
            }

#  endif /* SYSTEM_DEFAULT_LOCALE */

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

    /* Done with finding the locales; update our records */

#  ifdef USE_LOCALE_CTYPE

    new_ctype(curlocales[LC_CTYPE_INDEX]);

#  endif
#  ifdef USE_LOCALE_COLLATE

    new_collate(curlocales[LC_COLLATE_INDEX]);

#  endif
#  ifdef USE_LOCALE_NUMERIC

    new_numeric(curlocales[LC_NUMERIC_INDEX]);

#  endif

    for (i = 0; i < NOMINAL_LC_ALL_INDEX; i++) {

#  if defined(USE_ITHREADS) && ! defined(USE_THREAD_SAFE_LOCALE)

        /* This caches whether each category's locale is UTF-8 or not.  This
         * may involve changing the locale.  It is ok to do this at
         * initialization time before any threads have started, but not later
         * unless thread-safe operations are used.
         * Caching means that if the program heeds our dictate not to change
         * locales in threaded applications, this data will remain valid, and
         * it may get queried without having to change locales.  If the
         * environment is such that all categories have the same locale, this
         * isn't needed, as the code will not change the locale; but this
         * handles the uncommon case where the environment has disparate
         * locales for the categories */
        (void) _is_cur_LC_category_utf8(categories[i]);

#  endif

        Safefree(curlocales[i]);
    }

#  if defined(USE_PERLIO) && defined(USE_LOCALE_CTYPE)

    /* Set PL_utf8locale to TRUE if using PerlIO _and_ the current LC_CTYPE
     * locale is UTF-8.  The call to new_ctype() just above has already
     * calculated the latter value and saved it in PL_in_utf8_CTYPE_locale. If
     * both PL_utf8locale and PL_unicode (set by -C or by $ENV{PERL_UNICODE})
     * are true, perl.c:S_parse_body() will turn on the PerlIO :utf8 layer on
     * STDIN, STDOUT, STDERR, _and_ the default open discipline.  */
    PL_utf8locale = PL_in_utf8_CTYPE_locale;

    /* Set PL_unicode to $ENV{PERL_UNICODE} if using PerlIO.
       This is an alternative to using the -C command line switch
       (the -C if present will override this). */
    {
	 const char *p = PerlEnv_getenv("PERL_UNICODE");
	 PL_unicode = p ? parse_unicode_opts(&p) : 0;
	 if (PL_unicode & PERL_UNICODE_UTF8CACHEASSERT_FLAG)
	     PL_utf8cache = -1;
    }

#  endif
#  ifdef __GLIBC__

    Safefree(language);

#  endif

    Safefree(lc_all);
    Safefree(lang);

#endif /* USE_LOCALE */
#ifdef DEBUGGING

    /* So won't continue to output stuff */
    DEBUG_INITIALIZATION_set(FALSE);

#endif

    return ok;
}

#ifdef USE_LOCALE_COLLATE

char *
Perl__mem_collxfrm(pTHX_ const char *input_string,
                         STRLEN len,    /* Length of 'input_string' */
                         STRLEN *xlen,  /* Set to length of returned string
                                           (not including the collation index
                                           prefix) */
                         bool utf8      /* Is the input in UTF-8? */
                   )
{

    /* _mem_collxfrm() is a bit like strxfrm() but with two important
     * differences. First, it handles embedded NULs. Second, it allocates a bit
     * more memory than needed for the transformed data itself.  The real
     * transformed data begins at offset COLLXFRM_HDR_LEN.  *xlen is set to
     * the length of that, and doesn't include the collation index size.
     * Please see sv_collxfrm() to see how this is used. */

#define COLLXFRM_HDR_LEN    sizeof(PL_collation_ix)

    char * s = (char *) input_string;
    STRLEN s_strlen = strlen(input_string);
    char *xbuf = NULL;
    STRLEN xAlloc;          /* xalloc is a reserved word in VC */
    STRLEN length_in_chars;
    bool first_time = TRUE; /* Cleared after first loop iteration */

    PERL_ARGS_ASSERT__MEM_COLLXFRM;

    /* Must be NUL-terminated */
    assert(*(input_string + len) == '\0');

    /* If this locale has defective collation, skip */
    if (PL_collxfrm_base == 0 && PL_collxfrm_mult == 0) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                      "_mem_collxfrm: locale's collation is defective\n"));
        goto bad;
    }

    /* Replace any embedded NULs with the control that sorts before any others.
     * This will give as good as possible results on strings that don't
     * otherwise contain that character, but otherwise there may be
     * less-than-perfect results with that character and NUL.  This is
     * unavoidable unless we replace strxfrm with our own implementation. */
    if (UNLIKELY(s_strlen < len)) {   /* Only execute if there is an embedded
                                         NUL */
        char * e = s + len;
        char * sans_nuls;
        STRLEN sans_nuls_len;
        int try_non_controls;
        char this_replacement_char[] = "?\0";   /* Room for a two-byte string,
                                                   making sure 2nd byte is NUL.
                                                 */
        STRLEN this_replacement_len;

        /* If we don't know what non-NUL control character sorts lowest for
         * this locale, find it */
        if (PL_strxfrm_NUL_replacement == '\0') {
            int j;
            char * cur_min_x = NULL;    /* The min_char's xfrm, (except it also
                                           includes the collation index
                                           prefixed. */

            DEBUG_Lv(PerlIO_printf(Perl_debug_log, "Looking to replace NUL\n"));

            /* Unlikely, but it may be that no control will work to replace
             * NUL, in which case we instead look for any character.  Controls
             * are preferred because collation order is, in general, context
             * sensitive, with adjoining characters affecting the order, and
             * controls are less likely to have such interactions, allowing the
             * NUL-replacement to stand on its own.  (Another way to look at it
             * is to imagine what would happen if the NUL were replaced by a
             * combining character; it wouldn't work out all that well.) */
            for (try_non_controls = 0;
                 try_non_controls < 2;
                 try_non_controls++)
            {
                /* Look through all legal code points (NUL isn't) */
                for (j = 1; j < 256; j++) {
                    char * x;       /* j's xfrm plus collation index */
                    STRLEN x_len;   /* length of 'x' */
                    STRLEN trial_len = 1;
                    char cur_source[] = { '\0', '\0' };

                    /* Skip non-controls the first time through the loop.  The
                     * controls in a UTF-8 locale are the L1 ones */
                    if (! try_non_controls && (PL_in_utf8_COLLATE_locale)
                                               ? ! isCNTRL_L1(j)
                                               : ! isCNTRL_LC(j))
                    {
                        continue;
                    }

                    /* Create a 1-char string of the current code point */
                    cur_source[0] = (char) j;

                    /* Then transform it */
                    x = _mem_collxfrm(cur_source, trial_len, &x_len,
                                      0 /* The string is not in UTF-8 */);

                    /* Ignore any character that didn't successfully transform.
                     * */
                    if (! x) {
                        continue;
                    }

                    /* If this character's transformation is lower than
                     * the current lowest, this one becomes the lowest */
                    if (   cur_min_x == NULL
                        || strLT(x         + COLLXFRM_HDR_LEN,
                                 cur_min_x + COLLXFRM_HDR_LEN))
                    {
                        PL_strxfrm_NUL_replacement = j;
                        Safefree(cur_min_x);
                        cur_min_x = x;
                    }
                    else {
                        Safefree(x);
                    }
                } /* end of loop through all 255 characters */

                /* Stop looking if found */
                if (cur_min_x) {
                    break;
                }

                /* Unlikely, but possible, if there aren't any controls that
                 * work in the locale, repeat the loop, looking for any
                 * character that works */
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                "_mem_collxfrm: No control worked.  Trying non-controls\n"));
            } /* End of loop to try first the controls, then any char */

            if (! cur_min_x) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                    "_mem_collxfrm: Couldn't find any character to replace"
                    " embedded NULs in locale %s with", PL_collation_name));
                goto bad;
            }

            DEBUG_L(PerlIO_printf(Perl_debug_log,
                    "_mem_collxfrm: Replacing embedded NULs in locale %s with "
                    "0x%02X\n", PL_collation_name, PL_strxfrm_NUL_replacement));

            Safefree(cur_min_x);
        } /* End of determining the character that is to replace NULs */

        /* If the replacement is variant under UTF-8, it must match the
         * UTF8-ness of the original */
        if ( ! UVCHR_IS_INVARIANT(PL_strxfrm_NUL_replacement) && utf8) {
            this_replacement_char[0] =
                                UTF8_EIGHT_BIT_HI(PL_strxfrm_NUL_replacement);
            this_replacement_char[1] =
                                UTF8_EIGHT_BIT_LO(PL_strxfrm_NUL_replacement);
            this_replacement_len = 2;
        }
        else {
            this_replacement_char[0] = PL_strxfrm_NUL_replacement;
            /* this_replacement_char[1] = '\0' was done at initialization */
            this_replacement_len = 1;
        }

        /* The worst case length for the replaced string would be if every
         * character in it is NUL.  Multiply that by the length of each
         * replacement, and allow for a trailing NUL */
        sans_nuls_len = (len * this_replacement_len) + 1;
        Newx(sans_nuls, sans_nuls_len, char);
        *sans_nuls = '\0';

        /* Replace each NUL with the lowest collating control.  Loop until have
         * exhausted all the NULs */
        while (s + s_strlen < e) {
            my_strlcat(sans_nuls, s, sans_nuls_len);

            /* Do the actual replacement */
            my_strlcat(sans_nuls, this_replacement_char, sans_nuls_len);

            /* Move past the input NUL */
            s += s_strlen + 1;
            s_strlen = strlen(s);
        }

        /* And add anything that trails the final NUL */
        my_strlcat(sans_nuls, s, sans_nuls_len);

        /* Switch so below we transform this modified string */
        s = sans_nuls;
        len = strlen(s);
    } /* End of replacing NULs */

    /* Make sure the UTF8ness of the string and locale match */
    if (utf8 != PL_in_utf8_COLLATE_locale) {
        /* XXX convert above Unicode to 10FFFF? */
        const char * const t = s;   /* Temporary so we can later find where the
                                       input was */

        /* Here they don't match.  Change the string's to be what the locale is
         * expecting */

        if (! utf8) { /* locale is UTF-8, but input isn't; upgrade the input */
            s = (char *) bytes_to_utf8((const U8 *) s, &len);
            utf8 = TRUE;
        }
        else {   /* locale is not UTF-8; but input is; downgrade the input */

            s = (char *) bytes_from_utf8((const U8 *) s, &len, &utf8);

            /* If the downgrade was successful we are done, but if the input
             * contains things that require UTF-8 to represent, have to do
             * damage control ... */
            if (UNLIKELY(utf8)) {

                /* What we do is construct a non-UTF-8 string with
                 *  1) the characters representable by a single byte converted
                 *     to be so (if necessary);
                 *  2) and the rest converted to collate the same as the
                 *     highest collating representable character.  That makes
                 *     them collate at the end.  This is similar to how we
                 *     handle embedded NULs, but we use the highest collating
                 *     code point instead of the smallest.  Like the NUL case,
                 *     this isn't perfect, but is the best we can reasonably
                 *     do.  Every above-255 code point will sort the same as
                 *     the highest-sorting 0-255 code point.  If that code
                 *     point can combine in a sequence with some other code
                 *     points for weight calculations, us changing something to
                 *     be it can adversely affect the results.  But in most
                 *     cases, it should work reasonably.  And note that this is
                 *     really an illegal situation: using code points above 255
                 *     on a locale where only 0-255 are valid.  If two strings
                 *     sort entirely equal, then the sort order for the
                 *     above-255 code points will be in code point order. */

                utf8 = FALSE;

                /* If we haven't calculated the code point with the maximum
                 * collating order for this locale, do so now */
                if (! PL_strxfrm_max_cp) {
                    int j;

                    /* The current transformed string that collates the
                     * highest (except it also includes the prefixed collation
                     * index. */
                    char * cur_max_x = NULL;

                    /* Look through all legal code points (NUL isn't) */
                    for (j = 1; j < 256; j++) {
                        char * x;
                        STRLEN x_len;
                        char cur_source[] = { '\0', '\0' };

                        /* Create a 1-char string of the current code point */
                        cur_source[0] = (char) j;

                        /* Then transform it */
                        x = _mem_collxfrm(cur_source, 1, &x_len, FALSE);

                        /* If something went wrong (which it shouldn't), just
                         * ignore this code point */
                        if (! x) {
                            continue;
                        }

                        /* If this character's transformation is higher than
                         * the current highest, this one becomes the highest */
                        if (   cur_max_x == NULL
                            || strGT(x         + COLLXFRM_HDR_LEN,
                                     cur_max_x + COLLXFRM_HDR_LEN))
                        {
                            PL_strxfrm_max_cp = j;
                            Safefree(cur_max_x);
                            cur_max_x = x;
                        }
                        else {
                            Safefree(x);
                        }
                    }

                    if (! cur_max_x) {
                        DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "_mem_collxfrm: Couldn't find any character to"
                            " replace above-Latin1 chars in locale %s with",
                            PL_collation_name));
                        goto bad;
                    }

                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "_mem_collxfrm: highest 1-byte collating character"
                            " in locale %s is 0x%02X\n",
                            PL_collation_name,
                            PL_strxfrm_max_cp));

                    Safefree(cur_max_x);
                }

                /* Here we know which legal code point collates the highest.
                 * We are ready to construct the non-UTF-8 string.  The length
                 * will be at least 1 byte smaller than the input string
                 * (because we changed at least one 2-byte character into a
                 * single byte), but that is eaten up by the trailing NUL */
                Newx(s, len, char);

                {
                    STRLEN i;
                    STRLEN d= 0;
                    char * e = (char *) t + len;

                    for (i = 0; i < len; i+= UTF8SKIP(t + i)) {
                        U8 cur_char = t[i];
                        if (UTF8_IS_INVARIANT(cur_char)) {
                            s[d++] = cur_char;
                        }
                        else if (UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(t + i, e)) {
                            s[d++] = EIGHT_BIT_UTF8_TO_NATIVE(cur_char, t[i+1]);
                        }
                        else {  /* Replace illegal cp with highest collating
                                   one */
                            s[d++] = PL_strxfrm_max_cp;
                        }
                    }
                    s[d++] = '\0';
                    Renew(s, d, char);   /* Free up unused space */
                }
            }
        }

        /* Here, we have constructed a modified version of the input.  It could
         * be that we already had a modified copy before we did this version.
         * If so, that copy is no longer needed */
        if (t != input_string) {
            Safefree(t);
        }
    }

    length_in_chars = (utf8)
                      ? utf8_length((U8 *) s, (U8 *) s + len)
                      : len;

    /* The first element in the output is the collation id, used by
     * sv_collxfrm(); then comes the space for the transformed string.  The
     * equation should give us a good estimate as to how much is needed */
    xAlloc = COLLXFRM_HDR_LEN
           + PL_collxfrm_base
           + (PL_collxfrm_mult * length_in_chars);
    Newx(xbuf, xAlloc, char);
    if (UNLIKELY(! xbuf)) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                      "_mem_collxfrm: Couldn't malloc %zu bytes\n", xAlloc));
	goto bad;
    }

    /* Store the collation id */
    *(U32*)xbuf = PL_collation_ix;

    /* Then the transformation of the input.  We loop until successful, or we
     * give up */
    for (;;) {

        *xlen = strxfrm(xbuf + COLLXFRM_HDR_LEN, s, xAlloc - COLLXFRM_HDR_LEN);

        /* If the transformed string occupies less space than we told strxfrm()
         * was available, it means it successfully transformed the whole
         * string. */
        if (*xlen < xAlloc - COLLXFRM_HDR_LEN) {

            /* Some systems include a trailing NUL in the returned length.
             * Ignore it, using a loop in case multiple trailing NULs are
             * returned. */
            while (   (*xlen) > 0
                   && *(xbuf + COLLXFRM_HDR_LEN + (*xlen) - 1) == '\0')
            {
                (*xlen)--;
            }

            /* If the first try didn't get it, it means our prediction was low.
             * Modify the coefficients so that we predict a larger value in any
             * future transformations */
            if (! first_time) {
                STRLEN needed = *xlen + 1;   /* +1 For trailing NUL */
                STRLEN computed_guess = PL_collxfrm_base
                                      + (PL_collxfrm_mult * length_in_chars);

                /* On zero-length input, just keep current slope instead of
                 * dividing by 0 */
                const STRLEN new_m = (length_in_chars != 0)
                                     ? needed / length_in_chars
                                     : PL_collxfrm_mult;

                DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                    "%s: %d: initial size of %zu bytes for a length "
                    "%zu string was insufficient, %zu needed\n",
                    __FILE__, __LINE__,
                    computed_guess, length_in_chars, needed));

                /* If slope increased, use it, but discard this result for
                 * length 1 strings, as we can't be sure that it's a real slope
                 * change */
                if (length_in_chars > 1 && new_m  > PL_collxfrm_mult) {

#  ifdef DEBUGGING

                    STRLEN old_m = PL_collxfrm_mult;
                    STRLEN old_b = PL_collxfrm_base;

#  endif

                    PL_collxfrm_mult = new_m;
                    PL_collxfrm_base = 1;   /* +1 For trailing NUL */
                    computed_guess = PL_collxfrm_base
                                    + (PL_collxfrm_mult * length_in_chars);
                    if (computed_guess < needed) {
                        PL_collxfrm_base += needed - computed_guess;
                    }

                    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                        "%s: %d: slope is now %zu; was %zu, base "
                        "is now %zu; was %zu\n",
                        __FILE__, __LINE__,
                        PL_collxfrm_mult, old_m,
                        PL_collxfrm_base, old_b));
                }
                else {  /* Slope didn't change, but 'b' did */
                    const STRLEN new_b = needed
                                        - computed_guess
                                        + PL_collxfrm_base;
                    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                        "%s: %d: base is now %zu; was %zu\n",
                        __FILE__, __LINE__,
                        new_b, PL_collxfrm_base));
                    PL_collxfrm_base = new_b;
                }
            }

            break;
        }

        if (UNLIKELY(*xlen >= PERL_INT_MAX)) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                  "_mem_collxfrm: Needed %zu bytes, max permissible is %u\n",
                  *xlen, PERL_INT_MAX));
            goto bad;
        }

        /* A well-behaved strxfrm() returns exactly how much space it needs
         * (usually not including the trailing NUL) when it fails due to not
         * enough space being provided.  Assume that this is the case unless
         * it's been proven otherwise */
        if (LIKELY(PL_strxfrm_is_behaved) && first_time) {
            xAlloc = *xlen + COLLXFRM_HDR_LEN + 1;
        }
        else { /* Here, either:
                *  1)  The strxfrm() has previously shown bad behavior; or
                *  2)  It isn't the first time through the loop, which means
                *      that the strxfrm() is now showing bad behavior, because
                *      we gave it what it said was needed in the previous
                *      iteration, and it came back saying it needed still more.
                *      (Many versions of cygwin fit this.  When the buffer size
                *      isn't sufficient, they return the input size instead of
                *      how much is needed.)
                * Increase the buffer size by a fixed percentage and try again.
                * */
            xAlloc += (xAlloc / 4) + 1;
            PL_strxfrm_is_behaved = FALSE;

#  ifdef DEBUGGING

            if (DEBUG_Lv_TEST || debug_initialization) {
                PerlIO_printf(Perl_debug_log,
                "_mem_collxfrm required more space than previously calculated"
                " for locale %s, trying again with new guess=%d+%zu\n",
                PL_collation_name, (int) COLLXFRM_HDR_LEN,
                xAlloc - COLLXFRM_HDR_LEN);
            }

#  endif

        }

        Renew(xbuf, xAlloc, char);
        if (UNLIKELY(! xbuf)) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                      "_mem_collxfrm: Couldn't realloc %zu bytes\n", xAlloc));
            goto bad;
        }

        first_time = FALSE;
    }


#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {

        print_collxfrm_input_and_return(s, s + len, xlen, utf8);
        PerlIO_printf(Perl_debug_log, "Its xfrm is:");
        PerlIO_printf(Perl_debug_log, "%s\n",
                      _byte_dump_string((U8 *) xbuf + COLLXFRM_HDR_LEN,
                       *xlen, 1));
    }

#  endif

    /* Free up unneeded space; retain ehough for trailing NUL */
    Renew(xbuf, COLLXFRM_HDR_LEN + *xlen + 1, char);

    if (s != input_string) {
        Safefree(s);
    }

    return xbuf;

  bad:
    Safefree(xbuf);
    if (s != input_string) {
        Safefree(s);
    }
    *xlen = 0;

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST || debug_initialization) {
        print_collxfrm_input_and_return(s, s + len, NULL, utf8);
    }

#  endif

    return NULL;
}

#  ifdef DEBUGGING

STATIC void
S_print_collxfrm_input_and_return(pTHX_
                                  const char * const s,
                                  const char * const e,
                                  const STRLEN * const xlen,
                                  const bool is_utf8)
{

    PERL_ARGS_ASSERT_PRINT_COLLXFRM_INPUT_AND_RETURN;

    PerlIO_printf(Perl_debug_log, "_mem_collxfrm[%" UVuf "]: returning ",
                                                        (UV)PL_collation_ix);
    if (xlen) {
        PerlIO_printf(Perl_debug_log, "%zu", *xlen);
    }
    else {
        PerlIO_printf(Perl_debug_log, "NULL");
    }
    PerlIO_printf(Perl_debug_log, " for locale '%s', string='",
                                                            PL_collation_name);
    print_bytes_for_locale(s, e, is_utf8);

    PerlIO_printf(Perl_debug_log, "'\n");
}

#  endif    /* DEBUGGING */
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE
#  ifdef DEBUGGING

STATIC void
S_print_bytes_for_locale(pTHX_
                    const char * const s,
                    const char * const e,
                    const bool is_utf8)
{
    const char * t = s;
    bool prev_was_printable = TRUE;
    bool first_time = TRUE;

    PERL_ARGS_ASSERT_PRINT_BYTES_FOR_LOCALE;

    while (t < e) {
        UV cp = (is_utf8)
                ?  utf8_to_uvchr_buf((U8 *) t, e, NULL)
                : * (U8 *) t;
        if (isPRINT(cp)) {
            if (! prev_was_printable) {
                PerlIO_printf(Perl_debug_log, " ");
            }
            PerlIO_printf(Perl_debug_log, "%c", (U8) cp);
            prev_was_printable = TRUE;
        }
        else {
            if (! first_time) {
                PerlIO_printf(Perl_debug_log, " ");
            }
            PerlIO_printf(Perl_debug_log, "%02" UVXf, cp);
            prev_was_printable = FALSE;
        }
        t += (is_utf8) ? UTF8SKIP(t) : 1;
        first_time = FALSE;
    }
}

#  endif   /* #ifdef DEBUGGING */

STATIC const char *
S_switch_category_locale_to_template(pTHX_ const int switch_category, const int template_category, const char * template_locale)
{
    /* Changes the locale for LC_'switch_category" to that of
     * LC_'template_category', if they aren't already the same.  If not NULL,
     * 'template_locale' is the locale that 'template_category' is in.
     *
     * Returns a copy of the name of the original locale for 'switch_category'
     * so can be switched back to with the companion function
     * restore_switched_locale(),  (NULL if no restoral is necessary.) */

    char * restore_to_locale = NULL;

    if (switch_category == template_category) { /* No changes needed */
        return NULL;
    }

    /* Find the original locale of the category we may need to change, so that
     * it can be restored to later */
    restore_to_locale = stdize_locale(savepv(do_setlocale_r(switch_category,
                                                            NULL)));
    if (! restore_to_locale) {
        Perl_croak(aTHX_
             "panic: %s: %d: Could not find current %s locale, errno=%d\n",
                __FILE__, __LINE__, category_name(switch_category), errno);
    }

    /* If the locale of the template category wasn't passed in, find it now */
    if (template_locale == NULL) {
        template_locale = do_setlocale_r(template_category, NULL);
        if (! template_locale) {
            Perl_croak(aTHX_
             "panic: %s: %d: Could not find current %s locale, errno=%d\n",
                   __FILE__, __LINE__, category_name(template_category), errno);
        }
    }

    /* It the locales are the same, there's nothing to do */
    if (strEQ(restore_to_locale, template_locale)) {
        Safefree(restore_to_locale);

        DEBUG_Lv(PerlIO_printf(Perl_debug_log, "%s locale unchanged as %s\n",
                            category_name(switch_category), restore_to_locale));

        return NULL;
    }

    /* Finally, change the locale to the template one */
    if (! do_setlocale_r(switch_category, template_locale)) {
        Perl_croak(aTHX_
         "panic: %s: %d: Could not change %s locale to %s, errno=%d\n",
                            __FILE__, __LINE__, category_name(switch_category),
                                                       template_locale, errno);
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log, "%s locale switched to %s\n",
                            category_name(switch_category), template_locale));

    return restore_to_locale;
}

STATIC void
S_restore_switched_locale(pTHX_ const int category, const char * const original_locale)
{
    /* Restores the locale for LC_'category' to 'original_locale' (which is a
     * copy that will be freed by this function), or do nothing if the latter
     * parameter is NULL */

    if (original_locale == NULL) {
        return;
    }

    if (! do_setlocale_r(category, original_locale)) {
        Perl_croak(aTHX_
             "panic: %s: %d: setlocale %s restore to %s failed, errno=%d\n",
                 __FILE__, __LINE__,
                             category_name(category), original_locale, errno);
    }

    Safefree(original_locale);
}

/* is_cur_LC_category_utf8 uses a small char buffer to avoid malloc/free */
#define CUR_LC_BUFFER_SIZE  64

bool
Perl__is_cur_LC_category_utf8(pTHX_ int category)
{
    /* Returns TRUE if the current locale for 'category' is UTF-8; FALSE
     * otherwise. 'category' may not be LC_ALL.  If the platform doesn't have
     * nl_langinfo(), nor MB_CUR_MAX, this employs a heuristic, which hence
     * could give the wrong result.  The result will very likely be correct for
     * languages that have commonly used non-ASCII characters, but for notably
     * English, it comes down to if the locale's name ends in something like
     * "UTF-8".  It errs on the side of not being a UTF-8 locale.
     *
     * If the platform is early C89, not containing mbtowc(), or we are
     * compiled to not pay attention to LC_CTYPE, this employs heuristics.
     * These work very well for non-Latin locales or those whose currency
     * symbol isn't a '$' nor plain ASCII text.  But without LC_CTYPE and at
     * least MB_CUR_MAX, English locales with an ASCII currency symbol depend
     * on the name containing UTF-8 or not. */

    /* Name of current locale corresponding to the input category */
    const char *save_input_locale = NULL;

    bool is_utf8 = FALSE;                /* The return value */

    /* The variables below are for the cache of previous lookups using this
     * function.  The cache is a C string, described at the definition for
     * 'C_and_POSIX_utf8ness'.
     *
     * The first part of the cache is fixed, for the C and POSIX locales.  The
     * varying part starts just after them. */
    char * utf8ness_cache = PL_locale_utf8ness + STRLENs(C_and_POSIX_utf8ness);

    Size_t utf8ness_cache_size; /* Size of the varying portion */
    Size_t input_name_len;      /* Length in bytes of save_input_locale */
    Size_t input_name_len_with_overhead;    /* plus extra chars used to store
                                               the name in the cache */
    char * delimited;           /* The name plus the delimiters used to store
                                   it in the cache */
    char buffer[CUR_LC_BUFFER_SIZE];        /* small buffer */
    char * name_pos;            /* position of 'delimited' in the cache, or 0
                                   if not there */


#  ifdef LC_ALL

    assert(category != LC_ALL);

#  endif

    /* Get the desired category's locale */
    save_input_locale = stdize_locale(savepv(do_setlocale_r(category, NULL)));
    if (! save_input_locale) {
        Perl_croak(aTHX_
             "panic: %s: %d: Could not find current %s locale, errno=%d\n",
                     __FILE__, __LINE__, category_name(category), errno);
    }

    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Current locale for %s is %s\n",
                          category_name(category), save_input_locale));

    input_name_len = strlen(save_input_locale);

    /* In our cache, each name is accompanied by two delimiters and a single
     * utf8ness digit */
    input_name_len_with_overhead = input_name_len + 3;

    if ( input_name_len_with_overhead <= CUR_LC_BUFFER_SIZE ) {
        /* we can use the buffer, avoid a malloc */
        delimited = buffer;
    } else { /* need a malloc */
        /* Allocate and populate space for a copy of the name surrounded by the
         * delimiters */
        Newx(delimited, input_name_len_with_overhead, char);
    }

    delimited[0] = UTF8NESS_SEP[0];
    Copy(save_input_locale, delimited + 1, input_name_len, char);
    delimited[input_name_len+1] = UTF8NESS_PREFIX[0];
    delimited[input_name_len+2] = '\0';

    /* And see if that is in the cache */
    name_pos = instr(PL_locale_utf8ness, delimited);
    if (name_pos) {
        is_utf8 = *(name_pos + input_name_len_with_overhead - 1) - '0';

#  ifdef DEBUGGING

        if (DEBUG_Lv_TEST || debug_initialization) {
            PerlIO_printf(Perl_debug_log, "UTF8ness for locale %s=%d, \n",
                                          save_input_locale, is_utf8);
        }

#  endif

        /* And, if not already in that position, move it to the beginning of
         * the non-constant portion of the list, since it is the most recently
         * used.  (We don't have to worry about overflow, since just moving
         * existing names around) */
        if (name_pos > utf8ness_cache) {
            Move(utf8ness_cache,
                 utf8ness_cache + input_name_len_with_overhead,
                 name_pos - utf8ness_cache, char);
            Copy(delimited,
                 utf8ness_cache,
                 input_name_len_with_overhead - 1, char);
            utf8ness_cache[input_name_len_with_overhead - 1] = is_utf8 + '0';
        }

        /* free only when not using the buffer */
        if ( delimited != buffer ) Safefree(delimited);
        Safefree(save_input_locale);
        return is_utf8;
    }

    /* Here we don't have stored the utf8ness for the input locale.  We have to
     * calculate it */

#  if        defined(USE_LOCALE_CTYPE)                                  \
     && (    defined(HAS_NL_LANGINFO)                                   \
         || (defined(HAS_MBTOWC) || defined(HAS_MBRTOWC)))

    {
        const char *original_ctype_locale
                        = switch_category_locale_to_template(LC_CTYPE,
                                                             category,
                                                             save_input_locale);

        /* Here the current LC_CTYPE is set to the locale of the category whose
         * information is desired.  This means that nl_langinfo() and mbtowc()
         * should give the correct results */

#    ifdef MB_CUR_MAX  /* But we can potentially rule out UTF-8ness, avoiding
                          calling the functions if we have this */

            /* Standard UTF-8 needs at least 4 bytes to represent the maximum
             * Unicode code point. */

            DEBUG_L(PerlIO_printf(Perl_debug_log, "%s: %d: MB_CUR_MAX=%d\n",
                                       __FILE__, __LINE__, (int) MB_CUR_MAX));
            if ((unsigned) MB_CUR_MAX < STRLENs(MAX_UNICODE_UTF8)) {
                is_utf8 = FALSE;
                restore_switched_locale(LC_CTYPE, original_ctype_locale);
                goto finish_and_return;
            }

#    endif
#    if defined(HAS_NL_LANGINFO)

        { /* The task is easiest if the platform has this POSIX 2001 function.
             Except on some platforms it can wrongly return "", so have to have
             a fallback.  And it can return that it's UTF-8, even if there are
             variances from that.  For example, Turkish locales may use the
             alternate dotted I rules, and sometimes it appears to be a
             defective locale definition.  XXX We should probably check for
             these in the Latin1 range and warn (but on glibc, requires
             iswalnum() etc. due to their not handling 80-FF correctly */
            const char *codeset = my_nl_langinfo(CODESET, FALSE);
                                          /* FALSE => already in dest locale */

            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                            "\tnllanginfo returned CODESET '%s'\n", codeset));

            if (codeset && strNE(codeset, "")) {

                              /* If the implementation of foldEQ() somehow were
                               * to change to not go byte-by-byte, this could
                               * read past end of string, as only one length is
                               * checked.  But currently, a premature NUL will
                               * compare false, and it will stop there */
                is_utf8 = cBOOL(   foldEQ(codeset, STR_WITH_LEN("UTF-8"))
                                || foldEQ(codeset, STR_WITH_LEN("UTF8")));

                DEBUG_L(PerlIO_printf(Perl_debug_log,
                       "\tnllanginfo returned CODESET '%s'; ?UTF8 locale=%d\n",
                                                     codeset,         is_utf8));
                restore_switched_locale(LC_CTYPE, original_ctype_locale);
                goto finish_and_return;
            }
        }

#    endif
#    if defined(HAS_MBTOWC) || defined(HAS_MBRTOWC)
     /* We can see if this is a UTF-8-like locale if have mbtowc().  It was a
      * late adder to C89, so very likely to have it.  However, testing has
      * shown that, like nl_langinfo() above, there are locales that are not
      * strictly UTF-8 that this will return that they are */

        {
            wchar_t wc;
            int len;
            dSAVEDERRNO;

#      if defined(HAS_MBRTOWC) && defined(USE_ITHREADS)

            mbstate_t ps;

#      endif

            /* mbrtowc() and mbtowc() convert a byte string to a wide
             * character.  Feed a byte string to one of them and check that the
             * result is the expected Unicode code point */

#      if defined(HAS_MBRTOWC) && defined(USE_ITHREADS)
            /* Prefer this function if available, as it's reentrant */

            memset(&ps, 0, sizeof(ps));;
            PERL_UNUSED_RESULT(mbrtowc(&wc, NULL, 0, &ps)); /* Reset any shift
                                                               state */
            SETERRNO(0, 0);
            len = mbrtowc(&wc, STR_WITH_LEN(REPLACEMENT_CHARACTER_UTF8), &ps);
            SAVE_ERRNO;

#      else

            LOCALE_LOCK;
            PERL_UNUSED_RESULT(mbtowc(&wc, NULL, 0));/* Reset any shift state */
            SETERRNO(0, 0);
            len = mbtowc(&wc, STR_WITH_LEN(REPLACEMENT_CHARACTER_UTF8));
            SAVE_ERRNO;
            LOCALE_UNLOCK;

#      endif

            RESTORE_ERRNO;
            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                    "\treturn from mbtowc; len=%d; code_point=%x; errno=%d\n",
                                   len,      (unsigned int) wc, GET_ERRNO));

            is_utf8 = cBOOL(   len == STRLENs(REPLACEMENT_CHARACTER_UTF8)
                            && wc == (wchar_t) UNICODE_REPLACEMENT);
        }

#    endif

        restore_switched_locale(LC_CTYPE, original_ctype_locale);
        goto finish_and_return;
    }

#  else

        /* Here, we must have a C89 compiler that doesn't have mbtowc().  Next
         * try looking at the currency symbol to see if it disambiguates
         * things.  Often that will be in the native script, and if the symbol
         * isn't in UTF-8, we know that the locale isn't.  If it is non-ASCII
         * UTF-8, we infer that the locale is too, as the odds of a non-UTF8
         * string being valid UTF-8 are quite small */

#    ifdef USE_LOCALE_MONETARY

        /* If have LC_MONETARY, we can look at the currency symbol.  Often that
         * will be in the native script.  We do this one first because there is
         * just one string to examine, so potentially avoids work */

        {
            const char *original_monetary_locale
                        = switch_category_locale_to_template(LC_MONETARY,
                                                             category,
                                                             save_input_locale);
            bool only_ascii = FALSE;
            const U8 * currency_string
                            = (const U8 *) my_nl_langinfo(CRNCYSTR, FALSE);
                                      /* 2nd param not relevant for this item */
            const U8 * first_variant;

            assert(   *currency_string == '-'
                   || *currency_string == '+'
                   || *currency_string == '.');

            currency_string++;

            if (is_utf8_invariant_string_loc(currency_string, 0, &first_variant))
            {
                DEBUG_L(PerlIO_printf(Perl_debug_log, "Couldn't get currency symbol for %s, or contains only ASCII; can't use for determining if UTF-8 locale\n", save_input_locale));
                only_ascii = TRUE;
            }
            else {
                is_utf8 = is_strict_utf8_string(first_variant, 0);
            }

            restore_switched_locale(LC_MONETARY, original_monetary_locale);

            if (! only_ascii) {

                /* It isn't a UTF-8 locale if the symbol is not legal UTF-8;
                 * otherwise assume the locale is UTF-8 if and only if the symbol
                 * is non-ascii UTF-8. */
                DEBUG_Lv(PerlIO_printf(Perl_debug_log, "\t?Currency symbol for %s is UTF-8=%d\n",
                                        save_input_locale, is_utf8));
                goto finish_and_return;
            }
        }

#    endif /* USE_LOCALE_MONETARY */
#    if defined(HAS_STRFTIME) && defined(USE_LOCALE_TIME)

    /* Still haven't found a non-ASCII string to disambiguate UTF-8 or not.  Try
     * the names of the months and weekdays, timezone, and am/pm indicator */
        {
            const char *original_time_locale
                            = switch_category_locale_to_template(LC_TIME,
                                                                 category,
                                                                 save_input_locale);
            int hour = 10;
            bool is_dst = FALSE;
            int dom = 1;
            int month = 0;
            int i;
            char * formatted_time;

            /* Here the current LC_TIME is set to the locale of the category
             * whose information is desired.  Look at all the days of the week and
             * month names, and the timezone and am/pm indicator for UTF-8 variant
             * characters.  The first such a one found will tell us if the locale
             * is UTF-8 or not */

            for (i = 0; i < 7 + 12; i++) {  /* 7 days; 12 months */
                formatted_time = my_strftime("%A %B %Z %p",
                                0, 0, hour, dom, month, 2012 - 1900, 0, 0, is_dst);
                if ( ! formatted_time
                    || is_utf8_invariant_string((U8 *) formatted_time, 0))
                {

                    /* Here, we didn't find a non-ASCII.  Try the next time through
                     * with the complemented dst and am/pm, and try with the next
                     * weekday.  After we have gotten all weekdays, try the next
                     * month */
                    is_dst = ! is_dst;
                    hour = (hour + 12) % 24;
                    dom++;
                    if (i > 6) {
                        month++;
                    }
                    continue;
                }

                /* Here, we have a non-ASCII.  Return TRUE is it is valid UTF8;
                 * false otherwise.  But first, restore LC_TIME to its original
                 * locale if we changed it */
                restore_switched_locale(LC_TIME, original_time_locale);

                DEBUG_Lv(PerlIO_printf(Perl_debug_log, "\t?time-related strings for %s are UTF-8=%d\n",
                                    save_input_locale,
                                    is_utf8_string((U8 *) formatted_time, 0)));
                is_utf8 = is_utf8_string((U8 *) formatted_time, 0);
                goto finish_and_return;
            }

            /* Falling off the end of the loop indicates all the names were just
             * ASCII.  Go on to the next test.  If we changed it, restore LC_TIME
             * to its original locale */
            restore_switched_locale(LC_TIME, original_time_locale);
            DEBUG_Lv(PerlIO_printf(Perl_debug_log, "All time-related words for %s contain only ASCII; can't use for determining if UTF-8 locale\n", save_input_locale));
        }

#    endif

#    if 0 && defined(USE_LOCALE_MESSAGES) && defined(HAS_SYS_ERRLIST)

    /* This code is ifdefd out because it was found to not be necessary in testing
     * on our dromedary test machine, which has over 700 locales.  There, this
     * added no value to looking at the currency symbol and the time strings.  I
     * left it in so as to avoid rewriting it if real-world experience indicates
     * that dromedary is an outlier.  Essentially, instead of returning abpve if we
     * haven't found illegal utf8, we continue on and examine all the strerror()
     * messages on the platform for utf8ness.  If all are ASCII, we still don't
     * know the answer; but otherwise we have a pretty good indication of the
     * utf8ness.  The reason this doesn't help much is that the messages may not
     * have been translated into the locale.  The currency symbol and time strings
     * are much more likely to have been translated.  */
        {
            int e;
            bool non_ascii = FALSE;
            const char *original_messages_locale
                            = switch_category_locale_to_template(LC_MESSAGES,
                                                                 category,
                                                                 save_input_locale);
            const char * errmsg = NULL;

            /* Here the current LC_MESSAGES is set to the locale of the category
             * whose information is desired.  Look through all the messages.  We
             * can't use Strerror() here because it may expand to code that
             * segfaults in miniperl */

            for (e = 0; e <= sys_nerr; e++) {
                errno = 0;
                errmsg = sys_errlist[e];
                if (errno || !errmsg) {
                    break;
                }
                errmsg = savepv(errmsg);
                if (! is_utf8_invariant_string((U8 *) errmsg, 0)) {
                    non_ascii = TRUE;
                    is_utf8 = is_utf8_string((U8 *) errmsg, 0);
                    break;
                }
            }
            Safefree(errmsg);

            restore_switched_locale(LC_MESSAGES, original_messages_locale);

            if (non_ascii) {

                /* Any non-UTF-8 message means not a UTF-8 locale; if all are valid,
                 * any non-ascii means it is one; otherwise we assume it isn't */
                DEBUG_Lv(PerlIO_printf(Perl_debug_log, "\t?error messages for %s are UTF-8=%d\n",
                                    save_input_locale,
                                    is_utf8));
                goto finish_and_return;
            }

            DEBUG_L(PerlIO_printf(Perl_debug_log, "All error messages for %s contain only ASCII; can't use for determining if UTF-8 locale\n", save_input_locale));
        }

#    endif
#    ifndef EBCDIC  /* On os390, even if the name ends with "UTF-8', it isn't a
                   UTF-8 locale */

    /* As a last resort, look at the locale name to see if it matches
     * qr/UTF -?  * 8 /ix, or some other common locale names.  This "name", the
     * return of setlocale(), is actually defined to be opaque, so we can't
     * really rely on the absence of various substrings in the name to indicate
     * its UTF-8ness, but if it has UTF8 in the name, it is extremely likely to
     * be a UTF-8 locale.  Similarly for the other common names */

    {
        const Size_t final_pos = strlen(save_input_locale) - 1;

        if (final_pos >= 3) {
            const char *name = save_input_locale;

            /* Find next 'U' or 'u' and look from there */
            while ((name += strcspn(name, "Uu") + 1)
                                        <= save_input_locale + final_pos - 2)
            {
                if (   isALPHA_FOLD_NE(*name, 't')
                    || isALPHA_FOLD_NE(*(name + 1), 'f'))
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
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                        "Locale %s ends with UTF-8 in name\n",
                                        save_input_locale));
                    is_utf8 = TRUE;
                    goto finish_and_return;
                }
            }
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                                "Locale %s doesn't end with UTF-8 in name\n",
                                    save_input_locale));
        }

#      ifdef WIN32

        /* http://msdn.microsoft.com/en-us/library/windows/desktop/dd317756.aspx */
        if (memENDs(save_input_locale, final_pos, "65001")) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                        "Locale %s ends with 65001 in name, is UTF-8 locale\n",
                        save_input_locale));
            is_utf8 = TRUE;
            goto finish_and_return;
        }

#      endif
    }
#    endif

    /* Other common encodings are the ISO 8859 series, which aren't UTF-8.  But
     * since we are about to return FALSE anyway, there is no point in doing
     * this extra work */

#    if 0
    if (instr(save_input_locale, "8859")) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                             "Locale %s has 8859 in name, not UTF-8 locale\n",
                             save_input_locale));
        is_utf8 = FALSE;
        goto finish_and_return;
    }
#    endif

    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Assuming locale %s is not a UTF-8 locale\n",
                                    save_input_locale));
    is_utf8 = FALSE;

#  endif /* the code that is compiled when no modern LC_CTYPE */

  finish_and_return:

    /* Cache this result so we don't have to go through all this next time. */
    utf8ness_cache_size = sizeof(PL_locale_utf8ness)
                       - (utf8ness_cache - PL_locale_utf8ness);

    /* But we can't save it if it is too large for the total space available */
    if (LIKELY(input_name_len_with_overhead < utf8ness_cache_size)) {
        Size_t utf8ness_cache_len = strlen(utf8ness_cache);

        /* Here it can fit, but we may need to clear out the oldest cached
         * result(s) to do so.  Check */
        if (utf8ness_cache_len + input_name_len_with_overhead
                                                        >= utf8ness_cache_size)
        {
            /* Here we have to clear something out to make room for this.
             * Start looking at the rightmost place where it could fit and find
             * the beginning of the entry that extends past that. */
            char * cutoff = (char *) my_memrchr(utf8ness_cache,
                                                UTF8NESS_SEP[0],
                                                utf8ness_cache_size
                                              - input_name_len_with_overhead);

            assert(cutoff);
            assert(cutoff >= utf8ness_cache);

            /* This and all subsequent entries must be removed */
            *cutoff = '\0';
            utf8ness_cache_len = strlen(utf8ness_cache);
        }

        /* Make space for the new entry */
        Move(utf8ness_cache,
             utf8ness_cache + input_name_len_with_overhead,
             utf8ness_cache_len + 1 /* Incl. trailing NUL */, char);

        /* And insert it */
        Copy(delimited, utf8ness_cache, input_name_len_with_overhead - 1, char);
        utf8ness_cache[input_name_len_with_overhead - 1] = is_utf8 + '0';

        if ((PL_locale_utf8ness[strlen(PL_locale_utf8ness)-1]
                                                & (PERL_UINTMAX_T) ~1) != '0')
        {
            Perl_croak(aTHX_
             "panic: %s: %d: Corrupt utf8ness_cache=%s\nlen=%zu,"
             " inserted_name=%s, its_len=%zu\n",
                __FILE__, __LINE__,
                PL_locale_utf8ness, strlen(PL_locale_utf8ness),
                delimited, input_name_len_with_overhead);
        }
    }

#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST) {
        const char * s = PL_locale_utf8ness;

        /* Audit the structure */
        while (s < PL_locale_utf8ness + strlen(PL_locale_utf8ness)) {
            const char *e;

            if (*s != UTF8NESS_SEP[0]) {
                Perl_croak(aTHX_
                           "panic: %s: %d: Corrupt utf8ness_cache: missing"
                           " separator %.*s<-- HERE %s\n",
                           __FILE__, __LINE__,
                           (int) (s - PL_locale_utf8ness), PL_locale_utf8ness,
                           s);
            }
            s++;
            e = strchr(s, UTF8NESS_PREFIX[0]);
            if (! e) {
                Perl_croak(aTHX_
                           "panic: %s: %d: Corrupt utf8ness_cache: missing"
                           " separator %.*s<-- HERE %s\n",
                           __FILE__, __LINE__,
                           (int) (e - PL_locale_utf8ness), PL_locale_utf8ness,
                           e);
            }
            e++;
            if (*e != '0' && *e != '1') {
                Perl_croak(aTHX_
                           "panic: %s: %d: Corrupt utf8ness_cache: utf8ness"
                           " must be [01] %.*s<-- HERE %s\n",
                           __FILE__, __LINE__,
                           (int) (e + 1 - PL_locale_utf8ness),
                           PL_locale_utf8ness, e + 1);
            }
            if (ninstr(PL_locale_utf8ness, s, s-1, e)) {
                Perl_croak(aTHX_
                           "panic: %s: %d: Corrupt utf8ness_cache: entry"
                           " has duplicate %.*s<-- HERE %s\n",
                           __FILE__, __LINE__,
                           (int) (e - PL_locale_utf8ness), PL_locale_utf8ness,
                           e);
            }
            s = e + 1;
        }
    }

    if (DEBUG_Lv_TEST || debug_initialization) {

        PerlIO_printf(Perl_debug_log,
                "PL_locale_utf8ness is now %s; returning %d\n",
                                     PL_locale_utf8ness, is_utf8);
    }

#  endif

    /* free only when not using the buffer */
    if ( delimited != buffer ) Safefree(delimited);
    Safefree(save_input_locale);
    return is_utf8;
}

#endif

bool
Perl__is_in_locale_category(pTHX_ const bool compiling, const int category)
{
    dVAR;
    /* Internal function which returns if we are in the scope of a pragma that
     * enables the locale category 'category'.  'compiling' should indicate if
     * this is during the compilation phase (TRUE) or not (FALSE). */

    const COP * const cop = (compiling) ? &PL_compiling : PL_curcop;

    SV *these_categories = cop_hints_fetch_pvs(cop, "locale", 0);
    if (! these_categories || these_categories == &PL_sv_placeholder) {
        return FALSE;
    }

    /* The pseudo-category 'not_characters' is -1, so just add 1 to each to get
     * a valid unsigned */
    assert(category >= -1);
    return cBOOL(SvUV(these_categories) & (1U << (category + 1)));
}

char *
Perl_my_strerror(pTHX_ const int errnum)
{
    /* Returns a mortalized copy of the text of the error message associated
     * with 'errnum'.  It uses the current locale's text unless the platform
     * doesn't have the LC_MESSAGES category or we are not being called from
     * within the scope of 'use locale'.  In the former case, it uses whatever
     * strerror returns; in the latter case it uses the text from the C locale.
     *
     * The function just calls strerror(), but temporarily switches, if needed,
     * to the C locale */

    char *errstr;
    dVAR;

#ifndef USE_LOCALE_MESSAGES

    /* If platform doesn't have messages category, we don't do any switching to
     * the C locale; we just use whatever strerror() returns */

    errstr = savepv(Strerror(errnum));

#else   /* Has locale messages */

    const bool within_locale_scope = IN_LC(LC_MESSAGES);

#  ifndef USE_ITHREADS

    /* This function is trivial without threads. */
    if (within_locale_scope) {
        errstr = savepv(strerror(errnum));
    }
    else {
        const char * save_locale = savepv(do_setlocale_c(LC_MESSAGES, NULL));

        do_setlocale_c(LC_MESSAGES, "C");
        errstr = savepv(strerror(errnum));
        do_setlocale_c(LC_MESSAGES, save_locale);
        Safefree(save_locale);
    }

#  elif defined(HAS_POSIX_2008_LOCALE)                      \
     && defined(HAS_STRERROR_L)                             \
     && defined(HAS_DUPLOCALE)

    /* This function is also trivial if we don't have to worry about thread
     * safety and have strerror_l(), as it handles the switch of locales so we
     * don't have to deal with that.  We don't have to worry about thread
     * safety if strerror_r() is also available.  Both it and strerror_l() are
     * thread-safe.  Plain strerror() isn't thread safe.  But on threaded
     * builds when strerror_r() is available, the apparent call to strerror()
     * below is actually a macro that behind-the-scenes calls strerror_r(). */

#    ifdef HAS_STRERROR_R

    if (within_locale_scope) {
        errstr = savepv(strerror(errnum));
    }
    else {
        errstr = savepv(strerror_l(errnum, PL_C_locale_obj));
    }

#    else

    /* Here we have strerror_l(), but not strerror_r() and we are on a
     * threaded-build.  We use strerror_l() for everything, constructing a
     * locale to pass to it if necessary */

    bool do_free = FALSE;
    locale_t locale_to_use;

    if (within_locale_scope) {
        locale_to_use = uselocale((locale_t) 0);
        if (locale_to_use == LC_GLOBAL_LOCALE) {
            locale_to_use = duplocale(LC_GLOBAL_LOCALE);
            do_free = TRUE;
        }
    }
    else {  /* Use C locale if not within 'use locale' scope */
        locale_to_use = PL_C_locale_obj;
    }

    errstr = savepv(strerror_l(errnum, locale_to_use));

    if (do_free) {
        freelocale(locale_to_use);
    }

#    endif
#  else /* Doesn't have strerror_l() */

    const char * save_locale = NULL;
    bool locale_is_C = FALSE;

    /* We have a critical section to prevent another thread from executing this
     * same code at the same time.  (On thread-safe perls, the LOCK is a
     * no-op.)  Since this is the only place in core that changes LC_MESSAGES
     * (unless the user has called setlocale(), this works to prevent races. */
    LOCALE_LOCK;

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                            "my_strerror called with errnum %d\n", errnum));
    if (! within_locale_scope) {
        save_locale = do_setlocale_c(LC_MESSAGES, NULL);
        if (! save_locale) {
            Perl_croak(aTHX_
                 "panic: %s: %d: Could not find current LC_MESSAGES locale,"
                 " errno=%d\n", __FILE__, __LINE__, errno);
        }
        else {
            locale_is_C = isNAME_C_OR_POSIX(save_locale);

            /* Switch to the C locale if not already in it */
            if (! locale_is_C) {

                /* The setlocale() just below likely will zap 'save_locale', so
                 * create a copy.  */
                save_locale = savepv(save_locale);
                do_setlocale_c(LC_MESSAGES, "C");
            }
        }
    }   /* end of ! within_locale_scope */
    else {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log, "%s: %d: WITHIN locale scope\n",
                                               __FILE__, __LINE__));
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
             "Any locale change has been done; about to call Strerror\n"));
    errstr = savepv(Strerror(errnum));

    if (! within_locale_scope) {
        if (save_locale && ! locale_is_C) {
            if (! do_setlocale_c(LC_MESSAGES, save_locale)) {
                Perl_croak(aTHX_
                     "panic: %s: %d: setlocale restore failed, errno=%d\n",
                             __FILE__, __LINE__, errno);
            }
            Safefree(save_locale);
        }
    }

    LOCALE_UNLOCK;

#  endif /* End of doesn't have strerror_l */
#  ifdef DEBUGGING

    if (DEBUG_Lv_TEST) {
        PerlIO_printf(Perl_debug_log, "Strerror returned; saving a copy: '");
        print_bytes_for_locale(errstr, errstr + strlen(errstr), 0);
        PerlIO_printf(Perl_debug_log, "'\n");
    }

#  endif
#endif   /* End of does have locale messages */

    SAVEFREEPV(errstr);
    return errstr;
}

/*

=for apidoc switch_to_global_locale

On systems without locale support, or on typical single-threaded builds, or on
platforms that do not support per-thread locale operations, this function does
nothing.  On such systems that do have locale support, only a locale global to
the whole program is available.

On multi-threaded builds on systems that do have per-thread locale operations,
this function converts the thread it is running in to use the global locale.
This is for code that has not yet or cannot be updated to handle multi-threaded
locale operation.  As long as only a single thread is so-converted, everything
works fine, as all the other threads continue to ignore the global one, so only
this thread looks at it.

However, on Windows systems this isn't quite true prior to Visual Studio 15,
at which point Microsoft fixed a bug.  A race can occur if you use the
following operations on earlier Windows platforms:

=over

=item L<POSIX::localeconv|POSIX/localeconv>

=item L<I18N::Langinfo>, items C<CRNCYSTR> and C<THOUSEP>

=item L<perlapi/Perl_langinfo>, items C<CRNCYSTR> and C<THOUSEP>

=back

The first item is not fixable (except by upgrading to a later Visual Studio
release), but it would be possible to work around the latter two items by using
the Windows API functions C<GetNumberFormat> and C<GetCurrencyFormat>; patches
welcome.

Without this function call, threads that use the L<C<setlocale(3)>> system
function will not work properly, as all the locale-sensitive functions will
look at the per-thread locale, and C<setlocale> will have no effect on this
thread.

Perl code should convert to either call
L<C<Perl_setlocale>|perlapi/Perl_setlocale> (which is a drop-in for the system
C<setlocale>) or use the methods given in L<perlcall> to call
L<C<POSIX::setlocale>|POSIX/setlocale>.  Either one will transparently properly
handle all cases of single- vs multi-thread, POSIX 2008-supported or not.

Non-Perl libraries, such as C<gtk>, that call the system C<setlocale> can
continue to work if this function is called before transferring control to the
library.

Upon return from the code that needs to use the global locale,
L<C<sync_locale()>|perlapi/sync_locale> should be called to restore the safe
multi-thread operation.

=cut
*/

void
Perl_switch_to_global_locale()
{

#ifdef USE_THREAD_SAFE_LOCALE
#  ifdef WIN32

    _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);

#  else
#    ifdef HAS_QUERYLOCALE

    setlocale(LC_ALL, querylocale(LC_ALL_MASK, uselocale((locale_t) 0)));

#    else

    {
        unsigned int i;

        for (i = 0; i < LC_ALL_INDEX; i++) {
            setlocale(categories[i], do_setlocale_r(categories[i], NULL));
        }
    }

#    endif

    uselocale(LC_GLOBAL_LOCALE);

#  endif
#endif

}

/*

=for apidoc sync_locale

L<C<Perl_setlocale>|perlapi/Perl_setlocale> can be used at any time to query or
change the locale (though changing the locale is antisocial and dangerous on
multi-threaded systems that don't have multi-thread safe locale operations.
(See L<perllocale/Multi-threaded operation>).  Using the system
L<C<setlocale(3)>> should be avoided.  Nevertheless, certain non-Perl libraries
called from XS, such as C<Gtk> do so, and this can't be changed.  When the
locale is changed by XS code that didn't use
L<C<Perl_setlocale>|perlapi/Perl_setlocale>, Perl needs to be told that the
locale has changed.  Use this function to do so, before returning to Perl.

The return value is a boolean: TRUE if the global locale at the time of call
was in effect; and FALSE if a per-thread locale was in effect.  This can be
used by the caller that needs to restore things as-they-were to decide whether
or not to call
L<C<Perl_switch_to_global_locale>|perlapi/switch_to_global_locale>.

=cut
*/

bool
Perl_sync_locale()
{

#ifndef USE_LOCALE

    return TRUE;

#else

    const char * newlocale;
    dTHX;

#  ifdef USE_POSIX_2008_LOCALE

    bool was_in_global_locale = FALSE;
    locale_t cur_obj = uselocale((locale_t) 0);

    /* On Windows, unless the foreign code has turned off the thread-safe
     * locale setting, any plain setlocale() will have affected what we see, so
     * no need to worry.  Otherwise, If the foreign code has done a plain
     * setlocale(), it will only affect the global locale on POSIX systems, but
     * will affect the */
    if (cur_obj == LC_GLOBAL_LOCALE) {

#    ifdef HAS_QUERY_LOCALE

        do_setlocale_c(LC_ALL, setlocale(LC_ALL, NULL));

#    else

        unsigned int i;

        /* We can't trust that we can read the LC_ALL format on the
         * platform, so do them individually */
        for (i = 0; i < LC_ALL_INDEX; i++) {
            do_setlocale_r(categories[i], setlocale(categories[i], NULL));
        }

#    endif

        was_in_global_locale = TRUE;
    }

#  else

    bool was_in_global_locale = TRUE;

#  endif
#  ifdef USE_LOCALE_CTYPE

    newlocale = savepv(do_setlocale_c(LC_CTYPE, NULL));
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
        "%s:%d: %s\n", __FILE__, __LINE__,
        setlocale_debug_string(LC_CTYPE, NULL, newlocale)));
    new_ctype(newlocale);
    Safefree(newlocale);

#  endif /* USE_LOCALE_CTYPE */
#  ifdef USE_LOCALE_COLLATE

    newlocale = savepv(do_setlocale_c(LC_COLLATE, NULL));
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
        "%s:%d: %s\n", __FILE__, __LINE__,
        setlocale_debug_string(LC_COLLATE, NULL, newlocale)));
    new_collate(newlocale);
    Safefree(newlocale);

#  endif
#  ifdef USE_LOCALE_NUMERIC

    newlocale = savepv(do_setlocale_c(LC_NUMERIC, NULL));
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
        "%s:%d: %s\n", __FILE__, __LINE__,
        setlocale_debug_string(LC_NUMERIC, NULL, newlocale)));
    new_numeric(newlocale);
    Safefree(newlocale);

#  endif /* USE_LOCALE_NUMERIC */

    return was_in_global_locale;

#endif

}

#if defined(DEBUGGING) && defined(USE_LOCALE)

STATIC char *
S_setlocale_debug_string(const int category,        /* category number,
                                                           like LC_ALL */
                            const char* const locale,   /* locale name */

                            /* return value from setlocale() when attempting to
                             * set 'category' to 'locale' */
                            const char* const retval)
{
    /* Returns a pointer to a NUL-terminated string in static storage with
     * added text about the info passed in.  This is not thread safe and will
     * be overwritten by the next call, so this should be used just to
     * formulate a string to immediately print or savepv() on. */

    /* initialise to a non-null value to keep it out of BSS and so keep
     * -DPERL_GLOBAL_STRUCT_PRIVATE happy */
    static char ret[256] = "If you can read this, thank your buggy C"
                           " library strlcpy(), and change your hints file"
                           " to undef it";

    my_strlcpy(ret, "setlocale(", sizeof(ret));
    my_strlcat(ret, category_name(category), sizeof(ret));
    my_strlcat(ret, ", ", sizeof(ret));

    if (locale) {
        my_strlcat(ret, "\"", sizeof(ret));
        my_strlcat(ret, locale, sizeof(ret));
        my_strlcat(ret, "\"", sizeof(ret));
    }
    else {
        my_strlcat(ret, "NULL", sizeof(ret));
    }

    my_strlcat(ret, ") returned ", sizeof(ret));

    if (retval) {
        my_strlcat(ret, "\"", sizeof(ret));
        my_strlcat(ret, retval, sizeof(ret));
        my_strlcat(ret, "\"", sizeof(ret));
    }
    else {
        my_strlcat(ret, "NULL", sizeof(ret));
    }

    assert(strlen(ret) < sizeof(ret));

    return ret;
}

#endif

void
Perl_thread_locale_init()
{
    /* Called from a thread on startup*/

#ifdef USE_THREAD_SAFE_LOCALE

    dTHX_DEBUGGING;

    /* C starts the new thread in the global C locale.  If we are thread-safe,
     * we want to not be in the global locale */

     DEBUG_L(PerlIO_printf(Perl_debug_log,
            "%s:%d: new thread, initial locale is %s; calling setlocale\n",
            __FILE__, __LINE__, setlocale(LC_ALL, NULL)));

#  ifdef WIN32

    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);

#  else

    Perl_setlocale(LC_ALL, "C");

#  endif
#endif

}

void
Perl_thread_locale_term()
{
    /* Called from a thread as it gets ready to terminate */

#ifdef USE_THREAD_SAFE_LOCALE

    /* C starts the new thread in the global C locale.  If we are thread-safe,
     * we want to not be in the global locale */

#  ifndef WIN32

    {   /* Free up */
        dVAR;
        locale_t cur_obj = uselocale(LC_GLOBAL_LOCALE);
        if (cur_obj != LC_GLOBAL_LOCALE && cur_obj != PL_C_locale_obj) {
            freelocale(cur_obj);
        }
    }

#  endif
#endif

}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
