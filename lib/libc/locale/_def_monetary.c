/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _def_monetary.c,v 1.2 1996/08/19 08:28:14 tholo Exp $";
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
