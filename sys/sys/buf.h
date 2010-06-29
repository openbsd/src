/*	$OpenBSD: buf.h,v 1.69 2010/06/29 18:52:20 kettenis Exp $	*/
/*	$NetBSD: buf.h,v 1.25 1997/04/09 21:12:17 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)buf.h	8.7 (Berkeley) 1/21/94
 */

#ifndef _SYS_BUF_H_
#define	_SYS_BUF_H_
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/mutex.h>

#define NOLIST ((struct buf *)0x87654321)

struct buf;
struct vnode;

struct buf_rb_bufs;
RB_PROTOTYPE(buf_rb_bufs, buf, b_rbbufs, rb_buf_compare);

LIST_HEAD(bufhead, buf);

/*
 * To avoid including <ufs/ffs/softdep.h>
 */

LIST_HEAD(workhead, worklist);

/*
 * Buffer queues
 */
#define BUFQ_DISKSORT	0
#define	BUFQ_FIFO	1
#define BUFQ_DEFAULT	BUFQ_DISKSORT
#define BUFQ_HOWMANY	2

struct bufq {
	SLIST_ENTRY(bufq)	 bufq_entries;
	struct mutex	 	 bufq_mtx;
	void			*bufq_data;
	u_int			 bufq_outstanding;
	int			 bufq_stop;
	int			 bufq_type;
};

struct buf	*bufq_disksort_dequeue(struct bufq *, int);
void		 bufq_disksort_queue(struct bufq *, struct buf *);
void		 bufq_disksort_requeue(struct bufq *, struct buf *);
int		 bufq_disksort_init(struct bufq *);
struct bufq_disksort {
	struct buf	 *bqd_actf;
	struct buf	**bqd_actb;
};

struct buf	*bufq_fifo_dequeue(struct bufq *, int);
void		 bufq_fifo_queue(struct bufq *, struct buf *);
void		 bufq_fifo_requeue(struct bufq *, struct buf *);
int		 bufq_fifo_init(struct bufq *);
TAILQ_HEAD(bufq_fifo_head, buf);
struct bufq_fifo {
	TAILQ_ENTRY(buf)	bqf_entries;
};

union bufq_data {
	struct bufq_disksort	bufq_data_disksort;
	struct bufq_fifo	bufq_data_fifo;
};

extern struct buf *(*bufq_dequeuev[BUFQ_HOWMANY])(struct bufq *, int);
extern void (*bufq_queuev[BUFQ_HOWMANY])(struct bufq *, struct buf *);
extern void (*bufq_requeuev[BUFQ_HOWMANY])(struct bufq *, struct buf *);

#define	BUFQ_QUEUE(_bufq, _bp)	 bufq_queue(_bufq, _bp)
#define BUFQ_REQUEUE(_bufq, _bp) bufq_requeue(_bufq, _bp)
#define	BUFQ_DEQUEUE(_bufq)		\
	    bufq_dequeuev[(_bufq)->bufq_type](_bufq, 0)
#define	BUFQ_PEEK(_bufq)		\
	bufq_dequeuev[(_bufq)->bufq_type](_bufq, 1)

struct bufq	*bufq_init(int);
void		 bufq_queue(struct bufq *, struct buf *);
void		 bufq_requeue(struct bufq *, struct buf *);
void		 bufq_destroy(struct bufq *);
void		 bufq_drain(struct bufq *);
void		 bufq_done(struct bufq *, struct buf *);
void		 bufq_quiesce(void);
void		 bufq_restart(void);


/*
 * These are currently used only by the soft dependency code, hence
 * are stored once in a global variable. If other subsystems wanted
 * to use these hooks, a pointer to a set of bio_ops could be added
 * to each buffer.
 */
extern struct bio_ops {
	void	(*io_start)(struct buf *);
	void	(*io_complete)(struct buf *);
	void	(*io_deallocate)(struct buf *);
	void	(*io_movedeps)(struct buf *, struct buf *);
	int	(*io_countdeps)(struct buf *, int, int);
} bioops;

/* XXX: disksort(); */
#define b_actf	b_bufq.bufq_data_disksort.bqd_actf
#define b_actb	b_bufq.bufq_data_disksort.bqd_actb

