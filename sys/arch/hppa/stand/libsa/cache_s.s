/*	$OpenBSD: cache_s.s,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $	*/
/*	$NOWHERE: cache_s.s,v 2.1 1998/06/22 19:34:46 mickey Exp $	*/

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
 * $Log: cache_s.s,v $
 * Revision 1.1.1.1  1998/06/23 18:46:42  mickey
 * ok, it boots, include and libkern to come
 *
 * Revision 2.1  1998/06/22 19:34:46  mickey
 * add cache manipulating routines
 *
 * Revision 1.1.2.1  1996/08/19  07:47:16  bruel
 * 	First revision
 * 	[1996/08/02  09:17:26  bruel]
 *
 * Revision 1.1.1.2  1996/08/02  09:17:26  bruel
 * 	First revision
 *
 * Revision 1.1.2.2  91/11/20  16:21:39  sharpe
 * 	Initial version from DSEE
 * 	[91/11/20  16:08:35  sharpe]
 * 
 */

/*
 * FlushDCache(start,size)
 * Stolen from pmap_fdcache.
 * void FlushDCache(space, start, end) - Flush the data cache.
 *
 * This routine flushes the given range of virtual addresses, from start (inclusive)
 * to end (exclusive) from the data cache.
 *
 */
        .space  $TEXT$
        .subspa $CODE$
	
	.export FlushDCache,entry
        .proc
        .callinfo
FlushDCache
	.entry
	
        comb,=	%arg1,%r0,FDCdone	/* If len=0, then done */
	ldi	0x10,%r21
	ldi	0x0f,%r22
	add	%arg0,%arg1,%r20
        andcm   %arg0,%r22,%arg0	/* Truncate lower bound to stridesize boundary */
	sub	%r20,%arg0,%arg1
	ldi	0xff,%r22
	add	%arg1,%r22,%arg1
	andcm	%arg1,%r22,%arg1
	add	%arg0,%arg1,%r20	/* Round up upper bound */
        fdc,m   %r21(%arg0)
FDCloop
        fdc,m   %r21(%arg0)		/* Flush block */
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        fdc,m   %r21(%arg0)                     
        comb,<<,n %arg0,%r20,FDCloop	/* Continue until fstart = fend */
        fdc,m   %r21(%arg0)
FDCdone
        bv      0(%rp)			/* return */
        nop
	.exit
	.procend
	
	.export FlushICache,entry
        .proc
        .callinfo
FlushICache
	.entry
	
        comb,=	%arg1,%r0,PICdone	/* If len=0, then done */
	ldi	0x10,%r21
	ldi	0x0f,%r22
	add	%arg0,%arg1,%r20
        andcm   %arg0,%r22,%arg0	/* Truncate lower bound to stridesize boundary */
	sub	%r20,%arg0,%arg1
	ldi	0xff,%r22
	add	%arg1,%r22,%arg1
	andcm	%arg1,%r22,%arg1
	add	%arg0,%arg1,%r20	/* Round up upper bound */
        fic,m   %r21(%arg0)
PICloop
        fic,m   %r21(%arg0)		/* Flush block */
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        fic,m   %r21(%arg0)                     
        comb,<<,n %arg0,%r20,PICloop	/* Continue until fstart = fend */
        fic,m   %r21(%arg0)
PICdone
        bv      0(%rp)			/* return */
        nop
	.exit
	.procend

/*
 * void sync_caches - Synchronize the cache.
 *
 * This routine executes a sync instruction and executes 7 nops.
 * Intended to be used with kdb when setting breakpoints.
 * Stolen from pmap_as.s.
 */
	.export sync_caches,entry
	.proc
	.callinfo
sync_caches
	.entry
	
        sync                                            /* Sync access */
        nop                                             /* voodoo */
        nop
        nop
        nop
        nop
        nop
        bv      0(%rp)
        nop
	.exit
	.procend

/*
 * void fdce(space, offset) - Perform fdce operation.
 *
 * This routine is called by pmap_fcacheall to whack the data cache.  Must
 * be only used inside an architectured loop.
 */
	.export fdce,entry
	.proc
	.callinfo
fdce
	.entry	

        fdce    0(0,%arg1)	/* Space does not make a difference */
        sync
        bv      0(%rp)
        nop
	.exit
	.procend

/*
 * void fice(space, offset) - Perform fice operation.
 *          
 * This routine is called by pmap_fcacheall to whack the instruction cache.
 * Must be only used inside an architectured loop
 */
	.export fice,entry
	.proc
	.callinfo
fice
	.entry	
        fice    0(0,%arg1)	/* Space does not make a difference */
        sync
        bv      0(%rp)
	.exit
	.procend

	.end

