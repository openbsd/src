/*	$OpenBSD: ffs_softdep.c,v 1.138 2018/02/10 05:24:23 deraadt Exp $	*/

/*
 * Copyright 1998, 2000 Marshall Kirk McKusick. All Rights Reserved.
 *
 * The soft updates code is derived from the appendix of a University
 * of Michigan technical report (Gregory R. Ganger and Yale N. Patt,
 * "Soft Updates: A Solution to the Metadata Update Problem in File
 * Systems", CSE-TR-254-95, August 1995).
 *
 * Further information about soft updates can be obtained from:
 *
 *	Marshall Kirk McKusick		http://www.mckusick.com/softdep/
 *	1614 Oxford Street		mckusick@mckusick.com
 *	Berkeley, CA 94709-1608		+1-510-843-9542
 *	USA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ffs_softdep.c	9.59 (McKusick) 6/21/00
 * $FreeBSD: src/sys/ufs/ffs/ffs_softdep.c,v 1.86 2001/02/04 16:08:18 phk Exp $
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/specdev.h>
#include <crypto/siphash.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/softdep.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_extern.h>

#define STATIC

/*
 * Mapping of dependency structure types to malloc types.
 */
#define	D_PAGEDEP	0
#define	D_INODEDEP	1
#define	D_NEWBLK	2
#define	D_BMSAFEMAP	3
#define	D_ALLOCDIRECT	4
#define	D_INDIRDEP	5
#define	D_ALLOCINDIR	6
#define	D_FREEFRAG	7
#define	D_FREEBLKS	8
#define	D_FREEFILE	9
#define	D_DIRADD	10
#define	D_MKDIR		11
#define	D_DIRREM	12
#define	D_NEWDIRBLK	13
#define	D_LAST		13
/*
 * Names of softdep types.
 */
const char *softdep_typenames[] = {
	"pagedep",
	"inodedep",
	"newblk",
	"bmsafemap",
	"allocdirect",
	"indirdep",
	"allocindir",
	"freefrag",
	"freeblks",
	"freefile",
	"diradd",
	"mkdir",
	"dirrem",
	"newdirblk",
};
#define	TYPENAME(type) \
	((unsigned)(type) <= D_LAST ? softdep_typenames[type] : "???")
/*
 * Finding the current process.
 */
#define CURPROC curproc
/*
 * End system adaptation definitions.
 */

/*
 * Internal function prototypes.
 */
STATIC	void softdep_error(char *, int);
STATIC	void drain_output(struct vnode *, int);
STATIC	int getdirtybuf(struct buf *, int);
STATIC	void clear_remove(struct proc *);
STATIC	void clear_inodedeps(struct proc *);
STATIC	int flush_pagedep_deps(struct vnode *, struct mount *,
	    struct diraddhd *);
STATIC	int flush_inodedep_deps(struct fs *, ufsino_t);
STATIC	int handle_written_filepage(struct pagedep *, struct buf *);
STATIC  void diradd_inode_written(struct diradd *, struct inodedep *);
STATIC	int handle_written_inodeblock(struct inodedep *, struct buf *);
STATIC	void handle_allocdirect_partdone(struct allocdirect *);
STATIC	void handle_allocindir_partdone(struct allocindir *);
STATIC	void initiate_write_filepage(struct pagedep *, struct buf *);
STATIC	void handle_written_mkdir(struct mkdir *, int);
STATIC	void initiate_write_inodeblock_ufs1(struct inodedep *, struct buf *);
#ifdef FFS2
STATIC	void initiate_write_inodeblock_ufs2(struct inodedep *, struct buf *);
#endif
STATIC	void handle_workitem_freefile(struct freefile *);
STATIC	void handle_workitem_remove(struct dirrem *);
STATIC	struct dirrem *newdirrem(struct buf *, struct inode *,
	    struct inode *, int, struct dirrem **);
STATIC	void free_diradd(struct diradd *);
STATIC	void free_allocindir(struct allocindir *, struct inodedep *);
STATIC	void free_newdirblk(struct newdirblk *);
STATIC	int indir_trunc(struct inode *, daddr_t, int, daddr_t, long *);
STATIC	void deallocate_dependencies(struct buf *, struct inodedep *);
STATIC	void free_allocdirect(struct allocdirectlst *,
	    struct allocdirect *, int);
STATIC	int check_inode_unwritten(struct inodedep *);
STATIC	int free_inodedep(struct inodedep *);
STATIC	void handle_workitem_freeblocks(struct freeblks *);
STATIC	void merge_inode_lists(struct inodedep *);
STATIC	void setup_allocindir_phase2(struct buf *, struct inode *,
	    struct allocindir *);
STATIC	struct allocindir *newallocindir(struct inode *, int, daddr_t,
	    daddr_t);
STATIC	void handle_workitem_freefrag(struct freefrag *);
STATIC	struct freefrag *newfreefrag(struct inode *, daddr_t, long);
STATIC	void allocdirect_merge(struct allocdirectlst *,
	    struct allocdirect *, struct allocdirect *);
STATIC	struct bmsafemap *bmsafemap_lookup(struct buf *);
STATIC	int newblk_lookup(struct fs *, daddr_t, int,
	    struct newblk **);
STATIC	int inodedep_lookup(struct fs *, ufsino_t, int, struct inodedep **);
STATIC	int pagedep_lookup(struct inode *, daddr_t, int, struct pagedep **);
STATIC	void pause_timer(void *);
STATIC	int request_cleanup(int, int);
STATIC	int process_worklist_item(struct mount *, int);
STATIC	void add_to_worklist(struct worklist *);

/*
 * Exported softdep operations.
 */
void softdep_disk_io_initiation(struct buf *);
void softdep_disk_write_complete(struct buf *);
void softdep_deallocate_dependencies(struct buf *);
void softdep_move_dependencies(struct buf *, struct buf *);
int softdep_count_dependencies(struct buf *bp, int, int);

/*
 * Locking primitives.
 *
 * For a uniprocessor, all we need to do is protect against disk
 * interrupts. For a multiprocessor, this lock would have to be
 * a mutex. A single mutex is used throughout this file, though
 * finer grain locking could be used if contention warranted it.
 *
 * For a multiprocessor, the sleep call would accept a lock and
 * release it after the sleep processing was complete. In a uniprocessor
 * implementation there is no such interlock, so we simple mark
 * the places where it needs to be done with the `interlocked' form
 * of the lock calls. Since the uniprocessor sleep already interlocks
 * the spl, there is nothing that really needs to be done.
 */
#ifndef /* NOT */ DEBUG
STATIC struct lockit {
	int	lkt_spl;
} lk = { 0 };
#define ACQUIRE_LOCK(lk)		(lk)->lkt_spl = splbio()
#define FREE_LOCK(lk)			splx((lk)->lkt_spl)
#define ACQUIRE_LOCK_INTERLOCKED(lk,s)	(lk)->lkt_spl = (s)
#define FREE_LOCK_INTERLOCKED(lk)	((lk)->lkt_spl)

#else /* DEBUG */
STATIC struct lockit {
	int	lkt_spl;
	pid_t	lkt_held;
	int     lkt_line;
} lk = { 0, -1 };
STATIC int lockcnt;

STATIC	void acquire_lock(struct lockit *, int);
STATIC	void free_lock(struct lockit *, int);
STATIC	void acquire_lock_interlocked(struct lockit *, int, int);
STATIC	int free_lock_interlocked(struct lockit *, int);

#define ACQUIRE_LOCK(lk)		acquire_lock(lk, __LINE__)
#define FREE_LOCK(lk)			free_lock(lk, __LINE__)
#define ACQUIRE_LOCK_INTERLOCKED(lk,s)	acquire_lock_interlocked(lk, (s), __LINE__)
#define FREE_LOCK_INTERLOCKED(lk)	free_lock_interlocked(lk, __LINE__)

STATIC void
acquire_lock(struct lockit *lk, int line)
{
	pid_t holder;
	int original_line;

	if (lk->lkt_held != -1) {
		holder = lk->lkt_held;
		original_line = lk->lkt_line;
		FREE_LOCK(lk);
		if (holder == CURPROC->p_tid)
			panic("softdep_lock: locking against myself, acquired at line %d, relocked at line %d", original_line, line);
		else
			panic("softdep_lock: lock held by %d, acquired at line %d, relocked at line %d", holder, original_line, line);
	}
	lk->lkt_spl = splbio();
	lk->lkt_held = CURPROC->p_tid;
	lk->lkt_line = line;
	lockcnt++;
}

STATIC void
free_lock(struct lockit *lk, int line)
{

	if (lk->lkt_held == -1)
		panic("softdep_unlock: lock not held at line %d", line);
	lk->lkt_held = -1;
	splx(lk->lkt_spl);
}

STATIC void
acquire_lock_interlocked(struct lockit *lk, int s, int line)
{
	pid_t holder;
	int original_line;

	if (lk->lkt_held != -1) {
		holder = lk->lkt_held;
		original_line = lk->lkt_line;
		FREE_LOCK_INTERLOCKED(lk);
		if (holder == CURPROC->p_tid)
			panic("softdep_lock: locking against myself, acquired at line %d, relocked at line %d", original_line, line);
		else
			panic("softdep_lock: lock held by %d, acquired at line %d, relocked at line %d", holder, original_line, line);
	}
	lk->lkt_held = CURPROC->p_tid;
	lk->lkt_line = line;
	lk->lkt_spl = s;
	lockcnt++;
}

STATIC int
free_lock_interlocked(struct lockit *lk, int line)
{

	if (lk->lkt_held == -1)
		panic("softdep_unlock_interlocked: lock not held at line %d", line);
	lk->lkt_held = -1;

	return (lk->lkt_spl);
}
#endif /* DEBUG */

/*
 * Place holder for real semaphores.
 */
struct sema {
	int	value;
	pid_t	holder;
	char	*name;
	int	prio;
	int	timo;
};
STATIC	void sema_init(struct sema *, char *, int, int);
STATIC	int sema_get(struct sema *, struct lockit *);
STATIC	void sema_release(struct sema *);

STATIC void
sema_init(struct sema *semap, char *name, int prio, int timo)
{

	semap->holder = -1;
	semap->value = 0;
	semap->name = name;
	semap->prio = prio;
	semap->timo = timo;
}

STATIC int
sema_get(struct sema *semap, struct lockit *interlock)
{
	int s;

	if (semap->value++ > 0) {
		if (interlock != NULL)
			s = FREE_LOCK_INTERLOCKED(interlock);
		tsleep((caddr_t)semap, semap->prio, semap->name, semap->timo);
		if (interlock != NULL) {
			ACQUIRE_LOCK_INTERLOCKED(interlock, s);
			FREE_LOCK(interlock);
		}
		return (0);
	}
	semap->holder = CURPROC->p_tid;
	if (interlock != NULL)
		FREE_LOCK(interlock);
	return (1);
}

STATIC void
sema_release(struct sema *semap)
{

	if (semap->value <= 0 || semap->holder != CURPROC->p_tid) {
#ifdef DEBUG
		if (lk.lkt_held != -1)
			FREE_LOCK(&lk);
#endif
		panic("sema_release: not held");
	}
	if (--semap->value > 0) {
		semap->value = 0;
		wakeup(semap);
	}
	semap->holder = -1;
}

/*
 * Memory management.
 */
STATIC struct pool pagedep_pool;
STATIC struct pool inodedep_pool;
STATIC struct pool newblk_pool;
STATIC struct pool bmsafemap_pool;
STATIC struct pool allocdirect_pool;
STATIC struct pool indirdep_pool;
STATIC struct pool allocindir_pool;
STATIC struct pool freefrag_pool;
STATIC struct pool freeblks_pool;
STATIC struct pool freefile_pool;
STATIC struct pool diradd_pool;
STATIC struct pool mkdir_pool;
STATIC struct pool dirrem_pool;
STATIC struct pool newdirblk_pool;

static __inline void
softdep_free(struct worklist *item, int type)
{

	switch (type) {
	case D_PAGEDEP:
		pool_put(&pagedep_pool, item);
		break;

	case D_INODEDEP:
		pool_put(&inodedep_pool, item);
		break;

	case D_BMSAFEMAP:
		pool_put(&bmsafemap_pool, item);
		break;

	case D_ALLOCDIRECT:
		pool_put(&allocdirect_pool, item);
		break;

	case D_INDIRDEP:
		pool_put(&indirdep_pool, item);
		break;

	case D_ALLOCINDIR:
		pool_put(&allocindir_pool, item);
		break;

	case D_FREEFRAG:
		pool_put(&freefrag_pool, item);
		break;

	case D_FREEBLKS:
		pool_put(&freeblks_pool, item);
		break;

	case D_FREEFILE:
		pool_put(&freefile_pool, item);
		break;

	case D_DIRADD:
		pool_put(&diradd_pool, item);
		break;

	case D_MKDIR:
		pool_put(&mkdir_pool, item);
		break;

	case D_DIRREM:
		pool_put(&dirrem_pool, item);
		break;

	case D_NEWDIRBLK:
		pool_put(&newdirblk_pool, item);
		break;

	default:
#ifdef DEBUG
		if (lk.lkt_held != -1)
			FREE_LOCK(&lk);
#endif
		panic("softdep_free: unknown type %d", type);
	}
}

struct workhead softdep_freequeue;

static __inline void
softdep_freequeue_add(struct worklist *item)
{
	int s;

	s = splbio();
	LIST_INSERT_HEAD(&softdep_freequeue, item, wk_list);
	splx(s);
}

static __inline void
softdep_freequeue_process(void)
{
	struct worklist *wk;

	splassert(IPL_BIO);

	while ((wk = LIST_FIRST(&softdep_freequeue)) != NULL) {
		LIST_REMOVE(wk, wk_list);
		FREE_LOCK(&lk);
		softdep_free(wk, wk->wk_type);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Worklist queue management.
 * These routines require that the lock be held.
 */
#ifndef /* NOT */ DEBUG
#define WORKLIST_INSERT(head, item) do {	\
	(item)->wk_state |= ONWORKLIST;		\
	LIST_INSERT_HEAD(head, item, wk_list);	\
} while (0)
#define WORKLIST_REMOVE(item) do {		\
	(item)->wk_state &= ~ONWORKLIST;	\
	LIST_REMOVE(item, wk_list);		\
} while (0)
#define WORKITEM_FREE(item, type) softdep_freequeue_add((struct worklist *)item)

#else /* DEBUG */
STATIC	void worklist_insert(struct workhead *, struct worklist *);
STATIC	void worklist_remove(struct worklist *);
STATIC	void workitem_free(struct worklist *);

#define WORKLIST_INSERT(head, item) worklist_insert(head, item)
#define WORKLIST_REMOVE(item) worklist_remove(item)
#define WORKITEM_FREE(item, type) workitem_free((struct worklist *)item)

STATIC void
worklist_insert(struct workhead *head, struct worklist *item)
{

	if (lk.lkt_held == -1)
		panic("worklist_insert: lock not held");
	if (item->wk_state & ONWORKLIST) {
		FREE_LOCK(&lk);
		panic("worklist_insert: already on list");
	}
	item->wk_state |= ONWORKLIST;
	LIST_INSERT_HEAD(head, item, wk_list);
}

STATIC void
worklist_remove(struct worklist *item)
{

	if (lk.lkt_held == -1)
		panic("worklist_remove: lock not held");
	if ((item->wk_state & ONWORKLIST) == 0) {
		FREE_LOCK(&lk);
		panic("worklist_remove: not on list");
	}
	item->wk_state &= ~ONWORKLIST;
	LIST_REMOVE(item, wk_list);
}

STATIC void
workitem_free(struct worklist *item)
{

	if (item->wk_state & ONWORKLIST) {
		if (lk.lkt_held != -1)
			FREE_LOCK(&lk);
		panic("workitem_free: still on list");
	}
	softdep_freequeue_add(item);
}
#endif /* DEBUG */

/*
 * Workitem queue management
 */
STATIC struct workhead softdep_workitem_pending;
STATIC struct worklist *worklist_tail;
STATIC int num_on_worklist;	/* number of worklist items to be processed */
STATIC int softdep_worklist_busy; /* 1 => trying to do unmount */
STATIC int softdep_worklist_req; /* serialized waiters */
STATIC int max_softdeps;	/* maximum number of structs before slowdown */
STATIC int tickdelay = 2;	/* number of ticks to pause during slowdown */
STATIC int proc_waiting;	/* tracks whether we have a timeout posted */
STATIC int *stat_countp;	/* statistic to count in proc_waiting timeout */
STATIC struct timeout proc_waiting_timeout; 
STATIC struct proc *filesys_syncer; /* proc of filesystem syncer process */
STATIC int req_clear_inodedeps;	/* syncer process flush some inodedeps */
#define FLUSH_INODES	1
STATIC int req_clear_remove;	/* syncer process flush some freeblks */
#define FLUSH_REMOVE	2
/*
 * runtime statistics
 */
STATIC int stat_worklist_push;	/* number of worklist cleanups */
STATIC int stat_blk_limit_push;	/* number of times block limit neared */
STATIC int stat_ino_limit_push;	/* number of times inode limit neared */
STATIC int stat_blk_limit_hit;	/* number of times block slowdown imposed */
STATIC int stat_ino_limit_hit;	/* number of times inode slowdown imposed */
STATIC int stat_sync_limit_hit;	/* number of synchronous slowdowns imposed */
STATIC int stat_indir_blk_ptrs;	/* bufs redirtied as indir ptrs not written */
STATIC int stat_inode_bitmap;	/* bufs redirtied as inode bitmap not written */
STATIC int stat_direct_blk_ptrs;/* bufs redirtied as direct ptrs not written */
STATIC int stat_dir_entry;	/* bufs redirtied as dir entry cannot write */

/*
 * Add an item to the end of the work queue.
 * This routine requires that the lock be held.
 * This is the only routine that adds items to the list.
 * The following routine is the only one that removes items
 * and does so in order from first to last.
 */
STATIC void
add_to_worklist(struct worklist *wk)
{

	if (wk->wk_state & ONWORKLIST) {
#ifdef DEBUG
		if (lk.lkt_held != -1)
			FREE_LOCK(&lk);
#endif
		panic("add_to_worklist: already on list");
	}
	wk->wk_state |= ONWORKLIST;
	if (LIST_FIRST(&softdep_workitem_pending) == NULL)
		LIST_INSERT_HEAD(&softdep_workitem_pending, wk, wk_list);
	else
		LIST_INSERT_AFTER(worklist_tail, wk, wk_list);
	worklist_tail = wk;
	num_on_worklist += 1;
}

/*
 * Process that runs once per second to handle items in the background queue.
 *
 * Note that we ensure that everything is done in the order in which they
 * appear in the queue. The code below depends on this property to ensure
 * that blocks of a file are freed before the inode itself is freed. This
 * ordering ensures that no new <vfsid, inum, lbn> triples will be generated
 * until all the old ones have been purged from the dependency lists.
 */
int 
softdep_process_worklist(struct mount *matchmnt)
{
	struct proc *p = CURPROC;
	int matchcnt, loopcount;
	struct timeval starttime;

	/*
	 * First process any items on the delayed-free queue.
	 */
	ACQUIRE_LOCK(&lk);
	softdep_freequeue_process();
	FREE_LOCK(&lk);

	/*
	 * Record the process identifier of our caller so that we can give
	 * this process preferential treatment in request_cleanup below.
	 * We can't do this in softdep_initialize, because the syncer doesn't
	 * have to run then.
	 * NOTE! This function _could_ be called with a curproc != syncerproc.
	 */
	filesys_syncer = syncerproc;
	matchcnt = 0;

	/*
	 * There is no danger of having multiple processes run this
	 * code, but we have to single-thread it when softdep_flushfiles()
	 * is in operation to get an accurate count of the number of items
	 * related to its mount point that are in the list.
	 */
	if (matchmnt == NULL) {
		if (softdep_worklist_busy < 0)
			return(-1);
		softdep_worklist_busy += 1;
	}

	/*
	 * If requested, try removing inode or removal dependencies.
	 */
	if (req_clear_inodedeps) {
		clear_inodedeps(p);
		req_clear_inodedeps -= 1;
		wakeup_one(&proc_waiting);
	}
	if (req_clear_remove) {
		clear_remove(p);
		req_clear_remove -= 1;
		wakeup_one(&proc_waiting);
	}
	loopcount = 1;
	getmicrouptime(&starttime);
	while (num_on_worklist > 0) {
		matchcnt += process_worklist_item(matchmnt, 0);

		/*
		 * If a umount operation wants to run the worklist
		 * accurately, abort.
		 */
		if (softdep_worklist_req && matchmnt == NULL) {
			matchcnt = -1;
			break;
		}

		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(p);
			req_clear_inodedeps -= 1;
			wakeup_one(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(p);
			req_clear_remove -= 1;
			wakeup_one(&proc_waiting);
		}
		/*
		 * We do not generally want to stop for buffer space, but if
		 * we are really being a buffer hog, we will stop and wait.
		 */
#if 0
		if (loopcount++ % 128 == 0)
			bwillwrite();
#endif
		/*
		 * Never allow processing to run for more than one
		 * second. Otherwise the other syncer tasks may get
		 * excessively backlogged.
		 */
		{
			struct timeval diff;
			struct timeval tv;

			getmicrouptime(&tv);
			timersub(&tv, &starttime, &diff);
			if (diff.tv_sec != 0 && matchmnt == NULL) {
				matchcnt = -1;
				break;
			}
		}

		/*
		 * Process any new items on the delayed-free queue.
		 */
		ACQUIRE_LOCK(&lk);
		softdep_freequeue_process();
		FREE_LOCK(&lk);
	}
	if (matchmnt == NULL) {
		softdep_worklist_busy -= 1;
		if (softdep_worklist_req && softdep_worklist_busy == 0)
			wakeup(&softdep_worklist_req);
	}
	return (matchcnt);
}

/*
 * Process one item on the worklist.
 */
