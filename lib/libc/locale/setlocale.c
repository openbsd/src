/*	$OpenBSD: setlocale.c,v 1.10 2002/09/17 07:45:35 itojun Exp $	*/
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: setlocale.c,v 1.10 2002/09/17 07:45:35 itojun Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/localedef.h>
#include <locale.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Category names for getenv()
 */
static char *categories[_LC_LAST] = {
    "LC_ALL",
    "LC_COLLATE",
    "LC_CTYPE",
    "LC_MONETARY",
    "LC_NUMERIC",
    "LC_TIME",
    "LC_MESSAGES"
};

/*
 * Current locales for each category
 */
static char current_categories[_LC_LAST][32] = {
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
    "C"
};

/*
 * The locales we are going to try and load
 */
static char new_categories[_LC_LAST][32];

static char current_locale_string[_LC_LAST * 33];
static char *PathLocale;

static char	*currentlocale(void);
static char	*loadlocale(int);
static const char *__get_locale_env __P((int));

char *
setlocale(category, locale)
	int category;
	const char *locale;
{
	int i, loadlocale_success;
	size_t len;
	const char *env, *r;

	if (issetugid() != 0 ||
	    ((!PathLocale && !(PathLocale = getenv("PATH_LOCALE"))) ||
	     !*PathLocale))
		PathLocale = _PATH_LOCALE;

	if (category < 0 || category >= _LC_LAST)
		return (NULL);

	if (!locale)
		return (category ?
		    current_categories[category] : currentlocale());

	/*
	 * Default to the current locale for everything.
	 */
	for (i = 1; i < _LC_LAST; ++i)
		(void)strlcpy(new_categories[i], current_categories[i],
		    sizeof(new_categories[i]));

	/*
	 * Now go fill up new_categories from the locale argument
	 */
	if (!*locale) {
		if (category == LC_ALL) {
			for (i = 1; i < _LC_LAST; ++i) {
				env = __get_locale_env(i);
				(void)strlcpy(new_categories[i], env,
				    sizeof(new_categories[i]));
			}
		}
		else {
			env = __get_locale_env(category);
			(void)strlcpy(new_categories[category], env,
				sizeof(new_categories[category]));
		}
	} else if (category) {
		(void)strlcpy(new_categories[category], locale,
		    sizeof(new_categories[category]));
	} else {
		if ((r = strchr(locale, '/')) == 0) {
			for (i = 1; i < _LC_LAST; ++i) {
				(void)strlcpy(new_categories[i], locale,
				    sizeof(new_categories[i]));
			}
		} else {
			for (i = 1;;) {
				if (*locale == '/')
					return (NULL);	/* invalid format. */
				len = r - locale;
				if (len + 1 > sizeof(new_categories[i]))
					return (NULL);	/* too long */
				(void)memcpy(new_categories[i], locale, len);
				new_categories[i][len] = '\0';
				if (*r == 0)
					break;
				if (*(locale = ++r) == 0)
					/* slash followed by NUL */
					return (NULL);
				/* skip until NUL or '/' */
				while (*r && *r != '/')
					r++;
				if (++i == _LC_LAST)
					return (NULL);	/* too many slashes. */
			}
			if (i + 1 != _LC_LAST)
				return (NULL);	/* too few slashes. */
		}
	}

	if (category)
		return (loadlocale(category));

	loadlocale_success = 0;
	for (i = 1; i < _LC_LAST; ++i) {
		if (loadlocale(i) != NULL)
			loadlocale_success = 1;
	}

	/*
	 * If all categories failed, return NULL; we don't need to back
	 * changes off, since none happened.
	 */
	if (!loadlocale_success)
		return NULL;

	return (currentlocale());
}

static char *
currentlocale()
{
	int i;

	(void)strlcpy(current_locale_string, current_categories[1],
	    sizeof(current_locale_string));

	for (i = 2; i < _LC_LAST; ++i)
		if (strcmp(current_categories[1], current_categories[i])) {
			(void)snprintf(current_locale_string,
			    sizeof(current_locale_string), "%s/%s/%s/%s/%s/%s",
			    current_categories[1], current_categories[2],
			    current_categories[3], current_categories[4],
			    current_categories[5], current_categories[6]);
			break;
		}
	return (current_locale_string);
}

static char *
loadlocale(category)
	int category;
{
	char name[PATH_MAX];

	if (strcmp(new_categories[category],
	    current_categories[category]) == 0)
		return (current_categories[category]);

	if (!strcmp(new_categories[category], "C") ||
	    !strcmp(new_categories[category], "POSIX")) {

		/*
		 * Some day this will need to reset the locale to the default
		 * C locale.  Since we have no way to change them as of yet,
		 * there is no need to reset them.
		 */
		(void)strlcpy(current_categories[category],
		    new_categories[category],
		    sizeof(current_categories[category]));
		return (current_categories[category]);
	}

	/*
	 * Some day we will actually look at this file.
	 */
	(void)snprintf(name, sizeof(name), "%s/%s/%s",
	    PathLocale, new_categories[category], categories[category]);

	switch (category) {
	case LC_CTYPE:
	case LC_COLLATE:
	case LC_MESSAGES:
	case LC_MONETARY:
	case LC_NUMERIC:
	case LC_TIME:
		return (NULL);
	}

	(void)strlcpy(current_categories[category],
	    new_categories[category], sizeof(current_categories[category]));
	return current_categories[category];
}

static const char *
__get_locale_env(category)
	int category;
{
	const char *env;

	/* 1. check LC_ALL. */
	env = getenv(categories[0]);

	/* 2. check LC_* */
	if (!env || !*env)
		env = getenv(categories[category]);

	/* 3. check LANG */
	if (!env || !*env)
		env = getenv("LANG");

	/* 4. if none is set, fall to "C" */
	if (!env || !*env || strchr(env, '/'))
		env = "C";

	return env;
}
