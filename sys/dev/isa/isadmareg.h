/*	$OpenBSD: isadmareg.h,v 1.2 1996/11/29 22:55:02 niklas Exp $	*/
/*	$NetBSD: isadmareg.h,v 1.4 1995/06/28 04:31:48 cgd Exp $	*/

#include <dev/ic/i8237reg.h>

/*
 * Register definitions for the DMA controllers
 */
#define	DMA_CHN(u, c)	((u) * (2 * (c)))	/* addr reg for channel c */
#define	DMA_SR(u)	((u) * 8)		/* status register */
#define	DMA_SMSK(u)	((u) * 10)		/* single mask register */
#define	DMA_MODE(u)	((u) * 11)		/* mode register */
#define	DMA_FFC(u)	((u) * 12)		/* clear first/last FF */

#define DMA_NREGS(u)	((u) * 13)		/* XXX true? */	
