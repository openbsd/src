/*	$OpenBSD: parse.c,v 1.7 1998/03/31 06:41:05 millert Exp $	*/

/*
 *  CU sudo version 1.5.5
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 * parse.c -- sudo parser frontend and comparison routines.
 *
 * Chris Jepeway <jepeway@cs.utk.edu>
 */

#ifndef lint
static char rcsid[] = "Id: parse.c,v 1.88 1998/03/31 05:05:40 millert Exp $";
#endif /* lint */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#  include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#  include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif /* HAVE_STRINGS_H */
#if defined(HAVE_FNMATCH) && defined(HAVE_FNMATCH_H)
#  include <fnmatch.h>
#else
#  ifndef HAVE_FNMATCH
#    include "emul/fnmatch.h"
#  endif /* HAVE_FNMATCH */
#endif /* HAVE_FNMATCH_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#  include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_NETGROUP_H
#  include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
#endif

#include "sudo.h"
#include <options.h>

/*
 * Globals
 */
int parse_error = FALSE;
extern FILE *yyin, *yyout;
extern int printmatches;

/*
 * Prototypes
 */
static int has_meta	__P((char *));
       void init_parser	__P((void));

/*
 * This routine is called from the sudo.c module and tries to validate
 * the user, host and command triplet.
 */
int validate(check_cmnd)
    int check_cmnd;
{
    int return_code;

    /* become sudoers file owner */
    set_perms(PERM_SUDOERS, 0);

    /* we opened _PATH_SUDO_SUDOERS in check_sudoers() so just rewind it */
    rewind(sudoers_fp);
    yyin = sudoers_fp;
    yyout = stdout;

    /*
     * Allocate space for data structures in the parser.
     */
    init_parser();

    /*
     * Need to be root while stat'ing things in the parser.
     */
    set_perms(PERM_ROOT, 0);
    return_code = yyparse();

    /*
     * Don't need to keep this open...
     */
    (void) fclose(sudoers_fp);
    sudoers_fp = NULL;

    /* relinquish extra privs */
    set_perms(PERM_USER, 0);

    if (return_code || parse_error)
	return(VALIDATE_ERROR);

    /*
     * Nothing on the top of the stack => user doesn't appear in sudoers.
     * Allow anyone to try the psuedo commands "list" and "validate".
     */
    if (top == 0) {
	if (check_cmnd == TRUE)
	    return(VALIDATE_NO_USER);
	else
	    return(VALIDATE_NOT_OK);
    }

    /*
     * Only check the actual command if the check_cmnd
     * flag is set.  It is not set for the "validate"
     * and "list" pseudo-commands.  Always check the
     * host and user.
     */
    if (check_cmnd == FALSE)
	while (top) {
	    if (host_matches == TRUE)
		/* user may always do validate or list on allowed hosts */
		if (no_passwd == TRUE)
		    return(VALIDATE_OK_NOPASS);
		else
		    return(VALIDATE_OK);
	    top--;
	}
    else
	while (top) {
	    if (host_matches == TRUE) {
		if (cmnd_matches == TRUE) {
		   if (runas_matches == TRUE) {
		    	/*
			 * User was granted access to cmnd on host.
		    	 * If no passwd required return as such.
			 */
		    	if (no_passwd == TRUE)
			    return(VALIDATE_OK_NOPASS);
		    	else
			    return(VALIDATE_OK);
		    }
		} else if (cmnd_matches == FALSE) {
		    /* User was explicitly denied acces to cmnd on host. */
		    return(VALIDATE_NOT_OK);
		}
	    }
	    top--;
	}

    /*
     * we popped everything off the stack =>
     * user was mentioned, but not explicitly
     * granted nor denied access => say no
     */
    return(VALIDATE_NOT_OK);
}



/*
 * If path doesn't end in /, return TRUE iff cmnd & path name the same inode;
 * otherwise, return TRUE if cmnd names one of the inodes in path.
 */