STATIC int
process_worklist_item(struct mount *matchmnt, int flags)
{
	struct worklist *wk, *wkend;
	struct dirrem *dirrem;
	struct mount *mp;
	struct vnode *vp;
	int matchcnt = 0;

	ACQUIRE_LOCK(&lk);
	/*
	 * Normally we just process each item on the worklist in order.
	 * However, if we are in a situation where we cannot lock any
	 * inodes, we have to skip over any dirrem requests whose
	 * vnodes are resident and locked.
	 */
	LIST_FOREACH(wk, &softdep_workitem_pending, wk_list) {
		if ((flags & LK_NOWAIT) == 0 || wk->wk_type != D_DIRREM)
			break;
		dirrem = WK_DIRREM(wk);
		vp = ufs_ihashlookup(VFSTOUFS(dirrem->dm_mnt)->um_dev,
		    dirrem->dm_oldinum);
		if (vp == NULL || !VOP_ISLOCKED(vp))
			break;
	}
	if (wk == NULL) {
		FREE_LOCK(&lk);
		return (0);
	}
	/*
	 * Remove the item to be processed. If we are removing the last
	 * item on the list, we need to recalculate the tail pointer.
	 * As this happens rarely and usually when the list is short,
	 * we just run down the list to find it rather than tracking it
	 * in the above loop.
	 */
	WORKLIST_REMOVE(wk);
	if (wk == worklist_tail) {
		LIST_FOREACH(wkend, &softdep_workitem_pending, wk_list)
			if (LIST_NEXT(wkend, wk_list) == NULL)
				break;
		worklist_tail = wkend;
	}
	num_on_worklist -= 1;
	FREE_LOCK(&lk);
	switch (wk->wk_type) {

	case D_DIRREM:
		/* removal of a directory entry */
		mp = WK_DIRREM(wk)->dm_mnt;
#if 0
		if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
			panic("%s: dirrem on suspended filesystem",
				"process_worklist_item");
#endif
		if (mp == matchmnt)
			matchcnt += 1;
		handle_workitem_remove(WK_DIRREM(wk));
		break;

	case D_FREEBLKS:
		/* releasing blocks and/or fragments from a file */
		mp = WK_FREEBLKS(wk)->fb_mnt;
#if 0
		if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
			panic("%s: freeblks on suspended filesystem",
				"process_worklist_item");
#endif
		if (mp == matchmnt)
			matchcnt += 1;
		handle_workitem_freeblocks(WK_FREEBLKS(wk));
		break;

	case D_FREEFRAG:
		/* releasing a fragment when replaced as a file grows */
		mp = WK_FREEFRAG(wk)->ff_mnt;
#if 0
		if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
			panic("%s: freefrag on suspended filesystem",
				"process_worklist_item");
#endif
		if (mp == matchmnt)
			matchcnt += 1;
		handle_workitem_freefrag(WK_FREEFRAG(wk));
		break;

	case D_FREEFILE:
		/* releasing an inode when its link count drops to 0 */
		mp = WK_FREEFILE(wk)->fx_mnt;
#if 0
		if (vn_write_suspend_wait(NULL, mp, V_NOWAIT))
			panic("%s: freefile on suspended filesystem",
				"process_worklist_item");
#endif
		if (mp == matchmnt)
			matchcnt += 1;
		handle_workitem_freefile(WK_FREEFILE(wk));
		break;

	default:
		panic("%s_process_worklist: Unknown type %s",
		    "softdep", TYPENAME(wk->wk_type));
		/* NOTREACHED */
	}
	return (matchcnt);
}

/*
 * Move dependencies from one buffer to another.
 */
void
softdep_move_dependencies(struct buf *oldbp, struct buf *newbp)
{
	struct worklist *wk, *wktail;

	if (LIST_FIRST(&newbp->b_dep) != NULL)
		panic("softdep_move_dependencies: need merge code");
	wktail = NULL;
	ACQUIRE_LOCK(&lk);
	while ((wk = LIST_FIRST(&oldbp->b_dep)) != NULL) {
		LIST_REMOVE(wk, wk_list);
		if (wktail == NULL)
			LIST_INSERT_HEAD(&newbp->b_dep, wk, wk_list);
		else
			LIST_INSERT_AFTER(wktail, wk, wk_list);
		wktail = wk;
	}
	FREE_LOCK(&lk);
}

/*
 * Purge the work list of all items associated with a particular mount point.
 */
int
softdep_flushworklist(struct mount *oldmnt, int *countp, struct proc *p)
{
	struct vnode *devvp;
	int count, error = 0;

	/*
	 * Await our turn to clear out the queue, then serialize access.
	 */
	while (softdep_worklist_busy) {
		softdep_worklist_req += 1;
		tsleep(&softdep_worklist_req, PRIBIO, "softflush", 0);
		softdep_worklist_req -= 1;
	}
	softdep_worklist_busy = -1;
	/*
	 * Alternately flush the block device associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. We continue until no more worklist dependencies
	 * are found.
	 */
	*countp = 0;
	devvp = VFSTOUFS(oldmnt)->um_devvp;
	while ((count = softdep_process_worklist(oldmnt)) > 0) {
		*countp += count;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_FSYNC(devvp, p->p_ucred, MNT_WAIT, p);
		VOP_UNLOCK(devvp, p);
		if (error)
			break;
	}
	softdep_worklist_busy = 0;
	if (softdep_worklist_req)
		wakeup(&softdep_worklist_req);
	return (error);
}

/*
 * Flush all vnodes and worklist items associated with a specified mount point.
 */
int
softdep_flushfiles(struct mount *oldmnt, int flags, struct proc *p)
{
	int error, count, loopcnt;

	/*
	 * Alternately flush the vnodes associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. In theory, this loop can happen at most twice,
	 * but we give it a few extra just to be sure.
	 */
	for (loopcnt = 10; loopcnt > 0; loopcnt--) {
		/*
		 * Do another flush in case any vnodes were brought in
		 * as part of the cleanup operations.
		 */
		if ((error = ffs_flushfiles(oldmnt, flags, p)) != 0)
			break;
		if ((error = softdep_flushworklist(oldmnt, &count, p)) != 0 ||
		    count == 0)
			break;
	}
	/*
	 * If the reboot process sleeps during the loop, the update
	 * process may call softdep_process_worklist() and create
	 * new dirty vnodes at the mount point.  Call ffs_flushfiles()
	 * again after the loop has flushed all soft dependencies.
	 */
	if (error == 0)
		error = ffs_flushfiles(oldmnt, flags, p);
	/*
	 * If we are unmounting then it is an error to fail. If we
	 * are simply trying to downgrade to read-only, then filesystem
	 * activity can keep us busy forever, so we just fail with EBUSY.
	 */
	if (loopcnt == 0) {
		error = EBUSY;
	}
	return (error);
}

/*
 * Structure hashing.
 * 
 * There are three types of structures that can be looked up:
 *	1) pagedep structures identified by mount point, inode number,
 *	   and logical block.
 *	2) inodedep structures identified by mount point and inode number.
 *	3) newblk structures identified by mount point and
 *	   physical block number.
 *
 * The "pagedep" and "inodedep" dependency structures are hashed
 * separately from the file blocks and inodes to which they correspond.
 * This separation helps when the in-memory copy of an inode or
 * file block must be replaced. It also obviates the need to access
 * an inode or file page when simply updating (or de-allocating)
 * dependency structures. Lookup of newblk structures is needed to
 * find newly allocated blocks when trying to associate them with
 * their allocdirect or allocindir structure.
 *
 * The lookup routines optionally create and hash a new instance when
 * an existing entry is not found.
 */
#define DEPALLOC	0x0001	/* allocate structure if lookup fails */
#define NODELAY         0x0002  /* cannot do background work */

SIPHASH_KEY softdep_hashkey;

/*
 * Structures and routines associated with pagedep caching.
 */
LIST_HEAD(pagedep_hashhead, pagedep) *pagedep_hashtbl;
u_long	pagedep_hash;		/* size of hash table - 1 */
STATIC struct sema pagedep_in_progress;

/*
 * Look up a pagedep. Return 1 if found, 0 if not found or found
 * when asked to allocate but not associated with any buffer.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in pagedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
STATIC int
pagedep_lookup(struct inode *ip, daddr_t lbn, int flags,
    struct pagedep **pagedeppp)
{
	SIPHASH_CTX ctx;
	struct pagedep *pagedep;
	struct pagedep_hashhead *pagedephd;
	struct mount *mp;
	int i;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("pagedep_lookup: lock not held");
#endif
	mp = ITOV(ip)->v_mount;

	SipHash24_Init(&ctx, &softdep_hashkey);
	SipHash24_Update(&ctx, &mp, sizeof(mp));
	SipHash24_Update(&ctx, &ip->i_number, sizeof(ip->i_number));
	SipHash24_Update(&ctx, &lbn, sizeof(lbn));
	pagedephd = &pagedep_hashtbl[SipHash24_End(&ctx) & pagedep_hash];
top:
	LIST_FOREACH(pagedep, pagedephd, pd_hash)
		if (ip->i_number == pagedep->pd_ino &&
		    lbn == pagedep->pd_lbn &&
		    mp == pagedep->pd_mnt)
			break;
	if (pagedep) {
		*pagedeppp = pagedep;
		if ((flags & DEPALLOC) != 0 &&
		    (pagedep->pd_state & ONWORKLIST) == 0)
			return (0);
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*pagedeppp = NULL;
		return (0);
	}
	if (sema_get(&pagedep_in_progress, &lk) == 0) {
		ACQUIRE_LOCK(&lk);
		goto top;
	}
	pagedep = pool_get(&pagedep_pool, PR_WAITOK | PR_ZERO);
	pagedep->pd_list.wk_type = D_PAGEDEP;
	pagedep->pd_mnt = mp;
	pagedep->pd_ino = ip->i_number;
	pagedep->pd_lbn = lbn;
	LIST_INIT(&pagedep->pd_dirremhd);
	LIST_INIT(&pagedep->pd_pendinghd);
	for (i = 0; i < DAHASHSZ; i++)
		LIST_INIT(&pagedep->pd_diraddhd[i]);
	ACQUIRE_LOCK(&lk);
	LIST_INSERT_HEAD(pagedephd, pagedep, pd_hash);
	sema_release(&pagedep_in_progress);
	*pagedeppp = pagedep;
	return (0);
}

/*
 * Structures and routines associated with inodedep caching.
 */
LIST_HEAD(inodedep_hashhead, inodedep) *inodedep_hashtbl;
STATIC u_long	inodedep_hash;	/* size of hash table - 1 */
STATIC long	num_inodedep;	/* number of inodedep allocated */
STATIC struct sema inodedep_in_progress;

/*
 * Look up a inodedep. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in inodedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
STATIC int
inodedep_lookup(struct fs *fs, ufsino_t inum, int flags,
    struct inodedep **inodedeppp)
{
	SIPHASH_CTX ctx;
	struct inodedep *inodedep;
	struct inodedep_hashhead *inodedephd;
	int firsttry;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("inodedep_lookup: lock not held");
#endif
	firsttry = 1;
	SipHash24_Init(&ctx, &softdep_hashkey);
	SipHash24_Update(&ctx, &fs, sizeof(fs));
	SipHash24_Update(&ctx, &inum, sizeof(inum));
	inodedephd = &inodedep_hashtbl[SipHash24_End(&ctx) & inodedep_hash];
top:
	LIST_FOREACH(inodedep, inodedephd, id_hash)
		if (inum == inodedep->id_ino && fs == inodedep->id_fs)
			break;
	if (inodedep) {
		*inodedeppp = inodedep;
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*inodedeppp = NULL;
		return (0);
	}
	/*
	 * If we are over our limit, try to improve the situation.
	 */
	if (num_inodedep > max_softdeps && firsttry && (flags & NODELAY) == 0 &&
	    request_cleanup(FLUSH_INODES, 1)) {
		firsttry = 0;
		goto top;
	}
	if (sema_get(&inodedep_in_progress, &lk) == 0) {
		ACQUIRE_LOCK(&lk);
		goto top;
	}
	num_inodedep += 1;
	inodedep = pool_get(&inodedep_pool, PR_WAITOK);
	inodedep->id_list.wk_type = D_INODEDEP;
	inodedep->id_fs = fs;
	inodedep->id_ino = inum;
	inodedep->id_state = ALLCOMPLETE;
	inodedep->id_nlinkdelta = 0;
	inodedep->id_savedino1 = NULL;
	inodedep->id_savedsize = -1;
	inodedep->id_buf = NULL;
	LIST_INIT(&inodedep->id_pendinghd);
	LIST_INIT(&inodedep->id_inowait);
	LIST_INIT(&inodedep->id_bufwait);
	TAILQ_INIT(&inodedep->id_inoupdt);
	TAILQ_INIT(&inodedep->id_newinoupdt);
	ACQUIRE_LOCK(&lk);
	LIST_INSERT_HEAD(inodedephd, inodedep, id_hash);
	sema_release(&inodedep_in_progress);
	*inodedeppp = inodedep;
	return (0);
}

/*
 * Structures and routines associated with newblk caching.
 */
LIST_HEAD(newblk_hashhead, newblk) *newblk_hashtbl;
u_long	newblk_hash;		/* size of hash table - 1 */
STATIC struct sema newblk_in_progress;

/*
 * Look up a newblk. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in newblkpp.
 */
STATIC int
newblk_lookup(struct fs *fs, daddr_t newblkno, int flags,
    struct newblk **newblkpp)
{
	SIPHASH_CTX ctx;
	struct newblk *newblk;
	struct newblk_hashhead *newblkhd;

	SipHash24_Init(&ctx, &softdep_hashkey);
	SipHash24_Update(&ctx, &fs, sizeof(fs));
	SipHash24_Update(&ctx, &newblkno, sizeof(newblkno));
	newblkhd = &newblk_hashtbl[SipHash24_End(&ctx) & newblk_hash];
top:
	LIST_FOREACH(newblk, newblkhd, nb_hash)
		if (newblkno == newblk->nb_newblkno && fs == newblk->nb_fs)
			break;
	if (newblk) {
		*newblkpp = newblk;
		return (1);
	}
	if ((flags & DEPALLOC) == 0) {
		*newblkpp = NULL;
		return (0);
	}
	if (sema_get(&newblk_in_progress, NULL) == 0)
		goto top;
	newblk = pool_get(&newblk_pool, PR_WAITOK);
	newblk->nb_state = 0;
	newblk->nb_fs = fs;
	newblk->nb_newblkno = newblkno;
	LIST_INSERT_HEAD(newblkhd, newblk, nb_hash);
	sema_release(&newblk_in_progress);
	*newblkpp = newblk;
	return (0);
}

/*
 * Executed during filesystem system initialization before
 * mounting any file systems.
 */
void 
softdep_initialize(void)
{

	bioops.io_start = softdep_disk_io_initiation;
	bioops.io_complete = softdep_disk_write_complete;
	bioops.io_deallocate = softdep_deallocate_dependencies;
	bioops.io_movedeps = softdep_move_dependencies;
	bioops.io_countdeps = softdep_count_dependencies;

	LIST_INIT(&mkdirlisthd);
	LIST_INIT(&softdep_workitem_pending);
#ifdef KMEMSTATS
	max_softdeps = min (initialvnodes * 8,
	    kmemstats[M_INODEDEP].ks_limit / (2 * sizeof(struct inodedep)));
#else
	max_softdeps = initialvnodes * 4;
#endif
	arc4random_buf(&softdep_hashkey, sizeof(softdep_hashkey));
	pagedep_hashtbl = hashinit(initialvnodes / 5, M_PAGEDEP, M_WAITOK,
	    &pagedep_hash);
	sema_init(&pagedep_in_progress, "pagedep", PRIBIO, 0);
	inodedep_hashtbl = hashinit(initialvnodes, M_INODEDEP, M_WAITOK,
	    &inodedep_hash);
	sema_init(&inodedep_in_progress, "inodedep", PRIBIO, 0);
	newblk_hashtbl = hashinit(64, M_NEWBLK, M_WAITOK, &newblk_hash);
	sema_init(&newblk_in_progress, "newblk", PRIBIO, 0);
	timeout_set(&proc_waiting_timeout, pause_timer, NULL);
	pool_init(&pagedep_pool, sizeof(struct pagedep), 0, IPL_NONE,
	    PR_WAITOK, "pagedep", NULL);
	pool_init(&inodedep_pool, sizeof(struct inodedep), 0, IPL_NONE,
	    PR_WAITOK, "inodedep", NULL);
	pool_init(&newblk_pool, sizeof(struct newblk), 0, IPL_NONE,
	    PR_WAITOK, "newblk", NULL);
	pool_init(&bmsafemap_pool, sizeof(struct bmsafemap), 0, IPL_NONE,
	    PR_WAITOK, "bmsafemap", NULL);
	pool_init(&allocdirect_pool, sizeof(struct allocdirect), 0, IPL_NONE,
	    PR_WAITOK, "allocdir", NULL);
	pool_init(&indirdep_pool, sizeof(struct indirdep), 0, IPL_NONE,
	    PR_WAITOK, "indirdep", NULL);
	pool_init(&allocindir_pool, sizeof(struct allocindir), 0, IPL_NONE,
	    PR_WAITOK, "allocindir", NULL);
	pool_init(&freefrag_pool, sizeof(struct freefrag), 0, IPL_NONE,
	    PR_WAITOK, "freefrag", NULL);
	pool_init(&freeblks_pool, sizeof(struct freeblks), 0, IPL_NONE,
	    PR_WAITOK, "freeblks", NULL);
	pool_init(&freefile_pool, sizeof(struct freefile), 0, IPL_NONE,
	    PR_WAITOK, "freefile", NULL);
	pool_init(&diradd_pool, sizeof(struct diradd), 0, IPL_NONE,
	    PR_WAITOK, "diradd", NULL);
	pool_init(&mkdir_pool, sizeof(struct mkdir), 0, IPL_NONE,
	    PR_WAITOK, "mkdir", NULL);
	pool_init(&dirrem_pool, sizeof(struct dirrem), 0, IPL_NONE,
	    PR_WAITOK, "dirrem", NULL);
	pool_init(&newdirblk_pool, sizeof(struct newdirblk), 0, IPL_NONE,
	    PR_WAITOK, "newdirblk", NULL);
}

/*
 * Called at mount time to notify the dependency code that a
 * filesystem wishes to use it.
 */
int
softdep_mount(struct vnode *devvp, struct mount *mp, struct fs *fs,
    struct ucred *cred)
{
	struct csum_total cstotal;
	struct cg *cgp;
	struct buf *bp;
	int error, cyl;

	/*
	 * When doing soft updates, the counters in the
	 * superblock may have gotten out of sync, so we have
	 * to scan the cylinder groups and recalculate them.
	 */
	if ((fs->fs_flags & FS_UNCLEAN) == 0)
		return (0);
	memset(&cstotal, 0, sizeof(cstotal));
	for (cyl = 0; cyl < fs->fs_ncg; cyl++) {
		if ((error = bread(devvp, fsbtodb(fs, cgtod(fs, cyl)),
		    fs->fs_cgsize, &bp)) != 0) {
			brelse(bp);
			return (error);
		}
		cgp = (struct cg *)bp->b_data;
		cstotal.cs_nffree += cgp->cg_cs.cs_nffree;
		cstotal.cs_nbfree += cgp->cg_cs.cs_nbfree;
		cstotal.cs_nifree += cgp->cg_cs.cs_nifree;
		cstotal.cs_ndir += cgp->cg_cs.cs_ndir;
		fs->fs_cs(fs, cyl) = cgp->cg_cs;
		brelse(bp);
	}
#ifdef DEBUG
	if (memcmp(&cstotal, &fs->fs_cstotal, sizeof(cstotal)))
		printf("ffs_mountfs: superblock updated for soft updates\n");
#endif
	memcpy(&fs->fs_cstotal, &cstotal, sizeof(cstotal));
	return (0);
}

/*
 * Protecting the freemaps (or bitmaps).
 * 
 * To eliminate the need to execute fsck before mounting a file system
 * after a power failure, one must (conservatively) guarantee that the
 * on-disk copy of the bitmaps never indicate that a live inode or block is
 * free.  So, when a block or inode is allocated, the bitmap should be
 * updated (on disk) before any new pointers.  When a block or inode is
 * freed, the bitmap should not be updated until all pointers have been
 * reset.  The latter dependency is handled by the delayed de-allocation
 * approach described below for block and inode de-allocation.  The former
 * dependency is handled by calling the following procedure when a block or
 * inode is allocated. When an inode is allocated an "inodedep" is created
 * with its DEPCOMPLETE flag cleared until its bitmap is written to disk.
 * Each "inodedep" is also inserted into the hash indexing structure so
 * that any additional link additions can be made dependent on the inode
 * allocation.
 * 
 * The ufs file system maintains a number of free block counts (e.g., per
 * cylinder group, per cylinder and per <cylinder, rotational position> pair)
 * in addition to the bitmaps.  These counts are used to improve efficiency
 * during allocation and therefore must be consistent with the bitmaps.
 * There is no convenient way to guarantee post-crash consistency of these
 * counts with simple update ordering, for two main reasons: (1) The counts
 * and bitmaps for a single cylinder group block are not in the same disk
 * sector.  If a disk write is interrupted (e.g., by power failure), one may
 * be written and the other not.  (2) Some of the counts are located in the
 * superblock rather than the cylinder group block. So, we focus our soft
 * updates implementation on protecting the bitmaps. When mounting a
 * filesystem, we recompute the auxiliary counts from the bitmaps.
 */

/*
 * Called just after updating the cylinder group block to allocate an inode.
 */
