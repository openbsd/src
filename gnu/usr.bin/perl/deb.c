/*    deb.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Didst thou think that the eyes of the White Tower were blind?  Nay, I
 * have seen more than thou knowest, Gray Fool."  --Denethor
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef DEBUGGING
#if !defined(I_STDARG) && !defined(I_VARARGS)

/*
 * Fallback on the old hackers way of doing varargs
 */

/*VARARGS1*/
void
deb(pat,a1,a2,a3,a4,a5,a6,a7,a8)
    char *pat;
{
    register I32 i;
    GV* gv = curcop->cop_filegv;

    PerlIO_printf(Perl_debug_log, "(%s:%ld)\t",
	SvTYPE(gv) == SVt_PVGV ? SvPVX(GvSV(gv)) : "<free>",
	(long)curcop->cop_line);
    for (i=0; i<dlevel; i++)
	PerlIO_printf(Perl_debug_log, "%c%c ",debname[i],debdelim[i]);
    PerlIO_printf(Perl_debug_log, pat,a1,a2,a3,a4,a5,a6,a7,a8);
}

#else /* !defined(I_STDARG) && !defined(I_VARARGS) */

#  ifdef I_STDARG
void
deb(const char *pat, ...)
#  else
/*VARARGS1*/
void
deb(pat, va_alist)
    const char *pat;
    va_dcl
#  endif
{
    va_list args;
    register I32 i;
    GV* gv = curcop->cop_filegv;

    PerlIO_printf(Perl_debug_log, "(%s:%ld)\t",
	SvTYPE(gv) == SVt_PVGV ? SvPVX(GvSV(gv)) : "<free>",
	(long)curcop->cop_line);
    for (i=0; i<dlevel; i++)
	PerlIO_printf(Perl_debug_log, "%c%c ",debname[i],debdelim[i]);

#  ifdef I_STDARG
    va_start(args, pat);
#  else
    va_start(args);
#  endif
    (void) PerlIO_vprintf(Perl_debug_log,pat,args);
    va_end( args );
}
#endif /* !defined(I_STDARG) && !defined(I_VARARGS) */

void
deb_growlevel()
{
    dlmax += 128;
    Renew(debname, dlmax, char);
    Renew(debdelim, dlmax, char);
}

I32
debstackptrs()
{
    PerlIO_printf(Perl_debug_log, "%8lx %8lx %8ld %8ld %8ld\n",
	(unsigned long)curstack, (unsigned long)stack_base,
	(long)*markstack_ptr, (long)(stack_sp-stack_base),
	(long)(stack_max-stack_base));
    PerlIO_printf(Perl_debug_log, "%8lx %8lx %8ld %8ld %8ld\n",
	(unsigned long)mainstack, (unsigned long)AvARRAY(curstack),
	(long)mainstack, (long)AvFILL(curstack), (long)AvMAX(curstack));
    return 0;
}

I32
debstack()
{
    I32 top = stack_sp - stack_base;
    register I32 i = top - 30;
    I32 *markscan = markstack;

    if (i < 0)
	i = 0;
    
    while (++markscan <= markstack_ptr)
	if (*markscan >= i)
	    break;

    PerlIO_printf(Perl_debug_log, i ? "    =>  ...  " : "    =>  ");
    if (stack_base[0] != &sv_undef || stack_sp < stack_base)
	PerlIO_printf(Perl_debug_log, " [STACK UNDERFLOW!!!]\n");
    do {
	++i;
	if (markscan <= markstack_ptr && *markscan < i) {
	    do {
		++markscan;
		PerlIO_putc(Perl_debug_log, '*');
	    }
	    while (markscan <= markstack_ptr && *markscan < i);
	    PerlIO_printf(Perl_debug_log, "  ");
	}
	if (i > top)
	    break;
	PerlIO_printf(Perl_debug_log, "%-4s  ", SvPEEK(stack_base[i]));
    }
    while (1);
    PerlIO_printf(Perl_debug_log, "\n");
    return 0;
}
#else
static int dummy; /* avoid totally empty deb.o file */
#endif /* DEBUGGING */
