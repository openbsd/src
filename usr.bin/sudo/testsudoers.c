/*
 * Copyright (c) 1996, 1998-2005, 2007-2008
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

#define _SUDO_MAIN

#include <config.h>

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
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "sudo.h"
#include "interfaces.h"
#include "parse.h"
#include <gram.h>

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */

#ifndef lint
__unused static const char rcsid[] = "$Sudo: testsudoers.c,v 1.128 2008/11/19 17:01:20 millert Exp $";
#endif /* lint */


/*
 * Globals
 */
int  Argc, NewArgc;
char **Argv, **NewArgv;
int num_interfaces;
struct interface *interfaces;
struct sudo_user sudo_user;
struct passwd *list_pw;
extern int parse_error;

/* passwd/group redirection for pwutil.c */
void (*my_setgrent) __P((void)) = setgrent;
void (*my_endgrent) __P((void)) = endgrent;
struct group *(*my_getgrnam) __P((const char *)) = getgrnam;
struct group *(*my_getgrgid) __P((gid_t)) = getgrgid;
void (*my_setpwent) __P((void)) = setpwent;
void (*my_endpwent) __P((void)) = endpwent;
struct passwd *(*my_getpwnam) __P((const char *)) = getpwnam;
struct passwd *(*my_getpwuid) __P((uid_t)) = getpwuid;

/* For getopt(3) */
extern char *optarg;
extern int optind;

#if defined(SUDO_DEVEL) && defined(__OpenBSD__)
extern char *malloc_options;
#endif
#ifdef YYDEBUG
extern int yydebug;
#endif

int  print_alias __P((void *, void *));
void dump_sudoers __P((void));
void print_defaults __P((void));
void print_privilege __P((struct privilege *));
void print_userspecs __P((void));
void usage __P((void)) __attribute__((__noreturn__));
void set_runasgr __P((char *));
void set_runaspw __P((char *));

extern void ts_setgrfile __P((const char *));
extern void ts_setgrent __P((void));
extern void ts_endgrent __P((void));
extern struct group *ts_getgrent __P((void));
extern struct group *ts_getgrnam __P((const char *));
extern struct group *ts_getgrgid __P((gid_t));
extern void ts_setpwfile __P((const char *));
extern void ts_setpwent __P((void));
extern void ts_endpwent __P((void));
extern struct passwd *ts_getpwent __P((void));
extern struct passwd *ts_getpwnam __P((const char *));
extern struct passwd *ts_getpwuid __P((uid_t));