/* buffer for cylgroup block with inode map */
/* inode related to allocation */
/* new inode number being allocated */
void
softdep_setup_inomapdep(struct buf *bp, struct inode *ip, ufsino_t newinum)
{
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

	/*
	 * Create a dependency for the newly allocated inode.
	 * Panic if it already exists as something is seriously wrong.
	 * Otherwise add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, newinum, DEPALLOC | NODELAY, &inodedep)
	    != 0) {
		FREE_LOCK(&lk);
		panic("softdep_setup_inomapdep: found inode");
	}
	inodedep->id_buf = bp;
	inodedep->id_state &= ~DEPCOMPLETE;
	bmsafemap = bmsafemap_lookup(bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_inodedephd, inodedep, id_deps);
	FREE_LOCK(&lk);
}

/*
 * Called just after updating the cylinder group block to
 * allocate block or fragment.
 */
/* buffer for cylgroup block with block map */
/* filesystem doing allocation */
/* number of newly allocated block */
void
softdep_setup_blkmapdep(struct buf *bp, struct fs *fs, daddr_t newblkno)
{
	struct newblk *newblk;
	struct bmsafemap *bmsafemap;

	/*
	 * Create a dependency for the newly allocated block.
	 * Add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	if (newblk_lookup(fs, newblkno, DEPALLOC, &newblk) != 0)
		panic("softdep_setup_blkmapdep: found block");
	ACQUIRE_LOCK(&lk);
	newblk->nb_bmsafemap = bmsafemap = bmsafemap_lookup(bp);
	LIST_INSERT_HEAD(&bmsafemap->sm_newblkhd, newblk, nb_deps);
	FREE_LOCK(&lk);
}

/*
 * Find the bmsafemap associated with a cylinder group buffer.
 * If none exists, create one. The buffer must be locked when
 * this routine is called and this routine must be called with
 * splbio interrupts blocked.
 */
STATIC struct bmsafemap *
bmsafemap_lookup(struct buf *bp)
{
	struct bmsafemap *bmsafemap;
	struct worklist *wk;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("bmsafemap_lookup: lock not held");
#endif
	LIST_FOREACH(wk, &bp->b_dep, wk_list)
		if (wk->wk_type == D_BMSAFEMAP)
			return (WK_BMSAFEMAP(wk));
	FREE_LOCK(&lk);
	bmsafemap = pool_get(&bmsafemap_pool, PR_WAITOK);
	bmsafemap->sm_list.wk_type = D_BMSAFEMAP;
	bmsafemap->sm_list.wk_state = 0;
	bmsafemap->sm_buf = bp;
	LIST_INIT(&bmsafemap->sm_allocdirecthd);
	LIST_INIT(&bmsafemap->sm_allocindirhd);
	LIST_INIT(&bmsafemap->sm_inodedephd);
	LIST_INIT(&bmsafemap->sm_newblkhd);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&bp->b_dep, &bmsafemap->sm_list);
	return (bmsafemap);
}

/*
 * Direct block allocation dependencies.
 * 
 * When a new block is allocated, the corresponding disk locations must be
 * initialized (with zeros or new data) before the on-disk inode points to
 * them.  Also, the freemap from which the block was allocated must be
 * updated (on disk) before the inode's pointer. These two dependencies are
 * independent of each other and are needed for all file blocks and indirect
 * blocks that are pointed to directly by the inode.  Just before the
 * "in-core" version of the inode is updated with a newly allocated block
 * number, a procedure (below) is called to setup allocation dependency
 * structures.  These structures are removed when the corresponding
 * dependencies are satisfied or when the block allocation becomes obsolete
 * (i.e., the file is deleted, the block is de-allocated, or the block is a
 * fragment that gets upgraded).  All of these cases are handled in
 * procedures described later.
 * 
 * When a file extension causes a fragment to be upgraded, either to a larger
 * fragment or to a full block, the on-disk location may change (if the
 * previous fragment could not simply be extended). In this case, the old
 * fragment must be de-allocated, but not until after the inode's pointer has
 * been updated. In most cases, this is handled by later procedures, which
 * will construct a "freefrag" structure to be added to the workitem queue
 * when the inode update is complete (or obsolete).  The main exception to
 * this is when an allocation occurs while a pending allocation dependency
 * (for the same block pointer) remains.  This case is handled in the main
 * allocation dependency setup procedure by immediately freeing the
 * unreferenced fragments.
 */ 
/* inode to which block is being added */
/* block pointer within inode */
/* disk block number being added */
/* previous block number, 0 unless frag */
/* size of new block */
/* size of new block */
/* bp for allocated block */
void 
softdep_setup_allocdirect(struct inode *ip, daddr_t lbn, daddr_t newblkno,
    daddr_t oldblkno, long newsize, long oldsize, struct buf *bp)
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct bmsafemap *bmsafemap;
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct newblk *newblk;

	adp = pool_get(&allocdirect_pool, PR_WAITOK | PR_ZERO);
	adp->ad_list.wk_type = D_ALLOCDIRECT;
	adp->ad_lbn = lbn;
	adp->ad_newblkno = newblkno;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;
	adp->ad_state = ATTACHED;
	LIST_INIT(&adp->ad_newdirblk);
	if (newblkno == oldblkno)
		adp->ad_freefrag = NULL;
	else
		adp->ad_freefrag = newfreefrag(ip, oldblkno, oldsize);

	if (newblk_lookup(ip->i_fs, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocdirect: lost block");

	ACQUIRE_LOCK(&lk);
	inodedep_lookup(ip->i_fs, ip->i_number, DEPALLOC | NODELAY, &inodedep);
	adp->ad_inodedep = inodedep;

	if (newblk->nb_state == DEPCOMPLETE) {
		adp->ad_state |= DEPCOMPLETE;
		adp->ad_buf = NULL;
	} else {
		bmsafemap = newblk->nb_bmsafemap;
		adp->ad_buf = bmsafemap->sm_buf;
		LIST_REMOVE(newblk, nb_deps);
		LIST_INSERT_HEAD(&bmsafemap->sm_allocdirecthd, adp, ad_deps);
	}
	LIST_REMOVE(newblk, nb_hash);
	pool_put(&newblk_pool, newblk);

	if (bp == NULL) {
		/*
		 * XXXUBC - Yes, I know how to fix this, but not right now.
		 */
		panic("softdep_setup_allocdirect: Bonk art in the head");
	}
	WORKLIST_INSERT(&bp->b_dep, &adp->ad_list);
	if (lbn >= NDADDR) {
		/* allocating an indirect block */
		if (oldblkno != 0) {
			FREE_LOCK(&lk);
			panic("softdep_setup_allocdirect: non-zero indir");
		}
	} else {
		/*
		 * Allocating a direct block.
		 *
		 * If we are allocating a directory block, then we must
		 * allocate an associated pagedep to track additions and
		 * deletions.
		 */
		if ((DIP(ip, mode) & IFMT) == IFDIR &&
		    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
			WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	}
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newinoupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_lbn <= lbn) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_lbn == lbn)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	TAILQ_FOREACH(oldadp, adphead, ad_next) {
		if (oldadp->ad_lbn >= lbn)
			break;
	}
	if (oldadp == NULL) {
		FREE_LOCK(&lk);
		panic("softdep_setup_allocdirect: lost entry");
	}
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_lbn == lbn)
		allocdirect_merge(adphead, adp, oldadp);
	FREE_LOCK(&lk);
}

/*
 * Replace an old allocdirect dependency with a newer one.
 * This routine must be called with splbio interrupts blocked.
 */
/* head of list holding allocdirects */
/* allocdirect being added */
/* existing allocdirect being checked */
STATIC void
allocdirect_merge(struct allocdirectlst *adphead, struct allocdirect *newadp,
    struct allocdirect *oldadp)
{
	struct worklist *wk;
	struct freefrag *freefrag;
	struct newdirblk *newdirblk;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("allocdirect_merge: lock not held");
#endif
	if (newadp->ad_oldblkno != oldadp->ad_newblkno ||
	    newadp->ad_oldsize != oldadp->ad_newsize ||
	    newadp->ad_lbn >= NDADDR) {
		FREE_LOCK(&lk);
		panic("allocdirect_merge: old %lld != new %lld || lbn %lld >= "
		    "%d", (long long)newadp->ad_oldblkno,
		    (long long)oldadp->ad_newblkno, (long long)newadp->ad_lbn,
		    NDADDR);
	}
	newadp->ad_oldblkno = oldadp->ad_oldblkno;
	newadp->ad_oldsize = oldadp->ad_oldsize;
	/*
	 * If the old dependency had a fragment to free or had never
	 * previously had a block allocated, then the new dependency
	 * can immediately post its freefrag and adopt the old freefrag.
	 * This action is done by swapping the freefrag dependencies.
	 * The new dependency gains the old one's freefrag, and the
	 * old one gets the new one and then immediately puts it on
	 * the worklist when it is freed by free_allocdirect. It is
	 * not possible to do this swap when the old dependency had a
	 * non-zero size but no previous fragment to free. This condition
	 * arises when the new block is an extension of the old block.
	 * Here, the first part of the fragment allocated to the new
	 * dependency is part of the block currently claimed on disk by
	 * the old dependency, so cannot legitimately be freed until the
	 * conditions for the new dependency are fulfilled.
	 */
	if (oldadp->ad_freefrag != NULL || oldadp->ad_oldblkno == 0) {
		freefrag = newadp->ad_freefrag;
		newadp->ad_freefrag = oldadp->ad_freefrag;
		oldadp->ad_freefrag = freefrag;
	}
	/*
	 * If we are tracking a new directory-block allocation,
	 * move it from the old allocdirect to the new allocdirect.
	 */
	if ((wk = LIST_FIRST(&oldadp->ad_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (LIST_FIRST(&oldadp->ad_newdirblk) != NULL)
			panic("allocdirect_merge: extra newdirblk");
		WORKLIST_INSERT(&newadp->ad_newdirblk, &newdirblk->db_list);
	}
	free_allocdirect(adphead, oldadp, 0);
}
		
/*
 * Allocate a new freefrag structure if needed.
 */
STATIC struct freefrag *
newfreefrag(struct inode *ip, daddr_t blkno, long size)
{
	struct freefrag *freefrag;
	struct fs *fs;

	if (blkno == 0)
		return (NULL);
	fs = ip->i_fs;
	if (fragnum(fs, blkno) + numfrags(fs, size) > fs->fs_frag)
		panic("newfreefrag: frag size");
	freefrag = pool_get(&freefrag_pool, PR_WAITOK);
	freefrag->ff_list.wk_type = D_FREEFRAG;
	freefrag->ff_state = DIP(ip, uid) & ~ONWORKLIST; /* used below */
	freefrag->ff_inum = ip->i_number;
	freefrag->ff_mnt = ITOV(ip)->v_mount;
	freefrag->ff_devvp = ip->i_devvp;
	freefrag->ff_blkno = blkno;
	freefrag->ff_fragsize = size;
	return (freefrag);
}

/*
 * This workitem de-allocates fragments that were replaced during
 * file block allocation.
 */
STATIC void 
handle_workitem_freefrag(struct freefrag *freefrag)
{
	struct inode tip;
	struct ufs1_dinode dtip1;

	tip.i_vnode = NULL;
	tip.i_din1 = &dtip1;
	tip.i_fs = VFSTOUFS(freefrag->ff_mnt)->um_fs;
	tip.i_ump = VFSTOUFS(freefrag->ff_mnt);
	tip.i_dev = freefrag->ff_devvp->v_rdev;
	tip.i_number = freefrag->ff_inum;
	tip.i_ffs1_uid = freefrag->ff_state & ~ONWORKLIST; /* set above */
	ffs_blkfree(&tip, freefrag->ff_blkno, freefrag->ff_fragsize);
	pool_put(&freefrag_pool, freefrag);
}

/*
 * Indirect block allocation dependencies.
 * 
 * The same dependencies that exist for a direct block also exist when
 * a new block is allocated and pointed to by an entry in a block of
 * indirect pointers. The undo/redo states described above are also
 * used here. Because an indirect block contains many pointers that
 * may have dependencies, a second copy of the entire in-memory indirect
 * block is kept. The buffer cache copy is always completely up-to-date.
 * The second copy, which is used only as a source for disk writes,
 * contains only the safe pointers (i.e., those that have no remaining
 * update dependencies). The second copy is freed when all pointers
 * are safe. The cache is not allowed to replace indirect blocks with
 * pending update dependencies. If a buffer containing an indirect
 * block with dependencies is written, these routines will mark it
 * dirty again. It can only be successfully written once all the
 * dependencies are removed. The ffs_fsync routine in conjunction with
 * softdep_sync_metadata work together to get all the dependencies
 * removed so that a file can be successfully written to disk. Three
 * procedures are used when setting up indirect block pointer
 * dependencies. The division is necessary because of the organization
 * of the "balloc" routine and because of the distinction between file
 * pages and file metadata blocks.
 */

/*
 * Allocate a new allocindir structure.
 */
/* inode for file being extended */
/* offset of pointer in indirect block */
/* disk block number being added */
/* previous block number, 0 if none */
STATIC struct allocindir *
newallocindir(struct inode *ip, int ptrno, daddr_t newblkno,
    daddr_t oldblkno)
{
	struct allocindir *aip;

	aip = pool_get(&allocindir_pool, PR_WAITOK | PR_ZERO);
	aip->ai_list.wk_type = D_ALLOCINDIR;
	aip->ai_state = ATTACHED;
	aip->ai_offset = ptrno;
	aip->ai_newblkno = newblkno;
	aip->ai_oldblkno = oldblkno;
	aip->ai_freefrag = newfreefrag(ip, oldblkno, ip->i_fs->fs_bsize);
	return (aip);
}

/*
 * Called just before setting an indirect block pointer
 * to a newly allocated file page.
 */
/* inode for file being extended */
/* allocated block number within file */
/* buffer with indirect blk referencing page */
/* offset of pointer in indirect block */
/* disk block number being added */
/* previous block number, 0 if none */
/* buffer holding allocated page */
void
softdep_setup_allocindir_page(struct inode *ip, daddr_t lbn, struct buf *bp,
    int ptrno, daddr_t newblkno, daddr_t oldblkno, struct buf *nbp)
{
	struct allocindir *aip;
	struct pagedep *pagedep;

	aip = newallocindir(ip, ptrno, newblkno, oldblkno);
	ACQUIRE_LOCK(&lk);
	/*
	 * If we are allocating a directory page, then we must
	 * allocate an associated pagedep to track additions and
	 * deletions.
	 */
	if ((DIP(ip, mode) & IFMT) == IFDIR &&
	    pagedep_lookup(ip, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&nbp->b_dep, &pagedep->pd_list);
	if (nbp == NULL) {
		/*
		 * XXXUBC - Yes, I know how to fix this, but not right now.
		 */
		panic("softdep_setup_allocindir_page: Bonk art in the head");
	}
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	FREE_LOCK(&lk);
	setup_allocindir_phase2(bp, ip, aip);
}

/*
 * Called just before setting an indirect block pointer to a
 * newly allocated indirect block.
 */
/* newly allocated indirect block */
/* inode for file being extended */
/* indirect block referencing allocated block */
/* offset of pointer in indirect block */
/* disk block number being added */
void
softdep_setup_allocindir_meta(struct buf *nbp, struct inode *ip,
    struct buf *bp, int ptrno, daddr_t newblkno)
{
	struct allocindir *aip;

	aip = newallocindir(ip, ptrno, newblkno, 0);
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_list);
	FREE_LOCK(&lk);
	setup_allocindir_phase2(bp, ip, aip);
}

/*
 * Called to finish the allocation of the "aip" allocated
 * by one of the two routines above.
 */
/* in-memory copy of the indirect block */
/* inode for file being extended */
/* allocindir allocated by the above routines */
STATIC void 
setup_allocindir_phase2(struct buf *bp, struct inode *ip,
    struct allocindir *aip)
{
	struct worklist *wk;
	struct indirdep *indirdep, *newindirdep;
	struct bmsafemap *bmsafemap;
	struct allocindir *oldaip;
	struct freefrag *freefrag;
	struct newblk *newblk;

	if (bp->b_lblkno >= 0)
		panic("setup_allocindir_phase2: not indir blk");
	for (indirdep = NULL, newindirdep = NULL; ; ) {
		ACQUIRE_LOCK(&lk);
		LIST_FOREACH(wk, &bp->b_dep, wk_list) {
			if (wk->wk_type != D_INDIRDEP)
				continue;
			indirdep = WK_INDIRDEP(wk);
			break;
		}
		if (indirdep == NULL && newindirdep) {
			indirdep = newindirdep;
			WORKLIST_INSERT(&bp->b_dep, &indirdep->ir_list);
			newindirdep = NULL;
		}
		FREE_LOCK(&lk);
		if (indirdep) {
			if (newblk_lookup(ip->i_fs, aip->ai_newblkno, 0,
			    &newblk) == 0)
				panic("setup_allocindir: lost block");
			ACQUIRE_LOCK(&lk);
			if (newblk->nb_state == DEPCOMPLETE) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
			} else {
				bmsafemap = newblk->nb_bmsafemap;
				aip->ai_buf = bmsafemap->sm_buf;
				LIST_REMOVE(newblk, nb_deps);
				LIST_INSERT_HEAD(&bmsafemap->sm_allocindirhd,
				    aip, ai_deps);
			}
			LIST_REMOVE(newblk, nb_hash);
			pool_put(&newblk_pool, newblk);
			aip->ai_indirdep = indirdep;
			/*
			 * Check to see if there is an existing dependency
			 * for this block. If there is, merge the old
			 * dependency into the new one.
			 */
			if (aip->ai_oldblkno == 0)
				oldaip = NULL;
			else

				LIST_FOREACH(oldaip, &indirdep->ir_deplisthd, ai_next)
					if (oldaip->ai_offset == aip->ai_offset)
						break;
			freefrag = NULL;
			if (oldaip != NULL) {
				if (oldaip->ai_newblkno != aip->ai_oldblkno) {
					FREE_LOCK(&lk);
					panic("setup_allocindir_phase2: blkno");
				}
				aip->ai_oldblkno = oldaip->ai_oldblkno;
				freefrag = aip->ai_freefrag;
				aip->ai_freefrag = oldaip->ai_freefrag;
				oldaip->ai_freefrag = NULL;
				free_allocindir(oldaip, NULL);
			}
			LIST_INSERT_HEAD(&indirdep->ir_deplisthd, aip, ai_next);
			if (ip->i_ump->um_fstype == UM_UFS1)
				((int32_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			else
				((int64_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			FREE_LOCK(&lk);
			if (freefrag != NULL)
				handle_workitem_freefrag(freefrag);
		}
		if (newindirdep) {
			if (indirdep->ir_savebp != NULL)
				brelse(newindirdep->ir_savebp);
			WORKITEM_FREE(newindirdep, D_INDIRDEP);
		}
		if (indirdep)
			break;
		newindirdep = pool_get(&indirdep_pool, PR_WAITOK);
		newindirdep->ir_list.wk_type = D_INDIRDEP;
		newindirdep->ir_state = ATTACHED;
		if (ip->i_ump->um_fstype == UM_UFS1)
			newindirdep->ir_state |= UFS1FMT;
		LIST_INIT(&newindirdep->ir_deplisthd);
		LIST_INIT(&newindirdep->ir_donehd);
		if (bp->b_blkno == bp->b_lblkno) {
			VOP_BMAP(bp->b_vp, bp->b_lblkno, NULL, &bp->b_blkno,
				NULL);
		}
		newindirdep->ir_savebp =
		    getblk(ip->i_devvp, bp->b_blkno, bp->b_bcount, 0, 0);
#if 0
		BUF_KERNPROC(newindirdep->ir_savebp);
#endif
		memcpy(newindirdep->ir_savebp->b_data, bp->b_data, bp->b_bcount);
	}
}

/*
 * Block de-allocation dependencies.
 * 
 * When blocks are de-allocated, the on-disk pointers must be nullified before
 * the blocks are made available for use by other files.  (The true
 * requirement is that old pointers must be nullified before new on-disk
 * pointers are set.  We chose this slightly more stringent requirement to
 * reduce complexity.) Our implementation handles this dependency by updating
 * the inode (or indirect block) appropriately but delaying the actual block
 * de-allocation (i.e., freemap and free space count manipulation) until
 * after the updated versions reach stable storage.  After the disk is
 * updated, the blocks can be safely de-allocated whenever it is convenient.
 * This implementation handles only the common case of reducing a file's
 * length to zero. Other cases are handled by the conventional synchronous
 * write approach.
 *
 * The ffs implementation with which we worked double-checks
 * the state of the block pointers and file size as it reduces
 * a file's length.  Some of this code is replicated here in our
 * soft updates implementation.  The freeblks->fb_chkcnt field is
 * used to transfer a part of this information to the procedure
 * that eventually de-allocates the blocks.
 *
 * This routine should be called from the routine that shortens
 * a file's length, before the inode's size or block pointers
 * are modified. It will save the block pointer information for
 * later release and zero the inode so that the calling routine
 * can release it.
 */
/* The inode whose length is to be reduced */
/* The new length for the file */
void
softdep_setup_freeblocks(struct inode *ip, off_t length)
{
	struct freeblks *freeblks;
	struct inodedep *inodedep;
	struct allocdirect *adp;
	struct vnode *vp;
	struct buf *bp;
	struct fs *fs;
	int i, delay, error;

	fs = ip->i_fs;
	if (length != 0)
		panic("softdep_setup_freeblocks: non-zero length");
	freeblks = pool_get(&freeblks_pool, PR_WAITOK | PR_ZERO);
	freeblks->fb_list.wk_type = D_FREEBLKS;
	freeblks->fb_state = ATTACHED;
	freeblks->fb_uid = DIP(ip, uid);
	freeblks->fb_previousinum = ip->i_number;
	freeblks->fb_devvp = ip->i_devvp;
	freeblks->fb_mnt = ITOV(ip)->v_mount;
	freeblks->fb_oldsize = DIP(ip, size);
	freeblks->fb_newsize = length;
	freeblks->fb_chkcnt = DIP(ip, blocks);

	for (i = 0; i < NDADDR; i++) {
		freeblks->fb_dblks[i] = DIP(ip, db[i]);
		DIP_ASSIGN(ip, db[i], 0);
	}

	for (i = 0; i < NIADDR; i++) {
		freeblks->fb_iblks[i] = DIP(ip, ib[i]);
		DIP_ASSIGN(ip, ib[i], 0);
	}

	DIP_ASSIGN(ip, blocks, 0);
	DIP_ASSIGN(ip, size, 0);

	/*
	 * Push the zero'ed inode to to its disk buffer so that we are free
	 * to delete its dependencies below. Once the dependencies are gone
	 * the buffer can be safely released.
	 */
	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->fs_bsize, &bp)) != 0)
		softdep_error("softdep_setup_freeblocks", error);

	if (ip->i_ump->um_fstype == UM_UFS1)
		*((struct ufs1_dinode *) bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din1;
	else
		*((struct ufs2_dinode *) bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din2;

	/*
	 * Find and eliminate any inode dependencies.
	 */
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(fs, ip->i_number, DEPALLOC, &inodedep);
	if ((inodedep->id_state & IOSTARTED) != 0) {
		FREE_LOCK(&lk);
		panic("softdep_setup_freeblocks: inode busy");
	}
	/*
	 * Add the freeblks structure to the list of operations that
	 * must await the zero'ed inode being written to disk. If we
	 * still have a bitmap dependency (delay == 0), then the inode
	 * has never been written to disk, so we can process the
	 * freeblks below once we have deleted the dependencies.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	if (delay)
		WORKLIST_INSERT(&inodedep->id_bufwait, &freeblks->fb_list);
	/*
	 * Because the file length has been truncated to zero, any
	 * pending block allocation dependency structures associated
	 * with this inode are obsolete and can simply be de-allocated.
	 * We must first merge the two dependency lists to get rid of
	 * any duplicate freefrag structures, then purge the merged list.
	 * If we still have a bitmap dependency, then the inode has never
	 * been written to disk, so we can free any fragments without delay.
	 */
	merge_inode_lists(inodedep);
	while ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != NULL)
		free_allocdirect(&inodedep->id_inoupdt, adp, delay);
	FREE_LOCK(&lk);
	bdwrite(bp);
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 * Once they are all there, walk the list and get rid of
	 * any dependencies.
	 */
	vp = ITOV(ip);
	ACQUIRE_LOCK(&lk);
	drain_output(vp, 1);
	while ((bp = LIST_FIRST(&vp->v_dirtyblkhd))) {
		if (getdirtybuf(bp, MNT_WAIT) <= 0)
			break;
		(void) inodedep_lookup(fs, ip->i_number, 0, &inodedep);
		deallocate_dependencies(bp, inodedep);
		bp->b_flags |= B_INVAL | B_NOCACHE;
		FREE_LOCK(&lk);
		brelse(bp);
		ACQUIRE_LOCK(&lk);
	}
	if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);

	if (delay) {
		freeblks->fb_state |= DEPCOMPLETE;
		/*
		 * If the inode with zeroed block pointers is now on disk we
		 * can start freeing blocks. Add freeblks to the worklist
		 * instead of calling handle_workitem_freeblocks() directly as
		 * it is more likely that additional IO is needed to complete
		 * the request than in the !delay case.
		 */
		if ((freeblks->fb_state & ALLCOMPLETE) == ALLCOMPLETE)
			add_to_worklist(&freeblks->fb_list);
	}

	FREE_LOCK(&lk);
	/*
	 * If the inode has never been written to disk (delay == 0),
	 * then we can process the freeblks now that we have deleted
	 * the dependencies.
	 */
	if (!delay)
		handle_workitem_freeblocks(freeblks);
}

/*
 * Reclaim any dependency structures from a buffer that is about to
 * be reallocated to a new vnode. The buffer must be locked, thus,
 * no I/O completion operations can occur while we are manipulating
 * its associated dependencies. The mutex is held so that other I/O's
 * associated with related dependencies do not occur.
 */
STATIC void
deallocate_dependencies(struct buf *bp, struct inodedep *inodedep)
{
	struct worklist *wk;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct dirrem *dirrem;
	struct diradd *dap;
	int i;

	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		switch (wk->wk_type) {

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			/*
			 * None of the indirect pointers will ever be visible,
			 * so they can simply be tossed. GOINGAWAY ensures
			 * that allocated pointers will be saved in the buffer
			 * cache until they are freed. Note that they will
			 * only be able to be found by their physical address
			 * since the inode mapping the logical address will
			 * be gone. The save buffer used for the safe copy
			 * was allocated in setup_allocindir_phase2 using
			 * the physical address so it could be used for this
			 * purpose. Hence we swap the safe copy with the real
			 * copy, allowing the safe copy to be freed and holding
			 * on to the real copy for later use in indir_trunc.
			 */
			if (indirdep->ir_state & GOINGAWAY) {
				FREE_LOCK(&lk);
				panic("deallocate_dependencies: already gone");
			}
			indirdep->ir_state |= GOINGAWAY;
			while ((aip = LIST_FIRST(&indirdep->ir_deplisthd)))
				free_allocindir(aip, inodedep);
			if (bp->b_lblkno >= 0 ||
			    bp->b_blkno != indirdep->ir_savebp->b_lblkno) {
				FREE_LOCK(&lk);
				panic("deallocate_dependencies: not indir");
			}
			memcpy(indirdep->ir_savebp->b_data, bp->b_data,
			    bp->b_bcount);
			WORKLIST_REMOVE(wk);
			WORKLIST_INSERT(&indirdep->ir_savebp->b_dep, wk);
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			/*
			 * None of the directory additions will ever be
			 * visible, so they can simply be tossed.
			 */
			for (i = 0; i < DAHASHSZ; i++)
				while ((dap =
				    LIST_FIRST(&pagedep->pd_diraddhd[i])))
					free_diradd(dap);
			while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)))
				free_diradd(dap);
			/*
			 * Copy any directory remove dependencies to the list
			 * to be processed after the zero'ed inode is written.
			 * If the inode has already been written, then they 
			 * can be dumped directly onto the work list.
			 */
			while ((dirrem = LIST_FIRST(&pagedep->pd_dirremhd))) {
				LIST_REMOVE(dirrem, dm_next);
				dirrem->dm_dirinum = pagedep->pd_ino;
				if (inodedep == NULL ||
				    (inodedep->id_state & ALLCOMPLETE) ==
				     ALLCOMPLETE)
					add_to_worklist(&dirrem->dm_list);
				else
					WORKLIST_INSERT(&inodedep->id_bufwait,
					    &dirrem->dm_list);
			}
			if ((pagedep->pd_state & NEWBLOCK) != 0) {
				LIST_FOREACH(wk, &inodedep->id_bufwait, wk_list)
					if (wk->wk_type == D_NEWDIRBLK &&
					    WK_NEWDIRBLK(wk)->db_pagedep ==
					    pagedep)
						break;
				if (wk != NULL) {
					WORKLIST_REMOVE(wk);
					free_newdirblk(WK_NEWDIRBLK(wk));
				} else {
					FREE_LOCK(&lk);
					panic("deallocate_dependencies: "
					    "lost pagedep");
					}
			}
			WORKLIST_REMOVE(&pagedep->pd_list);
			LIST_REMOVE(pagedep, pd_hash);
			WORKITEM_FREE(pagedep, D_PAGEDEP);
			continue;

