/*	$OpenBSD: procfs_status.c,v 1.8 2004/05/05 23:52:10 tedu Exp $	*/
/*	$NetBSD: procfs_status.c,v 1.11 1996/03/16 23:52:50 christos Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <miscfs/procfs/procfs.h>

int	procfs_stat_gen(struct proc *, char *s, int);

#define COUNTORCAT(s, l, ps, n)	do { \
					if (s) \
						strlcat(s, ps, l); \
					else \
						n += strlen(ps); \
				} while (0)

/* Generates:
 *  comm pid ppid pgid sid maj,min ctty,sldr start ut st wmsg uid gid groups
 */
int
procfs_stat_gen(p, s, l)
	struct proc *p;
	char *s;
	int l;
{
	struct session *sess;
	struct tty *tp;
	struct ucred *cr;
	int pid, ppid, pgid, sid;
	struct timeval ut, st;
	char ps[256], *sep;
	int i, n;

	pid = p->p_pid;
	ppid = p->p_pptr ? p->p_pptr->p_pid : 0;
	pgid = p->p_pgrp->pg_id;
	sess = p->p_pgrp->pg_session;
	sid = sess->s_leader ? sess->s_leader->p_pid : 0;

	n = 0;
	if (s)
		bzero(s, l);

	bcopy(p->p_comm, ps, MAXCOMLEN-1);
	ps[MAXCOMLEN] = '\0';
	COUNTORCAT(s, l, ps, n);

	(void) snprintf(ps, sizeof(ps), " %d %d %d %d ",
	    pid, ppid, pgid, sid);
	COUNTORCAT(s, l, ps, n);

	if ((p->p_flag&P_CONTROLT) && (tp = sess->s_ttyp))
		snprintf(ps, sizeof(ps), "%d,%d ",
		    major(tp->t_dev), minor(tp->t_dev));
	else
		snprintf(ps, sizeof(ps), "%d,%d ",
		    -1, -1);
	COUNTORCAT(s, l, ps, n);

	sep = "";
	if (sess->s_ttyvp) {
		snprintf(ps, sizeof(ps), "%sctty", sep);
		sep = ",";
		COUNTORCAT(s, l, ps, n);
	}

	if (SESS_LEADER(p)) {
		snprintf(ps, sizeof(ps), "%ssldr", sep);
		sep = ",";
		COUNTORCAT(s, l, ps, n);
	}

	if (*sep != ',') {
		snprintf(ps, sizeof(ps), "noflags");
		COUNTORCAT(s, l, ps, n);
	}

	if (p->p_flag & P_INMEM)
		snprintf(ps, sizeof(ps), " %ld,%ld",
		    p->p_stats->p_start.tv_sec, p->p_stats->p_start.tv_usec);
	else
		snprintf(ps, sizeof(ps), " -1,-1");
	COUNTORCAT(s, l, ps, n);

	calcru(p, &ut, &st, (void *) 0);
	snprintf(ps, sizeof(ps), " %ld,%ld %ld,%ld",
	    ut.tv_sec, ut.tv_usec, st.tv_sec, st.tv_usec);
	COUNTORCAT(s, l, ps, n);

	snprintf(ps, sizeof(ps), " %s",
	    (p->p_wchan && p->p_wmesg) ? p->p_wmesg : "nochan");
	COUNTORCAT(s, l, ps, n);

	cr = p->p_ucred;

	snprintf(ps, sizeof(ps), " %u, %u", cr->cr_uid, cr->cr_gid);
	COUNTORCAT(s, l, ps, n);
	for (i = 0; i < cr->cr_ngroups; i++) {
		snprintf(ps, sizeof(ps), ",%u", cr->cr_groups[i]);
		COUNTORCAT(s, l, ps, n);
	}

	snprintf(ps, sizeof(ps), "\n");
	COUNTORCAT(s, l, ps, n);

	return (s != NULL ? strlen(s) + 1 : n + 1);
}

int
procfs_dostatus(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int error, len;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	len = procfs_stat_gen(p, NULL, 0);
	ps = malloc(len, M_TEMP, M_WAITOK);
	len = procfs_stat_gen(p, ps, len);

	if (len <= uio->uio_offset)
		error = 0;
	else {
		len -= uio->uio_offset;
		len = imin(len, uio->uio_resid);
		error = uiomove(ps + uio->uio_offset, len, uio);
	}

	free(ps, M_TEMP);
	return (error);
}
