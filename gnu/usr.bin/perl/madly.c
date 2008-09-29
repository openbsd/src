/*    madly.c
 *
 *    Copyright (c) 2004, 2005, 2006 Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 * 
 * Note that this file is essentially empty, and just #includes perly.c,
 * to allow compilation of a second parser, Perl_madparse, that is
 * identical to Perl_yyparse, but which includes extra code for dumping
 * the parse tree.  This is controlled by the PERL_IN_MADLY_C define.
 */

#define PERL_IN_MADLY_C

#include "perly.c"

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
