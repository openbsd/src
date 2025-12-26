/* Replaces <langinfo.h>, and allows our code to work on systems that don't
 * have that. */

#ifndef PERL_LANGINFO_H
#define PERL_LANGINFO_H 1

#include "config.h"

#if defined(I_LANGINFO)
#   include <langinfo.h>
#else

typedef int nl_item;    /* Substitute 'int' for emulated nl_langinfo() */

#endif

/* NOTE that this file is parsed by ext/XS-APItest/t/locale.t, so be careful
 * with changes */

/* If foo doesn't exist define it to a negative number. */

#ifndef CODESET
#  define CODESET -1
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef D_T_FMT
#  define D_T_FMT -2
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef D_FMT
#  define D_FMT -3
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef T_FMT
#  define T_FMT -4
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef T_FMT_AMPM
#  define T_FMT_AMPM -5
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef AM_STR
#  define AM_STR -6
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef PM_STR
#  define PM_STR -7
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_1
#  define DAY_1 -8
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_2
#  define DAY_2 -9
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_3
#  define DAY_3 -10
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_4
#  define DAY_4 -11
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_5
#  define DAY_5 -12
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_6
#  define DAY_6 -13
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef DAY_7
#  define DAY_7 -14
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_1
#  define ABDAY_1 -15
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_2
#  define ABDAY_2 -16
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_3
#  define ABDAY_3 -17
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_4
#  define ABDAY_4 -18
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_5
#  define ABDAY_5 -19
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_6
#  define ABDAY_6 -20
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABDAY_7
#  define ABDAY_7 -21
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_1
#  define MON_1 -22
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_2
#  define MON_2 -23
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_3
#  define MON_3 -24
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_4
#  define MON_4 -25
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_5
#  define MON_5 -26
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_6
#  define MON_6 -27
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_7
#  define MON_7 -28
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_8
#  define MON_8 -29
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_9
#  define MON_9 -30
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_10
#  define MON_10 -31
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_11
#  define MON_11 -32
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef MON_12
#  define MON_12 -33
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_1
#  define ABMON_1 -34
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_2
#  define ABMON_2 -35
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_3
#  define ABMON_3 -36
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_4
#  define ABMON_4 -37
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_5
#  define ABMON_5 -38
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_6
#  define ABMON_6 -39
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_7
#  define ABMON_7 -40
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_8
#  define ABMON_8 -41
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_9
#  define ABMON_9 -42
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_10
#  define ABMON_10 -43
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_11
#  define ABMON_11 -44
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ABMON_12
#  define ABMON_12 -45
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ERA
#  define ERA -46
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ERA_D_FMT
#  define ERA_D_FMT -47
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ERA_D_T_FMT
#  define ERA_D_T_FMT -48
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ERA_T_FMT
#  define ERA_T_FMT -49
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef ALT_DIGITS
#  define ALT_DIGITS -50
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef RADIXCHAR
#  define RADIXCHAR -51
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef THOUSEP
#  define THOUSEP -52
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef YESEXPR
#  define YESEXPR -53
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef YESSTR
#  define YESSTR -54
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef NOEXPR
#  define NOEXPR -55
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef NOSTR
#  define NOSTR -56
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#ifndef CRNCYSTR
#  define CRNCYSTR -57
#  define HAS_MISSING_LANGINFO_ITEM_
#endif

/* The rest of the items are gnu extensions, and are not #defined by its
 * langinfo.h.  There is a slight possibility that one of these numbers could
 * conflict with some other value, in which case after much gnashing of teeth
 * you will find this comment, and end up having to adjust the numbers.   But
 * glibc values are not (so far) negative.
 *
 * Android is special in that it doesn't define any of these, but has the
 * corresponding category */
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_ADDRESS)
#  define _NL_ADDRESS_POSTAL_FMT   -58
#  define _NL_ADDRESS_COUNTRY_NAME -59
#  define _NL_ADDRESS_COUNTRY_POST -60
#  define _NL_ADDRESS_COUNTRY_AB2  -61
#  define _NL_ADDRESS_COUNTRY_AB3  -62
#  define _NL_ADDRESS_COUNTRY_CAR  -63
#  define _NL_ADDRESS_COUNTRY_NUM  -64
#  define _NL_ADDRESS_COUNTRY_ISBN -65
#  define _NL_ADDRESS_LANG_NAME    -66
#  define _NL_ADDRESS_LANG_AB      -67
#  define _NL_ADDRESS_LANG_TERM    -68
#  define _NL_ADDRESS_LANG_LIB     -69

#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_IDENTIFICATION)
#  define _NL_IDENTIFICATION_TITLE        -70
#  define _NL_IDENTIFICATION_SOURCE       -71
#  define _NL_IDENTIFICATION_ADDRESS      -72
#  define _NL_IDENTIFICATION_CONTACT      -73
#  define _NL_IDENTIFICATION_EMAIL        -74
#  define _NL_IDENTIFICATION_TEL          -75
#  define _NL_IDENTIFICATION_FAX          -76
#  define _NL_IDENTIFICATION_LANGUAGE     -77
#  define _NL_IDENTIFICATION_TERRITORY    -78
#  define _NL_IDENTIFICATION_AUDIENCE     -79
#  define _NL_IDENTIFICATION_APPLICATION  -80
#  define _NL_IDENTIFICATION_ABBREVIATION -81
#  define _NL_IDENTIFICATION_REVISION     -82
#  define _NL_IDENTIFICATION_DATE         -83
#  define _NL_IDENTIFICATION_CATEGORY     -84

#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_MEASUREMENT)
#  define _NL_MEASUREMENT_MEASUREMENT -85
#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_NAME)
#  define _NL_NAME_NAME_FMT     -86
#  define _NL_NAME_NAME_GEN     -87
#  define _NL_NAME_NAME_MR      -88
#  define _NL_NAME_NAME_MRS     -89
#  define _NL_NAME_NAME_MISS    -90
#  define _NL_NAME_NAME_MS      -91

#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_PAPER)
#  define _NL_PAPER_HEIGHT  -92
#  define _NL_PAPER_WIDTH   -93

#  define HAS_MISSING_LANGINFO_ITEM_
#endif
#if defined(__ANDROID__) || ! defined(HAS_NL_LANGINFO) || ! defined(LC_TELEPHONE)
#  define _NL_TELEPHONE_TEL_INT_FMT  -94
#  define _NL_TELEPHONE_TEL_DOM_FMT  -95
#  define _NL_TELEPHONE_INT_SELECT   -96
#  define _NL_TELEPHONE_INT_PREFIX   -97

#  define HAS_MISSING_LANGINFO_ITEM_
#endif

/* All these categories have to be emulated if not available on the platform */
#if ! LC_CTYPE_AVAIL_           \
 || ! LC_MESSAGES_AVAIL_        \
 || ! LC_MONETARY_AVAIL_        \
 || ! LC_NUMERIC_AVAIL_         \
 || ! LC_TIME_AVAIL_            \
 || ! LC_ADDRESS_AVAIL_         \
 || ! LC_IDENTIFICATION_AVAIL_  \
 || ! LC_MEASUREMENT_AVAIL_     \
 || ! LC_NAME_AVAIL_            \
 || ! LC_PAPER_AVAIL_           \
 || ! LC_TELEPHONE_AVAIL_

#  define HAS_MISSING_LANGINFO_ITEM_
#endif

#endif  /* PERL_LANGINFO_H */
