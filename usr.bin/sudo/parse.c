/*
 * Copyright (c) 1996, 1998-2005, 2007
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#endif /* HAVE_FNMATCH */
#ifdef HAVE_EXTENDED_GLOB
# include <glob.h>
#endif /* HAVE_EXTENDED_GLOB */
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "sudo.h"
#include "parse.h"
#include "interfaces.h"

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */
#ifndef HAVE_EXTENDED_GLOB
# include "emul/glob.h"
#endif /* HAVE_EXTENDED_GLOB */

#ifndef lint
__unused static const char rcsid[] = "$Sudo: parse.c,v 1.160.2.12 2007/08/13 16:30:02 millert Exp $";
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
sudoers_lookup(pwflag)
    int pwflag;
{
    int error, nopass;

    /* We opened _PATH_SUDOERS in check_sudoers() so just rewind it. */
    rewind(sudoers_fp);
    yyin = sudoers_fp;
    yyout = stdout;

    /* Allocate space for data structures in the parser. */
    init_parser();

    /* Keep more state for pseudo-commands so that listpw and verifypw work */
    if (pwflag > 0)
	keepall = TRUE;

    /* Need to be runas user while stat'ing things in the parser. */
    set_perms(PERM_RUNAS);
    error = yyparse();

    /* Close the sudoers file now that we are done with it. */
    (void) fclose(sudoers_fp);
    sudoers_fp = NULL;

    if (error || parse_error) {
	set_perms(PERM_ROOT);
	return(VALIDATE_ERROR);
    }

    /*
     * Assume the worst.  If the stack is empty the user was
     * not mentioned at all.
     */
    if (def_authenticate)
	error = VALIDATE_NOT_OK;
    else
	error = VALIDATE_NOT_OK | FLAG_NOPASS;
    if (pwflag) {
	SET(error, FLAG_NO_CHECK);
    } else {
	SET(error, FLAG_NO_HOST);
	if (!top)
	    SET(error, FLAG_NO_USER);
    }

    /*
     * Only check the actual command if pwflag is not set.
     * It is set for the "validate", "list" and "kill" pseudo-commands.
     * Always check the host and user.
     */
    nopass = -1;
    if (pwflag) {
	int found;
	enum def_tupple pwcheck;

	pwcheck = (pwflag == -1) ? never : sudo_defs_table[pwflag].sd_un.tuple;

	if (pwcheck == always && def_authenticate)
	    nopass = FLAG_CHECK_USER;
	else if (pwcheck == never || !def_authenticate)
	    nopass = FLAG_NOPASS;
	found = 0;
	while (top) {
	    if (host_matches == TRUE) {
		found = 1;
		if (pwcheck == any && no_passwd == TRUE)
		    nopass = FLAG_NOPASS;
		else if (pwcheck == all && nopass != 0)
		    nopass = (no_passwd == TRUE) ? FLAG_NOPASS : 0;
	    }
	    top--;
	}
	if (found) {
	    set_perms(PERM_ROOT);
	    if (nopass == -1)
		nopass = 0;
	    return(VALIDATE_OK | nopass);
	}
    } else {
	while (top) {
	    if (host_matches == TRUE) {
		CLR(error, FLAG_NO_HOST);
		if (runas_matches == TRUE && cmnd_matches == TRUE) {
		    /*
		     * User was granted access to cmnd on host as user.
		     */
		    set_perms(PERM_ROOT);
		    return(VALIDATE_OK |
			(no_passwd == TRUE ? FLAG_NOPASS : 0) |
			(no_execve == TRUE ? FLAG_NOEXEC : 0) |
			(setenv_ok == TRUE ? FLAG_SETENV : 0));
		} else if ((runas_matches == TRUE && cmnd_matches == FALSE) ||
		    (runas_matches == FALSE && cmnd_matches == TRUE)) {
		    /*
		     * User was explicitly denied access to cmnd on host.
		     */
		    set_perms(PERM_ROOT);
		    return(VALIDATE_NOT_OK |
			(no_passwd == TRUE ? FLAG_NOPASS : 0) |
			(no_execve == TRUE ? FLAG_NOEXEC : 0) |
			(setenv_ok == TRUE ? FLAG_SETENV : 0));
		}
	    }
	    top--;
	}
    }
    set_perms(PERM_ROOT);

    /*
     * The user was neither explicitly granted nor denied access.
     */
    if (nopass == -1)
	nopass = 0;
    return(error | nopass);
}

