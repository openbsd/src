/* $OpenBSD: uselocale.c,v 1.1 2017/08/10 14:45:42 schwarze Exp $ */
/*
 * Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2015 Sebastien Marie <semarie@openbsd.org>
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
#include <locale.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define	_LOCALE_NONE	 (locale_t)0
#define _LOCALE_C	 (locale_t)1
#define _LOCALE_UTF8	 (locale_t)2
#define _LOCALE_BAD	 (locale_t)3

#define	SWITCH_SIGNAL	 1
#define SWITCH_WAIT	 2

/*
 * test helpers for __LINE__
 */
#define test_newlocale(_e, _ee, _m, _l) \
	_test_newlocale(_e, _ee, _m, _l, __LINE__)
#define	test_duplocale(_e, _ee, _l) _test_duplocale(_e, _ee, _l, __LINE__)
#define	test_uselocale(_e, _ee, _l) _test_uselocale(_e, _ee, _l, __LINE__)
#define test_setlocale(_e, _c, _l) _test_setlocale(_e, _c, _l, __LINE__)
#define test_MB_CUR_MAX(_e) _test_MB_CUR_MAX(_e, __LINE__)


static void
_test_newlocale(locale_t expected, int exp_err,
    int mask, const char *locname, int line)
{
	locale_t result = newlocale(mask, locname, _LOCALE_NONE);

	if (result != expected)
		errx(1, "[%d] newlocale(%d, \"%s\")=\"%d\" [expected: \"%d\"]",
		    line, mask, locname, (int)result, (int)expected);
	if (errno != exp_err)
		errx(1, "[%d] newlocale(%d, \"%s\") errno=\"%d\" [expected:"
		    " \"%d\"]", line, mask, locname, errno, exp_err);
	errno = 0;
}

static void
_test_duplocale(locale_t expected, int exp_err, locale_t oldloc, int line)
{
	locale_t result = duplocale(oldloc);

	if (result != expected)
		errx(1, "[%d] duplocale(%d)=\"%d\" [expected: \"%d\"]",
		    line, (int)oldloc, (int)result, (int)expected);
	if (errno != exp_err)
		errx(1, "[%d] duplocale(%d) errno=\"%d\" [expected:"
		    " \"%d\"]", line, (int)oldloc, errno, exp_err);
	errno = 0;
}

static void
_test_uselocale(locale_t expected, int exp_err, locale_t newloc, int line)
{
	locale_t result = uselocale(newloc);

	if (result != expected)
		errx(1, "[%d] uselocale(%d)=\"%d\" [expected: \"%d\"]",
		    line, (int)newloc, (int)result, (int)expected);
	if (errno != exp_err)
		errx(1, "[%d] uselocale(%d) errno=\"%d\" [expected:"
		    " \"%d\"]", line, (int)newloc, errno, exp_err);
	errno = 0;
}

static void
_test_setlocale(const char *expected, int category, char *locale, int line)
{
	const char *result = setlocale(category, locale);

	if (expected == NULL)
		expected = "(null)";
	if (result == NULL)
		result = "(null)";
	if (strcmp(expected, result) != 0)
		errx(1, "[%d] setlocale(%d, \"%s\")=\"%s\" [expected: \"%s\"]",
		    line, category, locale, result, expected);
	errno = 0;
}

static void
_test_MB_CUR_MAX(size_t expected, int line)
{
	if (MB_CUR_MAX != expected)
		errx(1, "[%d] MB_CUR_MAX=%ld [expected %ld]", 
			line, MB_CUR_MAX, expected);
}

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
	test_newlocale(_LOCALE_NONE, EINVAL, LC_CTYPE_MASK, NULL);
	test_MB_CUR_MAX(1);
	test_newlocale(_LOCALE_NONE, EINVAL, LC_ALL_MASK + 1, "C.UTF-8");
	test_MB_CUR_MAX(1);
	test_newlocale(_LOCALE_NONE, ENOENT, LC_COLLATE_MASK, "C.INV");
	test_MB_CUR_MAX(1);
	setenv("LC_TIME", "C.INV", 1);
	test_newlocale(_LOCALE_NONE, ENOENT, LC_TIME_MASK, "");
	unsetenv("LC_TIME");
	test_MB_CUR_MAX(1);
	setenv("LC_CTYPE", "C.INV", 1);
	test_newlocale(_LOCALE_NONE, ENOENT, LC_CTYPE_MASK, "");
	test_MB_CUR_MAX(1);

	/* Test duplocale(3). */
	test_duplocale(_LOCALE_NONE, EINVAL, _LOCALE_UTF8);
	test_duplocale(_LOCALE_C, 0, _LOCALE_C);
	test_duplocale(_LOCALE_C, 0, LC_GLOBAL_LOCALE);

	/* Test premature UTF-8 uselocale(3). */
	test_uselocale(_LOCALE_NONE, EINVAL, _LOCALE_UTF8);
	test_MB_CUR_MAX(1);
	test_uselocale(LC_GLOBAL_LOCALE, 0, _LOCALE_NONE);

	/* Test UTF-8 initialization. */
	setenv("LC_CTYPE", "C.UTF-8", 1);
	test_newlocale(_LOCALE_UTF8, 0, LC_CTYPE_MASK, "");
	unsetenv("LC_CTYPE");
	test_MB_CUR_MAX(1);
	test_duplocale(_LOCALE_UTF8, 0, _LOCALE_UTF8);

	/* Test invalid uselocale(3) argument. */
	test_uselocale(_LOCALE_NONE, EINVAL, _LOCALE_BAD);
	test_MB_CUR_MAX(1);
	test_uselocale(LC_GLOBAL_LOCALE, 0, _LOCALE_NONE);

	/* Test switching the thread locale. */
	test_uselocale(LC_GLOBAL_LOCALE, 0, _LOCALE_UTF8);
	test_MB_CUR_MAX(4);
	test_uselocale(_LOCALE_UTF8, 0, _LOCALE_NONE);

	/* Test non-ctype newlocale(3). */
	test_newlocale(_LOCALE_C, 0, LC_MESSAGES_MASK, "en_US.UTF-8");

	/* Temporarily switch to the main thread. */
	switch_thread(2, SWITCH_SIGNAL | SWITCH_WAIT);

	/* Test displaying the global locale while a local one is set. */
	test_setlocale("C/C.UTF-8/C/C/C/C", LC_ALL, NULL);

	/* Test switching the thread locale back. */
	test_MB_CUR_MAX(4);
	test_duplocale(_LOCALE_UTF8, 0, LC_GLOBAL_LOCALE);
	test_uselocale(_LOCALE_UTF8, 0, _LOCALE_C);
	test_MB_CUR_MAX(1);
	test_uselocale(_LOCALE_C, 0, _LOCALE_NONE);

	/* Test switching back to the global locale. */
	test_uselocale(_LOCALE_C, 0, LC_GLOBAL_LOCALE);
	test_MB_CUR_MAX(4);
	test_uselocale(LC_GLOBAL_LOCALE, 0, _LOCALE_NONE);

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
	test_setlocale("C", LC_ALL, NULL);
	test_MB_CUR_MAX(1);

	/* Test setting the globale locale. */
	test_setlocale("C.UTF-8", LC_CTYPE, "C.UTF-8");
	test_MB_CUR_MAX(4);
	test_uselocale(LC_GLOBAL_LOCALE, 0, _LOCALE_NONE);

	/* Let the child do some more tests, then clean up. */
	switch_thread(3, SWITCH_SIGNAL);
	if ((irc = pthread_join(child_thread, NULL)) != 0)
		errc(1, irc, "pthread_join");
	return 0;
}