		case D_ALLOCINDIR:
			free_allocindir(WK_ALLOCINDIR(wk), inodedep);
			continue;

		case D_ALLOCDIRECT:
		case D_INODEDEP:
			FREE_LOCK(&lk);
			panic("deallocate_dependencies: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */

		default:
			FREE_LOCK(&lk);
			panic("deallocate_dependencies: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
}

/*
 * Free an allocdirect. Generate a new freefrag work request if appropriate.
 * This routine must be called with splbio interrupts blocked.
 */
STATIC void
free_allocdirect(struct allocdirectlst *adphead, struct allocdirect *adp,
    int delay)
{
	struct newdirblk *newdirblk;
	struct worklist *wk;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_allocdirect: lock not held");
#endif
	if ((adp->ad_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(adp, ad_deps);
	TAILQ_REMOVE(adphead, adp, ad_next);
	if ((adp->ad_state & COMPLETE) == 0)
		WORKLIST_REMOVE(&adp->ad_list);
	if (adp->ad_freefrag != NULL) {
		if (delay)
			WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
			    &adp->ad_freefrag->ff_list);
		else
			add_to_worklist(&adp->ad_freefrag->ff_list);
	}
	if ((wk = LIST_FIRST(&adp->ad_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (LIST_FIRST(&adp->ad_newdirblk) != NULL)
			panic("free_allocdirect: extra newdirblk");
		if (delay)
			WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
			    &newdirblk->db_list);
		else
			free_newdirblk(newdirblk);
	}
	WORKITEM_FREE(adp, D_ALLOCDIRECT);
}

/*
 * Free a newdirblk. Clear the NEWBLOCK flag on its associated pagedep.
 * This routine must be called with splbio interrupts blocked.
 */
void
free_newdirblk(struct newdirblk *newdirblk)
{
	struct pagedep *pagedep;
	struct diradd *dap;
	int i;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_newdirblk: lock not held");
#endif
	/*
	 * If the pagedep is still linked onto the directory buffer
	 * dependency chain, then some of the entries on the
	 * pd_pendinghd list may not be committed to disk yet. In
	 * this case, we will simply clear the NEWBLOCK flag and
	 * let the pd_pendinghd list be processed when the pagedep
	 * is next written. If the pagedep is no longer on the buffer
	 * dependency chain, then all the entries on the pd_pending
	 * list are committed to disk and we can free them here.
	 */
	pagedep = newdirblk->db_pagedep;
	pagedep->pd_state &= ~NEWBLOCK;
	if ((pagedep->pd_state & ONWORKLIST) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap);
	/*
	 * If no dependencies remain, the pagedep will be freed.
	 */
	for (i = 0; i < DAHASHSZ; i++)
		if (LIST_FIRST(&pagedep->pd_diraddhd[i]) != NULL)
			break;
	if (i == DAHASHSZ && (pagedep->pd_state & ONWORKLIST) == 0) {
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
}

/*
 * Prepare an inode to be freed. The actual free operation is not
 * done until the zero'ed inode has been written to disk.
 */
void
softdep_freefile(struct vnode *pvp, ufsino_t ino, mode_t mode)
{
	struct inode *ip = VTOI(pvp);
	struct inodedep *inodedep;
	struct freefile *freefile;

	/*
	 * This sets up the inode de-allocation dependency.
	 */
	freefile = pool_get(&freefile_pool, PR_WAITOK);
	freefile->fx_list.wk_type = D_FREEFILE;
	freefile->fx_list.wk_state = 0;
	freefile->fx_mode = mode;
	freefile->fx_oldinum = ino;
	freefile->fx_devvp = ip->i_devvp;
	freefile->fx_mnt = ITOV(ip)->v_mount;

	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can free the file immediately.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ino, 0, &inodedep) == 0 ||
	    check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		handle_workitem_freefile(freefile);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &freefile->fx_list);
	FREE_LOCK(&lk);
}

/*
 * Check to see if an inode has never been written to disk. If
 * so free the inodedep and return success, otherwise return failure.
 * This routine must be called with splbio interrupts blocked.
 *
 * If we still have a bitmap dependency, then the inode has never
 * been written to disk. Drop the dependency as it is no longer
 * necessary since the inode is being deallocated. We set the
 * ALLCOMPLETE flags since the bitmap now properly shows that the
 * inode is not allocated. Even if the inode is actively being
 * written, it has been rolled back to its zero'ed state, so we
 * are ensured that a zero inode is what is on the disk. For short
 * lived files, this change will usually result in removing all the
 * dependencies from the inode so that it can be freed immediately.
 */
STATIC int
check_inode_unwritten(struct inodedep *inodedep)
{
	splassert(IPL_BIO);

	if ((inodedep->id_state & DEPCOMPLETE) != 0 ||
	    LIST_FIRST(&inodedep->id_pendinghd) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL ||
	    inodedep->id_nlinkdelta != 0)
		return (0);
	inodedep->id_state |= ALLCOMPLETE;
	LIST_REMOVE(inodedep, id_deps);
	inodedep->id_buf = NULL;
	if (inodedep->id_state & ONWORKLIST)
		WORKLIST_REMOVE(&inodedep->id_list);
	if (inodedep->id_savedino1 != NULL) {
		free(inodedep->id_savedino1, M_INODEDEP, 0);
		inodedep->id_savedino1 = NULL;
	}
	if (free_inodedep(inodedep) == 0) {
		FREE_LOCK(&lk);
		panic("check_inode_unwritten: busy inode");
	}
	return (1);
}

/*
 * Try to free an inodedep structure. Return 1 if it could be freed.
 */
STATIC int
free_inodedep(struct inodedep *inodedep)
{

	if ((inodedep->id_state & ONWORKLIST) != 0 ||
	    (inodedep->id_state & ALLCOMPLETE) != ALLCOMPLETE ||
	    LIST_FIRST(&inodedep->id_pendinghd) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL ||
	    inodedep->id_nlinkdelta != 0 || inodedep->id_savedino1 != NULL)
		return (0);
	LIST_REMOVE(inodedep, id_hash);
	WORKITEM_FREE(inodedep, D_INODEDEP);
	num_inodedep -= 1;
	return (1);
}

/*
 * This workitem routine performs the block de-allocation.
 * The workitem is added to the pending list after the updated
 * inode block has been written to disk.  As mentioned above,
 * checks regarding the number of blocks de-allocated (compared
 * to the number of blocks allocated for the file) are also
 * performed in this function.
 */
STATIC void
handle_workitem_freeblocks(struct freeblks *freeblks)
{
	struct inode tip;
	daddr_t bn;
	union {
		struct ufs1_dinode di1;
		struct ufs2_dinode di2;
	} di;
	struct fs *fs;
	int i, level, bsize;
	long nblocks, blocksreleased = 0;
	int error, allerror = 0;
	daddr_t baselbns[NIADDR], tmpval;

	if (VFSTOUFS(freeblks->fb_mnt)->um_fstype == UM_UFS1)
		tip.i_din1 = &di.di1;
	else
		tip.i_din2 = &di.di2;

	tip.i_fs = fs = VFSTOUFS(freeblks->fb_mnt)->um_fs;
	tip.i_number = freeblks->fb_previousinum;
	tip.i_ump = VFSTOUFS(freeblks->fb_mnt);
	tip.i_dev = freeblks->fb_devvp->v_rdev;
	DIP_ASSIGN(&tip, size, freeblks->fb_oldsize);
	DIP_ASSIGN(&tip, uid, freeblks->fb_uid);
	tip.i_vnode = NULL;
	tmpval = 1;
	baselbns[0] = NDADDR;
	for (i = 1; i < NIADDR; i++) {
		tmpval *= NINDIR(fs);
		baselbns[i] = baselbns[i - 1] + tmpval;
	}
	nblocks = btodb(fs->fs_bsize);
	blocksreleased = 0;
	/*
	 * Indirect blocks first.
	 */
	for (level = (NIADDR - 1); level >= 0; level--) {
		if ((bn = freeblks->fb_iblks[level]) == 0)
			continue;
		if ((error = indir_trunc(&tip, fsbtodb(fs, bn), level,
		    baselbns[level], &blocksreleased)) != 0)
			allerror = error;
		ffs_blkfree(&tip, bn, fs->fs_bsize);
		blocksreleased += nblocks;
	}
	/*
	 * All direct blocks or frags.
	 */
	for (i = (NDADDR - 1); i >= 0; i--) {
		if ((bn = freeblks->fb_dblks[i]) == 0)
			continue;
		bsize = blksize(fs, &tip, i);
		ffs_blkfree(&tip, bn, bsize);
		blocksreleased += btodb(bsize);
	}

#ifdef DIAGNOSTIC
	if (freeblks->fb_chkcnt != blocksreleased)
		printf("handle_workitem_freeblocks: block count\n");
	if (allerror)
		softdep_error("handle_workitem_freeblks", allerror);
#endif /* DIAGNOSTIC */
	WORKITEM_FREE(freeblks, D_FREEBLKS);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block dbn. If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 */
STATIC int
indir_trunc(struct inode *ip, daddr_t dbn, int level, daddr_t lbn,
    long *countp)
{
	struct buf *bp;
	int32_t *bap1 = NULL;
	int64_t nb, *bap2 = NULL;
	struct fs *fs;
	struct worklist *wk;
	struct indirdep *indirdep;
	int i, lbnadd, nblocks, ufs1fmt;
	int error, allerror = 0;

	fs = ip->i_fs;
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	/*
	 * Get buffer of block pointers to be freed. This routine is not
	 * called until the zero'ed inode has been written, so it is safe
	 * to free blocks as they are encountered. Because the inode has
	 * been zero'ed, calls to bmap on these blocks will fail. So, we
	 * have to use the on-disk address and the block device for the
	 * filesystem to look them up. If the file was deleted before its
	 * indirect blocks were all written to disk, the routine that set
	 * us up (deallocate_dependencies) will have arranged to leave
	 * a complete copy of the indirect block in memory for our use.
	 * Otherwise we have to read the blocks in from the disk.
	 */
	ACQUIRE_LOCK(&lk);
	if ((bp = incore(ip->i_devvp, dbn)) != NULL &&
	    (wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		if (wk->wk_type != D_INDIRDEP ||
		    (indirdep = WK_INDIRDEP(wk))->ir_savebp != bp ||
		    (indirdep->ir_state & GOINGAWAY) == 0) {
			FREE_LOCK(&lk);
			panic("indir_trunc: lost indirdep");
		}
		WORKLIST_REMOVE(wk);
		WORKITEM_FREE(indirdep, D_INDIRDEP);
		if (LIST_FIRST(&bp->b_dep) != NULL) {
			FREE_LOCK(&lk);
			panic("indir_trunc: dangling dep");
		}
		FREE_LOCK(&lk);
	} else {
		FREE_LOCK(&lk);
		error = bread(ip->i_devvp, dbn, (int)fs->fs_bsize, &bp);
		if (error)
			return (error);
	}
	/*
	 * Recursively free indirect blocks.
	 */
	if (ip->i_ump->um_fstype == UM_UFS1) {
		ufs1fmt = 1;
		bap1 = (int32_t *)bp->b_data;
	} else {
		ufs1fmt = 0;
		bap2 = (int64_t *)bp->b_data;
	}
	nblocks = btodb(fs->fs_bsize);
	for (i = NINDIR(fs) - 1; i >= 0; i--) {
		if (ufs1fmt)
			nb = bap1[i];
		else
			nb = bap2[i];
		if (nb == 0)
			continue;
		if (level != 0) {
			if ((error = indir_trunc(ip, fsbtodb(fs, nb),
			     level - 1, lbn + (i * lbnadd), countp)) != 0)
				allerror = error;
		}
		ffs_blkfree(ip, nb, fs->fs_bsize);
		*countp += nblocks;
	}
	bp->b_flags |= B_INVAL | B_NOCACHE;
	brelse(bp);
	return (allerror);
}

/*
 * Free an allocindir.
 * This routine must be called with splbio interrupts blocked.
 */
STATIC void
free_allocindir(struct allocindir *aip, struct inodedep *inodedep)
{
	struct freefrag *freefrag;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_allocindir: lock not held");
#endif
	if ((aip->ai_state & DEPCOMPLETE) == 0)
		LIST_REMOVE(aip, ai_deps);
	if (aip->ai_state & ONWORKLIST)
		WORKLIST_REMOVE(&aip->ai_list);
	LIST_REMOVE(aip, ai_next);
	if ((freefrag = aip->ai_freefrag) != NULL) {
		if (inodedep == NULL)
			add_to_worklist(&freefrag->ff_list);
		else
			WORKLIST_INSERT(&inodedep->id_bufwait,
			    &freefrag->ff_list);
	}
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Directory entry addition dependencies.
 * 
 * When adding a new directory entry, the inode (with its incremented link
 * count) must be written to disk before the directory entry's pointer to it.
 * Also, if the inode is newly allocated, the corresponding freemap must be
 * updated (on disk) before the directory entry's pointer. These requirements
 * are met via undo/redo on the directory entry's pointer, which consists
 * simply of the inode number.
 * 
 * As directory entries are added and deleted, the free space within a
 * directory block can become fragmented.  The ufs file system will compact
 * a fragmented directory block to make space for a new entry. When this
 * occurs, the offsets of previously added entries change. Any "diradd"
 * dependency structures corresponding to these entries must be updated with
 * the new offsets.
 */

/*
 * This routine is called after the in-memory inode's link
 * count has been incremented, but before the directory entry's
 * pointer to the inode has been set.
 */
/* buffer containing directory block */
/* inode for directory */
/* offset of new entry in directory */
/* inode referenced by new directory entry */
/* non-NULL => contents of new mkdir */
/* entry is in a newly allocated block */
int 
softdep_setup_directory_add(struct buf *bp, struct inode *dp, off_t diroffset,
    long newinum, struct buf *newdirbp, int isnewblk)
{
	int offset;		/* offset of new entry within directory block */
	daddr_t lbn;		/* block in directory containing new entry */
	struct fs *fs;
	struct diradd *dap;
	struct allocdirect *adp;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct newdirblk *newdirblk = NULL;
	struct mkdir *mkdir1, *mkdir2;
	

	fs = dp->i_fs;
	lbn = lblkno(fs, diroffset);
	offset = blkoff(fs, diroffset);
	dap = pool_get(&diradd_pool, PR_WAITOK | PR_ZERO);
	dap->da_list.wk_type = D_DIRADD;
	dap->da_offset = offset;
	dap->da_newinum = newinum;
	dap->da_state = ATTACHED;
	if (isnewblk && lbn < NDADDR && fragoff(fs, diroffset) == 0) {
		newdirblk = pool_get(&newdirblk_pool, PR_WAITOK);
		newdirblk->db_list.wk_type = D_NEWDIRBLK;
		newdirblk->db_state = 0;
	}
	if (newdirbp == NULL) {
		dap->da_state |= DEPCOMPLETE;
		ACQUIRE_LOCK(&lk);
	} else {
		dap->da_state |= MKDIR_BODY | MKDIR_PARENT;
		mkdir1 = pool_get(&mkdir_pool, PR_WAITOK);
		mkdir1->md_list.wk_type = D_MKDIR;
		mkdir1->md_state = MKDIR_BODY;
		mkdir1->md_diradd = dap;
		mkdir2 = pool_get(&mkdir_pool, PR_WAITOK);
		mkdir2->md_list.wk_type = D_MKDIR;
		mkdir2->md_state = MKDIR_PARENT;
		mkdir2->md_diradd = dap;
		/*
		 * Dependency on "." and ".." being written to disk.
		 */
		mkdir1->md_buf = newdirbp;
		ACQUIRE_LOCK(&lk);
		LIST_INSERT_HEAD(&mkdirlisthd, mkdir1, md_mkdirs);
		WORKLIST_INSERT(&newdirbp->b_dep, &mkdir1->md_list);
		FREE_LOCK(&lk);
		bdwrite(newdirbp);
		/*
		 * Dependency on link count increase for parent directory
		 */
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(fs, dp->i_number, 0, &inodedep) == 0
		    || (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
			dap->da_state &= ~MKDIR_PARENT;
			WORKITEM_FREE(mkdir2, D_MKDIR);
		} else {
			LIST_INSERT_HEAD(&mkdirlisthd, mkdir2, md_mkdirs);
			WORKLIST_INSERT(&inodedep->id_bufwait,&mkdir2->md_list);
		}
	}
	/*
	 * Link into parent directory pagedep to await its being written.
	 */
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dap->da_pagedep = pagedep;
	LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)], dap,
	    da_pdlist);
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	(void) inodedep_lookup(fs, newinum, DEPALLOC, &inodedep);
	if ((inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE)
		diradd_inode_written(dap, inodedep);
	else
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	if (isnewblk) {
		/*
		 * Directories growing into indirect blocks are rare
		 * enough and the frequency of new block allocation
		 * in those cases even more rare, that we choose not
		 * to bother tracking them. Rather we simply force the
		 * new directory entry to disk.
		 */
		if (lbn >= NDADDR) {
			FREE_LOCK(&lk);
			/*
			 * We only have a new allocation when at the
			 * beginning of a new block, not when we are
			 * expanding into an existing block.
			 */
			if (blkoff(fs, diroffset) == 0)
				return (1);
			return (0);
		}
		/*
		 * We only have a new allocation when at the beginning
		 * of a new fragment, not when we are expanding into an
		 * existing fragment. Also, there is nothing to do if we
		 * are already tracking this block.
		 */
		if (fragoff(fs, diroffset) != 0) {
			FREE_LOCK(&lk);
			return (0);
		}
			
		if ((pagedep->pd_state & NEWBLOCK) != 0) {
			WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
			FREE_LOCK(&lk);
			return (0);
		}
		/*
		 * Find our associated allocdirect and have it track us.
		 */
		if (inodedep_lookup(fs, dp->i_number, 0, &inodedep) == 0)
			panic("softdep_setup_directory_add: lost inodedep");
		adp = TAILQ_LAST(&inodedep->id_newinoupdt, allocdirectlst);
		if (adp == NULL || adp->ad_lbn != lbn) {
			FREE_LOCK(&lk);
			panic("softdep_setup_directory_add: lost entry");
		}
		pagedep->pd_state |= NEWBLOCK;
		newdirblk->db_pagedep = pagedep;
		WORKLIST_INSERT(&adp->ad_newdirblk, &newdirblk->db_list);
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * This procedure is called to change the offset of a directory
 * entry when compacting a directory block which must be owned
 * exclusively by the caller. Note that the actual entry movement
 * must be done in this procedure to ensure that no I/O completions
 * occur while the move is in progress.
 */
/* inode for directory */
/* address of dp->i_offset */
/* address of old directory location */
/* address of new directory location */
/* size of directory entry */
void 
softdep_change_directoryentry_offset(struct inode *dp, caddr_t base,
    caddr_t oldloc, caddr_t newloc, int entrysize)
{
	int offset, oldoffset, newoffset;
	struct pagedep *pagedep;
	struct diradd *dap;
	daddr_t lbn;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, 0, &pagedep) == 0)
		goto done;
	oldoffset = offset + (oldloc - base);
	newoffset = offset + (newloc - base);

	LIST_FOREACH(dap, &pagedep->pd_diraddhd[DIRADDHASH(oldoffset)], da_pdlist) {
		if (dap->da_offset != oldoffset)
			continue;
		dap->da_offset = newoffset;
		if (DIRADDHASH(newoffset) == DIRADDHASH(oldoffset))
			break;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(newoffset)],
		    dap, da_pdlist);
		break;
	}
	if (dap == NULL) {

		LIST_FOREACH(dap, &pagedep->pd_pendinghd, da_pdlist) {
			if (dap->da_offset == oldoffset) {
				dap->da_offset = newoffset;
				break;
			}
		}
	}
done:
	memmove(newloc, oldloc, entrysize);
	FREE_LOCK(&lk);
}

/*
 * Free a diradd dependency structure. This routine must be called
 * with splbio interrupts blocked.
 */
STATIC void
free_diradd(struct diradd *dap)
{
	struct dirrem *dirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mkdir *mkdir, *nextmd;

	splassert(IPL_BIO);

#ifdef DEBUG
	if (lk.lkt_held == -1)
		panic("free_diradd: lock not held");
#endif
	WORKLIST_REMOVE(&dap->da_list);
	LIST_REMOVE(dap, da_pdlist);
	if ((dap->da_state & DIRCHG) == 0) {
		pagedep = dap->da_pagedep;
	} else {
		dirrem = dap->da_previous;
		pagedep = dirrem->dm_pagedep;
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	if (inodedep_lookup(VFSTOUFS(pagedep->pd_mnt)->um_fs, dap->da_newinum,
	    0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir; mkdir = nextmd) {
			nextmd = LIST_NEXT(mkdir, md_mkdirs);
			if (mkdir->md_diradd != dap)
				continue;
			dap->da_state &= ~mkdir->md_state;
			WORKLIST_REMOVE(&mkdir->md_list);
			LIST_REMOVE(mkdir, md_mkdirs);
			WORKITEM_FREE(mkdir, D_MKDIR);
		}
		if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
			FREE_LOCK(&lk);
			panic("free_diradd: unfound ref");
		}
	}
	WORKITEM_FREE(dap, D_DIRADD);
}

/*
 * Directory entry removal dependencies.
 * 
 * When removing a directory entry, the entry's inode pointer must be
 * zero'ed on disk before the corresponding inode's link count is decremented
 * (possibly freeing the inode for re-use). This dependency is handled by
 * updating the directory entry but delaying the inode count reduction until
 * after the directory block has been written to disk. After this point, the
 * inode count can be decremented whenever it is convenient.
 */

/*
 * This routine should be called immediately after removing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will do this task when it is safe.
 */
/* buffer containing directory block */
/* inode for the directory being modified */
/* inode for directory entry being removed */
/* indicates if doing RMDIR */
void 
softdep_setup_remove(struct buf *bp, struct inode *dp, struct inode *ip,
    int isrmdir)
{
	struct dirrem *dirrem, *prevdirrem;

	/*
	 * Allocate a new dirrem if appropriate and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to a zeroed entry until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set then we have deleted an entry that never made it to
	 * disk. If the entry we deleted resulted from a name change,
	 * then the old name still resides on disk. We cannot delete
	 * its inode (returned to us in prevdirrem) until the zeroed
	 * directory entry gets to disk. The new inode has never been
	 * referenced on the disk, so can be deleted immediately.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd, dirrem,
		    dm_next);
		FREE_LOCK(&lk);
	} else {
		if (prevdirrem != NULL)
			LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd,
			    prevdirrem, dm_next);
		dirrem->dm_dirinum = dirrem->dm_pagedep->pd_ino;
		FREE_LOCK(&lk);
		handle_workitem_remove(dirrem);
	}
}

STATIC long num_dirrem;		/* number of dirrem allocated */
/*
 * Allocate a new dirrem if appropriate and return it along with
 * its associated pagedep. Called without a lock, returns with lock.
 */
/* buffer containing directory block */
/* inode for the directory being modified */
/* inode for directory entry being removed */
/* indicates if doing RMDIR */
/* previously referenced inode, if any */
STATIC struct dirrem *
newdirrem(struct buf *bp, struct inode *dp, struct inode *ip, int isrmdir,
    struct dirrem **prevdirremp)
{
	int offset;
	daddr_t lbn;
	struct diradd *dap;
	struct dirrem *dirrem;
	struct pagedep *pagedep;

	/*
	 * Whiteouts have no deletion dependencies.
	 */
	if (ip == NULL)
		panic("newdirrem: whiteout");
	/*
	 * If we are over our limit, try to improve the situation.
	 * Limiting the number of dirrem structures will also limit
	 * the number of freefile and freeblks structures.
	 */
	if (num_dirrem > max_softdeps / 2)
		(void) request_cleanup(FLUSH_REMOVE, 0);
	num_dirrem += 1;
	dirrem = pool_get(&dirrem_pool, PR_WAITOK | PR_ZERO);
	dirrem->dm_list.wk_type = D_DIRREM;
	dirrem->dm_state = isrmdir ? RMDIR : 0;
	dirrem->dm_mnt = ITOV(ip)->v_mount;
	dirrem->dm_oldinum = ip->i_number;
	*prevdirremp = NULL;

	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(dp, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dirrem->dm_pagedep = pagedep;
	/*
	 * Check for a diradd dependency for the same directory entry.
	 * If present, then both dependencies become obsolete and can
	 * be de-allocated. Check for an entry on both the pd_dirraddhd
	 * list and the pd_pendinghd list.
	 */

	LIST_FOREACH(dap, &pagedep->pd_diraddhd[DIRADDHASH(offset)], da_pdlist)
		if (dap->da_offset == offset)
			break;
	if (dap == NULL) {

		LIST_FOREACH(dap, &pagedep->pd_pendinghd, da_pdlist)
			if (dap->da_offset == offset)
				break;
		if (dap == NULL)
			return (dirrem);
	}
	/*
	 * Must be ATTACHED at this point.
	 */
	if ((dap->da_state & ATTACHED) == 0) {
		FREE_LOCK(&lk);
		panic("newdirrem: not ATTACHED");
	}
	if (dap->da_newinum != ip->i_number) {
		FREE_LOCK(&lk);
		panic("newdirrem: inum %u should be %u",
		    ip->i_number, dap->da_newinum);
	}
	/*
	 * If we are deleting a changed name that never made it to disk,
	 * then return the dirrem describing the previous inode (which
	 * represents the inode currently referenced from this entry on disk).
	 */
	if ((dap->da_state & DIRCHG) != 0) {
		*prevdirremp = dap->da_previous;
		dap->da_state &= ~DIRCHG;
		dap->da_pagedep = pagedep;
	}
	/*
	 * We are deleting an entry that never made it to disk.
	 * Mark it COMPLETE so we can delete its inode immediately.
	 */
	dirrem->dm_state |= COMPLETE;
	free_diradd(dap);
	return (dirrem);
}

/*
 * Directory entry change dependencies.
 * 
 * Changing an existing directory entry requires that an add operation
 * be completed first followed by a deletion. The semantics for the addition
 * are identical to the description of adding a new entry above except
 * that the rollback is to the old inode number rather than zero. Once
 * the addition dependency is completed, the removal is done as described
 * in the removal routine above.
 */

/*
 * This routine should be called immediately after changing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will perform this task when it is safe.
 */
/* buffer containing directory block */
/* inode for the directory being modified */
/* inode for directory entry being removed */
/* new inode number for changed entry */
/* indicates if doing RMDIR */
void 
softdep_setup_directory_change(struct buf *bp, struct inode *dp,
    struct inode *ip, long newinum, int isrmdir)
{
	int offset;
	struct diradd *dap;
	struct dirrem *dirrem, *prevdirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;

	offset = blkoff(dp->i_fs, dp->i_offset);
	dap = pool_get(&diradd_pool, PR_WAITOK | PR_ZERO);
	dap->da_list.wk_type = D_DIRADD;
	dap->da_state = DIRCHG | ATTACHED | DEPCOMPLETE;
	dap->da_offset = offset;
	dap->da_newinum = newinum;

	/*
	 * Allocate a new dirrem and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);
	pagedep = dirrem->dm_pagedep;
	/*
	 * The possible values for isrmdir:
	 *	0 - non-directory file rename
	 *	1 - directory rename within same directory
	 *   inum - directory rename to new directory of given inode number
	 * When renaming to a new directory, we are both deleting and
	 * creating a new directory entry, so the link count on the new
	 * directory should not change. Thus we do not need the followup
	 * dirrem which is usually done in handle_workitem_remove. We set
	 * the DIRCHG flag to tell handle_workitem_remove to skip the 
	 * followup dirrem.
	 */
	if (isrmdir > 1)
		dirrem->dm_state |= DIRCHG;

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to the previous inode until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set, then we have deleted an entry that never made it to disk.
	 * If the entry we deleted resulted from a name change, then the old
	 * inode reference still resides on disk. Any rollback that we do
	 * needs to be to that old inode (returned to us in prevdirrem). If
	 * the entry we deleted resulted from a create, then there is
	 * no entry on the disk, so we want to roll back to zero rather
	 * than the uncommitted inode. In either of the COMPLETE cases we
	 * want to immediately free the unwritten and unreferenced inode.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		dap->da_previous = dirrem;
	} else {
		if (prevdirrem != NULL) {
			dap->da_previous = prevdirrem;
		} else {
			dap->da_state &= ~DIRCHG;
			dap->da_pagedep = pagedep;
		}
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Link into its inodedep. Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	if (inodedep_lookup(dp->i_fs, newinum, DEPALLOC, &inodedep) == 0 ||
	    (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
		dap->da_state |= COMPLETE;
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
	} else {
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)],
		    dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	}
	FREE_LOCK(&lk);
}

/*
 * Called whenever the link count on an inode is changed.
 * It creates an inode dependency so that the new reference(s)
 * to the inode cannot be committed to disk until the updated
 * inode has been written.
 */
/* the inode with the increased link count */
/* do background work or not */
void
softdep_change_linkcnt(struct inode *ip, int nodelay)
{
	struct inodedep *inodedep;
	int flags;

	/*
	 * If requested, do not allow background work to happen.
	 */
	flags = DEPALLOC;
	if (nodelay)
		flags |= NODELAY;

	ACQUIRE_LOCK(&lk);

	(void) inodedep_lookup(ip->i_fs, ip->i_number, flags, &inodedep);
	if (DIP(ip, nlink) < ip->i_effnlink) {
		FREE_LOCK(&lk);
		panic("softdep_change_linkcnt: bad delta");
	}

	inodedep->id_nlinkdelta = DIP(ip, nlink) - ip->i_effnlink;

	FREE_LOCK(&lk);
}

/*
 * This workitem decrements the inode's link count.
 * If the link count reaches zero, the file is removed.
 */
STATIC void 
handle_workitem_remove(struct dirrem *dirrem)
{
	struct proc *p = CURPROC;	/* XXX */
	struct inodedep *inodedep;
	struct vnode *vp;
	struct inode *ip;
	ufsino_t oldinum;
	int error;

	if ((error = VFS_VGET(dirrem->dm_mnt, dirrem->dm_oldinum, &vp)) != 0) {
		softdep_error("handle_workitem_remove: vget", error);
		return;
	}
	ip = VTOI(vp);
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(ip->i_fs, dirrem->dm_oldinum, 0, &inodedep)) 
	    == 0) {
		FREE_LOCK(&lk);
		panic("handle_workitem_remove: lost inodedep");
	}
	/*
	 * Normal file deletion.
	 */
	if ((dirrem->dm_state & RMDIR) == 0) {
		DIP_ADD(ip, nlink, -1);
		ip->i_flag |= IN_CHANGE;
		if (DIP(ip, nlink) < ip->i_effnlink) {
			FREE_LOCK(&lk);
			panic("handle_workitem_remove: bad file delta");
		}
		inodedep->id_nlinkdelta = DIP(ip, nlink) - ip->i_effnlink;
		FREE_LOCK(&lk);
		vput(vp);
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		return;
	}
	/*
	 * Directory deletion. Decrement reference count for both the
	 * just deleted parent directory entry and the reference for ".".
	 * Next truncate the directory to length zero. When the
	 * truncation completes, arrange to have the reference count on
	 * the parent decremented to account for the loss of "..".
	 */
	DIP_ADD(ip, nlink, -2);
	ip->i_flag |= IN_CHANGE;
	if (DIP(ip, nlink) < ip->i_effnlink)
		panic("handle_workitem_remove: bad dir delta");
	inodedep->id_nlinkdelta = DIP(ip, nlink) - ip->i_effnlink;
	FREE_LOCK(&lk);
	if ((error = UFS_TRUNCATE(ip, (off_t)0, 0, p->p_ucred)) != 0)
		softdep_error("handle_workitem_remove: truncate", error);
	/*
	 * Rename a directory to a new parent. Since, we are both deleting
	 * and creating a new directory entry, the link count on the new
	 * directory should not change. Thus we skip the followup dirrem.
	 */
	if (dirrem->dm_state & DIRCHG) {
		vput(vp);
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		return;
	}
	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can remove the file immediately.
	 */
	ACQUIRE_LOCK(&lk);
	dirrem->dm_state = 0;
	oldinum = dirrem->dm_oldinum;
	dirrem->dm_oldinum = dirrem->dm_dirinum;
	if (inodedep_lookup(ip->i_fs, oldinum, 0, &inodedep) == 0 ||
	    check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		vput(vp);
		handle_workitem_remove(dirrem);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &dirrem->dm_list);
	FREE_LOCK(&lk);
	ip->i_flag |= IN_CHANGE;
	UFS_UPDATE(VTOI(vp), 0);
	vput(vp);
}

/*
 * Inode de-allocation dependencies.
 * 
 * When an inode's link count is reduced to zero, it can be de-allocated. We
 * found it convenient to postpone de-allocation until after the inode is
 * written to disk with its new link count (zero).  At this point, all of the
 * on-disk inode's block pointers are nullified and, with careful dependency
 * list ordering, all dependencies related to the inode will be satisfied and
 * the corresponding dependency structures de-allocated.  So, if/when the
 * inode is reused, there will be no mixing of old dependencies with new
 * ones.  This artificial dependency is set up by the block de-allocation
 * procedure above (softdep_setup_freeblocks) and completed by the
 * following procedure.
 */
STATIC void 
handle_workitem_freefile(struct freefile *freefile)
{
	struct fs *fs;
	struct vnode vp;
	struct inode tip;
#ifdef DEBUG
	struct inodedep *idp;
#endif
	int error;

	fs = VFSTOUFS(freefile->fx_mnt)->um_fs;
#ifdef DEBUG
	ACQUIRE_LOCK(&lk);
	error = inodedep_lookup(fs, freefile->fx_oldinum, 0, &idp);
	FREE_LOCK(&lk);
	if (error)
		panic("handle_workitem_freefile: inodedep survived");
#endif
	tip.i_ump = VFSTOUFS(freefile->fx_mnt);
	tip.i_dev = freefile->fx_devvp->v_rdev;
	tip.i_fs = fs;
	tip.i_vnode = &vp;
	vp.v_data = &tip;

	if ((error = ffs_freefile(&tip, freefile->fx_oldinum, 
		 freefile->fx_mode)) != 0) {
		softdep_error("handle_workitem_freefile", error);
	}
	WORKITEM_FREE(freefile, D_FREEFILE);
}

/*
 * Disk writes.
 * 
 * The dependency structures constructed above are most actively used when file
 * system blocks are written to disk.  No constraints are placed on when a
 * block can be written, but unsatisfied update dependencies are made safe by
 * modifying (or replacing) the source memory for the duration of the disk
 * write.  When the disk write completes, the memory block is again brought
 * up-to-date.
 *
 * In-core inode structure reclamation.
 * 
 * Because there are a finite number of "in-core" inode structures, they are
 * reused regularly.  By transferring all inode-related dependencies to the
 * in-memory inode block and indexing them separately (via "inodedep"s), we
 * can allow "in-core" inode structures to be reused at any time and avoid
 * any increase in contention.
 *
 * Called just before entering the device driver to initiate a new disk I/O.
 * The buffer must be locked, thus, no I/O completion operations can occur
 * while we are manipulating its associated dependencies.
 */
/* structure describing disk write to occur */
void 
softdep_disk_io_initiation(struct buf *bp)
{
	struct worklist *wk, *nextwk;
	struct indirdep *indirdep;
	struct inodedep *inodedep;
	struct buf *sbp;

	/*
	 * We only care about write operations. There should never
	 * be dependencies for reads.
	 */
	if (bp->b_flags & B_READ)
		panic("softdep_disk_io_initiation: read");

	ACQUIRE_LOCK(&lk);

	/*
	 * Do any necessary pre-I/O processing.
	 */
	for (wk = LIST_FIRST(&bp->b_dep); wk; wk = nextwk) {
		nextwk = LIST_NEXT(wk, wk_list);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			initiate_write_filepage(WK_PAGEDEP(wk), bp);
			continue;

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC)
				initiate_write_inodeblock_ufs1(inodedep, bp);
#ifdef FFS2
			else
				initiate_write_inodeblock_ufs2(inodedep, bp);
#endif
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_io_initiation: indirdep gone");
			/*
			 * If there are no remaining dependencies, this
			 * will be writing the real pointers, so the
			 * dependency can be freed.
			 */
			if (LIST_FIRST(&indirdep->ir_deplisthd) == NULL) {
				sbp = indirdep->ir_savebp;
				sbp->b_flags |= B_INVAL | B_NOCACHE;
				/* inline expand WORKLIST_REMOVE(wk); */
				wk->wk_state &= ~ONWORKLIST;
				LIST_REMOVE(wk, wk_list);
				WORKITEM_FREE(indirdep, D_INDIRDEP);
				FREE_LOCK(&lk);
				brelse(sbp);
				ACQUIRE_LOCK(&lk);
				continue;
			}
			/*
			 * Replace up-to-date version with safe version.
			 */
			FREE_LOCK(&lk);
			indirdep->ir_saveddata = malloc(bp->b_bcount,
			    M_INDIRDEP, M_WAITOK);
			ACQUIRE_LOCK(&lk);
			indirdep->ir_state &= ~ATTACHED;
			indirdep->ir_state |= UNDONE;
			memcpy(indirdep->ir_saveddata, bp->b_data, bp->b_bcount);
			memcpy(bp->b_data, indirdep->ir_savebp->b_data,
			    bp->b_bcount);
			continue;

		case D_MKDIR:
		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			continue;

		default:
			FREE_LOCK(&lk);
			panic("handle_disk_io_initiation: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}

	FREE_LOCK(&lk);
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in a directory. The buffer must be locked,
 * thus, no I/O completion operations can occur while we are
 * manipulating its associated dependencies.
 */
STATIC void
initiate_write_filepage(struct pagedep *pagedep, struct buf *bp)
{
	struct diradd *dap;
	struct direct *ep;
	int i;

	if (pagedep->pd_state & IOSTARTED) {
		/*
		 * This can only happen if there is a driver that does not
		 * understand chaining. Here biodone will reissue the call
		 * to strategy for the incomplete buffers.
		 */
		printf("initiate_write_filepage: already started\n");
		return;
	}
	pagedep->pd_state |= IOSTARTED;
	for (i = 0; i < DAHASHSZ; i++) {
		LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			if (ep->d_ino != dap->da_newinum) {
				FREE_LOCK(&lk);
				panic("%s: dir inum %u != new %u",
				    "initiate_write_filepage",
				    ep->d_ino, dap->da_newinum);
			}
			if (dap->da_state & DIRCHG)
				ep->d_ino = dap->da_previous->dm_oldinum;
			else
				ep->d_ino = 0;
			dap->da_state &= ~ATTACHED;
			dap->da_state |= UNDONE;
		}
	}
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
/* The inode block */
STATIC void 
initiate_write_inodeblock_ufs1(struct inodedep *inodedep, struct buf *bp)
{
	struct allocdirect *adp, *lastadp;
	struct ufs1_dinode *dp;
	struct fs *fs;
#ifdef DIAGNOSTIC
	daddr_t prevlbn = 0;
	int32_t d1, d2;
#endif
	int i, deplist;

	if (inodedep->id_state & IOSTARTED) {
		FREE_LOCK(&lk);
		panic("initiate_write_inodeblock: already started");
	}
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs1_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino1 != NULL) {
			FREE_LOCK(&lk);
			panic("initiate_write_inodeblock: already doing I/O");
		}
		FREE_LOCK(&lk);
		inodedep->id_savedino1 = malloc(sizeof(struct ufs1_dinode),
		    M_INODEDEP, M_WAITOK);
		ACQUIRE_LOCK(&lk);
		*inodedep->id_savedino1 = *dp;
		memset(dp, 0, sizeof(struct ufs1_dinode));
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	if (TAILQ_FIRST(&inodedep->id_inoupdt) == NULL)
		return;
	/*
	 * Set the dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: lbn order");
		}
		prevlbn = adp->ad_lbn;
		if (adp->ad_lbn < NDADDR &&
		    (d1 = dp->di_db[adp->ad_lbn]) != (d2 = adp->ad_newblkno)) {
			FREE_LOCK(&lk);
			panic("%s: direct pointer #%lld mismatch %d != %d",
			    "softdep_write_inodeblock", (long long)adp->ad_lbn,
			    d1, d2);
		}
		if (adp->ad_lbn >= NDADDR &&
		    (d1 = dp->di_ib[adp->ad_lbn - NDADDR]) !=
		    (d2 = adp->ad_newblkno)) {
			FREE_LOCK(&lk);
			panic("%s: indirect pointer #%lld mismatch %d != %d",
			    "softdep_write_inodeblock", (long long)(adp->ad_lbn -
			    NDADDR), d1, d2);
		}
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
		}
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_lbn >= NDADDR)
			break;
		dp->di_db[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NDADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0) {
				FREE_LOCK(&lk);
				panic("softdep_write_inodeblock: lost dep1");
			}
#endif /* DIAGNOSTIC */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0) {
				FREE_LOCK(&lk);
				panic("softdep_write_inodeblock: lost dep2");
			}
#endif /* DIAGNOSTIC */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_lbn - NDADDR] = 0;
}

#ifdef FFS2
/*
 * Version of initiate_write_inodeblock that handles FFS2 dinodes.
 */
/* The inode block */
STATIC void
initiate_write_inodeblock_ufs2(struct inodedep *inodedep, struct buf *bp)
{
	struct allocdirect *adp, *lastadp;
	struct ufs2_dinode *dp;
	struct fs *fs = inodedep->id_fs;
#ifdef DIAGNOSTIC
	daddr_t prevlbn = -1, d1, d2;
#endif
	int deplist, i;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock_ufs2: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs2_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino2 != NULL)
			panic("initiate_write_inodeblock_ufs2: I/O underway");
		inodedep->id_savedino2 = malloc(sizeof(struct ufs2_dinode),
		    M_INODEDEP, M_WAITOK);
		*inodedep->id_savedino2 = *dp;
		memset(dp, 0, sizeof(struct ufs2_dinode));
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	if (TAILQ_FIRST(&inodedep->id_inoupdt) == NULL)
		return;

#ifdef notyet
	inodedep->id_savedextsize = dp->di_extsize;
	if (TAILQ_FIRST(&inodedep->id_inoupdt) == NULL &&
	    TAILQ_FIRST(&inodedep->id_extupdt) == NULL)
		return;
	/*
	 * Set the ext data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: lbn order");
		}
		prevlbn = adp->ad_lbn;
		if ((d1 = dp->di_extb[adp->ad_lbn]) !=
		    (d2 = adp->ad_newblkno)) {
			FREE_LOCK(&lk);
			panic("%s: direct pointer #%lld mismatch %lld != %lld",
			    "softdep_write_inodeblock", (long long)adp->ad_lbn,
			    d1, d2);
		}
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
		}
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the ext
	 * data which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		dp->di_extb[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_extsize = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NXADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_extb[i] != 0 && (deplist & (1 << i)) == 0) {
				FREE_LOCK(&lk);
				panic("softdep_write_inodeblock: lost dep1");
			}
#endif /* DIAGNOSTIC */
			dp->di_extb[i] = 0;
		}
		lastadp = NULL;
		break;
	}
	/*
	 * If we have zero'ed out the last allocated block of the ext
	 * data, roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_extsize <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_extb[i] != 0)
				break;
		dp->di_extsize = (i + 1) * fs->fs_bsize;
	}
#endif /* notyet */

	/*
	 * Set the file data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef DIAGNOSTIC
		if (deplist != 0 && prevlbn >= adp->ad_lbn) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: lbn order");
		}
		prevlbn = adp->ad_lbn;
		if (adp->ad_lbn < NDADDR &&
		    (d1 = dp->di_db[adp->ad_lbn]) != (d2 = adp->ad_newblkno)) {
			FREE_LOCK(&lk);
			panic("%s: direct pointer #%lld mismatch %lld != %lld",
			    "softdep_write_inodeblock", (long long)adp->ad_lbn,
			    d1, d2);
		}
		if (adp->ad_lbn >= NDADDR &&
		    (d1 = dp->di_ib[adp->ad_lbn - NDADDR]) !=
		    (d2 = adp->ad_newblkno)) {
			FREE_LOCK(&lk);
			panic("%s: indirect pointer #%lld mismatch %lld != %lld",
			    "softdep_write_inodeblock", (long long)(adp->ad_lbn -
			    NDADDR), d1, d2);
		}
		deplist |= 1 << adp->ad_lbn;
		if ((adp->ad_state & ATTACHED) == 0) {
			FREE_LOCK(&lk);
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
		}
#endif /* DIAGNOSTIC */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_lbn >= NDADDR)
			break;
		dp->di_db[adp->ad_lbn] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_lbn + adp->ad_oldsize;
		for (i = adp->ad_lbn + 1; i < NDADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0) {
				FREE_LOCK(&lk);
				panic("softdep_write_inodeblock: lost dep2");
			}