int command_matches(cmnd, user_args, path, sudoers_args)
    char *cmnd;
    char *user_args;
    char *path;
    char *sudoers_args;
{
    int plen;
    struct stat pst;
    DIR *dirp;
    struct dirent *dent;
    char buf[MAXPATHLEN+1];
    static char *c;

    /* don't bother with pseudo commands like "validate" */
    if (strchr(cmnd, '/') == NULL)
	return(FALSE);

    /* only need to stat cmnd once since it never changes */
    if (cmnd_st.st_dev == 0) {
	if (stat(cmnd, &cmnd_st) < 0)
	    return(FALSE);
	if ((c = strrchr(cmnd, '/')) == NULL)
	    c = cmnd;
	else
	    c++;
    }

    /*
     * If the pathname has meta characters in it use fnmatch(3)
     * to do the matching
     */
    if (has_meta(path)) {
	/*
	 * Return true if fnmatch(3) succeeds and there are no args
	 * (in sudoers or command) or if the args match;
	 * else return false.
	 */
	if (fnmatch(path, cmnd, FNM_PATHNAME))
	    return(FALSE);
	if (!sudoers_args)
	    return(TRUE);
	else if (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args))
	    return(TRUE);
	else if (sudoers_args)
	    return((fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0));
	else
	    return(FALSE);
    } else {
	plen = strlen(path);
	if (path[plen - 1] != '/') {
#ifdef FAST_MATCH
	    char *p;

	    /* Only proceed if the basenames of cmnd and path are the same */
	    if ((p = strrchr(path, '/')) == NULL)
		p = path;
	    else
		p++;
	    if (strcmp(c, p))
		return(FALSE);
#endif /* FAST_MATCH */

	    if (stat(path, &pst) < 0)
		return(FALSE);

	    /*
	     * Return true if inode/device matches and there are no args
	     * (in sudoers or command) or if the args match;
	     * else return false.
	     */
	    if (cmnd_st.st_dev != pst.st_dev || cmnd_st.st_ino != pst.st_ino)
		return(FALSE);
	    if (!sudoers_args)
		return(TRUE);
	    else if (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args))
		return(TRUE);
	    else if (sudoers_args)
		return((fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0));
	    else
		return(FALSE);
	}

	/*
	 * Grot through path's directory entries, looking for cmnd.
	 */
	dirp = opendir(path);
	if (dirp == NULL)
	    return(FALSE);

	while ((dent = readdir(dirp)) != NULL) {
	    strcpy(buf, path);
	    strcat(buf, dent->d_name);
#ifdef FAST_MATCH
	    /* only stat if basenames are not the same */
	    if (strcmp(c, dent->d_name))
		continue;
#endif /* FAST_MATCH */
	    if (stat(buf, &pst) < 0)
		continue;
	    if (cmnd_st.st_dev == pst.st_dev && cmnd_st.st_ino == pst.st_ino)
		break;
	}

	closedir(dirp);
	return(dent != NULL);
    }
}



/*
 * Returns TRUE if "n" is one of our ip addresses or if
 * "n" is a network that we are on, else returns FALSE.
 */
int addr_matches(n)
    char *n;
{
    int i;
    char *m;
    struct in_addr addr, mask;

    /* If there's an explicate netmask, use it. */
    if ((m = strchr(n, '/'))) {
	*m++ = '\0';
	mask.s_addr = inet_addr(m);
	addr.s_addr = inet_addr(n);
	*(m - 1) = '/';

	for (i = 0; i < num_interfaces; i++)
	    if ((interfaces[i].addr.s_addr & mask.s_addr) == addr.s_addr)
		return(TRUE);
    } else {
	addr.s_addr = inet_addr(n);

	for (i = 0; i < num_interfaces; i++)
	    if (interfaces[i].addr.s_addr == addr.s_addr ||
		(interfaces[i].addr.s_addr & interfaces[i].netmask.s_addr)
		== addr.s_addr)
		return(TRUE);
    }

    return(FALSE);
}



/*
 *  Returns TRUE if the given user belongs to the named group,
 *  else returns FALSE.
 */
int usergr_matches(group, user)
    char *group;
    char *user;
{
    struct group *grpent;
    char **cur;

    /* make sure we have a valid usergroup, sudo style */
    if (*group++ != '%')
	return(FALSE);

    if ((grpent = getgrnam(group)) == NULL) 
	return(FALSE);

    /*
     * Check against user's real gid as well as group's user list
     */
    if (grpent->gr_gid == user_gid)
	return(TRUE);

    for (cur=grpent->gr_mem; *cur; cur++) {
	if (strcmp(*cur, user) == 0)
	    return(TRUE);
    }

    return(FALSE);
}



/*
 * Returns TRUE if "host" and "user" belong to the netgroup "netgr",
 * else return FALSE.  Either of "host" or "user" may be NULL
 * in which case that argument is not checked...
 */
int netgr_matches(netgr, host, user)
    char *netgr;
    char *host;
    char *user;
{
#ifdef HAVE_GETDOMAINNAME
    static char *domain = (char *) -1;
#else
    static char *domain = NULL;
#endif /* HAVE_GETDOMAINNAME */

    /* make sure we have a valid netgroup, sudo style */
    if (*netgr++ != '+')
	return(FALSE);

#ifdef HAVE_GETDOMAINNAME
    /* get the domain name (if any) */
    if (domain == (char *) -1) {
	if ((domain = (char *) malloc(MAXHOSTNAMELEN + 1)) == NULL) {
	    perror("malloc");
	    (void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	    exit(1);
	}

	if (getdomainname(domain, MAXHOSTNAMELEN + 1) != 0 || *domain == '\0') {
	    (void) free(domain);
	    domain = NULL;
	}
    }
#endif /* HAVE_GETDOMAINNAME */

#ifdef HAVE_INNETGR
    return(innetgr(netgr, host, user, domain));
#else
    return(FALSE);
#endif /* HAVE_INNETGR */
}



/*
 * Returns TRUE if "s" has shell meta characters in it,
 * else returns FALSE.
 */
static int has_meta(s)
    char *s;
{
    register char *t;
    
    for (t = s; *t; t++) {
	if (*t == '\\' || *t == '?' || *t == '*' || *t == '[' || *t == ']')
	    return(TRUE);
    }
    return(FALSE);
}
