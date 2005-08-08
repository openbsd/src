/*	$OpenBSD: _def_numeric.c,v 1.4 2005/08/08 08:05:35 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <sys/localedef.h>
#include <locale.h>

const _NumericLocale _DefaultNumericLocale =
{
	".",
	"",
	""
};

const _NumericLocale *_CurrentNumericLocale = &_DefaultNumericLocale;