#endif /* DIAGNOSTIC */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef DIAGNOSTIC
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0) {
				FREE_LOCK(&lk);
				panic("softdep_write_inodeblock: lost dep3");
			}
#endif /* DIAGNOSTIC */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_lbn + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_lbn; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_lbn - NDADDR] = 0;
}
#endif /* FFS2 */

/*
 * This routine is called during the completion interrupt
 * service routine for a disk write (from the procedure called
 * by the device driver to inform the file system caches of
 * a request completion).  It should be called early in this
 * procedure, before the block is made available to other
 * processes or other routines are called.
 */
/* describes the completed disk write */
void 
softdep_disk_write_complete(struct buf *bp)
{
	struct worklist *wk;
	struct workhead reattach;
	struct newblk *newblk;
	struct allocindir *aip;
	struct allocdirect *adp;
	struct indirdep *indirdep;
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;

	/*
	 * If an error occurred while doing the write, then the data
	 * has not hit the disk and the dependencies cannot be unrolled.
	 */
	if ((bp->b_flags & B_ERROR) && !(bp->b_flags & B_INVAL))
		return;

#ifdef DEBUG
	if (lk.lkt_held != -1)
		panic("softdep_disk_write_complete: lock is held");
	lk.lkt_held = -2;
#endif
	LIST_INIT(&reattach);
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			if (handle_written_filepage(WK_PAGEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_INODEDEP:
			if (handle_written_inodeblock(WK_INODEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_BMSAFEMAP:
			bmsafemap = WK_BMSAFEMAP(wk);
			while ((newblk = LIST_FIRST(&bmsafemap->sm_newblkhd))) {
				newblk->nb_state |= DEPCOMPLETE;
				newblk->nb_bmsafemap = NULL;
				LIST_REMOVE(newblk, nb_deps);
			}
			while ((adp =
			   LIST_FIRST(&bmsafemap->sm_allocdirecthd))) {
				adp->ad_state |= DEPCOMPLETE;
				adp->ad_buf = NULL;
				LIST_REMOVE(adp, ad_deps);
				handle_allocdirect_partdone(adp);
			}
			while ((aip =
			    LIST_FIRST(&bmsafemap->sm_allocindirhd))) {
				aip->ai_state |= DEPCOMPLETE;
				aip->ai_buf = NULL;
				LIST_REMOVE(aip, ai_deps);
				handle_allocindir_partdone(aip);
			}
			while ((inodedep =
			     LIST_FIRST(&bmsafemap->sm_inodedephd)) != NULL) {
				inodedep->id_state |= DEPCOMPLETE;
				LIST_REMOVE(inodedep, id_deps);
				inodedep->id_buf = NULL;
			}
			WORKITEM_FREE(bmsafemap, D_BMSAFEMAP);
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_BODY);
			continue;

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			adp->ad_state |= COMPLETE;
			handle_allocdirect_partdone(adp);
			continue;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			aip->ai_state |= COMPLETE;
			handle_allocindir_partdone(aip);
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (indirdep->ir_state & GOINGAWAY)
				panic("disk_write_complete: indirdep gone");
			memcpy(bp->b_data, indirdep->ir_saveddata, bp->b_bcount);
			free(indirdep->ir_saveddata, M_INDIRDEP, 0);
			indirdep->ir_saveddata = NULL;
			indirdep->ir_state &= ~UNDONE;
			indirdep->ir_state |= ATTACHED;
			while ((aip = LIST_FIRST(&indirdep->ir_donehd))) {
				handle_allocindir_partdone(aip);
				if (aip == LIST_FIRST(&indirdep->ir_donehd))
					panic("disk_write_complete: not gone");
			}
			WORKLIST_INSERT(&reattach, wk);
			if ((bp->b_flags & B_DELWRI) == 0)
				stat_indir_blk_ptrs++;
			buf_dirty(bp);
			continue;

		default:
			panic("handle_disk_write_complete: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	/*
	 * Reattach any requests that must be redone.
	 */
	while ((wk = LIST_FIRST(&reattach)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&bp->b_dep, wk);
	}
#ifdef DEBUG
	if (lk.lkt_held != -2)
		panic("softdep_disk_write_complete: lock lost");
	lk.lkt_held = -1;
#endif
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
/* the completed allocdirect */
STATIC void 
handle_allocdirect_partdone(struct allocdirect *adp)
{
	struct allocdirect *listadp;
	struct inodedep *inodedep;
	long bsize, delay;

	splassert(IPL_BIO);

	if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (adp->ad_buf != NULL)
		panic("handle_allocdirect_partdone: dangling dep");

	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem. Thus, we cannot free any
	 * allocdirects after one whose ad_oldblkno claims a fragment as
	 * these blocks must be rolled back to zero before writing the inode.
	 * We check the currently active set of allocdirects in id_inoupdt.
	 */
	inodedep = adp->ad_inodedep;
	bsize = inodedep->id_fs->fs_bsize;
	TAILQ_FOREACH(listadp, &inodedep->id_inoupdt, ad_next) {
		/* found our block */
		if (listadp == adp)
			break;
		/* continue if ad_oldlbn is not a fragment */
		if (listadp->ad_oldsize == 0 ||
		    listadp->ad_oldsize == bsize)
			continue;
		/* hit a fragment */
		return;
	}
	/*
	 * If we have reached the end of the current list without
	 * finding the just finished dependency, then it must be
	 * on the future dependency list. Future dependencies cannot
	 * be freed until they are moved to the current list.
	 */
	if (listadp == NULL) {
#ifdef DEBUG
		TAILQ_FOREACH(listadp, &inodedep->id_newinoupdt, ad_next)
			/* found our block */
			if (listadp == adp)
				break;
		if (listadp == NULL)
			panic("handle_allocdirect_partdone: lost dep");
#endif /* DEBUG */
		return;
	}
	/*
	 * If we have found the just finished dependency, then free
	 * it along with anything that follows it that is complete.
	 * If the inode still has a bitmap dependency, then it has
	 * never been written to disk, hence the on-disk inode cannot
	 * reference the old fragment so we can free it without delay.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	for (; adp; adp = listadp) {
		listadp = TAILQ_NEXT(adp, ad_next);
		if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
			return;
		free_allocdirect(&inodedep->id_inoupdt, adp, delay);
	}
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
/* the completed allocindir */
STATIC void
handle_allocindir_partdone(struct allocindir *aip)
{
	struct indirdep *indirdep;

	splassert(IPL_BIO);

	if ((aip->ai_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (aip->ai_buf != NULL)
		panic("handle_allocindir_partdone: dangling dependency");
	indirdep = aip->ai_indirdep;
	if (indirdep->ir_state & UNDONE) {
		LIST_REMOVE(aip, ai_next);
		LIST_INSERT_HEAD(&indirdep->ir_donehd, aip, ai_next);
		return;
	}
	if (indirdep->ir_state & UFS1FMT)
		((int32_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	else
		((int64_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	LIST_REMOVE(aip, ai_next);
	if (aip->ai_freefrag != NULL)
		add_to_worklist(&aip->ai_freefrag->ff_list);
	WORKITEM_FREE(aip, D_ALLOCINDIR);
}

/*
 * Called from within softdep_disk_write_complete above to restore
 * in-memory inode block contents to their most up-to-date state. Note
 * that this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
/* buffer containing the inode block */
STATIC int 
handle_written_inodeblock(struct inodedep *inodedep, struct buf *bp)
{
	struct worklist *wk, *filefree;
	struct allocdirect *adp, *nextadp;
	struct ufs1_dinode *dp1 = NULL;
	struct ufs2_dinode *dp2 = NULL;
	int hadchanges, fstype;

	splassert(IPL_BIO);

	if ((inodedep->id_state & IOSTARTED) == 0)
		panic("handle_written_inodeblock: not started");
	inodedep->id_state &= ~IOSTARTED;

	if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC) {
		fstype = UM_UFS1;
		dp1 = (struct ufs1_dinode *) bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
	} else {
		fstype = UM_UFS2;
		dp2 = (struct ufs2_dinode *) bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
	}

	/*
	 * If we had to rollback the inode allocation because of
	 * bitmaps being incomplete, then simply restore it.
	 * Keep the block dirty so that it will not be reclaimed until
	 * all associated dependencies have been cleared and the
	 * corresponding updates written to disk.
	 */
	if (inodedep->id_savedino1 != NULL) {
		if (fstype == UM_UFS1)
			*dp1 = *inodedep->id_savedino1;
		else
			*dp2 = *inodedep->id_savedino2;
		free(inodedep->id_savedino1, M_INODEDEP, 0);
		inodedep->id_savedino1 = NULL;
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_inode_bitmap++;
		buf_dirty(bp);
		return (1);
	}
	inodedep->id_state |= COMPLETE;
	/*
	 * Roll forward anything that had to be rolled back before 
	 * the inode could be updated.
	 */
	hadchanges = 0;
	for (adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (fstype == UM_UFS1) {
			if (adp->ad_lbn < NDADDR) {
				if (dp1->di_db[adp->ad_lbn] != adp->ad_oldblkno)
					 panic("%s: %s #%lld mismatch %d != "
					     "%lld",
					     "handle_written_inodeblock",
					     "direct pointer",
					     (long long)adp->ad_lbn,
					     dp1->di_db[adp->ad_lbn],
					     (long long)adp->ad_oldblkno);
				dp1->di_db[adp->ad_lbn] = adp->ad_newblkno;
			} else {
				if (dp1->di_ib[adp->ad_lbn - NDADDR] != 0)
					panic("%s: %s #%lld allocated as %d",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (long long)(adp->ad_lbn - NDADDR),
					    dp1->di_ib[adp->ad_lbn - NDADDR]);
				dp1->di_ib[adp->ad_lbn - NDADDR] =
				   adp->ad_newblkno;
			}
		} else {
			if (adp->ad_lbn < NDADDR) {
				if (dp2->di_db[adp->ad_lbn] != adp->ad_oldblkno)
					panic("%s: %s #%lld mismatch %lld != "
					    "%lld", "handle_written_inodeblock",
					    "direct pointer",
					    (long long)adp->ad_lbn,
					    dp2->di_db[adp->ad_lbn],
					    (long long)adp->ad_oldblkno);
				dp2->di_db[adp->ad_lbn] = adp->ad_newblkno;
			} else {
				if (dp2->di_ib[adp->ad_lbn - NDADDR] != 0)
					panic("%s: %s #%lld allocated as %lld",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (long long)(adp->ad_lbn - NDADDR),
					    dp2->di_ib[adp->ad_lbn - NDADDR]);
				dp2->di_ib[adp->ad_lbn - NDADDR] =
				    adp->ad_newblkno;
			}
		}
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	if (hadchanges && (bp->b_flags & B_DELWRI) == 0)
		stat_direct_blk_ptrs++;
	/*
	 * Reset the file size to its most up-to-date value.
	 */
	if (inodedep->id_savedsize == -1)
		panic("handle_written_inodeblock: bad size");
	
	if (fstype == UM_UFS1) {
		if (dp1->di_size != inodedep->id_savedsize) {
			dp1->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
	} else {
		if (dp2->di_size != inodedep->id_savedsize) {
			dp2->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
	}
	inodedep->id_savedsize = -1;
	/*
	 * If there were any rollbacks in the inode block, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (hadchanges)
		buf_dirty(bp);
	/*
	 * Process any allocdirects that completed during the update.
	 */
	if ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != NULL)
		handle_allocdirect_partdone(adp);
	/*
	 * Process deallocations that were held pending until the
	 * inode had been written to disk. Freeing of the inode
	 * is delayed until after all blocks have been freed to
	 * avoid creation of new <vfsid, inum, lbn> triples
	 * before the old ones have been deleted.
	 */
	filefree = NULL;
	while ((wk = LIST_FIRST(&inodedep->id_bufwait)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {

		case D_FREEFILE:
			/*
			 * We defer adding filefree to the worklist until
			 * all other additions have been made to ensure
			 * that it will be done after all the old blocks
			 * have been freed.
			 */
			if (filefree != NULL)
				panic("handle_written_inodeblock: filefree");
			filefree = wk;
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_PARENT);
			continue;

		case D_DIRADD:
			diradd_inode_written(WK_DIRADD(wk), inodedep);
			continue;

		case D_FREEBLKS:
			wk->wk_state |= COMPLETE;
			if ((wk->wk_state & ALLCOMPLETE) != ALLCOMPLETE)
				continue;
			/* FALLTHROUGH */
		case D_FREEFRAG:
		case D_DIRREM:
			add_to_worklist(wk);
			continue;

		case D_NEWDIRBLK:
			free_newdirblk(WK_NEWDIRBLK(wk));
			continue;

		default:
			panic("handle_written_inodeblock: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	if (filefree != NULL) {
		if (free_inodedep(inodedep) == 0)
			panic("handle_written_inodeblock: live inodedep");
		add_to_worklist(filefree);
		return (0);
	}

	/*
	 * If no outstanding dependencies, free it.
	 */
	if (free_inodedep(inodedep) ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) == NULL)
		return (0);
	return (hadchanges);
}

/*
 * Process a diradd entry after its dependent inode has been written.
 * This routine must be called with splbio interrupts blocked.
 */
STATIC void
diradd_inode_written(struct diradd *dap, struct inodedep *inodedep)
{
	struct pagedep *pagedep;

	splassert(IPL_BIO);

	dap->da_state |= COMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
}

/*
 * Handle the completion of a mkdir dependency.
 */
STATIC void
handle_written_mkdir(struct mkdir *mkdir, int type)
{
	struct diradd *dap;
	struct pagedep *pagedep;

	splassert(IPL_BIO);

	if (mkdir->md_state != type)
		panic("handle_written_mkdir: bad type");
	dap = mkdir->md_diradd;
	dap->da_state &= ~type;
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) == 0)
		dap->da_state |= DEPCOMPLETE;
	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
	LIST_REMOVE(mkdir, md_mkdirs);
	WORKITEM_FREE(mkdir, D_MKDIR);
}

/*
 * Called from within softdep_disk_write_complete above.
 * A write operation was just completed. Removed inodes can
 * now be freed and associated block pointers may be committed.
 * Note that this routine is always called from interrupt level
 * with further splbio interrupts blocked.
 */
/* buffer containing the written page */
STATIC int 
handle_written_filepage(struct pagedep *pagedep, struct buf *bp)
{
	struct dirrem *dirrem;
	struct diradd *dap, *nextdap;
	struct direct *ep;
	int i, chgs;

	splassert(IPL_BIO);

	if ((pagedep->pd_state & IOSTARTED) == 0)
		panic("handle_written_filepage: not started");
	pagedep->pd_state &= ~IOSTARTED;
	/*
	 * Process any directory removals that have been committed.
	 */
	while ((dirrem = LIST_FIRST(&pagedep->pd_dirremhd)) != NULL) {
		LIST_REMOVE(dirrem, dm_next);
		dirrem->dm_dirinum = pagedep->pd_ino;
		add_to_worklist(&dirrem->dm_list);
	}
	/*
	 * Free any directory additions that have been committed.
	 * If it is a newly allocated block, we have to wait until
	 * the on-disk directory inode claims the new block.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap);
	/*
	 * Uncommitted directory entries must be restored.
	 */
	for (chgs = 0, i = 0; i < DAHASHSZ; i++) {
		for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]); dap;
		     dap = nextdap) {
			nextdap = LIST_NEXT(dap, da_pdlist);
			if (dap->da_state & ATTACHED)
				panic("handle_written_filepage: attached");
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			ep->d_ino = dap->da_newinum;
			dap->da_state &= ~UNDONE;
			dap->da_state |= ATTACHED;
			chgs = 1;
			/*
			 * If the inode referenced by the directory has
			 * been written out, then the dependency can be
			 * moved to the pending list.
			 */
			if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
				LIST_REMOVE(dap, da_pdlist);
				LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap,
				    da_pdlist);
			}
		}
	}
	/*
	 * If there were any rollbacks in the directory, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (chgs) {
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_dir_entry++;
		buf_dirty(bp);
		return (1);
	}
	/*
	 * If we are not waiting for a new directory block to be
	 * claimed by its inode, then the pagedep will be freed.
	 * Otherwise it will remain to track any new entries on
	 * the page in case they are fsync'ed.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0) {
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	return (0);
}

/*
 * Writing back in-core inode structures.
 * 
 * The file system only accesses an inode's contents when it occupies an
 * "in-core" inode structure.  These "in-core" structures are separate from
 * the page frames used to cache inode blocks.  Only the latter are
 * transferred to/from the disk.  So, when the updated contents of the
 * "in-core" inode structure are copied to the corresponding in-memory inode
 * block, the dependencies are also transferred.  The following procedure is
 * called when copying a dirty "in-core" inode to a cached inode block.
 */

/*
 * Called when an inode is loaded from disk. If the effective link count
 * differed from the actual link count when it was last flushed, then we
 * need to ensure that the correct effective link count is put back.
 */
/* the "in_core" copy of the inode */
void 
softdep_load_inodeblock(struct inode *ip)
{
	struct inodedep *inodedep;

	/*
	 * Check for alternate nlink count.
	 */
	ip->i_effnlink = DIP(ip, nlink);
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return;
	}
	ip->i_effnlink -= inodedep->id_nlinkdelta;
	FREE_LOCK(&lk);
}

/*
 * This routine is called just before the "in-core" inode
 * information is to be copied to the in-memory inode block.
 * Recall that an inode block contains several inodes. If
 * the force flag is set, then the dependencies will be
 * cleared so that the update can always be made. Note that
 * the buffer is locked when this routine is called, so we
 * will never be in the middle of writing the inode block 
 * to disk.
 */
/* the "in_core" copy of the inode */
/* the buffer containing the inode block */
/* nonzero => update must be allowed */
void 
softdep_update_inodeblock(struct inode *ip, struct buf *bp, int waitfor)
{
	struct inodedep *inodedep;
	struct worklist *wk;
	int error, gotit;

	/*
	 * If the effective link count is not equal to the actual link
	 * count, then we must track the difference in an inodedep while
	 * the inode is (potentially) tossed out of the cache. Otherwise,
	 * if there is no existing inodedep, then there are no dependencies
	 * to track.
	 */
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(ip->i_fs, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		if (ip->i_effnlink != DIP(ip, nlink))
			panic("softdep_update_inodeblock: bad link count");
		return;
	}
	if (inodedep->id_nlinkdelta != DIP(ip, nlink) - ip->i_effnlink) {
		FREE_LOCK(&lk);
		panic("softdep_update_inodeblock: bad delta");
	}
	/*
	 * Changes have been initiated. Anything depending on these
	 * changes cannot occur until this inode has been written.
	 */
	inodedep->id_state &= ~COMPLETE;
	if ((inodedep->id_state & ONWORKLIST) == 0)
		WORKLIST_INSERT(&bp->b_dep, &inodedep->id_list);
	/*
	 * Any new dependencies associated with the incore inode must 
	 * now be moved to the list associated with the buffer holding
	 * the in-memory copy of the inode. Once merged process any
	 * allocdirects that are completed by the merger.
	 */
	merge_inode_lists(inodedep);
	if (TAILQ_FIRST(&inodedep->id_inoupdt) != NULL)
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_inoupdt));
	/*
	 * Now that the inode has been pushed into the buffer, the
	 * operations dependent on the inode being written to disk
	 * can be moved to the id_bufwait so that they will be
	 * processed when the buffer I/O completes.
	 */
	while ((wk = LIST_FIRST(&inodedep->id_inowait)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&inodedep->id_bufwait, wk);
	}
	/*
	 * Newly allocated inodes cannot be written until the bitmap
	 * that allocates them have been written (indicated by
	 * DEPCOMPLETE being set in id_state). If we are doing a
	 * forced sync (e.g., an fsync on a file), we force the bitmap
	 * to be written so that the update can be done.
	 */
	do {
		if ((inodedep->id_state & DEPCOMPLETE) != 0 || waitfor == 0) {
			FREE_LOCK(&lk);
			return;
		}
		bp = inodedep->id_buf;
		gotit = getdirtybuf(bp, MNT_WAIT);
	} while (gotit == -1);
	FREE_LOCK(&lk);
	if (gotit && (error = bwrite(bp)) != 0)
		softdep_error("softdep_update_inodeblock: bwrite", error);
	if ((inodedep->id_state & DEPCOMPLETE) == 0)
		panic("softdep_update_inodeblock: update failed");
}

/*
 * Merge the new inode dependency list (id_newinoupdt) into the old
 * inode dependency list (id_inoupdt). This routine must be called
 * with splbio interrupts blocked.
 */
STATIC void
merge_inode_lists(struct inodedep *inodedep)
{
	struct allocdirect *listadp, *newadp;

	splassert(IPL_BIO);

	newadp = TAILQ_FIRST(&inodedep->id_newinoupdt);
	for (listadp = TAILQ_FIRST(&inodedep->id_inoupdt); listadp && newadp;) {
		if (listadp->ad_lbn < newadp->ad_lbn) {
			listadp = TAILQ_NEXT(listadp, ad_next);
			continue;
		}
		TAILQ_REMOVE(&inodedep->id_newinoupdt, newadp, ad_next);
		TAILQ_INSERT_BEFORE(listadp, newadp, ad_next);
		if (listadp->ad_lbn == newadp->ad_lbn) {
			allocdirect_merge(&inodedep->id_inoupdt, newadp,
			    listadp);
			listadp = newadp;
		}
		newadp = TAILQ_FIRST(&inodedep->id_newinoupdt);
	}
	while ((newadp = TAILQ_FIRST(&inodedep->id_newinoupdt)) != NULL) {
		TAILQ_REMOVE(&inodedep->id_newinoupdt, newadp, ad_next);
		TAILQ_INSERT_TAIL(&inodedep->id_inoupdt, newadp, ad_next);
	}
}

/*
 * If we are doing an fsync, then we must ensure that any directory
 * entries for the inode have been written after the inode gets to disk.
 */
/* the "in_core" copy of the inode */
int
softdep_fsync(struct vnode *vp)
{
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct worklist *wk;
	struct diradd *dap;
	struct mount *mnt;
	struct vnode *pvp;
	struct inode *ip;
	struct inode *pip;
	struct buf *bp;
	struct fs *fs;
	struct proc *p = CURPROC;		/* XXX */
	int error, flushparent;
	ufsino_t parentino;
	daddr_t lbn;

	ip = VTOI(vp);
	fs = ip->i_fs;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return (0);
	}
	if (LIST_FIRST(&inodedep->id_inowait) != NULL ||
	    LIST_FIRST(&inodedep->id_bufwait) != NULL ||
	    TAILQ_FIRST(&inodedep->id_inoupdt) != NULL ||
	    TAILQ_FIRST(&inodedep->id_newinoupdt) != NULL) {
		FREE_LOCK(&lk);
		panic("softdep_fsync: pending ops");
	}
	for (error = 0, flushparent = 0; ; ) {
		if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) == NULL)
			break;
		if (wk->wk_type != D_DIRADD) {
			FREE_LOCK(&lk);
			panic("softdep_fsync: Unexpected type %s",
			    TYPENAME(wk->wk_type));
		}
		dap = WK_DIRADD(wk);
		/*
		 * Flush our parent if this directory entry has a MKDIR_PARENT
		 * dependency or is contained in a newly allocated block.
		 */
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		mnt = pagedep->pd_mnt;
		parentino = pagedep->pd_ino;
		lbn = pagedep->pd_lbn;
		if ((dap->da_state & (MKDIR_BODY | COMPLETE)) != COMPLETE) {
			FREE_LOCK(&lk);
			panic("softdep_fsync: dirty");
		}
		if ((dap->da_state & MKDIR_PARENT) ||
		    (pagedep->pd_state & NEWBLOCK))
			flushparent = 1;
		else
			flushparent = 0;
		/*
		 * If we are being fsync'ed as part of vgone'ing this vnode,
		 * then we will not be able to release and recover the
		 * vnode below, so we just have to give up on writing its
		 * directory entry out. It will eventually be written, just
		 * not now, but then the user was not asking to have it
		 * written, so we are not breaking any promises.
		 */
		if (vp->v_flag & VXLOCK)
			break;
		/*
		 * We prevent deadlock by always fetching inodes from the
		 * root, moving down the directory tree. Thus, when fetching
		 * our parent directory, we must unlock ourselves before
		 * requesting the lock on our parent. See the comment in
		 * ufs_lookup for details on possible races.
		 */
		FREE_LOCK(&lk);
		VOP_UNLOCK(vp, p);
		error = VFS_VGET(mnt, parentino, &pvp);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error != 0)
			return (error);
		/*
		 * All MKDIR_PARENT dependencies and all the NEWBLOCK pagedeps
		 * that are contained in direct blocks will be resolved by 
		 * doing a UFS_UPDATE. Pagedeps contained in indirect blocks
		 * may require a complete sync'ing of the directory. So, we
		 * try the cheap and fast UFS_UPDATE first, and if that fails,
		 * then we do the slower VOP_FSYNC of the directory.
		 */
		pip = VTOI(pvp);
		if (flushparent) {
			error = UFS_UPDATE(pip, 1);
			if (error) {
				vput(pvp);
				return (error);
			}
			if (pagedep->pd_state & NEWBLOCK) {
				error = VOP_FSYNC(pvp, p->p_ucred, MNT_WAIT, p);
				if (error) {
					vput(pvp);
					return (error);
				}
			}
		}
		/*
		 * Flush directory page containing the inode's name.
		 */
		error = bread(pvp, lbn, fs->fs_bsize, &bp);
		if (error == 0) {
			bp->b_bcount = blksize(fs, pip, lbn);
			error = bwrite(bp);
		} else
			brelse(bp);
		vput(pvp);
		if (error != 0)
			return (error);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(fs, ip->i_number, 0, &inodedep) == 0)
			break;
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * Flush all the dirty bitmaps associated with the block device
 * before flushing the rest of the dirty blocks so as to reduce
 * the number of dependencies that will have to be rolled back.
 */
void
softdep_fsync_mountdev(struct vnode *vp, int waitfor)
{
	struct buf *bp, *nbp;
	struct worklist *wk;

	if (!vn_isdisk(vp, NULL))
		panic("softdep_fsync_mountdev: vnode not a disk");
	ACQUIRE_LOCK(&lk);
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		/* 
		 * If it is already scheduled, skip to the next buffer.
		 */
		splassert(IPL_BIO);
		if (bp->b_flags & B_BUSY)
			continue;

		if ((bp->b_flags & B_DELWRI) == 0) {
			FREE_LOCK(&lk);
			panic("softdep_fsync_mountdev: not dirty");
		}
		/*
		 * We are only interested in bitmaps with outstanding
		 * dependencies.
		 */
		if ((wk = LIST_FIRST(&bp->b_dep)) == NULL ||
		    wk->wk_type != D_BMSAFEMAP) {
			continue;
		}
		bremfree(bp);
		buf_acquire(bp);
		FREE_LOCK(&lk);
		(void) bawrite(bp);
		ACQUIRE_LOCK(&lk);
		/*
		 * Since we may have slept during the I/O, we need 
		 * to start from a known point.
		 */
		nbp = LIST_FIRST(&vp->v_dirtyblkhd);
	}
	if (waitfor == MNT_WAIT)
		drain_output(vp, 1);
	FREE_LOCK(&lk);
}

/*
 * This routine is called when we are trying to synchronously flush a
 * file. This routine must eliminate any filesystem metadata dependencies
 * so that the syncing routine can succeed by pushing the dirty blocks
 * associated with the file. If any I/O errors occur, they are returned.
 */
int
softdep_sync_metadata(struct vop_fsync_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct pagedep *pagedep;
	struct allocdirect *adp;
	struct allocindir *aip;
	struct buf *bp, *nbp;
	struct worklist *wk;
	int i, gotit, error, waitfor;

	/*
	 * Check whether this vnode is involved in a filesystem
	 * that is doing soft dependency processing.
	 */
	if (!vn_isdisk(vp, NULL)) {
		if (!DOINGSOFTDEP(vp))
			return (0);
	} else
		if (vp->v_specmountpoint == NULL ||
		    (vp->v_specmountpoint->mnt_flag & MNT_SOFTDEP) == 0)
			return (0);
	/*
	 * Ensure that any direct block dependencies have been cleared.
	 */
	ACQUIRE_LOCK(&lk);
	if ((error = flush_inodedep_deps(VTOI(vp)->i_fs, VTOI(vp)->i_number))) {
		FREE_LOCK(&lk);
		return (error);
	}
	/*
	 * For most files, the only metadata dependencies are the
	 * cylinder group maps that allocate their inode or blocks.
	 * The block allocation dependencies can be found by traversing
	 * the dependency lists for any buffers that remain on their
	 * dirty buffer list. The inode allocation dependency will
	 * be resolved when the inode is updated with MNT_WAIT.
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 */
	waitfor = MNT_NOWAIT;
top:
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	drain_output(vp, 1);
	bp = LIST_FIRST(&vp->v_dirtyblkhd);
	gotit = getdirtybuf(bp, MNT_WAIT);
	if (gotit == 0) {
		FREE_LOCK(&lk);
		return (0);
	} else if (gotit == -1)
		goto top;
loop:
	/*
	 * As we hold the buffer locked, none of its dependencies
	 * will disappear.
	 */
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_ALLOCDIRECT:
			adp = WK_ALLOCDIRECT(wk);
			if (adp->ad_state & DEPCOMPLETE)
				break;
			nbp = adp->ad_buf;
			gotit = getdirtybuf(nbp, waitfor);
			if (gotit == 0)
				break;
			else if (gotit == -1)
				goto loop;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = VOP_BWRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			if (aip->ai_state & DEPCOMPLETE)
				break;
			nbp = aip->ai_buf;
			gotit = getdirtybuf(nbp, waitfor);
			if (gotit == 0)
				break;
			else if (gotit == -1)
				goto loop;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = VOP_BWRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_INDIRDEP:
		restart:

			LIST_FOREACH(aip, &WK_INDIRDEP(wk)->ir_deplisthd, ai_next) {
				if (aip->ai_state & DEPCOMPLETE)
					continue;
				nbp = aip->ai_buf;
				if (getdirtybuf(nbp, MNT_WAIT) <= 0)
					goto restart;
				FREE_LOCK(&lk);
				if ((error = VOP_BWRITE(nbp)) != 0) {
					bawrite(bp);
					return (error);
				}
				ACQUIRE_LOCK(&lk);
				goto restart;
			}
			break;

		case D_INODEDEP:
			if ((error = flush_inodedep_deps(WK_INODEDEP(wk)->id_fs,
			    WK_INODEDEP(wk)->id_ino)) != 0) {
				FREE_LOCK(&lk);
				bawrite(bp);
				return (error);
			}
			break;

		case D_PAGEDEP:
			/*
			 * We are trying to sync a directory that may
			 * have dependencies on both its own metadata
			 * and/or dependencies on the inodes of any
			 * recently allocated files. We walk its diradd
			 * lists pushing out the associated inode.
			 */
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {
				if (LIST_FIRST(&pagedep->pd_diraddhd[i]) ==
				    NULL)
					continue;
				if ((error =
				    flush_pagedep_deps(vp, pagedep->pd_mnt,
						&pagedep->pd_diraddhd[i]))) {
					FREE_LOCK(&lk);
					bawrite(bp);
					return (error);
				}
			}
			break;

		case D_MKDIR:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_MKDIR(wk)->md_buf;
			KASSERT(bp != nbp);
			gotit = getdirtybuf(nbp, waitfor);
			if (gotit == 0)
				break;
			else if (gotit == -1)
				goto loop;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = VOP_BWRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		case D_BMSAFEMAP:
			/*
			 * This case should never happen if the vnode has
			 * been properly sync'ed. However, if this function
			 * is used at a place where the vnode has not yet
			 * been sync'ed, this dependency can show up. So,
			 * rather than panic, just flush it.
			 */
			nbp = WK_BMSAFEMAP(wk)->sm_buf;
			if (bp == nbp)
				break;
			gotit = getdirtybuf(nbp, waitfor);
			if (gotit == 0)
				break;
			else if (gotit == -1)
				goto loop;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = VOP_BWRITE(nbp)) != 0) {
				bawrite(bp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;

		default:
			FREE_LOCK(&lk);
			panic("softdep_sync_metadata: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	do {
		nbp = LIST_NEXT(bp, b_vnbufs);
		gotit = getdirtybuf(nbp, MNT_WAIT);
	} while (gotit == -1);
	FREE_LOCK(&lk);
	bawrite(bp);
	ACQUIRE_LOCK(&lk);
	if (nbp != NULL) {
		bp = nbp;
		goto loop;
	}
	/*
	 * The brief unlock is to allow any pent up dependency
	 * processing to be done. Then proceed with the second pass.
	 */
	if (waitfor == MNT_NOWAIT) {
		waitfor = MNT_WAIT;
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
		goto top;
	}

	/*
	 * If we have managed to get rid of all the dirty buffers,
	 * then we are done. For certain directories and block
	 * devices, we may need to do further work.
	 *
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	drain_output(vp, 1);
	if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
		FREE_LOCK(&lk);
		return (0);
	}

	FREE_LOCK(&lk);
	/*
	 * If we are trying to sync a block device, some of its buffers may
	 * contain metadata that cannot be written until the contents of some
	 * partially written files have been written to disk. The only easy
	 * way to accomplish this is to sync the entire filesystem (luckily
	 * this happens rarely).
	 */
	if (vn_isdisk(vp, NULL) &&
	    vp->v_specmountpoint && !VOP_ISLOCKED(vp) &&
	    (error = VFS_SYNC(vp->v_specmountpoint, MNT_WAIT, 0, ap->a_cred,
	     ap->a_p)) != 0)
		return (error);
	return (0);
}

/*
 * Flush the dependencies associated with an inodedep.
 * Called with splbio blocked.
 */
STATIC int
flush_inodedep_deps(struct fs *fs, ufsino_t ino)
{
	struct inodedep *inodedep;
	struct allocdirect *adp;
	int gotit, error, waitfor;
	struct buf *bp;

	splassert(IPL_BIO);

	/*
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 * We give a brief window at the top of the loop to allow
	 * any pending I/O to complete.
	 */
	for (waitfor = MNT_NOWAIT; ; ) {
	retry_ino:
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(fs, ino, 0, &inodedep) == 0)
			return (0);
		TAILQ_FOREACH(adp, &inodedep->id_inoupdt, ad_next) {
			if (adp->ad_state & DEPCOMPLETE)
				continue;
			bp = adp->ad_buf;
			gotit = getdirtybuf(bp, waitfor);
			if (gotit == 0) {
				if (waitfor == MNT_NOWAIT)
					continue;
				break;
			} else if (gotit == -1)
				goto retry_ino;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(bp);
			} else if ((error = VOP_BWRITE(bp)) != 0) {
				ACQUIRE_LOCK(&lk);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;
		}
		if (adp != NULL)
			continue;
	retry_newino:
		TAILQ_FOREACH(adp, &inodedep->id_newinoupdt, ad_next) {
			if (adp->ad_state & DEPCOMPLETE)
				continue;
			bp = adp->ad_buf;
			gotit = getdirtybuf(bp, waitfor);
			if (gotit == 0) {
				if (waitfor == MNT_NOWAIT)
					continue;
				break;
			} else if (gotit == -1)
				goto retry_newino;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(bp);
			} else if ((error = VOP_BWRITE(bp)) != 0) {
				ACQUIRE_LOCK(&lk);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			break;
		}
		if (adp != NULL)
			continue;
		/*
		 * If pass2, we are done, otherwise do pass 2.
		 */
		if (waitfor == MNT_WAIT)
			break;
		waitfor = MNT_WAIT;
	}
	/*
	 * Try freeing inodedep in case all dependencies have been removed.
	 */
	if (inodedep_lookup(fs, ino, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	return (0);
}

/*
 * Eliminate a pagedep dependency by flushing out all its diradd dependencies.
 * Called with splbio blocked.
 */
STATIC int
flush_pagedep_deps(struct vnode *pvp, struct mount *mp,
    struct diraddhd *diraddhdp)
{
	struct proc *p = CURPROC;	/* XXX */
	struct worklist *wk;
	struct inodedep *inodedep;
	struct ufsmount *ump;
	struct diradd *dap;
	struct vnode *vp;
	int gotit, error = 0;
	struct buf *bp;
	ufsino_t inum;

	splassert(IPL_BIO);

	ump = VFSTOUFS(mp);
	while ((dap = LIST_FIRST(diraddhdp)) != NULL) {
		/*
		 * Flush ourselves if this directory entry
		 * has a MKDIR_PARENT dependency.
		 */
		if (dap->da_state & MKDIR_PARENT) {
			FREE_LOCK(&lk);
			if ((error = UFS_UPDATE(VTOI(pvp), 1)))
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_PARENT) {
				FREE_LOCK(&lk);
				panic("flush_pagedep_deps: MKDIR_PARENT");
			}
		}
		/*
		 * A newly allocated directory must have its "." and
		 * ".." entries written out before its name can be
		 * committed in its parent. We do not want or need
		 * the full semantics of a synchronous VOP_FSYNC as
		 * that may end up here again, once for each directory
		 * level in the filesystem. Instead, we push the blocks
		 * and wait for them to clear. We have to fsync twice
		 * because the first call may choose to defer blocks
		 * that still have dependencies, but deferral will
		 * happen at most once.
		 */
		inum = dap->da_newinum;
		if (dap->da_state & MKDIR_BODY) {
			FREE_LOCK(&lk);
			if ((error = VFS_VGET(mp, inum, &vp)) != 0)
				break;
			if ((error=VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)) ||
			    (error=VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p))) {
				vput(vp);
				break;
			}
			drain_output(vp, 0);
			/*
			 * If first block is still dirty with a D_MKDIR
			 * dependency then it needs to be written now.
			 */
			for (;;) {
				error = 0;
				ACQUIRE_LOCK(&lk);
				bp = incore(vp, 0);
				if (bp == NULL) {
					FREE_LOCK(&lk);
					break;
				}
				LIST_FOREACH(wk, &bp->b_dep, wk_list)
					if (wk->wk_type == D_MKDIR)
						break;
				if (wk) {
					gotit = getdirtybuf(bp, MNT_WAIT);
					FREE_LOCK(&lk);
					if (gotit == -1)
						continue;
					if (gotit && (error = bwrite(bp)) != 0)
						break;
				} else
					FREE_LOCK(&lk);
				break;
			}
			vput(vp);
			/* Flushing of first block failed */
			if (error)
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_BODY) {
				FREE_LOCK(&lk);
				panic("flush_pagedep_deps: MKDIR_BODY");
			}
		}
		/*
		 * Flush the inode on which the directory entry depends.
		 * Having accounted for MKDIR_PARENT and MKDIR_BODY above,
		 * the only remaining dependency is that the updated inode
		 * count must get pushed to disk. The inode has already
		 * been pushed into its inode buffer (via VOP_UPDATE) at
		 * the time of the reference count change. So we need only
		 * locate that buffer, ensure that there will be no rollback
		 * caused by a bitmap dependency, then write the inode buffer.
		 */
		if (inodedep_lookup(ump->um_fs, inum, 0, &inodedep) == 0) {
			FREE_LOCK(&lk);
			panic("flush_pagedep_deps: lost inode");
		}
		/*
		 * If the inode still has bitmap dependencies,
		 * push them to disk.
		 */
	retry:
		if ((inodedep->id_state & DEPCOMPLETE) == 0) {
			bp = inodedep->id_buf;
			gotit = getdirtybuf(bp, MNT_WAIT);
			if (gotit == -1)
				goto retry;
			FREE_LOCK(&lk);
			if (gotit && (error = bwrite(bp)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			if (dap != LIST_FIRST(diraddhdp))
				continue;
		}
		/*
		 * If the inode is still sitting in a buffer waiting
		 * to be written, push it to disk.
		 */
		FREE_LOCK(&lk);
		if ((error = bread(ump->um_devvp,
		    fsbtodb(ump->um_fs, ino_to_fsba(ump->um_fs, inum)),
		    (int)ump->um_fs->fs_bsize, &bp)) != 0) {
		    	brelse(bp);
			break;
		}
		if ((error = bwrite(bp)) != 0)
			break;
		ACQUIRE_LOCK(&lk);
		/*
		 * If we have failed to get rid of all the dependencies
		 * then something is seriously wrong.
		 */
		if (dap == LIST_FIRST(diraddhdp)) {
			FREE_LOCK(&lk);
			panic("flush_pagedep_deps: flush failed");
		}
	}
	if (error)
		ACQUIRE_LOCK(&lk);
	return (error);
}

