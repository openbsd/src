/*       $OpenBSD: vfs_sync.c,v 1.46 2010/07/03 03:45:16 thib Exp $  */

/*
 *  Portions of this code are:
 *
 * Copyright (c) 1989, 1993
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
 */

/*
 * Syncer daemon
 */

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <sys/kernel.h>
#include <sys/sched.h>

#ifdef FFS_SOFTUPDATES
int   softdep_process_worklist(struct mount *);
#endif

/*
 * The workitem queue.
 */
#define SYNCER_MAXDELAY	32		/* maximum sync delay time */
#define SYNCER_DEFAULT 30		/* default sync delay time */
int syncer_maxdelay = SYNCER_MAXDELAY;	/* maximum delay time */
int syncdelay = SYNCER_DEFAULT;		/* time to delay syncing vnodes */

int rushjob = 0;			/* number of slots to run ASAP */
int stat_rush_requests = 0;		/* number of rush requests */

int syncer_delayno = 0;
long syncer_mask;
LIST_HEAD(synclist, vnode);
static struct synclist *syncer_workitem_pending;

struct proc *syncerproc;

/*
 * The workitem queue.
 *
 * It is useful to delay writes of file data and filesystem metadata
 * for tens of seconds so that quickly created and deleted files need
 * not waste disk bandwidth being created and removed. To realize this,
 * we append vnodes to a "workitem" queue. When running with a soft
 * updates implementation, most pending metadata dependencies should
 * not wait for more than a few seconds. Thus, mounted block devices
 * are delayed only about half the time that file data is delayed.
 * Similarly, directory updates are more critical, so are only delayed
 * about a third the time that file data is delayed. Thus, there are
 * SYNCER_MAXDELAY queues that are processed round-robin at a rate of
 * one each second (driven off the filesystem syncer process). The
 * syncer_delayno variable indicates the next queue that is to be processed.
 * Items that need to be processed soon are placed in this queue:
 *
 *	syncer_workitem_pending[syncer_delayno]
 *
 * A delay of fifteen seconds is done by placing the request fifteen
 * entries later in the queue:
 *
 *	syncer_workitem_pending[(syncer_delayno + 15) & syncer_mask]
 *
 */

void
vn_initialize_syncerd(void)
{
	syncer_workitem_pending = hashinit(syncer_maxdelay, M_VNODE, M_WAITOK,
	    &syncer_mask);
	syncer_maxdelay = syncer_mask + 1;
}

/*
 * Add an item to the syncer work queue.
 */
void
vn_syncer_add_to_worklist(struct vnode *vp, int delay)
{
	int s, slot;

	if (delay > syncer_maxdelay - 2)
		delay = syncer_maxdelay - 2;
	slot = (syncer_delayno + delay) & syncer_mask;

	s = splbio();
	if (vp->v_bioflag & VBIOONSYNCLIST)
		LIST_REMOVE(vp, v_synclist);

	vp->v_bioflag |= VBIOONSYNCLIST;
	LIST_INSERT_HEAD(&syncer_workitem_pending[slot], vp, v_synclist);
	splx(s);
}

/*
 * System filesystem synchronizer daemon.
 */
void
sched_sync(struct proc *p)
{
	struct synclist *slp;
	struct vnode *vp;
	long starttime;
	int s;

	syncerproc = curproc;

	for (;;) {
		starttime = time_second;

		/*
		 * Push files whose dirty time has expired.
		 */
		s = splbio();
		slp = &syncer_workitem_pending[syncer_delayno];

		syncer_delayno += 1;
		if (syncer_delayno == syncer_maxdelay)
			syncer_delayno = 0;

		while ((vp = LIST_FIRST(slp)) != NULL) {
			if (vget(vp, LK_EXCLUSIVE | LK_NOWAIT, p)) {
				/*
				 * If we fail to get the lock, we move this
				 * vnode one second ahead in time.
				 * XXX - no good, but the best we can do.
				 */
				vn_syncer_add_to_worklist(vp, 1);
				continue;
			}
			splx(s);
			(void) VOP_FSYNC(vp, p->p_ucred, MNT_LAZY, p);
			vput(vp);
			s = splbio();
			if (LIST_FIRST(slp) == vp) {
				/*
				 * Note: disk vps can remain on the
				 * worklist too with no dirty blocks, but
				 * since sync_fsync() moves it to a different
				 * slot we are safe.
				 */
#ifdef DIAGNOSTIC
				if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL &&
				    vp->v_type != VBLK) {
					vprint("fsync failed", vp);
					if (vp->v_mount != NULL)
						printf("mounted on: %s\n",
						    vp->v_mount->mnt_stat.f_mntonname);
					panic("sched_sync: fsync failed");
				}
#endif /* DIAGNOSTIC */
				/*
				 * Put us back on the worklist.  The worklist
				 * routine will remove us from our current
				 * position and then add us back in at a later
				 * position.
				 */
				vn_syncer_add_to_worklist(vp, syncdelay);
			}
		}

		splx(s);

#ifdef FFS_SOFTUPDATES
		/*
		 * Do soft update processing.
		 */
		softdep_process_worklist(NULL);
#endif

		/*
		 * The variable rushjob allows the kernel to speed up the
		 * processing of the filesystem syncer process. A rushjob
		 * value of N tells the filesystem syncer to process the next
		 * N seconds worth of work on its queue ASAP. Currently rushjob
		 * is used by the soft update code to speed up the filesystem
		 * syncer process when the incore state is getting so far
		 * ahead of the disk that the kernel memory pool is being
		 * threatened with exhaustion.
		 */
		if (rushjob > 0) {
			rushjob -= 1;
			continue;
		}
		/*
		 * If it has taken us less than a second to process the
		 * current work, then wait. Otherwise start right over
		 * again. We can still lose time if any single round
		 * takes more than two seconds, but it does not really
		 * matter as we are just trying to generally pace the
		 * filesystem activity.
		 */
		if (time_second == starttime)
			tsleep(&lbolt, PPAUSE, "syncer", 0);
	}
}