/* The buffer header describes an I/O operation in the kernel. */
struct buf {
	RB_ENTRY(buf) b_rbbufs;		/* vnode "hash" tree */
	LIST_ENTRY(buf) b_list;		/* All allocated buffers. */
	LIST_ENTRY(buf) b_vnbufs;	/* Buffer's associated vnode. */
	TAILQ_ENTRY(buf) b_freelist;	/* Free list position if not active. */
	time_t	b_synctime;		/* Time this buffer should be flushed */
	struct  proc *b_proc;		/* Associated proc; NULL if kernel. */
	volatile long	b_flags;	/* B_* flags. */
	int	b_error;		/* Errno value. */
	long	b_bufsize;		/* Allocated buffer size. */
	long	b_bcount;		/* Valid bytes in buffer. */
	size_t	b_resid;		/* Remaining I/O. */
	dev_t	b_dev;			/* Device associated with buffer. */
	caddr_t	b_data;			/* associated data */
	void	*b_saveaddr;		/* Original b_data for physio. */

	TAILQ_ENTRY(buf) b_valist;	/* LRU of va to reuse. */

	union	bufq_data b_bufq;
	struct	bufq	  *b_bq;	/* What bufq this buf is on */

	struct uvm_object *b_pobj;	/* Object containing the pages */
	off_t	b_poffs;		/* Offset within object */

	daddr64_t	b_lblkno;	/* Logical block number. */
	daddr64_t	b_blkno;	/* Underlying physical block number. */
					/* Function to call upon completion.
					 * Will be called at splbio(). */
	void	(*b_iodone)(struct buf *);
	struct	vnode *b_vp;		/* Device vnode. */
	int	b_dirtyoff;		/* Offset in buffer of dirty region. */
	int	b_dirtyend;		/* Offset of end of dirty region. */
	int	b_validoff;		/* Offset in buffer of valid region. */
	int	b_validend;		/* Offset of end of valid region. */
 	struct	workhead b_dep;		/* List of filesystem dependencies. */
};

/*
 * For portability with historic industry practice, the cylinder number has
 * to be maintained in the `b_resid' field.
 */
#define	b_cylinder b_resid		/* Cylinder number for disksort(). */

/* Device driver compatibility definitions. */
#define	b_active b_bcount		/* Driver queue head: drive active. */
#define	b_errcnt b_resid		/* Retry count while I/O in progress. */

/*
 * These flags are kept in b_flags.
 */
#define	B_AGE		0x00000001	/* Move to age queue when I/O done. */
#define	B_NEEDCOMMIT	0x00000002	/* Needs committing to stable storage */
#define	B_ASYNC		0x00000004	/* Start I/O, do not wait. */
#define	B_BAD		0x00000008	/* Bad block revectoring in progress. */
#define	B_BUSY		0x00000010	/* I/O in progress. */
#define	B_CACHE		0x00000020	/* Bread found us in the cache. */
#define	B_CALL		0x00000040	/* Call b_iodone from biodone. */
#define	B_DELWRI	0x00000080	/* Delay I/O until buffer reused. */
#define	B_DONE		0x00000200	/* I/O completed. */
#define	B_EINTR		0x00000400	/* I/O was interrupted */
#define	B_ERROR		0x00000800	/* I/O error occurred. */
#define	B_INVAL		0x00002000	/* Does not contain valid info. */
#define	B_NOCACHE	0x00008000	/* Do not cache block after use. */
#define	B_PHYS		0x00040000	/* I/O to user memory. */
#define	B_RAW		0x00080000	/* Set by physio for raw transfers. */
#define	B_READ		0x00100000	/* Read buffer. */
#define	B_WANTED	0x00800000	/* Process wants this buffer. */
#define	B_WRITE		0x00000000	/* Write buffer (pseudo flag). */
#define	B_WRITEINPROG	0x01000000	/* Write in progress. */
#define	B_XXX		0x02000000	/* Debugging flag. */
#define	B_DEFERRED	0x04000000	/* Skipped over for cleaning */
#define	B_SCANNED	0x08000000	/* Block already pushed during sync */
#define	B_PDAEMON	0x10000000	/* I/O started by pagedaemon */
#define B_RELEASED	0x20000000	/* free this buffer after its kvm */
#define B_NOTMAPPED	0x40000000	/* BUSY, but not necessarily mapped */

#define	B_BITS	"\010\001AGE\002NEEDCOMMIT\003ASYNC\004BAD\005BUSY\006CACHE" \
    "\007CALL\010DELWRI\012DONE\013EINTR\014ERROR" \
    "\016INVAL\020NOCACHE\023PHYS\024RAW\025READ" \
    "\030WANTED\031WRITEINPROG\032XXX\033DEFERRED" \
    "\034SCANNED\035PDAEMON"

/*
 * This structure describes a clustered I/O.  It is stored in the b_saveaddr
 * field of the buffer on which I/O is done.  At I/O completion, cluster
 * callback uses the structure to parcel I/O's to individual buffers, and
 * then free's this structure.
 */