/*
 * A large burst of file addition or deletion activity can drive the
 * memory load excessively high. First attempt to slow things down
 * using the techniques below. If that fails, this routine requests
 * the offending operations to fall back to running synchronously
 * until the memory load returns to a reasonable level.
 */
int
softdep_slowdown(struct vnode *vp)
{
	int max_softdeps_hard;

	max_softdeps_hard = max_softdeps * 11 / 10;
	if (num_dirrem < max_softdeps_hard / 2 &&
	    num_inodedep < max_softdeps_hard)
		return (0);
	stat_sync_limit_hit += 1;
	return (1);
}

/*
 * If memory utilization has gotten too high, deliberately slow things
 * down and speed up the I/O processing.
 */
STATIC int
request_cleanup(int resource, int islocked)
{
	struct proc *p = CURPROC;
	int s;

	/*
	 * We never hold up the filesystem syncer process.
	 */
	if (p == filesys_syncer || (p->p_flag & P_SOFTDEP))
		return (0);
	/*
	 * First check to see if the work list has gotten backlogged.
	 * If it has, co-opt this process to help clean up two entries.
	 * Because this process may hold inodes locked, we cannot
	 * handle any remove requests that might block on a locked
	 * inode as that could lead to deadlock. We set P_SOFTDEP
	 * to avoid recursively processing the worklist.
	 */
	if (num_on_worklist > max_softdeps / 10) {
		atomic_setbits_int(&p->p_flag, P_SOFTDEP);
		if (islocked)
			FREE_LOCK(&lk);
		process_worklist_item(NULL, LK_NOWAIT);
		process_worklist_item(NULL, LK_NOWAIT);
		atomic_clearbits_int(&p->p_flag, P_SOFTDEP);
		stat_worklist_push += 2;
		if (islocked)
			ACQUIRE_LOCK(&lk);
		return(1);
	}
	/*
	 * Next, we attempt to speed up the syncer process. If that
	 * is successful, then we allow the process to continue.
	 */
	if (speedup_syncer())
		return(0);
	/*
	 * If we are resource constrained on inode dependencies, try
	 * flushing some dirty inodes. Otherwise, we are constrained
	 * by file deletions, so try accelerating flushes of directories
	 * with removal dependencies. We would like to do the cleanup
	 * here, but we probably hold an inode locked at this point and 
	 * that might deadlock against one that we try to clean. So,
	 * the best that we can do is request the syncer daemon to do
	 * the cleanup for us.
	 */
	switch (resource) {

	case FLUSH_INODES:
		stat_ino_limit_push += 1;
		req_clear_inodedeps += 1;
		stat_countp = &stat_ino_limit_hit;
		break;

	case FLUSH_REMOVE:
		stat_blk_limit_push += 1;
		req_clear_remove += 1;
		stat_countp = &stat_blk_limit_hit;
		break;

	default:
		if (islocked)
			FREE_LOCK(&lk);
		panic("request_cleanup: unknown type");
	}
	/*
	 * Hopefully the syncer daemon will catch up and awaken us.
	 * We wait at most tickdelay before proceeding in any case.
	 */
	if (islocked == 0)
		ACQUIRE_LOCK(&lk);
	proc_waiting += 1;
	if (!timeout_pending(&proc_waiting_timeout))
		timeout_add(&proc_waiting_timeout, tickdelay > 2 ? tickdelay : 2);

	s = FREE_LOCK_INTERLOCKED(&lk);
	(void) tsleep((caddr_t)&proc_waiting, PPAUSE, "softupdate", 0);
	ACQUIRE_LOCK_INTERLOCKED(&lk, s);
	proc_waiting -= 1;
	if (islocked == 0)
		FREE_LOCK(&lk);
	return (1);
}

