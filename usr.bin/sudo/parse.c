/*
 * Copyright (c) 1996, 1998-2000 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Chris Jepeway
 * <jepeway@cs.utk.edu>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#endif /* HAVE_FNMATCH_H */
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
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
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "sudo.h"
#include "parse.h"
#include "interfaces.h"

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */

#ifndef lint
static const char rcsid[] = "$Sudo: parse.c,v 1.127 2000/01/17 23:46:25 millert Exp $";
#endif /* lint */

/*
 * Globals
 */
int parse_error = FALSE;
extern int keepall;
extern FILE *yyin, *yyout;

/*
 * Prototypes
 */
static int has_meta	__P((char *));
       void init_parser	__P((void));

/*
 * Look up the user in the sudoers file and check to see if they are
 * allowed to run the specified command on this host as the target user.
 */
int
sudoers_lookup(pwflags)
    int pwflags;
{
    int error;

    /* Become sudoers file owner */
    set_perms(PERM_SUDOERS, 0);

    /* We opened _PATH_SUDOERS in check_sudoers() so just rewind it. */
    rewind(sudoers_fp);
    yyin = sudoers_fp;
    yyout = stdout;

    /* Allocate space for data structures in the parser. */
    init_parser();

    /* For most pwflags to be useful we need to keep more state around. */
    if (pwflags && pwflags != PWCHECK_NEVER && pwflags != PWCHECK_ALWAYS)
	keepall = TRUE;

    /* Need to be root while stat'ing things in the parser. */
    set_perms(PERM_ROOT, 0);
    error = yyparse();

    /* Close the sudoers file now that we are done with it. */
    (void) fclose(sudoers_fp);
    sudoers_fp = NULL;

    if (error || parse_error)
	return(VALIDATE_ERROR);

    /*
     * Assume the worst.  If the stack is empty the user was
     * not mentioned at all.
     */
    if (def_flag(I_AUTHENTICATE))
	error = VALIDATE_NOT_OK;
    else
	error = VALIDATE_NOT_OK | FLAG_NOPASS;
    if (pwflags) {
	error |= FLAG_NO_CHECK;
    } else {
	error |= FLAG_NO_HOST;
	if (!top)
	    error |= FLAG_NO_USER;
    }

    /*
     * Only check the actual command if pwflags flag is not set.
     * It is set for the "validate", "list" and "kill" pseudo-commands.
     * Always check the host and user.
     */
    if (pwflags) {
	int nopass, found;

	if (pwflags == PWCHECK_NEVER || !def_flag(I_AUTHENTICATE))
	    nopass = FLAG_NOPASS;
	else
	    nopass = -1;
	found = 0;
	while (top) {
	    if (host_matches == TRUE) {
		found = 1;
		if (pwflags == PWCHECK_ANY && no_passwd == TRUE)
		    nopass = FLAG_NOPASS;
		else if (pwflags == PWCHECK_ALL && nopass != 0)
		    nopass = (no_passwd == TRUE) ? FLAG_NOPASS : 0;
	    }
	    top--;
	}
	if (found) {
	    if (nopass == -1)
		nopass = 0;
	    return(VALIDATE_OK | nopass);
	}
    } else {
	while (top) {
	    if (host_matches == TRUE) {
		error &= ~FLAG_NO_HOST;
		if (runas_matches == TRUE) {
		    if (cmnd_matches == TRUE) {
		    	/*
			 * User was granted access to cmnd on host.
		    	 * If no passwd required return as such.
			 */
		    	if (no_passwd == TRUE)
			    return(VALIDATE_OK | FLAG_NOPASS);
		    	else
			    return(VALIDATE_OK);
		    } else if (cmnd_matches == FALSE) {
			/*
			 * User was explicitly denied access to cmnd on host.
			 */
			if (no_passwd == TRUE)
			    return(VALIDATE_NOT_OK | FLAG_NOPASS);
			else
			    return(VALIDATE_NOT_OK);
		    }
		}
	    }
	    top--;
	}
    }

    /*
     * The user was not explicitly granted nor denied access.
     */
    return(error);
}

/*
 * If path doesn't end in /, return TRUE iff cmnd & path name the same inode;
 * otherwise, return TRUE if cmnd names one of the inodes in path.
 */
