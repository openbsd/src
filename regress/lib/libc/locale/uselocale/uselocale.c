/* $OpenBSD: uselocale.c,v 1.3 2017/08/16 01:40:30 schwarze Exp $ */
/*
 * Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/* Keep in sync with /usr/src/lib/libc/locale/rune.h. */
#define	_LOCALE_NONE	 (locale_t)0
#define	_LOCALE_C	 (locale_t)1
#define	_LOCALE_UTF8	 (locale_t)2
#define	_LOCALE_BAD	 (locale_t)3

/* Options for switch_thread() below. */
#define	SWITCH_SIGNAL	 1	/* Call pthread_cond_signal(3). */
#define	SWITCH_WAIT	 2	/* Call pthread_cond_timedwait(3). */

/* Options for TESTFUNC(). */
#define	TOPT_ERR	 (1 << 0)
#define	TOPT_STR	 (1 << 1)

/*
 * Generate one test function for a specific interface.
 * Fn =		function name
 * Ft =		function return type
 * FUNCPARA =	function parameter list with types and names
 * FUNCARGS =	function argument list, names only, no types
 * Af =		format string to print the arguments
 * Rf =		format string to print the return value
 * Op =		options for the test function, see above
 * line =	source code line number in this test file
 * ee =		expected error number
 * er =		expected return value
 * ar =		actual return value
 * errno =	actual error number (global)
 */
