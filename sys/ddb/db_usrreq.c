/*	$OpenBSD: db_usrreq.c,v 1.17 2015/03/14 03:38:46 jsg Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/sysctl.h>
#include <dev/cons.h>

#include <ddb/db_var.h>

int	db_log = 1;

int
ddb_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	int error, ctlval;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {

	case DBCTL_RADIX:
		return sysctl_int(oldp, oldlenp, newp, newlen, &db_radix);
	case DBCTL_MAXWIDTH:
		return sysctl_int(oldp, oldlenp, newp, newlen, &db_max_width);
	case DBCTL_TABSTOP:
		return sysctl_int(oldp, oldlenp, newp, newlen, &db_tab_stop_width);
	case DBCTL_MAXLINE:
		return sysctl_int(oldp, oldlenp, newp, newlen, &db_max_line);
	case DBCTL_PANIC:
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &db_panic));
		else {
			ctlval = db_panic;
			if ((error = sysctl_int(oldp, oldlenp, newp, newlen,
			    &ctlval)) || newp == NULL)
				return (error);
			if (ctlval != 1 && ctlval != 0)
				return (EINVAL);
			db_panic = ctlval;
			return (0);
		}
		break;
	case DBCTL_CONSOLE:
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &db_console));
		else {
			ctlval = db_console;
			if ((error = sysctl_int(oldp, oldlenp, newp, newlen,
			    &ctlval)) || newp == NULL)
				return (error);
			if (ctlval != 1 && ctlval != 0)
				return (EINVAL);
			db_console = ctlval;
			return (0);
		}
		break;
	case DBCTL_LOG:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &db_log));
	case DBCTL_TRIGGER:
		if (newp && db_console) {
			struct process *pr = curproc->p_p;

			if (securelevel < 1 ||
			    (pr->ps_flags & PS_CONTROLT && cn_tab &&
			    cn_tab->cn_dev == pr->ps_session->s_ttyp->t_dev)) {
				Debugger();
				newp = NULL;
			} else
				return (ENODEV);
		}
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