int
command_matches(cmnd, cmnd_args, path, sudoers_args)
    char *cmnd;
    char *cmnd_args;
    char *path;
    char *sudoers_args;
{
    int plen;
    static struct stat cst;
    struct stat pst;
    DIR *dirp;
    struct dirent *dent;
    char buf[MAXPATHLEN];
    static char *cmnd_base;

    /* Don't bother with pseudo commands like "validate" */
    if (strchr(cmnd, '/') == NULL)
	return(FALSE);

    plen = strlen(path);

    /* Only need to stat cmnd once since it never changes */
    if (cst.st_dev == 0) {
	if (stat(cmnd, &cst) == -1)
	    return(FALSE);
	if ((cmnd_base = strrchr(cmnd, '/')) == NULL)
	    cmnd_base = cmnd;
	else
	    cmnd_base++;
    }

    /*
     * If the pathname has meta characters in it use fnmatch(3)
     * to do the matching
     */
    if (has_meta(path)) {
	/*
	 * Return true if fnmatch(3) succeeds AND
	 *  a) there are no args in sudoers OR
	 *  b) there are no args on command line and none required by sudoers OR
	 *  c) there are args in sudoers and on command line and they match
	 * else return false.
	 */
	if (fnmatch(path, cmnd, FNM_PATHNAME) != 0)
	    return(FALSE);
	if (!sudoers_args ||
	    (!cmnd_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
	    (sudoers_args && fnmatch(sudoers_args, cmnd_args ? cmnd_args : "",
	    0) == 0)) {
	    if (safe_cmnd)
		free(safe_cmnd);
	    safe_cmnd = estrdup(user_cmnd);
	    return(TRUE);
	} else
	    return(FALSE);
    } else {
	/*
	 * No meta characters
	 * Check to make sure this is not a directory spec (doesn't end in '/')
	 */
	if (path[plen - 1] != '/') {
	    char *p;

	    /* Only proceed if the basenames of cmnd and path are the same */
	    if ((p = strrchr(path, '/')) == NULL)
		p = path;
	    else
		p++;
	    if (strcmp(cmnd_base, p) != 0 || stat(path, &pst) == -1)
		return(FALSE);

	    /*
	     * Return true if inode/device matches AND
	     *  a) there are no args in sudoers OR
	     *  b) there are no args on command line and none req by sudoers OR
	     *  c) there are args in sudoers and on command line and they match
	     */
	    if (cst.st_dev != pst.st_dev || cst.st_ino != pst.st_ino)
		return(FALSE);
	    if (!sudoers_args ||
		(!cmnd_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
		(sudoers_args &&
		 fnmatch(sudoers_args, cmnd_args ? cmnd_args : "", 0) == 0)) {
		if (safe_cmnd)
		    free(safe_cmnd);
		safe_cmnd = estrdup(path);
		return(TRUE);
	    } else
		return(FALSE);
	}

	/*
	 * Grot through path's directory entries, looking for cmnd.
	 */
	dirp = opendir(path);
	if (dirp == NULL)
	    return(FALSE);

	while ((dent = readdir(dirp)) != NULL) {
	    /* ignore paths > MAXPATHLEN (XXX - log) */
	    if (plen + NAMLEN(dent) >= sizeof(buf))
		continue;
	    strcpy(buf, path);
	    strcat(buf, dent->d_name);

	    /* only stat if basenames are the same */
	    if (strcmp(cmnd_base, dent->d_name) != 0 || stat(buf, &pst) == -1)
		continue;
	    if (cst.st_dev == pst.st_dev && cst.st_ino == pst.st_ino) {
		if (safe_cmnd)
		    free(safe_cmnd);
		safe_cmnd = estrdup(buf);
		break;
	    }
	}

	closedir(dirp);
	return(dent != NULL);
    }
}

/*
 * Returns TRUE if "n" is one of our ip addresses or if
 * "n" is a network that we are on, else returns FALSE.
 */
int
addr_matches(n)
    char *n;
{
    int i;
    char *m;
    struct in_addr addr, mask;

    /* If there's an explicit netmask, use it. */
    if ((m = strchr(n, '/'))) {
	*m++ = '\0';
	addr.s_addr = inet_addr(n);
	if (strchr(m, '.'))
	    mask.s_addr = inet_addr(m);
	else
	    mask.s_addr = (1 << atoi(m)) - 1;	/* XXX - better way? */
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
int
usergr_matches(group, user)
    char *group;
    char *user;
{
    struct group *grp;
    struct passwd *pw;
    char **cur;

    /* make sure we have a valid usergroup, sudo style */
    if (*group++ != '%')
	return(FALSE);

    if ((grp = getgrnam(group)) == NULL) 
	return(FALSE);

    /*
     * Check against user's real gid as well as group's user list
     */
    if ((pw = getpwnam(user)) == NULL)
	return(FALSE);

    if (grp->gr_gid == pw->pw_gid)
	return(TRUE);

    for (cur=grp->gr_mem; *cur; cur++) {
	if (strcmp(*cur, user) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

/*
 * Returns TRUE if "host" and "user" belong to the netgroup "netgr",
 * else return FALSE.  Either of "host", "shost" or "user" may be NULL
 * in which case that argument is not checked...
 */
int
netgr_matches(netgr, host, shost, user)
    char *netgr;
    char *host;
    char *shost;
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
	domain = (char *) emalloc(MAXHOSTNAMELEN);
	if (getdomainname(domain, MAXHOSTNAMELEN) == -1 || *domain == '\0') {
	    free(domain);
	    domain = NULL;
	}
    }
#endif /* HAVE_GETDOMAINNAME */

#ifdef HAVE_INNETGR
    if (innetgr(netgr, host, user, domain))
	return(TRUE);
    else if (host != shost && innetgr(netgr, shost, user, domain))
	return(TRUE);
#endif /* HAVE_INNETGR */

    return(FALSE);
}

/*
 * Returns TRUE if "s" has shell meta characters in it,
 * else returns FALSE.
 */
static int
has_meta(s)
    char *s;
{
    register char *t;
    
    for (t = s; *t; t++) {
	if (*t == '\\' || *t == '?' || *t == '*' || *t == '[' || *t == ']')
	    return(TRUE);
    }
    return(FALSE);
}
