/*    deb.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
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

    fprintf(stderr,"(%s:%ld)\t",
	SvTYPE(gv) == SVt_PVGV ? SvPVX(GvSV(gv)) : "<free>",
	(long)curcop->cop_line);
    for (i=0; i<dlevel; i++)
	fprintf(stderr,"%c%c ",debname[i],debdelim[i]);
    fprintf(stderr,pat,a1,a2,a3,a4,a5,a6,a7,a8);
}

#else /* !defined(I_STDARG) && !defined(I_VARARGS) */

#  ifdef I_STDARG
void
deb(char *pat, ...)
#  else
/*VARARGS1*/
void
deb(pat, va_alist)
    char *pat;
    va_dcl
#  endif
{
    va_list args;
    register I32 i;
    GV* gv = curcop->cop_filegv;

    fprintf(stderr,"(%s:%ld)\t",
	SvTYPE(gv) == SVt_PVGV ? SvPVX(GvSV(gv)) : "<free>",
	(long)curcop->cop_line);
    for (i=0; i<dlevel; i++)
	fprintf(stderr,"%c%c ",debname[i],debdelim[i]);

#  ifdef I_STDARG
    va_start(args, pat);
#  else
    va_start(args);
#  endif
    (void) vfprintf(stderr,pat,args);
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
    fprintf(stderr, "%8lx %8lx %8ld %8ld %8ld\n",
	(unsigned long)stack, (unsigned long)stack_base,
	(long)*markstack_ptr, (long)(stack_sp-stack_base),
	(long)(stack_max-stack_base));
    fprintf(stderr, "%8lx %8lx %8ld %8ld %8ld\n",
	(unsigned long)mainstack, (unsigned long)AvARRAY(stack),
	(long)mainstack, (long)AvFILL(stack), (long)AvMAX(stack));
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

    fprintf(stderr, i ? "    =>  ...  " : "    =>  ");
    if (stack_base[0] != &sv_undef || stack_sp < stack_base)
	fprintf(stderr, " [STACK UNDERFLOW!!!]\n");
    do {
	++i;
	if (markscan <= markstack_ptr && *markscan < i) {
	    do {
		++markscan;
		putc('*', stderr);
	    }
	    while (markscan <= markstack_ptr && *markscan < i);
	    fprintf(stderr, "  ");
	}
	if (i > top)
	    break;
	fprintf(stderr, "%-4s  ", SvPEEK(stack_base[i]));
    }
    while (1);
    fprintf(stderr, "\n");
    return 0;
}
#else
static int dummy; /* avoid totally empty deb.o file */
#endif /* DEBUGGING */
