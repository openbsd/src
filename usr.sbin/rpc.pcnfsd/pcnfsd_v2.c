/*	$OpenBSD: pcnfsd_v2.c,v 1.5 2003/02/15 11:53:45 deraadt Exp $	*/
/*	$NetBSD: pcnfsd_v2.c,v 1.4 1995/08/14 19:50:10 gwr Exp $	*/

/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_v2.c	1.2	12/18/91
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
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "pcnfsd.h"
#include "paths.h"

static void fillin_extra_groups(char *, u_int, int *, u_int *);

static char no_comment[] = "No comment";
static char not_supported[] = "Not supported";
static char pcnfsd_version[] = "@(#)pcnfsd_v2.c	1.2 - rpc.pcnfsd V2.0 (c) 1991 Sun Technology Enterprises, Inc.";

/*ARGSUSED*/
void *
pcnfsd2_null_2_svc(arg, req)
	void*arg;
	struct svc_req *req;
{
	static char dummy;

	return ((void *)&dummy);
}

v2_auth_results *
pcnfsd2_auth_2_svc(arg, req)
	v2_auth_args *arg;
	struct svc_req *req;
{
	static v2_auth_results  r;

	char uname[32], pw[64];
	int c1, c2;
	struct passwd *p;
	static u_int extra_gids[EXTRAGIDLEN];
	static char home[MAXPATHLEN];

	r.stat = AUTH_RES_FAIL;	/* assume failure */
	r.uid = (int)-2;
	r.gid = (int)-2;
	r.cm = &no_comment[0];
	r.gids.gids_len = 0;
	r.gids.gids_val = &extra_gids[0];
	home[0] = '\0';
	r.home = &home[0];
	r.def_umask = umask(0);
	(void)umask(r.def_umask);	/* or use 022 */

	scramble(arg->id, uname);
	scramble(arg->pw, pw);

	if (check_cache(uname, pw, &r.uid, &r.gid)) {
		r.stat = AUTH_RES_OK;
		wlogin(uname, req);
		fillin_extra_groups(uname, r.gid, &r.gids.gids_len, extra_gids);
		return (&r);
	}

	if ((p = get_password(uname)) == NULL)
		return (&r);

	c1 = strlen(pw);
	c2 = strlen(p->pw_passwd);
	if ((c1 && !c2) || (c2 && !c1) ||
	   (strcmp(p->pw_passwd, crypt(pw, p->pw_passwd)))) {
		return (&r);
	}

	r.stat = AUTH_RES_OK;
	r.uid = p->pw_uid;
	r.gid = p->pw_gid;
	wlogin(uname, req);
        fillin_extra_groups(uname, r.gid, &r.gids.gids_len, extra_gids);

	add_cache_entry(p);

	return (&r);
}

v2_pr_init_results *
pcnfsd2_pr_init_2_svc(arg, req)
	v2_pr_init_args *arg;
	struct svc_req *req;
{
	static v2_pr_init_results res;

	res.stat = (pirstat)pr_init(arg->system, arg->pn, &res.dir);
	res.cm = &no_comment[0];

	return (&res);
}

v2_pr_start_results *
pcnfsd2_pr_start_2_svc(arg, req)
	v2_pr_start_args *arg;
	struct svc_req *req;
{
	static v2_pr_start_results res;

	res.stat = (psrstat)pr_start2(arg->system, arg->pn, arg->user,
			    arg->file, arg->opts, &res.id);
	res.cm = &no_comment[0];

	return (&res);
}

/*ARGSUSED*/
v2_pr_list_results *
pcnfsd2_pr_list_2_svc(arg, req)
	void *arg;
	struct svc_req *req;
{
	static v2_pr_list_results res;

	if (printers == NULL)
		(void)build_pr_list();
	res.cm = &no_comment[0];
	res.printers = printers;

	return (&res);
}

v2_pr_queue_results *
pcnfsd2_pr_queue_2_svc(arg, req)
	v2_pr_queue_args *arg;
	struct svc_req *req;
{
	static v2_pr_queue_results res;

	res.stat = build_pr_queue(arg->pn, arg->user, arg->just_mine,
		   &res.qlen, &res.qshown);
	res.cm = &no_comment[0];
	res.just_yours = arg->just_mine;
	res.jobs = queue;

	return (&res);
}

v2_pr_status_results *
pcnfsd2_pr_status_2_svc(arg, req)
	v2_pr_status_args *arg;
	struct svc_req *req;
{
	static v2_pr_status_results res;
	static char status[128];

	res.stat = get_pr_status(arg->pn, &res.avail, &res.printing,
		   &res.qlen, &res.needs_operator, &status[0]);
	res.status = &status[0];	
	res.cm = &no_comment[0];

	return (&res);
}

v2_pr_cancel_results *
pcnfsd2_pr_cancel_2_svc(arg, req)
	v2_pr_cancel_args *arg;
	struct svc_req *req;
{
	static v2_pr_cancel_results res;

	res.stat = pr_cancel(arg->pn, arg->user, arg->id);
	res.cm = &no_comment[0];

	return (&res);
}

/*ARGSUSED*/
v2_pr_requeue_results *
pcnfsd2_pr_requeue_2_svc(arg, req)
	v2_pr_requeue_args *arg;
	struct svc_req *req;
{
	static v2_pr_requeue_results res;

	res.stat = PC_RES_FAIL;
	res.cm = &not_supported[0];

	return (&res);
}

/*ARGSUSED*/
v2_pr_hold_results *
pcnfsd2_pr_hold_2_svc(arg, req)
	v2_pr_hold_args *arg;
	struct svc_req *req;
{
	static v2_pr_hold_results res;

	res.stat = PC_RES_FAIL;
	res.cm = &not_supported[0];

