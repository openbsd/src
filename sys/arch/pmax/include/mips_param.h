/*	$NetBSD: mips_param.h,v 1.1 1996/05/19 17:52:18 jonathan Exp $	*/

#ifndef __MIPS_PARAM_H__
#define __MIPS_PARAM_H__

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	7
#define	ALIGN(p)	(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/4)

#define NBSEG		0x400000	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	22		/* LOG2(NBSEG) */

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(6*1024*1024/CLBYTES)
#endif

/* pages ("clicks") (4096 bytes) to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Mach derived conversion macros
 */
#define mips_round_page(x)	((((unsigned)(x)) + NBPG - 1) & ~(NBPG-1))
#define mips_trunc_page(x)	((unsigned)(x) & ~(NBPG-1))
#define mips_btop(x)		((unsigned)(x) >> PGSHIFT)
#define mips_ptob(x)		((unsigned)(x) << PGSHIFT)

#ifdef _KERNEL
#ifndef _LOCORE
typedef int spl_t;
extern spl_t splx __P((spl_t));
extern spl_t splsoftnet __P((void)), splsoftclock __P((void));
extern spl_t splhigh __P((void));
extern spl_t spl0 __P((void));	/* XXX should not enable TC on 3min */

extern void setsoftnet __P((void)), clearsoftnet __P((void));
extern void setsoftclock __P((void)), clearsoftclock __P((void));


extern int (*Mach_splnet) __P((void)), (*Mach_splbio) __P((void)),
	   (*Mach_splimp) __P((void)), (*Mach_spltty) __P((void)),
	   (*Mach_splclock) __P((void)), (*Mach_splstatclock) __P((void)),
	   (*Mach_splnone) __P((void));
#define	splnet()	((*Mach_splnet)())
#define	splbio()	((*Mach_splbio)())
#define	splimp()	((*Mach_splimp)())
#define	spltty()	((*Mach_spltty)())
#define	splclock()	((*Mach_splclock)())
#define	splstatclock()	((*Mach_splstatclock)())

extern void wbflush __P ((void));		/* XXX */
extern void delay __P((int n));

#endif	/* _LOCORE */
#endif	/* _KERNEL */

#endif /* __MIPS_PARAM_H__ */
