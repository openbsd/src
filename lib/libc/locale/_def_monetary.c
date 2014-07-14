/*	$OpenBSD: _def_monetary.c,v 1.5 2014/07/14 07:22:07 pelikan Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

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

	CHAR_MAX,	/* local p_cs_precedes */
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,

	CHAR_MAX,	/* intl p_cs_precedes */
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
};

const _MonetaryLocale *_CurrentMonetaryLocale = &_DefaultMonetaryLocale;