	return (&res);
}

/*ARGSUSED*/
v2_pr_release_results *
pcnfsd2_pr_release_2_svc(arg, req)
	v2_pr_release_args *arg;
	struct svc_req *req;
{
	static v2_pr_release_results res;

	res.stat = PC_RES_FAIL;
	res.cm = &not_supported[0];

	return (&res);
}

/*ARGSUSED*/
v2_pr_admin_results *
pcnfsd2_pr_admin_2_svc(arg, req)
	v2_pr_admin_args *arg;
	struct svc_req *req;
{
	static v2_pr_admin_results res;

	res.cm = &not_supported[0];
	res.stat = PI_RES_FAIL;

	return (&res);
}

void
free_mapreq_results(p)
mapreq_res p;
{
	if (p->mapreq_next)
		free_mapreq_results(p->mapreq_next); /* recurse */
	if (p->name)
		(void)free(p->name);
	(void)free(p);
	return;
}

v2_mapid_results *pcnfsd2_mapid_2_svc(arg, req)
v2_mapid_args *arg;
struct svc_req *req;
{
static v2_mapid_results res;
struct passwd *p_passwd;
struct group  *p_group;

mapreq_arg a;
mapreq_res next_r;
mapreq_res last_r = NULL;


	if (res.res_list) {
		free_mapreq_results(res.res_list);
		res.res_list = NULL;
	}

	a = arg->req_list;
	while (a) {
		next_r = (struct mapreq_res_item *)
			grab(sizeof(struct mapreq_res_item));
		next_r->stat = MAP_RES_UNKNOWN;
		next_r->req = a->req;
		next_r->id = a->id;
		next_r->name = NULL;
		next_r->mapreq_next = NULL;

		if (last_r == NULL)
			res.res_list = next_r;
		else
			last_r->mapreq_next = next_r;
		last_r = next_r;
		switch(a->req) {
		case MAP_REQ_UID:
			p_passwd = getpwuid((uid_t)a->id);
			if (p_passwd) {
				next_r->name = strdup(p_passwd->pw_name);
				next_r->stat = MAP_RES_OK;
			}
			break;
		case MAP_REQ_GID:
			p_group = getgrgid((gid_t)a->id);
			if (p_group) {
				next_r->name = strdup(p_group->gr_name);
				next_r->stat = MAP_RES_OK;
			}
			break;
		case MAP_REQ_UNAME:
			next_r->name = strdup(a->name);
			p_passwd = getpwnam(a->name);
			if (p_passwd) {
				next_r->id = p_passwd->pw_uid;
				next_r->stat = MAP_RES_OK;
			}
			break;
		case MAP_REQ_GNAME:
			next_r->name = strdup(a->name);
			p_group = getgrnam(a->name);
			if (p_group) {
				next_r->id = p_group->gr_gid;
				next_r->stat = MAP_RES_OK;
			}
			break;
		}
		if (next_r->name == NULL)
			next_r->name = strdup("");
		a = a->mapreq_next;
	}

	res.cm = &no_comment[0];

	return (&res);
}

	
/*ARGSUSED*/
v2_alert_results *pcnfsd2_alert_2_svc(arg, req)
v2_alert_args *arg;
struct svc_req *req;
{
static v2_alert_results res;

	res.stat = ALERT_RES_FAIL;
	res.cm = &not_supported[0];

	return (&res);
}

/*ARGSUSED*/
v2_info_results *pcnfsd2_info_2_svc(arg, req)
v2_info_args *arg;
struct svc_req *req;
{
static v2_info_results res;
static int facilities[FACILITIESMAX];
static int onetime = 1;

#define UNSUPPORTED -1
#define QUICK 100
#define SLOW 2000

	if (onetime) {
		onetime = 0;
		facilities[PCNFSD2_NULL] = QUICK;
		facilities[PCNFSD2_INFO] = QUICK;
		facilities[PCNFSD2_PR_INIT] = QUICK;
		facilities[PCNFSD2_PR_START] = SLOW;
		facilities[PCNFSD2_PR_LIST] = QUICK; /* except first time */
		facilities[PCNFSD2_PR_QUEUE] = SLOW;
		facilities[PCNFSD2_PR_STATUS] = SLOW;
		facilities[PCNFSD2_PR_CANCEL] = SLOW;
		facilities[PCNFSD2_PR_ADMIN] = UNSUPPORTED;
		facilities[PCNFSD2_PR_REQUEUE] = UNSUPPORTED;
		facilities[PCNFSD2_PR_HOLD] = UNSUPPORTED;
		facilities[PCNFSD2_PR_RELEASE] = UNSUPPORTED;
		facilities[PCNFSD2_MAPID] = QUICK;
		facilities[PCNFSD2_AUTH] = QUICK;
		facilities[PCNFSD2_ALERT] = QUICK;
	}
	res.facilities.facilities_len = PCNFSD2_ALERT+1;
	res.facilities.facilities_val = facilities;
	
	res.vers = &pcnfsd_version[0];
	res.cm = &no_comment[0];

	return (&res);
}



static void
fillin_extra_groups(uname, main_gid, len, extra_gids)
char *uname;
u_int main_gid;
int *len;
u_int extra_gids[EXTRAGIDLEN];
{
struct group *grp;
char **members;
int n = 0;

	setgrent();

	while (n < EXTRAGIDLEN) {
		grp = getgrent();
		if (grp == NULL)
			break;
		if (grp->gr_gid == main_gid)
			continue;
		for (members = grp->gr_mem; members && *members; members++) {
			if (!strcmp(*members, uname)) {
				extra_gids[n++] = grp->gr_gid;
				break;
			}
		}
	}
	endgrent();
	*len = n;
}