/*
 * Awaken processes pausing in request_cleanup and clear proc_waiting
 * to indicate that there is no longer a timer running.
 */
void
pause_timer(void *arg)
{

	*stat_countp += 1;
	wakeup_one(&proc_waiting);
	if (proc_waiting > 0)
		timeout_add(&proc_waiting_timeout, tickdelay > 2 ? tickdelay : 2);
}

/*
 * Flush out a directory with at least one removal dependency in an effort to
 * reduce the number of dirrem, freefile, and freeblks dependency structures.
 */
STATIC void
clear_remove(struct proc *p)
{
	struct pagedep_hashhead *pagedephd;
	struct pagedep *pagedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	int error, cnt;
	ufsino_t ino;

	ACQUIRE_LOCK(&lk);
	for (cnt = 0; cnt <= pagedep_hash; cnt++) {
		pagedephd = &pagedep_hashtbl[next++];
		if (next > pagedep_hash)
			next = 0;
		LIST_FOREACH(pagedep, pagedephd, pd_hash) {
			if (LIST_FIRST(&pagedep->pd_dirremhd) == NULL)
				continue;
			mp = pagedep->pd_mnt;
			ino = pagedep->pd_ino;
#if 0
			if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
				continue;
#endif
			FREE_LOCK(&lk);
			if ((error = VFS_VGET(mp, ino, &vp)) != 0) {
				softdep_error("clear_remove: vget", error);
#if 0
				vn_finished_write(mp);
#endif
				return;
			}
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)))
				softdep_error("clear_remove: fsync", error);
			drain_output(vp, 0);
			vput(vp);