struct cluster_save {
	long	bs_bcount;		/* Saved b_bcount. */
	long	bs_bufsize;		/* Saved b_bufsize. */
	void	*bs_saveaddr;		/* Saved b_addr. */
	int	bs_nchildren;		/* Number of associated buffers. */
	struct buf **bs_children;	/* List of associated buffers. */
};

/*
 * Zero out the buffer's data area.
 */
#define	clrbuf(bp) {							\
	bzero((bp)->b_data, (u_int)(bp)->b_bcount);			\
	(bp)->b_resid = 0;						\
}


/* Flags to low-level allocation routines. */
#define B_CLRBUF	0x01	/* Request allocated buffer be cleared. */
#define B_SYNC		0x02	/* Do all allocations synchronously. */

struct cluster_info {
	daddr64_t	ci_lastr;	/* last read (read-ahead) */
	daddr64_t	ci_lastw;	/* last write (write cluster) */
	daddr64_t	ci_cstart;	/* start block of cluster */
	daddr64_t	ci_lasta;	/* last allocation */
	int		ci_clen; 	/* length of current cluster */
	int		ci_ralen;	/* Read-ahead length */
	daddr64_t	ci_maxra;	/* last readahead block */
};

#ifdef _KERNEL
__BEGIN_DECLS
extern int bufpages;		/* Max number of pages for buffers' data */
extern struct pool bufpool;
extern struct bufhead bufhead;

void	bawrite(struct buf *);
void	bdwrite(struct buf *);
void	biodone(struct buf *);
int	biowait(struct buf *);
int bread(struct vnode *, daddr64_t, int, struct ucred *, struct buf **);
int breadn(struct vnode *, daddr64_t, int, daddr64_t *, int *, int,
    struct ucred *, struct buf **);
void	brelse(struct buf *);
void	bremfree(struct buf *);
void	bufinit(void);
void	buf_dirty(struct buf *);
void    buf_undirty(struct buf *);
int	bwrite(struct buf *);
struct buf *getblk(struct vnode *, daddr64_t, int, int, int);
struct buf *geteblk(int);
struct buf *incore(struct vnode *, daddr64_t);

/*
 * buf_kvm_init initializes the kvm handling for buffers.
 * buf_acquire sets the B_BUSY flag and ensures that the buffer is
 * mapped in the kvm.
 * buf_release clears the B_BUSY flag and allows the buffer to become
 * unmapped.
 * buf_unmap is for internal use only. Unmaps the buffer from kvm.
 */
void	buf_mem_init(vsize_t);
void	buf_acquire(struct buf *);
void	buf_acquire_unmapped(struct buf *);
void	buf_map(struct buf *);
void	buf_release(struct buf *);
int	buf_dealloc_mem(struct buf *);
void	buf_shrink_mem(struct buf *, vsize_t);
void	buf_alloc_pages(struct buf *, vsize_t);
void	buf_free_pages(struct buf *);


void	minphys(struct buf *bp);
int	physio(void (*strategy)(struct buf *), struct buf *bp, dev_t dev,
	    int flags, void (*minphys)(struct buf *), struct uio *uio);
void  brelvp(struct buf *);
void  reassignbuf(struct buf *);
void  bgetvp(struct vnode *, struct buf *);

void  buf_replacevnode(struct buf *, struct vnode *);
void  buf_daemon(struct proc *);
void  buf_replacevnode(struct buf *, struct vnode *);
void  buf_daemon(struct proc *);
int bread_cluster(struct vnode *, daddr64_t, int, struct buf **);

#ifdef DEBUG
void buf_print(struct buf *);
#endif

static __inline void
buf_start(struct buf *bp)
{
	if (bioops.io_start)
		(*bioops.io_start)(bp);
}

static __inline void
buf_complete(struct buf *bp)
{
	if (bioops.io_complete)
		(*bioops.io_complete)(bp);
}

static __inline void
buf_deallocate(struct buf *bp)
{
	if (bioops.io_deallocate)
		(*bioops.io_deallocate)(bp);
}

static __inline void
buf_movedeps(struct buf *bp, struct buf *bp2)
{
	if (bioops.io_movedeps)
		(*bioops.io_movedeps)(bp, bp2);
}

static __inline int
buf_countdeps(struct buf *bp, int i, int islocked)
{
	if (bioops.io_countdeps)
		return ((*bioops.io_countdeps)(bp, i, islocked));
	else
		return (0);
}

void	cluster_write(struct buf *, struct cluster_info *, u_quad_t);

__END_DECLS
#endif
#endif /* !_SYS_BUF_H_ */