int
main(argc, argv)
    int argc;
    char **argv;
{
    struct cmndspec *cs;
    struct privilege *priv;
    struct userspec *us;
    char *p, *grfile, *pwfile, *runas_group, *runas_user;
    char hbuf[MAXHOSTNAMELEN + 1];
    int ch, dflag, rval, matched;

#if defined(SUDO_DEVEL) && defined(__OpenBSD__)
    malloc_options = "AFGJPR";
#endif
#ifdef YYDEBUG
    yydebug = 1;
#endif

    Argv = argv;
    Argc = argc;

    dflag = 0;
    grfile = pwfile = runas_group = runas_user = NULL;
    while ((ch = getopt(argc, argv, "dg:G:h:p:u:")) != -1) {
	switch (ch) {
	    case 'd':
		dflag = 1;
		break;
	    case 'h':
		user_host = optarg;
		break;
	    case 'G':
		grfile = optarg;
		break;
	    case 'g':
		runas_group = optarg;
		break;
	    case 'p':
		pwfile = optarg;
		break;
	    case 'u':
		runas_user = optarg;
		break;
	    default:
		usage();
		break;
	}
    }
    argc -= optind;
    argv += optind;
    NewArgc = argc;
    NewArgv = argv;

    /* Set group/passwd file and init the cache. */
    if (grfile) {
	my_setgrent = ts_setgrent;
	my_endgrent = ts_endgrent;
	my_getgrnam = ts_getgrnam;
	my_getgrgid = ts_getgrgid;
	ts_setgrfile(grfile);
    }
    if (pwfile) {
	my_setpwent = ts_setpwent;
	my_endpwent = ts_endpwent;
	my_getpwnam = ts_getpwnam;
	my_getpwuid = ts_getpwuid;
	ts_setpwfile(pwfile);
    }
    sudo_setpwent();
    sudo_setgrent();

    if (argc < 2) {
	if (!dflag)
	    usage();
	if ((sudo_user.pw = sudo_getpwnam("nobody")) == NULL)
            errorx(1, "no passwd entry for nobody!");
	user_cmnd = user_base = "true";
    } else {
	if ((sudo_user.pw = sudo_getpwnam(*argv)) == NULL)
            errorx(1, "no passwd entry for %s!", *argv);
	user_cmnd = *++argv;
	if ((p = strrchr(user_cmnd, '/')) != NULL)
	    user_base = p + 1;
	else
	    user_base = user_cmnd;
	NewArgc -= 2;
    }

    if (user_host == NULL) {
	if (gethostname(hbuf, sizeof(hbuf)) != 0)
	    error(1, "gethostname");
	hbuf[sizeof(hbuf) - 1] = '\0';
	user_host = hbuf;
    }
    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }

    /* Fill in user_args from NewArgv. */
    if (NewArgc > 1) {
	char *to, **from;
	size_t size, n;

	for (size = 0, from = NewArgv + 1; *from; from++)
	    size += strlen(*from) + 1;

	user_args = (char *) emalloc(size);
	for (to = user_args, from = NewArgv + 1; *from; from++) {
	    n = strlcpy(to, *from, size - (to - user_args));
	    if (n >= size - (to - user_args))
		    errorx(1, "internal error, init_vars() overflow");
	    to += n;
	    *to++ = ' ';
	}
	*--to = '\0';
    }

    /* Initialize default values. */
    init_defaults();

    /* Load ip addr/mask for each interface. */
    load_interfaces();

    /* Allocate space for data structures in the parser. */
    init_parser("sudoers", 0);

    if (yyparse() != 0 || parse_error)
	(void) fputs("Does not parse", stdout);
    else
	(void) fputs("Parses OK", stdout);

    if (!update_defaults(SETDEF_ALL))
	(void) fputs(" (problem with defaults entries)", stdout);
    puts(".");

    /*
     * Set runas passwd/group entries based on command line or sudoers.
     * Note that if runas_group was specified without runas_user we
     * defer setting runas_pw so the match routines know to ignore it.
     */
    if (runas_group != NULL) {
        set_runasgr(runas_group);
        if (runas_user != NULL)
            set_runaspw(runas_user);
    } else
        set_runaspw(runas_user ? runas_user : def_runas_default);

    if (dflag) {
	(void) putchar('\n');
	dump_sudoers();
	if (argc < 2)
	    exit(0);
    }

    /* This loop must match the one in sudoers_lookup() */
    printf("\nEntries for user %s:\n", user_name);
    matched = UNSPEC;
    tq_foreach_rev(&userspecs, us) {
	if (userlist_matches(sudo_user.pw, &us->users) != ALLOW)
	    continue;
	tq_foreach_rev(&us->privileges, priv) {
	    putchar('\n');
	    print_privilege(priv); /* XXX */
	    putchar('\n');
	    if (hostlist_matches(&priv->hostlist) == ALLOW) {
		puts("\thost  matched");
		tq_foreach_rev(&priv->cmndlist, cs) {
		    if (runaslist_matches(&cs->runasuserlist,
			&cs->runasgrouplist) == ALLOW) {
			puts("\trunas matched");
			rval = cmnd_matches(cs->cmnd);
			if (rval != UNSPEC)
			    matched = rval;
			printf("\tcmnd  %s\n", rval == ALLOW ? "allowed" :
			    rval == DENY ? "denied" : "unmatched");
		    }
		}
	    } else
		puts("\thost  unmatched");
	}
    }
    printf("\nCommand %s\n", matched == ALLOW ? "allowed" :
	matched == DENY ? "denied" : "unmatched");

    exit(0);
}

void
set_runaspw(user)
    char *user;
{
    if (*user == '#') {
	if ((runas_pw = sudo_getpwuid(atoi(user + 1))) == NULL)
	    runas_pw = sudo_fakepwnam(user, runas_gr ? runas_gr->gr_gid : 0);
    } else {
	if ((runas_pw = sudo_getpwnam(user)) == NULL)
	    errorx(1, "unknown user: %s", user);
    }
}

void
set_runasgr(group)
    char *group;
{
    if (*group == '#') {
	if ((runas_gr = sudo_getgrgid(atoi(group + 1))) == NULL)
	    runas_gr = sudo_fakegrnam(group);
    } else {
	if ((runas_gr = sudo_getgrnam(group)) == NULL)
	    errorx(1, "unknown group: %s", group);
    }
}

void
sudo_setspent()
{
    return;
}

void
sudo_endspent()
{
    return;
}

char *
sudo_getepw(pw)
    const struct passwd *pw;
{
    return (pw->pw_passwd);
}

void
set_fqdn()
{
    return;
}

FILE *
open_sudoers(path, keepopen)
    const char *path;
    int *keepopen;
{
    return(fopen(path, "r"));
}

void
init_envtables()
{
    return;
}

void
set_perms(perm)
    int perm;
{
    return;
}

void
cleanup(gotsignal)
    int gotsignal;
{
    if (!gotsignal) {
	sudo_endpwent();
	sudo_endgrent();
    }
}

void
print_member(m)    
    struct member *m;
{
    struct sudo_command *c;