#define	TESTFUNC(Fn, Ft, Af, Rf, Op)					\
static void								\
_test_##Fn(int line, int ee, Ft er, FUNCPARA)				\
{									\
	Ft ar;								\
	errno = 0;							\
	ar = Fn(FUNCARGS);						\
	if (Op & TOPT_STR) {						\
		if (er == (Ft)NULL)					\
			er = (Ft)"NULL";				\
		if (ar == (Ft)NULL)					\
			ar = (Ft)"NULL";				\
	}								\
	if (Op & TOPT_STR ? strcmp((const char *)er, (const char *)ar)	\
	    : ar != er)							\
		errx(1, "[%d] %s(" Af ")=" Rf " [exp: " Rf "]",		\
		    line, #Fn, FUNCARGS, ar, er);			\
	if (Op & TOPT_ERR && errno != ee)				\
		errx(1, "[%d] %s(" Af ") errno=%d [exp: %d]",		\
		    line, #Fn, FUNCARGS, errno, ee);			\
}

/*
 * Test functions for all tested interfaces.
 */
#define	FUNCPARA	int mask, const char *locname
#define	FUNCARGS	mask, locname, _LOCALE_NONE
TESTFUNC(newlocale, locale_t, "%d, %s, %p", "%p", TOPT_ERR)

#define	FUNCPARA	locale_t locale
#define	FUNCARGS	locale
TESTFUNC(duplocale, locale_t, "%p", "%p", TOPT_ERR)
TESTFUNC(uselocale, locale_t, "%p", "%p", TOPT_ERR)

#define	FUNCPARA	int category, char *locname
#define	FUNCARGS	category, locname
TESTFUNC(setlocale, const char *, "%d, %s", "%s", TOPT_STR)

#define	FUNCPARA	nl_item item
#define	FUNCARGS	item
TESTFUNC(nl_langinfo, const char *, "%ld", "%s", TOPT_STR)

#define	FUNCPARA	nl_item item, locale_t locale
#define	FUNCARGS	item, locale
TESTFUNC(nl_langinfo_l, const char *, "%ld, %p", "%s", TOPT_STR)

#define	FUNCPARA	wint_t wc
#define	FUNCARGS	wc
TESTFUNC(iswalpha, int, "U+%.4X", "%d", 0)
TESTFUNC(towupper, wint_t, "U+%.4X", "U+%.4X", 0)

#define	FUNCPARA	wint_t wc, locale_t locale
#define	FUNCARGS	wc, locale
TESTFUNC(iswalpha_l, int, "U+%.4X, %p", "%d", 0)
TESTFUNC(towupper_l, wint_t, "U+%.4X, %p", "U+%.4X", 0)

static void
_test_MB_CUR_MAX(int line, int ee, size_t ar)
{
	if (MB_CUR_MAX != ar)
		errx(1, "[%d] MB_CUR_MAX=%zd [exp: %zd]",
		    line, MB_CUR_MAX, ar);
}

/*
 * Test macros:
 * TEST_R(funcname, er, arguments) if you expect errno == 0.
 * TEST_ER(funcname, ee, er, arguments) otherwise.
 */
#define	TEST_R(Fn, ...)		_test_##Fn(__LINE__, 0, __VA_ARGS__)
#define	TEST_ER(Fn, ...)	_test_##Fn(__LINE__, __VA_ARGS__)

/*
 * SWITCH_SIGNAL wakes the other thread.
 * SWITCH_WAIT goes to sleep.
 * Both can be combined.
 * The step argument is used for error reporting only.
 */
static void
switch_thread(int step, int flags)
{
	static pthread_mutexattr_t	 ma;
	static struct timespec		 t;
	static pthread_cond_t		*c;
	static pthread_mutex_t		*m;
	int				 irc;

	if (m == NULL) {
		if ((m = malloc(sizeof(*m))) == NULL)
			err(1, NULL);
		if ((irc = pthread_mutexattr_init(&ma)) != 0)
			errc(1, irc, "pthread_mutexattr_init");
		if ((irc = pthread_mutexattr_settype(&ma,
		    PTHREAD_MUTEX_STRICT_NP)) != 0)
			errc(1, irc, "pthread_mutexattr_settype");
		if ((irc = pthread_mutex_init(m, &ma)) != 0)
			errc(1, irc, "pthread_mutex_init");
	}
	if (c == NULL) {
		if ((c = malloc(sizeof(*c))) == NULL)
			err(1, NULL);
		if ((irc = pthread_cond_init(c, NULL)) != 0)
			errc(1, irc, "pthread_cond_init");
	}
	if (flags & SWITCH_SIGNAL) {
		if ((irc = pthread_cond_signal(c)) != 0)
			errc(1, irc, "pthread_cond_signal(%d)", step);
	}
	if (flags & SWITCH_WAIT) {
		if ((irc = pthread_mutex_trylock(m)) != 0)
			errc(1, irc, "pthread_mutex_trylock(%d)", step);
		t.tv_sec = time(NULL) + 2;
		if ((irc = pthread_cond_timedwait(c, m, &t)) != 0)
			errc(1, irc, "pthread_cond_timedwait(%d)", step);
		if ((irc = pthread_mutex_unlock(m)) != 0)
			errc(1, irc, "pthread_mutex_unlock(%d)", step);
	}
}

static void *
child_func(void *arg)
{
	/* Test invalid newlocale(3) arguments. */
	TEST_ER(newlocale, EINVAL, _LOCALE_NONE, LC_CTYPE_MASK, NULL);
	TEST_R(MB_CUR_MAX, 1);
	TEST_ER(newlocale, EINVAL, _LOCALE_NONE, LC_ALL_MASK + 1, "C.UTF-8");
	TEST_R(MB_CUR_MAX, 1);
	TEST_ER(newlocale, ENOENT, _LOCALE_NONE, LC_COLLATE_MASK, "C.INV");
	TEST_R(MB_CUR_MAX, 1);
	setenv("LC_TIME", "C.INV", 1);
	TEST_ER(newlocale, ENOENT, _LOCALE_NONE, LC_TIME_MASK, "");
	unsetenv("LC_TIME");
	TEST_R(MB_CUR_MAX, 1);
	setenv("LC_CTYPE", "C.INV", 1);
	TEST_ER(newlocale, ENOENT, _LOCALE_NONE, LC_CTYPE_MASK, "");
	TEST_R(MB_CUR_MAX, 1);

	/* Test duplocale(3). */
	TEST_ER(duplocale, EINVAL, _LOCALE_NONE, _LOCALE_UTF8);
	TEST_R(duplocale, _LOCALE_C, _LOCALE_C);
	TEST_R(duplocale, _LOCALE_C, LC_GLOBAL_LOCALE);

	/* Test premature UTF-8 uselocale(3). */
	TEST_ER(uselocale, EINVAL, _LOCALE_NONE, _LOCALE_UTF8);
	TEST_R(MB_CUR_MAX, 1);
	TEST_R(uselocale, LC_GLOBAL_LOCALE, _LOCALE_NONE);

	/* Test UTF-8 initialization. */
	setenv("LC_CTYPE", "C.UTF-8", 1);
	TEST_R(newlocale, _LOCALE_UTF8, LC_CTYPE_MASK, "");
	unsetenv("LC_CTYPE");
	TEST_R(MB_CUR_MAX, 1);
	TEST_R(duplocale, _LOCALE_UTF8, _LOCALE_UTF8);

	/* Test invalid uselocale(3) argument. */
	TEST_ER(uselocale, EINVAL, _LOCALE_NONE, _LOCALE_BAD);
	TEST_R(MB_CUR_MAX, 1);
	TEST_R(uselocale, LC_GLOBAL_LOCALE, _LOCALE_NONE);

	/* Test switching the thread locale. */
	TEST_R(uselocale, LC_GLOBAL_LOCALE, _LOCALE_UTF8);
	TEST_R(MB_CUR_MAX, 4);
	TEST_R(uselocale, _LOCALE_UTF8, _LOCALE_NONE);
	TEST_R(nl_langinfo, "UTF-8", CODESET);
	TEST_R(nl_langinfo_l, "UTF-8", CODESET, _LOCALE_UTF8);
	TEST_R(nl_langinfo_l, "US-ASCII", CODESET, _LOCALE_C);
	TEST_R(iswalpha, 1, 0x00E9);  /* e accent aigu */
	TEST_R(iswalpha_l, 1, 0x00E9, _LOCALE_UTF8);
	TEST_R(iswalpha_l, 0, 0x00E9, _LOCALE_C);
	TEST_R(iswalpha, 1, 0x0153);  /* ligature oe */
	TEST_R(iswalpha_l, 1, 0x0153, _LOCALE_UTF8);
	TEST_R(iswalpha_l, 0, 0x0153, _LOCALE_C);
	TEST_R(iswalpha, 0, 0x2205);  /* for all */
	TEST_R(iswalpha_l, 0, 0x2205, _LOCALE_UTF8);
	TEST_R(iswalpha_l, 0, 0x2205, _LOCALE_C);
	TEST_R(towupper, 0x00C9, 0x00E9);
	TEST_R(towupper_l, 0x00C9, 0x00E9, _LOCALE_UTF8);
	TEST_R(towupper_l, 0x00E9, 0x00E9, _LOCALE_C);
	TEST_R(towupper, 0x0152, 0x0153);
	TEST_R(towupper_l, 0x0152, 0x0153, _LOCALE_UTF8);
	TEST_R(towupper_l, 0x0153, 0x0153, _LOCALE_C);
	TEST_R(towupper, 0x2205, 0x2205);
	TEST_R(towupper_l, 0x2205, 0x2205, _LOCALE_UTF8);
	TEST_R(towupper_l, 0x2205, 0x2205, _LOCALE_C);

	/* Test non-ctype newlocale(3). */
	TEST_R(newlocale, _LOCALE_C, LC_MESSAGES_MASK, "en_US.UTF-8");

	/* Temporarily switch to the main thread. */
	switch_thread(2, SWITCH_SIGNAL | SWITCH_WAIT);

	/* Test displaying the global locale while a local one is set. */
	TEST_R(setlocale, "C/C.UTF-8/C/C/C/C", LC_ALL, NULL);

	/* Test switching the thread locale back. */
	TEST_R(MB_CUR_MAX, 4);
	TEST_R(duplocale, _LOCALE_UTF8, LC_GLOBAL_LOCALE);
	TEST_R(uselocale, _LOCALE_UTF8, _LOCALE_C);
	TEST_R(MB_CUR_MAX, 1);
	TEST_R(uselocale, _LOCALE_C, _LOCALE_NONE);

	/* Test switching back to the global locale. */
	TEST_R(uselocale, _LOCALE_C, LC_GLOBAL_LOCALE);
	TEST_R(MB_CUR_MAX, 4);
	TEST_R(uselocale, LC_GLOBAL_LOCALE, _LOCALE_NONE);

	/* Hand control back to the main thread. */
	switch_thread(4, SWITCH_SIGNAL);
	return NULL;
}

int
main(void)
{
	pthread_t	 child_thread;
	int		 irc;

	/* Initialize environment. */
	unsetenv("LC_ALL");
	unsetenv("LC_COLLATE");
	unsetenv("LC_CTYPE");
	unsetenv("LC_MONETARY");
	unsetenv("LC_NUMERIC");
	unsetenv("LC_TIME");
	unsetenv("LC_MESSAGES");
	unsetenv("LANG");

	/* First let the child do some tests. */
	if ((irc = pthread_create(&child_thread, NULL, child_func, NULL)) != 0)
		errc(1, irc, "pthread_create");
	switch_thread(1, SWITCH_WAIT);

	/* Check that the global locale is undisturbed. */
	TEST_R(setlocale, "C", LC_ALL, NULL);
	TEST_R(MB_CUR_MAX, 1);

	/* Test setting the globale locale. */
	TEST_R(setlocale, "C.UTF-8", LC_CTYPE, "C.UTF-8");
	TEST_R(MB_CUR_MAX, 4);
	TEST_R(uselocale, LC_GLOBAL_LOCALE, _LOCALE_NONE);

	/* Let the child do some more tests, then clean up. */
	switch_thread(3, SWITCH_SIGNAL);
	if ((irc = pthread_join(child_thread, NULL)) != 0)
		errc(1, irc, "pthread_join");
	return 0;
}
