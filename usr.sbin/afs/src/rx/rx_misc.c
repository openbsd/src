#include "rx_locl.h"

RCSID("$arla: rx_misc.c,v 1.6 2003/06/10 16:55:01 lha Exp $");

/*
 * We currently only include below the errors that
 * affect us the most. We should add to this list
 * more code mappings, as necessary.
 */

/*
 * Convert from the local (host) to the standard
 * (network) system error code.
 */
uint32_t
hton_syserr_conv(uint32_t code)
{
    uint32_t err;

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
uint32_t
ntoh_syserr_conv(uint32_t code)
{
    uint32_t err;

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
