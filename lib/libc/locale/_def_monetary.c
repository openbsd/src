/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _def_monetary.c,v 1.3 1997/07/23 21:14:38 kstailey Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/localedef.h>
#include <limits.h>
#include <locale.h>

const _MonetaryLocale _DefaultMonetaryLocale =
{
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX
};

const _MonetaryLocale *_CurrentMonetaryLocale = &_DefaultMonetaryLocale;
