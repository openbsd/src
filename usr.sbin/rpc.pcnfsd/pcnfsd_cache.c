/*	$NetBSD: pcnfsd_cache.c,v 1.2 1995/07/25 22:20:37 gwr Exp $	*/

/* RE_SID: @(%)/usr/dosnfs/shades_SCCS/unix/pcnfsd/v2/src/SCCS/s.pcnfsd_cache.c 1.1 91/09/03 12:45:14 SMI */
/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**	@(#)pcnfsd_cache.c	1.1	9/3/91
**=====================================================================
*/
#include "common.h"
/*
**=====================================================================
**             I N C L U D E   F I L E   S E C T I O N                *
**                                                                    *
** If your port requires different include files, add a suitable      *
** #define in the customization section, and make the inclusion or    *
** exclusion of the files conditional on this.                        *
**=====================================================================
*/
#include "pcnfsd.h"

#include <stdio.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>

extern char    *crypt();


/*
**---------------------------------------------------------------------
**                       Misc. variable definitions
**---------------------------------------------------------------------
*/

extern int      errno;

#ifdef USER_CACHE
#define CACHE_SIZE 16		/* keep it small, as linear searches are
				 * done */
struct cache 
       {
       int   cuid;
       int   cgid;
       char  cpw[_PASSWORD_LEN];
       char  cuname[10];	/* keep this even for machines
				 * with alignment problems */
       }User_cache[CACHE_SIZE];



/*
**---------------------------------------------------------------------
**                 User cache support procedures 
**---------------------------------------------------------------------
*/


int
check_cache(name, pw, p_uid, p_gid)
	char           *name;
   char           *pw;
   int            *p_uid;
   int            *p_gid;
{
	int             i;
   int             c1, c2;

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
	struct passwd  *p;
{
	int             i;

	for (i = CACHE_SIZE - 1; i > 0; i--)
		User_cache[i] = User_cache[i - 1];
	User_cache[0].cuid = p->pw_uid;
	User_cache[0].cgid = p->pw_gid;
	(void)strncpy(User_cache[0].cpw, p->pw_passwd, sizeof User_cache[0].cpw-1);
	User_cache[0].cpw[sizeof User_cache[0].cpw-1] = '\0';
	(void)strncpy(User_cache[0].cuname, p->pw_name, sizeof User_cache[0].cuname-1);
	User_cache[0].cuname[sizeof User_cache[0].cuname-1] = '\0';
}


#endif				/* USER_CACHE */


