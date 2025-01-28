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
 * There is more than the typical amount of variation between platforms with
 * regard to locale handling.  At the end of these introductory comments, are
 * listed various relevent Configuration options, including some that can be
 * used to pretend to some extent that this is being developed on a different
 * platform than it actually is.  This allows you to make changes and catch
 * some errors without having access to those other platforms.
 *
 * This code now has multi-thread-safe locale handling on systems that support
 * that.  This is completely transparent to most XS code.  On earlier systems,
 * it would be possible to emulate thread-safe locales, but this likely would
 * involve a lot of locale switching, and would require XS code changes.
 * Macros could be written so that the code wouldn't have to know which type of
 * system is being used.
 *
 * Table-driven code is used for simplicity and clarity, as many operations
 * differ only in which category is being worked on.  However the system
 * categories need not be small contiguous integers, so do not lend themselves
 * to table lookup.  Instead we have created our own equivalent values which
 * are all small contiguous non-negative integers, and translation functions
 * between the two sets.  For category 'LC_foo', the name of our index is
 * LC_foo_INDEX_.  Various parallel tables, indexed by these, are used for the
 * translation.  The tables are generated at compile-time based on platform
 * characteristics and Configure options.  They hide from the code many of the
 * vagaries of the different locale implementations out there.
 *
 * On unthreaded perls, most operations expand out to just the basic
 * setlocale() calls.  That sort of is true on threaded perls on modern Windows
 * systems where the same API, after set up, is used for thread-safe locale
 * handling.  (But there are complications on Windows due to internal character
 * set issues.)  On other systems, there is a completely different API,
 * specified in POSIX 2008, to do thread-safe locales.  On these systems, our
 * bool_setlocale_2008_i() function is used to hide the different API from the
 * outside.  This makes it completely transparent to most XS code.
 *
 * A huge complicating factor is that the LC_NUMERIC category is normally held
 * in the C locale, except during those relatively rare times when it needs to
 * be in the underlying locale.  There is a bunch of code to accomplish this,
 * and to allow easy switches from one state to the other.
 *
 * In addition, the setlocale equivalents have versions for the return context,
 * 'void' and 'bool', besides the full return value.  This can present
 * opportunities for avoiding work.  We don't have to necessarily create a safe
 * copy to return if no return is desired.
 *
 * There are 3.5 major implementations here; which one chosen depends on what
 * the platform has available, and Configuration options.
 *
 * 1) Raw posix_setlocale().  This implementation is basically the libc
 *    setlocale(), with possibly minor tweaks.  This is used for startup, and
 *    always for unthreaded perls, and when the API for safe locale threading
 *    is identical to the unsafe API (Windows, currently).
 *
 *    This implementation is composed of two layers:
 *      a)  posix_setlocale() implements the libc setlocale().  In most cases,
 *          it is just an alias for the libc version.  But Windows doesn't
 *          fully conform to the POSIX standard, and this is a layer on top of
 *          libc to bring it more into conformance.  And in Configurations
 *          where perl is to ignore some locale categories that the libc
 *          setlocale() knows about, there is a layer to cope with that.
 *      b)  stdized_setlocale() is a layer above a) that fixes some vagaries in
 *          the return value of the libc setlocale().  On most platforms this
 *          layer is empty; in order to be activated, it requires perl to be
 *          Configured with a parameter indicating the platform's defect.  The
 *          current ones are listed at the definition of the macro.
 *
 * 2) An implementation that adds a minimal layer above implementation 1),
 *    making that implementation uninterruptible and returning a
 *    per-thread/per-category value.
 *
 * 3a and 3b) An implementation of POSIX 2008 thread-safe locale handling,
 *    hiding from the programmer the completely different API for this.
 *    This automatically makes almost all code thread-safe without need for
 *    changes.  This implementation is chosen on threaded perls when the
 *    platform properly supports the POSIX 2008 functions, and when there is no
 *    manual override to the contrary passed to Configure.
 *
 *    3a) is when the platform has a documented reliable querylocale() function
 *        or equivalent that is selected to be used.
 *    3b) is when we have to emulate that functionality.
 *
 *    Unfortunately, it seems that some platforms that claim to support these
 *    are buggy, in one way or another.  There are workarounds encoded here,
 *    where feasible, for platforms where the bugs are amenable to that
 *    (glibc, for example).  But other platforms instead don't use this
 *    implementation.
 *
 * z/OS (os390) is an outlier.  Locales really don't work under threads when
 * either the radix character isn't a dot, or attempts are made to change
 * locales after the first thread is created.  The reason is that IBM has made
 * it thread-safe by refusing to change locales (returning failure if
 * attempted) any time after an application has called pthread_create() to
 * create another thread.  The expectation is that an application will set up
 * its locale information before the first fork, and be stable thereafter.  But
 * perl toggles LC_NUMERIC if the locale's radix character isn't a dot, as do
 * the other toggles, which are less common.
 *
 * Associated with each implementation are three sets of macros that translate
 * a consistent API into what that implementation needs.  Each set consists of
 * three macros with the suffixes:
 *  _c  Means the argument is a locale category number known at compile time.
 *          An example would be LC_TIME.  This token is a compile-time constant
 *          and can be passed to a '_c' macro.
 *  _r  Means the argument is a locale category number whose value might not be
 *      known until runtime
 *  _i  Means the argument is our internal index of a locale category
 *
 * The three sets are:    ('_X'  means one of '_c', '_r', '_i')
 * 1) bool_setlocale_X()
 *      This calls the appropriate setlocale()-equivalent for the
 *      implementation, with the category and new locale.  The input locale is
 *      not necessarily valid, so the return is true or false depending on
 *      whether or not the setlocale() succeeded.  This is not used for
 *      querying the locale, so the input locale must not be NULL.
 *
 *      This macro is suitable for toggling the locale back and forth during an
 *      operation.  For example, the names of days and months under LC_TIME are
 *      strings that are also subject to LC_CTYPE.  If the locales of these two
 *      categories differ, mojibake can result on many platforms.  The code
 *      here will toggle LC_CTYPE into the locale of LC_TIME temporarily to
 *      avoid this.
 *
 *      Several categories require extra work when their locale is changed.
 *      LC_CTYPE, for example, requires the calculation of the table of which
 *      characters fold to which others under /i pattern matching or fc(), as
 *      folding is not a concept in POSIX.  This table isn't needed when the
 *      LC_CTYPE locale gets toggled during an operation, and will be toggled
 *      back before return to the caller.  To save work that would be
 *      discarded, the bool_setlocale_X() implementations don't do this extra
 *      work.  Instead, there is a separate function for just this purpose to
 *      be done before control is transferred back to the external caller.  All
 *      categories that have such requirements have such a function.  The
 *      update_functions[] array contains pointers to them (or NULL for
 *      categories which don't need a function).
 *
 *      Care must be taken to remember to call the separate function before
 *      returning to an external caller, and to not use things it updates
 *      before its call.  An alternative approach would be to have
 *      bool_setlocale_X() always call the update, which would return
 *      immediately if a flag wasn't set indicating it was time to actually
 *      perform it.
 *
 * 2) void_setlocale_X()
 *      This is like bool_setlocale_X(), but it is used only when it is
 *      expected that the call must succeed, or something is seriously wrong.
 *      A panic is issued if it fails.  The caller uses this form when it just
 *      wants to assume things worked.
 *
 * 3) querylocale_X()
 *      This returns a string that specifies the current locale for the given
 *      category given by the input argument.  The string is safe from other
 *      threads zapping it, and the caller need not worry about freeing it, but
 *      it may be mortalized, so must be copied if you need to preserve it
 *      across calls, or long term.  This returns the actual current locale,
 *      not the nominal.  These differ, for example, when LC_NUMERIC is
 *      supposed to be a locale whose decimal radix character is a comma.  As
 *      mentioned above, Perl actually keeps this category set to C in such
 *      circumstances so that XS code can just assume a dot radix character.
 *      querylocale_X() returns the locale that libc has stored at this moment,
 *      so most of the time will return a locale whose radix character is a
 *      dot.  The macro query_nominal_locale_i() can be used to get the nominal
 *      locale that an external caller would expect, for all categories except
 *      LC_ALL.  For that, you can use the function
 *      S_calculate_LC_ALL_string().  Or S_native_querylocale_i() will operate
 *      on any category.
 *
 * The underlying C API that this implements uses category numbers, hence the
 * code is structured to use '_r' at the API level to convert to indexes, which
 * are then used internally with the '_i' forms.
 *
 * The splitting apart into setting vs querying means that the return value of
 * the bool macros is not subject to potential clashes with other threads,
 * eliminating any need for the calling code to worry about that and get it
 * wrong.  Whereas, you do have to think about thread interactions when using a
 * query.
 *
 * Additionally, for the implementations where there aren't any complications,
 * a setlocale_i() is defined that is like plain setlocale(), returning the new
 * locale.  Thus it combines a bool_setlocale_X() with a querylocale_X().  It
 * is used only for performance on implementations that allow it, such as
 * non-threaded perls.
 *
 * There are also a few other macros herein that use this naming convention to
 * describe their category parameter.
 *
 * Relevant Configure options
 *
 *      -Accflags=-DNO_LOCALE
 *          This compiles perl to always use the C locale, ignoring any
 *          attempts to change it.  This could be useful on platforms with a
 *          crippled locale implementation.
 *
 *      -Accflags=-DNO_THREAD_SAFE_LOCALE
 *          Even if thread-safe operations are available on this platform and
 *          would otherwise be used (because this is a perl with multiplicity),
 *          perl is compiled to not use them.  This could be useful on
 *          platforms where the libc is buggy.
 *
 *      -Accflags=-DNO_POSIX_2008_LOCALE
 *          Even if the libc locale operations specified by the Posix 2008
 *          Standard are available on this platform and would otherwise be used
 *          (because this is a threaded perl), perl is compiled to not use
 *          them.  This could be useful on platforms where the libc is buggy.
 *          This is like NO_THREAD_SAFE_LOCALE, but has no effect on platforms
 *          that don't have these functions.
 *
 *      -Accflags=-DUSE_POSIX_2008_LOCALE
 *          Normally, setlocale() is used for locale operations on perls
 *          compiled without threads.  This option causes the locale operations
 *          defined by the Posix 2008 Standard to always be used instead.  This
 *          could be useful on platforms where the libc setlocale() is buggy.
 *
 *      -Accflags=-DNO_THREAD_SAFE_QUERYLOCALE
 *          This applies only to platforms that have a querylocale() libc
 *          function.  perl assumes that that function is thread-safe, unless
 *          overridden by this, typically in a hints file.  When overridden,
 *          querylocale() is called only while the locale mutex is locked, and
 *          the result is copied to a per-thread place before unlocking.
 *
 *      -Accflags=-DNO_USE_NL_LOCALE_NAME
 *          glibc has an undocumented equivalent function to querylocale(),
 *          which our experience indicates is reliable.  But you can forbid its
 *          use by specifying this Configure option (with no effect on systems
 *          lacking it).  When this is function is enabled, it removes the need
 *          for perl to keep its own records, hence is more efficient and
 *          guaranteed to be accurate.
 *
 *      -Accflags=-DNO_LOCALE_CTYPE
 *      -Accflags=-DNO_LOCALE_NUMERIC
 *          etc.
 *
 *          If the named category(ies) does(do) not exist on this platform,
 *          these have no effect.  Otherwise they cause perl to be compiled to
 *          always keep the named category(ies) in the C locale.
 *
 *      -Accflags=-DHAS_BROKEN_SETLOCALE_QUERY_LC_ALL
 *          This would be set in a hints file to tell perl that doing a libc
 *              setlocale(LC_ALL, NULL)
 *          can give erroneous results, and perl will compensate to get the
 *          correct results.  This is known to be a problem in earlier AIX
 *          versions
 *
 *      -Accflags=-DHAS_LF_IN_SETLOCALE_RETURN
 *          This would be set in a hints file to tell perl that a libc
 *          setlocale() can return results containing \n characters that need
 *          to be stripped off.  khw believes there aren't any such platforms
 *          still in existence.
 *
 *      -Accflags=-DLIBC_HANDLES_MISMATCHED_CTYPE
 *          Consider the name of a month in some language, Chinese for example.
 *          If LC_TIME has been set to a Chinese locale, strftime() can be used
 *          to generate the Chinese month name for any given date, by using the
 *          %B format.  But also suppose that LC_CTYPE is set to, say, "C".
 *          The return from strftime() on many platforms will be mojibake given
 *          that no Chinese month name is composed of just ASCII characters.
 *          Perl handles this for you by automatically toggling LC_CTYPE to
 *          whatever LC_TIME is during the execution of strftime(), and
 *          afterwards restoring it to its prior value.  But the strftime()
 *          (and similar functions) in some libc implementations already do
 *          this toggle, meaning perl's action is redundant.  You can tell perl
 *          that a libc does this by setting this Configure option, and it will
 *          skip its syncing LC_CTYPE and whatever the other locale is.
 *          Currently, perl ignores this Configuration option and  syncs anyway
 *          for LC_COLLATE-related operations, due to perl's internal needs.
 *
 *      -Accflags=USE_FAKE_LC_ALL_POSITIONAL_NOTATION
 *          This is used when developing Perl on a platform that uses
 *          'name=value;' notation to represent LC_ALL when not all categories
 *          are the same.  When so compiled, much of the code gets compiled
 *          and exercised that applies to platforms that instead use positional
 *          notation.  This allows for finding many bugs in that portion of the
 *          implementation, without having to access such a platform.
 *
 *      -Accflags=-DWIN32_USE_FAKE_OLD_MINGW_LOCALES
 *          This is used when developing Perl on a non-Windows platform to
 *          compile and exercise much of the locale-related code that instead
 *          applies to MingW platforms that don't use the more modern UCRT
 *          library.  This allows for finding many bugs in that portion of the
 *          implementation, without having to access such a platform.
 */

/* If the environment says to, we can output debugging information during
 * initialization.  This is done before option parsing, and before any thread
 * creation, so can be a file-level static.  (Must come before #including
 * perl.h) */
#include "config.h"

/* Returns the Unix errno portion; ignoring any others.  This is a macro here
 * instead of putting it into perl.h, because unclear to khw what should be
 * done generally. */
#define GET_ERRNO   saved_errno

#ifdef DEBUGGING
static int debug_initialization = 0;
#  define DEBUG_INITIALIZATION_set(v) (debug_initialization = v)
#  define DEBUG_LOCALE_INITIALIZATION_  debug_initialization

#  ifdef HAS_EXTENDED_OS_ERRNO
     /* Output the non-zero errno and/or the non-zero extended errno */
#    define DEBUG_ERRNO                                                     \
        dSAVE_ERRNO; dTHX;                                                  \
        int extended = get_extended_os_errno();                             \
        const char * errno_string;                                          \
        if (GET_ERRNO == 0) { /* Skip output if both errno types are 0 */   \
            if (LIKELY(extended == 0)) errno_string = "";                   \
            else errno_string = Perl_form(aTHX_ "; $^E=%d", extended);      \
        }                                                                   \
        else if (LIKELY(extended == GET_ERRNO))                             \
            errno_string = Perl_form(aTHX_ "; $!=%d", GET_ERRNO);           \
        else errno_string = Perl_form(aTHX_ "; $!=%d, $^E=%d",              \
                                                    GET_ERRNO, extended);
#  else
     /* Output the errno, if non-zero */
#    define DEBUG_ERRNO                                                     \
        dSAVE_ERRNO;                                                        \
        const char * errno_string = "";                                     \
        if (GET_ERRNO != 0) {                                               \
            dTHX;                                                           \
            errno_string = Perl_form(aTHX_ "; $!=%d", GET_ERRNO);           \
        }
#  endif

/* Automatically include the caller's file, and line number in debugging output;
 * and the errno (and/or extended errno) if non-zero.  On threaded perls add
 * the aTHX too. */
#  if defined(MULTIPLICITY) && ! defined(NO_LOCALE_THREADS)
#    define DEBUG_PRE_STMTS                                                 \
        DEBUG_ERRNO;                                                        \
        PerlIO_printf(Perl_debug_log, "\n%s: %" LINE_Tf ": 0x%p%s: ",       \
                                      __FILE__, (line_t)__LINE__, aTHX_     \
                                      errno_string);
#  else
#    define DEBUG_PRE_STMTS                                                 \
        DEBUG_ERRNO;                                                        \
        PerlIO_printf(Perl_debug_log, "\n%s: %" LINE_Tf "%s: ",             \
                                      __FILE__, (line_t)__LINE__,           \
                                      errno_string);
#  endif
#  define DEBUG_POST_STMTS  RESTORE_ERRNO;
#else
#  define debug_initialization 0
#  define DEBUG_INITIALIZATION_set(v)
#  define DEBUG_PRE_STMTS
#  define DEBUG_POST_STMTS
#endif

#include "EXTERN.h"
#define PERL_IN_LOCALE_C
#include "perl.h"

/* Some platforms require LC_CTYPE to be congruent with the category we are
 * looking for.  XXX This still presumes that we have to match COLLATE and
 * CTYPE even on platforms that apparently handle this. */
#if defined(USE_LOCALE_CTYPE) && ! defined(LIBC_HANDLES_MISMATCHED_CTYPE)
#  define WE_MUST_DEAL_WITH_MISMATCHED_CTYPE    /* no longer used; kept for
                                                   possible future use */
#  define start_DEALING_WITH_MISMATCHED_CTYPE(locale)                          \
        const char * orig_CTYPE_locale = toggle_locale_c(LC_CTYPE, locale)
#  define end_DEALING_WITH_MISMATCHED_CTYPE(locale)                            \
        restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);
#else
#  define start_DEALING_WITH_MISMATCHED_CTYPE(locale)
#  define end_DEALING_WITH_MISMATCHED_CTYPE(locale)
#endif

#ifdef WIN32_USE_FAKE_OLD_MINGW_LOCALES

   /* Use -Accflags=-DWIN32_USE_FAKE_OLD_MINGW_LOCALES on a POSIX or *nix box
    * to get a semblance of pretending the locale handling is that of a MingW
    * that doesn't use UCRT (hence 'OLD' in the name).  This exercizes code
    * paths that are not compiled on non-Windows boxes, and allows for ASAN
    * and PERL_MEMLOG.  This is thus a way to see if locale.c on Windows is
    * likely going to compile, without having to use a real Win32 box.  And
    * running the test suite will verify to a large extent our logic and memory
    * allocation handling for such boxes.  Of course the underlying calls are
    * to the POSIX libc, so any differences in implementation between those and
    * the Windows versions will not be caught by this. */

#  define WIN32
#  undef P_CS_PRECEDES
#  undef CURRENCY_SYMBOL
#  define CP_UTF8 -1
#  undef _configthreadlocale
#  define _configthreadlocale(arg) NOOP

#  define MultiByteToWideChar(cp, flags, byte_string, m1, wstring, req_size) \
                    (PERL_UNUSED_ARG(cp),                                    \
                     mbsrtowcs(wstring, &(byte_string), req_size, NULL) + 1)
#  define WideCharToMultiByte(cp, flags, wstring, m1, byte_string,          \
                              req_size, default_char, found_default_char)   \
                    (PERL_UNUSED_ARG(cp),                                   \
                     wcsrtombs(byte_string, &(wstring), req_size, NULL) + 1)

#  ifdef USE_LOCALE

static const wchar_t * wsetlocale_buf = NULL;
static Size_t wsetlocale_buf_size = 0;

#    ifdef MULTIPLICITY

static PerlInterpreter * wsetlocale_buf_aTHX = NULL;

#    endif

STATIC
const wchar_t *
S_wsetlocale(const int category, const wchar_t * wlocale)
{
    /* Windows uses a setlocale that takes a wchar_t* locale.  Other boxes
     * don't have this, so this Windows replacement converts the wchar_t input
     * to plain 'char*', calls plain setlocale(), and converts the result back
     * to 'wchar_t*' */

    const char * byte_locale = NULL;
    if (wlocale) {
        byte_locale = Win_wstring_to_byte_string(CP_UTF8, wlocale);
    }

    const char * byte_result = setlocale(category, byte_locale);
    Safefree(byte_locale);
    if (byte_result == NULL) {
        return NULL;
    }

    const wchar_t * wresult = Win_byte_string_to_wstring(CP_UTF8, byte_result);

    if (! wresult) {
        return NULL;
    }

    /* Emulate a global static memory return from wsetlocale().  This currently
     * leaks at process end; would require changing LOCALE_TERM to fix that */
    Size_t string_size = wcslen(wresult) + 1;

    if (wsetlocale_buf_size == 0) {
        Newx(wsetlocale_buf, string_size, wchar_t);
        wsetlocale_buf_size = string_size;

#  ifdef MULTIPLICITY

        dTHX;
        wsetlocale_buf_aTHX = aTHX;

#  endif

    }
    else if (string_size > wsetlocale_buf_size) {
        Renew(wsetlocale_buf, string_size, wchar_t);
        wsetlocale_buf_size = string_size;
    }

    Copy(wresult, wsetlocale_buf, string_size, wchar_t);
    Safefree(wresult);

    return wsetlocale_buf;
}

#  define _wsetlocale(category, wlocale)  S_wsetlocale(category, wlocale)
#  endif
#endif  /* WIN32_USE_FAKE_OLD_MINGW_LOCALES */

/* 'for' loop headers to hide the necessary casts */
#define for_category_indexes_between(i, m, n)                               \
    for (locale_category_index i = (locale_category_index) (m);             \
         i <= (locale_category_index) (n);                                  \
         i = (locale_category_index) ((int) i + 1))
#define for_all_individual_category_indexes(i)                              \
        for_category_indexes_between(i, 0, LC_ALL_INDEX_ - 1)
#define for_all_but_0th_individual_category_indexes(i)                      \
        for_category_indexes_between(i, 1, LC_ALL_INDEX_ - 1)
#define for_all_category_indexes(i)                                         \
        for_category_indexes_between(i, 0, LC_ALL_INDEX_)

#ifdef USE_LOCALE
#  if defined(USE_FAKE_LC_ALL_POSITIONAL_NOTATION) && defined(LC_ALL)

/* This simulates an underlying positional notation for LC_ALL when compiled on
 * a system that uses name=value notation.  Use this to develop on Linux and
 * make a quick check that things have some chance of working on a positional
 * box.  Enable by adding to the Congfigure parameters:
 *      -Accflags=USE_FAKE_LC_ALL_POSITIONAL_NOTATION
 *
 * NOTE it redefines setlocale() and usequerylocale()
 * */

STATIC const char *
S_positional_name_value_xlation(const char * locale, bool direction)
{   /* direction == 1 is from name=value to positional
       direction == 0 is from positional to name=value */
    assert(locale);

    dTHX;
    const char * individ_locales[LC_ALL_INDEX_] = { NULL };

    /* This parses either notation */
    switch (parse_LC_ALL_string(locale,
                                (const char **) &individ_locales,
                                no_override,  /* Handled by other code */
                                false,      /* Return only [0] if suffices */
                                false,      /* Don't panic on error */
                                __LINE__))
    {
      default:      /* Some compilers don't realize that below is the complete
                       list of the available enum values */
      case invalid:
        return NULL;

      case no_array:
        return locale;
      case only_element_0:
        SAVEFREEPV(individ_locales[0]);
        return individ_locales[0];
      case full_array:
       {
        calc_LC_ALL_format  format = (direction)
                                     ? EXTERNAL_FORMAT_FOR_SET
                                     : INTERNAL_FORMAT;
        const char * retval = calculate_LC_ALL_string(individ_locales,
                                                      format,
                                                      WANT_TEMP_PV,
                                                      __LINE__);

        for_all_individual_category_indexes(i) {
            Safefree(individ_locales[i]);
        }

        return retval;
       }
    }
}

STATIC const char *
S_positional_setlocale(int cat, const char * locale)
{
    if (cat != LC_ALL) return setlocale(cat, locale);

    if (locale && strNE(locale, "")) {
        locale = S_positional_name_value_xlation(locale, 0);
        if (! locale) return NULL;
    }

    locale = setlocale(cat, locale);
    if (locale == NULL) return NULL;
    return S_positional_name_value_xlation(locale, 1);
}

#    undef setlocale
#    define setlocale(a,b)  S_positional_setlocale(a,b)
#    ifdef USE_POSIX_2008_LOCALE

STATIC locale_t
S_positional_newlocale(int mask, const char * locale, locale_t base)
{
    assert(locale);

    if (mask != LC_ALL_MASK) return newlocale(mask, locale, base);

    if (strNE(locale, "")) locale = S_positional_name_value_xlation(locale, 0);
    if (locale == NULL) return NULL;
    return newlocale(LC_ALL_MASK, locale, base);
}

#    undef newlocale
#    define newlocale(a,b,c)  S_positional_newlocale(a,b,c)
#    endif
#  endif
#endif  /* End of fake positional notation */

#include "reentr.h"

#ifdef I_WCHAR
#  include <wchar.h>
#endif
#ifdef I_WCTYPE
#  include <wctype.h>
#endif

 /* The main errno that gets used is this one, on platforms that support it */
#ifdef EINVAL
#  define SET_EINVAL  SETERRNO(EINVAL, LIB_INVARG)
#else
#  define SET_EINVAL
#endif

/* This is a starting guess as to when this is true.  It definititely isn't
 * true on *BSD where positional LC_ALL notation is used.  Likely this will end
 * up being defined in hints files. */
#ifdef PERL_LC_ALL_USES_NAME_VALUE_PAIRS
#  define NEWLOCALE_HANDLES_DISPARATE_LC_ALL
#endif

/* But regardless, we have to look at individual categories if some are
 * ignored.  */
#ifdef HAS_IGNORED_LOCALE_CATEGORIES_
#  undef NEWLOCALE_HANDLES_DISPARATE_LC_ALL
#endif
#ifdef USE_LOCALE

/* Not all categories need be set to the same locale.  This macro determines if
 * 'name' which represents LC_ALL is uniform or disparate.  There are two
 * situations: 1) the platform uses unordered name=value pairs; 2) the platform
 * uses ordered positional values, with a separator string between them */
#  ifdef PERL_LC_ALL_SEPARATOR   /* positional */
#    define is_disparate_LC_ALL(name)  cBOOL(instr(name, PERL_LC_ALL_SEPARATOR))
#  else     /* name=value */

    /* In the, hopefully never occurring, event that the platform doesn't use
     * either mechanism for disparate LC_ALL's, assume the name=value pairs
     * form, rather than taking the extreme step of refusing to compile.  Many
     * programs won't have disparate locales, so will generally work */
#    define PERL_LC_ALL_SEPARATOR  ";"
#    define is_disparate_LC_ALL(name)  cBOOL(   strchr(name, ';')   \
                                             && strchr(name, '='))
#  endif

/* It is possible to compile perl to always keep any individual category in the
 * C locale.  This would be done where the implementation on a platform is
 * flawed or incomplete.  At the time of this writing, for example, OpenBSD has
 * not implemented LC_COLLATE beyond the C locale.  The 'category_available[]'
 * table is a bool that says whether a category is changeable, or must be kept
 * in C.  This macro substitutes C for the locale appropriately, expanding to
 * nothing on the more typical case where all possible categories present on
 * the platform are handled. */
#  if defined(HAS_IGNORED_LOCALE_CATEGORIES_)       \
   || defined(HAS_MISSING_LANGINFO_ITEM_)
#    define need_to_override_category(i)  (! category_available[i])
#    define override_ignored_category(i, new_locale)                        \
                    ((need_to_override_category(i)) ? "C" : (new_locale))
#  else
#    define need_to_override_category(i)  0
#    define override_ignored_category(i, new_locale)  (new_locale)
#  endif

PERL_STATIC_INLINE const char *
S_mortalized_pv_copy(pTHX_ const char * const pv)
{
    PERL_ARGS_ASSERT_MORTALIZED_PV_COPY;

    /* Copies the input pv, and arranges for it to be freed at an unspecified
     * later time. */

    if (pv == NULL) {
        return NULL;
    }

    const char * copy = savepv(pv);
    SAVEFREEPV(copy);
    return copy;
}

#endif

/* Default values come from the C locale */
#define C_codeset "ANSI_X3.4-1968" /* Only in some Configurations, and usually
                                      a single instance, so is a #define */
static const char C_decimal_point[] = ".";

#if defined(HAS_NL_LANGINFO_L) || defined(HAS_NL_LANGINFO)
#  define HAS_SOME_LANGINFO
#endif

#if (defined(USE_LOCALE_NUMERIC) && ! defined(TS_W32_BROKEN_LOCALECONV))    \
 || ! (   defined(USE_LOCALE_NUMERIC)                                       \
       && (defined(HAS_SOME_LANGINFO) || defined(HAS_LOCALECONV)))
static const char C_thousands_sep[] = "";
#endif

/* Is the C string input 'name' "C" or "POSIX"?  If so, and 'name' is the
 * return of setlocale(), then this is extremely likely to be the C or POSIX
 * locale.  However, the output of setlocale() is documented to be opaque, but
 * the odds are extremely small that it would return these two strings for some
 * other locale.  Note that VMS includes many non-ASCII characters in these two
 * locales as controls and punctuation (below are hex bytes):
 *   cntrl:  84-97 9B-9F
 *   punct:  A1-A3 A5 A7-AB B0-B3 B5-B7 B9-BD BF-CF D1-DD DF-EF F1-FD
 * Oddly, none there are listed as alphas, though some represent alphabetics
 * https://www.nntp.perl.org/group/perl.perl5.porters/2013/02/msg198753.html */
#define isNAME_C_OR_POSIX(name)                                              \
                             (   (name) != NULL                              \
                              && (( *(name) == 'C' && (*(name + 1)) == '\0') \
                                   || strEQ((name), "POSIX")))

/* If this interface to nl_langinfo() isn't defined by embed.fnc, it means it
 * isn't available on this platform, so instead emulate it */
#ifndef langinfo_sv_i
#  define langinfo_sv_i(i, c, l, s, u)                                      \
                        (PERL_UNUSED_VAR(c), emulate_langinfo(i, l, s, u))
#endif

/* In either case, create a version that takes things like 'LC_NUMERIC' as a
 * parameter */
#define langinfo_sv_c(item, category, locale, sv, utf8ness)                 \
        langinfo_sv_i(item, category##_INDEX_, locale, sv, utf8ness)

/* The normal method for interfacing with nl_langinfo() in this file is to use
 * a scratch buffer (whose existence is hidden from the caller by these
 * macros). */
#define langinfo_i(item, index, locale, utf8ness)                           \
        langinfo_sv_i(item, index, locale, PL_scratch_langinfo, utf8ness)

#define langinfo_c(item, category, locale, utf8ness)                        \
        langinfo_i(item, category##_INDEX_, locale, utf8ness)

#ifndef USE_LOCALE  /* A no-op unless locales are enabled */
#  define toggle_locale_i(index, locale)                                    \
    ((const char *) (PERL_UNUSED_VAR(locale), NULL))
#  define restore_toggled_locale_i(index, locale)  PERL_UNUSED_VAR(locale)
#else
#  define toggle_locale_i(index, locale)                                    \
                 S_toggle_locale_i(aTHX_ index, locale, __LINE__)
#  define restore_toggled_locale_i(index, locale)                           \
                 S_restore_toggled_locale_i(aTHX_ index, locale, __LINE__)
#endif

#  define toggle_locale_c(cat, locale)  toggle_locale_i(cat##_INDEX_, locale)
#  define restore_toggled_locale_c(cat, locale)                             \
                             restore_toggled_locale_i(cat##_INDEX_, locale)
#ifdef USE_LOCALE
#  ifdef DEBUGGING
#    define setlocale_debug_string_i(index, locale, result)                 \
            my_setlocale_debug_string_i(index, locale, result, __LINE__)
#    define setlocale_debug_string_c(category, locale, result)              \
                setlocale_debug_string_i(category##_INDEX_, locale, result)
#    define setlocale_debug_string_r(category, locale, result)              \
             setlocale_debug_string_i(get_category_index(category),         \
                                      locale, result)
#  endif

/* On systems without LC_ALL, pretending it exists anyway simplifies things.
 * Choose a value for it that is very unlikely to clash with any actual
 * category */
#  define FAKE_LC_ALL  PERL_INT_MIN

/* Below are parallel arrays for locale information indexed by our mapping of
 * category numbers into small non-negative indexes.  locale_table.h contains
 * an entry like this for each individual category used on this system:
 *      PERL_LOCALE_TABLE_ENTRY(CTYPE, S_new_ctype)
 *
 * Each array redefines PERL_LOCALE_TABLE_ENTRY to generate the information
 * needed for that array, and #includes locale_table.h to get the valid
 * categories.
 *
 * An entry for the conglomerate category LC_ALL is added here, immediately
 * following the individual categories.  (The treatment for it varies, so can't
 * be in locale_table.h.)
 *
 * Following this, each array ends with an entry for illegal categories.  All
 * category numbers unknown to perl get mapped to this entry.  This is likely
 * to be a parameter error from the calling program; but it could be that this
 * platform has a category we don't know about, in which case it needs to be
 * added, using the paradigm of one of the existing categories. */

/* The first array is the locale categories perl uses on this system, used to
 * map our index back to the system's category number. */
STATIC const int categories[] = {

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)  LC_ ## name,
#  include "locale_table.h"

#  ifdef LC_ALL
    LC_ALL,
#  else
    FAKE_LC_ALL,
#  endif

   (FAKE_LC_ALL + 1)    /* Entry for unknown category; this number is unlikely
                           to clash with a real category */
};

# define GET_NAME_AS_STRING(token)  # token
# define GET_LC_NAME_AS_STRING(token) GET_NAME_AS_STRING(LC_ ## token)

/* The second array is the category names. */
STATIC const char * const category_names[] = {

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)  GET_LC_NAME_AS_STRING(name),
#  include "locale_table.h"

#  ifdef LC_ALL
#    define LC_ALL_STRING  "LC_ALL"
#  else
#    define LC_ALL_STRING  "If you see this, it is a bug in perl;"      \
                           " please report it via perlbug"
#  endif

    LC_ALL_STRING,

#  define LC_UNKNOWN_STRING  "Locale category unknown to Perl; if you see"  \
                             " this, it is a bug in perl; please report it" \
                             " via perlbug"
    LC_UNKNOWN_STRING
};

STATIC const Size_t category_name_lengths[] = {

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)                          \
                                    STRLENs(GET_LC_NAME_AS_STRING(name)),
#  include "locale_table.h"

    STRLENs(LC_ALL_STRING),
    STRLENs(LC_UNKNOWN_STRING)
};

/* Each entry includes space for the '=' and ';' */
#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)                          \
                                + STRLENs(GET_LC_NAME_AS_STRING(name)) + 2

STATIC const Size_t lc_all_boiler_plate_length = 1  /* space for trailing NUL */
#  include "locale_table.h"
;

/* A few categories require additional setup when they are changed.  This table
 * points to the functions that do that setup */
STATIC void (*update_functions[]) (pTHX_ const char *, bool force) = {

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)  call_back,
#  include "locale_table.h"

    S_new_LC_ALL,
    NULL,   /* No update for unknown category */
};

#  if defined(HAS_IGNORED_LOCALE_CATEGORIES_)       \
   || defined(HAS_MISSING_LANGINFO_ITEM_)

/* Indicates if each category on this platform is available to use not in
 * the C locale */
STATIC const bool category_available[] = {

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)  LC_ ## name ## _AVAIL_,
#  include "locale_table.h"

#  ifdef LC_ALL
    true,
#  else
    false,
#  endif

    false   /* LC_UNKNOWN_AVAIL_ */
};

#  endif
#  if defined(USE_POSIX_2008_LOCALE)

STATIC const int category_masks[] = {

#    undef PERL_LOCALE_TABLE_ENTRY
#    define PERL_LOCALE_TABLE_ENTRY(name, call_back)  LC_ ## name ## _MASK,
#    include "locale_table.h"

    LC_ALL_MASK,    /* Will rightly refuse to compile unless this is defined */
    0               /* Empty mask for unknown category */
};

#  endif
#  if ! defined(PERL_LC_ALL_USES_NAME_VALUE_PAIRS)

/* On platforms that use positional notation for expressing LC_ALL, this maps
 * the position of each category to our corresponding internal index for it.
 * This is initialized at run time if needed.  LC_ALL_INDEX_ is not legal for
 * an individual locale, hence marks the elements here as not actually
 * initialized. */
STATIC
unsigned int
map_LC_ALL_position_to_index[LC_ALL_INDEX_] = { LC_ALL_INDEX_ };

#  endif
#endif
#if defined(USE_LOCALE) || defined(DEBUGGING)

STATIC const char *
S_get_displayable_string(pTHX_
                         const char * const s,
                         const char * const e,
                         const bool is_utf8)
{
    PERL_ARGS_ASSERT_GET_DISPLAYABLE_STRING;

    if (e <= s) {
        return "";
    }

    const char * t = s;
    bool prev_was_printable = TRUE;
    bool first_time = TRUE;
    char * ret;

    /* Worst case scenario: All are non-printable so have a blank between each.
     * If UTF-8, all are the largest possible code point; otherwise all are a
     * single byte.  '(2 + 1)'  is from each byte takes 2 characters to
     * display, and a blank (or NUL for the final one) after it */
    const Size_t size = (e - s) * (2 + 1) * ((is_utf8) ? UVSIZE : 1);
    Newxz(ret, size, char);
    SAVEFREEPV(ret);

    while (t < e) {
        UV cp = (is_utf8)
                ?  utf8_to_uvchr_buf((U8 *) t, e, NULL)
                : * (U8 *) t;
        if (isPRINT(cp)) {
            if (! prev_was_printable) {
                my_strlcat(ret, " ", size);
            }

            /* Escape these to avoid any ambiguity */
            if (cp == ' ' || cp == '\\') {
                my_strlcat(ret, "\\", size);
            }
            my_strlcat(ret, Perl_form(aTHX_ "%c", (U8) cp), size);
            prev_was_printable = TRUE;
        }
        else {
            if (! first_time) {
                my_strlcat(ret, " ", size);
            }
            my_strlcat(ret, Perl_form(aTHX_ "%02" UVXf, cp), size);
            prev_was_printable = FALSE;
        }
        t += (is_utf8) ? UTF8SKIP(t) : 1;
        first_time = FALSE;
    }

    return ret;
}

#endif
#ifdef USE_LOCALE

# define get_category_index(cat) get_category_index_helper(cat, NULL, __LINE__)

STATIC locale_category_index
S_get_category_index_helper(pTHX_ const int category, bool * succeeded,
                                  const line_t caller_line)
{
    PERL_ARGS_ASSERT_GET_CATEGORY_INDEX_HELPER;

    /* Given a category, return the equivalent internal index we generally use
     * instead, warn or panic if not found. */

    locale_category_index i;

#  undef PERL_LOCALE_TABLE_ENTRY
#  define PERL_LOCALE_TABLE_ENTRY(name, call_back)                          \
                    case LC_ ## name: i =  LC_ ## name ## _INDEX_; break;

    switch (category) {

#  include "locale_table.h"
#  ifdef LC_ALL
      case LC_ALL: i =  LC_ALL_INDEX_; break;
#  endif

      default: goto unknown_locale;
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "index of category %d (%s) is %d;"
                           " called from %" LINE_Tf "\n",
                           category, category_names[i], i, caller_line));

    if (succeeded) {
        *succeeded = true;
    }

    return i;

  unknown_locale:

    if (succeeded) {
        *succeeded = false;
        return LC_ALL_INDEX_;   /* Arbitrary */
    }

    locale_panic_via_(Perl_form(aTHX_ "Unknown locale category %d", category),
                      __FILE__, caller_line);
    NOT_REACHED; /* NOTREACHED */
}

#endif /* ifdef USE_LOCALE */

void
Perl_force_locale_unlock(pTHX)
{
    /* Remove any locale mutex, in preperation for an inglorious termination,
     * typically a  panic */

#if defined(USE_LOCALE_THREADS)

    /* If recursively locked, clear all at once */
    if (PL_locale_mutex_depth > 1) {
        PL_locale_mutex_depth = 1;
    }

    if (PL_locale_mutex_depth > 0) {
        LOCALE_UNLOCK_;
    }

#endif

}

#ifdef USE_POSIX_2008_LOCALE

STATIC locale_t
S_use_curlocale_scratch(pTHX)
{
    /* This function is used to hide from the caller the case where the current
     * locale_t object in POSIX 2008 is the global one, which is illegal in
     * many of the P2008 API calls.  This checks for that and, if necessary
     * creates a proper P2008 object.  Any prior object is deleted, as is any
     * remaining object during global destruction. */

    locale_t cur = uselocale((locale_t) 0);

    if (cur != LC_GLOBAL_LOCALE) {
        return cur;
    }

    if (PL_scratch_locale_obj) {
        freelocale(PL_scratch_locale_obj);
    }

    PL_scratch_locale_obj = duplocale(LC_GLOBAL_LOCALE);
    return PL_scratch_locale_obj;
}

#endif

void
Perl_locale_panic(const char * msg,
                  const line_t immediate_caller_line,
                  const char * const higher_caller_file,
                  const line_t higher_caller_line)
{
    PERL_ARGS_ASSERT_LOCALE_PANIC;
    dTHX;
    dSAVE_ERRNO;

    force_locale_unlock();

#ifdef USE_C_BACKTRACE
    dump_c_backtrace(Perl_debug_log, 20, 1);
#endif

    const char * called_by = "";
    if (   strNE(__FILE__, higher_caller_file)
        || immediate_caller_line != higher_caller_line)
    {
        called_by = Perl_form(aTHX_ "\nCalled by %s: %" LINE_Tf "\n",
                                    higher_caller_file, higher_caller_line);
    }

    RESTORE_ERRNO;

    const char * errno_text;

#ifdef HAS_EXTENDED_OS_ERRNO

    const int extended_errnum = get_extended_os_errno();
    if (errno != extended_errnum) {
        errno_text = Perl_form(aTHX_ "; errno=%d, $^E=%d",
                                     errno, extended_errnum);
    }
    else

#endif

    {
        errno_text = Perl_form(aTHX_ "; errno=%d", errno);
    }

    /* diag_listed_as: panic: %s */
    Perl_croak(aTHX_ "%s: %" LINE_Tf ": panic: %s%s%s\n",
                     __FILE__, immediate_caller_line,
                     msg, errno_text, called_by);
}

/* Macros to report and croak on an unexpected failure to set the locale.  The
 * via version has more stack trace information */
#define setlocale_failure_panic_i(i, cur, fail, line, higher_line)          \
    setlocale_failure_panic_via_i(i, cur, fail, __LINE__, line,             \
                                  __FILE__, higher_line)

#define setlocale_failure_panic_c(cat, cur, fail, line, higher_line)        \
   setlocale_failure_panic_i(cat##_INDEX_, cur, fail, line, higher_line)

#if defined(USE_LOCALE)

/* Expands to the code to
 *      result = savepvn(s, len)
 * if the category whose internal index is 'i' doesn't need to be kept in the C
 * locale on this system, or if 'action is 'no_override'.  Otherwise it expands
 * to
 *      result = savepv("C")
 * unless 'action' isn't 'check_that_overridden', in which case if the string
 * 's' isn't already "C" it panics */
#  ifndef HAS_IGNORED_LOCALE_CATEGORIES_
#    define OVERRIDE_AND_SAVEPV(s, len, result, i, action)                  \
                                                  result = savepvn(s, len)
#  else
#    define OVERRIDE_AND_SAVEPV(s, len, result, i, action)                  \
        STMT_START {                                                        \
            if (LIKELY(   ! need_to_override_category(i)                    \
                       || action == no_override)) {                         \
                result = savepvn(s, len);                                   \
            }                                                               \
            else {                                                          \
                const char * temp = savepvn(s, len);                        \
                result = savepv(override_ignored_category(i, temp));        \
                if (action == check_that_overridden && strNE(result, temp)) { \
                    locale_panic_(Perl_form(aTHX_                           \
                                "%s expected to be '%s', instead is '%s'",  \
                                category_names[i], result, temp));          \
                }                                                           \
                Safefree(temp);                                             \
            }                                                               \
        } STMT_END
#  endif

STATIC parse_LC_ALL_string_return
S_parse_LC_ALL_string(pTHX_ const char * string,
                            const char ** output,
                            const parse_LC_ALL_STRING_action  override,
                            bool always_use_full_array,
                            const bool panic_on_error,
                            const line_t caller_line)
{
    /* This function parses the value of the input 'string' which is expected
     * to be the representation of an LC_ALL locale, and splits the result into
     * the values for the individual component categories, returning those in
     * the 'output' array.  Each array value will be a savepv() copy that is
     * the responsibility of the caller to make sure gets freed
     *
     * The locale for each category is independent of the other categories.
     * Often, they are all the same, but certainly not always.  Perl, in fact,
     * usually keeps LC_NUMERIC in the C locale, regardless of the underlying
     * locale.  LC_ALL has to be able to represent the case of when not all
     * categories have the same locale.  Platforms have differing ways of
     * representing this.  Internally, this file uses the 'name=value;'
     * representation found on some platforms, so this function always looks
     * for and parses that.  Other platforms use a positional notation.  On
     * those platforms, this function also parses that form.  It examines the
     * input to see which form is being parsed.
     *
     * Often, all categories will have the same locale.  This is special cased
     * if 'always_use_full_array' is false on input:
     *      1) If the input 'string' is a single value, this function doesn't
     *         store anything into 'output', and returns 'no_array'
     *      2) Some platforms will return multiple occurrences of the same
     *         value rather than coalescing them down to a single one.  HP-UX
     *         is such a one.  This function will do that collapsing for you,
     *         returning 'only_element_0' and saving the single value in
     *         output[0], which the caller will need to arrange to be freed.
     *         The rest of output[] is undefined, and does not need to be
     *         freed.
     *
     * Otherwise, the input 'string' may not be valid.  This function looks
     * mainly for syntactic errors, and if found, returns 'invalid'.  'output'
     * will not be filled in in that case, but the input state of it isn't
     * necessarily preserved.  Turning on -DL debugging will give details as to
     * the error.  If 'panic_on_error' is 'true', the function panics instead
     * of returning on error, with a message giving the details.
     *
     * Otherwise, output[] will be filled with the individual locale names for
     * all categories on the system, 'full_array' will be returned, and the
     * caller needs to arrange for each to be freed.  This means that either at
     * least one category differed from the others, or 'always_use_full_array' was
     * true on input.
     *
     * perl may be configured to ignore changes to a category's locale to
     * non-C.  The parameter 'override' tells this function what to do when
     * encountering such an illegal combination:
     *
     *      no_override             indicates to take no special action
     *      override_if_ignored,    indicates to return 'C' instead of what the
     *                              input string actually says.
     *      check_that_overridden   indicates to panic if the string says the
     *                              category is not 'C'.  This is used when
     *                              non-C is very unexpected behavior.
     * */

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "Entering parse_LC_ALL_string; called from %"    \
                           LINE_Tf "\nnew='%s'\n", caller_line, string));

#  ifdef PERL_LC_ALL_USES_NAME_VALUE_PAIRS

    const char separator[] = ";";
    const Size_t separator_len = 1;
    const bool single_component = (strchr(string, ';') == NULL);

#  else

    /* It's possible (but quite unlikely) that the separator string is an '='
     * or a ';'.  Requiring both to be present for using the 'name=value;' form
     * properly handles those possibilities */
    const bool name_value = strchr(string, '=') && strchr(string, ';');
    const char * separator;
    Size_t separator_len;
    bool single_component;
    if (name_value) {
        separator = ";";
        separator_len = 1;
        single_component = false;   /* Since has both [;=], must be multi */
    }
    else {
        separator = PERL_LC_ALL_SEPARATOR;
        separator_len = STRLENs(PERL_LC_ALL_SEPARATOR);
        single_component = instr(string, separator) == NULL;
    }

    Size_t component_number = 0;    /* Position in the parsing loop below */

#  endif
#  ifndef HAS_IGNORED_LOCALE_CATEGORIES_
    PERL_UNUSED_ARG(override);
#  else

    /* Any ignored categories are to be set to "C", so if this single-component
     * LC_ALL isn't to C, it has both "C" and non-C, so isn't really a single
     * component.  All the non-ignored categories are set to the input
     * component, but the ignored ones are overridden to be C.
     *
     * This incidentally handles the case where the string is "".  The return
     * will be C for each ignored category and "" for the others.  Then the
     * caller can individually set each category, and get the right answer. */
    if (single_component && ! isNAME_C_OR_POSIX(string)) {
        for_all_individual_category_indexes(i) {
           OVERRIDE_AND_SAVEPV(string, strlen(string), output[i], i, override);
        }

        return full_array;
    }

#  endif

    if (single_component) {
        if (! always_use_full_array) {
            return no_array;
        }

        for_all_individual_category_indexes(i) {
            output[i] = savepv(string);
        }

        return full_array;
    }

    /* Here the input is multiple components.  Parse through them.  (It is
     * possible that these components are all the same, so we check, and if so,
     * return just the 0th component (unless 'always_use_full_array' is true)
     *
     * This enum notes the possible errors findable in parsing */
    enum {
            incomplete,
            no_equals,
            unknown_category,
            contains_LC_ALL_element
    } error;

    /* Keep track of the categories we have encountered so far */
    bool seen[LC_ALL_INDEX_] = { false };

    Size_t index;           /* Our internal index for the current category */
    const char * s = string;
    const char * e = s + strlen(string);
    const char * category_end = NULL;
    const char * saved_first = NULL;

    /* Parse the input locale string */
    while (s < e) {

        /* 'separator' has been set up to delimit the components */
        const char * next_sep = instr(s, separator);
        if (! next_sep) {   /* At the end of the input */
            next_sep = e;
        }

#  ifndef PERL_LC_ALL_USES_NAME_VALUE_PAIRS

        if (! name_value) {
            /* Get the index of the category in this position */
            index = map_LC_ALL_position_to_index[component_number++];
        }
        else

#  endif

        {   /* Get the category part when each component is the
             * 'category=locale' form */

            category_end = strchr(s, '=');

            /* The '=' terminates the category name.  If no '=', is improper
             * form */
            if (! category_end) {
                error = no_equals;
                goto failure;
            }

            /* Find our internal index of the category name; uses a linear
             * search.  (XXX This could be avoided by various means, but the
             * maximum likely search is 6 items, and khw doesn't think the
             * added complexity would save very much at all.) */
            const unsigned int name_len = (unsigned int) (category_end - s);
            for (index = 0; index < C_ARRAY_LENGTH(category_names); index++) {
                if (   name_len == category_name_lengths[index]
                    && memEQ(s, category_names[index], name_len))
                {
                    goto found_category;
                }
            }

            /* Here, the category is not in our list. */
            error = unknown_category;
            goto failure;

          found_category:   /* The system knows about this category. */

            if (index == LC_ALL_INDEX_) {
                error = contains_LC_ALL_element;
                goto failure;
            }

            /* The locale name starts just beyond the '=' */
            s = category_end + 1;

            /* Linux (and maybe others) doesn't treat a duplicate category in
             * the string as an error.  Instead it uses the final occurrence as
             * the intended value.  So if this is a duplicate, free the former
             * value before setting the new one */
            if (seen[index]) {
                Safefree(output[index]);
            }
            else {
                seen[index] = true;
            }
        }

        /* Here, 'index' contains our internal index number for the current
         * category, and 's' points to the beginning of the locale name for
         * that category. */
        OVERRIDE_AND_SAVEPV(s, next_sep - s, output[index], index, override);

        if (! always_use_full_array) {
            if (! saved_first) {
                saved_first = output[index];
            }
            else {
                if (strNE(saved_first, output[index])) {
                    always_use_full_array = true;
                }
            }
        }

        /* Next time start from the new position */
        s = next_sep + separator_len;
    }

    /* Finished looping through all the categories
     *
     * Check if the input was incomplete. */

#  ifndef PERL_LC_ALL_USES_NAME_VALUE_PAIRS

    if (! name_value) {     /* Positional notation */
        if (component_number != LC_ALL_INDEX_) {
            error = incomplete;
            goto failure;
        }
    }
    else

#  endif

    {   /* Here is the name=value notation */
        for_all_individual_category_indexes(i) {
            if (! seen[i]) {
                error = incomplete;
                goto failure;
            }
        }
    }

    /* In the loop above, we changed 'always_use_full_array' to true iff not all
     * categories have the same locale.  Hence, if it is still 'false', all of
     * them are the same. */
    if (always_use_full_array) {
        return full_array;
    }

    /* Free the dangling ones */
    for_all_but_0th_individual_category_indexes(i) {
        Safefree(output[i]);
        output[i] = NULL;
    }

    return only_element_0;

  failure:

    /* Don't leave memory dangling that we allocated before the failure */
    for_all_individual_category_indexes(i) {
        if (seen[i]) {
            Safefree(output[i]);
            output[i] = NULL;
        }
    }

    const char * msg;
    const char * display_start = s;
    const char * display_end = e;

    switch (error) {
        case incomplete:
            msg = "doesn't list every locale category";
            display_start = string;
            break;
        case no_equals:
            msg = "needs an '=' to split name=value";
            break;
        case unknown_category:
            msg = "is an unknown category";
            display_end = (category_end && category_end > display_start)
                          ? category_end
                          : e;
            break;
        case contains_LC_ALL_element:
            msg = "has LC_ALL, which is illegal here";
            break;
    }

    msg = Perl_form(aTHX_ "'%.*s' %s\n",
                          (int) (display_end - display_start),
                          display_start, msg);

    DEBUG_L(PerlIO_printf(Perl_debug_log, "%s", msg));

    if (panic_on_error) {
        locale_panic_via_(msg, __FILE__, caller_line);
    }

    return invalid;
}

#  undef OVERRIDE_AND_SAVEPV
#endif

/*==========================================================================
 * Here starts the code that gives a uniform interface to its callers, hiding
 * the differences between platforms.
 *
 * base_posix_setlocale_() presents a consistent POSIX-compliant interface to
 * setlocale().   Windows requres a customized base-level setlocale().  This
 * layer should only be used by the next level up: the plain posix_setlocale
 * layer.  Any necessary mutex locking needs to be done at a higher level.  The
 * return may be overwritten by the next call to this function */
#ifdef WIN32
#  define base_posix_setlocale_(cat, locale) win32_setlocale(cat, locale)
#else
#  define base_posix_setlocale_(cat, locale)                                \
                                    ((const char *) setlocale(cat, locale))
#endif

/*==========================================================================
 * Here is the main posix layer.  It is the same as the base one unless the
 * system is lacking LC_ALL, or there are categories that we ignore, but that
 * the system libc knows about */

#if ! defined(USE_LOCALE)                                                   \
 ||  (defined(LC_ALL) && ! defined(HAS_IGNORED_LOCALE_CATEGORIES_))
#  define posix_setlocale(cat, locale) base_posix_setlocale_(cat, locale)
#else
#  define posix_setlocale(cat, locale)                                      \
        S_posix_setlocale_with_complications(aTHX_ cat, locale, __LINE__)

STATIC const char *
S_posix_setlocale_with_complications(pTHX_ const int cat,
                                           const char * new_locale,
                                           const line_t caller_line)
{
    /* This implements the posix layer above the base posix layer.
     * It is needed to reconcile our internal records that reflect only a
     * proper subset of the categories known by the system. */

    /* Querying the current locale returns the real value */
    if (new_locale == NULL) {
        new_locale = base_posix_setlocale_(cat, NULL);
        assert(new_locale);
        return new_locale;
    }

    const char * locale_on_entry = NULL;

    /* If setting from the environment, actually do the set to get the system's
     * idea of what that means; we may have to override later. */
    if (strEQ(new_locale, "")) {
        locale_on_entry = base_posix_setlocale_(cat, NULL);
        assert(locale_on_entry);
        new_locale = base_posix_setlocale_(cat, "");
        if (! new_locale) {
            SET_EINVAL;
            return NULL;
        }
    }

#  ifdef LC_ALL

    const char * new_locales[LC_ALL_INDEX_] = { NULL };

    if (cat == LC_ALL) {
        switch (parse_LC_ALL_string(new_locale,
                                    (const char **) &new_locales,
                                    override_if_ignored,   /* Override any
                                                              ignored
                                                              categories */
                                    false,    /* Return only [0] if suffices */
                                    false,    /* Don't panic on error */
                                    caller_line))
        {
          case invalid:
            SET_EINVAL;
            return NULL;

          case no_array:
            break;

          case only_element_0:
            new_locale = new_locales[0];
            SAVEFREEPV(new_locale);
            break;

          case full_array:

            /* Turn the array into a string that the libc setlocale() should
             * understand.   (Another option would be to loop, setting the
             * individual locales, and then return base(cat, NULL) */
            new_locale = calculate_LC_ALL_string(new_locales,
                                                 EXTERNAL_FORMAT_FOR_SET,
                                                 WANT_TEMP_PV,
                                                 caller_line);

            for_all_individual_category_indexes(i) {
                Safefree(new_locales[i]);
            }

            /* And call the libc setlocale.  We could avoid this call if
             * locale_on_entry is set and eq the new_locale.  But that would be
             * only for the relatively rare case of the desired locale being
             * "", and the time spent in doing the string compare might be more
             * than that of just setting it unconditionally */
            new_locale = base_posix_setlocale_(cat, new_locale);
            if (! new_locale) {
                 goto failure;
            }

            return new_locale;
        }
    }

#  endif

    /* Here, 'new_locale' is a single value, not an aggregation.  Just set it.
     * */
    new_locale =
        base_posix_setlocale_(cat,
                              override_ignored_category(
                                          get_category_index(cat), new_locale));
    if (! new_locale) {
        goto failure;
    }

    return new_locale;

 failure:

    /* 'locale_on_entry' being set indicates there has likely been a change in
     * locale which needs to be restored */
    if (locale_on_entry) {
        if (! base_posix_setlocale_(cat, locale_on_entry)) {
            setlocale_failure_panic_i(get_category_index(cat),
                                      NULL, locale_on_entry,
                                      __LINE__, caller_line);
        }
    }

    SET_EINVAL;
    return NULL;
}

#endif

/* End of posix layer
 *==========================================================================
 *
 * The next layer up is to catch vagaries and bugs in the libc setlocale return
 * value.  The return is not guaranteed to be stable.
 *
 * Any necessary mutex locking needs to be done at a higher level.
 *
 * On most platforms this layer is empty, expanding to just the layer
 * below.   To enable it, call Configure with either or both:
 * -Accflags=-DHAS_LF_IN_SETLOCALE_RETURN
 *                  to indicate that extraneous \n characters can be returned
 *                  by setlocale()
 * -Accflags=-DHAS_BROKEN_SETLOCALE_QUERY_LC_ALL
 *                  to indicate that setlocale(LC_ALL, NULL) cannot be relied
 *                  on
 */

#define STDIZED_SETLOCALE_LOCK    POSIX_SETLOCALE_LOCK
#define STDIZED_SETLOCALE_UNLOCK  POSIX_SETLOCALE_UNLOCK
#if ! defined(USE_LOCALE)                                                   \
 || ! (   defined(HAS_LF_IN_SETLOCALE_RETURN)                               \
       || defined(HAS_BROKEN_SETLOCALE_QUERY_LC_ALL))
#  define stdized_setlocale(cat, locale)  posix_setlocale(cat, locale)
#  define stdize_locale(cat, locale)  (locale)
#else
#  define stdized_setlocale(cat, locale)                                    \
        S_stdize_locale(aTHX_ cat, posix_setlocale(cat, locale), __LINE__)

STATIC const char *
S_stdize_locale(pTHX_ const int category,
                      const char *input_locale,
                      const line_t caller_line)
{
    /* The return value of setlocale() is opaque, but is required to be usable
     * as input to a future setlocale() to create the same state.
     * Unfortunately not all systems are compliant.  This function brings those
     * outliers into conformance.  It is based on what problems have arisen in
     * the field.
     *
     * This has similar constraints as the posix layer.  You need to lock
     * around it until its return is safely copied or no longer needed. (The
     * return may point to a global static buffer or may be mortalized.)
     *
     * The current things this corrects are:
     * 1) A new-line.  This function chops any \n characters
     * 2) A broken 'setlocale(LC_ALL, foo)'  This constructs a proper returned
     *                 string from the constituent categories
     *
     * If no changes were made, the input is returned as-is */

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                          "Entering stdize_locale(%d, '%s');"
                          " called from %" LINE_Tf "\n",
                          category, input_locale, caller_line));

    if (input_locale == NULL) {
        SET_EINVAL;
        return NULL;
    }

    char * retval = (char *) input_locale;

#  if defined(LC_ALL) && defined(HAS_BROKEN_SETLOCALE_QUERY_LC_ALL)

        /* If setlocale(LC_ALL, NULL) is broken, compute what the system
         * actually thinks it should be from its individual components */
    if (category == LC_ALL) {
        retval = (char *) calculate_LC_ALL_string(
                                          NULL,  /* query each individ locale */
                                          EXTERNAL_FORMAT_FOR_SET,
                                          WANT_TEMP_PV,
                                          caller_line);
    }

#  endif
#  ifdef HAS_NL_IN_SETLOCALE_RETURN

    char * first_bad = NULL;

#    ifndef LC_ALL

    PERL_UNUSED_ARG(category);
    PERL_UNUSED_ARG(caller_line);

#      define INPUT_LOCALE  retval
#      define MARK_CHANGED
#    else

    char * individ_locales[LC_ALL_INDEX_] = { NULL };
    bool made_changes = false;
    Size_t upper;
    if (category != LC_ALL) {
        individ_locales[0] = retval;
        upper = 0;
    }
    else {

        /* And parse the locale string, splitting into its individual
         * components. */
        switch (parse_LC_ALL_string(retval,
                                    (const char **) &individ_locales,
                                    check_that_overridden, /* ignored
                                                              categories should
                                                              already have been
                                                              overridden */
                                    false,    /* Return only [0] if suffices */
                                    false,    /* Don't panic on error */
                                    caller_line))
        {
          case invalid:
            SET_EINVAL;
            return NULL;

          case full_array: /* Loop below through all the component categories.
                            */
            upper = LC_ALL_INDEX_ - 1;
            break;

          case no_array:
            /* All categories here are set to the same locale, and the parse
             * didn't fill in any of 'individ_locales'.  Set the 0th element to
             * that locale. */
            individ_locales[0] = retval;
            /* FALLTHROUGH */

          case only_element_0: /* Element 0 is the only element we need to look
                                  at */
            upper = 0;
            break;
        }
    }

    for (unsigned int i = 0; i <= upper; i++)

#      define INPUT_LOCALE  individ_locales[i]
#      define MARK_CHANGED  made_changes = true;
#    endif    /* Has LC_ALL */

    {
        first_bad = (char *) strchr(INPUT_LOCALE, '\n');

        /* Most likely, there isn't a problem with the input */
        if (UNLIKELY(first_bad)) {

            /* This element will need to be adjusted.  Create a modifiable
             * copy. */
            MARK_CHANGED
            retval = savepv(INPUT_LOCALE);
            SAVEFREEPV(retval);

            /* Translate the found position into terms of the copy */
            first_bad = retval + (first_bad - INPUT_LOCALE);

            /* Get rid of the \n and what follows.  (Originally, only a
             * trailing \n was stripped.  Unsure what to do if not trailing) */
            *((char *) first_bad) = '\0';
        }   /* End of needs adjusting */
    }   /* End of looking for problems */

#    ifdef LC_ALL

    /* If we had multiple elements, extra work is required */
    if (upper != 0) {

        /* If no changes were made to the input, 'retval' already contains it
         * */
        if (made_changes) {

            /* But if did make changes, need to calculate the new value */
            retval = (char *) calculate_LC_ALL_string(
                                            (const char **) &individ_locales,
                                            EXTERNAL_FORMAT_FOR_SET,
                                            WANT_TEMP_PV,
                                            caller_line);
        }

        /* And free the no-longer needed memory */
        for (unsigned int i = 0; i <= upper; i++) {
            Safefree(individ_locales[i]);
        }
    }

#    endif
#    undef INPUT_LOCALE
#    undef MARK_CHANGED
#  endif    /* HAS_NL_IN_SETLOCALE_RETURN */

    return (const char *) retval;
}

#endif  /* USE_LOCALE */

/* End of stdize_locale layer
 *
 * ==========================================================================
 *
 * The next many lines form several implementations of a layer above the
 * close-to-the-metal 'posix' and 'stdized' macros.  They are used to present a
 * uniform API to the rest of the code in this file in spite of the disparate
 * underlying implementations.  Which implementation gets compiled depends on
 * the platform capabilities (and some user choice) as determined by Configure.
 *
 * As more fully described in the introductory comments in this file, the
 * API of each implementation consists of three sets of macros.  Each set has
 * three variants with suffixes '_c', '_r', and '_i'.  In the list below '_X'
 * is to be replaced by any of these suffixes.
 *
 * 1) bool_setlocale_X  attempts to set the given category's locale to the
 *                      given value, returning if it worked or not.
 * 2) void_setlocale_X  is like the corresponding bool_setlocale, but used when
 *                      success is the only sane outcome, so failure causes it
 *                      to panic.
 * 3) querylocale_X     to see what the given category's locale is
 *
 * 4) setlocale_i()     is defined only in those implementations where the bool
 *                      and query forms are essentially the same, and can be
 *                      combined to save CPU time.
 *
 * Each implementation is fundamentally defined by just two macros: a
 * bool_setlocale_X() and a querylocale_X().  The other macros are all
 * derivable from these.  Each fundamental macro is either a '_i' suffix one or
 * an '_r' suffix one, depending on what is the most efficient in getting to an
 * input form that the underlying libc functions want.  The derived macro
 * definitions are deferred in this file to after the code for all the
 * implementations.  This makes each implementation shorter and clearer, and
 * removes duplication.
 *
 * Each implementation below is separated by ==== lines, and includes bool,
 * void, and query macros.  The query macros are first, followed by any
 * functions needed to implement them.  Then come the bool, again followed by
 * any implementing functions  Then are the void macros; next is setlocale_i if
 * present on this implementation.  Finally are any helper functions.  The sets
 * in each implementation are separated by ---- lines.
 *
 * The returned strings from all the querylocale...() forms in all
 * implementations are thread-safe, and the caller should not free them,
 * but each may be a mortalized copy.  If you need something stable across
 * calls, you need to savepv() the result yourself.
 *
 *===========================================================================*/

#if    (! defined(USE_LOCALE_THREADS) && ! defined(USE_POSIX_2008_LOCALE))    \
    || (  defined(WIN32) && defined(USE_THREAD_SAFE_LOCALE))

/* For non-threaded perls, the implementation just expands to the base-level
 * functions (except if we are Configured to nonetheless use the POSIX 2008
 * interface) This implementation is also used on threaded perls where
 * threading is invisible to us.  Currently this is only on later Windows
 * versions. */

#  define querylocale_r(cat)  mortalized_pv_copy(stdized_setlocale(cat, NULL))
#  define bool_setlocale_r(cat, locale) cBOOL(posix_setlocale(cat, locale))

/*---------------------------------------------------------------------------*/

/* setlocale_i is only defined for Configurations where the libc setlocale()
 * doesn't need any tweaking.  It allows for some shortcuts */
#  ifndef USE_LOCALE_THREADS
#    define setlocale_i(i, locale)   stdized_setlocale(categories[i], locale)

#  elif defined(WIN32) && defined(USE_THREAD_SAFE_LOCALE)

/* On Windows, we don't know at compile time if we are in thread-safe mode or
 * not.  If we are, we can just return the result of the layer below us.  If we
 * are in unsafe mode, we need to first copy that result to a safe place while
 * in a critical section */

#    define setlocale_i(i, locale)   S_setlocale_i(aTHX_ categories[i], locale)

STATIC const char *
S_setlocale_i(pTHX_ const int category, const char * locale)
{
    if (LIKELY(_configthreadlocale(0) == _ENABLE_PER_THREAD_LOCALE)) {
        return stdized_setlocale(category, locale);
    }

    gwLOCALE_LOCK;
    const char * retval = save_to_buffer(stdized_setlocale(category, locale),
                                         &PL_setlocale_buf,
                                         &PL_setlocale_bufsize);
    gwLOCALE_UNLOCK;

    return retval;
}

#  endif

/*===========================================================================*/
#elif   defined(USE_LOCALE_THREADS)                 \
   && ! defined(USE_THREAD_SAFE_LOCALE)

   /* Here, there are threads, and there is no support for thread-safe
    * operation.  This is a dangerous situation, which perl is documented as
    * not supporting, but it arises in practice.  We can do a modicum of
    * automatic mitigation by making sure there is a per-thread return from
    * setlocale(), and that a mutex protects it from races */

#  define querylocale_r(cat)                                                \
                      mortalized_pv_copy(less_dicey_setlocale_r(cat, NULL))

STATIC const char *
S_less_dicey_setlocale_r(pTHX_ const int category, const char * locale)
{
    const char * retval;

    PERL_ARGS_ASSERT_LESS_DICEY_SETLOCALE_R;

    STDIZED_SETLOCALE_LOCK;

    retval = save_to_buffer(stdized_setlocale(category, locale),
                            &PL_less_dicey_locale_buf,
                            &PL_less_dicey_locale_bufsize);

    STDIZED_SETLOCALE_UNLOCK;

    return retval;
}

/*---------------------------------------------------------------------------*/

#  define bool_setlocale_r(cat, locale)                                     \
                               less_dicey_bool_setlocale_r(cat, locale)

STATIC bool
S_less_dicey_bool_setlocale_r(pTHX_ const int cat, const char * locale)
{
    bool retval;

    PERL_ARGS_ASSERT_LESS_DICEY_BOOL_SETLOCALE_R;

    /* Unlikely, but potentially possible that another thread could zap the
     * buffer from true to false or vice-versa, so need to lock here */
    POSIX_SETLOCALE_LOCK;
    retval = cBOOL(posix_setlocale(cat, locale));
    POSIX_SETLOCALE_UNLOCK;

    return retval;
}

/*---------------------------------------------------------------------------*/

/* setlocale_i is only defined for Configurations where the libc setlocale()
 * suffices for both querying and setting the locale.  It allows for some
 * shortcuts */
#  define setlocale_i(i, locale)  less_dicey_setlocale_r(categories[i], locale)

/* The code in this file may change the locale briefly during certain
 * operations.  This should be a critical section when that could interfere
 * with other instances executing at the same time. */
#  define TOGGLE_LOCK(i)    POSIX_SETLOCALE_LOCK
#  define TOGGLE_UNLOCK(i)  POSIX_SETLOCALE_UNLOCK

/*===========================================================================*/

#elif defined(USE_POSIX_2008_LOCALE)
#  ifndef LC_ALL
#    error This code assumes that LC_ALL is available on a system modern enough to have POSIX 2008
#  endif

/* Here, there is a completely different API to get thread-safe locales.  We
 * emulate the setlocale() API with our own function(s).  setlocale categories,
 * like LC_NUMERIC, are not valid here for the POSIX 2008 API.  Instead, there
 * are equivalents, like LC_NUMERIC_MASK, which we use instead, which we find
 * by table lookup. */

#  if defined(__GLIBC__) && defined(USE_LOCALE_MESSAGES)
            /* https://sourceware.org/bugzilla/show_bug.cgi?id=24936 */
#    define HAS_GLIBC_LC_MESSAGES_BUG
#    include <libintl.h>
#  endif

#  define querylocale_i(i)    querylocale_2008_i(i, __LINE__)

    /* We need to define this derivative macro here, as it is needed in
     * the implementing function (for recursive calls).  It also gets defined
     * where all the other derivative macros are defined, and the compiler
     * will complain if the definition gets out of sync */
#  define querylocale_c(cat)      querylocale_i(cat##_INDEX_)

STATIC const char *
S_querylocale_2008_i(pTHX_ const locale_category_index index,
                           const line_t caller_line)
{
    PERL_ARGS_ASSERT_QUERYLOCALE_2008_I;

    /* This function returns the name of the locale category given by the input
     * 'index' into our parallel tables of them.
     *
     * POSIX 2008, for some sick reason, chose not to provide a method to find
     * the category name of a locale, disregarding a basic linguistic tenet
     * that for any object, people will create a name for it.  (The next
     * version of the POSIX standard is proposed to fix this.)  Some vendors
     * have created a querylocale() function to do this in the meantime.  On
     * systems without querylocale(), we have to keep track of what the locale
     * has been set to, so that we can return its name so as to emulate
     * setlocale().  There are potential problems with this:
     *
     *  1)  We don't know what calling newlocale() with the locale argument ""
     *      actually does.  It gets its values from the program's environment.
     *      find_locale_from_environment() is used to work around this.  But it
     *      isn't fool-proof.  See the comments for that function for details.
     *  2)  It's possible for C code in some library to change the locale
     *      without us knowing it, and thus our records become wrong;
     *      querylocale() would catch this.  But as of September 2017, there
     *      are no occurrences in CPAN of uselocale().  Some libraries do use
     *      setlocale(), but that changes the global locale, and threads using
     *      per-thread locales will just ignore those changes.
     *  3)  Many systems have multiple names for the same locale.  Generally,
     *      there is an underlying base name, with aliases that evaluate to it.
     *      On some systems, if you set the locale to an alias, and then
     *      retrieve the name, you get the alias as expected; but on others you
     *      get the base name, not the alias you used.  And sometimes the
     *      charade is incomplete.  See
     *      https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=269375.
     *
     *      The code is structured so that the returned locale name when the
     *      locale is changed is whatever the result of querylocale() on the
     *      new locale is.  This effectively gives the result the system
     *      expects.  Without querylocale, the name returned is always the
     *      input name.  Theoretically this could cause problems, but khw knows
     *      of none so far, but mentions it here in case you are trying to
     *      debug something.  (This could be worked around by messing with the
     *      global locale temporarily, using setlocale() to get the base name;
     *      but that could cause a race.  The comments for
     *      find_locale_from_environment() give details on the potential race.)
     */

    const locale_t cur_obj = uselocale((locale_t) 0);
    const char * retval;

    DEBUG_Lv(PerlIO_printf(Perl_debug_log, "querylocale_2008_i(%s) on %p;"
                                           " called from %" LINE_Tf "\n",
                                           category_names[index], cur_obj,
                                           caller_line));

    if (UNLIKELY(cur_obj == LC_GLOBAL_LOCALE)) {

        /* Even on platforms that have querylocale(), it is unclear if they
         * work in the global locale, and we have the means to get the correct
         * answer anyway.  khw is unsure this situation even comes up these
         * days, hence the branch prediction */
        POSIX_SETLOCALE_LOCK;
        retval = mortalized_pv_copy(posix_setlocale(categories[index], NULL));
        POSIX_SETLOCALE_UNLOCK;
    }

    /* Here we have handled the case of the current locale being the global
     * one.  Below is the 'else' case of that.  There are two different
     * implementations, depending on USE_PL_CURLOCALES */

#  ifdef USE_PL_CURLOCALES

    else {

        /* PL_curlocales[] is kept up-to-date for all categories except LC_ALL,
         * which may have been invalidated by setting it to NULL, and if so,
         * should now be calculated.  (The called function updates that
         * element.) */
        if (index == LC_ALL_INDEX_ && PL_curlocales[LC_ALL_INDEX_] == NULL) {
            calculate_LC_ALL_string((const char **) &PL_curlocales,
                                    INTERNAL_FORMAT,
                                    WANT_VOID,
                                    caller_line);
        }

        if (cur_obj == PL_C_locale_obj) {

            /* If the current locale object is the C object, then the answer is
             * "C" or POSIX, regardless of the category.  Handling this
             * reasonably likely case specially shortcuts extra effort, and
             * hides some bugs from us in OS's that alias other locales to C,
             * but do so incompletely.  If our records say it is POSIX, use
             * that; otherwise use C.  See
             * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=269375 */
            retval = (strEQ(PL_curlocales[index], "POSIX"))
                     ? "POSIX"
                     : "C";
        }
        else {
            retval = mortalized_pv_copy(PL_curlocales[index]);
        }
    }

#  else

    /* Below is the implementation of the 'else' clause which handles the case
     * of the current locale not being the global one on platforms where
     * USE_PL_CURLOCALES is NOT in effect.  That means the system must have
     * some form of querylocale.  But these have varying characteristics, so
     * first create some #defines to make the actual 'else' clause uniform.
     *
     * First, glibc has a function that implements querylocale(), but is called
     * something else, and takes the category number; the others take the mask.
     * */
#    if defined(USE_QUERYLOCALE) && (   defined(_NL_LOCALE_NAME)            \
                                     && defined(HAS_NL_LANGINFO_L))
#      define my_querylocale(index, cur_obj)                                \
                nl_langinfo_l(_NL_LOCALE_NAME(categories[index]), cur_obj)

       /* Experience so far shows it is thread-safe, as well as glibc's
        * nl_langinfo_l(), so unless overridden, mark it so */
#      ifdef NO_THREAD_SAFE_QUERYLOCALE
#        undef HAS_THREAD_SAFE_QUERYLOCALE
#      else
#        define HAS_THREAD_SAFE_QUERYLOCALE
#      endif
#    else   /* below, ! glibc */

       /* Otherwise, use the system's querylocale(). */
#      define my_querylocale(index, cur_obj)                                \
                               querylocale(category_masks[index], cur_obj)

       /* There is no standard for this function, and khw has never seen
        * anything beyond minimal vendor documentation, lacking important
        * details.  Experience has shown that some implementations have race
        * condiions, and their returns may not be thread safe.  It would be
        * unreliable to test for complete thread safety in Configure.  What we
        * do instead is to assume that it is thread-safe, unless overriden by,
        * say, a hints file specifying
        * -Accflags='-DNO_THREAD_SAFE_QUERYLOCALE */
#      ifdef NO_THREAD_SAFE_QUERYLOCALE
#        undef HAS_THREAD_SAFE_QUERYLOCALE
#      else
#        define HAS_THREAD_SAFE_QUERYLOCALE
#      endif
#    endif

     /* Here, we have set up enough information to know if this querylocale()
      * is thread-safe, or needs to use a mutex */
#    ifdef HAS_THREAD_SAFE_QUERYLOCALE
#      define QUERYLOCALE_LOCK
#      define QUERYLOCALE_UNLOCK
#    else
#      define QUERYLOCALE_LOCK    gwLOCALE_LOCK
#      define QUERYLOCALE_UNLOCK  gwLOCALE_UNLOCK
#    endif

    /* Finally, everything is ready, so here is the 'else' clause to implement
     * the case of the current locale not being the global one on systems that
     * have some form of querylocale().  (POSIX will presumably eventually
     * publish their next version in their pipeline, which will define a
     * precisely specified querylocale equivalent, and there can be a new
     * #ifdef to use it without having to guess at its characteristics) */

    else {
        /* We don't keep records when there is querylocale(), so as to avoid the
         * pitfalls mentioned at the beginning of this function.
         *
         * That means LC_ALL has to be calculated from all its constituent
         * categories each time, since the querylocale() forms on many (if not
         * all) platforms only work on individual categories */
        if (index == LC_ALL_INDEX_) {
            retval = calculate_LC_ALL_string(NULL, INTERNAL_FORMAT,
                                             WANT_TEMP_PV,
                                             caller_line);
        }
        else {

            QUERYLOCALE_LOCK;
            retval = my_querylocale(index, cur_obj);

            /* querylocale() may conflate the C locale with something that
             * isn't exactly the same.  See for example
             * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=269375
             * We know that if the locale object is the C one, we
             * are in the C locale, which may go by the name POSIX, as both, by
             * definition, are equivalent.  But we consider any other name
             * spurious, so override with "C".  As in the PL_CURLOCALES case
             * above, this hides those glitches, for the most part, from the
             * rest of our code.  (The code is ordered this way so that if the
             * system distinugishes "C" from "POSIX", we do too.) */
            if (cur_obj == PL_C_locale_obj && ! isNAME_C_OR_POSIX(retval)) {
                QUERYLOCALE_UNLOCK;
                retval = "C";
            }
            else {
                retval = savepv(retval);
                QUERYLOCALE_UNLOCK;
                SAVEFREEPV(retval);
            }
        }
    }

#    undef QUERYLOCALE_LOCK
#    undef QUERYLOCALE_UNLOCK
#  endif

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "querylocale_2008_i(%s) returning '%s'\n",
                           category_names[index], retval));
    assert(strNE(retval, ""));
    return retval;
}

/*---------------------------------------------------------------------------*/

#  define bool_setlocale_i(i, locale)                                       \
                              bool_setlocale_2008_i(i, locale, __LINE__)

/* If this doesn't exist on this platform, make it a no-op (to save #ifdefs) */
#  ifndef update_PL_curlocales_i
#    define update_PL_curlocales_i(index, new_locale, caller_line)
#  endif

STATIC bool
S_bool_setlocale_2008_i(pTHX_

        /* Our internal index of the 'category' setlocale is called with */
        const locale_category_index  index,
        const char * new_locale,    /* The locale to set the category to */
        const line_t caller_line    /* Called from this line number */
       )
{
    PERL_ARGS_ASSERT_BOOL_SETLOCALE_2008_I;

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
     */

    int mask = category_masks[index];
    const locale_t entry_obj = uselocale((locale_t) 0);
    const char * locale_on_entry = querylocale_i(index);

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "bool_setlocale_2008_i: input=%d (%s), mask=0x%x,"
                           " new locale=\"%s\", current locale=\"%s\","
                           " index=%d, entry object=%p;"
                           " called from %" LINE_Tf "\n",
                           categories[index], category_names[index], mask,
                           ((new_locale == NULL) ? "(nil)" : new_locale),
                           locale_on_entry, index, entry_obj, caller_line));

    /* Here, trying to change the locale, but it is a no-op if the new boss is
     * the same as the old boss.  Except this routine is called when converting
     * from the global locale, so in that case we will create a per-thread
     * locale below (with the current values).  It also seemed that newlocale()
     * could free up the basis locale memory if we called it with the new and
     * old being the same, but khw now thinks that this was due to some other
     * bug, since fixed, as there are other places where newlocale() gets
     * similarly called without problems. */
    if (   entry_obj != LC_GLOBAL_LOCALE
        && locale_on_entry
        && strEQ(new_locale, locale_on_entry))
    {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                               "bool_setlocale_2008_i: no-op to change to"
                               " what it already was\n"));
        return true;
    }

#  ifndef USE_QUERYLOCALE

    /* Without a querylocale() mechanism, we have to figure out ourselves what
     * happens with setting a locale to "" */

    if (strEQ(new_locale, "")) {
        new_locale = find_locale_from_environment(index);
        if (! new_locale) {
            SET_EINVAL;
            return false;
        }
    }

#  endif
#  ifdef NEWLOCALE_HANDLES_DISPARATE_LC_ALL

    const bool need_loop = false;

#  else

    bool need_loop = false;
    const char * new_locales[LC_ALL_INDEX_] = { NULL };

    /* If we're going to have to parse the LC_ALL string, might as well do it
     * now before we have made changes that we would have to back out of if the
     * parse fails */
    if (index == LC_ALL_INDEX_) {
        switch (parse_LC_ALL_string(new_locale,
                                    (const char **) &new_locales,
                                    override_if_ignored,
                                    false,    /* Return only [0] if suffices */
                                    false,    /* Don't panic on error */
                                    caller_line))
        {
          case invalid:
            SET_EINVAL;
            return false;

          case no_array:
            need_loop = false;
            break;

          case only_element_0:
            SAVEFREEPV(new_locales[0]);
            new_locale = new_locales[0];
            need_loop = false;
            break;

          case full_array:
            need_loop = true;
            break;
        }
    }

#  endif
#  ifdef HAS_GLIBC_LC_MESSAGES_BUG

    /* For this bug, if the LC_MESSAGES locale changes, we have to do an
     * expensive workaround.  Save the current value so we can later determine
     * if it changed. */
    const char * old_messages_locale = NULL;
    if (   (index == LC_MESSAGES_INDEX_ || index == LC_ALL_INDEX_)
        &&  LIKELY(PL_phase != PERL_PHASE_CONSTRUCT))
    {
        old_messages_locale = querylocale_c(LC_MESSAGES);
    }

#  endif

    assert(PL_C_locale_obj);

    /* Now ready to switch to the input 'new_locale' */

    /* Switching locales generally entails freeing the current one's space (at
     * the C library's discretion), hence we can't be using that locale at the
     * time of the switch (this wasn't obvious to khw from the man pages).  So
     * switch to a known locale object that we don't otherwise mess with. */
    if (! uselocale(PL_C_locale_obj)) {

        /* Not being able to change to the C locale is severe; don't keep
         * going.  */
        setlocale_failure_panic_i(index, locale_on_entry, "C",
                                  __LINE__, caller_line);
        NOT_REACHED; /* NOTREACHED */
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "bool_setlocale_2008_i: now using C"
                           " object=%p\n", PL_C_locale_obj));

    /* These two objects are special:
     *  LC_GLOBAL_LOCALE    because it is undefined behavior to call
     *                      newlocale() with it as a parameter.
     *  PL_C_locale_obj     because newlocale() generally destroys its locale
     *                      object parameter when it succeeds; and we don't
     *                      want that happening to this immutable object.
     * Copies will be made for them to use instead if we get so far as to call
     * newlocale(). */
    bool entry_obj_is_special = (   entry_obj == LC_GLOBAL_LOCALE
                                 || entry_obj == PL_C_locale_obj);
    locale_t new_obj;

    /* PL_C_locale_obj is LC_ALL set to the C locale.  If this call is to
     * switch to LC_ALL => C, simply use that object.  But in fact, we already
     * have switched to it just above, in preparation for the general case.
     * Since we're already there, no need to do further switching. */
    if (mask == LC_ALL_MASK && isNAME_C_OR_POSIX(new_locale)) {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                               "bool_setlocale_2008_i: will stay in C"
                               " object\n"));
        new_obj = PL_C_locale_obj;

        /* 'entry_obj' is now dangling, of no further use to anyone (unless it
         * is one of the special ones).  Free it to avoid a leak */
        if (! entry_obj_is_special) {
            freelocale(entry_obj);
        }

        update_PL_curlocales_i(index, new_locale, caller_line);
    }
    else {  /* Here is the general case, not to LC_ALL => C */

        /* The newlocale() call(s) below take a basis object to build upon to
         * create the changed locale, trashing it iff successful.
         *
         * For the objects that are not to be modified by this function, we
         * create a duplicate that gets trashed instead.
         *
         * Also if we will have to loop doing multiple newlocale()s, there is a
         * chance we will succeed for the first few, and then fail, having to
         * back out.  We need to duplicate 'entry_obj' in this case as well, so
         * it remains valid as something to back out to. */
        locale_t basis_obj = entry_obj;

        if (entry_obj_is_special || need_loop) {
            basis_obj = duplocale(basis_obj);
            if (! basis_obj) {
                locale_panic_via_("duplocale failed", __FILE__, caller_line);
                NOT_REACHED; /* NOTREACHED */
            }

            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                                   "bool_setlocale_2008_i created %p by"
                                   " duping the input\n", basis_obj));
        }

#  define DEBUG_NEW_OBJECT_CREATED(category, locale, new, old, caller_line) \
      DEBUG_Lv(PerlIO_printf(Perl_debug_log,                                \
                             "bool_setlocale_2008_i(%s, %s): created %p"    \
                             " while freeing %p; called from %" LINE_Tf     \
                             " via %" LINE_Tf "\n",                         \
                             category, locale, new, old,                    \
                             caller_line, (line_t)__LINE__))
#  define DEBUG_NEW_OBJECT_FAILED(category, locale, basis_obj)              \
      DEBUG_L(PerlIO_printf(Perl_debug_log,                                 \
                            "bool_setlocale_2008_i: creating new object"    \
                            " for (%s '%s') from %p failed; called from %"  \
                            LINE_Tf " via %" LINE_Tf "\n",                  \
                            category, locale, basis_obj,                    \
                            caller_line, (line_t)__LINE__));

        /* Ready to create a new locale by modification of the existing one.
         *
         * NOTE: This code may incorrectly show up as a leak under the address
         * sanitizer. We do not free this object under normal teardown, however
         * you can set PERL_DESTRUCT_LEVEL=2 to cause it to be freed.
         */

#  ifdef NEWLOCALE_HANDLES_DISPARATE_LC_ALL

        /* Some platforms have a newlocale() that can handle disparate LC_ALL
         * input, so on these a single call to newlocale() always works */
#  else

        /* If a single call to newlocale() will do */
        if (! need_loop)

#  endif

        {
            new_obj = newlocale(mask,
                                override_ignored_category(index, new_locale),
                                basis_obj);
            if (! new_obj) {
                DEBUG_NEW_OBJECT_FAILED(category_names[index], new_locale,
                                        basis_obj);

                /* Since the call failed, it didn't trash 'basis_obj', which is
                 * a dup for these objects, and hence would leak if we don't
                 * free it.  XXX However, something is seriously wrong if we
                 * can't switch to C or the global locale, so maybe should
                 * panic instead */
                if (entry_obj_is_special) {
                    freelocale(basis_obj);
                }

                goto must_restore_state;
            }

            DEBUG_NEW_OBJECT_CREATED(category_names[index], new_locale,
                                     new_obj, basis_obj, caller_line);

            update_PL_curlocales_i(index, new_locale, caller_line);
        }

#  ifndef NEWLOCALE_HANDLES_DISPARATE_LC_ALL

        else {  /* Need multiple newlocale() calls */

            /* Loop through the individual categories, setting the locale of
             * each to the corresponding name previously populated into
             * newlocales[].  Each iteration builds on the previous one, adding
             * its category to what's already been calculated, and taking as a
             * basis for what's been calculated 'basis_obj', which is updated
             * each iteration to be the result of the previous one.  Upon
             * success, newlocale() trashes the 'basis_obj' parameter to it.
             * If any iteration fails, we immediately give up, restore the
             * locale to what it was at the time this function was called
             * (saved in 'entry_obj'), and return failure. */

            /* Loop, using the previous iteration's result as the basis for the
             * next one.  (The first time we effectively use the locale in
             * force upon entry to this function.) */
            for_all_individual_category_indexes(i) {
                new_obj = newlocale(category_masks[i],
                                    new_locales[i],
                                    basis_obj);
                if (new_obj) {
                    DEBUG_NEW_OBJECT_CREATED(category_names[i],
                                             new_locales[i],
                                             new_obj, basis_obj,
                                             caller_line);
                    basis_obj = new_obj;
                    continue;
                }

                /* Failed.  Likely this is because the proposed new locale
                 * isn't valid on this system. */

                DEBUG_NEW_OBJECT_FAILED(category_names[i],
                                        new_locales[i],
                                        basis_obj);

                /* newlocale() didn't trash this, since the function call
                 * failed */
                freelocale(basis_obj);

                for_all_individual_category_indexes(j) {
                    Safefree(new_locales[j]);
                }

                goto must_restore_state;
            }

            /* Success for all categories. */
            for_all_individual_category_indexes(i) {
                update_PL_curlocales_i(i, new_locales[i], caller_line);
                Safefree(new_locales[i]);
            }

            /* We dup'd entry_obj in case we had to fall back to it.  The
             * newlocale() above destroyed the dup when it first succeeded, but
             * entry_obj itself is left dangling, so free it */
            if (! entry_obj_is_special) {
                freelocale(entry_obj);
            }
        }

#  endif    /* End of newlocale can't handle disparate LC_ALL input */

    }

#  undef DEBUG_NEW_OBJECT_CREATED
#  undef DEBUG_NEW_OBJECT_FAILED

    /* Here, successfully created an object representing the desired locale;
     * now switch into it */
    if (! uselocale(new_obj)) {
        freelocale(new_obj);
        locale_panic_(Perl_form(aTHX_ "(called from %" LINE_Tf "):"
                                      " bool_setlocale_2008_i: switching"
                                      " into new locale failed",
                                      caller_line));
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "bool_setlocale_2008_i: now using %p\n", new_obj));

#  ifdef MULTIPLICITY   /* Unlikely, but POSIX 2008 functions could be
                           Configured to be used on unthreaded perls, in which
                           case this object doesn't exist */

    if (DEBUG_Lv_TEST) {
        if (PL_cur_locale_obj != new_obj) {
            PerlIO_printf(Perl_debug_log,
                          "bool_setlocale_2008_i: PL_cur_locale_obj"
                          " was %p, now is %p\n",
                          PL_cur_locale_obj, new_obj);
        }
    }

    /* Update the current object */
    PL_cur_locale_obj = new_obj;

#  endif
#  ifdef HAS_GLIBC_LC_MESSAGES_BUG

    /* Invalidate the glibc cache of loaded translations if the locale has
     * changed, see [perl #134264] and
     * https://sourceware.org/bugzilla/show_bug.cgi?id=24936 */
    if (old_messages_locale) {
        if (strNE(old_messages_locale, querylocale_c(LC_MESSAGES))) {
            textdomain(textdomain(NULL));
        }
    }

#  endif

    return true;

  must_restore_state:

    /* We earlier switched to the LC_ALL => C locale in anticipation of it
     * succeeding,  Now have to switch back to the state upon entry.  */
    if (! uselocale(entry_obj)) {
        setlocale_failure_panic_i(index, "switching back to",
                                  locale_on_entry, __LINE__, caller_line);
    }

    return false;
}

/*===========================================================================*/

#else
#  error Unexpected Configuration
#endif   /* End of the various implementations of the setlocale and
            querylocale macros used in the remainder of this program */

/*===========================================================================*/

/* Each implementation above is based on two fundamental macros #defined above:
 *  1) either a querylocale_r or a querylocale_i
 *  2) either a bool_setlocale_r or a bool_setlocale_i
 *
 * (Which one of each got #defined is based on which is most efficient in
 * interacting with the underlying libc functions called.)
 *
 * To complete the implementation, macros for the missing two suffixes must be
 * #defined, as well as all the void_setlocale_X() forms.  These all can be
 * mechanically derived from the fundamental ones. */

#ifdef querylocale_r
#  define querylocale_c(cat)    querylocale_r(cat)
#  define querylocale_i(i)      querylocale_r(categories[i])
#elif defined(querylocale_i)
#  define querylocale_c(cat)    querylocale_i(cat##_INDEX_)
#  define querylocale_r(cat)    querylocale_i(get_category_index(cat))
#else
#  error No querylocale() form defined
#endif

#ifdef bool_setlocale_r
#  define bool_setlocale_i(i, l)    bool_setlocale_r(categories[i], l)
#  define bool_setlocale_c(cat, l)  bool_setlocale_r(cat, l)

#  define void_setlocale_r_with_caller(cat, locale, file, line)             \
     STMT_START {                                                           \
        if (! bool_setlocale_r(cat, locale))                                \
            setlocale_failure_panic_via_i(get_category_index(cat),          \
                                          NULL, locale, __LINE__, 0,        \
                                          file, line);                      \
     } STMT_END

#  define void_setlocale_c_with_caller(cat, locale, file, line)             \
          void_setlocale_r_with_caller(cat, locale, file, line)

#  define void_setlocale_i_with_caller(i, locale, file, line)               \
          void_setlocale_r_with_caller(categories[i], locale, file, line)

#  define void_setlocale_r(cat, locale)                                     \
          void_setlocale_r_with_caller(cat, locale, __FILE__, __LINE__)
#  define void_setlocale_c(cat, locale)                                     \
          void_setlocale_r(cat, locale)
#  define void_setlocale_i(i, locale)                                       \
          void_setlocale_r(categories[i], locale)

#elif defined(bool_setlocale_i)
#  define bool_setlocale_c(cat, loc) bool_setlocale_i(cat##_INDEX_, loc)
#  define bool_setlocale_r(c, loc)   bool_setlocale_i(get_category_index(c), l)

#  define void_setlocale_i_with_caller(i, locale, file, line)               \
     STMT_START {                                                           \
        if (! bool_setlocale_i(i, locale))                                  \
            setlocale_failure_panic_via_i(i, NULL, locale, __LINE__, 0,     \
                                          file, line);                      \
     } STMT_END

#  define void_setlocale_r_with_caller(cat, locale, file, line)             \
          void_setlocale_i_with_caller(get_category_index(cat), locale,     \
                                       file, line)

#  define void_setlocale_c_with_caller(cat, locale, file, line)             \
          void_setlocale_i_with_caller(cat##_INDEX_, locale, file, line)

#  define void_setlocale_i(i, locale)                                       \
          void_setlocale_i_with_caller(i, locale, __FILE__, __LINE__)
#  define void_setlocale_c(cat, locale)                                     \
          void_setlocale_i(cat##_INDEX_, locale)
#  define void_setlocale_r(cat, locale)                                     \
          void_setlocale_i(get_category_index(cat), locale)

#else
#  error No bool_setlocale() form defined
#endif

/*===========================================================================*/

/* Most of the cases in this file just toggle the locale briefly; but there are
 * a few instances where a longer toggled interval, over multiple operations,
 * is desirable, since toggling and untoggling have a cost.  But on platforms
 * where toggling must be done in a critical section, it is even more desirable
 * to minimize the length of time in an uninterruptable state.
 *
 * The macros below try to balance these competing interests.  When the
 * toggling is to be brief, simply use the plain "toggle_locale" macros.  But
 * in addition, in the places where an over-arching toggle would be nice, add
 * calls to the macros below that have the "_locking" suffix.  These are no-ops
 * except on systems where the toggling doesn't force a critical section.  But
 * otherwise these toggle to the over-arching locale.  When the individual
 * toggles are executed, they will check and find that the locale is already in
 * the right state, and return without doing anything. */
#if TOGGLING_LOCKS
#  define toggle_locale_c_unless_locking(cat,          locale)  NULL
#  define toggle_locale_c_if_locking(    cat,          locale)              \
                         toggle_locale_i(cat##_INDEX_, locale)

#  define restore_toggled_locale_c_unless_locking(cat,          locale)     \
                         PERL_UNUSED_ARG(locale)
#  define restore_toggled_locale_c_if_locking(    cat,          locale)     \
                restore_toggled_locale_i(         cat##_INDEX_, locale)
#else
#  define toggle_locale_c_unless_locking(cat,          locale)              \
                         toggle_locale_i(cat##_INDEX_, locale)
#  define toggle_locale_c_if_locking(    cat,          locale)  NULL

#  define restore_toggled_locale_c_unless_locking(cat,          locale)     \
                         restore_toggled_locale_i(cat##_INDEX_, locale)
#  define restore_toggled_locale_c_if_locking(    cat,          locale)     \
                         PERL_UNUSED_ARG(locale)
#endif

/* query_nominal_locale_i() is used when the caller needs the locale that an
 * external caller would be expecting, and not what we're secretly using
 * behind the scenes.  It deliberately doesn't handle LC_ALL; use
 * calculate_LC_ALL_string() for that. */
#ifdef USE_LOCALE_NUMERIC
#  define query_nominal_locale_i(i)                                         \
      (__ASSERT_(i != LC_ALL_INDEX_)                                        \
       ((i == LC_NUMERIC_INDEX_) ? PL_numeric_name : querylocale_i(i)))
#elif defined(USE_LOCALE)
#  define query_nominal_locale_i(i)                                         \
      (__ASSERT_(i != LC_ALL_INDEX_) querylocale_i(i))
#else
#  define query_nominal_locale_i(i)  "C"
#endif

#ifdef USE_PL_CURLOCALES

STATIC void
S_update_PL_curlocales_i(pTHX_
                         const locale_category_index index,
                         const char * new_locale,
                         const line_t caller_line)
{
    /* Update PL_curlocales[], which is parallel to the other ones indexed by
     * our mapping of libc category number to our internal equivalents. */

    PERL_ARGS_ASSERT_UPDATE_PL_CURLOCALES_I;

    if (index == LC_ALL_INDEX_) {

        /* For LC_ALL, we change all individual categories to correspond,
         * including the LC_ALL element */
        for (unsigned int i = 0; i <= LC_ALL_INDEX_; i++) {
            Safefree(PL_curlocales[i]);
            PL_curlocales[i] = NULL;
        }

        switch (parse_LC_ALL_string(new_locale,
                                    (const char **) &PL_curlocales,
                                    check_that_overridden,  /* things should
                                                               have already
                                                               been overridden
                                                               */
                                    true,   /* Always fill array */
                                    true,   /* Panic if fails, as to get here
                                               it earlier had to have succeeded
                                               */
                                   caller_line))
        {
          case invalid:
          case no_array:
          case only_element_0:
            locale_panic_via_("Unexpected return from parse_LC_ALL_string",
                              __FILE__, caller_line);

          case full_array:
            /* parse_LC_ALL_string() has already filled PL_curlocales properly,
             * except for the LC_ALL element, which should be set to
             * 'new_locale'. */
            PL_curlocales[LC_ALL_INDEX_] = savepv(new_locale);
        }
    }
    else {  /* Not LC_ALL */

        /* Update the single category's record */
        Safefree(PL_curlocales[index]);
        PL_curlocales[index] = savepv(new_locale);

        /* Invalidate LC_ALL */
        Safefree(PL_curlocales[LC_ALL_INDEX_]);
        PL_curlocales[LC_ALL_INDEX_] = NULL;
    }
}

#  endif  /* Need PL_curlocales[] */

/*===========================================================================*/

#if defined(USE_LOCALE)

/* This paradigm is needed in several places in the function below.  We have to
 * substitute the nominal locale for LC_NUMERIC when returning a value for
 * external consumption */
#  ifndef USE_LOCALE_NUMERIC
#    define ENTRY(i, array, format)  array[i]
#  else
#    define ENTRY(i, array, format)                         \
       (UNLIKELY(   format == EXTERNAL_FORMAT_FOR_QUERY     \
                 && i == LC_NUMERIC_INDEX_)                 \
        ? PL_numeric_name                                   \
        : array[i])
#  endif

STATIC
const char *
S_calculate_LC_ALL_string(pTHX_ const char ** category_locales_list,
                                const calc_LC_ALL_format format,
                                const calc_LC_ALL_return returning,
                                const line_t caller_line)
{
    PERL_ARGS_ASSERT_CALCULATE_LC_ALL_STRING;

    /* NOTE: On Configurations that have PL_curlocales[], this function has the
     * side effect of updating the LC_ALL_INDEX_ element with its result.
     *
     * This function calculates a string that defines the locale(s) LC_ALL is
     * set to, in either:
     *  1)  Our internal format if 'format' is set to INTERNAL_FORMAT.
     *  2)  The external format returned by Perl_setlocale() if 'format' is set
     *      to EXTERNAL_FORMAT_FOR_QUERY or EXTERNAL_FORMAT_FOR_SET.
     *
     *      These two are distinguished by:
     *       a) EXTERNAL_FORMAT_FOR_SET returns the actual locale currently in
     *          effect.
     *       b) EXTERNAL_FORMAT_FOR_QUERY returns the nominal locale.
     *          Currently this can differ only from the actual locale in the
     *          LC_NUMERIC category when it is set to a locale whose radix is
     *          not a dot.  (The actual locale is kept as a dot to accommodate
     *          the large corpus of XS code that expects it to be that;
     *          switched to a non-dot temporarily during certain operations
     *          that require the actual radix.)
     *
     * In both 1) and 2), LC_ALL's values are passed to this function by
     * 'category_locales_list' which is either:
     *  1) a pointer to an array of strings with up-to-date values of all the
     *     individual categories; or
     *  2) NULL, to indicate to use querylocale_i() to get each individual
     *     value.
     *
     * The caller sets 'returning' to
     *      WANT_TEMP_PV        the function returns the calculated string
     *                              as a mortalized temporary, so the caller
     *                              doesn't have to worry about it being
     *                              per-thread, nor needs to arrange for its
     *                              clean-up.
     *      WANT_PL_setlocale_buf  the function stores the calculated string
     *                              into the per-thread buffer PL_setlocale_buf
     *                              and returns a pointer to that.  The buffer
     *                              is cleaned up automatically in process
     *                              destruction.  This return method avoids
     *                              extra copies in some circumstances.
     *      WANT_VOID           NULL is returned.  This is used when the
     *                              function is being called only for its side
     *                              effect of updating
     *                              PL_curlocales[LC_ALL_INDEX_]
     *
     * querylocale(), on systems that have it, doesn't tend to work for LC_ALL.
     * So we have to construct the answer ourselves based on the passed in
     * data.
     *
     * If all individual categories are the same locale, we can just set LC_ALL
     * to that locale.  But if not, we have to create an aggregation of all the
     * categories on the system.  Platforms differ as to the syntax they use
     * for these non-uniform locales for LC_ALL.  Some, like glibc and Windows,
     * use an unordered series of name=value pairs, like
     *      LC_NUMERIC=C;LC_TIME=en_US.UTF-8;...
     * to specify LC_ALL; others, like *BSD, use a positional notation with a
     * delimitter, typically a single '/' character:
     *      C/en_UK.UTF-8/...
     *
     * When the external format is desired, this function returns whatever the
     * system expects.  The internal format is always name=value pairs.
     *
     * For systems that have categories we don't know about, the algorithm
     * below won't know about those missing categories, leading to potential
     * bugs for code that looks at them.  If there is an environment variable
     * that sets that category, we won't know to look for it, and so our use of
     * LANG or "C" improperly overrides it.  On the other hand, if we don't do
     * what is done here, and there is no environment variable, the category's
     * locale should be set to LANG or "C".  So there is no good solution.  khw
     * thinks the best is to make sure we have a complete list of possible
     * categories, adding new ones as they show up on obscure platforms.
     */

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "Entering calculate_LC_ALL_string(%s);"
                           " called from %" LINE_Tf "\n",
                           ((format == EXTERNAL_FORMAT_FOR_QUERY)
                            ? "EXTERNAL_FORMAT_FOR_QUERY"
                            : ((format == EXTERNAL_FORMAT_FOR_SET)
                               ? "EXTERNAL_FORMAT_FOR_SET"
                               : "INTERNAL_FORMAT")),
                           caller_line));

    bool input_list_was_NULL = (category_locales_list == NULL);

    /* If there was no input category list, construct a temporary one
     * ourselves. */
    const char * my_category_locales_list[LC_ALL_INDEX_];
    const char ** locales_list = category_locales_list;
    if (locales_list == NULL) {
        locales_list = my_category_locales_list;

        if (format == EXTERNAL_FORMAT_FOR_QUERY) {
            for_all_individual_category_indexes(i) {
                locales_list[i] = query_nominal_locale_i(i);
            }
        }
        else {
            for_all_individual_category_indexes(i) {
                locales_list[i] = querylocale_i(i);
            }
        }
    }

    /* While we are calculating LC_ALL, we see if every category's locale is
     * the same as every other's or not. */
#  ifndef HAS_IGNORED_LOCALE_CATEGORIES_

    /* When we pay attention to all categories, we assume they are all the same
     * until proven different */
    bool disparate = false;

#  else

    /* But if there are ignored categories, those will be set to "C", so try an
     * arbitrary category, and if it isn't C, we know immediately that the
     * locales are disparate.  (The #if conditionals are to handle the case
     * where LC_NUMERIC_INDEX_ is 0.  We don't want to use LC_NUMERIC to
     * compare, as that may be different between external and internal forms.)
     * */
#    if ! defined(USE_LOCALE_NUMERIC)

    bool disparate = ! isNAME_C_OR_POSIX(locales_list[0]);

#    elif LC_NUMERIC_INDEX_ != 0

    bool disparate = ! isNAME_C_OR_POSIX(locales_list[0]);

#    else

    /* Would need revision to handle the very unlikely case where only a single
     * category, LC_NUMERIC, is defined */
    assert(LOCALE_CATEGORIES_COUNT_ > 0);

    bool disparate = ! isNAME_C_OR_POSIX(locales_list[1]);

#    endif
#  endif

    /* Calculate the needed size for the string listing the individual locales.
     * Initialize with values known at compile time. */
    Size_t total_len;
    const char *separator;

#  ifdef PERL_LC_ALL_USES_NAME_VALUE_PAIRS  /* Positional formatted LC_ALL */
    PERL_UNUSED_ARG(format);
#  else

    if (format != INTERNAL_FORMAT) {

        /* Here, we will be using positional notation.  it includes n-1
         * separators */
        total_len = (  LOCALE_CATEGORIES_COUNT_ - 1)
                     * STRLENs(PERL_LC_ALL_SEPARATOR)
                  + 1;   /* And a trailing NUL */
        separator = PERL_LC_ALL_SEPARATOR;
    }
    else

#  endif

    {
        /* name=value output is always used in internal format, and when
         * positional isn't available on the platform. */
        total_len = lc_all_boiler_plate_length;
        separator = ";";
    }

    /* The total length then is just the sum of the above boiler-plate plus the
     * total strlen()s of the locale name of each individual category. */
    for_all_individual_category_indexes(i) {
        const char * entry = ENTRY(i, locales_list, format);

        total_len += strlen(entry);
        if (! disparate && strNE(entry, locales_list[0])) {
            disparate = true;
        }
    }

    bool free_if_void_return = false;
    const char * retval;

    /* If all categories have the same locale, we already know the answer */
    if (! disparate) {
        if (returning == WANT_PL_setlocale_buf) {
            save_to_buffer(locales_list[0],
                           &PL_setlocale_buf,
                           &PL_setlocale_bufsize);
            retval = PL_setlocale_buf;
        }
        else {

            retval = locales_list[0];

            /* If a temporary is wanted for the return, and we had to create
             * the input list ourselves, we created it into such a temporary,
             * so no further work is needed; but otherwise, make a mortal copy
             * of this passed-in list element */
            if (returning == WANT_TEMP_PV && ! input_list_was_NULL) {
                retval = savepv(retval);
                SAVEFREEPV(retval);
            }

            /* In all cases here, there's nothing we create that needs to be
             * freed, so leave 'free_if_void_return' set to the default
             * 'false'. */
        }
    }
    else {  /* Here, not all categories have the same locale */

        char * constructed;

        /* If returning to PL_setlocale_buf, set up to write directly to it,
         * being sure it is resized to be large enough */
        if (returning == WANT_PL_setlocale_buf) {
            set_save_buffer_min_size(total_len,
                                     &PL_setlocale_buf,
                                     &PL_setlocale_bufsize);
            constructed = PL_setlocale_buf;
        }
        else {  /* Otherwise we need new memory to hold the calculated value. */

            Newx(constructed, total_len, char);

            /* If returning the new memory, it must be set up to be freed
             * later; otherwise at the end of this function */
            if (returning == WANT_TEMP_PV) {
                SAVEFREEPV(constructed);
            }
            else {
                free_if_void_return = true;
            }
        }

        constructed[0] = '\0';

        /* Loop through all the categories */
        for_all_individual_category_indexes(j) {

            /* Add a separator, except before the first one */
            if (j != 0) {
                my_strlcat(constructed, separator, total_len);
            }

            const char * entry;
            Size_t needed_len;
            unsigned int i = j;

#  ifndef PERL_LC_ALL_USES_NAME_VALUE_PAIRS

            if (UNLIKELY(format != INTERNAL_FORMAT)) {

                /* In positional notation 'j' means the position, and we have
                 * to convert to the index 'i' */
                i = map_LC_ALL_position_to_index[j];

                entry = ENTRY(i, locales_list, format);
                needed_len = my_strlcat(constructed, entry, total_len);
            }
            else

#  endif
            {
                /* Below, we are to use name=value notation, either because
                 * that's what the platform uses, or because this is the
                 * internal format, which uses that notation regardless of the
                 * external form */

                entry = ENTRY(i, locales_list, format);

                /* "name=locale;" */
                my_strlcat(constructed, category_names[i], total_len);
                my_strlcat(constructed, "=", total_len);
                needed_len = my_strlcat(constructed, entry, total_len);
            }

            if (LIKELY(needed_len <= total_len)) {
                continue;
            }

            /* If would have overflowed, panic */
            locale_panic_via_(Perl_form(aTHX_
                                        "Internal length calculation wrong.\n"
                                        "\"%s\" was not entirely added to"
                                        " \"%.*s\"; needed=%zu, had=%zu",
                                        entry, (int) total_len,
                                        constructed,
                                        needed_len, total_len),
                                __FILE__,
                                caller_line);
        } /* End of loop through the categories */

        retval = constructed;
    } /* End of the categories' locales are displarate */

#  if defined(USE_PL_CURLOCALES) && defined(LC_ALL)

    if (format == INTERNAL_FORMAT) {

        /* PL_curlocales[LC_ALL_INDEX_] is updated as a side-effect of this
         * function for internal format. */
        Safefree(PL_curlocales[LC_ALL_INDEX_]);
        PL_curlocales[LC_ALL_INDEX_] = savepv(retval);
    }

#  endif

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "calculate_LC_ALL_string calculated '%s'\n",
                           retval));

    if (returning == WANT_VOID) {
        if (free_if_void_return) {
            Safefree(retval);
        }

        return NULL;
    }

    return retval;
}

#  if defined(WIN32) || (     defined(USE_POSIX_2008_LOCALE)        \
                         && ! defined(USE_QUERYLOCALE))

STATIC const char *
S_find_locale_from_environment(pTHX_ const locale_category_index index)
{
    /* NB: This function may actually change the locale on Windows.  It
     * currently is designed to be called only from setting the locale on
     * Windows, and POSIX 2008
     *
     * This function returns the locale specified by the program's environment
     * for the category specified by our internal index number 'index'.  It
     * therefore simulates:
     *      setlocale(cat, "")
     * but, except for some cases in Windows, doesn't actually change the
     * locale; merely returns it.
     *
     * The return need not be freed by the caller.  This
     * promise relies on PerlEnv_getenv() returning a mortalized copy to us.
     *
     * The simulation is needed only on certain platforms; otherwise, libc is
     * called with "" to get the actual value(s).  The simulation is needed
     * for:
     *
     *  1)  On Windows systems, the concept of the POSIX ordering of
     *      environment variables is missing.  To increase portability of
     *      programs across platforms, the POSIX ordering is emulated on
     *      Windows.
     *
     *  2)  On POSIX 2008 systems without querylocale(), it is problematic
     *      getting the results of the POSIX 2008 equivalent of
     *
     *          setlocale(category, "")
     *
     *      To ensure that we know exactly what those values are, we do the
     *      setting ourselves, using the documented algorithm specified by the
     *      POSIX standard (assuming the platform follows the Standard) rather
     *      than use "" as the locale.  This will lead to results that differ
     *      from native behavior if the native behavior differs from the
     *      Standard's documented value, but khw believes it is better to know
     *      what's going on, even if different from native, than to just guess.
     *
     *      glibc systems differ from this standard in having a LANGUAGE
     *      environment variable used for just LC_MESSAGES.  This function does
     *      NOT handle that.
     *
     *      Another option for the POSIX 2008 case would be, in a critical
     *      section, to save the global locale's current value, and do a
     *      straight setlocale(LC_ALL, "").  That would return our desired
     *      values, destroying the global locale's, which we would then
     *      restore.  But that could cause races with any other thread that is
     *      using the global locale and isn't using the mutex.  And, the only
     *      reason someone would have done that is because they are calling a
     *      library function, like in gtk, that calls setlocale(), and which
     *      can't be changed to use the mutex.  That wouldn't be a problem if
     *      this were to be done before any threads had switched, say during
     *      perl construction time.  But this code would still be needed for
     *      the general case.
     *
     * The Windows and POSIX 2008 differ in that the ultimate fallback is "C"
     * in POSIX, and is the system default locale in Windows.  To get that
     * system default value, we actually have to call setlocale() on Windows.
     */

    const char * const lc_all = PerlEnv_getenv("LC_ALL");
    const char * locale_names[LC_ALL_INDEX_] = { NULL };

    /* Use any "LC_ALL" environment variable, as it overrides everything else.
     * */
    if (lc_all && strNE(lc_all, "")) {
        return lc_all;
    }

    /* Here, no usable LC_ALL environment variable.  We have to handle each
     * category separately.  If all categories are desired, we loop through
     * them all.  If only an individual category is desired, to avoid
     * duplicating logic, we use the same loop, but set up the limits so it is
     * only executed once, for that particular category. */
    locale_category_index lower, upper, offset;
    if (index == LC_ALL_INDEX_) {
        lower = (locale_category_index) 0;
        upper = (locale_category_index) ((int) LC_ALL_INDEX_ - 1);
        offset = (locale_category_index) 0;
    }
    else {
        lower = index;
        upper = index;

        /* 'offset' is used so that the result of the single loop iteration is
         * stored into output[0] */
        offset = lower;
    }

    /* When no LC_ALL environment variable, LANG is used as a default, but
     * overridden for individual categories that have corresponding environment
     * variables.  If no LANG exists, the default is "C" on POSIX 2008, or the
     * system default for the category on Windows. */
    const char * env_lang = NULL;

    /* For each desired category, use any corresponding environment variable;
     * or the default if none such exists. */
    bool is_disparate = false;  /* Assume is uniform until proven otherwise */
    for_category_indexes_between(i, lower, upper) {
        const char * const env_override = PerlEnv_getenv(category_names[i]);
        locale_category_index j = (locale_category_index) (i - offset);

        if (env_override && strNE(env_override, "")) {
            locale_names[j] = env_override;
        }
        else { /* Here, no corresponding environment variable, see if LANG
                  exists and is usable.  Done this way to avoid fetching LANG
                  unless it is actually needed */
            if (env_lang == NULL) {
                env_lang = PerlEnv_getenv("LANG");

                /* If not usable, set it to a non-NULL illegal value so won't
                 * try to use it below */
                if (env_lang == NULL || strEQ(env_lang, "")) {
                    env_lang = (const char *) 1;
                }
            }

            /* If a usable LANG exists, use it. */
            if (env_lang != NULL && env_lang != (const char *) 1) {
                locale_names[j] = env_lang;
            }
            else {

#    ifdef WIN32
                /* If no LANG, use the system default on Windows. */
                locale_names[j] = wrap_wsetlocale(categories[i], ".ACP");
                if (locale_names[j]) {
                    SAVEFREEPV(locale_names[j]);
                }
                else
#    endif
                {   /* If nothing was found or worked, use C */
                    locale_names[j] = "C";
                }
            }
        }

        if (j > 0 && ! is_disparate && strNE(locale_names[0], locale_names[j]))
        {
            is_disparate = true;
        }

        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                 "find_locale_from_environment i=%u, j=%u, name=%s,"
                 " locale=%s, locale of 0th category=%s, disparate=%d\n",
                 i, j, category_names[i],
                 locale_names[j], locale_names[0], is_disparate));
    }

    if (! is_disparate) {
        return locale_names[0];
    }

    return calculate_LC_ALL_string(locale_names, INTERNAL_FORMAT,
                                   WANT_TEMP_PV,
                                   __LINE__);
}

#  endif
#  if defined(DEBUGGING) || defined(USE_PERL_SWITCH_LOCALE_CONTEXT)

STATIC const char *
S_get_LC_ALL_display(pTHX)
{
    return calculate_LC_ALL_string(NULL, INTERNAL_FORMAT,
                                   WANT_TEMP_PV,
                                   __LINE__);
}

#  endif

STATIC void
S_setlocale_failure_panic_via_i(pTHX_
                                const locale_category_index cat_index,
                                const char * current,
                                const char * failed,
                                const line_t proxy_caller_line,
                                const line_t immediate_caller_line,
                                const char * const higher_caller_file,
                                const line_t higher_caller_line)
{
    PERL_ARGS_ASSERT_SETLOCALE_FAILURE_PANIC_VIA_I;

    /* Called to panic when a setlocale form unexpectedly failed for the
     * category determined by 'cat_index', and the locale that was in effect
     * (and likely still is) is 'current'.  'current' may be NULL, which causes
     * this function to query what it is.
     *
     * The extra caller information is used for when a function acts as a
     * stand-in for another function, which a typical reader would more likely
     * think would be the caller
     *
     * If a line number is 0, its stack (sort-of) frame is omitted; same if
     * it's the same line number as the next higher caller. */

    const int cat = categories[cat_index];
    const char * name = category_names[cat_index];

    dSAVE_ERRNO;

    if (current == NULL) {
        current = querylocale_i(cat_index);
    }

    const char * proxy_text = "";
    if (proxy_caller_line != 0 && proxy_caller_line != immediate_caller_line)
    {
        proxy_text = Perl_form(aTHX_ "\nCalled via %s: %" LINE_Tf,
                                      __FILE__, proxy_caller_line);
    }
    if (   strNE(__FILE__, higher_caller_file)
        || (   immediate_caller_line != 0
            && immediate_caller_line != higher_caller_line))
    {
        proxy_text = Perl_form(aTHX_ "%s\nCalled via %s: %" LINE_Tf,
                                      proxy_text, __FILE__,
                                      immediate_caller_line);
    }

    /* 'false' in the get_displayable_string() calls makes it not think the
     * locale is UTF-8, so just dumps bytes.  Actually figuring it out can be
     * too complicated for a panic situation. */
    const char * msg = Perl_form(aTHX_
                            "Can't change locale for %s (%d) from '%s' to '%s'"
                            " %s",
                            name, cat,
                            get_displayable_string(current,
                                                   current + strlen(current),
                                                   false),
                            get_displayable_string(failed,
                                                   failed + strlen(failed),
                                                   false),
                            proxy_text);
    RESTORE_ERRNO;

    Perl_locale_panic(msg, __LINE__, higher_caller_file, higher_caller_line);
    NOT_REACHED; /* NOTREACHED */
}

#  ifdef USE_LOCALE_NUMERIC

STATIC void
S_new_numeric(pTHX_ const char *newnum, bool force)
{
    PERL_ARGS_ASSERT_NEW_NUMERIC;

    /* Called after each libc setlocale() or uselocale() call affecting
     * LC_NUMERIC, to tell core Perl this and that 'newnum' is the name of the
     * new locale, and we are switched into it.  It installs this locale as the
     * current underlying default, and then switches to the C locale, if
     * necessary, so that the code that has traditionally expected the radix
     * character to be a dot may continue to do so.
     *
     * The default locale and the C locale can be toggled between by use of the
     * set_numeric_underlying() and set_numeric_standard() functions, which
     * should probably not be called directly, but only via macros like
     * SET_NUMERIC_STANDARD() in perl.h.
     *
     * The toggling is necessary mainly so that a non-dot radix decimal point
     * character can be input and output, while allowing internal calculations
     * to use a dot.
     *
     * This sets several interpreter-level variables:
     * PL_numeric_name  The underlying locale's name: a copy of 'newnum'
     * PL_numeric_underlying   A boolean indicating if the toggled state is
     *                  such that the current locale is the program's
     *                  underlying locale
     * PL_numeric_standard   An int indicating if the toggled state is such
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
     * PL_numeric_radix_sv   Contains the string that code should use for the
     *                  decimal point.  It is set to either a dot or the
     *                  program's underlying locale's radix character string,
     *                  depending on the situation.
     * PL_underlying_radix_sv   Contains the program's underlying locale's
     *                  radix character string.  This is copied into
     *                  PL_numeric_radix_sv when the situation warrants.  It
     *                  exists to avoid having to recalculate it when toggling.
     */

    DEBUG_L( PerlIO_printf(Perl_debug_log,
                           "Called new_numeric with %s, PL_numeric_name=%s\n",
                           newnum, PL_numeric_name));

    /* We keep records comparing the characteristics of the LC_NUMERIC catetory
     * of the current locale vs the standard C locale.  If the new locale that
     * has just been changed to is the same as the one our records are for,
     * they are still valid, and we don't have to recalculate them.  'force' is
     * true if the caller suspects that the records are out-of-date, so do go
     * ahead and recalculate them.  (This can happen when an external library
     * has had control and now perl is reestablishing control; we have to
     * assume that that library changed the locale in unknown ways.)
     *
     * Even if our records are valid, the new locale will likely have been
     * switched to before this function gets called, and we must toggle into
     * one indistinguishable from the C locale with regards to LC_NUMERIC
     * handling, so that all the libc functions that are affected by LC_NUMERIC
     * will work as expected.  This can be skipped if we already know that the
     * locale is indistinguishable from the C locale. */
    if (! force && strEQ(PL_numeric_name, newnum)) {
        if (! PL_numeric_underlying_is_standard) {
            set_numeric_standard(__FILE__, __LINE__);
        }

        return;
    }

    Safefree(PL_numeric_name);
    PL_numeric_name = savepv(newnum);

    /* Handle the trivial case.  Since this is called at process
     * initialization, be aware that this bit can't rely on much being
     * available. */
    if (isNAME_C_OR_POSIX(PL_numeric_name)) {
        PL_numeric_standard = TRUE;
        PL_numeric_underlying_is_standard = TRUE;
        PL_numeric_underlying = TRUE;
        sv_setpv(PL_numeric_radix_sv, C_decimal_point);
        SvUTF8_off(PL_numeric_radix_sv);
        sv_setpv(PL_underlying_radix_sv, C_decimal_point);
        SvUTF8_off(PL_underlying_radix_sv);
        return;
    }

    /* We are in the underlying locale until changed at the end of this
     * function */
    PL_numeric_underlying = TRUE;

    /* Passing a non-NULL causes the function call just below to
       automatically set the UTF-8 flag on PL_underlying_radix_sv */
    utf8ness_t dummy;

    /* Find and save this locale's radix character. */
    langinfo_sv_c(RADIXCHAR, LC_NUMERIC, PL_numeric_name,
                  PL_underlying_radix_sv, &dummy);
    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Locale radix is '%s', ?UTF-8=%d\n",
                          SvPVX(PL_underlying_radix_sv),
                          cBOOL(SvUTF8(PL_underlying_radix_sv))));

    /* This locale is indistinguishable from C (for numeric purposes) if both
     * the radix character and the thousands separator are the same as C's.
     * Start with the radix. */
    PL_numeric_underlying_is_standard = strEQ(C_decimal_point,
                                              SvPVX(PL_underlying_radix_sv));

#    ifndef TS_W32_BROKEN_LOCALECONV

    /* If the radix isn't the same as C's, we know it is distinguishable from
     * C; otherwise check the thousands separator too.  Only if both are the
     * same as C's is the locale indistinguishable from C.
     *
     * But on earlier Windows versions, there is a potential race.  This code
     * knows that localeconv() (elsewhere in this file) will be used to extract
     * the needed value, and localeconv() was buggy for quite a while, and that
     * code in this file hence uses a workaround.  And that workaround may have
     * an (unlikely) race.  Gathering the radix uses a different workaround on
     * Windows that doesn't involve a race.  It might be possible to do the
     * same for this (patches welcome).
     *
     * Until then khw doesn't think it's worth even the small risk of a race to
     * get this value, which doesn't appear to be used in any of the Microsoft
     * library routines anyway. */

    if (PL_numeric_underlying_is_standard) {
        PL_numeric_underlying_is_standard = strEQ(C_thousands_sep,
                                                  langinfo_c(THOUSEP,
                                                             LC_NUMERIC,
                                                             PL_numeric_name,
                                                             NULL));
    }

#    endif

    PL_numeric_standard = PL_numeric_underlying_is_standard;

    /* Keep LC_NUMERIC so that it has the C locale radix and thousands
     * separator.  This is for XS modules, so they don't have to worry about
     * the radix being a non-dot.  (Core operations that need the underlying
     * locale change to it temporarily). */
    if (! PL_numeric_standard) {
        set_numeric_standard(__FILE__, __LINE__);
    }
}

#  endif

void
Perl_set_numeric_standard(pTHX_ const char * const file, const line_t line)
{
    PERL_ARGS_ASSERT_SET_NUMERIC_STANDARD;
    PERL_UNUSED_ARG(file);      /* Some Configurations ignore these */
    PERL_UNUSED_ARG(line);

#  ifdef USE_LOCALE_NUMERIC

    /* Unconditionally toggle the LC_NUMERIC locale to the C locale
     *
     * Most code should use the macro SET_NUMERIC_STANDARD() in perl.h
     * instead of calling this directly.  The macro avoids calling this routine
     * if toggling isn't necessary according to our records (which could be
     * wrong if some XS code has changed the locale behind our back) */

    DEBUG_L(PerlIO_printf(Perl_debug_log, "Setting LC_NUMERIC locale to"
                                          " standard C; called from %s: %"
                                          LINE_Tf "\n", file, line));

    void_setlocale_c_with_caller(LC_NUMERIC, "C", file, line);
    PL_numeric_standard = TRUE;
    sv_setpv(PL_numeric_radix_sv, C_decimal_point);
    SvUTF8_off(PL_numeric_radix_sv);

    PL_numeric_underlying = PL_numeric_underlying_is_standard;

#  endif /* USE_LOCALE_NUMERIC */

}

void
Perl_set_numeric_underlying(pTHX_ const char * const file, const line_t line)
{
    PERL_ARGS_ASSERT_SET_NUMERIC_UNDERLYING;
    PERL_UNUSED_ARG(file);      /* Some Configurations ignore these */
    PERL_UNUSED_ARG(line);

#  ifdef USE_LOCALE_NUMERIC

    /* Unconditionally toggle the LC_NUMERIC locale to the current underlying
     * default.
     *
     * Most code should use the macro SET_NUMERIC_UNDERLYING() in perl.h
     * instead of calling this directly.  The macro avoids calling this routine
     * if toggling isn't necessary according to our records (which could be
     * wrong if some XS code has changed the locale behind our back) */

    DEBUG_L(PerlIO_printf(Perl_debug_log, "Setting LC_NUMERIC locale to %s;"
                                          " called from %s: %" LINE_Tf "\n",
                                          PL_numeric_name, file, line));
    /* Maybe not in init? assert(PL_locale_mutex_depth > 0);*/

    void_setlocale_c_with_caller(LC_NUMERIC, PL_numeric_name, file, line);
    PL_numeric_underlying = TRUE;
    sv_setsv_nomg(PL_numeric_radix_sv, PL_underlying_radix_sv);

    PL_numeric_standard = PL_numeric_underlying_is_standard;

#  endif /* USE_LOCALE_NUMERIC */

}

#  ifdef USE_LOCALE_CTYPE

STATIC void
S_new_ctype(pTHX_ const char *newctype, bool force)
{
    PERL_ARGS_ASSERT_NEW_CTYPE;
    PERL_UNUSED_ARG(force);

    /* Called after each libc setlocale() call affecting LC_CTYPE, to tell
     * core Perl this and that 'newctype' is the name of the new locale.
     *
     * This function sets up the folding arrays for all 256 bytes, assuming
     * that tofold() is tolc() since fold case is not a concept in POSIX,
     */

    DEBUG_L(PerlIO_printf(Perl_debug_log, "Entering new_ctype(%s)\n",
                                          newctype));

    /* No change means no-op */
    if (strEQ(PL_ctype_name, newctype)) {
        return;
    }

    /* We will replace any bad locale warning with
     *  1)  nothing if the new one is ok; or
     *  2)  a new warning for the bad new locale */
    if (PL_warn_locale) {
        SvREFCNT_dec_NN(PL_warn_locale);
        PL_warn_locale = NULL;
    }

    /* Clear cache */
    Safefree(PL_ctype_name);
    PL_ctype_name = "";

    PL_in_utf8_turkic_locale = FALSE;

    /* For the C locale, just use the standard folds, and we know there are no
     * glitches possible, so return early.  Since this is called at process
     * initialization, be aware that this bit can't rely on much being
     * available. */
    if (isNAME_C_OR_POSIX(newctype)) {
        Copy(PL_fold, PL_fold_locale, 256, U8);
        PL_ctype_name = savepv(newctype);
        PL_in_utf8_CTYPE_locale = FALSE;
        return;
    }

    /* The cache being cleared signals the called function to compute a new
     * value */
    PL_in_utf8_CTYPE_locale = is_locale_utf8(newctype);

    PL_ctype_name = savepv(newctype);
    bool maybe_utf8_turkic = FALSE;

    /* Don't check for problems if we are suppressing the warnings */
    bool check_for_problems = ckWARN_d(WARN_LOCALE) || UNLIKELY(DEBUG_L_TEST);

    if (PL_in_utf8_CTYPE_locale) {

        /* A UTF-8 locale gets standard rules.  But note that code still has to
         * handle this specially because of the three problematic code points
         * */
        Copy(PL_fold_latin1, PL_fold_locale, 256, U8);

        /* UTF-8 locales can have special handling for 'I' and 'i' if they are
         * Turkic.  Make sure these two are the only anomalies.  (We don't
         * require towupper and towlower because they aren't in C89.) */

#    if defined(HAS_TOWUPPER) && defined (HAS_TOWLOWER)

        if (towupper('i') == 0x130 && towlower('I') == 0x131)

#    else

        if (toU8_UPPER_LC('i') == 'i' && toU8_LOWER_LC('I') == 'I')

#    endif

        {
            /* This is how we determine it really is Turkic */
            check_for_problems = TRUE;
            maybe_utf8_turkic = TRUE;
        }
    }
    else {  /* Not a canned locale we know the values for.  Compute them */

#    ifdef DEBUGGING

        bool has_non_ascii_fold = FALSE;
        bool found_unexpected = FALSE;

        /* Under -DLv, see if there are any folds outside the ASCII range.
         * This factoid is used below */
        if (DEBUG_Lv_TEST) {
            for (unsigned i = 128; i < 256; i++) {
                int j = LATIN1_TO_NATIVE(i);
                if (toU8_LOWER_LC(j) != j || toU8_UPPER_LC(j) != j) {
                    has_non_ascii_fold = TRUE;
                    break;
                }
            }
        }

#    endif

        for (unsigned i = 0; i < 256; i++) {
            if (isU8_UPPER_LC(i))
                PL_fold_locale[i] = (U8) toU8_LOWER_LC(i);
            else if (isU8_LOWER_LC(i))
                PL_fold_locale[i] = (U8) toU8_UPPER_LC(i);
            else
                PL_fold_locale[i] = (U8) i;

#    ifdef DEBUGGING

            /* Most locales these days are supersets of ASCII.  When debugging,
             * it is helpful to know what the exceptions to that are in this
             * locale */
            if (DEBUG_L_TEST) {
                bool unexpected = FALSE;

                if (isUPPER_L1(i)) {
                    if (isUPPER_A(i)) {
                        if (PL_fold_locale[i] != toLOWER_A(i)) {
                            unexpected = TRUE;
                        }
                    }
                    else if (has_non_ascii_fold) {
                        if (PL_fold_locale[i] != toLOWER_L1(i)) {
                            unexpected = TRUE;
                        }
                    }
                    else if (PL_fold_locale[i] != i) {
                        unexpected = TRUE;
                    }
                }
                else if (   isLOWER_L1(i)
                         && i != LATIN_SMALL_LETTER_SHARP_S
                         && i != MICRO_SIGN)
                {
                    if (isLOWER_A(i)) {
                        if (PL_fold_locale[i] != toUPPER_A(i)) {
                            unexpected = TRUE;
                        }
                    }
                    else if (has_non_ascii_fold) {
                        if (PL_fold_locale[i] != toUPPER_LATIN1_MOD(i)) {
                            unexpected = TRUE;
                        }
                    }
                    else if (PL_fold_locale[i] != i) {
                        unexpected = TRUE;
                    }
                }
                else if (PL_fold_locale[i] != i) {
                    unexpected = TRUE;
                }

                if (unexpected) {
                    found_unexpected = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                           "For %s, fold of %02x is %02x\n",
                                           newctype, i, PL_fold_locale[i]));
                }
            }
        }

        if (found_unexpected) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                               "All bytes not mentioned above either fold to"
                               " themselves or are the expected ASCII or"
                               " Latin1 ones\n"));
        }
        else {
            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                                   "No nonstandard folds were found\n"));
#    endif

        }
    }

#    ifdef MB_CUR_MAX

    /* We only handle single-byte locales (outside of UTF-8 ones); so if this
     * locale requires more than one byte, there are going to be BIG problems.
     * */

    const int mb_cur_max = MB_CUR_MAX;

    if (mb_cur_max > 1 && ! PL_in_utf8_CTYPE_locale

            /* Some platforms return MB_CUR_MAX > 1 for even the "C" locale.
             * Just assume that the implementation for them (plus for POSIX) is
             * correct and the > 1 value is spurious.  (Since these are
             * specially handled to never be considered UTF-8 locales, as long
             * as this is the only problem, everything should work fine */
        && ! isNAME_C_OR_POSIX(newctype))
    {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                              "Unsupported, MB_CUR_MAX=%d\n", mb_cur_max));

        if (! IN_LC(LC_CTYPE) || ckWARN_d(WARN_LOCALE)) {
            char * msg = Perl_form(aTHX_
                                   "Locale '%s' is unsupported, and may hang"
                                   " or crash the interpreter",
                                     newctype);
            if (IN_LC(LC_CTYPE)) {
                Perl_warner(aTHX_ packWARN(WARN_LOCALE), "%s", msg);
            }
            else {
                PL_warn_locale = newSV(0);
                sv_setpvn(PL_warn_locale, msg, strlen(msg));
            }
        }
    }

#    endif

    DEBUG_Lv(PerlIO_printf(Perl_debug_log, "check_for_problems=%d\n",
                                           check_for_problems));

    /* We don't populate the other lists if a UTF-8 locale, but do check that
     * everything works as expected, unless checking turned off */
    if (check_for_problems) {
        /* Assume enough space for every character being bad.  4 spaces each
         * for the 94 printable characters that are output like "'x' "; and 5
         * spaces each for "'\\' ", "'\t' ", and "'\n' "; plus a terminating
         * NUL */
        char bad_chars_list[ (94 * 4) + (3 * 5) + 1 ] = { '\0' };
        unsigned int bad_count = 0;         /* Count of bad characters */

        for (unsigned i = 0; i < 256; i++) {

            /* If checking for locale problems, see if the native ASCII-range
             * printables plus \n and \t are in their expected categories in
             * the new locale.  If not, this could mean big trouble, upending
             * Perl's and most programs' assumptions, like having a
             * metacharacter with special meaning become a \w.  Fortunately,
             * it's very rare to find locales that aren't supersets of ASCII
             * nowadays.  It isn't a problem for most controls to be changed
             * into something else; we check only \n and \t, though perhaps \r
             * could be an issue as well. */
            if (isGRAPH_A(i) || isBLANK_A(i) || i == '\n') {
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
                if (UNLIKELY(cBOOL(isU8_ALPHANUMERIC_LC(i)) !=
                                                    cBOOL(isALPHANUMERIC_A(i))))
                {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                        "isalnum('%s') unexpectedly is %x\n",
                                        name, cBOOL(isU8_ALPHANUMERIC_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_ALPHA_LC(i)) != cBOOL(isALPHA_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isalpha('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_ALPHA_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_DIGIT_LC(i)) != cBOOL(isDIGIT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isdigit('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_DIGIT_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_GRAPH_LC(i)) != cBOOL(isGRAPH_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isgraph('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_GRAPH_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_LOWER_LC(i)) != cBOOL(isLOWER_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "islower('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_LOWER_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_PRINT_LC(i)) != cBOOL(isPRINT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isprint('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_PRINT_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_PUNCT_LC(i)) != cBOOL(isPUNCT_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "ispunct('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_PUNCT_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_SPACE_LC(i)) != cBOOL(isSPACE_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isspace('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_SPACE_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_UPPER_LC(i)) != cBOOL(isUPPER_A(i))))  {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isupper('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_UPPER_LC(i))));
                }
                if (UNLIKELY(cBOOL(isU8_XDIGIT_LC(i))!= cBOOL(isXDIGIT_A(i)))) {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                                          "isxdigit('%s') unexpectedly is %x\n",
                                          name, cBOOL(isU8_XDIGIT_LC(i))));
                }
                if (UNLIKELY(toU8_LOWER_LC(i) != (int) toLOWER_A(i))) {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "tolower('%s')=0x%x instead of the expected 0x%x\n",
                            name, toU8_LOWER_LC(i), (int) toLOWER_A(i)));
                }
                if (UNLIKELY(toU8_UPPER_LC(i) != (int) toUPPER_A(i))) {
                    is_bad = TRUE;
                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "toupper('%s')=0x%x instead of the expected 0x%x\n",
                            name, toU8_UPPER_LC(i), (int) toUPPER_A(i)));
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

            /* The casts are because otherwise some compilers warn:
                gcc.gnu.org/bugzilla/show_bug.cgi?id=99950
                gcc.gnu.org/bugzilla/show_bug.cgi?id=94182
             */
            PL_fold_locale[ (U8) 'I' ] = 'I';
            PL_fold_locale[ (U8) 'i' ] = 'i';
            PL_in_utf8_turkic_locale = TRUE;
            DEBUG_L(PerlIO_printf(Perl_debug_log, "%s is turkic\n", newctype));
        }

        /* If we found problems and we want them output, do so */
        if (   (UNLIKELY(bad_count))
            && (LIKELY(ckWARN_d(WARN_LOCALE)) || UNLIKELY(DEBUG_L_TEST)))
        {
            /* WARNING.  If you change the wording of these; be sure to update
             * t/loc_tools.pl correspondingly */

            if (PL_warn_locale) {
                sv_catpvs(PL_warn_locale, "\n");
            }
            else {
                PL_warn_locale = newSVpvs("");
            }

            if (PL_in_utf8_CTYPE_locale) {
                Perl_sv_catpvf(aTHX_ PL_warn_locale,
                     "Locale '%s' contains (at least) the following characters"
                     " which have\nunexpected meanings: %s\nThe Perl program"
                     " will use the expected meanings",
                      newctype, bad_chars_list);
            }
            else {
                Perl_sv_catpvf(aTHX_ PL_warn_locale,
                                  "\nThe following characters (and maybe"
                                  " others) may not have the same meaning as"
                                  " the Perl program expects: %s\n",
                                  bad_chars_list
                            );
            }

#    if defined(HAS_SOME_LANGINFO) || defined(WIN32)

            Perl_sv_catpvf(aTHX_ PL_warn_locale, "; codeset=%s",
                                 langinfo_c(CODESET, LC_CTYPE, newctype, NULL));

#    endif

            Perl_sv_catpvf(aTHX_ PL_warn_locale, "\n");

            /* If we are actually in the scope of the locale or are debugging,
             * output the message now.  If not in that scope, we save the
             * message to be output at the first operation using this locale,
             * if that actually happens.  Most programs don't use locales, so
             * they are immune to bad ones.  */
            if (IN_LC(LC_CTYPE) || UNLIKELY(DEBUG_L_TEST)) {

                /* The '0' below suppresses a bogus gcc compiler warning */
                Perl_warner(aTHX_ packWARN(WARN_LOCALE), SvPVX(PL_warn_locale),
                                                                            0);
                if (IN_LC(LC_CTYPE)) {
                    SvREFCNT_dec_NN(PL_warn_locale);
                    PL_warn_locale = NULL;
                }
            }
        }
    }
}

void
Perl_warn_problematic_locale()
{
    dTHX;

    /* Core-only function that outputs the message in PL_warn_locale,
     * and then NULLS it.  Should be called only through the macro
     * CHECK_AND_WARN_PROBLEMATIC_LOCALE_ */

    if (PL_warn_locale) {
        Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE),
                             SvPVX(PL_warn_locale),
                             0 /* dummy to avoid compiler warning */ );
        SvREFCNT_dec_NN(PL_warn_locale);
        PL_warn_locale = NULL;
    }
}

#  endif /* USE_LOCALE_CTYPE */

STATIC void
S_new_LC_ALL(pTHX_ const char *lc_all, bool force)
{
    PERL_ARGS_ASSERT_NEW_LC_ALL;

    /* new_LC_ALL() updates all the things we care about.  Note that this is
     * called just after a change, so uses the actual underlying locale just
     * set, and not the nominal one (should they differ, as they may in
     * LC_NUMERIC). */

    const char * individ_locales[LC_ALL_INDEX_] = { NULL };

    switch (parse_LC_ALL_string(lc_all,
                                individ_locales,
                                override_if_ignored,   /* Override any ignored
                                                          categories */
                                true,   /* Always fill array */
                                true,   /* Panic if fails, as to get here it
                                           earlier had to have succeeded */
                                __LINE__))
    {
      case invalid:
      case no_array:
      case only_element_0:
        locale_panic_("Unexpected return from parse_LC_ALL_string");

      case full_array:
        break;
    }

    for_all_individual_category_indexes(i) {
        if (update_functions[i]) {
            const char * this_locale = individ_locales[i];
            update_functions[i](aTHX_ this_locale, force);
        }

        Safefree(individ_locales[i]);
    }
}

#  ifdef USE_LOCALE_COLLATE

STATIC void
S_new_collate(pTHX_ const char *newcoll, bool force)
{
    PERL_ARGS_ASSERT_NEW_COLLATE;
    PERL_UNUSED_ARG(force);

    /* Called after each libc setlocale() call affecting LC_COLLATE, to tell
     * core Perl this and that 'newcoll' is the name of the new locale.
     *
     * The design of locale collation is that every locale change is given an
     * index 'PL_collation_ix'.  The first time a string participates in an
     * operation that requires collation while locale collation is active, it
     * is given PERL_MAGIC_collxfrm magic (via sv_collxfrm_flags()).  That
     * magic includes the collation index, and the transformation of the string
     * by strxfrm(), q.v.  That transformation is used when doing comparisons,
     * instead of the string itself.  If a string changes, the magic is
     * cleared.  The next time the locale changes, the index is incremented,
     * and so we know during a comparison that the transformation is not
     * necessarily still valid, and so is recomputed.  Note that if the locale
     * changes enough times, the index could wrap, and it is possible that a
     * transformation would improperly be considered valid, leading to an
     * unlikely bug.  The value is declared to the widest possible type on this
     * platform. */

    /* Return if the locale isn't changing */
    if (strEQ(PL_collation_name, newcoll)) {
        return;
    }

    Safefree(PL_collation_name);
    PL_collation_name = savepv(newcoll);
    ++PL_collation_ix;

    /* Set the new one up if trivial.  Since this is called at process
     * initialization, be aware that this bit can't rely on much being
     * available. */
    PL_collation_standard = isNAME_C_OR_POSIX(newcoll);
    if (PL_collation_standard) {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                               "Setting PL_collation name='%s'\n",
                               PL_collation_name));
        PL_collxfrm_base = 0;
        PL_collxfrm_mult = 2;
        PL_in_utf8_COLLATE_locale = FALSE;
        PL_strxfrm_NUL_replacement = '\0';
        PL_strxfrm_max_cp = 0;
        return;
    }

    /* Flag that the remainder of the set up is being deferred until first
     * need. */
    PL_collxfrm_mult = 0;
    PL_collxfrm_base = 0;

}

#  endif /* USE_LOCALE_COLLATE */

#  ifdef WIN32

STATIC wchar_t *
S_Win_byte_string_to_wstring(const UINT code_page, const char * byte_string)
{
    /* Caller must arrange to free the returned string */

    int req_size = MultiByteToWideChar(code_page, 0, byte_string, -1, NULL, 0);
    if (! req_size) {
        SET_EINVAL;
        return NULL;
    }

    wchar_t *wstring;
    Newx(wstring, req_size, wchar_t);

    if (! MultiByteToWideChar(code_page, 0, byte_string, -1, wstring, req_size))
    {
        Safefree(wstring);
        SET_EINVAL;
        return NULL;
    }

    return wstring;
}

#    define Win_utf8_string_to_wstring(s)                                   \
                                    Win_byte_string_to_wstring(CP_UTF8, (s))

STATIC char *
S_Win_wstring_to_byte_string(const UINT code_page, const wchar_t * wstring)
{
    /* Caller must arrange to free the returned string */

    int req_size =
            WideCharToMultiByte(code_page, 0, wstring, -1, NULL, 0, NULL, NULL);

    char *byte_string;
    Newx(byte_string, req_size, char);

    if (! WideCharToMultiByte(code_page, 0, wstring, -1, byte_string,
                                                         req_size, NULL, NULL))
    {
        Safefree(byte_string);
        SET_EINVAL;
        return NULL;
    }

    return byte_string;
}

#    define Win_wstring_to_utf8_string(ws)                                  \
                                   Win_wstring_to_byte_string(CP_UTF8, (ws))

STATIC const char *
S_wrap_wsetlocale(pTHX_ const int category, const char *locale)
{
    PERL_ARGS_ASSERT_WRAP_WSETLOCALE;

    /* Calls _wsetlocale(), converting the parameters/return to/from
     * Perl-expected forms as if plain setlocale() were being called instead.
     *
     * Caller must arrange for the returned PV to be freed.
     */

    const wchar_t * wlocale = NULL;

    if (locale) {
        wlocale = Win_utf8_string_to_wstring(locale);
        if (! wlocale) {
            return NULL;
        }
    }

    WSETLOCALE_LOCK;
    const wchar_t * wresult = _wsetlocale(category, wlocale);

    if (! wresult) {
        WSETLOCALE_UNLOCK;
        Safefree(wlocale);
        return NULL;
    }

    const char * result = Win_wstring_to_utf8_string(wresult);
    WSETLOCALE_UNLOCK;

    Safefree(wlocale);
    return result;
}

STATIC const char *
S_win32_setlocale(pTHX_ int category, const char* locale)
{
    /* This, for Windows, emulates POSIX setlocale() behavior.  There is no
     * difference between the two unless the input locale is "", which normally
     * means on Windows to get the machine default, which is set via the
     * computer's "Regional and Language Options" (or its current equivalent).
     * In POSIX, it instead means to find the locale from the user's
     * environment.  This routine changes the Windows behavior to try the POSIX
     * behavior first.  Further details are in the called function
     * find_locale_from_environment().
     */

    if (locale != NULL && strEQ(locale, "")) {
        /* Note this function may change the locale, but that's ok because we
         * are about to change it anyway */
        locale = find_locale_from_environment(get_category_index(category));
        if (locale == NULL) {
            SET_EINVAL;
            return NULL;
        }
    }

    const char * result = wrap_wsetlocale(category, locale);
    DEBUG_L(PerlIO_printf(Perl_debug_log, "%s\n",
                          setlocale_debug_string_r(category, locale, result)));

    if (! result) {
        SET_EINVAL;
        return NULL;
    }

    save_to_buffer(result, &PL_setlocale_buf, &PL_setlocale_bufsize);

#    ifndef USE_PL_CUR_LC_ALL

    Safefree(result);

#  else

    /* Here, we need to keep track of LC_ALL, so store the new value.  but if
     * the input locale is NULL, we were just querying, so the original value
     * hasn't changed */
    if (locale == NULL) {
        Safefree(result);
    }
    else {

        /* If we set LC_ALL directly above, we already know its new value; but
         * if we changed just an individual category, find the new LC_ALL */
        if (category != LC_ALL) {
            Safefree(result);
            result = wrap_wsetlocale(LC_ALL, NULL);
        }

        Safefree(PL_cur_LC_ALL);
        PL_cur_LC_ALL = result;
    }

    DEBUG_L(PerlIO_printf(Perl_debug_log, "new PL_cur_LC_ALL=%s\n",
                                          PL_cur_LC_ALL));
#    endif

    return PL_setlocale_buf;
}

#  endif

STATIC const char *
S_native_querylocale_i(pTHX_ const locale_category_index cat_index)
{
    /* Determine the current locale and return it in the form the platform's
     * native locale handling understands.  This is different only from our
     * internal form for the LC_ALL category, as platforms differ in how they
     * represent that.
     *
     * This is only called from Perl_setlocale().  As such it returns in
     * PL_setlocale_buf */

#  ifdef USE_LOCALE_NUMERIC

    /* We have the LC_NUMERIC name saved, because we are normally switched into
     * the C locale (or equivalent) for it. */
    if (cat_index == LC_NUMERIC_INDEX_) {

        /* We don't have to copy this return value, as it is a per-thread
         * variable, and won't change until a future setlocale */
        return PL_numeric_name;
    }

#  endif
#  ifdef LC_ALL

    if (cat_index != LC_ALL_INDEX_)

#  endif

    {
        /* Here, not LC_ALL, and not LC_NUMERIC: the actual and native values
         * match */

#  ifdef setlocale_i    /* Can shortcut if this is defined */

        return setlocale_i(cat_index, NULL);

#  else

        return save_to_buffer(querylocale_i(cat_index),
                              &PL_setlocale_buf, &PL_setlocale_bufsize);
#  endif

    }

    /* Below, querying LC_ALL */

#  ifdef LC_ALL
#    ifdef USE_PL_CURLOCALES
#      define LC_ALL_ARG  PL_curlocales
#    else
#      define LC_ALL_ARG  NULL  /* Causes calculate_LC_ALL_string() to find the
                                   locale using a querylocale function */
#    endif

    return calculate_LC_ALL_string(LC_ALL_ARG, EXTERNAL_FORMAT_FOR_QUERY,
                                   WANT_PL_setlocale_buf,
                                   __LINE__);
#    undef LC_ALL_ARG
#  endif    /* has LC_ALL */

}

#endif      /* USE_LOCALE */

/*
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

Changing the locale is not a good idea when more than one thread is running,
except on systems where the predefined variable C<${^SAFE_LOCALES}> is
non-zero.  This is because on such systems the locale is global to the whole
process and not local to just the thread calling the function.  So changing it
in one thread instantaneously changes it in all.  On some such systems, the
system C<setlocale()> is ineffective, returning the wrong information, and
failing to actually change the locale.  z/OS refuses to try to change the
locale once a second thread is created.  C<Perl_setlocale>, should give you
accurate results of what actually happened on these problematic platforms,
returning NULL if the system forbade the locale change.

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

    dTHX;

    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "Entering Perl_setlocale(%d, \"%s\")\n",
                          category, locale));

    bool valid_category;
    locale_category_index cat_index = get_category_index_helper(category,
                                                                &valid_category,
                                                                __LINE__);
    if (! valid_category) {
        if (ckWARN(WARN_LOCALE)) {
            const char * conditional_warn_text;
            if (locale == NULL) {
                conditional_warn_text = "";
                locale = "";
            }
            else {
                conditional_warn_text = "; can't set it to ";
            }

            /* diag_listed_as: Unknown locale category %d; can't set it to %s */
            Perl_warner(aTHX_
                           packWARN(WARN_LOCALE),
                           "Unknown locale category %d%s%s",
                           category, conditional_warn_text, locale);
        }

        SET_EINVAL;
        return NULL;
    }

#  ifdef setlocale_i

    /* setlocale_i() gets defined only on Configurations that use setlocale()
     * in a simple manner that adequately handles all cases.  If this category
     * doesn't have any perl complications, just do that. */
    if (! update_functions[cat_index]) {
        return setlocale_i(cat_index, locale);
    }

#  endif

    /* Get current locale */
    const char * current_locale = native_querylocale_i(cat_index);

    /* A NULL locale means only query what the current one is. */
    if (locale == NULL) {
        return current_locale;
    }

    if (strEQ(current_locale, locale)) {
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                             "Already in requested locale: no action taken\n"));
        return current_locale;
    }

    /* Here, an actual change is being requested.  Do it */
    if (! bool_setlocale_i(cat_index, locale)) {
        DEBUG_L(PerlIO_printf(Perl_debug_log, "%s\n",
                          setlocale_debug_string_i(cat_index, locale, "NULL")));
        return NULL;
    }

    /* At this point, the locale has been changed based on the requested value,
     * and the querylocale_i() will return the actual new value that the system
     * has for the category.  That may not be the same as the input, as libc
     * may have returned a synonymous locale name instead of the input one; or,
     * if there are locale categories that we are compiled to ignore, any
     * attempt to change them away from "C" is overruled */
    current_locale = querylocale_i(cat_index);

    /* But certain categories need further work.  For example we may need to
     * calculate new folding or collation rules.  And for LC_NUMERIC, we have
     * to switch into a locale that has a dot radix. */
    if (update_functions[cat_index]) {
        update_functions[cat_index](aTHX_ current_locale,
                                          /* No need to force recalculation, as
                                           * aren't coming from a situation
                                           * where Perl hasn't been controlling
                                           * the locale, so has accurate
                                           * records. */
                                          false);
    }

    /* Make sure the result is in a stable buffer for the caller's use, and is
     * in the expected format */
    current_locale = native_querylocale_i(cat_index);

    DEBUG_L(PerlIO_printf(Perl_debug_log, "returning '%s'\n", current_locale));

    return current_locale;

#endif

}

#ifdef USE_LOCALE
#  ifdef DEBUGGING

STATIC char *
S_my_setlocale_debug_string_i(pTHX_
                              const locale_category_index cat_index,
                              const char* locale, /* Optional locale name */

                              /* return value from setlocale() when attempting
                               * to set 'category' to 'locale' */
                              const char* retval,

                              const line_t line)
{
    /* Returns a pointer to a NUL-terminated string in static storage with
     * added text about the info passed in.  This is not thread safe and will
     * be overwritten by the next call, so this should be used just to
     * formulate a string to immediately print or savepv() on. */

    const char * locale_quote;
    const char * retval_quote;

    if (locale == NULL) {
        locale_quote = "";
        locale = "NULL";
    }
    else {
        locale_quote = "\"";
    }

    if (retval == NULL) {
        retval_quote = "";
        retval = "NULL";
    }
    else {
        retval_quote = "\"";
    }

#  ifdef MULTIPLICITY
#    define THREAD_FORMAT "%p:"
#    define THREAD_ARGUMENT aTHX_
#  else
#    define THREAD_FORMAT
#    define THREAD_ARGUMENT
#  endif

    return Perl_form(aTHX_
                     "%s:%" LINE_Tf ": " THREAD_FORMAT
                     " setlocale(%s[%d], %s%s%s) returned %s%s%s\n",

                     __FILE__, line, THREAD_ARGUMENT
                     category_names[cat_index], categories[cat_index],
                     locale_quote, locale, locale_quote,
                     retval_quote, retval, retval_quote);
}

#  endif

/* If this implementation hasn't defined these macros, they aren't needed */
#  ifndef TOGGLE_LOCK
#    define TOGGLE_LOCK(i)
#    define TOGGLE_UNLOCK(i)
#  endif

STATIC const char *
S_toggle_locale_i(pTHX_ const locale_category_index cat_index,
                        const char * new_locale,
                        const line_t caller_line)
{
    PERL_ARGS_ASSERT_TOGGLE_LOCALE_I;

    /* Changes the locale for the category specified by 'index' to 'new_locale,
     * if they aren't already the same.  EVERY CALL to this function MUST HAVE
     * a corresponding call to restore_toggled_locale_i()
     *
     * Returns a copy of the name of the original locale for 'cat_index'
     * so can be switched back to with the companion function
     * restore_toggled_locale_i(),  (NULL if no restoral is necessary.) */

    /* Find the original locale of the category we may need to change, so that
     * it can be restored to later */
    const char * locale_to_restore_to = querylocale_i(cat_index);

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "Entering toggle_locale_i: index=%d(%s),"        \
                           " wanted=%s, actual=%s; called from %" LINE_Tf   \
                           "\n", cat_index, category_names[cat_index],
                           new_locale, locale_to_restore_to ? locale_to_restore_to : "(null)",
                           caller_line));

    if (! locale_to_restore_to) {
        locale_panic_via_(Perl_form(aTHX_
                                    "Could not find current %s locale",
                                    category_names[cat_index]),
                         __FILE__, caller_line);
    }

    /* Begin a critical section on platforms that need it.  We do this even if
     * we don't have to change here, so as to prevent other instances from
     * changing the locale out from under us. */
    TOGGLE_LOCK(cat_index);

    /* If the locales are the same, there's nothing to do */
    if (strEQ(locale_to_restore_to, new_locale)) {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log, "%s locale unchanged as %s\n",
                                               category_names[cat_index],
                                               new_locale));
        return NULL;
    }

    /* Finally, change the locale to the new one */
    void_setlocale_i_with_caller(cat_index, new_locale, __FILE__, caller_line);

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "%s locale switched to %s\n",
                           category_names[cat_index], new_locale));

    return locale_to_restore_to;

#  ifndef DEBUGGING
    PERL_UNUSED_ARG(caller_line);
#  endif

}

STATIC void
S_restore_toggled_locale_i(pTHX_ const locale_category_index cat_index,
                                 const char * restore_locale,
                                 const line_t caller_line)
{
    /* Restores the locale for LC_category corresponding to cat_index to
     * 'restore_locale' (which is a copy that will be freed by this function),
     * or do nothing if the latter parameter is NULL */

    PERL_ARGS_ASSERT_RESTORE_TOGGLED_LOCALE_I;

    if (restore_locale == NULL) {
        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                               "restore_toggled_locale_i: No need to"       \
                               " restore %s; called from %" LINE_Tf "\n",   \
                               category_names[cat_index], caller_line));
        TOGGLE_UNLOCK(cat_index);
        return;
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "restore_toggled_locale_i: restoring locale for" \
                           " %s to  %s; called from %" LINE_Tf "\n",        \
                           category_names[cat_index], restore_locale,
                           caller_line));

    void_setlocale_i_with_caller(cat_index, restore_locale,
                                  __FILE__, caller_line);
    TOGGLE_UNLOCK(cat_index);

#  ifndef DEBUGGING
    PERL_UNUSED_ARG(caller_line);
#  endif

}

#endif
#if defined(USE_LOCALE) || defined(HAS_SOME_LANGINFO) || defined(HAS_LOCALECONV)

STATIC utf8ness_t
S_get_locale_string_utf8ness_i(pTHX_ const char * string,
                                     const locale_utf8ness_t known_utf8,
                                     const char * locale,
                                     const locale_category_index cat_index)
{
    PERL_ARGS_ASSERT_GET_LOCALE_STRING_UTF8NESS_I;

#  ifndef USE_LOCALE

    return UTF8NESS_NO;
    PERL_UNUSED_ARG(string);
    PERL_UNUSED_ARG(known_utf8);
    PERL_UNUSED_ARG(locale);
    PERL_UNUSED_ARG(cat_index);

#  else

    /* Return to indicate if 'string' in the locale given by the input
     * arguments should be considered UTF-8 or not.
     *
     * If the input 'locale' is not NULL, use that for the locale; otherwise
     * use the current locale for the category specified by 'cat_index'.
     */

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "Entering get_locale_string_utf8ness_i; locale=%s,"
                           " index=%u(%s), string=%s, known_utf8=%d\n",
                           locale, cat_index, category_names[cat_index],
                           ((string)
                            ?  _byte_dump_string((U8 *) string,
                                                 strlen(string),
                                                 0)
                            : "nil"),
                           known_utf8));
    if (string == NULL) {
        return UTF8NESS_IMMATERIAL;
    }

    if (IN_BYTES) { /* respect 'use bytes' */
        return UTF8NESS_NO;
    }

    Size_t len = strlen(string);

    /* UTF8ness is immaterial if the representation doesn't vary */
    const U8 * first_variant = NULL;
    if (is_utf8_invariant_string_loc((U8 *) string, len, &first_variant)) {
        return UTF8NESS_IMMATERIAL;
    }

    /* Can't be UTF-8 if invalid */
    if (! is_strict_utf8_string((U8 *) first_variant,
                                len - ((char *) first_variant - string)))
    {
        return UTF8NESS_NO;
    }

    /* Here and below, we know the string is legal UTF-8, containing at least
     * one character requiring a sequence of two or more bytes.  It is quite
     * likely to be UTF-8.  But it pays to be paranoid and do further checking.
     *
     * If we already know the UTF-8ness of the locale, then we immediately know
     * what the string is */
    if (UNLIKELY(known_utf8 != LOCALE_UTF8NESS_UNKNOWN)) {
        return (known_utf8 == LOCALE_IS_UTF8) ? UTF8NESS_YES : UTF8NESS_NO;
    }

    if (locale == NULL) {
        locale = querylocale_i(cat_index);
    }

    /* If the locale is UTF-8, the string is UTF-8;  otherwise it was
     * coincidental that the string is legal UTF-8
     *
     * However, if the perl is compiled to not pay attention to the category
     * being passed in, you might think that that locale is essentially always
     * the C locale, so it would make sense to say it isn't UTF-8.  But to get
     * here, the string has to contain characters unknown in the C locale.  And
     * in fact, Windows boxes are compiled without LC_MESSAGES, as their
     * message catalog isn't really a part of the locale system.  But those
     * messages really could be UTF-8, and given that the odds are rather small
     * of something not being UTF-8 but being syntactically valid UTF-8, khw
     * has decided to call such strings as UTF-8. */
    return (is_locale_utf8(locale)) ? UTF8NESS_YES : UTF8NESS_NO;

#  endif

}

STATIC bool
S_is_locale_utf8(pTHX_ const char * locale)
{
    PERL_ARGS_ASSERT_IS_LOCALE_UTF8;

    /* Returns TRUE if the locale 'locale' is UTF-8; FALSE otherwise. */

#  if ! defined(USE_LOCALE)                                                   \
   || ! defined(USE_LOCALE_CTYPE)                                             \
   ||   defined(EBCDIC) /* There aren't any real UTF-8 locales at this time */

    PERL_UNUSED_ARG(locale);

    return FALSE;

     /* Definitively, can't be UTF-8 */
#    define HAS_DEFINITIVE_UTF8NESS_DETERMINATION
#  else

    /* If the input happens to be the same locale as we are currently setup
     * for, the answer has already been cached. */
    if (strEQ(locale, PL_ctype_name)) {
        return PL_in_utf8_CTYPE_locale;
    }

    if (isNAME_C_OR_POSIX(locale)) {
        return false;
    }

#    if ! defined(HAS_SOME_LANGINFO) && ! defined(WIN32)

    /* On non-Windows without nl_langinfo(), we have to do some digging to get
     * the answer.  First, toggle to the desired locale so can query its state
     * */
    const char * orig_CTYPE_locale = toggle_locale_c(LC_CTYPE, locale);

#      define TEARDOWN_FOR_IS_LOCALE_UTF8                                   \
                      restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale)

#      ifdef MB_CUR_MAX

    /* If there are fewer bytes available in this locale than are required
     * to represent the largest legal UTF-8 code point, this isn't a UTF-8
     * locale. */
    const int mb_cur_max = MB_CUR_MAX;
    if (mb_cur_max < (int) UNISKIP(PERL_UNICODE_MAX)) {
        TEARDOWN_FOR_IS_LOCALE_UTF8;
        return false;
    }

#      endif
#      if defined(HAS_MBTOWC) || defined(HAS_MBRTOWC)

         /* With these functions, we can definitively determine a locale's
          * UTF-8ness */
#        define HAS_DEFINITIVE_UTF8NESS_DETERMINATION

    /* If libc mbtowc() evaluates the bytes that form the REPLACEMENT CHARACTER
     * as that Unicode code point, this has to be a UTF-8 locale; otherwise it
     * can't be  */
    wchar_t wc = 0;
    (void) Perl_mbtowc_(aTHX_ NULL, NULL, 0);/* Reset shift state */
    int mbtowc_ret = Perl_mbtowc_(aTHX_ &wc,
                                  STR_WITH_LEN(REPLACEMENT_CHARACTER_UTF8));
    TEARDOWN_FOR_IS_LOCALE_UTF8;
    return (   mbtowc_ret == STRLENs(REPLACEMENT_CHARACTER_UTF8)
            && wc == UNICODE_REPLACEMENT);

#      else

        /* If the above two C99 functions aren't working, you could try some
         * different methods.  It seems likely that the obvious choices,
         * wctomb() and wcrtomb(), wouldn't be working either.  But you could
         * choose one of the dozen-ish Unicode titlecase triples and verify
         * that towupper/towlower work as expected.
         *
         * But, our emulation of nl_langinfo() works quite well, so avoid the
         * extra code until forced to by some weird non-conforming platform. */
#        define USE_LANGINFO_FOR_UTF8NESS
#        undef HAS_DEFINITIVE_UTF8NESS_DETERMINATION
#      endif
#    else

     /* On Windows or on platforms with nl_langinfo(), there is a direct way to
      * get the locale's codeset, which will be some form of 'UTF-8' for a
      * UTF-8 locale.  langinfo_c() handles this, and we will call that
      * below */
#      define HAS_DEFINITIVE_UTF8NESS_DETERMINATION
#      define USE_LANGINFO_FOR_UTF8NESS
#      define TEARDOWN_FOR_IS_LOCALE_UTF8
#    endif  /* USE_LANGINFO_FOR_UTF8NESS */

     /* If the above compiled into code, it found the locale's UTF-8ness,
      * nothing more to do; if it didn't get compiled,
      * USE_LANGINFO_FOR_UTF8NESS is defined.  There are two possible reasons:
      *   1)  it is the preferred method because it knows directly for sure
      *       what the codeset is because the platform has libc functions that
      *       return this; or
      *   2)  the functions the above code section would compile to use don't
      *       exist or are unreliable on this platform; we are less sure of the
      *       langinfo_c() result, though it is very unlikely to be wrong
      *       about if it is UTF-8 or not */
#    ifdef USE_LANGINFO_FOR_UTF8NESS

    const char * codeset = langinfo_c(CODESET, LC_CTYPE, locale, NULL);
    bool retval = is_codeset_name_UTF8(codeset);

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "found codeset=%s, is_utf8=%d\n", codeset, retval));
    DEBUG_Lv(PerlIO_printf(Perl_debug_log, "is_locale_utf8(%s) returning %d\n",
                                                            locale, retval));
    TEARDOWN_FOR_IS_LOCALE_UTF8;
    return retval;

#    endif
#  endif      /* End of the #else clause, for the non-trivial case */

}

#endif

#ifdef USE_LOCALE
#  ifdef USE_LOCALE_CTYPE

STATIC bool
S_is_codeset_name_UTF8(const char * name)
{
    /* Return a boolean as to if the passed-in name indicates it is a UTF-8
     * code set.  Several variants are possible */
    const Size_t len = strlen(name);

    PERL_ARGS_ASSERT_IS_CODESET_NAME_UTF8;

#    ifdef WIN32

    /* https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers */
    if (memENDs(name, len, "65001")) {
        return TRUE;
    }

#    endif
               /* 'UTF8' or 'UTF-8' */
    return (    inRANGE(len, 4, 5)
            &&  name[len-1] == '8'
            && (   memBEGINs(name, len, "UTF")
                || memBEGINs(name, len, "utf"))
            && (len == 4 || name[3] == '-'));
}

#  endif
#  ifdef WIN32

bool
Perl_get_win32_message_utf8ness(pTHX_ const char * string)
{
    /* This is because Windows doesn't have LC_MESSAGES. */

#    ifdef USE_LOCALE_CTYPE

    /* We don't know the locale utf8ness here, and not even the locale itself.
     * Since Windows uses a different mechanism to specify message language
     * output than the locale system, it is going to be problematic deciding
     * if we are to store it as UTF-8 or not.  By specifying LOCALE_IS_UTF8, we
     * are telling the called function to return true iff the string has
     * non-ASCII characters in it that are all syntactically UTF-8.  We are
     * thus relying on the fact that a string that is syntactically valid UTF-8
     * is likely to be UTF-8.  Should this ever cause problems, this function
     * could be replaced by something more Windows-specific */
    return get_locale_string_utf8ness_i(string, LOCALE_IS_UTF8,
                                        NULL, LC_CTYPE_INDEX_);
#    else

    PERL_UNUSED_ARG(string);
    return false;

#    endif

}

#  endif

STATIC void
S_set_save_buffer_min_size(pTHX_ Size_t min_len,
                                 char **buf,
                                 Size_t * buf_cursize)
{
    /* Make sure the buffer pointed to by *buf is at least as large 'min_len';
     * *buf_cursize is the size of 'buf' upon entry; it will be updated to the
     * new size on exit.  'buf_cursize' being NULL is to be used when this is a
     * single use buffer, which will shortly be freed by the caller. */

    if (buf_cursize == NULL) {
        Newx(*buf, min_len, char);
    }
    else if (*buf_cursize == 0) {
        Newx(*buf, min_len, char);
        *buf_cursize = min_len;
    }
    else if (min_len > *buf_cursize) {
        Renew(*buf, min_len, char);
        *buf_cursize = min_len;
    }
}

STATIC const char *
S_save_to_buffer(pTHX_ const char * string, char **buf, Size_t *buf_size)
{
    PERL_ARGS_ASSERT_SAVE_TO_BUFFER;

    /* Copy the NUL-terminated 'string' to a buffer whose address before this
     * call began at *buf, and whose available length before this call was
     * *buf_size.
     *
     * If the length of 'string' is greater than the space available, the
     * buffer is grown accordingly, which may mean that it gets relocated.
     * *buf and *buf_size will be updated to reflect this.
     *
     * Regardless, the function returns a pointer to where 'string' is now
     * stored.
     *
     * 'string' may be NULL, which means no action gets taken, and NULL is
     * returned.
     *
     * 'buf_size' being NULL is to be used when this is a single use buffer,
     * which will shortly be freed by the caller.
     *
     * If *buf or 'buf_size' are NULL or *buf_size is 0, the buffer is assumed
     * empty, and memory is malloc'd.
     */

    if (! string) {
        return NULL;
    }

    /* No-op to copy over oneself */
    if (string == *buf) {
        return string;
    }

    Size_t string_size = strlen(string) + 1;
    set_save_buffer_min_size(string_size, buf, buf_size);

#  ifdef DEBUGGING

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                         "Copying '%s' to %p\n",
                         ((is_strict_utf8_string((U8 *) string, 0))
                          ? string
                          :_byte_dump_string((U8 *) string, strlen(string), 0)),
                          *buf));

#    ifdef USE_LOCALE_CTYPE

    /* Catch glitches.  Usually this is because LC_CTYPE needs to be the same
     * locale as whatever is being worked on */
    if (UNLIKELY(instr(string, REPLACEMENT_CHARACTER_UTF8))) {
        locale_panic_(Perl_form(aTHX_
                                "Unexpected REPLACEMENT_CHARACTER in '%s'\n%s",
                                string, get_LC_ALL_display()));
    }

#    endif
#  endif

    Copy(string, *buf, string_size, char);
    return *buf;
}

#endif

int
Perl_mbtowc_(pTHX_ const wchar_t * pwc, const char * s, const Size_t len)
{

#if ! defined(HAS_MBRTOWC) && ! defined(HAS_MBTOWC)

    PERL_UNUSED_ARG(pwc);
    PERL_UNUSED_ARG(s);
    PERL_UNUSED_ARG(len);
    return -1;

#else   /* Below we have some form of mbtowc() */
#  if defined(HAS_MBRTOWC)                                      \
   && (defined(MULTIPLICITY) || ! defined(HAS_MBTOWC))
#    define USE_MBRTOWC
#  else
#    undef USE_MBRTOWC
#  endif

    CHECK_AND_WARN_PROBLEMATIC_LOCALE_;
    int retval = -1;

    if (s == NULL) { /* Initialize the shift state to all zeros in
                        PL_mbrtowc_ps. */

#  if defined(USE_MBRTOWC)

        memzero(&PL_mbrtowc_ps, sizeof(PL_mbrtowc_ps));
        return 0;

#  else

        SETERRNO(0, 0);
        MBTOWC_LOCK_;
        retval = mbtowc(NULL, NULL, 0);
        MBTOWC_UNLOCK_;
        return retval;

#  endif

    }

#  if defined(USE_MBRTOWC)

    SETERRNO(0, 0);
    MBRTOWC_LOCK_;
    retval = (SSize_t) mbrtowc((wchar_t *) pwc, s, len, &PL_mbrtowc_ps);
    MBRTOWC_UNLOCK_;

#  else

    /* Locking prevents races, but locales can be switched out without locking,
     * so this isn't a cure all */
    SETERRNO(0, 0);
    MBTOWC_LOCK_;
    retval = mbtowc((wchar_t *) pwc, s, len);
    MBTOWC_UNLOCK_;

#  endif

    return retval;

#endif

}

/*
=for apidoc Perl_localeconv

This is a thread-safe version of the libc L<localeconv(3)>.  It is the same as
L<POSIX::localeconv|POSIX/localeconv> (returning a hash of the C<localeconv()>
fields), but directly callable from XS code.  The hash is mortalized, so must
be dealt with immediately.

=cut
*/

/* All Wndows versions we support, except possibly MingW, have general
 * thread-safety, and even localeconv() is thread safe, returning into a
 * per-thread buffer.  MingW when built with a modern MS C runtime (UCRT as of
 * this writing), also has those things.
 *
 * FreeBSD's localeconv() when used with uselocale() is supposed to be
 * thread-safe (as is their localeconv_l()), but we currently don't use
 * thread-safe locales there because of bugs. There may be other thread-safe
 * localeconv() implementations, especially on *BSD derivatives, but khw knows
 * of none, and hasn't really investigated, in part because of the past
 * unreliability of vendor thread-safety claims */
#if defined(WIN32) && (defined(_MSC_VER) || (defined(_UCRT)))
#  define LOCALECONV_IS_THREAD_SAFE
#endif

/* When multiple threads can be going at once, we need a critical section
 * around doing the localeconv() and saving its return, unless localeconv() is
 * thread-safe, and we are using it in a thread-safe manner, which we are only
 * doing if safe threads are available and we don't have a broken localeconv()
 * */
#if  defined(USE_THREADS)                               \
 && (   ! defined(LOCALECONV_IS_THREAD_SAFE)            \
     || ! defined(USE_THREAD_SAFE_LOCALE)               \
     ||   defined(TS_W32_BROKEN_LOCALECONV))
#  define LOCALECONV_NEEDS_CRITICAL_SECTION
#endif

HV *
Perl_localeconv(pTHX)
{
    return (HV *) sv_2mortal((SV *) my_localeconv(0));
}

HV *
S_my_localeconv(pTHX_ const int item)
{
    PERL_ARGS_ASSERT_MY_LOCALECONV;

    /* This returns a mortalized hash containing all or certain elements
     * returned by localeconv(). */
    HV * hv = newHV();      /* The returned hash, initially empty */

    /* The function is used by Perl_localeconv() and POSIX::localeconv(), or
     * internally from this file, and is thread-safe.
     *
     * localeconv() returns items from two different locale categories,
     * LC_MONETARY and LC_NUMERIC.  Various data structures in this function
     * are arrays with two elements, one for each category, and these indexes
     * indicate which array element applies to which category */
#define NUMERIC_OFFSET   0
#define MONETARY_OFFSET  1

    /* Some operations apply to one or the other category, or both.  A mask
     * is used to specify all the possibilities.  This macro converts from the
     * category offset to its bit position in the mask. */
#define OFFSET_TO_BIT(i)  (1 << (i))

    /* There are two use cases for this function:
     * 1) Called as Perl_localeconv(), or from POSIX::locale_conv().  This
     *    returns the lconv structure copied to a hash, based on the current
     *    underlying locales for LC_NUMERIC and LC_MONETARY. An input item==0
     *    signifies this case, or on many platforms it is the only use case
     *    compiled.
     * 2) Certain items that nl_langinfo() provides are also derivable from
     *    the return of localeconv().  Windows notably doesn't have
     *    nl_langinfo(), so on that, and actually any platform lacking it,
     *    my_localeconv() is used also to emulate it for those particular
     *    items.  The code to do this is compiled only on such platforms.
     *    Rather than going to the expense of creating a full hash when only
     *    one item is needed, the returned hash has just the desired item in
     *    it.
     *
     * To access all the localeconv() struct lconv fields, there is a data
     * structure that contains every commonly documented field in it.  (Maybe
     * some minority platforms have extra fields.  Those could be added here
     * without harm; they would just be ignored on platforms lacking them.)
     *
     * Our structure is compiled to make looping through the fields easier by
     * pointing each name to its value's offset within lconv, e.g.,
        { "thousands_sep", STRUCT_OFFSET(struct lconv, thousands_sep) }
     */
#define LCONV_ENTRY(name) {STRINGIFY(name), STRUCT_OFFSET(struct lconv, name)}

    /* These synonyms are just for clarity, and to make it easier in case
     * something needs to change in the future */
#define LCONV_NUMERIC_ENTRY(name)  LCONV_ENTRY(name)
#define LCONV_MONETARY_ENTRY(name) LCONV_ENTRY(name)

    /* There are just a few fields for NUMERIC strings */
    const lconv_offset_t lconv_numeric_strings[] = {
#ifndef NO_LOCALECONV_GROUPING
        LCONV_NUMERIC_ENTRY(grouping),
# endif
        LCONV_NUMERIC_ENTRY(thousands_sep),
# define THOUSANDS_SEP_LITERAL  "thousands_sep"
        LCONV_NUMERIC_ENTRY(decimal_point),
# define DECIMAL_POINT_LITERAL "decimal_point"
        {NULL, 0}
    };

    /* When used to implement nl_langinfo(), we save time by only populating
     * the hash with the field(s) needed.  Thus we would need a data structure
     * of just:
     *  LCONV_NUMERIC_ENTRY(decimal_point),
     *  {NULL, 0}
     *
     * By placing the decimal_point field last in the full structure, we can
     * use just the tail for this bit of it, saving space.  This macro yields
     * the address of the sub structure. */
#define DECIMAL_POINT_ADDRESS                                             \
        &lconv_numeric_strings[(C_ARRAY_LENGTH(lconv_numeric_strings) - 2)]

    /* And the MONETARY string fields */
    const lconv_offset_t lconv_monetary_strings[] = {
        LCONV_MONETARY_ENTRY(int_curr_symbol),
        LCONV_MONETARY_ENTRY(mon_decimal_point),
#ifndef NO_LOCALECONV_MON_THOUSANDS_SEP
        LCONV_MONETARY_ENTRY(mon_thousands_sep),
#endif
#ifndef NO_LOCALECONV_MON_GROUPING
        LCONV_MONETARY_ENTRY(mon_grouping),
#endif
        LCONV_MONETARY_ENTRY(positive_sign),
        LCONV_MONETARY_ENTRY(negative_sign),
        LCONV_MONETARY_ENTRY(currency_symbol),
#define CURRENCY_SYMBOL_LITERAL  "currency_symbol"
        {NULL, 0}
    };

    /* Like above, this field being last can be used as a sub structure */
#define CURRENCY_SYMBOL_ADDRESS                                            \
      &lconv_monetary_strings[(C_ARRAY_LENGTH(lconv_monetary_strings) - 2)]

    /* Finally there are integer fields, all are for monetary purposes */
    const lconv_offset_t lconv_integers[] = {
        LCONV_ENTRY(int_frac_digits),
        LCONV_ENTRY(frac_digits),
        LCONV_ENTRY(p_sep_by_space),
        LCONV_ENTRY(n_cs_precedes),
        LCONV_ENTRY(n_sep_by_space),
        LCONV_ENTRY(p_sign_posn),
        LCONV_ENTRY(n_sign_posn),
#ifdef HAS_LC_MONETARY_2008
        LCONV_ENTRY(int_p_cs_precedes),
        LCONV_ENTRY(int_p_sep_by_space),
        LCONV_ENTRY(int_n_cs_precedes),
        LCONV_ENTRY(int_n_sep_by_space),
        LCONV_ENTRY(int_p_sign_posn),
        LCONV_ENTRY(int_n_sign_posn),
#endif
#    define P_CS_PRECEDES_LITERAL    "p_cs_precedes"
        LCONV_ENTRY(p_cs_precedes),
        {NULL, 0}
    };

    /* Like above, this field being last can be used as a sub structure */
#define P_CS_PRECEDES_ADDRESS                                       \
      &lconv_integers[(C_ARRAY_LENGTH(lconv_integers) - 2)]

    /* The actual populating of the hash is done by two sub functions that get
     * passed an array of length two containing the data structure they are
     * supposed to use to get the key names to fill the hash with.  One element
     * is always for the NUMERIC strings (or NULL if none to use), and the
     * other element similarly for the MONETARY ones. */
    const lconv_offset_t * strings[2] = { lconv_numeric_strings,
                                          lconv_monetary_strings
                                        };

    /* The LC_MONETARY category also has some integer-valued fields, whose
     * information is kept in a separate parallel array to 'strings' */
    const lconv_offset_t * integers[2] = {
                                           NULL,
                                           lconv_integers
                                         };

#if  ! defined(HAS_LOCALECONV)                                          \
 || (! defined(USE_LOCALE_NUMERIC) && ! defined(USE_LOCALE_MONETARY))

    /* If both NUMERIC and MONETARY must be the "C" locale, simply populate the
     * hash using the function that works on just that locale. */
    populate_hash_from_C_localeconv(hv,
                                    "C",
                                    (  OFFSET_TO_BIT(NUMERIC_OFFSET)
                                     | OFFSET_TO_BIT(MONETARY_OFFSET)),
                                     strings, integers);

    /* We shouldn't get to here for the case of an individual item, as
     * preprocessor directives elsewhere in this file should have filled in the
     * correct values at a higher level */
    assert(item == 0);
    PERL_UNUSED_ARG(item);

    return hv;

#else

    /* From here to the end of this function, at least one of NUMERIC or
     * MONETARY can be non-C */

    /* This is a mask, with one bit to tell the populate functions to populate
     * the NUMERIC items; another bit for the MONETARY ones.  This way they can
     * choose which (or both) to populate from */
    U32 index_bits = 0;

    /* Some platforms, for correct non-mojibake results, require LC_CTYPE's
     * locale to match LC_NUMERIC's for the numeric fields, and LC_MONETARY's
     * for the monetary ones.  What happens if LC_NUMERIC and LC_MONETARY
     * aren't compatible?  Wrong results.  To avoid that, we call localeconv()
     * twice, once for each locale, setting LC_CTYPE to match the category.
     * But if the locales of both categories are the same, there is no need for
     * a second call.  Assume this is the case unless overridden below */
    bool requires_2nd_localeconv = false;

    /* The actual hash populating is done by one of the two populate functions.
     * Which one is appropriate for either the MONETARY_OFFSET or the
     * NUMERIC_OFFSET is calculated and then stored in this table */
    void (*populate[2]) (pTHX_
                         HV * ,
                         const char *,
                         const U32,
                         const lconv_offset_t **,
                         const lconv_offset_t **);

    /* This gives the locale to use for the corresponding OFFSET, like the
     * 'populate' array above */
    const char * locales[2];

#  ifdef HAS_SOME_LANGINFO

    /* If the only use-case for this is the full localeconv(), the 'item'
     * parameter is ignored. */
    PERL_UNUSED_ARG(item);

#  else     /* This only gets compiled for the use-case of using localeconv()
               to emulate nl_langinfo() when missing from the platform. */

#    ifdef USE_LOCALE_NUMERIC

    /* We need this substructure to only return this field for the THOUSEP
     * item.  The other items also need substructures, but they were handled
     * above by placing the substructure's item at the end of the full one, so
     * the data structure could do double duty.  However, both this and
     * RADIXCHAR would need to be in the final position of the same full
     * structure; an impossibility.  So make this into a separate structure */
    const lconv_offset_t  thousands_sep_string[] = {
        LCONV_NUMERIC_ENTRY(thousands_sep),
        {NULL, 0}
    };

#    endif

    /* End of all the initialization of data structures.  Now for actual code.
     *
     * Without nl_langinfo(), the call to my_localeconv() could be for all of
     * the localeconv() items or for just one of the following 3 items to
     * emulate nl_langinfo().
     *
     * This is compiled only when using perl_langinfo.h, which we control, and
     * it has been constructed so that no item is numbered 0.
     *
     * For each individual item, either return the known value if the current
     * locale is "C", or set up the appropriate parameters for the call below
     * to the populate function */
    if (item != 0) {
        const char *locale;

        switch (item) {
          default:
            locale_panic_(Perl_form(aTHX_
                          "Unexpected item passed to my_localeconv: %d", item));
            break;

#    ifdef USE_LOCALE_NUMERIC

          case RADIXCHAR:
            if (isNAME_C_OR_POSIX(PL_numeric_name)) {
                (void) hv_stores(hv, DECIMAL_POINT_LITERAL, newSVpvs("."));
                return hv;
            }

            strings[NUMERIC_OFFSET] = DECIMAL_POINT_ADDRESS;
            goto numeric_common;

          case THOUSEP:
            if (isNAME_C_OR_POSIX(PL_numeric_name)) {
                (void) hv_stores(hv, THOUSANDS_SEP_LITERAL, newSVpvs(""));
                return hv;
            }

            strings[NUMERIC_OFFSET] = thousands_sep_string;

          numeric_common:
            index_bits = OFFSET_TO_BIT(NUMERIC_OFFSET);
            locale = PL_numeric_name;
            break;

#    endif
#    ifdef USE_LOCALE_MONETARY

          case CRNCYSTR:    /* This item needs the values for both the currency
                               symbol, and another one used to construct the
                               nl_langino()-compatible return. */

            locale = querylocale_c(LC_MONETARY);
            if (isNAME_C_OR_POSIX(locale)) {
                (void) hv_stores(hv, CURRENCY_SYMBOL_LITERAL, newSVpvs(""));
                (void) hv_stores(hv, P_CS_PRECEDES_LITERAL, newSViv(-1));
                return hv;
            }

            strings[MONETARY_OFFSET] = CURRENCY_SYMBOL_ADDRESS;
            integers[MONETARY_OFFSET] = P_CS_PRECEDES_ADDRESS;

            index_bits = OFFSET_TO_BIT(MONETARY_OFFSET);
            break;

#    endif

        } /* End of switch() */

        /* There's only one item, so only one of each of these will get used,
         * but cheap to initialize both */
        populate[MONETARY_OFFSET] =
        populate[NUMERIC_OFFSET]  = S_populate_hash_from_localeconv;
        locales[MONETARY_OFFSET] = locales[NUMERIC_OFFSET]  = locale;
    }
    else   /* End of for just one item to emulate nl_langinfo() */

#  endif

    {
        /* Here, the call is for all of localeconv().  It has a bunch of
         * items.  The first function call always gets the MONETARY values */
        index_bits = OFFSET_TO_BIT(MONETARY_OFFSET);

#  ifdef USE_LOCALE_MONETARY

        locales[MONETARY_OFFSET] = querylocale_c(LC_MONETARY);
        populate[MONETARY_OFFSET] =
                                (isNAME_C_OR_POSIX(locales[MONETARY_OFFSET]))
                                ?  S_populate_hash_from_C_localeconv
                                :  S_populate_hash_from_localeconv;

#  else

        locales[MONETARY_OFFSET] = "C";
        populate[MONETARY_OFFSET] = S_populate_hash_from_C_localeconv;

#  endif
#  ifdef USE_LOCALE_NUMERIC

        /* And if the locales for the two categories are the same, we can also
         * do the NUMERIC values in the same call */
        if (strEQ(PL_numeric_name, locales[MONETARY_OFFSET])) {
            index_bits |= OFFSET_TO_BIT(NUMERIC_OFFSET);
            locales[NUMERIC_OFFSET] = locales[MONETARY_OFFSET];
            populate[NUMERIC_OFFSET] = populate[MONETARY_OFFSET];
        }
        else {
            requires_2nd_localeconv = true;
            locales[NUMERIC_OFFSET] = PL_numeric_name;
            populate[NUMERIC_OFFSET] = (isNAME_C_OR_POSIX(PL_numeric_name))
                                       ?  S_populate_hash_from_C_localeconv
                                       :  S_populate_hash_from_localeconv;
        }

#  else

        /* When LC_NUMERIC is confined to "C", the two locales are the same
           iff LC_MONETARY in this case is also "C".  We set up the function
           for that case above, so fastest to test just its address */
        locales[NUMERIC_OFFSET] = "C";
        if (populate[MONETARY_OFFSET] == S_populate_hash_from_C_localeconv) {
            index_bits |= OFFSET_TO_BIT(NUMERIC_OFFSET);
            populate[NUMERIC_OFFSET] = populate[MONETARY_OFFSET];
        }
        else {
            requires_2nd_localeconv = true;
            populate[NUMERIC_OFFSET] = S_populate_hash_from_C_localeconv;
        }

#  endif

    }   /* End of call is for localeconv() */

    /* Call the proper populate function (which may call localeconv()) and copy
     * its results into the hash.  All the parameters have been initialized
     * above */
    (*populate[MONETARY_OFFSET])(aTHX_
                                 hv, locales[MONETARY_OFFSET],
                                 index_bits, strings, integers);

#  ifndef HAS_SOME_LANGINFO  /* Could be using this function to emulate
                                nl_langinfo() */

    /* We are done when called with an individual item.  There are no integer
     * items to adjust, and it's best for the caller to determine if this
     * string item is UTF-8 or not.  This is because the locale's UTF-8ness is
     * calculated below, and in some Configurations, that can lead to a
     * recursive call to here, which could recurse infinitely. */
    if (item != 0) {
        return hv;
    }

#  endif

    /* The above call may have done all the hash fields, but not always, as
     * already explained.  If we need a second call it is always for the
     * NUMERIC fields */
    if (requires_2nd_localeconv) {
        (*populate[NUMERIC_OFFSET])(aTHX_
                                    hv,
                                    locales[NUMERIC_OFFSET],
                                    OFFSET_TO_BIT(NUMERIC_OFFSET),
                                    strings, integers);
    }

    /* Here, the hash has been completely populated. */

#  ifdef LOCALECONV_NEEDS_CRITICAL_SECTION

    /* When the hash was populated during a critical section, the determination
     * of whether or not a string element should be marked as UTF-8 was
     * deferred, so as to minimize the amount of time in the critical section.
     * But now we have the hash specific to this thread, and can do the
     * adjusting without worrying about delaying other threads. */
    for (unsigned int i = 0; i < 2; i++) {  /* Try both types of strings */

        /* The return from this function is already adjusted */
        if (populate[i] == S_populate_hash_from_C_localeconv) {
            continue;
        }

        /* Examine each string */
        for (const lconv_offset_t *strp = strings[i]; strp->name; strp++) {
            const char * name = strp->name;

            /* 'value' will contain the string that may need to be marked as
             * UTF-8 */
            SV ** value = hv_fetch(hv, name, strlen(name), true);
            if (value == NULL) {
                continue;
            }

            /* Determine if the string should be marked as UTF-8. */
            if (UTF8NESS_YES == (get_locale_string_utf8ness_i(SvPVX(*value),
                                                  LOCALE_UTF8NESS_UNKNOWN,
                                                  locales[i],
                                                  LC_ALL_INDEX_ /* OOB */)))
            {
                SvUTF8_on(*value);
            }
        }
    }

#  endif

    return hv;

#endif    /* End of must have one or both USE_MONETARY, USE_NUMERIC */

}

STATIC void
S_populate_hash_from_C_localeconv(pTHX_ HV * hv,
                                        const char * locale,  /* Unused */

                                        /* bit mask of which categories to
                                         * populate */
                                        const U32 which_mask,

                                        /* The string type values to return;
                                         * one element for numeric; the other
                                         * for monetary */
                                        const lconv_offset_t * strings[2],

                                        /* And the integer fields */
                                        const lconv_offset_t * integers[2])
{
    PERL_ARGS_ASSERT_POPULATE_HASH_FROM_C_LOCALECONV;
    PERL_UNUSED_ARG(locale);
    assert(isNAME_C_OR_POSIX(locale));

    /* Fill hv with the values that localeconv() is supposed to return for
     * the C locale */

    U32 working_mask = which_mask;
    while (working_mask) {

        /* Get the bit position of the next lowest set bit.  That is the
         * index into the 'strings' array of the category we use in this loop
         * iteration.  Turn the bit off so we don't work on this category
         * again in this function call. */
        const PERL_UINT_FAST8_T i = lsbit_pos(working_mask);
        working_mask &= ~ (1 << i);

        /* This category's string fields */
        const lconv_offset_t * category_strings = strings[i];

#ifndef HAS_SOME_LANGINFO /* This doesn't work properly if called on a single
                             item, which could only happen when there isn't
                             nl_langinfo on the platform */
        assert(category_strings[1].name != NULL);
#endif

        /* All string fields are empty except for one NUMERIC one.  That one
         * has been initialized to be the final one in the NUMERIC strings, so
         * stop the loop early in that case.  Otherwise, we would store an
         * empty string to the hash, and immediately overwrite it with the
         * correct value */
        const unsigned int stop_early = (i == NUMERIC_OFFSET) ? 1 : 0;

        /* A NULL element terminates the list */
        while ((category_strings + stop_early)->name) {
            (void) hv_store(hv,
                            category_strings->name,
                            strlen(category_strings->name),
                            newSVpvs(""),
                            0);

            category_strings++;
        }

        /* And fill in the NUMERIC exception */
        if (i == NUMERIC_OFFSET) {
            (void) hv_stores(hv, "decimal_point", newSVpvs("."));
            category_strings++;
        }

        /* Add any int fields.  In the C locale, all are -1 */
        if (integers[i]) {
            const lconv_offset_t * current = integers[i];
            while (current->name) {
                (void) hv_store(hv,
                                current->name, strlen(current->name),
                                newSViv(-1),
                                0);
                current++;
            }
        }
    }
}

#if defined(HAS_LOCALECONV) && (   defined(USE_LOCALE_NUMERIC)      \
                                || defined(USE_LOCALE_MONETARY))

STATIC void
S_populate_hash_from_localeconv(pTHX_ HV * hv,

                                      /* Switch to this locale to run
                                       * localeconv() from */
                                      const char * locale,

                                      /* bit mask of which categories to
                                       * populate */
                                      const U32 which_mask,

                                      /* The string type values to return; one
                                       * element for numeric; the other for
                                       * monetary */
                                      const lconv_offset_t * strings[2],

                                      /* And similarly the integer fields */
                                      const lconv_offset_t * integers[2])
{
    PERL_ARGS_ASSERT_POPULATE_HASH_FROM_LOCALECONV;

    /* Run localeconv() and copy some or all of its results to the input 'hv'
     * hash.  Most localeconv() implementations return the values in a global
     * static buffer, so for them, the operation must be performed in a
     * critical section, ending only after the copy is completed.  There are so
     * many locks because localeconv() deals with two categories, and returns
     * in a single global static buffer.  Some locks might be no-ops on this
     * platform, but not others.  We need to lock if any one isn't a no-op. */

    /* If the call could be for either or both of the two categories, we need
     * to test which one; but if the Configuration is such that we will never
     * be called with one of them, the code for that one will be #ifdef'd out
     * below, leaving code for just the other category.  That code will always
     * want to be executed, no conditional required.  Create a macro that
     * replaces the condition with an always-true value so the compiler will
     * omit the conditional */
#  if defined(USE_LOCALE_NUMERIC) && defined(USE_LOCALE_MONETARY)
#    define CALL_IS_FOR(x)  (which_mask & OFFSET_TO_BIT(x ## _OFFSET))
#  else
#    define CALL_IS_FOR(x) 1
#  endif

    /* This function is unfortunately full of #ifdefs.  It consists of three
     * sections:
     *  1)  Setup:
     *        a)  On platforms where it matters, toggle LC_CTYPE to the same
     *            locale that LC_NUMERIC and LC_MONETARY will be toggled to
     *        b)  On calls that process LC_NUMERIC, toggle to the desired locale
     *        c)  On calls that process LC_MONETARY, toggle to the desired
     *            locale
     *        d)  Do any necessary mutex locking not (automatically) done by
     *            the toggling
     *        e)  Work around some Windows-only issues and bugs
     *  2)  Do the localeconv(), copying the results.
     *  3)  Teardown, which is the inverse of setup.
     *
     * The setup and teardown are highly variable due to the variance in the
     * possible Configurations.  What is done here to make it slightly more
     * understandable is each setup section creates the details of its
     * corresponding teardown section, and macroizes them.  So that the
     * finished teardown product is just a linear series of macros.  You can
     * thus easily see the logic there. */

    /* Setup any LC_CTYPE handling */
    start_DEALING_WITH_MISMATCHED_CTYPE(locale);
#  define CTYPE_TEARDOWN  end_DEALING_WITH_MISMATCHED_CTYPE(locale)

   /* Setup any LC_NUMERIC handling */
#  ifndef USE_LOCALE_NUMERIC
#    define NUMERIC_TEARDOWN
#  else

    /* We need to toggle the NUMERIC locale to the desired one if we are
     * getting NUMERIC strings */
    const char * orig_NUMERIC_locale = NULL;
    if (CALL_IS_FOR(NUMERIC)) {

#    ifdef WIN32

        /* There is a bug in Windows in which setting LC_CTYPE after the others
         * doesn't actually take effect for localeconv().  See commit
         * 418efacd1950763f74ed3cc22f8cf9206661b892 for details.  Thus we have
         * to make sure that the locale we want is set after LC_CTYPE.  We
         * unconditionally toggle away from and back to the current locale
         * prior to calling localeconv(). */
        orig_NUMERIC_locale = toggle_locale_c(LC_NUMERIC, "C");
        (void) toggle_locale_c(LC_NUMERIC, locale);

#      define NUMERIC_TEARDOWN                                              \
          STMT_START {                                                      \
            if (CALL_IS_FOR(NUMERIC)) {                                     \
                restore_toggled_locale_c(LC_NUMERIC, "C");                  \
                restore_toggled_locale_c(LC_NUMERIC, orig_NUMERIC_locale);  \
            }                                                               \
          } STMT_END

#    else

        /* No need for the extra toggle when not on Windows */
        orig_NUMERIC_locale = toggle_locale_c(LC_NUMERIC, locale);

#      define NUMERIC_TEARDOWN                                              \
         STMT_START {                                                       \
            if (CALL_IS_FOR(NUMERIC)) {                                     \
                restore_toggled_locale_c(LC_NUMERIC, orig_NUMERIC_locale);  \
            }                                                               \
         } STMT_END
#    endif

    }

#  endif  /* End of LC_NUMERIC setup */

   /* Setup any LC_MONETARY handling, using the same logic as for
    * USE_LOCALE_NUMERIC just above */
#  ifndef USE_LOCALE_MONETARY
#    define MONETARY_TEARDOWN
#  else

    /* Same logic as LC_NUMERIC, and same Windows bug */
    const char * orig_MONETARY_locale = NULL;
    if (CALL_IS_FOR(MONETARY)) {

#    ifdef WIN32

        orig_MONETARY_locale = toggle_locale_c(LC_MONETARY, "C");
        (void) toggle_locale_c(LC_MONETARY, locale);

#      define MONETARY_TEARDOWN                                             \
         STMT_START {                                                       \
            if (CALL_IS_FOR(MONETARY)) {                                    \
                restore_toggled_locale_c(LC_MONETARY, "C");                 \
                restore_toggled_locale_c(LC_MONETARY, orig_MONETARY_locale);\
            }                                                               \
         } STMT_END

#    else

        /* No need for the extra toggle when not on Windows */
        orig_MONETARY_locale = toggle_locale_c(LC_MONETARY, locale);

#      define MONETARY_TEARDOWN                                             \
         STMT_START {                                                       \
            if (CALL_IS_FOR(MONETARY)) {                                    \
                restore_toggled_locale_c(LC_MONETARY, orig_MONETARY_locale);\
            }                                                               \
         } STMT_END

#    endif

    }

#  endif  /* End of LC_MONETARY setup */

    /* Here, have toggled to the correct locale.
     *
     * We don't need to worry about locking at all if localeconv() is
     * thread-safe, regardless of if using threads or not. */
#  ifdef LOCALECONV_IS_THREAD_SAFE
#    define LOCALECONV_UNLOCK
#  else

     /* Otherwise, the gwLOCALE_LOCK macro expands to whatever locking is
      * needed (none if there is only a single perl instance) */
    gwLOCALE_LOCK;

#    define LOCALECONV_UNLOCK  gwLOCALE_UNLOCK
#  endif
#  if ! defined(TS_W32_BROKEN_LOCALECONV) || ! defined(USE_THREAD_SAFE_LOCALE)
#    define WIN32_TEARDOWN
#  else

    /* This is a workaround for another bug in Windows.  localeconv() was
     * broken with thread-safe locales prior to VS 15.  It looks at the global
     * locale instead of the thread one.  As a work-around, we toggle to the
     * global locale; populate the return; then toggle back.  We have to use
     * LC_ALL instead of the individual categories because of yet another bug
     * in Windows.  And this all has to be done in a critical section.
     *
     * This introduces a potential race with any other thread that has also
     * converted to use the global locale, and doesn't protect its locale calls
     * with mutexes.  khw can't think of any reason for a thread to do so on
     * Windows, as the locale API is the same regardless of thread-safety,
     * except if the code is ported from working on another platform where
     * there might be some reason to do this.  But this is typically due to
     * some alien-to-Perl library that thinks it owns locale setting.  Such a
     * library isn't likely to exist on Windows, so such an application is
     * unlikely to be run on Windows
     */
    bool restore_per_thread = FALSE;

    /* Save the per-thread locale state */
    const char * save_thread = querylocale_c(LC_ALL);

    /* Change to the global locale, and note if we already were there */
    int config_return = _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
    if (config_return != _DISABLE_PER_THREAD_LOCALE) {
        if (config_return == -1) {
            locale_panic_("_configthreadlocale returned an error");
        }

        restore_per_thread = TRUE;
    }

    /* Save the state of the global locale; then convert to our desired
     * state.  */
    const char * save_global = querylocale_c(LC_ALL);
    void_setlocale_c(LC_ALL, save_thread);

#   define WIN32_TEARDOWN                                                   \
         STMT_START {                                                       \
            /* Restore the global locale's prior state */                   \
            void_setlocale_c(LC_ALL, save_global);                          \
                                                                            \
            /* And back to per-thread locales */                            \
            if (restore_per_thread) {                                       \
                if (_configthreadlocale(_ENABLE_PER_THREAD_LOCALE) == -1) { \
                    locale_panic_("_configthreadlocale returned an error"); \
                }                                                           \
            }                                                               \
                                                                            \
            /* Restore the per-thread locale state */                       \
            void_setlocale_c(LC_ALL, save_thread);                          \
        } STMT_END
#  endif  /* TS_W32_BROKEN_LOCALECONV */


    /* Finally, everything is locked and loaded; do the actual call to
     * localeconv() */
    const char *lcbuf_as_string = (const char *) localeconv();

    /* Copy its results for each desired category as determined by
     * 'which_mask' */
    U32 working_mask = which_mask;
    while (working_mask) {

        /* Get the bit position of the next lowest set bit.  That is the
         * index into the 'strings' array of the category we use in this loop
         * iteration.  Turn the bit off so we don't work on this category
         * again in this function call. */
        const PERL_UINT_FAST8_T i = lsbit_pos32(working_mask);
        working_mask &= ~ (1 << i);

        /* Point to the string field list for the given category ... */
        const lconv_offset_t * category_strings = strings[i];

        /* The string fields returned by localeconv() are stored as SVs in the
         * hash.  Their utf8ness needs to be calculated at some point, and the
         * SV flagged accordingly.  It is easier to do that now as we go
         * through them, but strongly countering this is the need to minimize
         * the length of time spent in a critical section with other threads
         * locked out.  Therefore, when this is being executed in a critical
         * section, the strings are stored as-is, and the utf8ness calculation
         * is done by our caller, outside the critical section, in an extra
         * pass through the hash.  But when this code is not being executed in
         * a critical section, that extra pass would be extra work, so the
         * calculation is done here.  We have #defined a symbol that indicates
         * whether or not this is being done in a critical section.  But there
         * is a complication.  When this is being called with just a single
         * string to populate the hash with, there may be extra adjustments
         * needed, and the ultimate caller is expecting to do all adjustments,
         * so the adjustment is deferred in this case even if there is no
         * critical section.  (This case is indicated by element [1] being a
         * NULL marker, hence having only one real element.) */
#  ifndef LOCALECONV_NEEDS_CRITICAL_SECTION
        const bool calculate_utf8ness_here = category_strings[1].name;
#  endif
        bool utf8ness = false;

        /* For each string field */
        while (category_strings->name) {

            /* We have set things up so that we know where in the returned
             * structure, when viewed as a string, the corresponding value is.
             * */
            char *value = *((char **)(  lcbuf_as_string
                                      + category_strings->offset));
            if (value) {    /* Copy to the hash */

#  ifndef LOCALECONV_NEEDS_CRITICAL_SECTION

                if (calculate_utf8ness_here) {
                    utf8ness =
                      (   UTF8NESS_YES
                       == get_locale_string_utf8ness_i(value,
                                                      LOCALE_UTF8NESS_UNKNOWN,
                                                      locale,
                                                      LC_ALL_INDEX_ /* OOB */));
                }
#  endif
                (void) hv_store(hv,
                                category_strings->name,
                                strlen(category_strings->name),
                                newSVpvn_utf8(value, strlen(value), utf8ness),
                                0);
            }

            category_strings++;
        }

        /* Add any int fields to the HV*. */
        if (integers[i]) {
            const lconv_offset_t * current = integers[i];
            while (current->name) {
                int value = *((const char *)(  lcbuf_as_string
                                             + current->offset));
                if (value == CHAR_MAX) { /* Change CHAR_MAX to -1 */
                    value = -1;
                }

                (void) hv_store(hv,
                                current->name, strlen(current->name),
                                newSViv(value),
                                0);
                current++;
            }
        }
    }   /* End of loop through the fields */

    /* Done with copying to the hash.  Can unwind the critical section locks */

    /* Back out of what we set up */
    WIN32_TEARDOWN;
    LOCALECONV_UNLOCK;
    MONETARY_TEARDOWN;
    NUMERIC_TEARDOWN;
    CTYPE_TEARDOWN;
}

#endif    /* defined(USE_LOCALE_NUMERIC) || defined(USE_LOCALE_MONETARY) */

/*

=for apidoc      Perl_langinfo
=for apidoc_item Perl_langinfo8

C<Perl_langinfo> is an (almost) drop-in replacement for the system
C<L<nl_langinfo(3)>>, taking the same C<item> parameter values, and returning
the same information.  But it is more thread-safe than regular
C<nl_langinfo()>, and hides the quirks of Perl's locale handling from your
code, and can be used on systems that lack a native C<nl_langinfo>.

However, you should instead use either the improved version of this,
L</Perl_langinfo8>, or even better, L</sv_langinfo>.  The latter returns an SV,
handling all the possible non-standard returns of C<nl_langinfo()>, including
the UTF8ness of any returned string.

C<Perl_langinfo8> is identical to C<Perl_langinfo> except for an additional
parameter, a pointer to a variable declared as L</C<utf8ness_t>>, into which it
returns to you how you should treat the returned string with regards to it
being encoded in UTF-8 or not.

These two functions share private per-thread memory that will be changed the
next time either one of them is called with any input, but not before.

Concerning the differences between these and plain C<nl_langinfo()>:

=over

=item a.

C<Perl_langinfo8> has an extra parameter, described above.  Besides this, the
other reason they aren't quite a drop-in replacement is actually an advantage.
The C<const>ness of the return allows the compiler to catch attempts to write
into the returned buffer, which is illegal and could cause run-time crashes.

=item b.

They deliver the correct results for the C<RADIXCHAR> and C<THOUSEP> items,
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

=item c.

The system function they replace can have its static return buffer trashed,
not only by a subsequent call to that function, but by a C<freelocale>,
C<setlocale>, or other locale change.  The returned buffer of these functions
is not changed until the next call to one or the other, so the buffer is never
in a trashed state.

=item d.

The return buffer is per-thread, so it also is never overwritten by a call to
these functions from another thread;  unlike the function it replaces.

=item e.

But most importantly, they work on systems that don't have C<nl_langinfo>, such
as Windows, hence making your code more portable.  Of the fifty-some possible
items specified by the POSIX 2008 standard,
L<https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/langinfo.h.html>,
only one is completely unimplemented, though on non-Windows platforms, another
significant one is not fully implemented).  They use various techniques to
recover the other items, including calling C<L<localeconv(3)>>, and
C<L<strftime(3)>>, both of which are specified in C89, so should be always be
available.  Later C<strftime()> versions have additional capabilities.
If an item is not available on your system, this returns either the value
associated with the C locale, or simply C<"">, whichever is more appropriate.

It is important to note that, when called with an item that is recovered by
using C<localeconv>, the buffer from any previous explicit call to
C<L<localeconv(3)>> will be overwritten.  But you shouldn't be using
C<localeconv> anyway because it is is very much not thread-safe, and suffers
from the same problems outlined in item 'b.' above for the fields it returns
that are controlled by the LC_NUMERIC locale category.  Instead, avoid all of
those problems by calling L</Perl_localeconv>, which is thread-safe; or by
using the methods given in L<perlcall>  to call
L<C<POSIX::localeconv()>|POSIX/localeconv>, which is also thread-safe.

=back

The details for those items which may deviate from what this emulation returns
and what a native C<nl_langinfo()> would return are specified in
L<I18N::Langinfo>.

=for apidoc  sv_langinfo

This is the preferred interface for accessing the data that L<nl_langinfo(3)>
provides (or Perl's emulation of it on platforms lacking it), returning an SV.
Unlike, the earlier-defined interfaces to this (L</Perl_langinfo> and
L</Perl_langinfo8>), which return strings, the UTF8ness of the result is
automatically handled for you.  And like them, it is thread-safe and
automatically handles getting the proper values for the C<RADIXCHAR> and
C<THOUSEP> items (that calling the plain libc C<nl_langinfo()> could give the
wrong results for).  Like them, this also doesn't play well with the libc
C<localeconv()>; use L<C<POSIX::localeconv()>|POSIX/localeconv> instead.

There are a few deviations from what a native C<nl_langinfo()> would return and
what this returns on platforms that don't implement that function.  These are
detailed in L<I18N::Langinfo>.

=cut

*/

/* external_call_langinfo() is an interface to callers from outside this file to
 * langinfo_sv_i(), calculating a necessary value for it.  If those functions
 * aren't defined, the fallback function is emulate_langinfo(), which doesn't
 * use that value (as everything in this situation takes place in the "C"
 * locale), and so we define this macro to transparently hide the absence of
 * the missing functions */
#ifndef external_call_langinfo
#  define external_call_langinfo(item, sv, utf8p)                           \
                                    emulate_langinfo(item, "C", sv, utf8p)
#endif

SV *
Perl_sv_langinfo(pTHX_ const nl_item  item) {
    utf8ness_t dummy;   /* Having this tells the layers below that we want the
                           UTF-8 flag in 'sv' to be set properly. */

    SV * sv = newSV_type(SVt_PV);
    (void) external_call_langinfo(item, sv, &dummy);

    return sv;
}

const char *
Perl_langinfo(const nl_item item)
{
    dTHX;

    (void) external_call_langinfo(item, PL_langinfo_sv, NULL);
    return SvPV_nolen(PL_langinfo_sv);
}

const char *
Perl_langinfo8(const nl_item item, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_PERL_LANGINFO8;
    dTHX;

    (void) external_call_langinfo(item, PL_langinfo_sv, utf8ness);
    return SvPV_nolen(PL_langinfo_sv);
}

#ifdef USE_LOCALE

const char *
S_external_call_langinfo(pTHX_ const nl_item item,
                               SV * sv,
                               utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_EXTERNAL_CALL_LANGINFO;

    /* Find the locale category that controls the input 'item', and call
     * langinfo_sv_i() including that value.
     *
     * If we are not paying attention to that category, instead call
     * emulate_langinfo(), which knows how to handle this situation. */
    locale_category_index  cat_index = LC_ALL_INDEX_;  /* Out-of-bounds */

    switch (item) {
      case CODESET:

#  ifdef USE_LOCALE_CTYPE
        cat_index = LC_CTYPE_INDEX_;
#  endif
        break;


      case YESEXPR: case YESSTR: case NOEXPR: case NOSTR:

#  ifdef USE_LOCALE_MESSAGES
        cat_index = LC_MESSAGES_INDEX_;
#  endif
        break;


      case CRNCYSTR:

#  ifdef USE_LOCALE_MONETARY
        cat_index = LC_MONETARY_INDEX_;
#  endif
        break;


      case RADIXCHAR: case THOUSEP:

#  ifdef USE_LOCALE_NUMERIC
        cat_index = LC_NUMERIC_INDEX_;
#  endif
        break;


      case _NL_ADDRESS_POSTAL_FMT:
      case _NL_ADDRESS_COUNTRY_NAME:
      case _NL_ADDRESS_COUNTRY_POST:
      case _NL_ADDRESS_COUNTRY_AB2:
      case _NL_ADDRESS_COUNTRY_AB3:
      case _NL_ADDRESS_COUNTRY_CAR:
      case _NL_ADDRESS_COUNTRY_NUM:
      case _NL_ADDRESS_COUNTRY_ISBN:
      case _NL_ADDRESS_LANG_NAME:
      case _NL_ADDRESS_LANG_AB:
      case _NL_ADDRESS_LANG_TERM:
      case _NL_ADDRESS_LANG_LIB:
#  ifdef USE_LOCALE_ADDRESS
        cat_index = LC_ADDRESS_INDEX_;
#  endif
        break;


      case _NL_IDENTIFICATION_TITLE:
      case _NL_IDENTIFICATION_SOURCE:
      case _NL_IDENTIFICATION_ADDRESS:
      case _NL_IDENTIFICATION_CONTACT:
      case _NL_IDENTIFICATION_EMAIL:
      case _NL_IDENTIFICATION_TEL:
      case _NL_IDENTIFICATION_FAX:
      case _NL_IDENTIFICATION_LANGUAGE:
      case _NL_IDENTIFICATION_TERRITORY:
      case _NL_IDENTIFICATION_AUDIENCE:
      case _NL_IDENTIFICATION_APPLICATION:
      case _NL_IDENTIFICATION_ABBREVIATION:
      case _NL_IDENTIFICATION_REVISION:
      case _NL_IDENTIFICATION_DATE:
      case _NL_IDENTIFICATION_CATEGORY:
#  ifdef USE_LOCALE_IDENTIFICATION
        cat_index = LC_IDENTIFICATION_INDEX_;
#  endif
        break;


      case _NL_MEASUREMENT_MEASUREMENT:
#  ifdef USE_LOCALE_MEASUREMENT
        cat_index = LC_MEASUREMENT_INDEX_;
#  endif
        break;


      case _NL_NAME_NAME_FMT:
      case _NL_NAME_NAME_GEN:
      case _NL_NAME_NAME_MR:
      case _NL_NAME_NAME_MRS:
      case _NL_NAME_NAME_MISS:
      case _NL_NAME_NAME_MS:
#  ifdef USE_LOCALE_NAME
        cat_index = LC_NAME_INDEX_;
#  endif
        break;


      case _NL_PAPER_HEIGHT:
      case _NL_PAPER_WIDTH:
#  ifdef USE_LOCALE_PAPER
        cat_index = LC_PAPER_INDEX_;
#  endif
        break;


      case _NL_TELEPHONE_TEL_INT_FMT:
      case _NL_TELEPHONE_TEL_DOM_FMT:
      case _NL_TELEPHONE_INT_SELECT:
      case _NL_TELEPHONE_INT_PREFIX:
#  ifdef USE_LOCALE_TELEPHONE
        cat_index = LC_TELEPHONE_INDEX_;
#  endif
        break;


      default:  /* The other possible items are all in LC_TIME. */
#  ifdef USE_LOCALE_TIME
        cat_index = LC_TIME_INDEX_;
#  endif
        break;

    } /* End of switch on item */

#  if defined(HAS_MISSING_LANGINFO_ITEM_)

    /* If the above didn't find the category's index, it has to be because the
     * item is unknown to us (and the callee will handle that), or the category
     * is confined to the "C" locale on this platform, which the callee also
     * handles.  (LC_MESSAGES is not required by the C Standard (the others
     * above are), so we have to emulate it on platforms lacking it (such as
     * Windows).) */
    if (cat_index == LC_ALL_INDEX_) {
        return emulate_langinfo(item, "C", sv, utf8ness);
    }

#  endif

    /* And get the value for this 'item', whose category has now been
     * calculated.  We need to find the current corresponding locale, and pass
     * that as well. */
    return langinfo_sv_i(item, cat_index,
                         query_nominal_locale_i(cat_index),
                         sv, utf8ness);
}

#endif
#if defined(USE_LOCALE) && defined(HAS_NL_LANGINFO)

STATIC const char *
S_langinfo_sv_i(pTHX_
                const nl_item item,           /* The item to look up */

                /* The locale category that controls it */
                locale_category_index cat_index,

                /* The locale to look up 'item' in. */
                const char * locale,

                /* The SV to store the result in; see below */
                SV * sv,

                /* If not NULL, the location to store the UTF8-ness of 'item's
                 * value, as documented */
                utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_LANGINFO_SV_I;
    assert(cat_index < LC_ALL_INDEX_);

    /* This function is the interface to nl_langinfo(), returning a thread-safe
     * result, valid until its next call that uses the same 'sv'.  Similarly,
     * the S_emulate_langinfo() function below does the same, when
     * nl_langinfo() isn't available for the desired locale, or is completely
     * absent from the system.  It is hopefully invisible to an outside caller
     * as to which one of the two actually ends up processing the request.
     * This comment block hence generally describes the two functions as a
     * unit.
     *
     * The two functions both return values (using 'return' statements) and
     * potentially change the contents of the passed in SV 'sv'.  However, in
     * any given call, only one of the return types is reliable.
     *
     * When the passed in SV is 'PL_scratch_langinfo', the functions make sure
     * that the 'return' statements return the correct value, but whatever
     * value is in 'PL_scratch_langinfo' should be considered garbage.  When it
     * is any other SV, that SV will get the correct result, and the value
     * returned by a 'return' statement should be considered garbage.
     *
     * The reason for this is twofold:
     *
     *  1) These functions serve two masters.  For most purposes when called
     *     from within this file, the desired value is used immediately, and
     *     then no longer required.  For these, the 'return' statement values
     *     are most convenient.
     *
     *     But when the call is initiated from an external XS source, like
     *     I18N::Langinfo, the value needs to be able to be stable for a longer
     *     time and likely returned to Perl space.  An SV return is most
     *     convenient for these
     *
     *     Further, some Configurations use these functions reentrantly.  For
     *     those, an SV must be passed.
     *
     *  2) In S_emulate_langinfo(), most langinfo items are easy or even
     *     trivial to get.  These are amenable to being returned by 'return'
     *     statements.  But others are more complex, and use the infrastructure
     *     provided by perl's SV functions to help out.
     *
     * So for some items, it is most convenient to 'return' a simple value; for
     * others an SV is most convenient.  And some callers want a simple value;
     * others want or need an SV.  It would be wasteful to have an SV, convert
     * it to a simple value, discarding the SV, then create a new SV.
     *
     * The solution adopted here is to always pass an SV, and have a reserved
     * one, PL_scratch_langinfo, indicate that a 'return' is desired.  That SV
     * is then used as scratch for the items that it is most convenient to use
     * an SV in calculating.  Besides these two functions and initialization,
     * the only mention of PL_scratch_langinfo is in the expansion of a single
     * macro that is used by the code in this file that desires a non-SV return
     * value.
     *
     * A wart of this interface is that to get the UTF-8 flag of the passed-in
     * SV set, you have to also pass a non-null 'utf8ness' parameter.  This is
     * entirely to prevent the extra expense of calculating UTF-8ness when the
     * caller is plain Perl_langinfo(), which doesn't care about this.  If that
     * seems too kludgy, other mechanisms could be devised.  But be aware that
     * the SV interface has to have a way to not calculate UTF-8ness, or else
     * the reentrant uses could infinitely recurse */

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "Entering langinfo_sv_i item=%jd, using locale %s\n",
                           (PERL_INTMAX_T) item, locale));

#  ifdef HAS_MISSING_LANGINFO_ITEM_

    if (! category_available[cat_index]) {
        return emulate_langinfo(item, locale, sv, utf8ness);
    }

#  endif

    /* One might be tempted to avoid any toggling by instead using
     * nl_langinfo_l() on platforms that have it.  This would entail creating a
     * locale object with newlocale() and freeing it afterwards.  But doing so
     * runs significantly slower than just doing the toggle ourselves.
     * lib/locale_threads.t was slowed down by 25% on Ubuntu 22.04 */

    start_DEALING_WITH_MISMATCHED_CTYPE(locale);

    const char * orig_switched_locale = toggle_locale_i(cat_index, locale);

/* nl_langinfo() is supposedly thread-safe except for its return value.  The
 * POSIX 2017 Standard states:
 *
 *    "The pointer returned by nl_langinfo() might be invalidated or the string
 *    content might be overwritten by a subsequent call to nl_langinfo() in any
 *    thread or to nl_langinfo_l() in the same thread or the initial thread, by
 *    subsequent calls to setlocale() with a category corresponding to the
 *    category of item (see <langinfo.h>) or the category LC_ALL, or by
 *    subsequent calls to uselocale() which change the category corresponding
 *    to the category of item."
 *
 * The implications of this are:
 *  a) Threaded:    nl_langinfo()'s return must be saved in a critical section
 *                  to avoid having another thread's call to it destroying the
 *                  result.  That means that the whole call to nl_langinfo()
 *                  plus the save must be done in a critical section.
 *  b) Unthreaded:  No critical section is needed (accomplished by having the
 *                  locks below be no-ops in this case).  But any subsequent
 *                  setlocale() or uselocale() could still destroy it.
 *                  Note that before returning, this function restores any
 *                  toggled locale categories.  These could easily end up
 *                  calling uselocale() or setlocale(), destroying our
 *                  result.  (And in some Configurations, this file currently
 *                  calls nl_langinfo_l() to determine if a uselocale() is
 *                  needed.)  So, a copy of the result is made in this case as
 *                  well.
 */
    const char * retval = NULL;
    utf8ness_t is_utf8 = UTF8NESS_UNKNOWN;

    /* Do a bit of extra work so avoid
     *  switch() { default: ... }
     * where the only case in it is the default: */
#  if defined(USE_LOCALE_PAPER)                 \
   || defined(USE_LOCALE_MEASUREMENT)           \
   || defined(USE_LOCALE_ADDRESS)
#    define IS_SWITCH  1
#    define MAYBE_SWITCH(n)  switch(n)
#  else
#    define IS_SWITCH  0
#    define MAYBE_SWITCH(n)
#  endif

    GCC_DIAG_IGNORE_STMT(-Wimplicit-fallthrough);

    MAYBE_SWITCH(item) {

#  if defined(USE_LOCALE_MEASUREMENT)

      case _NL_MEASUREMENT_MEASUREMENT:
       {
        /* An ugly API; only the first byte of the returned char* address means
         * anything */
        gwLOCALE_LOCK;
        char char_value = nl_langinfo(item)[0];
        gwLOCALE_UNLOCK;

        sv_setuv(sv, char_value);
       }

        goto non_string_common;

#  endif
#  if defined(USE_LOCALE_ADDRESS) || defined(USE_LOCALE_PAPER)
#    if defined(USE_LOCALE_ADDRESS)

      case _NL_ADDRESS_COUNTRY_NUM:

        /* Some glibc's return random values for this item and locale;
         * workaround by special casing it. */
        if (isNAME_C_OR_POSIX(locale)) {
            sv_setuv(sv, 0);
            goto non_string_common;
        }

        /* FALLTHROUGH */

#    endif
#    if defined(USE_LOCALE_PAPER)

      case _NL_PAPER_HEIGHT: case _NL_PAPER_WIDTH:

#    endif

       {    /* A slightly less ugly API; the int portion of the returned char*
             * address is an integer. */
        gwLOCALE_LOCK;
        int int_value = (int) PTR2UV(nl_langinfo(item));
        gwLOCALE_UNLOCK;

        sv_setuv(sv, int_value);
       }

#  endif
#  if IS_SWITCH
#    if defined(USE_LOCALE_MEASUREMENT)

       non_string_common:

#    endif

        /* In all cases that get here, the char* instead delivers a numeric
         * value, so its UTF-8ness is meaningless */
        is_utf8 = UTF8NESS_IMMATERIAL;

        if (sv == PL_scratch_langinfo) {
            retval = SvPV_nomg_const_nolen(sv);
        }

        break;

      default:

#  endif

        /* The rest of the possibilities deliver a true char* pointer to a
         * string (or sequence of strings in the case of ALT_DIGITS) */
        gwLOCALE_LOCK;

        retval = nl_langinfo(item);
        Size_t total_len = strlen(retval);

        /* Initialized only to silence some dumber compilers warning that
         * might be uninitialized */
        char separator = ';';

        if (UNLIKELY(item == ALT_DIGITS) && total_len > 0) {

            /* The return from nl_langinfo(ALT_DIGITS) is specified by the
             * 2017 POSIX Standard as a string consisting of "semicolon-
             * separated symbols. The first is the alternative symbol
             * corresponding to zero, the second is the symbol corresponding to
             * one, and so on.  Up to 100 alternative symbols may be
             * specified".  Infuriatingly, Linux does not follow this, and uses
             * the least C-language-friendly separator possible, the NUL.  In
             * case other platforms also violate the standard, the code below
             * looks for NUL and any graphic \W character as a potential
             * separator. */
            const char * sep_pos = strchr(retval, ';');
            if (! sep_pos) {
                sep_pos = strpbrk(retval, " !\"#$%&'()*+,-./:<=>?@[\\]^_`{|}~");
            }
            if (sep_pos) {
                separator = *sep_pos;
            }
            else if (strpbrk(retval, "123456789")) {

                /* Alternate digits, with the possible exception of 0,
                 * shouldn't be standard digits, so if we get any back, return
                 * that there aren't alternate digits.  0 is an exception
                 * because there may be locales that do not have a zero, such
                 * as Roman numerals.  It could therefore be that alt-0 is 0,
                 * but alt-1 better be some multi-byte Unicode character(s)
                 * like U+2160, ROMAN NUMERAL ONE.  This clause is necessary
                 * because the total length of the ASCII digits won't trigger
                 * the conditional in the next clause that protects against
                 * non-Standard libc returns, such as in Alpine platforms, but
                 * multi-byte returns will trigger it */
                retval = "";
                total_len = 0;
            }
            else if (UNLIKELY(total_len >
                                        2 * UVCHR_SKIP(PERL_UNICODE_MAX) * 4))
            {   /* But as a check against the possibility that the separator is
                 * some other character, look at the length of the returned
                 * string.  If the separator is a NUL, the length will be just
                 * for the first NUL-terminated segment; if it is some other
                 * character, there is only a single segment with all returned
                 * alternate digits, which will be quite a bit longer than just
                 * the first one.  Many locales will always have a leading zero
                 * to represent 0-9 (hence the 2* in the conditional above).
                 * The conditional uses the worst case value of the most number
                 * of byte possible for a Unicode character, and it is possible
                 * that it requires several characters to represent a single
                 * value; hence the final multiplier.  This length represents a
                 * conservative upper limit of the number of bytes for the
                 * alternative representation of 00, but if the string
                 * represents even only the first 10 alternative digits, it
                 * will be much longer than that.  So to reach here, the
                 * separator must be some other byte. */
                locale_panic_(Perl_form(aTHX_
                                        "Can't find separator in ALT_DIGITS"
                                        " representation '%s' for locale '%s'",
                                        _byte_dump_string((U8 *) retval,
                                                          total_len, 0),
                                        locale));
            }
            else {
                separator = '\0';

                /* Must be using NUL to separate the digits.  There are up to
                 * 100 of them.  Find the length of the entire sequence.
                 *
                 * The only way it could work if fewer is if it ends in two
                 * NULs.  khw has seen cases where there is no 2nd NUL on a 100
                 * digit return. */
                const char * s = retval + total_len + 1;

                for (unsigned int i = 1; i <= 99; i++) {
                    Size_t len = strlen(s) + 1;
                    total_len += len;

                    if (len == 1) {     /* Only a NUL */
                        break;
                    }

                    s += len;
                }
            }
        }

        sv_setpvn(sv, retval, total_len);

        gwLOCALE_UNLOCK;

        /* Convert the ALT_DIGITS separator to a semi-colon if not already */
        if (UNLIKELY(item == ALT_DIGITS) && total_len > 0 && separator != ';') {

            /* Operate directly on the string in the SV */
            char * digit_string = SvPVX(sv);
            char * s = digit_string;
            char * e = s + total_len;

            do {
                char * this_end = (char *) memchr(s, separator, total_len);
                if (! this_end || this_end >= e) {
                    break;
                }

                *this_end = ';';
                s = this_end;
            } while (1);
        }

        SvUTF8_off(sv);
        retval = SvPV_nomg_const_nolen(sv);
    }

    GCC_DIAG_RESTORE_STMT;

    restore_toggled_locale_i(cat_index, orig_switched_locale);
    end_DEALING_WITH_MISMATCHED_CTYPE(locale);

    if (utf8ness) {
        if (LIKELY(is_utf8 == UTF8NESS_UNKNOWN)) {  /* default: case above */
            is_utf8 = get_locale_string_utf8ness_i(retval,
                                                   LOCALE_UTF8NESS_UNKNOWN,
                                                   locale, cat_index);
        }

        *utf8ness = is_utf8;

        if (*utf8ness == UTF8NESS_YES) {
            SvUTF8_on(sv);
        }
    }

    return retval;
}

#  undef IS_SWITCH
#  undef MAYBE_SWITCH
#endif
#ifndef HAS_DEFINITIVE_UTF8NESS_DETERMINATION

/* Forward declaration of function that we don't put into embed.fnc so as to
 * make its removal easier, as there may not be any extant platforms that need
 * it; and the function is located after emulate_langinfo() because it's easier
 * to understand when placed in the context of that code */
STATIC bool
S_maybe_override_codeset(pTHX_ const char * codeset,
                               const char * locale,
                               const char ** new_codeset);
#endif
#if ! defined(HAS_NL_LANGINFO) || defined(HAS_MISSING_LANGINFO_ITEM_)

STATIC const char *
S_emulate_langinfo(pTHX_ const PERL_INTMAX_T item,
                         const char * locale,
                         SV * sv,
                         utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_EMULATE_LANGINFO;
    PERL_UNUSED_ARG(locale);    /* Too complicated to specify which
                                   Configurations use this vs which don't */

    /* This emulates nl_langinfo() on platforms:
     *   1) where it doesn't exist; or
     *   2) where it does exist, but there are categories that it shouldn't be
     *      called on because they don't exist on the platform or we are
     *      supposed to always stay in the C locale for them.  This function
     *      has hard-coded in the results for those for the C locale.
     *
     * This function returns a thread-safe result, valid until its next call
     * that uses the same 'sv'.  Similarly, the S_langinfo_sv_i() function
     * above does the same when nl_langinfo() is available.  Its comments
     * include a general description of the interface for both it and this
     * function.  That function should be the one called by code outside this
     * little group.  If it can't handle the request, it gets handed off to
     * this function.
     *
     * The major platform lacking nl_langinfo() is Windows.  It does have
     * GetLocaleInfoEx() that could be used to get most of the items, but it
     * (and other similar Windows API functions) use what MS calls "locale
     * names", whereas the C functions use what MS calls "locale strings".  The
     * locale string "English_United_States.1252" is equivalent to the locale
     * name "en_US".  There are tables inside Windows that translate between
     * the two forms, but they are not exposed.  Also calling setlocale(), then
     * calling GetThreadLocale() doesn't work, as the former doesn't change the
     * latter's return.  Therefore we are stuck using the mechanisms below. */

    /* Almost all the items will have ASCII return values.  Set that here, and
     * override if necessary */
    utf8ness_t is_utf8 = UTF8NESS_IMMATERIAL;
    const char * retval = NULL;

    /* This function returns its result either by returning the calculated
     * value 'retval' if the 'sv' argument is PL_scratch_langinfo; or for any
     * other value of 'sv', it places the result into that 'sv'.  For some
     * paths through the code, it is more convenient, in the moment, to use one
     * or the other to hold the calculated result.  And, the calculation could
     * end up with the value in both places.  At the end, if the caller
     * wants the convenient result, we are done; but if it wants the opposite
     * type of value, it must be converted.  These macros are used to tell the
     * code at the end where the value got placed. */
#  define RETVAL_IN_retval -1
#  define RETVAL_IN_BOTH    0
#  define RETVAL_IN_sv      1
#  define isRETVAL_IN_sv(type)      ((type) >= RETVAL_IN_BOTH)
#  define isRETVAL_IN_retval(type)  ((type) <= RETVAL_IN_BOTH)

    /* Most calculations place the result in 'retval', so initialize to that,
     * and override if necessary */
    int retval_type = RETVAL_IN_retval;

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                        "Entering emulate_langinfo item=%jd, using locale %s\n",
                        item, locale));

#  if   defined(HAS_LOCALECONV)                                         \
   && ! defined(HAS_SOME_LANGINFO)                                      \
   &&  (defined(USE_LOCALE_NUMERIC) || defined(USE_LOCALE_MONETARY))

    locale_category_index  cat_index;
    const char * localeconv_key;
    I32 localeconv_klen;

#  endif

    GCC_DIAG_IGNORE_STMT(-Wimplicit-fallthrough);

    switch (item) {

#  if ! defined(HAS_SOME_LANGINFO) || ! LC_MESSAGES_AVAIL_

      /* The following items have no way khw could figure out how to get except
       * via nl_langinfo() */
      case YESEXPR:   retval = "^[+1yY]"; break;
      case YESSTR:    retval = "yes";     break;
      case NOEXPR:    retval = "^[-0nN]"; break;
      case NOSTR:     retval = "no";      break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_MONETARY_AVAIL_
#    if defined(USE_LOCALE_MONETARY) && defined(HAS_LOCALECONV)
#      define NEED_USE_LOCALECONV

      case CRNCYSTR:
        cat_index = LC_MONETARY_INDEX_;
        localeconv_key = CURRENCY_SYMBOL_LITERAL;
        localeconv_klen = STRLENs(CURRENCY_SYMBOL_LITERAL);
        goto use_localeconv;

#    else

      case CRNCYSTR:

        /* The locale's currency symbol may be empty.  But if not, the return
         * from nl_langinfo() prefixes it with a character that indicates where
         * in the monetary value the symbol is to be placed
         *  a) before, like $9.99
         *  b) middle, rare, but would like be 9$99
         *  c) after,  like 9.99USD
         *
         * The POSIX Standard permits an implementation to choose whether or
         * not to omit the prefix character if the symbol is empty (the
         * placement position is meaningless if there is nothing to place).
         * glibc has chosen to always prefix an empty symbol by a minus (which
         * is the prefix for 'before' positioning).  FreeBSD has chosen to
         * return an empty string for an empty symbol.  Perl has always
         * emulated the glibc way (probably with little thought). */
        retval = "-";
        break;

#    endif
#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_NUMERIC_AVAIL_
#    if defined(USE_LOCALE_NUMERIC) && defined(HAS_LOCALECONV)
#      define NEED_USE_LOCALECONV

      case THOUSEP:
        cat_index = LC_NUMERIC_INDEX_;
        localeconv_key = THOUSANDS_SEP_LITERAL;
        localeconv_klen = STRLENs(THOUSANDS_SEP_LITERAL);
        goto use_localeconv;

#    else

      case THOUSEP:
        retval = C_thousands_sep;
        break;

#    endif

      case RADIXCHAR:

#    if defined(USE_LOCALE_NUMERIC) && defined(HAS_STRTOD)

       {
        /* khw knows of only three possible radix characters used in the world.
         * By far the two most common are comma and dot.  We can use strtod()
         * to quickly check for those without without much fuss.  If it is
         * something other than those two, the code drops down and lets
         * localeconv() find it.
         *
         * We don't have to toggle LC_CTYPE here because all locales Perl
         * supports are compatible with ASCII, which the two possibilities are.
         * */
        const char * orig_switched_locale = toggle_locale_c(LC_NUMERIC, locale);

        /* Comma tried first in case strtod() always accepts dot regardless of
         * the locale */
        if (strtod("1,5", NULL) > 1.4) {
            retval = ",";
        }
        else if (strtod("1.5", NULL) > 1.4) {
            retval = ".";
        }
        else {
            retval = NULL;
        }

        restore_toggled_locale_c(LC_NUMERIC, orig_switched_locale);

        if (retval) {
            break;
        }
       }

#    endif  /* Trying strtod() */

        /* If gets to here, the strtod() method wasn't compiled, or it failed;
         * drop down.
         *
         * (snprintf() used to be used instead of strtod(), but it was removed
         * as being somewhat more clumsy, and maybe non-conforming on some
         * platforms.  But before resorting to localeconv(), the code that was
         * removed by the strtod commit could be inserted here.  This seems
         * unlikely to be wanted unless some really broken localeconv() shows
         * up) */

#    if ! defined(USE_LOCALE_NUMERIC) || ! defined(HAS_LOCALECONV)

        retval = C_decimal_point;
        break;

#    else
#      define NEED_USE_LOCALECONV

        cat_index = LC_NUMERIC_INDEX_;
        localeconv_key = DECIMAL_POINT_LITERAL;
        localeconv_klen = STRLENs(DECIMAL_POINT_LITERAL);

#    endif
#  endif
#  ifdef NEED_USE_LOCALECONV

    /* These items are available from localeconv(). */

   /* case RADIXCHAR:   // May drop down to here in some configurations
      case THOUSEP:     // Jumps to here
      case CRNCYSTR:    // Jumps to here */
      use_localeconv:
       {

        /* The hash gets populated with just the field(s) related to 'item'. */
        HV * result_hv = my_localeconv(item);
        SV* string = hv_delete(result_hv, localeconv_key, localeconv_klen, 0);

#  ifdef USE_LOCALE_MONETARY

        if (item == CRNCYSTR) {

            /* CRNCYSTR localeconv() returns a slightly different value
             * than the nl_langinfo() API calls for, so have to modify this one
             * to conform.  We need another value from localeconv() to know
             * what to change it to.  my_localeconv() has populated the hash
             * with exactly both fields. */
            SV* precedes = hv_deletes(result_hv, P_CS_PRECEDES_LITERAL, 0);
            if (! precedes) {
                locale_panic_("my_localeconv() unexpectedly didn't return"
                              " a value for " P_CS_PRECEDES_LITERAL);
            }

            /* The modification is to prefix the localeconv() return with a
             * single byte, calculated as follows: */
            const char * prefix = (LIKELY(SvIV(precedes) != -1))
                                   ? ((precedes != 0) ?  "-" : "+")
                                   : ".";
            /* (khw couldn't find any documentation that the dot is signalled
             * by CHAR_MAX (which we modify to -1), but cygwin uses it thusly,
             * and it makes sense given that CHAR_MAX indicates the value isn't
             * used, so it neither precedes nor succeeds) */

            /* Perform the modification */
            sv_insert(string, 0, 0, prefix, 1);
        }

#  endif

        /* Here, 'string' contains the value we want to return, and the
         * hv_delete() has left it mortalized so its PV may be reused instead of
         * copied */
        sv_setsv_nomg(sv, string);
        retval_type = RETVAL_IN_sv;

        if (utf8ness) {
            is_utf8 = get_locale_string_utf8ness_i(SvPVX(sv),
                                                   LOCALE_UTF8NESS_UNKNOWN,
                                                   locale,
                                                   cat_index);
        }

        SvREFCNT_dec_NN(result_hv);
        break;
       }

#  endif  /* Using localeconv() for something or other */
#  undef NEED_USE_LOCALECONV
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_CTYPE_AVAIL_
#    ifndef USE_LOCALE_CTYPE

      case CODESET:
        retval = C_codeset;
        break;

#    else

      case CODESET:

        /* The trivial case */
        if (isNAME_C_OR_POSIX(locale)) {
            retval = C_codeset;
            break;
        }

        /* If this happens to match our cached value */
        if (PL_in_utf8_CTYPE_locale && strEQ(locale, PL_ctype_name)) {
            retval = "UTF-8";
            break;
        }

#      ifdef WIN32
#        ifdef WIN32_USE_FAKE_OLD_MINGW_LOCALES
#          define CODE_PAGE_FORMAT  "%s"
#          define CODE_PAGE_FUNCTION  nl_langinfo(CODESET)
#        else
#          define CODE_PAGE_FORMAT  "%d"

         /* This Windows function retrieves the code page.  It is subject to
          * change, but is documented, and has been stable for many releases */
#          define CODE_PAGE_FUNCTION  ___lc_codepage_func()
#        endif

        const char * orig_CTYPE_locale;
        orig_CTYPE_locale = toggle_locale_c(LC_CTYPE, locale);
        Perl_sv_setpvf(aTHX_ sv, CODE_PAGE_FORMAT, CODE_PAGE_FUNCTION);
        retval_type = RETVAL_IN_sv;

        /* We just assume the codeset is ASCII; no need to check for it being
         * UTF-8 */
        SvUTF8_off(sv);

        restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);

        DEBUG_Lv(PerlIO_printf(Perl_debug_log, "locale='%s' cp=%s\n",
                                               locale, SvPVX(sv)));
        break;

#      else   /* Below is ! Win32 */

        /* The codeset is important, but khw did not figure out a way for it to
         * be retrieved on non-Windows boxes without nl_langinfo().  But even
         * if we can't get it directly, we can usually determine if it is a
         * UTF-8 locale or not.  If it is UTF-8, we (correctly) use that for
         * the code set. */

#        ifdef HAS_DEFINITIVE_UTF8NESS_DETERMINATION

        if (is_locale_utf8(locale)) {
            retval = "UTF-8";
            break;
        }

#        endif

        /* Here, the code set has not been found.  The only other option khw
         * could think of is to see if the codeset is part of the locale name.
         * This is very less than ideal; often there is no code set in the
         * name; and at other times they even lie.
         *
         * But there is an XPG standard syntax, which many locales follow:
         *
         *    language[_territory[.codeset]][@modifier]
         *
         * So we take the part between the dot and any '@' */
        const char * name;
        name = strchr(locale, '.');
        if (! name) {
            retval = "";  /* Alas, no dot */
        }
        else {

            /* Don't include the dot */
            name++;

            /* The code set name is considered to be everything between the dot
             * and any '@', so stop before any '@' */
            const char * modifier = strchr(name, '@');
            if (modifier) {
                sv_setpvn(sv, name, modifier - name);
            }
            else {
                sv_setpv(sv, name);
            }
            SvUTF8_off(sv);

            retval_type = RETVAL_IN_sv;
        }

#        ifndef HAS_DEFINITIVE_UTF8NESS_DETERMINATION

        /* Here, 'retval' contains any codeset name derived from the locale
         * name.  That derived name may be empty or not necessarily indicative
         * of the real codeset.  But we can often determine if it should be
         * UTF-8, regardless of what the name is.  On most platforms, that
         * determination is definitive, and was already done.  But for this
         * code to be compiled, this platform is not one of them.  However,
         * there are typically tools available to make a very good guess, and
         * knowing the derived codeset name improves the quality of that guess.
         * The following function overrides the derived codeset name when it
         * guesses that it actually should be UTF-8.  It could be inlined here,
         * but was moved out of this switch() so as to make the switch()
         * control flow easier to follow */
        if (isRETVAL_IN_sv(retval_type)) {
            retval = SvPVX_const(sv);
            retval_type = RETVAL_IN_BOTH;
        }

        if (S_maybe_override_codeset(aTHX_ retval, locale, &retval)) {
            retval_type = RETVAL_IN_retval;
        }

#        endif

        break;

#      endif    /* ! WIN32 */
#    endif      /* USE_LOCALE_CTYPE */
#  endif

   /* The _NL_foo items are mostly empty; the rest are copied from Ubuntu C
    * locale values.  khw fairly arbitrarily decided which of its non-empty
    * values to copy and which to change to empty.  All the numeric ones needed
    * some value */

#  if ! defined(HAS_SOME_LANGINFO) || ! LC_ADDRESS_AVAIL_

      case _NL_ADDRESS_POSTAL_FMT:
      case _NL_ADDRESS_COUNTRY_NAME:
      case _NL_ADDRESS_COUNTRY_POST:
      case _NL_ADDRESS_COUNTRY_AB2:
      case _NL_ADDRESS_COUNTRY_AB3:
      case _NL_ADDRESS_COUNTRY_CAR:
      case _NL_ADDRESS_COUNTRY_ISBN:
      case _NL_ADDRESS_LANG_NAME:
      case _NL_ADDRESS_LANG_AB:
      case _NL_ADDRESS_LANG_TERM:
      case _NL_ADDRESS_LANG_LIB:
        retval = "";
        break;

      case _NL_ADDRESS_COUNTRY_NUM:
        sv_setuv(sv, 0);
        retval_type = RETVAL_IN_sv;
        break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_IDENTIFICATION_AVAIL_

      case _NL_IDENTIFICATION_ADDRESS:
      case _NL_IDENTIFICATION_CONTACT:
      case _NL_IDENTIFICATION_EMAIL:
      case _NL_IDENTIFICATION_TEL:
      case _NL_IDENTIFICATION_FAX:
      case _NL_IDENTIFICATION_LANGUAGE:
      case _NL_IDENTIFICATION_AUDIENCE:
      case _NL_IDENTIFICATION_APPLICATION:
      case _NL_IDENTIFICATION_ABBREVIATION:
        retval = "";
        break;

      case _NL_IDENTIFICATION_DATE:     retval = "1997-12-20"; break;
      case _NL_IDENTIFICATION_REVISION: retval = "1.0"; break;
      case _NL_IDENTIFICATION_CATEGORY: retval = "i18n:1999"; break;
      case _NL_IDENTIFICATION_TERRITORY:retval = "ISO"; break;

      case _NL_IDENTIFICATION_TITLE:
        retval = "ISO/IEC 14652 i18n FDCC-set";
        break;

      case _NL_IDENTIFICATION_SOURCE:
        retval = "ISO/IEC JTC1/SC22/WG20 - internationalization";
        break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_MEASUREMENT_AVAIL_

      case _NL_MEASUREMENT_MEASUREMENT:
        sv_setuv(sv, 1);
        retval_type = RETVAL_IN_sv;
        break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_NAME_AVAIL_

      case _NL_NAME_NAME_FMT:
      case _NL_NAME_NAME_GEN:
      case _NL_NAME_NAME_MR:
      case _NL_NAME_NAME_MRS:
      case _NL_NAME_NAME_MISS:
      case _NL_NAME_NAME_MS:
        retval = "";
        break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_PAPER_AVAIL_

      case _NL_PAPER_HEIGHT:
        sv_setuv(sv, 297);
        retval_type = RETVAL_IN_sv;
        break;

      case _NL_PAPER_WIDTH:
        sv_setuv(sv, 210);
        retval_type = RETVAL_IN_sv;
        break;

#  endif
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_TELEPHONE_AVAIL_

      case _NL_TELEPHONE_INT_SELECT:
      case _NL_TELEPHONE_INT_PREFIX:
      case _NL_TELEPHONE_TEL_DOM_FMT:
        retval = "";
        break;

      case _NL_TELEPHONE_TEL_INT_FMT:
        retval = "+%c %a %l";
        break;

#  endif

   /* When we have to emulate TIME-related items, this bit of code is compiled
    * to have the default: case be a nested switch() which distinguishes
    * between legal inputs and unknown ones.  This bit does initialization and
    * then at the end calls switch().  But when we aren't emulating TIME, by
    * the time we get to here all legal inputs have been handled above, and it
    * is cleaner to not have a nested switch().  So this bit of code is skipped
    * and the other-wise nested default: case is compiled as part of the outer
    * (and actually only) switch() */
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_TIME_AVAIL_

      default:  /* Anything else that is legal is LC_TIME-related */
       {

        const char * format = NULL;
        retval = NULL;

#    ifdef HAS_STRFTIME

        bool return_format = FALSE;

        /* Without strftime(), default compiled-in values are returned.
         * Otherwise, we generally compute a date as explained below.
         * Initialize default values for that computation */
        int mon = 0;
        int mday = 1;
        int hour = 6;

#    endif

        /* Nested switch for LC_TIME items, plus the default: case is for
         * unknown items */
        switch (item) {

#  endif    /* ! defined(HAS_SOME_LANGINFO) || ! LC_TIME_AVAIL_ */

          default:

            /* On systems with langinfo.h, 'item' is an enum.  If we don't
             * handle one of those, the code needs to change to be able to do
             * so.  But otherwise, the parameter can be any int, and so could
             * be a garbage value and all we can do is to return that it is
             * invalid. */;
#  if defined(I_LANGINFO)

            Perl_croak_nocontext("panic: Unexpected nl_langinfo() item %jd",
                                 item);

#  else
            assert(item < 0);   /* Make sure using perl_langinfo.h */
            SET_EINVAL;
            retval = "";
            break;
#  endif

   /* Back to the nested switch() */
#  if ! defined(HAS_SOME_LANGINFO) || ! LC_TIME_AVAIL_

            /* The case: statments in this switch are all for LC_TIME related
             * values.  There are four types of values returned.  One type is
             * "Give me the name in this locale of the 3rd month of the year"
             * (March in an English locale).  The second main type is "Give me
             * the best format string understood by strftime(), like '%c', for
             * formatting the date and time in this locale."  The other two
             * types are for ERA and ALT_DIGITS, and are explained at the case
             * statements for them.
             *
             * For the first type, suppose we want to find the name of the 3rd
             * month of the year.  We pass a date/time to strftime() that is
             * known to evaluate to sometime in March, along with a format that
             * tells strftime() to return the month's name.  We then return
             * that to our caller.  Similarly for the names of the days of the
             * week, like "Tuesday".  There are also abbreviated versions for
             * each of these.
             *
             * To implement the second type (returning to the caller a string
             * containing a format suitable for passing to strftime() ) we
             * guess a format, pass that to strftime, and examine its return to
             * see if that format is known on this platform.  If so, we return
             * that guess.  Otherwise we return the empty string "".  There are
             * no second guesses, as there don't seem to be alternatives
             * lurking out there.  For some formats that are supposed to be
             * known to all strftime()s since C89, we just assume that they are
             * valid, not bothering to check.  The guesses may not be the best
             * available for this locale on this platform, but should be good
             * enough, so that a native speaker would find them understandable.
             * */

            /* Unimplemented by perl; for use with strftime() %E modifier */
          case ERA: retval = ""; break;

#    if ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)

          case AM_STR: retval = "AM"; break;
          case PM_STR: retval = "PM"; break;
#    else
          case PM_STR: hour = 18;
          case AM_STR:
            format = "%p";
            break;
#    endif
#    if ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)

          case ABDAY_1: retval = "Sun"; break;
          case ABDAY_2: retval = "Mon"; break;
          case ABDAY_3: retval = "Tue"; break;
          case ABDAY_4: retval = "Wed"; break;
          case ABDAY_5: retval = "Thu"; break;
          case ABDAY_6: retval = "Fri"; break;
          case ABDAY_7: retval = "Sat"; break;
#    else
          case ABDAY_7: mday++;
          case ABDAY_6: mday++;
          case ABDAY_5: mday++;
          case ABDAY_4: mday++;
          case ABDAY_3: mday++;
          case ABDAY_2: mday++;
          case ABDAY_1:
            format = "%a";
            break;
#    endif
#    if ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)

          case DAY_1: retval = "Sunday";    break;
          case DAY_2: retval = "Monday";    break;
          case DAY_3: retval = "Tuesday";   break;
          case DAY_4: retval = "Wednesday"; break;
          case DAY_5: retval = "Thursday";  break;
          case DAY_6: retval = "Friday";    break;
          case DAY_7: retval = "Saturday";  break;
#    else
          case DAY_7: mday++;
          case DAY_6: mday++;
          case DAY_5: mday++;
          case DAY_4: mday++;
          case DAY_3: mday++;
          case DAY_2: mday++;
          case DAY_1:
            format = "%A";
            break;
#    endif
#    if ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)
          case ABMON_1:  retval = "Jan"; break;
          case ABMON_2:  retval = "Feb"; break;
          case ABMON_3:  retval = "Mar"; break;
          case ABMON_4:  retval = "Apr"; break;
          case ABMON_5:  retval = "May"; break;
          case ABMON_6:  retval = "Jun"; break;
          case ABMON_7:  retval = "Jul"; break;
          case ABMON_8:  retval = "Aug"; break;
          case ABMON_9:  retval = "Sep"; break;
          case ABMON_10: retval = "Oct"; break;
          case ABMON_11: retval = "Nov"; break;
          case ABMON_12: retval = "Dec"; break;
#    else
          case ABMON_12: mon++;
          case ABMON_11: mon++;
          case ABMON_10: mon++;
          case ABMON_9:  mon++;
          case ABMON_8:  mon++;
          case ABMON_7:  mon++;
          case ABMON_6:  mon++;
          case ABMON_5:  mon++;
          case ABMON_4:  mon++;
          case ABMON_3:  mon++;
          case ABMON_2:  mon++;
          case ABMON_1:
            format = "%b";
            break;
#    endif
#    if ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)

          case MON_1:  retval = "January";  break;
          case MON_2:  retval = "February"; break;
          case MON_3:  retval = "March";    break;
          case MON_4:  retval = "April";    break;
          case MON_5:  retval = "May";      break;
          case MON_6:  retval = "June";     break;
          case MON_7:  retval = "July";     break;
          case MON_8:  retval = "August";   break;
          case MON_9:  retval = "September";break;
          case MON_10: retval = "October";  break;
          case MON_11: retval = "November"; break;
          case MON_12: retval = "December"; break;
#    else
          case MON_12: mon++;
          case MON_11: mon++;
          case MON_10: mon++;
          case MON_9:  mon++;
          case MON_8:  mon++;
          case MON_7:  mon++;
          case MON_6:  mon++;
          case MON_5:  mon++;
          case MON_4:  mon++;
          case MON_3:  mon++;
          case MON_2:  mon++;
          case MON_1:
            format = "%B";
            break;
#    endif
#    ifndef HAS_STRFTIME

          /* If no strftime() on this system, no format will be recognized, so
           * return empty */
          case D_FMT:  case T_FMT:  case D_T_FMT:
          case ERA_D_FMT: case ERA_T_FMT: case ERA_D_T_FMT:
          case T_FMT_AMPM:
            retval = "";
            break;
#    else
          /* These strftime formats are defined by C89, so we assume that
           * strftime supports them, and so are returned unconditionally; they
           * may not be what the locale actually says, but should give good
           * enough results for someone using them as formats (as opposed to
           * trying to parse them to figure out what the locale says).  The
           * other format items are actually tested to verify they work on the
           * platform */
          case D_FMT:   retval = "%x"; break;
          case T_FMT:   retval = "%X"; break;
          case D_T_FMT: retval = "%c"; break;

          /* This format isn't in C89; test that it actually works on the
           * platform */
          case T_FMT_AMPM:
            format = "%r";
            return_format = TRUE;
            break;

#      if defined(WIN32) || ! defined(USE_LOCALE_TIME)

          /* strftime() on Windows doesn't have the POSIX (beyond C89)
           * extensions that would allow it to recover these, so use the plain
           * non-ERA formats.  Also, when LC_TIME is constrained to the C
           * locale, the %E modifier is useless, so don't return it. */
          case ERA_D_FMT:   retval = "%x"; break;
          case ERA_T_FMT:   retval = "%X"; break;
          case ERA_D_T_FMT: retval = "%c"; break;
#      else
          case ERA_D_FMT:
            format = "%Ex";
            return_format = TRUE;   /* Test that this works on the platform */
            break;

          case ERA_T_FMT:
            format = "%EX";
            return_format = TRUE;
            break;

          case ERA_D_T_FMT:
            format = "%Ec";
            return_format = TRUE;
            break;
#      endif
#    endif
#    if defined(WIN32) || ! defined(USE_LOCALE_TIME) || ! defined(HAS_STRFTIME)

          case ALT_DIGITS: retval = ""; break;
#    else
#      define CAN_BE_ALT_DIGITS

          case ALT_DIGITS:
            format = "%Ow"; /* Find the alternate digit for 0 */
            break;
#    endif

        } /* End of inner switch() */

        /* The inner switch() above has set 'retval' iff that is the final
         * answer */
        if (retval) {
            break;
        }

        /* And it hasn't set 'format' iff it can't figure out a good value on
         * this platform. */
        if (! format) {
            retval = "";
            break;
        }

#    ifdef HAS_STRFTIME

        /* Here we have figured out what to call strftime() with */

        struct tm  mytm;
        const char * orig_TIME_locale
                            = toggle_locale_c_unless_locking(LC_TIME, locale);

        /* The year was deliberately chosen so that January 1 is on the
        * first day of the week.  Since we're only getting one thing at a
        * time, it all works */
        ints_to_tm(&mytm, locale, 30, 30, hour, mday, mon, 2011, 0, 0, 0);
        bool succeeded;
        if (utf8ness) {
            succeeded = strftime8(format,
                                  sv,
                                  locale,
                                  &mytm,

                                  /* All possible formats specified above are
                                   * entirely ASCII */
                                  UTF8NESS_IMMATERIAL,

                                  &is_utf8,
                                  false    /* not calling from sv_strftime */
                              );
        }
        else {
            succeeded = strftime_tm(format, sv, locale, &mytm);
        }

        restore_toggled_locale_c_unless_locking(LC_TIME, orig_TIME_locale);

        if (UNLIKELY(! succeeded)) {
            retval = "";
            break;
        }

#      ifdef CAN_BE_ALT_DIGITS

        if (LIKELY(item != ALT_DIGITS))

#      endif

        {

            /* If to return what strftime() returns, are done */
            if (! return_format) {
                retval_type = RETVAL_IN_sv;
                break;
            }

            /* Here are to return the format, not the value.  This is used when
             * we are testing if the format we expect to return is legal on
             * this platform.  We have passed the format, say "%r, to
             * strftime(), and now have in 'sv' what strftime processed it
             * to be.  But the caller doesnt't want that; it wants the actual
             * %r, if it is understood on this platform, and "" if it isn't.
             * Some strftime()s return "" for an unknown format.  (None of the
             * formats exposed by langinfo can have "" be a legal result.)
             * Other strftime()s return the format unchanged if not understood.
             * So if we pass "%r" to strftime(), and it's illegal, we will get
             * back either "" or "%r", and we return "" to our caller.  If the
             * strftime() return is anything else, we conclude that "%r" is
             * understood by the platform, and return "%r". */
            if (strEQ(SvPVX(sv), format)) {
                retval = "";
            }
            else {
                retval = format;
            }

            /* A format is always in ASCII */
            is_utf8 = UTF8NESS_IMMATERIAL;

            break;
        }

#      ifdef CAN_BE_ALT_DIGITS

        /* Here, the item is 'ALT_DIGITS' and 'sv' contains the zeroth
         * alternate digit.  If empty, return that there aren't alternate
         * digits */
        Size_t alt0_len = SvCUR(sv);
        if (alt0_len == 0) {
            retval_type = RETVAL_IN_sv;
            break;
        }

        /* ALT_DIGITS requires special handling because it requires up to 100
         * values.  Below we generate those by using the %O modifier to
         * strftime() formats.
         *
         * We already have the alternate digit for zero in 'sv', generated
         * using the %Ow format, which was used because it seems least likely
         * to have a leading zero.  But some locales return the equivalent of
         * 00 anyway.  If the first half of 'sv' is identical to the second
         * half, assume that is the case, and use just the first half */
        if ((alt0_len & 1) == 0) {
            Size_t half_alt0_len = alt0_len / 2;
            if (strnEQ(SvPVX(sv), SvPVX(sv) + half_alt0_len, half_alt0_len)) {
                alt0_len = half_alt0_len;
                SvCUR_set(sv, alt0_len);
            }
        }

        sv_catpvn_nomg (sv, ";", 1);

        /* Many of the remaining digits have representations that include at
         * least two 0-sized strings */
        SV* alt_dig_sv = newSV(2 * alt0_len);

        /* Various %O formats can be used to derive the alternate digits.  Only
         * %Oy can go up to the full 100 values.  If it doesn't work, we try
         * various fallbacks in decreasing order of how many values they can
         * deliver.  maxes[] tells the highest value that the format applies
         * to; offsets[] compensates for 0-based vs 1-based indices; and vars[]
         * holds what field in the 'struct tm' to applies to the corresponding
         * format */
        int year, min, sec;
      const char  * fmts[] = {"%Oy", "%OM", "%OS", "%Od", "%OH", "%Om", "%Ow" };
      const Size_t maxes[] = {  99,    59,    59,    31,    23,    11,    6   };
      const int  offsets[] = {   0,     0,     0,     1,     0,     1,    0   };
      int         * vars[] = {&year,  &min,  &sec,  &mday, &hour, &mon, &mday };
        Size_t j = 0;   /* Current index into the above tables */

        orig_TIME_locale = toggle_locale_c_unless_locking(LC_TIME, locale);

        for (unsigned int i = 1; i <= 99; i++) {
            struct tm  mytm;

          redo:
            if (j >= C_ARRAY_LENGTH(fmts)) {
                break;  /* Exhausted formats early; can't continue */
            }

            if (i > maxes[j]) {
                j++;    /* Exhausted this format; try next one */
                goto redo;
            }

            year = (strchr(fmts[j], 'y')) ? 1900 : 2011;
            hour = 0;
            min = 0;
            sec = 0;
            mday = 1;
            mon = 0;

            /* Change the variable corresponding to this format to the
            * current time being run in 'i' */
            *(vars[j]) += i - offsets[j];

            /* Do the strftime.  Once we have determined the UTF8ness (if
            * we want it), assume the rest will be the same, and use
            * strftime_tm(), which doesn't recalculate UTF8ness */
            ints_to_tm(&mytm, locale, sec, min, hour, mday, mon, year, 0, 0, 0);
            if (utf8ness && is_utf8 != UTF8NESS_NO && is_utf8 != UTF8NESS_YES) {
                succeeded = strftime8(fmts[j],
                                      alt_dig_sv,
                                      locale,
                                      &mytm,
                                      UTF8NESS_IMMATERIAL,
                                      &is_utf8,
                                      false   /* not calling from sv_strftime */
                                     );
            }
            else {
                succeeded = strftime_tm(fmts[j], alt_dig_sv, locale, &mytm);
            }

            /* If didn't recognize this format, try the next */
            if (UNLIKELY(! succeeded)) {
                j++;
                goto redo;
            }

            const char * current = SvPVX(alt_dig_sv);

            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                                "i=%d, format=%s, alt='%s'\n",
                                i, fmts[j], current));


            /* If it returned regular digits, give up on this format, to try
             * the next candidate one */
            if (strpbrk(current, "0123456789")) {
                j++;
                goto redo;
            }

            /* If there is a leading alternate zero, skip past it, to get the
             * second one in the string.  The first 'alt0_len' bytes in 'sv'
             * will be the alternate-zero representation */
            if (strnEQ(current, SvPVX(sv), alt0_len)) {
                current += alt0_len;
            }

            /* Append this number to the ongoing list, including the separator.
             * */
            sv_catpv_nomg (sv, current);
            sv_catpvn_nomg (sv, ";", 1);
        } /* End of loop generating ALT_DIGIT strings */

        /* Above we accepted 0 for alt-0 in case the locale doesn't have a
         * zero, but we rejected any other ASCII digits.  Now that we have
         * processed everything, if that 0 is the only thing we found, it was a
         * false positive, and the locale doesn't have alternate digits */
        if (SvCUR(sv) == alt0_len + 1) {
            SvCUR_set(sv, 0);
        }

        SvREFCNT_dec_NN(alt_dig_sv);

        restore_toggled_locale_c_unless_locking(LC_TIME, orig_TIME_locale);

        retval_type = RETVAL_IN_sv;
        break;

#      endif    /* End of CAN_BE_ALT_DIGITS */
#    endif      /* End of HAS_STRFTIME */

       }    /* End of braced group for outer switch 'default:' case */

#  endif

    } /* Giant switch() of nl_langinfo() items */

    GCC_DIAG_RESTORE_STMT;

    if (sv != PL_scratch_langinfo) {    /* Caller wants return in 'sv' */
        if (! isRETVAL_IN_sv(retval_type)) {
            sv_setpv(sv, retval);
            SvUTF8_off(sv);
        }

        if (utf8ness) {
            *utf8ness = is_utf8;
            if (is_utf8 == UTF8NESS_YES) {
                SvUTF8_on(sv);
            }
        }

        DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                         "Leaving emulate_langinfo item=%jd, using locale %s\n",
                         item, locale));

        /* The caller shouldn't also be wanting a 'retval'; make sure segfaults
         * if they call this wrong */
        return NULL;
    }

    /* Here, wants a 'retval' return.  Extract that if not already there. */
    if (! isRETVAL_IN_retval(retval_type)) {
        retval = SvPV_nolen(sv);
    }

    /* Here, 'retval' started as a simple value, or has been converted into
     * being simple */
    if (utf8ness) {
        *utf8ness = is_utf8;
    }

    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                         "Leaving emulate_langinfo item=%jd, using locale %s\n",
                         item, locale));
    return retval;

#  undef RETVAL_IN_retval
#  undef RETVAL_IN_BOTH
#  undef RETVAL_IN_sv
#  undef isRETVAL_IN_sv
#  undef isRETVAL_IN_retval

}

#endif      /* Needs emulate_langinfo() */
#ifndef HAS_DEFINITIVE_UTF8NESS_DETERMINATION

STATIC bool
S_maybe_override_codeset(pTHX_ const char * codeset,
                               const char * locale,
                               const char ** new_codeset)
{
#  define NAME_INDICATES_UTF8       0x1
#  define MB_CUR_MAX_SUGGESTS_UTF8  0x2

    /* Override 'codeset' with UTF-8 if this routine guesses that it should be.
     * Conversely (but rarely), "UTF-8" in the locale name might be wrong.  We
     * return "" as the code set name if we find that to be the case.  */

    unsigned int lean_towards_being_utf8 = 0;
    if (is_codeset_name_UTF8(codeset)) {
        lean_towards_being_utf8 |= NAME_INDICATES_UTF8;
    }

    const char * orig_CTYPE_locale = toggle_locale_c(LC_CTYPE, locale);

    /* For this portion of the file to compile, some C99 functions aren't
     * available to us, even though we now require C99.  So, something must be
     * wrong with them.  The code here should be good enough to work around
     * this issue, but should the need arise, comments in S_is_locale_utf8()
     * list some alternative C99 functions that could be tried.
     *
     * But MB_CUR_MAX is a C89 construct that helps a lot, is simple for a
     * vendor to implement, and our experience with it is that it works well on
     * a variety of platforms.  We have found that it returns a too-large
     * number on some platforms for the C locale, but for no others.  That
     * locale was already ruled out in the code that called this function.  (If
     * MB_CUR_MAX returned too small a number, that would break a lot of
     * things, and likely would be quickly corrected by the vendor.)  khw has
     * some confidence that it doesn't return >1 when 1 is meant, as that would
     * trigger a Perl warning, and we've had no reports of invalid occurrences
     * of such. */
#  ifdef MB_CUR_MAX

    /* If there are fewer bytes available in this locale than are required to
     * represent the largest legal UTF-8 code point, this definitely isn't a
     * UTF-8 locale, even if the locale name says it is. */
    const int mb_cur_max = MB_CUR_MAX;
    if (mb_cur_max < (int) UNISKIP(PERL_UNICODE_MAX)) {
        restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);

        if (lean_towards_being_utf8 & NAME_INDICATES_UTF8) {
            *new_codeset = "";    /* The name is wrong; override */
            return true;
        }

        return false;
    }

    /* But if the locale could be UTF-8, and also the name corroborates this,
     * assume it is so */
    if (lean_towards_being_utf8 & NAME_INDICATES_UTF8) {
        restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);
        return false;
    }

    restore_toggled_locale_c_if_locking(LC_CTYPE, orig_CTYPE_locale);

    /* Here, the name doesn't indicate UTF-8, but MB_CUR_MAX indicates it could
     * be.  khw knows of only two other locales in the world, EUC-TW and GB
     * 18030, that legitimately require this many bytes (4).  So, if the name
     * is one of those, MB_CUR_MAX has corroborated that. */
    bool name_implies_non_utf8 = false;
    if (foldEQ(codeset, "GB", 2)) {
        const char * s = codeset + 2;
        if (*s == '-' || *s == '_') {
            s++;
        }

        if strEQ(s, "18030") {
            name_implies_non_utf8 = true;
        }
    }
    else if (foldEQ(codeset, "EUC", 3)) {
        const char * s = codeset + 3;
        if (*s == '-' || *s == '_') {
            s++;
        }

        if (foldEQ(s, "TW", 2)) {
            name_implies_non_utf8 = true;
        }
    }

    /* Otherwise, the locale is likely UTF-8 */
    if (! name_implies_non_utf8) {
        lean_towards_being_utf8 |= MB_CUR_MAX_SUGGESTS_UTF8;
    }

    /* (In both those two other multibyte locales, the single byte characters
     * are the same as ASCII.  No multi-byte character in EUC-TW is legal UTF-8
     * (since the first byte of each is a continuation).  GB 18030 has no three
     * byte sequences, and none of the four byte ones is legal UTF-8 (as the
     * second byte for these is a non-continuation).  But every legal UTF-8 two
     * byte sequence is also legal in GB 18030, though none have the same
     * meaning, and no Han code point expressed in UTF-8 is two byte.  So the
     * further tests below which look for native expressions of currency and
     * time will not return two byte sequences, hence they will reliably rule
     * out such a locale as being UTF-8, even if the code set name checked
     * above isn't correct.) */

#  endif    /* has MB_CUR_MAX */

    /* Here, MB_CUR_MAX is not available, or was inconclusive.  What we do is
     * to look at various strings associated with the locale:
     *  1)  If any are illegal UTF-8, the locale can't be UTF-8.
     *  2)  If all are legal UTF-8, and some non-ASCII characters are present,
     *      it is likely to be UTF-8, because of the strictness of UTF-8
     *      syntax. So assume it is UTF-8
     *  3)  If all are ASCII and the locale name and/or MB_CUR_MAX indicate
     *      UTF-8, assume the locale is UTF-8.
     *  4)  Otherwise, assume the locale isn't UTF-8
     *
     * To save cycles, if the locale name indicates it is a UTF-8 locale, we
     * stop looking at the first instance with legal non-ASCII UTF-8.  It is
     * very unlikely this combination is coincidental. */

    utf8ness_t strings_utf8ness = UTF8NESS_UNKNOWN;

    /* List of strings to look at */
    const int trials[] = {

#  if defined(USE_LOCALE_MONETARY) && defined(HAS_LOCALECONV)

        /* The first string tried is the locale currency name.  Often that will
         * be in the native script.
         *
         * But this is usable only if localeconv() is available, as that's the
         * way we find out the currency symbol. */

        CRNCYSTR,

#  endif
#  ifdef USE_LOCALE_TIME

    /* We can also try various strings associated with LC_TIME, like the names
     * of months or days of the week */

        DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7,
        MON_1, MON_2, MON_3, MON_4, MON_5, MON_6, MON_7, MON_8,
                                    MON_9, MON_10, MON_11, MON_12,
        ALT_DIGITS, AM_STR, PM_STR,
        ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7,
        ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6,
        ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12

#  endif

    };

#  ifdef USE_LOCALE_TIME

    /* The code in the recursive call below can handle switching the locales,
     * but by doing it now here, that code will check and discover that there
     * is no need to switch then restore, avoiding those each loop iteration.
     *
     * But don't do this if toggling actually creates a critical section, so as
     * to minimize the amount of time spent in each critical section. */
    const char * orig_TIME_locale =
                                toggle_locale_c_unless_locking(LC_TIME, locale);

#  endif

    /* The trials array may consist of strings from two different locale
     * categories.  The call to langinfo_i() below needs to pass the proper
     * category for each string.  There is a max of 1 trial for LC_MONETARY;
     * the rest are LC_TIME.  So the array is arranged so the LC_MONETARY item
     * (if any) is first, and all subsequent iterations will use LC_TIME.
     * These #ifdefs set up the values for all possible combinations. */
#  if defined(USE_LOCALE_MONETARY) && defined(HAS_LOCALECONV)

    locale_category_index  cat_index = LC_MONETARY_INDEX_;

#    ifdef USE_LOCALE_TIME

    const locale_category_index  follow_on_cat_index = LC_TIME_INDEX_;
    assert(trials[1] == DAY_1); /* Make sure only a single non-time entry */

#    else

    /* Effectively out-of-bounds, as there is only the monetary entry */
    const locale_category_index  follow_on_cat_index = LC_ALL_INDEX_;

#    endif
#  elif defined(USE_LOCALE_TIME)

    locale_category_index  cat_index = LC_TIME_INDEX_;
    const locale_category_index  follow_on_cat_index = LC_TIME_INDEX_;

#  else

    /* Effectively out-of-bounds, as here there are no trial entries at all.
     * This allows this code to compile, but there are no strings to test, and
     * so the answer will always be non-UTF-8. */
    locale_category_index  cat_index = LC_ALL_INDEX_;
    const locale_category_index  follow_on_cat_index = LC_ALL_INDEX_;

#  endif

    /* We will need to use the reentrant interface. */
    SV * sv = newSVpvs("");

    /* Everything set up; look through all the strings */
    for (PERL_UINT_FAST8_T i = 0; i < C_ARRAY_LENGTH(trials); i++) {

        /* To prevent infinite recursive calls, we don't ask for the UTF-8ness
         * of the string.  Instead we examine the result below */
        langinfo_sv_i(trials[i], cat_index, locale, sv, NULL);

        cat_index = follow_on_cat_index;

        const char * result = SvPVX(sv);
        const Size_t len = strlen(result);
        const U8 * first_variant;

        /* If the string is identical whether or not it is encoded as UTF-8, it
         * isn't helpful in determining UTF8ness. */
        if (is_utf8_invariant_string_loc((U8 *) result, len, &first_variant))
        {
            continue;
        }

        /* Here, has non-ASCII.  If not legal UTF-8, isn't a UTF-8 locale */
        if (! is_strict_utf8_string(first_variant,
                                    len - (first_variant - (U8 *) result)))
        {
            strings_utf8ness = UTF8NESS_NO;
            break;
        }

        /* Here, is a legal non-ASCII UTF-8 string; tentatively set the return
         * to YES; possibly overridden by later iterations */
        strings_utf8ness = UTF8NESS_YES;

        /* But if this corroborates our expectation, quit now */
        if (lean_towards_being_utf8 & NAME_INDICATES_UTF8) {
            break;
        }
    }

#  ifdef USE_LOCALE_TIME

    restore_toggled_locale_c_unless_locking(LC_TIME, orig_TIME_locale);

#  endif

    restore_toggled_locale_c_unless_locking(LC_CTYPE, orig_CTYPE_locale);

    if (strings_utf8ness == UTF8NESS_NO) {
        return false;     /* No override */
    }

    /* Here all tested strings are legal UTF-8.
     *
     * Above we set UTF8NESS_YES if any string wasn't ASCII.  But even if they
     * are all ascii, and the locale name indicates it is a UTF-8 locale,
     * assume the locale is UTF-8. */
    if (lean_towards_being_utf8) {
        strings_utf8ness = UTF8NESS_YES;
    }

    if (strings_utf8ness == UTF8NESS_YES) {
        *new_codeset = "UTF-8";
        return true;
    }

    /* Here, nothing examined indicates that the codeset is or isn't UTF-8.
     * But what is it?  The other locale categories are not likely to be of
     * further help:
     *
     * LC_NUMERIC   Only a few locales in the world have a non-ASCII radix or
     *              group separator.
     * LC_CTYPE     This code wouldn't be compiled if mbtowc() existed and was
     *              reliable.  This is unlikely in C99.  There are other
     *              functions that could be used instead, but are they going to
     *              exist, and be able to distinguish between UTF-8 and 8859-1?
     *              Deal with this only if it becomes necessary.
     * LC_MESSAGES  The strings returned from strerror() would seem likely
     *              candidates, but experience has shown that many systems
     *              don't actually have translations installed for them.  They
     *              are instead always in English, so everything in them is
     *              ASCII, which is of no help to us.  A Configure probe could
     *              possibly be written to see if this platform has non-ASCII
     *              error messages.  But again, wait until it turns out to be
     *              an actual problem.
     *
     *              Things like YESSTR, NOSTR, might not be in ASCII, but need
     *              nl_langinfo() to access, which we don't have.
     */

    /* Otherwise, assume the locale isn't UTF-8.  This can be wrong if we don't
     * have MB_CUR_MAX, and the locale is English without UTF-8 in its name,
     * and with a dollar currency symbol. */
    return false;     /* No override */
}

#  endif /* ! HAS_DEFINITIVE_UTF8NESS_DETERMINATION */

/*
=for apidoc_section $time
=for apidoc      sv_strftime_tm
=for apidoc_item my_strftime

These implement the libc strftime().

On failure, they return NULL, and set C<errno> to C<EINVAL>.

C<sv_strftime_tm> is preferred, as it transparently handles the UTF-8ness of
the current locale, the input C<fmt>, and the returned result.  Only if the
current C<LC_TIME> locale is a UTF-8 one (and S<C<use bytes>> is not in effect)
will the result be marked as UTF-8.

C<sv_strftime_tm> takes a pointer to a filled-in S<C<struct tm>> parameter.  It
ignores the values of the C<wday> and C<yday> fields in it.  The other fields
give enough information to accurately calculate these values, and are used for
that purpose.

The caller assumes ownership of the returned SV with a reference count of 1.

C<my_strftime> is kept for backwards compatibility.  Knowing if its result
should be considered UTF-8 or not requires significant extra logic.

The return value is a pointer to the formatted result (which MUST be arranged
to be FREED BY THE CALLER).  This allows this function to increase the buffer
size as needed, so that the caller doesn't have to worry about that, unlike
libc C<strftime()>.

The C<wday>, C<yday>, and C<isdst> parameters are ignored by C<my_strftime>.
Daylight savings time is never considered to exist, and the values returned for
the other two fields (if C<fmt> even calls for them) are calculated from the
other parameters, without need for referencing these.

Note that both functions are always executed in the underlying
C<LC_TIME> locale of the program, giving results based on that locale.

=cut
 */

char *
Perl_my_strftime(pTHX_ const char *fmt, int sec, int min, int hour,
                       int mday, int mon, int year, int wday, int yday,
                       int isdst)
{   /* Documented above */
    PERL_ARGS_ASSERT_MY_STRFTIME;

#ifdef USE_LOCALE_TIME
    const char * locale = querylocale_c(LC_TIME);
#else
    const char * locale = "C";
#endif

    struct tm  mytm;
    ints_to_tm(&mytm, locale, sec, min, hour, mday, mon, year, wday, yday,
               isdst);
    if (! strftime_tm(fmt, PL_scratch_langinfo, locale, &mytm)) {
        return NULL;
    }

    return savepv(SvPVX(PL_scratch_langinfo));
}

SV *
Perl_sv_strftime_ints(pTHX_ SV * fmt, int sec, int min, int hour,
                            int mday, int mon, int year, int wday,
                            int yday, int isdst)
{   /* Documented above */
    PERL_ARGS_ASSERT_SV_STRFTIME_INTS;

#ifdef USE_LOCALE_TIME
    const char * locale = querylocale_c(LC_TIME);
#else
    const char * locale = "C";
#endif

    struct tm  mytm;
    ints_to_tm(&mytm, locale, sec, min, hour, mday, mon, year, wday, yday,
               isdst);
    return sv_strftime_common(fmt, locale, &mytm);
}

SV *
Perl_sv_strftime_tm(pTHX_ SV * fmt, const struct tm * mytm)
{   /* Documented above */
    PERL_ARGS_ASSERT_SV_STRFTIME_TM;

#ifdef USE_LOCALE_TIME

    return sv_strftime_common(fmt, querylocale_c(LC_TIME), mytm);

#else

    return sv_strftime_common(fmt, "C", mytm);

#endif

}

SV *
S_sv_strftime_common(pTHX_ SV * fmt,
                           const char * locale,
                           const struct tm * mytm)
{   /* Documented above */
    PERL_ARGS_ASSERT_SV_STRFTIME_COMMON;

    STRLEN fmt_cur;
    const char *fmt_str = SvPV_const(fmt, fmt_cur);

    utf8ness_t fmt_utf8ness = (SvUTF8(fmt) && LIKELY(! IN_BYTES))
                              ? UTF8NESS_YES
                              : UTF8NESS_UNKNOWN;

    utf8ness_t result_utf8ness;

    /* Use a fairly generous guess as to how big the buffer needs to be, so as
     * to get almost all the typical returns to fit without the called function
     * having to realloc; this is a somewhat educated guess, but feel free to
     * tweak it. */
    SV* sv = newSV(MAX(fmt_cur * 2, 64));
    if (! strftime8(fmt_str,
                    sv,
                    locale,
                    mytm,
                    fmt_utf8ness,
                    &result_utf8ness,
                    true  /* calling from sv_strftime */ ))
    {
        return NULL;
    }


    if (result_utf8ness == UTF8NESS_YES) {
        SvUTF8_on(sv);
    }

    return sv;
}

STATIC void
S_ints_to_tm(pTHX_ struct tm * mytm,
                   const char * locale,
                   int sec, int min, int hour, int mday, int mon, int year,
                   int wday, int yday, int isdst)
{
    /* Create a struct tm structure from the input time-related integer
     * variables for 'locale' */

    /* Override with the passed-in values */
    Zero(mytm, 1, struct tm);
    mytm->tm_sec = sec;
    mytm->tm_min = min;
    mytm->tm_hour = hour;
    mytm->tm_mday = mday;
    mytm->tm_mon = mon;
    mytm->tm_year = year;
    mytm->tm_wday = wday;
    mytm->tm_yday = yday;
    mytm->tm_isdst = isdst;

    /* Long-standing behavior is to ignore the effects of locale (in
     * particular, daylight savings time) on the input, so we use mini_mktime.
     * See GH #22062. */
    mini_mktime(mytm);

    /* But some of those effect are deemed desirable, so use libc to get the
     * values for tm_gmtoff and tm_zone on platforms that have them [perl
     * #18238] */
#if  defined(HAS_MKTIME)                                      \
 && (defined(HAS_TM_TM_GMTOFF) || defined(HAS_TM_TM_ZONE))

    const char * orig_TIME_locale = toggle_locale_c(LC_TIME, locale);
    struct tm mytm2 = *mytm;
    MKTIME_LOCK;
    mktime(&mytm2);
    MKTIME_UNLOCK;
    restore_toggled_locale_c(LC_TIME, orig_TIME_locale);

#  ifdef HAS_TM_TM_GMTOFF
    mytm->tm_gmtoff = mytm2.tm_gmtoff;
#  endif
#  ifdef HAS_TM_TM_ZONE
    mytm->tm_zone = mytm2.tm_zone;
#  endif
#endif

    return;
}

STATIC bool
S_strftime_tm(pTHX_ const char *fmt,
                    SV * sv,
                    const char *locale,
                    const struct tm *mytm)
{
    PERL_ARGS_ASSERT_STRFTIME_TM;

    /* Execute strftime() based on the input struct tm, and the current LC_TIME
     * locale.
     *
     * Returns 'true' if succeeded, with the PV pointer in 'sv' filled with the
     * result, and all other C<OK> bits disabled, and not marked as UTF-8.
     * Determining the UTF-8ness must be done at a higher level.
     *
     * 'false' is returned if if fails; the state of 'sv' is unspecified. */

    /* An empty format yields an empty result */
    const Size_t fmtlen = strlen(fmt);
    if (fmtlen == 0) {
        sv_setpvs(sv, "");
        SvUTF8_off(sv);
        return true;
    }

    bool succeeded = false;

#ifndef HAS_STRFTIME
    Perl_croak(aTHX_ "panic: no strftime");
#endif

    start_DEALING_WITH_MISMATCHED_CTYPE(locale);

#if defined(USE_LOCALE_TIME)

    const char * orig_TIME_locale = toggle_locale_c(LC_TIME, locale);

#  define LC_TIME_TEARDOWN                                                  \
                        restore_toggled_locale_c(LC_TIME, orig_TIME_locale)
#else
   PERL_UNUSED_ARG(locale);
#  define LC_TIME_TEARDOWN
#endif

    /* Assume the caller has furnished a reasonable sized guess, but guard
     * against one that won't work */
    Size_t bufsize = MAX(2, SvLEN(sv));
    SvUPGRADE(sv, SVt_PV);
    SvPOK_only(sv);

    do {
        char * buf = SvGROW(sv, bufsize);

        /* allowing user-supplied (rather than literal) formats is normally
         * frowned upon as a potential security risk; but this is part of the
         * API so we have to allow it (and the available formats have a much
         * lower chance of doing something bad than the ones for printf etc. */
        GCC_DIAG_IGNORE_STMT(-Wformat-nonliteral);

#ifdef WIN32    /* Windows will tell you if the input is invalid */

        /* Needed because the LOCK might (or might not) save/restore errno */
        bool strftime_failed = false;

        STRFTIME_LOCK;
        dSAVE_ERRNO;
        errno = 0;

        Size_t len = strftime(buf, bufsize, fmt, mytm);
        if (errno == EINVAL) {
            strftime_failed = true;
        }

        RESTORE_ERRNO;
        STRFTIME_UNLOCK;

        if (strftime_failed) {
            goto strftime_failed;
        }

#else
        STRFTIME_LOCK;
        Size_t len = strftime(buf, bufsize, fmt, mytm);
        STRFTIME_UNLOCK;
#endif

        GCC_DIAG_RESTORE_STMT;

        /* A non-zero return indicates success.  But to make sure we're not
         * dealing with some rogue strftime that returns how much space it
         * needs instead of 0 when there isn't enough, check that the return
         * indicates we have at least one byte of spare space (which will be
         * used for the terminating NUL). */
        if (inRANGE(len, 1, bufsize - 1)) {
            succeeded = true;
            SvCUR_set(sv, len);
            goto strftime_return;
        }

        /* There are several possible reasons for a 0 return code for a
         * non-empty format, and they are not trivial to tease apart.  This
         * issue is a known bug in the strftime() API.  What we do to cope is
         * to assume that the reason is not enough space in the buffer, so
         * increase it and try again. */
        bufsize *= 2;

        /* But don't just keep increasing the size indefinitely.  Stop when it
         * becomes obvious that the reason for failure is something besides not
         * enough space.  The most likely largest expanding format is %c.  On
         * khw's Linux box, the maximum result of this is 67 characters, in the
         * km_KH locale.  If a new script comes along that uses 4 UTF-8 bytes
         * per character, and with a similar expansion factor, that would be a
         * 268:2 byte ratio, or a bit more than 128:1 = 2**7:1.  Some strftime
         * implementations allow you to say %1000c to pad to 1000 bytes.  This
         * shows that it is impossible to implement this without a heuristic
         * (which can fail).  But it indicates we need to be generous in the
         * upper limit before failing.  The previous heuristic used was too
         * stingy.  Since the size doubles per iteration, it doesn't take many
         * to reach the limit */
    } while (bufsize < ((1 << 11) + 1) * fmtlen);

    /* Here, strftime() returned 0, and it likely wasn't for lack of space.
     * There are two possible reasons:
     *
     * First is that the result is legitimately 0 length.  This can happen
     * when the format is precisely "%p".  That is the only documented format
     * that can have an empty result. */
    if (strEQ(fmt, "%p")) {
        sv_setpvs(sv, "");
        SvUTF8_off(sv);
        succeeded = true;
        goto strftime_return;
    }

    /* The other reason is that the format string is malformed.  Probably it is
     * that the string is syntactically invalid for the locale.  On some
     * platforms an invalid conversion specifier '%?' (for all illegal '?') is
     * treated as a literal, but others may fail when '?' is illegal */

#ifdef WIN32
  strftime_failed:
#endif

    SET_EINVAL;
    succeeded = false;

  strftime_return:

    LC_TIME_TEARDOWN;
    end_DEALING_WITH_MISMATCHED_CTYPE(locale);

    return succeeded;
}

STATIC bool
S_strftime8(pTHX_ const char * fmt,
                  SV * sv,
                  const char * locale,
                  const struct tm * mytm,
                  const utf8ness_t fmt_utf8ness,
                  utf8ness_t * result_utf8ness,
                  const bool called_externally)
{
    PERL_ARGS_ASSERT_STRFTIME8;

    /* Wrap strftime_tm, taking into account the input and output UTF-8ness */

#ifdef USE_LOCALE_TIME
#  define INDEX_TO_USE  LC_TIME_INDEX_

    locale_utf8ness_t locale_utf8ness = LOCALE_UTF8NESS_UNKNOWN;

#else
#  define INDEX_TO_USE  LC_ALL_INDEX_   /* Effectively out of bounds */

    locale_utf8ness_t locale_utf8ness = LOCALE_NOT_UTF8;

#endif

    switch (fmt_utf8ness) {
      case UTF8NESS_IMMATERIAL:
        break;

      case UTF8NESS_NO: /* Known not to be UTF-8; must not be UTF-8 locale */
        if (is_locale_utf8(locale)) {
            SET_EINVAL;
            return false;
        }

        locale_utf8ness = LOCALE_NOT_UTF8;
        break;

      case UTF8NESS_YES:    /* Known to be UTF-8; must be UTF-8 locale if can't
                               downgrade. */
        if (! is_locale_utf8(locale)) {
            locale_utf8ness = LOCALE_NOT_UTF8;

            bool is_utf8 = true;
            Size_t fmt_len = strlen(fmt);
            fmt = (char *) bytes_from_utf8((U8 *) fmt, &fmt_len, &is_utf8);
            if (is_utf8) {
                SET_EINVAL;
                return false;
            }

            SAVEFREEPV(fmt);
        }
        else {
            locale_utf8ness = LOCALE_IS_UTF8;
        }

        break;

      case UTF8NESS_UNKNOWN:
        if (! is_locale_utf8(locale)) {
            locale_utf8ness = LOCALE_NOT_UTF8;
        }
        else {
            locale_utf8ness = LOCALE_IS_UTF8;
            if (called_externally) {

                /* All internal calls from this file use ASCII-only formats;
                 * but otherwise the format could be anything, so make sure to
                 * upgrade it to UTF-8 for a UTF-8 locale.  Otherwise the
                 * locale would find any UTF-8 variant characters to be
                 * malformed */
                Size_t fmt_len = strlen(fmt);
                fmt = (char *) bytes_to_utf8((U8 *) fmt, &fmt_len);
                SAVEFREEPV(fmt);
            }
        }

        break;
    }

    if (! strftime_tm(fmt, sv, locale, mytm)) {
        return false;
    }

    *result_utf8ness = get_locale_string_utf8ness_i(SvPVX(sv),
                                                    locale_utf8ness,
                                                    locale,
                                                    INDEX_TO_USE);
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                          "fmt=%s, retval=%s; utf8ness=%d",
                          fmt,
                          ((is_strict_utf8_string((U8 *) SvPVX(sv), 0))
                           ? SvPVX(sv)
                           :_byte_dump_string((U8 *) SvPVX(sv), SvCUR(sv) ,0)),
                          *result_utf8ness));
    return true;

#undef INDEX_TO_USE

}

#ifdef USE_LOCALE

STATIC void
S_give_perl_locale_control(pTHX_
#  ifdef LC_ALL
                           const char * lc_all_string,
#  else
                           const char ** locales,
#  endif
                           const line_t caller_line)
{
    PERL_UNUSED_ARG(caller_line);

    /* This is called when the program is in the global locale and are
     * switching to per-thread (if available).  And it is called at
     * initialization time to do the same.
     */

#  if defined(WIN32) && defined(USE_THREAD_SAFE_LOCALE)

    /* On Windows, convert to per-thread behavior.  This isn't necessary in
     * POSIX 2008, as the conversion gets done automatically in the
     * void_setlocale_i() calls below. */
    if (_configthreadlocale(_ENABLE_PER_THREAD_LOCALE) == -1) {
        locale_panic_("_configthreadlocale returned an error");
    }

#  endif
#  if ! defined(USE_THREAD_SAFE_LOCALE)                               \
   && ! defined(USE_POSIX_2008_LOCALE)
#    if defined(LC_ALL)
    PERL_UNUSED_ARG(lc_all_string);
#    else
    PERL_UNUSED_ARG(locales);
#    endif
#  else

    /* This platform has per-thread locale handling.  Do the conversion. */

#    if defined(LC_ALL)

    void_setlocale_c_with_caller(LC_ALL, lc_all_string, __FILE__, caller_line);

#    else

    for_all_individual_category_indexes(i) {
        void_setlocale_i_with_caller(i, locales[i], __FILE__, caller_line);
    }

#    endif
#  endif

    /* Finally, update our remaining records.  'true' => force recalculation.
     * This is needed because we don't know what's happened while Perl hasn't
     * had control, so we need to figure out the current state */

#  if defined(LC_ALL)

    new_LC_ALL(lc_all_string, true);

#    else

    new_LC_ALL(calculate_LC_ALL_string(locales,
                                       INTERNAL_FORMAT,
                                       WANT_TEMP_PV,
                                       caller_line),
               true);
#    endif

}

STATIC void
S_output_check_environment_warning(pTHX_ const char * const language,
                                         const char * const lc_all,
                                         const char * const lang)
{
    PerlIO_printf(Perl_error_log,
                  "perl: warning: Please check that your locale settings:\n");

#  ifdef __GLIBC__

    PerlIO_printf(Perl_error_log, "\tLANGUAGE = %c%s%c,\n",
                                  language ? '"' : '(',
                                  language ? language : "unset",
                                  language ? '"' : ')');
#  else
    PERL_UNUSED_ARG(language);
#  endif

    PerlIO_printf(Perl_error_log, "\tLC_ALL = %c%s%c,\n",
                                  lc_all ? '"' : '(',
                                  lc_all ? lc_all : "unset",
                                  lc_all ? '"' : ')');

    for_all_individual_category_indexes(i) {
        const char * value = PerlEnv_getenv(category_names[i]);
        PerlIO_printf(Perl_error_log,
                      "\t%s = %c%s%c,\n",
                      category_names[i],
                      value ? '"' : '(',
                      value ? value : "unset",
                      value ? '"' : ')');
    }

    PerlIO_printf(Perl_error_log, "\tLANG = %c%s%c\n",
                                  lang ? '"' : '(',
                                  lang ? lang : "unset",
                                  lang ? '"' : ')');
    PerlIO_printf(Perl_error_log,
                  "    are supported and installed on your system.\n");
}

#endif

/* A helper macro for the next function.  Needed because would be called in two
 * places.  Knows about the internal workings of the function */
#define GET_DESCRIPTION(trial, name)                                    \
    ((isNAME_C_OR_POSIX(name))                                          \
     ? "the standard locale"                                            \
     : ((trial == (system_default_trial)                                \
                  ? "the system default locale"                         \
                  : "a fallback locale")))

/*
 * Initialize locale awareness.
 */
int
Perl_init_i18nl10n(pTHX_ int printwarn)
{
    /* printwarn is:
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
     * This routine effectively does the following in most cases:
     *
     *      basic initialization;
     *      asserts that the compiled tables are consistent;
     *      initialize data structures;
     *      make sure we are in the global locale;
     *      setlocale(LC_ALL, "");
     *      switch to per-thread locale if applicable;
     *
     * The "" causes the locale to be set to what the environment variables at
     * the time say it should be.
     *
     * To handle possible failures, the setlocale is expanded to be like:
     *
     *      trial_locale = pre-first-trial;
     *      while (has_another_trial()) {
     *          trial_locale = next_trial();
     *          if setlocale(LC_ALL, trial_locale) {
     *              ok = true;
     *              break;
     *          }
     *
     *          had_failure = true;
     *          warn();
     *      }
     *
     *      if (had_failure) {
     *          warn_even_more();
     *          if (! ok) warn_still_more();
     *      }
     *
     * The first trial is either:
     *      ""      to examine the environment variables for the locale
     *      NULL    to use the values already set for the locale by the program
     *              embedding this perl instantiation.
     *
     * Something is wrong if this trial fails, but there is a sequence of
     * fallbacks to try should that happen.  They are given in the enum below.

     * If there is no LC_ALL defined on the system, the setlocale() above is
     * replaced by a loop setting each individual category separately.
     *
     * In a non-embeded environment, this code is executed exactly once.  It
     * sets up the global locale environment.  At the end, if some sort of
     * thread-safety is in effect, it will turn thread 0 into using that, with
     * the same locale as the global initially.  thread 0 can then change its
     * locale at will without affecting the global one.
     *
     * At destruction time, thread 0 will revert to the global locale as the
     * other threads die.
     *
     * Care must be taken in an embedded environment.  This code will be
     * executed for each instantiation.  Since it changes the global locale, it
     * could clash with another running instantiation that isn't using
     * per-thread locales.  perlembed suggests having the controlling program
     * set each instantiation's locale and set PERL_SKIP_LOCALE_INIT so this
     * code uses that without actually changing anything.  Then the onus is on
     * the controlling program to prevent any races.  The code below does
     * enough locking so as to prevent system calls from overwriting data
     * before it is safely copied here, but that isn't a general solution.
     */

    if (PL_langinfo_sv == NULL) {
         PL_langinfo_sv = newSVpvs("");
    }
    if (PL_scratch_langinfo == NULL) {
         PL_scratch_langinfo = newSVpvs("");
    }

#ifndef USE_LOCALE

    PERL_UNUSED_ARG(printwarn);
    const int ok = 1;

#else  /* USE_LOCALE to near the end of the routine */

    int ok = 0;

#  ifdef __GLIBC__

    const char * const language = PerlEnv_getenv("LANGUAGE");

#  else
    const char * const language = NULL;     /* Unused placeholder */
#  endif

    /* A later getenv() could zap this, so only use here */
    const char * const bad_lang_use_once = PerlEnv_getenv("PERL_BADLANG");

    const bool locwarn = (printwarn > 1
                          || (          printwarn
                              && (    ! bad_lang_use_once
                                  || (
                                         /* disallow with "" or "0" */
                                         *bad_lang_use_once
                                       && strNE("0", bad_lang_use_once)))));

#  ifndef DEBUGGING
#    define DEBUG_LOCALE_INIT(a,b,c)
#  else

    DEBUG_INITIALIZATION_set(cBOOL(PerlEnv_getenv("PERL_DEBUG_LOCALE_INIT")));

#    define DEBUG_LOCALE_INIT(cat_index, locale, result)                    \
        DEBUG_L(PerlIO_printf(Perl_debug_log, "%s\n",                       \
                    setlocale_debug_string_i(cat_index, locale, result)));

#    ifdef LC_ALL
    assert(categories[LC_ALL_INDEX_] == LC_ALL);
    assert(strEQ(category_names[LC_ALL_INDEX_], "LC_ALL"));
#      ifdef USE_POSIX_2008_LOCALE
    assert(category_masks[LC_ALL_INDEX_] == LC_ALL_MASK);
#      endif
#    endif

    for_all_individual_category_indexes(i) {
        assert(category_name_lengths[i] == strlen(category_names[i]));
    }

#  endif    /* DEBUGGING */

    /* Initialize the per-thread mbrFOO() state variables.  See POSIX.xs for
     * why these particular incantations are used. */
#  ifdef HAS_MBRLEN
    memzero(&PL_mbrlen_ps, sizeof(PL_mbrlen_ps));
#  endif
#  ifdef HAS_MBRTOWC
    memzero(&PL_mbrtowc_ps, sizeof(PL_mbrtowc_ps));
#  endif
#  ifdef HAS_WCTOMBR
    wcrtomb(NULL, L'\0', &PL_wcrtomb_ps);
#  endif
#  ifdef USE_PL_CURLOCALES

    for (unsigned int i = 0; i <= LC_ALL_INDEX_; i++) {
        PL_curlocales[i] = savepv("C");
    }

#  endif
#  ifdef USE_PL_CUR_LC_ALL

    PL_cur_LC_ALL = savepv("C");

#  endif
#  if ! defined(PERL_LC_ALL_USES_NAME_VALUE_PAIRS) && defined(LC_ALL)

    LOCALE_LOCK;

    /* If we haven't done so already, translate the LC_ALL positions of
     * categories into our internal indices. */
    if (map_LC_ALL_position_to_index[0] == LC_ALL_INDEX_) {

#    ifdef PERL_LC_ALL_CATEGORY_POSITIONS_INIT
        /* Use this array, initialized by a config.h constant */
        int lc_all_category_positions[] = PERL_LC_ALL_CATEGORY_POSITIONS_INIT;
        STATIC_ASSERT_STMT(   C_ARRAY_LENGTH(lc_all_category_positions)
                           == LC_ALL_INDEX_);

        for (unsigned int i = 0;
             i < C_ARRAY_LENGTH(lc_all_category_positions);
             i++)
        {
            map_LC_ALL_position_to_index[i] =
                              get_category_index(lc_all_category_positions[i]);
        }
#    else
        /* It is possible for both PERL_LC_ALL_USES_NAME_VALUE_PAIRS and
         * PERL_LC_ALL_CATEGORY_POSITIONS_INIT not to be defined, e.g. on
         * systems with only a C locale during ./Configure.  Assume that this
         * can only happen as part of some sort of bootstrapping so allow
         * compilation to succeed by ignoring correctness.
         */
        for (unsigned int i = 0;
             i < C_ARRAY_LENGTH(map_LC_ALL_position_to_index);
             i++)
        {
            map_LC_ALL_position_to_index[i] = 0;
        }
#    endif

    }

    LOCALE_UNLOCK;

#  endif
#  ifdef USE_POSIX_2008_LOCALE

    /* This is a global, so be sure to keep another instance from zapping it */
    LOCALE_LOCK;
    if (PL_C_locale_obj) {
        LOCALE_UNLOCK;
    }
    else {
        PL_C_locale_obj = newlocale(LC_ALL_MASK, "C", (locale_t) 0);
        if (! PL_C_locale_obj) {
            LOCALE_UNLOCK;
            locale_panic_("Cannot create POSIX 2008 C locale object");
        }
        LOCALE_UNLOCK;

        DEBUG_Lv(PerlIO_printf(Perl_debug_log, "created C object %p\n",
                                               PL_C_locale_obj));
    }

    /* Switch to using the POSIX 2008 interface now.  This would happen below
     * anyway, but deferring it can lead to leaks of memory that would also get
     * malloc'd in the interim.  We arbitrarily switch to the C locale,
     * overridden below  */
    if (! uselocale(PL_C_locale_obj)) {
        locale_panic_(Perl_form(aTHX_
                                "Can't uselocale(%p), LC_ALL supposed to"
                                " be 'C'",
                                PL_C_locale_obj));
    }

#    ifdef MULTIPLICITY

    PL_cur_locale_obj = PL_C_locale_obj;

#    endif
#  endif

    /* Now initialize some data structures.  This is entirely so that
     * later-executed code doesn't have to concern itself with things not being
     * initialized.  Arbitrarily use the C locale (which we know has to exist
     * on the system). */

#  ifdef USE_LOCALE_NUMERIC

    PL_numeric_radix_sv    = newSV(1);
    PL_underlying_radix_sv = newSV(1);
    Newxz(PL_numeric_name, 1, char);    /* Single NUL character */

#  endif
#  ifdef USE_LOCALE_COLLATE

    Newxz(PL_collation_name, 1, char);

#  endif
#  ifdef USE_LOCALE_CTYPE

    Newxz(PL_ctype_name, 1, char);

#  endif

    new_LC_ALL("C", true /* Don't shortcut */);

/*===========================================================================*/

    /* Now ready to override the initialization with the values that the user
     * wants.  This is done in the global locale as explained in the
     * introductory comments to this function */
    switch_to_global_locale();

    const char * const lc_all     = PerlEnv_getenv("LC_ALL");
    const char * const lang       = PerlEnv_getenv("LANG");

    /* We try each locale in the enum, in order, until we get one that works,
     * or exhaust the list.  Normally the loop is executed just once.
     *
     * Each enum value is +1 from the previous */
    typedef enum {
            dummy_trial       = -1,
            environment_trial =  0,     /* "" or NULL; code below assumes value
                                           0 is the first real trial */
            LC_ALL_trial,               /* ENV{LC_ALL} */
            LANG_trial,                 /* ENV{LANG} */
            system_default_trial,       /* Windows .ACP */
            C_trial,                    /* C locale */
            beyond_final_trial,
    } trials;

    trials trial;
    unsigned int already_checked = 0;
    const char * checked[C_trial];

#  ifdef LC_ALL
    const char * lc_all_string;
#  else
    const char * curlocales[LC_ALL_INDEX_];
#  endif

    /* Loop through the initial setting and all the possible fallbacks,
     * breaking out of the loop on success */
    trial = dummy_trial;
    while (trial != beyond_final_trial) {

        /* Each time through compute the next trial to use based on the one in
         * the previous iteration and switch to the new one.  This enforces the
         * order in which the fallbacks are applied */
      next_trial:
        trial = (trials) ((int) trial + 1);     /* Casts are needed for g++ */

        const char * locale = NULL;

        /* Set up the parameters for this trial */
        switch (trial) {
          case dummy_trial:
            locale_panic_("Unexpectedly got 'dummy_trial");
            break;

          case environment_trial:
            /* This is either "" to get the values from the environment, or
             * NULL if the calling program has initialized the values already.
             * */
            locale = (PerlEnv_getenv("PERL_SKIP_LOCALE_INIT"))
                     ? NULL
                     : "";
            break;

          case LC_ALL_trial:
            if (! lc_all || strEQ(lc_all, "")) {
                continue;   /* No-op */
            }

            locale = lc_all;
            break;

          case LANG_trial:
            if (! lang || strEQ(lang, "")) {
                continue;   /* No-op */
            }

            locale = lang;
            break;

          case system_default_trial:

#  if ! defined(WIN32) || ! defined(LC_ALL)

            continue;   /* No-op */

#  else
            /* For Windows, we also try the system default locale before "C".
             * (If there exists a Windows without LC_ALL we skip this because
             * it gets too complicated.  For those, "C" is the next fallback
             * possibility). */
            locale = ".ACP";
#  endif
            break;

          case C_trial:
            locale = "C";
            break;

          case beyond_final_trial:
            continue;     /* No-op, causes loop to exit */
        }

        /* If the locale is a substantive name, don't try the same locale
         * twice. */
        if (locale && strNE(locale, "")) {
            for (unsigned int i = 0; i < already_checked; i++) {
                if (strEQ(checked[i], locale)) {
                    goto next_trial;
                }
            }

            /* And, for future iterations, indicate we've tried this locale */
            assert(already_checked < C_ARRAY_LENGTH(checked));
            checked[already_checked] = savepv(locale);
            SAVEFREEPV(checked[already_checked]);
            already_checked++;
        }

#  ifdef LC_ALL

        STDIZED_SETLOCALE_LOCK;
        lc_all_string = savepv(stdized_setlocale(LC_ALL, locale));
        STDIZED_SETLOCALE_UNLOCK;

        DEBUG_LOCALE_INIT(LC_ALL_INDEX_, locale, lc_all_string);

        if (LIKELY(lc_all_string)) {     /* Succeeded */
            ok = 1;
            break;
        }

        if (trial == 0 && locwarn) {
            PerlIO_printf(Perl_error_log,
                                  "perl: warning: Setting locale failed.\n");
            output_check_environment_warning(language, lc_all, lang);
        }

#  else /* Below is ! LC_ALL */

        bool setlocale_failure = FALSE;  /* This trial hasn't failed so far */
        bool dowarn = trial == 0 && locwarn;

        for_all_individual_category_indexes(j) {
            STDIZED_SETLOCALE_LOCK;
            curlocales[j] = savepv(stdized_setlocale(categories[j], locale));
            STDIZED_SETLOCALE_UNLOCK;

            DEBUG_LOCALE_INIT(j, locale, curlocales[j]);

            if (UNLIKELY(! curlocales[j])) {
                setlocale_failure = TRUE;

                /* If are going to warn below, continue to loop so all failures
                 * are included in the message */
                if (! dowarn) {
                    break;
                }
            }
        }

        if (LIKELY(! setlocale_failure)) {  /* All succeeded */
            ok = 1;
            break;  /* Exit trial_locales loop */
        }

        /* Here, this trial failed */

        if (dowarn) {
            PerlIO_printf(Perl_error_log,
                "perl: warning: Setting locale failed for the categories:\n");

            for_all_individual_category_indexes(j) {
                if (! curlocales[j]) {
                    PerlIO_printf(Perl_error_log, "\t%s\n", category_names[j]);
                }
            }

            output_check_environment_warning(language, lc_all, lang);
        }   /* end of warning on first failure */

#  endif /* LC_ALL */

    }   /* end of looping through the trial locales */

    /* If we had to do more than the first trial, it means that one failed, and
     * we may need to output a warning, and, if none worked, do more */
    if (UNLIKELY(trial != 0)) {
        if (locwarn) {
            const char * description = "a fallback locale";
            const char * name = NULL;;

            /* If we didn't find a good fallback, list all we tried */
            if (! ok && already_checked > 0) {
                PerlIO_printf(Perl_error_log, "perl: warning: Failed to fall"
                                              " back to ");
                if (already_checked > 1) {  /* more than one was tried */
                    PerlIO_printf(Perl_error_log, "any of:\n");
                }

                while (already_checked > 0) {
                    name = checked[--already_checked];
                    description = GET_DESCRIPTION(trial, name);
                    PerlIO_printf(Perl_error_log, "%s (\"%s\")\n",
                                                  description, name);
                }
            }

            if (ok) {

                /* Here, a fallback worked.  So we have saved its name, and the
                 * trial that succeeded is still valid */
#  ifdef LC_ALL
                const char * individ_locales[LC_ALL_INDEX_] = { NULL };

                /* Even though we know the valid string for LC_ALL that worked,
                 * translate it into our internal format, which is the
                 * name=value pairs notation.  This is easier for a human to
                 * decipher than the positional notation.  Some platforms
                 * can return "C C C C C C" for LC_ALL.  This code also
                 * standardizes that result into plain "C". */
                switch (parse_LC_ALL_string(lc_all_string,
                                            (const char **) &individ_locales,
                                            no_override,
                                            false,   /* Return only [0] if
                                                        suffices */
                                            false,   /* Don't panic on error */
                                            __LINE__))
                {
                  case invalid:

                    /* Here, the parse failed, which shouldn't happen, but if
                     * it does, we have an easy fallback that allows us to keep
                     * going. */
                    name = lc_all_string;
                    break;

                  case no_array:    /* The original is a single locale */
                    name = lc_all_string;
                    break;

                  case only_element_0:  /* element[0] is a single locale valid
                                           for all categories */
                    SAVEFREEPV(individ_locales[0]);
                    name = individ_locales[0];
                    break;

                  case full_array:
                    name = calculate_LC_ALL_string(individ_locales,
                                                   INTERNAL_FORMAT,
                                                   WANT_TEMP_PV,
                                                   __LINE__);
                    for_all_individual_category_indexes(j) {
                        Safefree(individ_locales[j]);
                    }
                }
#  else
                name = calculate_LC_ALL_string(curlocales,
                                               INTERNAL_FORMAT,
                                               WANT_TEMP_PV,
                                               __LINE__);
#  endif
                description = GET_DESCRIPTION(trial, name);
            }
            else {

                /* Nothing seems to be working, yet we want to continue
                 * executing.  It may well be that locales are mostly
                 * irrelevant to this particular program, and there must be
                 * some locale underlying the program.  Figure it out as best
                 * we can, by querying the system's current locale */

#  ifdef LC_ALL

                STDIZED_SETLOCALE_LOCK;
                name = stdized_setlocale(LC_ALL, NULL);
                STDIZED_SETLOCALE_UNLOCK;

                if (UNLIKELY(! name)) {
                    name = "locale name not determinable";
                }

#  else /* Below is ! LC_ALL */

                const char * system_locales[LC_ALL_INDEX_] = { NULL };

                for_all_individual_category_indexes(j) {
                    STDIZED_SETLOCALE_LOCK;
                    system_locales[j] = savepv(stdized_setlocale(categories[j],
                                                                 NULL));
                    STDIZED_SETLOCALE_UNLOCK;

                    if (UNLIKELY(! system_locales[j])) {
                        system_locales[j] = "not determinable";
                    }
                }

                /* We use the name=value form for the string, as that is more
                 * human readable than the positional notation */
                name = calculate_LC_ALL_string(system_locales,
                                               INTERNAL_FORMAT,
                                               WANT_TEMP_PV,
                                               __LINE__);
                description = "what the system says";

                for_all_individual_category_indexes(j) {
                    Safefree(system_locales[j]);
                }
#  endif
            }

            PerlIO_printf(Perl_error_log,
                          "perl: warning: Falling back to %s (\"%s\").\n",
                          description, name);

            /* Here, ok being true indicates that the first attempt failed, but
             * a fallback succeeded; false => nothing working.  Translate to
             * API return values. */
            ok = (ok) ? 0 : -1;
        }
    }

#  ifdef LC_ALL

    give_perl_locale_control(lc_all_string, __LINE__);
    Safefree(lc_all_string);

#  else

    give_perl_locale_control((const char **) &curlocales, __LINE__);

    for_all_individual_category_indexes(j) {
        Safefree(curlocales[j]);
    }

#  endif
#  if defined(USE_PERLIO) && defined(USE_LOCALE_CTYPE)

    /* Set PL_utf8locale to TRUE if using PerlIO _and_ the current LC_CTYPE
     * locale is UTF-8.  give_perl_locale_control() just above has already
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
#  if defined(USE_POSIX_2008_LOCALE) && defined(MULTIPLICITY)
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                           "finished Perl_init_i18nl10n; actual obj=%p,"
                           " expected obj=%p, initial=%s\n",
                           uselocale(0), PL_cur_locale_obj,
                           get_LC_ALL_display()));
#  endif

    /* So won't continue to output stuff */
    DEBUG_INITIALIZATION_set(FALSE);

#endif /* USE_LOCALE */

    return ok;
}

#undef GET_DESCRIPTION
#ifdef USE_LOCALE_COLLATE

STATIC void
S_compute_collxfrm_coefficients(pTHX)
{

    /* A locale collation definition includes primary, secondary, tertiary,
     * etc. weights for each character.  To sort, the primary weights are used,
     * and only if they compare equal, then the secondary weights are used, and
     * only if they compare equal, then the tertiary, etc.
     *
     * strxfrm() works by taking the input string, say ABC, and creating an
     * output transformed string consisting of first the primary weights,
     * A¹B¹C¹ followed by the secondary ones, A²B²C²; and then the tertiary,
     * etc, yielding A¹B¹C¹ A²B²C² A³B³C³ ....  Some characters may not have
     * weights at every level.  In our example, let's say B doesn't have a
     * tertiary weight, and A doesn't have a secondary weight.  The constructed
     * string is then going to be
     *  A¹B¹C¹ B²C² A³C³ ....
     * This has the desired effect that strcmp() will look at the secondary or
     * tertiary weights only if the strings compare equal at all higher
     * priority weights.  The spaces shown here, like in
     *  "A¹B¹C¹ A²B²C² "
     * are not just for readability.  In the general case, these must actually
     * be bytes, which we will call here 'separator weights'; and they must be
     * smaller than any other weight value, but since these are C strings, only
     * the terminating one can be a NUL (some implementations may include a
     * non-NUL separator weight just before the NUL).  Implementations tend to
     * reserve 01 for the separator weights.  They are needed so that a shorter
     * string's secondary weights won't be misconstrued as primary weights of a
     * longer string, etc.  By making them smaller than any other weight, the
     * shorter string will sort first.  (Actually, if all secondary weights are
     * smaller than all primary ones, there is no need for a separator weight
     * between those two levels, etc.)
     *
     * The length of the transformed string is roughly a linear function of the
     * input string.  It's not exactly linear because some characters don't
     * have weights at all levels.  When we call strxfrm() we have to allocate
     * some memory to hold the transformed string.  The calculations below try
     * to find coefficients 'm' and 'b' for this locale so that m*x + b equals
     * how much space we need, given the size of the input string in 'x'.  If
     * we calculate too small, we increase the size as needed, and call
     * strxfrm() again, but it is better to get it right the first time to
     * avoid wasted expensive string transformations.
     *
     * We use the string below to find how long the transformation of it is.
     * Almost all locales are supersets of ASCII, or at least the ASCII
     * letters.  We use all of them, half upper half lower, because if we used
     * fewer, we might hit just the ones that are outliers in a particular
     * locale.  Most of the strings being collated will contain a preponderance
     * of letters, and even if they are above-ASCII, they are likely to have
     * the same number of weight levels as the ASCII ones.  It turns out that
     * digits tend to have fewer levels, and some punctuation has more, but
     * those are relatively sparse in text, and khw believes this gives a
     * reasonable result, but it could be changed if experience so dictates. */
    const char longer[] = "ABCDEFGHIJKLMnopqrstuvwxyz";
    char * x_longer;        /* Transformed 'longer' */
    Size_t x_len_longer;    /* Length of 'x_longer' */

    char * x_shorter;   /* We also transform a substring of 'longer' */
    Size_t x_len_shorter;

    PL_in_utf8_COLLATE_locale = (PL_collation_standard)
                                ? 0
                                : is_locale_utf8(PL_collation_name);
    PL_strxfrm_NUL_replacement = '\0';
    PL_strxfrm_max_cp = 0;

    /* mem_collxfrm_() is used get the transformation (though here we are
     * interested only in its length).  It is used because it has the
     * intelligence to handle all cases, but to work, it needs some values of
     * 'm' and 'b' to get it started.  For the purposes of this calculation we
     * use a very conservative estimate of 'm' and 'b'.  This assumes a weight
     * can be multiple bytes, enough to hold any UV on the platform, and there
     * are 5 levels, 4 weight bytes, and a trailing NUL.  */
    PL_collxfrm_base = 5;
    PL_collxfrm_mult = 5 * sizeof(UV);

    /* Find out how long the transformation really is */
    x_longer = mem_collxfrm_(longer,
                             sizeof(longer) - 1,
                             &x_len_longer,

                             /* We avoid converting to UTF-8 in the called
                              * function by telling it the string is in UTF-8
                              * if the locale is a UTF-8 one.  Since the string
                              * passed here is invariant under UTF-8, we can
                              * claim it's UTF-8 even if it isn't.  */
                              PL_in_utf8_COLLATE_locale);
    Safefree(x_longer);

    /* Find out how long the transformation of a substring of 'longer' is.
     * Together the lengths of these transformations are sufficient to
     * calculate 'm' and 'b'.  The substring is all of 'longer' except the
     * first character.  This minimizes the chances of being swayed by outliers
     * */
    x_shorter = mem_collxfrm_(longer + 1,
                              sizeof(longer) - 2,
                              &x_len_shorter,
                              PL_in_utf8_COLLATE_locale);
    Safefree(x_shorter);

    /* If the results are nonsensical for this simple test, the whole locale
     * definition is suspect.  Mark it so that locale collation is not active
     * at all for it.  XXX Should we warn? */
    if (   x_len_shorter == 0
        || x_len_longer == 0
        || x_len_shorter >= x_len_longer)
    {
        PL_collxfrm_mult = 0;
        PL_collxfrm_base = 1;
        DEBUG_L(PerlIO_printf(Perl_debug_log,
                "Disabling locale collation for LC_COLLATE='%s';"
                " length for shorter sample=%zu; longer=%zu\n",
                PL_collation_name, x_len_shorter, x_len_longer));
    }
    else {
        SSize_t base;       /* Temporary */

        /* We have both: m * strlen(longer)  + b = x_len_longer
         *               m * strlen(shorter) + b = x_len_shorter;
         * subtracting yields:
         *          m * (strlen(longer) - strlen(shorter))
         *                             = x_len_longer - x_len_shorter
         * But we have set things up so that 'shorter' is 1 byte smaller than
         * 'longer'.  Hence:
         *          m = x_len_longer - x_len_shorter
         *
         * But if something went wrong, make sure the multiplier is at least 1.
         */
        if (x_len_longer > x_len_shorter) {
            PL_collxfrm_mult = (STRLEN) x_len_longer - x_len_shorter;
        }
        else {
            PL_collxfrm_mult = 1;
        }

        /*     mx + b = len
         * so:      b = len - mx
         * but in case something has gone wrong, make sure it is non-negative
         * */
        base = x_len_longer - PL_collxfrm_mult * (sizeof(longer) - 1);
        if (base < 0) {
            base = 0;
        }

        /* Add 1 for the trailing NUL */
        PL_collxfrm_base = base + 1;
    }

    DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "?UTF-8 locale=%d; x_len_shorter=%zu, "
                          "x_len_longer=%zu,"
                          " collate multipler=%zu, collate base=%zu\n",
                          PL_in_utf8_COLLATE_locale,
                          x_len_shorter, x_len_longer,
                          PL_collxfrm_mult, PL_collxfrm_base));
}

char *
Perl_mem_collxfrm_(pTHX_ const char *input_string,
                         STRLEN len,    /* Length of 'input_string' */
                         STRLEN *xlen,  /* Set to length of returned string
                                           (not including the collation index
                                           prefix) */
                         bool utf8      /* Is the input in UTF-8? */
                   )
{
    /* mem_collxfrm_() is like strxfrm() but with two important differences.
     * First, it handles embedded NULs. Second, it allocates a bit more memory
     * than needed for the transformed data itself.  The real transformed data
     * begins at offset COLLXFRM_HDR_LEN.  *xlen is set to the length of that,
     * and doesn't include the collation index size.
     *
     * It is the caller's responsibility to eventually free the memory returned
     * by this function.
     *
     * Please see sv_collxfrm() to see how this is used. */

#  define COLLXFRM_HDR_LEN    sizeof(PL_collation_ix)

    char * s = (char *) input_string;
    STRLEN s_strlen = strlen(input_string);
    char *xbuf = NULL;
    STRLEN xAlloc;          /* xalloc is a reserved word in VC */
    STRLEN length_in_chars;
    bool first_time = TRUE; /* Cleared after first loop iteration */

#  ifdef USE_LOCALE_CTYPE
        const char * orig_CTYPE_locale = NULL;
#  endif

#  if defined(USE_POSIX_2008_LOCALE) && defined HAS_STRXFRM_L
    locale_t constructed_locale = (locale_t) 0;
#  endif

    PERL_ARGS_ASSERT_MEM_COLLXFRM_;

    /* Must be NUL-terminated */
    assert(*(input_string + len) == '\0');

    if (PL_collxfrm_mult == 0) {     /* unknown or bad */
        if (PL_collxfrm_base != 0) { /* bad collation => skip */
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                          "mem_collxfrm_: locale's collation is defective\n"));
            goto bad;
        }

        /* (mult, base) == (0,0) means we need to calculate mult and base
         * before proceeding */
        S_compute_collxfrm_coefficients(aTHX);
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

#  ifdef USE_LOCALE_CTYPE

                /* In this case we use isCNTRL_LC() below, which relies on
                 * LC_CTYPE, so that must be switched to correspond with the
                 * LC_COLLATE locale */
                const bool need_to_toggle = (   ! try_non_controls
                                             && ! PL_in_utf8_COLLATE_locale);
                if (need_to_toggle) {
                    orig_CTYPE_locale = toggle_locale_c(LC_CTYPE,
                                                        PL_collation_name);
                }
#  endif
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
                    x = mem_collxfrm_(cur_source, trial_len, &x_len,
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

#  ifdef USE_LOCALE_CTYPE

                if (need_to_toggle) {
                    restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);
                }
#  endif

                /* Stop looking if found */
                if (cur_min_x) {
                    break;
                }

                /* Unlikely, but possible, if there aren't any controls that
                 * work in the locale, repeat the loop, looking for any
                 * character that works */
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                "mem_collxfrm_: No control worked.  Trying non-controls\n"));
            } /* End of loop to try first the controls, then any char */

            if (! cur_min_x) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                    "mem_collxfrm_: Couldn't find any character to replace"
                    " embedded NULs in locale %s with", PL_collation_name));
                goto bad;
            }

            DEBUG_L(PerlIO_printf(Perl_debug_log,
                    "mem_collxfrm_: Replacing embedded NULs in locale %s with "
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
                        x = mem_collxfrm_(cur_source, 1, &x_len, FALSE);

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
                            "mem_collxfrm_: Couldn't find any character to"
                            " replace above-Latin1 chars in locale %s with",
                            PL_collation_name));
                        goto bad;
                    }

                    DEBUG_L(PerlIO_printf(Perl_debug_log,
                            "mem_collxfrm_: highest 1-byte collating character"
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
                      "mem_collxfrm_: Couldn't malloc %zu bytes\n", xAlloc));
        goto bad;
    }

    /* Store the collation id */
    *(PERL_UINTMAX_T *)xbuf = PL_collation_ix;

#  if defined(USE_POSIX_2008_LOCALE) && defined HAS_STRXFRM_L
#    ifdef USE_LOCALE_CTYPE

    constructed_locale = newlocale(LC_CTYPE_MASK, PL_collation_name,
                                   duplocale(use_curlocale_scratch()));
#    else

    constructed_locale = duplocale(use_curlocale_scratch());

#    endif
#    define my_strxfrm(dest, src, n)  strxfrm_l(dest, src, n,           \
                                                constructed_locale)
#    define CLEANUP_STRXFRM                                             \
        STMT_START {                                                    \
            if (constructed_locale != (locale_t) 0)                     \
                freelocale(constructed_locale);                         \
        } STMT_END
#  else
#    define my_strxfrm(dest, src, n)  strxfrm(dest, src, n)
#    ifdef USE_LOCALE_CTYPE

    orig_CTYPE_locale = toggle_locale_c(LC_CTYPE, PL_collation_name);

#      define CLEANUP_STRXFRM                                           \
                restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale)
#    else
#      define CLEANUP_STRXFRM  NOOP
#    endif
#  endif

    /* Then the transformation of the input.  We loop until successful, or we
     * give up */
    for (;;) {

        errno = 0;
        *xlen = my_strxfrm(xbuf + COLLXFRM_HDR_LEN,
                           s,
                           xAlloc - COLLXFRM_HDR_LEN);


        /* If the transformed string occupies less space than we told strxfrm()
         * was available, it means it transformed the whole string. */
        if (*xlen < xAlloc - COLLXFRM_HDR_LEN) {

            /* But there still could have been a problem */
            if (errno != 0) {
                DEBUG_L(PerlIO_printf(Perl_debug_log,
                       "strxfrm failed for LC_COLLATE=%s; errno=%d, input=%s\n",
                       PL_collation_name, errno,
                       _byte_dump_string((U8 *) s, len, 0)));
                goto bad;
            }

            /* Here, the transformation was successful.  Some systems include a
             * trailing NUL in the returned length.  Ignore it, using a loop in
             * case multiple trailing NULs are returned. */
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
                    "initial size of %zu bytes for a length "
                    "%zu string was insufficient, %zu needed\n",
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
                                    "slope is now %zu; was %zu, base "
                        "is now %zu; was %zu\n",
                        PL_collxfrm_mult, old_m,
                        PL_collxfrm_base, old_b));
                }
                else {  /* Slope didn't change, but 'b' did */
                    const STRLEN new_b = needed
                                        - computed_guess
                                        + PL_collxfrm_base;
                    DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                        "base is now %zu; was %zu\n", new_b, PL_collxfrm_base));
                    PL_collxfrm_base = new_b;
                }
            }

            break;
        }

        if (UNLIKELY(*xlen >= PERL_INT_MAX)) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                  "mem_collxfrm_: Needed %zu bytes, max permissible is %u\n",
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

            DEBUG_Lv(PerlIO_printf(Perl_debug_log,
                     "mem_collxfrm_ required more space than previously"
                     " calculated for locale %s, trying again with new"
                     " guess=%zu+%zu\n",
                     PL_collation_name,  COLLXFRM_HDR_LEN,
                     xAlloc - COLLXFRM_HDR_LEN));
        }

        Renew(xbuf, xAlloc, char);
        if (UNLIKELY(! xbuf)) {
            DEBUG_L(PerlIO_printf(Perl_debug_log,
                      "mem_collxfrm_: Couldn't realloc %zu bytes\n", xAlloc));
            goto bad;
        }

        first_time = FALSE;
    }

    CLEANUP_STRXFRM;

    DEBUG_L(print_collxfrm_input_and_return(s, s + len, xbuf, *xlen, utf8));

    /* Free up unneeded space; retain enough for trailing NUL */
    Renew(xbuf, COLLXFRM_HDR_LEN + *xlen + 1, char);

    if (s != input_string) {
        Safefree(s);
    }

    return xbuf;

  bad:

    CLEANUP_STRXFRM;
    DEBUG_L(print_collxfrm_input_and_return(s, s + len, NULL, 0, utf8));

    Safefree(xbuf);
    if (s != input_string) {
        Safefree(s);
    }
    *xlen = 0;

    return NULL;
}

#  ifdef DEBUGGING

STATIC void
S_print_collxfrm_input_and_return(pTHX_
                                  const char * s,
                                  const char * e,
                                  const char * xbuf,
                                  const STRLEN xlen,
                                  const bool is_utf8)
{

    PERL_ARGS_ASSERT_PRINT_COLLXFRM_INPUT_AND_RETURN;

    PerlIO_printf(Perl_debug_log,
                  "mem_collxfrm_[ix %" UVuf "] for locale '%s':\n"
                  "     input=%s\n    return=%s\n    return len=%zu\n",
                  (UV) PL_collation_ix, PL_collation_name,
                  get_displayable_string(s, e, is_utf8),
                  ((xbuf == NULL)
                   ? "(null)"
                   : ((xlen == 0)
                      ? "(empty)"
                      : _byte_dump_string((U8 *) xbuf + COLLXFRM_HDR_LEN,
                                          xlen, 0))),
                  xlen);
}

#  endif    /* DEBUGGING */

SV *
Perl_strxfrm(pTHX_ SV * src)
{
    PERL_ARGS_ASSERT_STRXFRM;

    /* For use by POSIX::strxfrm().  If they differ, toggle LC_CTYPE to
     * LC_COLLATE to avoid potential mojibake.
     *
     * If we can't calculate a collation, 'src' is instead returned, so that
     * future comparisons will be by code point order */

#  ifdef USE_LOCALE_CTYPE

    const char * orig_ctype = toggle_locale_c(LC_CTYPE,
                                              querylocale_c(LC_COLLATE));
#  endif

    SV * dst = src;
    STRLEN dstlen;
    STRLEN srclen;
    const char *p = SvPV_const(src, srclen);
    const U32 utf8_flag = SvUTF8(src);
    char *d = mem_collxfrm_(p, srclen, &dstlen, cBOOL(utf8_flag));

    assert(utf8_flag == 0 || utf8_flag == SVf_UTF8);

    if (d != NULL) {
        assert(dstlen > 0);
        dst =newSVpvn_flags(d + COLLXFRM_HDR_LEN,
                            dstlen, SVs_TEMP|utf8_flag);
        Safefree(d);
    }

#  ifdef USE_LOCALE_CTYPE

    restore_toggled_locale_c(LC_CTYPE, orig_ctype);

#  endif

    return dst;
}

#endif /* USE_LOCALE_COLLATE */

/* my_strerror() returns a mortalized copy of the text of the error message
 * associated with 'errnum'.
 *
 * If not called from within the scope of 'use locale', it uses the text from
 * the C locale.  If Perl is compiled to not pay attention to LC_CTYPE nor
 * LC_MESSAGES, it uses whatever strerror() returns.  Otherwise the text is
 * derived from the locale, LC_MESSAGES if we have that; LC_CTYPE if not.
 *
 * It returns in *utf8ness the result's UTF-8ness
 *
 * The function just calls strerror(), but temporarily switches locales, if
 * needed.  Many platforms require LC_CTYPE and LC_MESSAGES to be in the same
 * CODESET in order for the return from strerror() to not contain '?' symbols,
 * or worse, mojibaked.  It's cheaper to just use the stricter criteria of
 * being in the same locale.  So the code below uses a common locale for both
 * categories.  Again, that is C if not within 'use locale' scope; or the
 * LC_MESSAGES locale if in scope and we have that category; and LC_CTYPE if we
 * don't have LC_MESSAGES; and whatever strerror returns if we don't have
 * either category.
 *
 * There are two sets of implementations.  The first below is if we have
 * strerror_l().  This is the simpler.  We just use the already-built C locale
 * object if not in locale scope, or build up a custom one otherwise.
 *
 * When strerror_l() is not available, we may have to swap locales temporarily
 * to bring the two categories into sync with each other, and possibly to the C
 * locale.
 *
 * Because the prepropessing directives to conditionally compile this function
 * would greatly obscure the logic of the various implementations, the whole
 * function is repeated for each configuration, with some common macros. */

/* Used to shorten the definitions of the following implementations of
 * my_strerror() */
#define DEBUG_STRERROR_ENTER(errnum, in_locale)                             \
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,                                  \
                           "my_strerror called with errnum %d;"             \
                           " Within locale scope=%d\n",                     \
                           errnum, in_locale))

#define DEBUG_STRERROR_RETURN(errstr, utf8ness)                             \
    DEBUG_Lv(PerlIO_printf(Perl_debug_log,                                  \
                           "Strerror returned; saving a copy: '%s';"        \
                           " utf8ness=%d\n",                                \
                           get_displayable_string(errstr,                   \
                                                  errstr + strlen(errstr),  \
                                                  *utf8ness),               \
                           (int) *utf8ness))

/* On platforms that have precisely one of these categories (Windows
 * qualifies), these yield the correct one */
#if defined(USE_LOCALE_CTYPE)
#  define WHICH_LC_INDEX LC_CTYPE_INDEX_
#elif defined(USE_LOCALE_MESSAGES)
#  define WHICH_LC_INDEX LC_MESSAGES_INDEX_
#endif

/*===========================================================================*/
/* First set of implementations, when have strerror_l() */

#if defined(USE_POSIX_2008_LOCALE) && defined(HAS_STRERROR_L)

#  if ! defined(USE_LOCALE_CTYPE) && ! defined(USE_LOCALE_MESSAGES)

/* Here, neither category is defined: use the C locale */
const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, 0);

    const char *errstr = savepv(strerror_l(errnum, PL_C_locale_obj));
    *utf8ness = UTF8NESS_IMMATERIAL;

    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}

#  elif ! defined(USE_LOCALE_CTYPE) || ! defined(USE_LOCALE_MESSAGES)

/*--------------------------------------------------------------------------*/

/* Here one or the other of CTYPE or MESSAGES is defined, but not both.  If we
 * are not within 'use locale' scope of the only one defined, we use the C
 * locale; otherwise use the current locale object */

const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, IN_LC(categories[WHICH_LC_INDEX]));

    /* Use C if not within locale scope;  Otherwise, use current locale */
    const locale_t which_obj = (IN_LC(categories[WHICH_LC_INDEX]))
                               ? PL_C_locale_obj
                               : use_curlocale_scratch();

    const char *errstr = savepv(strerror_l(errnum, which_obj));
    *utf8ness = get_locale_string_utf8ness_i(errstr, LOCALE_UTF8NESS_UNKNOWN,
                                             NULL, WHICH_LC_INDEX);
    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}

/*--------------------------------------------------------------------------*/
#  else     /* Are using both categories.  Place them in the same CODESET,
             * either C or the LC_MESSAGES locale */

const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, IN_LC(LC_MESSAGES));

    const char *errstr;
    if (! IN_LC(LC_MESSAGES)) {    /* Use C if not within locale scope */
        errstr = savepv(strerror_l(errnum, PL_C_locale_obj));
        *utf8ness = UTF8NESS_IMMATERIAL;
    }
    else {  /* Otherwise, use the LC_MESSAGES locale, making sure LC_CTYPE
               matches */
        locale_t cur = duplocale(use_curlocale_scratch());

        const char * locale = querylocale_c(LC_MESSAGES);
        cur = newlocale(LC_CTYPE_MASK, locale, cur);
        errstr = savepv(strerror_l(errnum, cur));
        *utf8ness = get_locale_string_utf8ness_i(errstr,
                                                 LOCALE_UTF8NESS_UNKNOWN,
                                                 locale,
                                                 LC_MESSAGES_INDEX_);
        freelocale(cur);
    }

    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}
#  endif    /* Above is using strerror_l */
/*===========================================================================*/
#else       /* Below is not using strerror_l */
#  if ! defined(USE_LOCALE_CTYPE) && ! defined(USE_LOCALE_MESSAGES)

/* If not using using either of the categories, return plain, unadorned
 * strerror */

const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, 0);

    const char *errstr = savepv(Strerror(errnum));
    *utf8ness = UTF8NESS_IMMATERIAL;

    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}

/*--------------------------------------------------------------------------*/
#  elif ! defined(USE_LOCALE_CTYPE) || ! defined(USE_LOCALE_MESSAGES)

/* Here one or the other of CTYPE or MESSAGES is defined, but not both.  If we
 * are not within 'use locale' scope of the only one defined, we use the C
 * locale; otherwise use the current locale */

const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, IN_LC(categories[WHICH_LC_INDEX]));

    const char *errstr;
    if (IN_LC(categories[WHICH_LC_INDEX])) {
        errstr = savepv(Strerror(errnum));
        *utf8ness = get_locale_string_utf8ness_i(errstr,
                                                 LOCALE_UTF8NESS_UNKNOWN,
                                                 NULL, WHICH_LC_INDEX);
    }
    else {

        LOCALE_LOCK;

        const char * orig_locale = toggle_locale_i(WHICH_LC_INDEX, "C");

        errstr = savepv(Strerror(errnum));

        restore_toggled_locale_i(WHICH_LC_INDEX, orig_locale);

        LOCALE_UNLOCK;

        *utf8ness = UTF8NESS_IMMATERIAL;
    }

    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}

/*--------------------------------------------------------------------------*/
#  else

/* Below, have both LC_CTYPE and LC_MESSAGES.  Place them in the same CODESET,
 * either C or the LC_MESSAGES locale */

const char *
Perl_my_strerror(pTHX_ const int errnum, utf8ness_t * utf8ness)
{
    PERL_ARGS_ASSERT_MY_STRERROR;

    DEBUG_STRERROR_ENTER(errnum, IN_LC(LC_MESSAGES));

    const char * desired_locale = (IN_LC(LC_MESSAGES))
                                  ? querylocale_c(LC_MESSAGES)
                                  : "C";
    /* XXX Can fail on z/OS */

    LOCALE_LOCK;

    const char* orig_CTYPE_locale    = toggle_locale_c(LC_CTYPE,
                                                       desired_locale);
    const char* orig_MESSAGES_locale = toggle_locale_c(LC_MESSAGES,
                                                       desired_locale);
    const char *errstr = savepv(Strerror(errnum));

    restore_toggled_locale_c(LC_MESSAGES, orig_MESSAGES_locale);
    restore_toggled_locale_c(LC_CTYPE, orig_CTYPE_locale);

    LOCALE_UNLOCK;

    *utf8ness = get_locale_string_utf8ness_i(errstr, LOCALE_UTF8NESS_UNKNOWN,
                                             desired_locale,
                                             LC_MESSAGES_INDEX_);
    DEBUG_STRERROR_RETURN(errstr, utf8ness);

    SAVEFREEPV(errstr);
    return errstr;
}

/*--------------------------------------------------------------------------*/
#  endif /* end of not using strerror_l() */
#endif   /* end of all the my_strerror() implementations */

bool
Perl__is_in_locale_category(pTHX_ const bool compiling, const int category)
{
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

/*

=for apidoc_section $locale
=for apidoc switch_to_global_locale

This function copies the locale state of the calling thread into the program's
global locale, and converts the thread to use that global locale.

It is intended so that Perl can safely be used with C libraries that access the
global locale and which can't be converted to not access it.  Effectively, this
means libraries that call C<L<setlocale(3)>> on non-Windows systems.  (For
portability, it is a good idea to use it on Windows as well.)

A downside of using it is that it disables the services that Perl provides to
hide locale gotchas from your code.  The service you most likely will miss
regards the radix character (decimal point) in floating point numbers.  Code
executed after this function is called can no longer just assume that this
character is correct for the current circumstances.

To return to Perl control, and restart the gotcha prevention services, call
C<L</sync_locale>>.  Behavior is undefined for any pure Perl code that executes
while the switch is in effect.

The global locale and the per-thread locales are independent.  As long as just
one thread converts to the global locale, everything works smoothly.  But if
more than one does, they can easily interfere with each other, and races are
likely.  On Windows systems prior to Visual Studio 15 (at which point Microsoft
fixed a bug), races can occur (even if only one thread has been converted to
the global locale), but only if you use the following operations:

=over

=item L<POSIX::localeconv|POSIX/localeconv>

=item L<I18N::Langinfo>, items C<CRNCYSTR> and C<THOUSEP>

=item L<perlapi/sv_langinfo>, items C<CRNCYSTR> and C<THOUSEP>

=back

The first item is not fixable (except by upgrading to a later Visual Studio
release), but it would be possible to work around the latter two items by
having Perl change its algorithm for calculating these to use Windows API
functions (likely C<GetNumberFormat> and C<GetCurrencyFormat>); patches
welcome.

XS code should never call plain C<setlocale>, but should instead be converted
to either call L<C<Perl_setlocale>|perlapi/Perl_setlocale> (which is a drop-in
for the system C<setlocale>) or use the methods given in L<perlcall> to call
L<C<POSIX::setlocale>|POSIX/setlocale>.  Either one will transparently properly
handle all cases of single- vs multi-thread, POSIX 2008-supported or not.

=cut
*/

#if defined(WIN32) && defined(USE_THREAD_SAFE_LOCALE)
#  define CHANGE_SYSTEM_LOCALE_TO_GLOBAL                                \
    STMT_START {                                                        \
        if (_configthreadlocale(_DISABLE_PER_THREAD_LOCALE) == -1) {    \
            locale_panic_("_configthreadlocale returned an error");     \
        }                                                               \
    } STMT_END
#elif defined(USE_POSIX_2008_LOCALE)
#  define CHANGE_SYSTEM_LOCALE_TO_GLOBAL                                \
    STMT_START {                                                        \
        locale_t old_locale = uselocale(LC_GLOBAL_LOCALE);              \
        if (! old_locale) {                                             \
            locale_panic_("Could not change to global locale");         \
        }                                                               \
                                                                        \
        /* Free the per-thread memory */                                \
        if (   old_locale != LC_GLOBAL_LOCALE                           \
            && old_locale != PL_C_locale_obj)                           \
        {                                                               \
            freelocale(old_locale);                                     \
        }                                                               \
    } STMT_END
#else
#  define CHANGE_SYSTEM_LOCALE_TO_GLOBAL
#endif

void
Perl_switch_to_global_locale(pTHX)
{

#ifdef USE_LOCALE

    DEBUG_L(PerlIO_printf(Perl_debug_log, "Entering switch_to_global; %s\n",
                                          get_LC_ALL_display()));

   /* In these cases, we use the system state to determine if we are in the
    * global locale or not. */
#  ifdef USE_POSIX_2008_LOCALE

    const bool perl_controls = (LC_GLOBAL_LOCALE != uselocale((locale_t) 0));

#  elif defined(USE_THREAD_SAFE_LOCALE) && defined(WIN32)

    int config_return = _configthreadlocale(0);
    if (config_return == -1) {
        locale_panic_("_configthreadlocale returned an error");
    }
    const bool perl_controls = (config_return == _ENABLE_PER_THREAD_LOCALE);

#  else

    const bool perl_controls = false;

#  endif

    /* No-op if already in global */
    if (! perl_controls) {
        return;
    }

#  ifdef LC_ALL

    const char * thread_locale = calculate_LC_ALL_string(NULL,
                                                         EXTERNAL_FORMAT_FOR_SET,
                                                         WANT_TEMP_PV,
                                                         __LINE__);
    CHANGE_SYSTEM_LOCALE_TO_GLOBAL;
    posix_setlocale(LC_ALL, thread_locale);

#  else   /* Must be USE_POSIX_2008_LOCALE) */

    const char * cur_thread_locales[LC_ALL_INDEX_];

    /* Save each category's current per-thread state */
    for_all_individual_category_indexes(i) {
        cur_thread_locales[i] = querylocale_i(i);
    }

    CHANGE_SYSTEM_LOCALE_TO_GLOBAL;

    /* Set the global to what was our per-thread state */
    POSIX_SETLOCALE_LOCK;
    for_all_individual_category_indexes(i) {
        posix_setlocale(categories[i], cur_thread_locales[i]);
    }
    POSIX_SETLOCALE_UNLOCK;

#  endif
#  ifdef USE_LOCALE_NUMERIC

    /* Switch to the underlying C numeric locale; the application is on its
     * own. */
    POSIX_SETLOCALE_LOCK;
    posix_setlocale(LC_NUMERIC, PL_numeric_name);
    POSIX_SETLOCALE_UNLOCK;

#  endif
#endif

}

/*

=for apidoc sync_locale

This function copies the state of the program global locale into the calling
thread, and converts that thread to using per-thread locales, if it wasn't
already, and the platform supports them.  The LC_NUMERIC locale is toggled into
the standard state (using the C locale's conventions), if not within the
lexical scope of S<C<use locale>>.

Perl will now consider itself to have control of the locale.

Since unthreaded perls have only a global locale, this function is a no-op
without threads.

This function is intended for use with C libraries that do locale manipulation.
It allows Perl to accommodate the use of them.  Call this function before
transferring back to Perl space so that it knows what state the C code has left
things in.

XS code should not manipulate the locale on its own.  Instead,
L<C<Perl_setlocale>|perlapi/Perl_setlocale> can be used at any time to query or
change the locale (though changing the locale is antisocial and dangerous on
multi-threaded systems that don't have multi-thread safe locale operations.
(See L<perllocale/Multi-threaded operation>).

Using the libc L<C<setlocale(3)>> function should be avoided.  Nevertheless,
certain non-Perl libraries called from XS, do call it, and their behavior may
not be able to be changed.  This function, along with
C<L</switch_to_global_locale>>, can be used to get seamless behavior in these
circumstances, as long as only one thread is involved.

If the library has an option to turn off its locale manipulation, doing that is
preferable to using this mechanism.  C<Gtk> is such a library.

The return value is a boolean: TRUE if the global locale at the time of call
was in effect for the caller; and FALSE if a per-thread locale was in effect.

=cut
*/

bool
Perl_sync_locale(pTHX)
{

#ifndef USE_LOCALE

    return TRUE;

#else

    bool was_in_global = TRUE;

#  ifdef USE_THREAD_SAFE_LOCALE
#    if defined(WIN32)

    int config_return = _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
    if (config_return == -1) {
        locale_panic_("_configthreadlocale returned an error");
    }
    was_in_global = (config_return == _DISABLE_PER_THREAD_LOCALE);

#    elif defined(USE_POSIX_2008_LOCALE)

    was_in_global = (LC_GLOBAL_LOCALE == uselocale(LC_GLOBAL_LOCALE));

#    else
#      error Unexpected Configuration
#    endif
#  endif    /* USE_THREAD_SAFE_LOCALE */

    /* Here, we are in the global locale.  Get and save the values for each
     * category, and convert the current thread to use them */

#  ifdef LC_ALL

    STDIZED_SETLOCALE_LOCK;
    const char * lc_all_string = savepv(stdized_setlocale(LC_ALL, NULL));
    STDIZED_SETLOCALE_UNLOCK;

    give_perl_locale_control(lc_all_string, __LINE__);
    Safefree(lc_all_string);

#  else

    const char * current_globals[LC_ALL_INDEX_];
    for_all_individual_category_indexes(i) {
        STDIZED_SETLOCALE_LOCK;
        current_globals[i] = savepv(stdized_setlocale(categories[i], NULL));
        STDIZED_SETLOCALE_UNLOCK;
    }

    give_perl_locale_control((const char **) &current_globals, __LINE__);

    for_all_individual_category_indexes(i) {
        Safefree(current_globals[i]);
    }

#  endif

    return was_in_global;

#endif

}

#ifdef USE_PERL_SWITCH_LOCALE_CONTEXT

void
Perl_switch_locale_context(pTHX)
{
    /* libc keeps per-thread locale status information in some configurations.
     * So, we can't just switch out aTHX to switch to a new thread.  libc has
     * to follow along.  This routine does that based on per-interpreter
     * variables we keep just for this purpose.
     *
     * There are two implementations where this is an issue.  For the other
     * implementations, it doesn't matter because libc is using global values
     * that all threads know about.
     *
     * The two implementations are where libc keeps thread-specific information
     * on its own.  These are
     *
     * POSIX 2008:  The current locale is kept by libc as an object.  We save
     *              a copy of that in the per-thread PL_cur_locale_obj, and so
     *              this routine uses that copy to tell the thread it should be
     *              operating with that object
     * Windows thread-safe locales:  A given thread in Windows can be being run
     *              with per-thread locales, or not.  When the thread context
     *              changes, libc doesn't automatically know if the thread is
     *              using per-thread locales, nor does it know what the new
     *              thread's locale is.  We keep that information in the
     *              per-thread variables:
     *                  PL_controls_locale  indicates if this thread is using
     *                                      per-thread locales or not
     *                  PL_cur_LC_ALL       indicates what the locale should be
     *                                      if it is a per-thread locale.
     */

    if (UNLIKELY(   PL_veto_switch_non_tTHX_context
                 || PL_phase == PERL_PHASE_CONSTRUCT))
    {
        return;
    }

#  ifdef USE_POSIX_2008_LOCALE

    if (! uselocale(PL_cur_locale_obj)) {
        locale_panic_(Perl_form(aTHX_
                                "Can't uselocale(%p), LC_ALL supposed to"
                                " be '%s'",
                                PL_cur_locale_obj, get_LC_ALL_display()));
    }

#  elif defined(WIN32)

    if (! bool_setlocale_c(LC_ALL, PL_cur_LC_ALL)) {
        locale_panic_(Perl_form(aTHX_ "Can't setlocale(%s)", PL_cur_LC_ALL));
    }

#  endif

}

#endif
#ifdef USE_THREADS

void
Perl_thread_locale_init(pTHX)
{

#  ifdef USE_THREAD_SAFE_LOCALE
#    ifdef USE_POSIX_2008_LOCALE

    /* Called from a thread on startup.
     *
     * The operations here have to be done from within the calling thread, as
     * they affect libc's knowledge of the thread; libc has no knowledge of
     * aTHX */

     DEBUG_L(PerlIO_printf(Perl_debug_log,
                           "new thread, initial locale is %s;"
                           " calling setlocale(LC_ALL, \"C\")\n",
                           get_LC_ALL_display()));

    if (! uselocale(PL_C_locale_obj)) {

        /* Not being able to change to the C locale is severe; don't keep
         * going.  */
        locale_panic_(Perl_form(aTHX_
                                "Can't uselocale(%p), 'C'", PL_C_locale_obj));
        NOT_REACHED; /* NOTREACHED */
    }

    PL_cur_locale_obj = PL_C_locale_obj;

#    elif defined(WIN32)

    /* On Windows, make sure new thread has per-thread locales enabled */
    if (_configthreadlocale(_ENABLE_PER_THREAD_LOCALE) == -1) {
        locale_panic_("_configthreadlocale returned an error");
    }
    void_setlocale_c(LC_ALL, "C");

#    endif
#  endif

}

void
Perl_thread_locale_term(pTHX)
{
    /* Called from a thread as it gets ready to terminate.
     *
     * The operations here have to be done from within the calling thread, as
     * they affect libc's knowledge of the thread; libc has no knowledge of
     * aTHX */

#  if defined(USE_POSIX_2008_LOCALE)

    /* Switch to the global locale, so can free up the per-thread object */
    locale_t actual_obj = uselocale(LC_GLOBAL_LOCALE);
    if (actual_obj != LC_GLOBAL_LOCALE && actual_obj != PL_C_locale_obj) {
        freelocale(actual_obj);
    }

    /* Prevent leaks even if something has gone wrong */
    locale_t expected_obj = PL_cur_locale_obj;
    if (UNLIKELY(   expected_obj != actual_obj
                 && expected_obj != LC_GLOBAL_LOCALE
                 && expected_obj != PL_C_locale_obj))
    {
        freelocale(expected_obj);
    }

    PL_cur_locale_obj = LC_GLOBAL_LOCALE;

#  endif
#  ifdef WIN32_USE_FAKE_OLD_MINGW_LOCALES

    /* When faking the mingw implementation, we coerce this function into doing
     * something completely different from its intent -- namely to free up our
     * static buffer to avoid a leak.  This function gets called for each
     * thread that is terminating, so will give us a chance to free the buffer
     * from the appropriate pool.  On unthreaded systems, it gets called by the
     * mutex termination code. */

    if (aTHX != wsetlocale_buf_aTHX) {
        return;
    }

    if (wsetlocale_buf_size > 0) {
        Safefree(wsetlocale_buf);
        wsetlocale_buf_size = 0;
    }

#  endif

}

#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
