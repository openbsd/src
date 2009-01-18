/*
 * Copyright (c) 2009 Thordur I. Bjornsson. <thib@openbsd.org>
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

extern struct nfsreqhead nfs_reqq;

void
db_show_all_nfsreqs(db_expr_t expr, int haddr, db_expr_t count, char *modif)
{
	struct nfsreq	*rep;

	if (TAILQ_EMPTY(&nfs_reqq)) {
		db_printf("no outstanding requests\n");
		return;
	}

	TAILQ_FOREACH(rep, &nfs_reqq, r_chain)
		db_printf("%p\n", rep);

}

void
db_nfsreq_print(struct nfsreq *rep, int full, int (*pr)(const char *, ...))
{
	(*pr)("xid 0x%x flags 0x%x rexmit %i procnum %i proc %p\n",
	    rep->r_xid, rep->r_flags, rep->r_rexmit, rep->r_procnum,
	    rep->r_procp);

	if (full) {
		(*pr)("mreq %p mrep %p md %p nfsmount %p vnode %p timer %i",
		    " rtt %i\n",
		    rep->r_mreq, rep->r_mrep, rep->r_md, rep->r_nmp,
		    rep->r_vp, rep->r_timer, rep->r_rtt);
	}
}
