/*	$OpenBSD: pcnfsd_v1.c,v 1.3 2003/02/15 11:53:45 deraadt Exp $	*/
/*	$NetBSD: pcnfsd_v1.c,v 1.2 1995/07/25 22:21:19 gwr Exp $	*/

/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_v1.c	1.1	9/3/91
**
** pcnfsd is copyrighted software, but is freely licensed. This
** means that you are free to redistribute it, modify it, ship it
** in binary with your system, whatever, provided:
**
** - you leave the Sun copyright notice in the source code
** - you make clear what changes you have introduced and do
**   not represent them as being supported by Sun.
** - you do not charge money for the source code (unlikely, given
**   its free availability)
**
** If you make changes to this software, we ask that you do so in
** a way which allows you to build either the "standard" version or
** your custom version from a single source file. Test it, lint
** it (it won't lint 100%, very little does, and there are bugs in
** some versions of lint :-), and send it back to Sun via email
** so that we can roll it into the source base and redistribute
** it. We'll try to make sure your contributions are acknowledged
** in the source, but after all these years it's getting hard to
** remember who did what.
**=====================================================================
*/

#include <sys/types.h>
#include <sys/stat.h>

#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pcnfsd.h"
#include "paths.h"

/*ARGSUSED*/
void *
pcnfsd_null_1_svc(arg, req)
	void *arg;
	struct svc_req *req;
{
	static char dummy;

	return ((void *)&dummy);
}

auth_results *
pcnfsd_auth_1_svc(arg, req)
	auth_args *arg;
	struct svc_req *req;
{
	static auth_results r;
	char uname[32], pw[64];
	int c1, c2;
	struct passwd *p;


	r.stat = AUTH_RES_FAIL;	/* assume failure */
	r.uid = (int)-2;
	r.gid = (int)-2;

	scramble(arg->id, uname);
	scramble(arg->pw, pw);

	if (check_cache(uname, pw, &r.uid, &r.gid)) {
		r.stat = AUTH_RES_OK;
		wlogin(uname, req);
		return (&r);
	}

	if ((p = get_password(uname)) == NULL)
		return (&r);

	c1 = strlen(pw);
	c2 = strlen(p->pw_passwd);
	if ((c1 && !c2) || (c2 && !c1) ||
	   (strcmp(p->pw_passwd, crypt(pw, p->pw_passwd)))) 
           {
	   return (&r);
	   }
	r.stat = AUTH_RES_OK;
	r.uid = p->pw_uid;
	r.gid = p->pw_gid;
		wlogin(uname, req);

	add_cache_entry(p);

	return (&r);
}

pr_init_results *
pcnfsd_pr_init_1_svc(pi_arg, req)
	pr_init_args *pi_arg;
	struct svc_req *req;
{
	static pr_init_results pi_res;

	pi_res.stat = (pirstat)pr_init(pi_arg->system, pi_arg->pn, &pi_res.dir);

	return (&pi_res);
}

pr_start_results *
pcnfsd_pr_start_1_svc(ps_arg, req)
	pr_start_args *ps_arg;
	struct svc_req *req;
{
	static pr_start_results ps_res;
	char *dummyptr;

	ps_res.stat = (psrstat)pr_start2(ps_arg->system, ps_arg->pn,
			ps_arg->user, ps_arg->file, ps_arg->opts, &dummyptr);

	return (&ps_res);
}
