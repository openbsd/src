/*	$OpenBSD: rx_misc.c,v 1.1.1.1 1998/09/14 21:53:15 art Exp $	*/
#include "rx_locl.h"

RCSID("$KTH: rx_misc.c,v 1.4 1998/02/22 19:47:15 joda Exp $");

/*
 * We currently only include below the errors that
 * affect us the most. We should add to this list
 * more code mappings, as necessary.
 */

/*
 * Convert from the local (host) to the standard
 * (network) system error code.
 */
int 
hton_syserr_conv(long code)
{
    register long err;

#if 0
    if (code == ENOSPC)
	err = VDISKFULL;
#ifndef	AFS_SUN5_ENV
    /* EDQUOT doesn't exist on solaris */
    else if (code == EDQUOT)
	err = VOVERQUOTA;
#endif
    else
#endif
	err = code;
    return err;
}


/*
 * Convert from the standard (Network) format to the
 * local (host) system error code.
 */
int 
ntoh_syserr_conv(long code)
{
    register long err;

#if 0
    if (code == VDISKFULL)
	err = ENOSPC;
    else if (code == VOVERQUOTA)
#ifdef	AFS_SUN5_ENV
	err = ENOSPC;
#else
	err = EDQUOT;
#endif
    else
#endif
	err = code;
    return err;
}


#ifndef	KERNEL
/*
 * We provide the following because some systems (like aix) would fail if we
 * pass 0 as length.
 */

#ifndef osi_alloc
static char memZero;

char *
osi_alloc(long x)
{

    /*
     * 0-length allocs may return NULL ptr from osi_kalloc, so we
     * special-case things so that NULL returned iff an error occurred
     */
    if (x == 0)
	return &memZero;
    return (char *) malloc(x);
}

void
osi_free(char *x, long size)
{
    if (x == &memZero)
	return;
    free(x);
}

#endif
#endif				       /* KERNEL */
