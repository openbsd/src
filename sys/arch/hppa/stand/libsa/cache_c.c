/*	$OpenBSD: cache_c.c,v 1.2 1998/07/08 21:34:31 mickey Exp $	*/
/*	$NOWHERE: cache_c.c,v 2.1 1998/06/22 19:34:46 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * pmk1.1
 */
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */
/*
 * HISTORY
 * $Log: cache_c.c,v $
 * Revision 1.2  1998/07/08 21:34:31  mickey
 * use those new pdc call types
 *
 * Revision 1.1.1.1  1998/06/23 18:46:41  mickey
 * ok, it boots, include and libkern to come
 *
 * Revision 2.1  1998/06/22 19:34:46  mickey
 * add cache manipulating routines
 *
 * Revision 1.1.2.1  1996/08/19  07:46:48  bruel
 * 	First revision
 * 	[1996/08/02  09:17:19  bruel]
 *
 * Revision 1.1.1.2  1996/08/02  09:17:19  bruel
 * 	First revision
 *
 * Revision 1.1.2.2  91/11/20  16:21:21  sharpe
 * 	Initial version from DSEE
 * 	[91/11/20  16:08:06  sharpe]
 * 
 */

/*
 * Stolen - Lock, stock, and barrel from tmm's pmap* .
 */

#include "libsa.h"
#include <machine/pdc.h>

void
fall(c_base, c_count, c_loop, c_stride, rot)
	int c_base, c_count, c_loop, c_stride; 
	void (*rot)();
{
        int addr, count, loop;                  /* Internal vars */

        addr = c_base;
        for (count = 0; count < c_count; count++) {
                for (loop = 0; loop < c_loop; loop++) {
                        (*rot)(0, addr);
                }
                addr += c_stride;
        }
        
}

/*
 * fcacheall - Flush all caches.
 *
 * This routine is just a wrapper around the real cache flush routine.
 * 
 * Parameters:
 *              None.
 *
 * Returns:
 *              Hopefully.
 */
struct pdc_cache pdc_cacheinfo __attribute__ ((aligned(8)));

void 
fcacheall()
{
        extern int fice();
        extern int fdce();
	int err;

        err = (*pdc)(PDC_CACHE, PDC_CACHE_DFLT, &pdc_cacheinfo);
        if (err) {
#ifdef DEBUG
		if (debug)
			printf("fcacheall: PDC_CACHE failed (%d).\n", err);
#endif
		return;
        }

        /*
         * Flush the instruction, then data cache.
         */
        fall(pdc_cacheinfo.ic_base, pdc_cacheinfo.ic_count, pdc_cacheinfo.ic_loop,
                  pdc_cacheinfo.ic_stride, fice);
	sync_caches();
        fall(pdc_cacheinfo.dc_base, pdc_cacheinfo.dc_count, pdc_cacheinfo.dc_loop,
                  pdc_cacheinfo.dc_stride, fdce);
	sync_caches();
}

