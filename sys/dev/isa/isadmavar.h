/*	$OpenBSD: isadmavar.h,v 1.10 1997/12/25 12:06:48 downsj Exp $	*/
/*	$NetBSD: isadmavar.h,v 1.4 1996/03/01 04:08:46 mycroft Exp $	*/

#define	DMAMODE_WRITE	0
#define	DMAMODE_READ	1
#define	DMAMODE_LOOP	2

#define ISADMA_START_READ	DMAMODE_READ	/* read from device */
#define ISADMA_START_WRITE	DMAMODE_WRITE	/* write to device */

#define	ISADMA_MAP_WAITOK	0x0001	/* OK for isadma_map to sleep */
#define	ISADMA_MAP_BOUNCE	0x0002	/* use bounce buffer if necessary */
#define	ISADMA_MAP_CONTIG	0x0004	/* must be physically contiguous */
#define	ISADMA_MAP_8BIT		0x0008	/* must not cross 64k boundary */
#define	ISADMA_MAP_16BIT	0x0010	/* must not cross 128k boundary */

struct isadma_seg {		/* a physical contiguous segment */
	vm_offset_t addr;	/* address of this segment */
	vm_size_t length;	/* length of this segment (bytes) */
};

int isadma_map __P((caddr_t, vm_size_t, struct isadma_seg *, int));
void isadma_unmap __P((caddr_t, vm_size_t, int, struct isadma_seg *));
void isadma_copytobuf __P((caddr_t, vm_size_t, int, struct isadma_seg *));
void isadma_copyfrombuf __P((caddr_t, vm_size_t, int, struct isadma_seg *));

int isadma_acquire __P((int));
void isadma_release __P((int));
void isadma_cascade __P((int));
void isadma_start __P((caddr_t, vm_size_t, int, int));
void isadma_abort __P((int));
int isadma_finished __P((int));
void isadma_done __P((int));

/*
 * XXX these are needed until all drivers have been cleaned up
 */

#define isa_dma_acquire(c)       isadma_acquire(c)
#define isa_dma_release(c)       isadma_release(c)
#define isa_dmacascade(c)	isadma_cascade((c))
#define isa_dmastart(f, a, s, c)	isadma_start((a), (s), (c), (f))
#define isa_dmaabort(c)		isadma_abort((c))
#define isa_dmafinished(c)	isadma_finished((c))
#define isa_dmadone(f, a, s, c)	isadma_done((c))
