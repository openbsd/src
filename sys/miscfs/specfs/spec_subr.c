/*	$OpenBSD: spec_subr.c,v 1.4 2008/04/08 14:46:45 thib Exp $	*/

/*
 * Copyright (c) 2006 Pedro Martelletto <pedro@ambientworks.net>
 * Copyright (c) 2006 Thordur Bjornsson <thib@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>

#ifdef	CLONE_DEBUG
#define	DNPRINTF(m...)	do { printf(m);  } while (0)
#else
#define	DNPRINTF(m...)	/* nothing */
#endif

int
spec_open_clone(struct vop_open_args *ap)
{
	struct vnode *cvp, *vp = ap->a_vp;
	struct cloneinfo *cip;
	int error, i;

	DNPRINTF("cloning vnode\n");

	for (i = 1; i < sizeof(vp->v_specbitmap) * NBBY; i++)
		if (isclr(vp->v_specbitmap, i)) {
			setbit(vp->v_specbitmap, i);
			break;
		}

	if (i == sizeof(vp->v_specbitmap) * NBBY)
		return (EBUSY); /* too many open instances */

	error = cdevvp(makedev(major(vp->v_rdev), i), &cvp);
	if (error)
		return (error); /* out of vnodes */

	VOP_UNLOCK(vp, 0, ap->a_p);

	error = cdevsw[major(vp->v_rdev)].d_open(cvp->v_rdev, ap->a_mode,
	    S_IFCHR, ap->a_p);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);

	if (error) {
		 clrbit(vp->v_specbitmap, i);
		 return (error); /* device open failed */
	}

	cvp->v_flag |= VCLONE;

	cip = malloc(sizeof(struct cloneinfo), M_TEMP, M_WAITOK);
	cip->ci_data = vp->v_data;
	cip->ci_vp = cvp;

	cvp->v_specparent = vp;
	vp->v_flag |= VCLONED;
	vp->v_data = cip;

	DNPRINTF("clone of vnode %p is vnode %p\n", vp, cvp);

	return (0); /* device cloned */
}

int
spec_close_clone(struct vop_close_args *ap)
{
	struct vnode *pvp, *vp = ap->a_vp;
	int error;

	error = cdevsw[major(vp->v_rdev)].d_close(vp->v_rdev, ap->a_fflag,
	    S_IFCHR, ap->a_p);
	if (error)
		return (error); /* device close failed */

	pvp = vp->v_specparent; /* get parent device */
	clrbit(pvp->v_specbitmap, minor(vp->v_rdev));
	vrele(pvp);

	return (0); /* clone closed */
}
