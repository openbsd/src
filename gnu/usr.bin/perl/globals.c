/*    globals.c
 *
 *    Copyright (C) 1995, 1999, 2000, 2001, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "For the rest, they shall represent the other Free Peoples of the World:
 * Elves, Dwarves, and Men." --Elrond
 */

#include "INTERN.h"
#define PERL_IN_GLOBALS_C
#include "perl.h"

int
Perl_fprintf_nocontext(PerlIO *stream, const char *format, ...)
{
    dTHXs;
    va_list(arglist);
    va_start(arglist, format);
    return PerlIO_vprintf(stream, format, arglist);
}

int
Perl_printf_nocontext(const char *format, ...)
{
    dTHX;
    va_list(arglist);
    va_start(arglist, format);
    return PerlIO_vprintf(PerlIO_stdout(), format, arglist);
}

#include "perlapi.h"		/* bring in PL_force_link_funcs */