    if (m->negated)
	putchar('!');
    if (m->name == NULL)
	fputs("ALL", stdout);
    else if (m->type != COMMAND)
	fputs(m->name, stdout);
    else {
	c = (struct sudo_command *) m->name;
	printf("%s%s%s", c->cmnd, c->args ? " " : "",
	    c->args ? c->args : "");
    }
}

void
print_defaults()
{
    struct defaults *d;
    struct member *m;

    tq_foreach_fwd(&defaults, d) {
	(void) fputs("Defaults", stdout);
	switch (d->type) {
	    case DEFAULTS_HOST:
		putchar('@');
		break;
	    case DEFAULTS_USER:
		putchar(':');
		break;
	    case DEFAULTS_RUNAS:
		putchar('>');
		break;
	    case DEFAULTS_CMND:
		putchar('!');
		break;
	}
	tq_foreach_fwd(&d->binding, m) {
	    if (m != tq_first(&d->binding))
		putchar(',');
	    print_member(m);
	}
	printf("\t%s%s", d->op == FALSE ? "!" : "", d->var);
	if (d->val != NULL) {
	    printf("%c%s", d->op == TRUE ? '=' : d->op, d->val);
	}
	putchar('\n');
    }
}

int
print_alias(v1, v2)
    void *v1, *v2;
{
    struct alias *a = (struct alias *)v1;
    struct member *m;
    struct sudo_command *c;

    switch (a->type) {
	case HOSTALIAS:
	    (void) printf("Host_Alias\t%s = ", a->name);
	    break;
	case CMNDALIAS:
	    (void) printf("Cmnd_Alias\t%s = ", a->name);
	    break;
	case USERALIAS:
	    (void) printf("User_Alias\t%s = ", a->name);
	    break;
	case RUNASALIAS:
	    (void) printf("Runas_Alias\t%s = ", a->name);
	    break;
    }
    tq_foreach_fwd(&a->members, m) {
	if (m != tq_first(&a->members))
	    fputs(", ", stdout);
	if (m->type == COMMAND) {
	    c = (struct sudo_command *) m->name;
	    printf("%s%s%s", c->cmnd, c->args ? " " : "",
		c->args ? c->args : "");
	} else
	    fputs(m->name, stdout);
    }
    putchar('\n');
    return(0);
}

void
print_privilege(priv)
    struct privilege *priv;
{
    struct cmndspec *cs;
    struct member *m;
    struct privilege *p;
    struct cmndtag tags;

    for (p = priv; p != NULL; p = p->next) {
	if (p != priv)
	    fputs(" : ", stdout);
	tq_foreach_fwd(&p->hostlist, m) {
	    if (m != tq_first(&p->hostlist))
		fputs(", ", stdout);
	    print_member(m);
	}
	fputs(" = ", stdout);
	tags.nopasswd = tags.noexec = UNSPEC;
	tq_foreach_fwd(&p->cmndlist, cs) {
	    if (cs != tq_first(&p->cmndlist))
		fputs(", ", stdout);
	    /* XXX - runasgrouplist too */
	    if (!tq_empty(&cs->runasuserlist)) {
		fputs("(", stdout);
		tq_foreach_fwd(&cs->runasuserlist, m) {
		    if (m != tq_first(&cs->runasuserlist))
			fputs(", ", stdout);
		    print_member(m);
		}
		fputs(") ", stdout);
	    }
#ifdef HAVE_SELINUX
	    if (cs->role)
		printf("ROLE=%s ", cs->role);
	    if (cs->type)
		printf("TYPE=%s ", cs->type);
#endif /* HAVE_SELINUX */
	    if (cs->tags.nopasswd != UNSPEC && cs->tags.nopasswd != tags.nopasswd)
		printf("%sPASSWD: ", cs->tags.nopasswd ? "NO" : "");
	    if (cs->tags.noexec != UNSPEC && cs->tags.noexec != tags.noexec)
		printf("%sEXEC: ", cs->tags.noexec ? "NO" : "");
	    print_member(cs->cmnd);
	    memcpy(&tags, &cs->tags, sizeof(tags));
	}
    }
}

void
print_userspecs()
{
    struct member *m;
    struct userspec *us;

    tq_foreach_fwd(&userspecs, us) {
	tq_foreach_fwd(&us->users, m) {
	    if (m != tq_first(&us->users))
		fputs(", ", stdout);
	    print_member(m);
	}
	putchar('\t');
	print_privilege(us->privileges.first); /* XXX */
	putchar('\n');
    }
}

void
dump_sudoers()
{
    print_defaults();

    putchar('\n');
    alias_apply(print_alias, NULL);

    putchar('\n');
    print_userspecs();
}

void
usage()
{
    (void) fprintf(stderr, "usage: %s [-d] [-G grfile] [-g group] [-h host] [-p pwfile] [-u user] <user> <command> [args]\n", getprogname());
    exit(1);
}