#if 0
			vn_finished_write(mp);
#endif
			return;
		}
	}
	FREE_LOCK(&lk);
}

/*
 * Clear out a block of dirty inodes in an effort to reduce
 * the number of inodedep dependency structures.
 */
STATIC void
clear_inodedeps(struct proc *p)
{
	struct inodedep_hashhead *inodedephd;
	struct inodedep *inodedep = NULL;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	struct fs *fs;
	int error, cnt;
	ufsino_t firstino, lastino, ino;

	ACQUIRE_LOCK(&lk);
	/*
	 * Pick a random inode dependency to be cleared.
	 * We will then gather up all the inodes in its block 
	 * that have dependencies and flush them out.
	 */
	for (cnt = 0; cnt <= inodedep_hash; cnt++) {
		inodedephd = &inodedep_hashtbl[next++];
		if (next > inodedep_hash)
			next = 0;
		if ((inodedep = LIST_FIRST(inodedephd)) != NULL)
			break;
	}
	if (inodedep == NULL) {
		FREE_LOCK(&lk);
		return;
	}
	/*
	 * Ugly code to find mount point given pointer to superblock.
	 */
	fs = inodedep->id_fs;
	TAILQ_FOREACH(mp, &mountlist, mnt_list)
		if ((mp->mnt_flag & MNT_SOFTDEP) && fs == VFSTOUFS(mp)->um_fs)
			break;
	/*
	 * Find the last inode in the block with dependencies.
	 */
	firstino = inodedep->id_ino & ~(INOPB(fs) - 1);
	for (lastino = firstino + INOPB(fs) - 1; lastino > firstino; lastino--)
		if (inodedep_lookup(fs, lastino, 0, &inodedep) != 0)
			break;
	/*
	 * Asynchronously push all but the last inode with dependencies.
	 * Synchronously push the last inode with dependencies to ensure
	 * that the inode block gets written to free up the inodedeps.
	 */
	for (ino = firstino; ino <= lastino; ino++) {
		if (inodedep_lookup(fs, ino, 0, &inodedep) == 0)
			continue;
		FREE_LOCK(&lk);
#if 0
		if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
			continue;
#endif
		if ((error = VFS_VGET(mp, ino, &vp)) != 0) {
			softdep_error("clear_inodedeps: vget", error);
#if 0
			vn_finished_write(mp);
#endif
			return;
		}
		if (ino == lastino) {
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_WAIT, p)))
				softdep_error("clear_inodedeps: fsync1", error);
		} else {
			if ((error = VOP_FSYNC(vp, p->p_ucred, MNT_NOWAIT, p)))
				softdep_error("clear_inodedeps: fsync2", error);
			drain_output(vp, 0);
		}
		vput(vp);
#if 0
		vn_finished_write(mp);
#endif
		ACQUIRE_LOCK(&lk);
	}
	FREE_LOCK(&lk);
}

/*
 * Function to determine if the buffer has outstanding dependencies
 * that will cause a roll-back if the buffer is written. If wantcount
 * is set, return number of dependencies, otherwise just yes or no.
 */
int
softdep_count_dependencies(struct buf *bp, int wantcount, int islocked)
{
	struct worklist *wk;
	struct inodedep *inodedep;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct diradd *dap;
	int i, retval;

	retval = 0;
	if (!islocked)
		ACQUIRE_LOCK(&lk);
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if ((inodedep->id_state & DEPCOMPLETE) == 0) {
				/* bitmap allocation dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_inoupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);

			LIST_FOREACH(aip, &indirdep->ir_deplisthd, ai_next) {
				/* indirect block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {

				LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
					/* directory entry dependency */
					retval += 1;
					if (!wantcount)
						goto out;
				}
			}
			continue;

		case D_BMSAFEMAP:
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
		case D_MKDIR:
			/* never a dependency on these blocks */
			continue;

		default:
			if (!islocked)
				FREE_LOCK(&lk);
			panic("softdep_check_for_rollback: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
out:
	if (!islocked)
		FREE_LOCK(&lk);
	return retval;
}

/*
 * Acquire exclusive access to a buffer.
 * Must be called with splbio blocked.
 * Returns:
 * 1 if the buffer was acquired and is dirty;
 * 0 if the buffer was clean, or we would have slept but had MN_NOWAIT;
 * -1 if we slept and may try again (but not with this bp).
 */
STATIC int
getdirtybuf(struct buf *bp, int waitfor)
{
	int s;

	if (bp == NULL)
		return (0);

	splassert(IPL_BIO);

	if (bp->b_flags & B_BUSY) {
		if (waitfor != MNT_WAIT)
			return (0);
		bp->b_flags |= B_WANTED;
		s = FREE_LOCK_INTERLOCKED(&lk);
		tsleep((caddr_t)bp, PRIBIO + 1, "sdsdty", 0);
		ACQUIRE_LOCK_INTERLOCKED(&lk, s);
		return (-1);
	}
	if ((bp->b_flags & B_DELWRI) == 0)
		return (0);
	bremfree(bp);
	buf_acquire(bp);
	return (1);
}

/*
 * Wait for pending output on a vnode to complete.
 * Must be called with vnode locked.
 */
STATIC void
drain_output(struct vnode *vp, int islocked)
{
	int s;

	if (!islocked)
		ACQUIRE_LOCK(&lk);

	splassert(IPL_BIO);

	while (vp->v_numoutput) {
		vp->v_bioflag |= VBIOWAIT;
		s = FREE_LOCK_INTERLOCKED(&lk);
		tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "drain_output", 0);
		ACQUIRE_LOCK_INTERLOCKED(&lk, s);
	}
	if (!islocked)
		FREE_LOCK(&lk);
}

/*
 * Called whenever a buffer that is being invalidated or reallocated
 * contains dependencies. This should only happen if an I/O error has
 * occurred. The routine is called with the buffer locked.
 */ 
void
softdep_deallocate_dependencies(struct buf *bp)
{

	if ((bp->b_flags & B_ERROR) == 0)
		panic("softdep_deallocate_dependencies: dangling deps");
	softdep_error(bp->b_vp->v_mount->mnt_stat.f_mntonname, bp->b_error);
	panic("softdep_deallocate_dependencies: unrecovered I/O error");
}

/*
 * Function to handle asynchronous write errors in the filesystem.
 */
void
softdep_error(char *func, int error)
{

	/* XXX should do something better! */
	printf("%s: got error %d while accessing filesystem\n", func, error);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
softdep_print(struct buf *bp, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct worklist *wk;

	(*pr)("  deps:\n");
	LIST_FOREACH(wk, &bp->b_dep, wk_list)
		worklist_print(wk, full, pr);
}

void
worklist_print(struct worklist *wk, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct newblk *newblk;
	struct bmsafemap *bmsafemap;
	struct allocdirect *adp;
	struct indirdep *indirdep;
	struct allocindir *aip;
	struct freefrag *freefrag;
	struct freeblks *freeblks;
	struct freefile *freefile;
	struct diradd *dap;
	struct mkdir *mkdir;
	struct dirrem *dirrem;
	struct newdirblk *newdirblk;
	char prefix[33];
	int i;

	for (prefix[i = 2 * MIN(16, full)] = '\0'; i--; prefix[i] = ' ')
		;

	(*pr)("%s%s(%p) state %b\n%s", prefix, TYPENAME(wk->wk_type), wk,
	    wk->wk_state, DEP_BITS, prefix);
	switch (wk->wk_type) {
	case D_PAGEDEP:
		pagedep = WK_PAGEDEP(wk);
		(*pr)("mount %p ino %u lbn %lld\n", pagedep->pd_mnt,
		    pagedep->pd_ino, (long long)pagedep->pd_lbn);
		break;
	case D_INODEDEP:
		inodedep = WK_INODEDEP(wk);
		(*pr)("fs %p ino %u nlinkdelta %u dino %p\n"
		    "%s  bp %p savsz %lld\n", inodedep->id_fs,
		    inodedep->id_ino, inodedep->id_nlinkdelta,
		    inodedep->id_un.idu_savedino1,
		    prefix, inodedep->id_buf, inodedep->id_savedsize);
		break;
	case D_NEWBLK:
		newblk = WK_NEWBLK(wk);
		(*pr)("fs %p newblk %lld state %d bmsafemap %p\n",
		    newblk->nb_fs, (long long)newblk->nb_newblkno,
		    newblk->nb_state, newblk->nb_bmsafemap);
		break;
	case D_BMSAFEMAP:
		bmsafemap = WK_BMSAFEMAP(wk);
		(*pr)("buf %p\n", bmsafemap->sm_buf);
		break;
	case D_ALLOCDIRECT:
		adp = WK_ALLOCDIRECT(wk);
		(*pr)("lbn %lld newlbk %lld oldblk %lld newsize %ld olsize "
		    "%ld\n%s  bp %p inodedep %p freefrag %p\n",
		    (long long)adp->ad_lbn, (long long)adp->ad_newblkno,
		    (long long)adp->ad_oldblkno, adp->ad_newsize,
		    adp->ad_oldsize,
		    prefix, adp->ad_buf, adp->ad_inodedep, adp->ad_freefrag);
		break;
	case D_INDIRDEP:
		indirdep = WK_INDIRDEP(wk);
		(*pr)("savedata %p savebp %p\n", indirdep->ir_saveddata,
		    indirdep->ir_savebp);
		break;
	case D_ALLOCINDIR:
		aip = WK_ALLOCINDIR(wk);
		(*pr)("off %d newblk %lld oldblk %lld freefrag %p\n"
		    "%s  indirdep %p buf %p\n", aip->ai_offset,
		    (long long)aip->ai_newblkno, (long long)aip->ai_oldblkno,
		    aip->ai_freefrag, prefix, aip->ai_indirdep, aip->ai_buf);
		break;
	case D_FREEFRAG:
		freefrag = WK_FREEFRAG(wk);
		(*pr)("vnode %p mp %p blkno %lld fsize %ld ino %u\n",
		    freefrag->ff_devvp, freefrag->ff_mnt,
		    (long long)freefrag->ff_blkno, freefrag->ff_fragsize,
		    freefrag->ff_inum);
		break;
	case D_FREEBLKS:
		freeblks = WK_FREEBLKS(wk);
		(*pr)("previno %u devvp %p mp %p oldsz %lld newsz %lld\n"
		    "%s  chkcnt %d uid %d\n", freeblks->fb_previousinum,
		    freeblks->fb_devvp, freeblks->fb_mnt, freeblks->fb_oldsize,
		    freeblks->fb_newsize,
		    prefix, freeblks->fb_chkcnt, freeblks->fb_uid);
		break;
	case D_FREEFILE:
		freefile = WK_FREEFILE(wk);
		(*pr)("mode %x oldino %u vnode %p mp %p\n", freefile->fx_mode,
		    freefile->fx_oldinum, freefile->fx_devvp, freefile->fx_mnt);
		break;
	case D_DIRADD:
		dap = WK_DIRADD(wk);
		(*pr)("off %d ino %u da_un %p\n", dap->da_offset, 
		    dap->da_newinum, dap->da_un.dau_previous);
		break;
	case D_MKDIR:
		mkdir = WK_MKDIR(wk);
		(*pr)("diradd %p bp %p\n", mkdir->md_diradd, mkdir->md_buf);
		break;
	case D_DIRREM:
		dirrem = WK_DIRREM(wk);
		(*pr)("mp %p ino %u dm_un %p\n", dirrem->dm_mnt, 
		    dirrem->dm_oldinum, dirrem->dm_un.dmu_pagedep);
		break;
	case D_NEWDIRBLK:
		newdirblk = WK_NEWDIRBLK(wk);
		(*pr)("pagedep %p\n", newdirblk->db_pagedep);
		break;
	}
}
#endif
