/*
 * Copyright (c) 1996, 1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Chris Jepeway.
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

#define _SUDO_MAIN

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
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
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>

#include "sudo.h"
#include "parse.h"
#include "interfaces.h"

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */

#ifndef lint
static const char rcsid[] = "$Sudo: testsudoers.c,v 1.81 2003/04/02 18:25:19 millert Exp $";
#endif /* lint */


/*
 * Prototypes
 */
void init_parser	__P((void));
void dumpaliases	__P((void));
void set_perms_dummy	__P((int));

/*
 * Globals
 */
int  Argc, NewArgc;
char **Argv, **NewArgv;
int parse_error = FALSE;
int num_interfaces;
struct interface *interfaces;
struct sudo_user sudo_user;
extern int clearaliases;
extern int pedantic;
void (*set_perms) __P((int)) = set_perms_dummy;

/*
 * Returns TRUE if "s" has shell meta characters in it,
 * else returns FALSE.
 */
int
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

/*
 * Returns TRUE if cmnd matches, in the sudo sense,
 * the pathname in path; otherwise, return FALSE
 */
int
command_matches(cmnd, cmnd_args, path, sudoers_args)
    char *cmnd;
    char *cmnd_args;
    char *path;
    char *sudoers_args;
{
    int clen, plen;
    char *args;

    if (cmnd == NULL)
	return(FALSE);

    if ((args = strchr(path, ' ')))  
	*args++ = '\0';

    if (has_meta(path)) {
	if (fnmatch(path, cmnd, FNM_PATHNAME))
	    return(FALSE);
	if (!sudoers_args)
	    return(TRUE);
	else if (!cmnd_args && sudoers_args && !strcmp("\"\"", sudoers_args))
	    return(TRUE);
	else if (sudoers_args)
	    return((fnmatch(sudoers_args, cmnd_args ? cmnd_args : "", 0) == 0));
	else
	    return(FALSE);
    } else {
	plen = strlen(path);
	if (path[plen - 1] != '/') {
	    if (strcmp(cmnd, path))
		return(FALSE);
	    if (!sudoers_args)
		return(TRUE);
	    else if (!cmnd_args && sudoers_args && !strcmp("\"\"", sudoers_args))
		return(TRUE);
	    else if (sudoers_args)
		return((fnmatch(sudoers_args, cmnd_args ? cmnd_args : "", 0) == 0));
	    else
		return(FALSE);
	}

	clen = strlen(cmnd);
	if (clen < plen + 1)
	    /* path cannot be the parent dir of cmnd */
	    return(FALSE);

	if (strchr(cmnd + plen + 1, '/') != NULL)
	    /* path could only be an anscestor of cmnd -- */
	    /* ignoring, of course, things like // & /./  */
	    return(FALSE);

	/* see whether path is the prefix of cmnd */
	return((strncmp(cmnd, path, plen) == 0));
    }
}

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
	else {
	    i = 32 - atoi(m);
	    mask.s_addr = 0xffffffff;
	    mask.s_addr >>= i;
	    mask.s_addr <<= i;
	    mask.s_addr = htonl(mask.s_addr);
	}
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

int
usergr_matches(group, user)
    char *group;
    char *user;
{
    struct group *grp;
    char **cur;

    /* Make sure we have a valid usergroup, sudo style. */
    if (*group++ != '%')
	return(FALSE);

    if ((grp = getgrnam(group)) == NULL) 
	return(FALSE);

    /*
     * Check against user's real gid as well as group's user list
     */
    if (getgid() == grp->gr_gid)
	return(TRUE);

    for (cur=grp->gr_mem; *cur; cur++) {
	if (strcmp(*cur, user) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

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

    /* Make sure we have a valid netgroup, sudo style. */
    if (*netgr++ != '+')
	return(FALSE);

#ifdef HAVE_GETDOMAINNAME
    /* Get the domain name (if any). */
    if (domain == (char *) -1) {
	domain = (char *) emalloc(MAXHOSTNAMELEN);

	if (getdomainname(domain, MAXHOSTNAMELEN) != 0 || *domain == '\0') {
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

void
set_perms_dummy(i)
    int i;
{
    return;
}

void
set_fqdn()
{
    return;
}

void
init_envtables()
{
    return;
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    struct passwd pw;
    char *p;
#ifdef	YYDEBUG
    extern int yydebug;
    yydebug = 1;
#endif

    Argv = argv;
    Argc = argc;

    if (Argc >= 6 && strcmp(Argv[1], "-u") == 0) {
	user_runas = &Argv[2];
	pw.pw_name = Argv[3];
	user_host = Argv[4];
	user_cmnd = Argv[5];

	NewArgv = &Argv[5];
	NewArgc = Argc - 5;
    } else if (Argc >= 4) {
	pw.pw_name = Argv[1];
	user_host = Argv[2];
	user_cmnd = Argv[3];

	NewArgv = &Argv[3];
	NewArgc = Argc - 3;
    } else {
	(void) fprintf(stderr,
	    "usage: sudo [-u user] <user> <host> <command> [args]\n");
	exit(1);
    }

    sudo_user.pw = &pw;		/* user_name needs to be defined */

    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }

    /* Fill in cmnd_args from NewArgv. */
    if (NewArgc > 1) {
	char *to, **from;
	size_t size, n;

	size = (size_t) (NewArgv[NewArgc-1] - NewArgv[1]) +
		strlen(NewArgv[NewArgc-1]) + 1;
	user_args = (char *) emalloc(size);
	for (to = user_args, from = NewArgv + 1; *from; from++) {
	    n = strlcpy(to, *from, size - (to - user_args));
	    if (n >= size - (to - user_args))
		    errx(1, "internal error, init_vars() overflow");
	    to += n;
	    *to++ = ' ';
	}
	*--to = '\0';
    }

    /* Initialize default values. */
    init_defaults();

    /* Warn about aliases that are used before being defined. */
    pedantic = TRUE;

    /* Need to keep aliases around for dumpaliases(). */
    clearaliases = FALSE;

    /* Load ip addr/mask for each interface. */
    load_interfaces();

    /* Allocate space for data structures in the parser. */
    init_parser();

    if (yyparse() || parse_error) {
	(void) printf("doesn't parse.\n");
    } else {
	(void) printf("parses OK.\n\n");
	if (top == 0)
	    (void) printf("User %s not found\n", pw.pw_name);
	else while (top) {
	    (void) printf("[%d]\n", top-1);
	    (void) printf("user_match : %d\n", user_matches);
	    (void) printf("host_match : %d\n", host_matches);
	    (void) printf("cmnd_match : %d\n", cmnd_matches);
	    (void) printf("no_passwd  : %d\n", no_passwd);
	    (void) printf("runas_match: %d\n", runas_matches);
	    (void) printf("runas      : %s\n", *user_runas);
	    top--;
	}
    }

    /* Dump aliases. */
    (void) printf("Matching Aliases --\n");
    dumpaliases();

    exit(0);
}
