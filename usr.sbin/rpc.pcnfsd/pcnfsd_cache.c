/*	$OpenBSD: pcnfsd_cache.c,v 1.6 2003/02/15 12:15:04 deraadt Exp $	*/
/*	$NetBSD: pcnfsd_cache.c,v 1.2 1995/07/25 22:20:37 gwr Exp $	*/

/*
 *=====================================================================
 * Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
 *	@(#)pcnfsd_cache.c	1.1	9/3/91
 *
 * pcnfsd is copyrighted software, but is freely licensed. This
 * means that you are free to redistribute it, modify it, ship it
 * in binary with your system, whatever, provided:
 *
 * - you leave the Sun copyright notice in the source code
 * - you make clear what changes you have introduced and do
 *   not represent them as being supported by Sun.
 *
 * If you make changes to this software, we ask that you do so in
 * a way which allows you to build either the "standard" version or
 * your custom version from a single source file. Test it, lint
 * it (it won't lint 100%, very little does, and there are bugs in
 * some versions of lint :-), and send it back to Sun via email
 * so that we can roll it into the source base and redistribute
 * it. We'll try to make sure your contributions are acknowledged
 * in the source, but after all these years it's getting hard to
 * remember who did what.
 *=====================================================================
 */

#include <stdio.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "pcnfsd.h"

#define CACHE_SIZE 16		/* keep it small, as linear searches are
				 * done */
struct cache 
{
       int   cuid;
       int   cgid;
       char  cpw[_PASSWORD_LEN];
       char  cuname[10];	/* keep this even for machines
				 * with alignment problems */
} User_cache[CACHE_SIZE];

int
check_cache(name, pw, p_uid, p_gid)
	char *name, *pw;
	int *p_uid, *p_gid;
{
	int i, c1, c2;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (!strcmp(User_cache[i].cuname, name)) {
           		c1 = strlen(pw);
	       		c2 = strlen(User_cache[i].cpw);
	        	if ((!c1 && !c2) ||
	  	       	    !(strcmp(User_cache[i].cpw,
		       	           crypt(pw, User_cache[i].cpw)))) {
		        	*p_uid = User_cache[i].cuid;
		        	*p_gid = User_cache[i].cgid;
		        	return (1);
		    	}
		    	User_cache[i].cuname[0] = '\0'; /* nuke entry */
           		return (0);
       		}
	}
	return (0);
}

void
add_cache_entry(p)
	struct passwd *p;
{
	int i;

	for (i = CACHE_SIZE - 1; i > 0; i--)
		User_cache[i] = User_cache[i - 1];
	User_cache[0].cuid = p->pw_uid;
	User_cache[0].cgid = p->pw_gid;
	(void)strncpy(User_cache[0].cpw, p->pw_passwd, sizeof User_cache[0].cpw-1);
	User_cache[0].cpw[sizeof User_cache[0].cpw-1] = '\0';
	(void)strncpy(User_cache[0].cuname, p->pw_name, sizeof User_cache[0].cuname-1);
	User_cache[0].cuname[sizeof User_cache[0].cuname-1] = '\0';
}