/*
 * Request the syncer daemon to speed up its work.
 * We never push it to speed up more than half of its
 * normal turn time, otherwise it could take over the cpu.
 */
int
speedup_syncer(void)
{
	int s;

	SCHED_LOCK(s);
	if (syncerproc && syncerproc->p_wchan == &lbolt)
		setrunnable(syncerproc);
	SCHED_UNLOCK(s);
	if (rushjob < syncdelay / 2) {
		rushjob += 1;
		stat_rush_requests += 1;
		return 1;
	}
	return 0;
}

/*
 * Routine to create and manage a filesystem syncer vnode.
 */
#define sync_close nullop
int   sync_fsync(void *);
int   sync_inactive(void *);
#define sync_reclaim nullop
#define sync_lock vop_generic_lock
#define sync_unlock vop_generic_unlock
int   sync_print(void *);
#define sync_islocked vop_generic_islocked

int (**sync_vnodeop_p)(void *);
struct vnodeopv_entry_desc sync_vnodeop_entries[] = {
      { &vop_default_desc, eopnotsupp },
      { &vop_close_desc, sync_close },
      { &vop_fsync_desc, sync_fsync },
      { &vop_inactive_desc, sync_inactive },
      { &vop_reclaim_desc, sync_reclaim },
      { &vop_lock_desc, sync_lock },
      { &vop_unlock_desc, sync_unlock },
      { &vop_print_desc, sync_print },
      { &vop_islocked_desc, sync_islocked },
      { (struct vnodeop_desc*)NULL, (int(*)(void *))NULL }
};
struct vnodeopv_desc sync_vnodeop_opv_desc = {
	&sync_vnodeop_p, sync_vnodeop_entries
};

/*
 * Create a new filesystem syncer vnode for the specified mount point.
 */
int
vfs_allocate_syncvnode(struct mount *mp)
{
	struct vnode *vp;
	static long start, incr, next;
	int error;

	/* Allocate a new vnode */
	if ((error = getnewvnode(VT_VFS, mp, sync_vnodeop_p, &vp)) != 0) {
		mp->mnt_syncer = NULL;
		return (error);
	}
	vp->v_writecount = 1;
	vp->v_type = VNON;
	/*
	 * Place the vnode onto the syncer worklist. We attempt to
	 * scatter them about on the list so that they will go off
	 * at evenly distributed times even if all the filesystems
	 * are mounted at once.
	 */
	next += incr;
	if (next == 0 || next > syncer_maxdelay) {
		start /= 2;
		incr /= 2;
		if (start == 0) {
			start = syncer_maxdelay / 2;
			incr = syncer_maxdelay;
		}
		next = start;
	}
	vn_syncer_add_to_worklist(vp, next);
	mp->mnt_syncer = vp;
	return (0);
}

/*
 * Do a lazy sync of the filesystem.
 */
int
sync_fsync(void *v)
{
	struct vop_fsync_args *ap = v;
	struct vnode *syncvp = ap->a_vp;
	struct mount *mp = syncvp->v_mount;
	int asyncflag;

	/*
	 * We only need to do something if this is a lazy evaluation.
	 */
	if (ap->a_waitfor != MNT_LAZY)
		return (0);

	/*
	 * Move ourselves to the back of the sync list.
	 */
	vn_syncer_add_to_worklist(syncvp, syncdelay);

	/*
	 * Walk the list of vnodes pushing all that are dirty and
	 * not already on the sync list.
	 */
	if (vfs_busy(mp, VB_READ|VB_NOWAIT) == 0) {
		asyncflag = mp->mnt_flag & MNT_ASYNC;
		mp->mnt_flag &= ~MNT_ASYNC;
		VFS_SYNC(mp, MNT_LAZY, ap->a_cred, ap->a_p);
		if (asyncflag)
			mp->mnt_flag |= MNT_ASYNC;
		vfs_unbusy(mp);
	}

	return (0);
}

/*
 * The syncer vnode is no longer needed and is being decommissioned.
 */
int
sync_inactive(void *v)
{
	struct vop_inactive_args *ap = v;

	struct vnode *vp = ap->a_vp;
	int s;

	if (vp->v_usecount == 0) {
		VOP_UNLOCK(vp, 0, ap->a_p);
		return (0);
	}

	vp->v_mount->mnt_syncer = NULL;

	s = splbio();

	LIST_REMOVE(vp, v_synclist);
	vp->v_bioflag &= ~VBIOONSYNCLIST;

	splx(s);

	vp->v_writecount = 0;
	vput(vp);

	return (0);
}

/*
 * Print out a syncer vnode.
 */
int
sync_print(void *v)
{
	printf("syncer vnode\n");

	return (0);
}