/*
 * If path doesn't end in /, return TRUE iff cmnd & path name the same inode;
 * otherwise, return TRUE if user_cmnd names one of the inodes in path.
 */
int
command_matches(sudoers_cmnd, sudoers_args)
    char *sudoers_cmnd;
    char *sudoers_args;
{
    struct stat sudoers_stat;
    struct dirent *dent;
    char **ap, *base, buf[PATH_MAX];
    glob_t gl;
    DIR *dirp;

    /* Check for pseudo-commands */
    if (strchr(user_cmnd, '/') == NULL) {
	/*
	 * Return true if both sudoers_cmnd and user_cmnd are "sudoedit" AND
	 *  a) there are no args in sudoers OR
	 *  b) there are no args on command line and none req by sudoers OR
	 *  c) there are args in sudoers and on command line and they match
	 */
	if (strcmp(sudoers_cmnd, "sudoedit") != 0 ||
	    strcmp(user_cmnd, "sudoedit") != 0)
	    return(FALSE);
	if (!sudoers_args ||
	    (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
	    (sudoers_args &&
	     fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
	    efree(safe_cmnd);
	    safe_cmnd = estrdup(sudoers_cmnd);
	    return(TRUE);
	} else
	    return(FALSE);
    }

    /*
     * If sudoers_cmnd has meta characters in it, use fnmatch(3)
     * to do the matching.
     */
    if (has_meta(sudoers_cmnd)) {
	/*
	 * Return true if we find a match in the glob(3) results AND
	 *  a) there are no args in sudoers OR
	 *  b) there are no args on command line and none required by sudoers OR
	 *  c) there are args in sudoers and on command line and they match
	 * else return false.
	 *
	 * Could optimize patterns ending in "/*" to "/user_base"
	 */
#define GLOB_FLAGS	(GLOB_NOSORT | GLOB_MARK | GLOB_BRACE | GLOB_TILDE)
	if (glob(sudoers_cmnd, GLOB_FLAGS, NULL, &gl) != 0) {
	    globfree(&gl);
	    return(FALSE);
	}
	/* For each glob match, compare basename, st_dev and st_ino. */
	for (ap = gl.gl_pathv; *ap != NULL; ap++) {
	    /* only stat if basenames are the same */
	    if ((base = strrchr(*ap, '/')) != NULL)
		base++;
	    else
		base = *ap;
	    if (strcmp(user_base, base) != 0 ||
		stat(*ap, &sudoers_stat) == -1)
		continue;
	    if (user_stat->st_dev == sudoers_stat.st_dev &&
		user_stat->st_ino == sudoers_stat.st_ino) {
		efree(safe_cmnd);
		safe_cmnd = estrdup(*ap);
		break;
	    }
	}
	globfree(&gl);
	if (*ap == NULL)
	    return(FALSE);

	if (!sudoers_args ||
	    (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
	    (sudoers_args &&
	     fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
	    efree(safe_cmnd);
	    safe_cmnd = estrdup(user_cmnd);
	    return(TRUE);
	} else
	    return(FALSE);
    } else {
	size_t dlen = strlen(sudoers_cmnd);

	/*
	 * No meta characters
	 * Check to make sure this is not a directory spec (doesn't end in '/')
	 */
	if (sudoers_cmnd[dlen - 1] != '/') {
	    /* Only proceed if user_base and basename(sudoers_cmnd) match */
	    if ((base = strrchr(sudoers_cmnd, '/')) == NULL)
		base = sudoers_cmnd;
	    else
		base++;
	    if (strcmp(user_base, base) != 0 ||
		stat(sudoers_cmnd, &sudoers_stat) == -1)
		return(FALSE);

	    /*
	     * Return true if inode/device matches AND
	     *  a) there are no args in sudoers OR
	     *  b) there are no args on command line and none req by sudoers OR
	     *  c) there are args in sudoers and on command line and they match
	     */
	    if (user_stat->st_dev != sudoers_stat.st_dev ||
		user_stat->st_ino != sudoers_stat.st_ino)
		return(FALSE);
	    if (!sudoers_args ||
		(!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
		(sudoers_args &&
		 fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
		efree(safe_cmnd);
		safe_cmnd = estrdup(sudoers_cmnd);
		return(TRUE);
	    } else
		return(FALSE);
	}

	/*
	 * Grot through sudoers_cmnd's directory entries, looking for user_base.
	 */
	dirp = opendir(sudoers_cmnd);
	if (dirp == NULL)
	    return(FALSE);

	if (strlcpy(buf, sudoers_cmnd, sizeof(buf)) >= sizeof(buf))
	    return(FALSE);
	while ((dent = readdir(dirp)) != NULL) {
	    /* ignore paths > PATH_MAX (XXX - log) */
	    buf[dlen] = '\0';
	    if (strlcat(buf, dent->d_name, sizeof(buf)) >= sizeof(buf))
		continue;

	    /* only stat if basenames are the same */
	    if (strcmp(user_base, dent->d_name) != 0 ||
		stat(buf, &sudoers_stat) == -1)
		continue;
	    if (user_stat->st_dev == sudoers_stat.st_dev &&
		user_stat->st_ino == sudoers_stat.st_ino) {
		efree(safe_cmnd);
		safe_cmnd = estrdup(buf);
		break;
	    }
	}

	closedir(dirp);
	return(dent != NULL);
    }
}

static int
addr_matches_if(n)
    char *n;
{
    int i;
    struct in_addr addr;
    struct interface *ifp;
#ifdef AF_INET6
    struct in6_addr addr6;
    int j;
#endif
    int family = AF_UNSPEC;

#ifdef AF_INET6
    if (inet_pton(AF_INET6, n, &addr6) > 0) {
	family = AF_INET6;
    } else
#else
    {
	family = AF_INET;
	addr.s_addr = inet_addr(n);
    }
#endif

    for (i = 0; i < num_interfaces; i++) {
	ifp = &interfaces[i];
	if (ifp->family != family)
	    continue;
	switch(family) {
	    case AF_INET:
		if (ifp->addr.ip4.s_addr == addr.s_addr ||
		    (ifp->addr.ip4.s_addr & ifp->netmask.ip4.s_addr)
		    == addr.s_addr)
		    return(TRUE);
		break;
#ifdef AF_INET6
	    case AF_INET6:
		if (memcmp(ifp->addr.ip6.s6_addr, addr6.s6_addr,
		    sizeof(addr6.s6_addr)) == 0)
		    return(TRUE);
		for (j = 0; j < sizeof(addr6.s6_addr); j++) {
		    if ((ifp->addr.ip6.s6_addr[j] & ifp->netmask.ip6.s6_addr[j]) != addr6.s6_addr[j])
			break;
		}
		if (j == sizeof(addr6.s6_addr))
		    return(TRUE);
#endif /* AF_INET6 */
	}
    }

    return(FALSE);
}

static int
addr_matches_if_netmask(n, m)
    char *n;
    char *m;
{
    int i;
    struct in_addr addr, mask;
    struct interface *ifp;
#ifdef AF_INET6
    struct in6_addr addr6, mask6;
    int j;
#endif
    int family = AF_UNSPEC;

#ifdef AF_INET6
    if (inet_pton(AF_INET6, n, &addr6) > 0)
	family = AF_INET6;
    else
#else
    {
	family = AF_INET;
	addr.s_addr = inet_addr(n);
    }
#endif

    if (family == AF_INET) {
	if (strchr(m, '.'))
	    mask.s_addr = inet_addr(m);
	else {
	    i = 32 - atoi(m);
	    mask.s_addr = 0xffffffff;
	    mask.s_addr >>= i;
	    mask.s_addr <<= i;
	    mask.s_addr = htonl(mask.s_addr);
	}
    }
#ifdef AF_INET6
    else {
	if (inet_pton(AF_INET6, m, &mask6) <= 0) {
	    j = atoi(m);
	    for (i = 0; i < 16; i++) {
		if (j < i * 8)
		    mask6.s6_addr[i] = 0;
		else if (i * 8 + 8 <= j)
		    mask6.s6_addr[i] = 0xff;
		else
		    mask6.s6_addr[i] = 0xff00 >> (j - i * 8);
	    }
	}
    }
#endif /* AF_INET6 */

    for (i = 0; i < num_interfaces; i++) {
	ifp = &interfaces[i];
	if (ifp->family != family)
	    continue;
	switch(family) {
	    case AF_INET:
		if ((ifp->addr.ip4.s_addr & mask.s_addr) == addr.s_addr)
		    return(TRUE);
#ifdef AF_INET6
	    case AF_INET6:
		for (j = 0; j < sizeof(addr6.s6_addr); j++) {
		    if ((ifp->addr.ip6.s6_addr[j] & mask6.s6_addr[j]) != addr6.s6_addr[j])
			break;
		}
		if (j == sizeof(addr6.s6_addr))
		    return(TRUE);
#endif /* AF_INET6 */
	}
    }

    return(FALSE);
}

/*
 * Returns TRUE if "n" is one of our ip addresses or if
 * "n" is a network that we are on, else returns FALSE.
 */
int
addr_matches(n)
    char *n;
{
    char *m;
    int retval;

    /* If there's an explicit netmask, use it. */
    if ((m = strchr(n, '/'))) {
	*m++ = '\0';
	retval = addr_matches_if_netmask(n, m);
	*(m - 1) = '/';
    } else
	retval = addr_matches_if(n);

    return(retval);
}

/*
 * Returns 0 if the hostname matches the pattern and non-zero otherwise.
 */
int
hostname_matches(shost, lhost, pattern)
    char *shost;
    char *lhost;
    char *pattern;
{
    if (has_meta(pattern)) {
	if (strchr(pattern, '.'))
	    return(fnmatch(pattern, lhost, FNM_CASEFOLD));
	else
	    return(fnmatch(pattern, shost, FNM_CASEFOLD));
    } else {
	if (strchr(pattern, '.'))
	    return(strcasecmp(lhost, pattern));
	else
	    return(strcasecmp(shost, pattern));
    }
}

/*
 *  Returns TRUE if the user/uid from sudoers matches the specified user/uid,
 *  else returns FALSE.
 */
int
userpw_matches(sudoers_user, user, pw)
    char *sudoers_user;
    char *user;
    struct passwd *pw;
{
    if (pw != NULL && *sudoers_user == '#') {
	uid_t uid = atoi(sudoers_user + 1);
	if (uid == pw->pw_uid)
	    return(1);
    }
    return(strcmp(sudoers_user, user) == 0);
}

/*
 *  Returns TRUE if the given user belongs to the named group,
 *  else returns FALSE.
 *  XXX - reduce the number of passwd/group lookups
 */
int
usergr_matches(group, user, pw)
    char *group;
    char *user;
    struct passwd *pw;
{
    struct group *grp;
    gid_t pw_gid;
    char **cur;
    int i;

    /* make sure we have a valid usergroup, sudo style */
    if (*group++ != '%')
	return(FALSE);

    /* look up user's primary gid in the passwd file */
    if (pw == NULL && (pw = getpwnam(user)) == NULL)
	return(FALSE);
    pw_gid = pw->pw_gid;

    if ((grp = getgrnam(group)) == NULL)
	return(FALSE);

    /* check against user's primary (passwd file) gid */
    if (grp->gr_gid == pw_gid)
	return(TRUE);

    /*
     * If the user has a supplementary group vector, check it first.
     */
    for (i = 0; i < user_ngroups; i++) {
	if (grp->gr_gid == user_groups[i])
	    return(TRUE);
    }
    if (grp->gr_mem != NULL) {
	for (cur = grp->gr_mem; *cur; cur++) {
	    if (strcmp(*cur, user) == 0)
		return(TRUE);
	}
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
    static char *domain;
#ifdef HAVE_GETDOMAINNAME
    static int initialized;
#endif

    /* make sure we have a valid netgroup, sudo style */
    if (*netgr++ != '+')
	return(FALSE);

#ifdef HAVE_GETDOMAINNAME
    /* get the domain name (if any) */
    if (!initialized) {
	domain = (char *) emalloc(MAXHOSTNAMELEN);
	if (getdomainname(domain, MAXHOSTNAMELEN) == -1 || *domain == '\0') {
	    efree(domain);
	    domain = NULL;
	}
	initialized = 1;
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
    char *t;
 
    for (t = s; *t; t++) {
	if (*t == '\\' || *t == '?' || *t == '*' || *t == '[' || *t == ']')
	    return(TRUE);
    }
    return(FALSE);
}
