/*
 * Copyright (c) 2004-2005, 2007-2009 Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
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
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#include "sudo.h"
#include "parse.h"
#include "lbuf.h"
#include <gram.h>

#ifndef lint
__unused static const char rcsid[] = "$Sudo: parse.c,v 1.242 2009/05/25 12:02:41 millert Exp $";
#endif /* lint */

/* Characters that must be quoted in sudoers */
#define SUDOERS_QUOTED	":\\,=#\""

/* sudoers nsswitch routines */
struct sudo_nss sudo_nss_file = {
    &sudo_nss_file,
    NULL,
    sudo_file_open,
    sudo_file_close,
    sudo_file_parse,
    sudo_file_setdefs,
    sudo_file_lookup,
    sudo_file_display_cmnd,
    sudo_file_display_defaults,
    sudo_file_display_bound_defaults,
    sudo_file_display_privs
};

/*
 * Parser externs.
 */
extern FILE *yyin;
extern char *errorfile;
extern int errorlineno, parse_error;

/*
 * Local prototypes.
 */
static void print_member	__P((struct lbuf *, char *, int, int, int));
static int display_bound_defaults __P((int, struct lbuf *));

int
sudo_file_open(nss)
    struct sudo_nss *nss;
{
    if (def_ignore_local_sudoers)
	return(-1);
    nss->handle = open_sudoers(_PATH_SUDOERS, FALSE, NULL);
    return(nss->handle ? 0 : -1);
}

int
sudo_file_close(nss)
    struct sudo_nss *nss;
{
    /* Free parser data structures and close sudoers file. */
    init_parser(NULL, 0);
    if (nss->handle != NULL) {
	fclose(nss->handle);
	nss->handle = NULL;
	yyin = NULL;
    }
    return(0);
}

/*
 * Parse the specified sudoers file.
 */
int
sudo_file_parse(nss)
    struct sudo_nss *nss;
{
    if (nss->handle == NULL)
	return(-1);

    init_parser(_PATH_SUDOERS, 0);
    yyin = nss->handle;
    if (yyparse() != 0 || parse_error) {
	log_error(NO_EXIT, "parse error in %s near line %d",
	    errorfile, errorlineno);
	return(-1);
    }
    return(0);
}

/*
 * Wrapper around update_defaults() for nsswitch code.
 */
int
sudo_file_setdefs(nss)
    struct sudo_nss *nss;
{
    if (nss->handle == NULL)
	return(-1);

    if (!update_defaults(SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER))
	return(-1);
    return(0);
}

/*
 * Look up the user in the parsed sudoers file and check to see if they are
 * allowed to run the specified command on this host as the target user.
 */
int
sudo_file_lookup(nss, validated, pwflag)
    struct sudo_nss *nss;
    int validated;
    int pwflag;
{
    int match, host_match, runas_match, cmnd_match;
    struct cmndspec *cs;
    struct cmndtag *tags = NULL;
    struct privilege *priv;
    struct userspec *us;

    if (nss->handle == NULL)
	return(validated);

    /*
     * Only check the actual command if pwflag is not set.
     * It is set for the "validate", "list" and "kill" pseudo-commands.
     * Always check the host and user.
     */
    if (pwflag) {
	int nopass;
	enum def_tupple pwcheck;

	pwcheck = (pwflag == -1) ? never : sudo_defs_table[pwflag].sd_un.tuple;
	nopass = (pwcheck == all) ? TRUE : FALSE;

	if (list_pw == NULL)
	    SET(validated, FLAG_NO_CHECK);
	CLR(validated, FLAG_NO_USER);
	CLR(validated, FLAG_NO_HOST);
	match = DENY;
	tq_foreach_fwd(&userspecs, us) {
	    if (userlist_matches(sudo_user.pw, &us->users) != ALLOW)
		continue;
	    tq_foreach_fwd(&us->privileges, priv) {
		if (hostlist_matches(&priv->hostlist) != ALLOW)
		    continue;
		tq_foreach_fwd(&priv->cmndlist, cs) {
		    /* Only check the command when listing another user. */
		    if (user_uid == 0 || list_pw == NULL ||
			user_uid == list_pw->pw_uid ||
			cmnd_matches(cs->cmnd) == ALLOW)
			    match = ALLOW;
		    if ((pwcheck == any && cs->tags.nopasswd == TRUE) ||
			(pwcheck == all && cs->tags.nopasswd != TRUE))
			nopass = cs->tags.nopasswd;
		}
	    }
	}
	if (match == ALLOW || user_uid == 0) {
	    /* User has an entry for this host. */
	    SET(validated, VALIDATE_OK);
	} else if (match == DENY)
	    SET(validated, VALIDATE_NOT_OK);
	if (pwcheck == always && def_authenticate)
	    SET(validated, FLAG_CHECK_USER);
	else if (pwcheck == never || nopass == TRUE)
	    def_authenticate = FALSE;
	return(validated);
    }

    /* Need to be runas user while stat'ing things. */
    set_perms(PERM_RUNAS);

    match = UNSPEC;
    tq_foreach_rev(&userspecs, us) {
	if (userlist_matches(sudo_user.pw, &us->users) != ALLOW)
	    continue;
	CLR(validated, FLAG_NO_USER);
	tq_foreach_rev(&us->privileges, priv) {
	    host_match = hostlist_matches(&priv->hostlist);
	    if (host_match == ALLOW)
		CLR(validated, FLAG_NO_HOST);
	    else
		continue;
	    tq_foreach_rev(&priv->cmndlist, cs) {
		runas_match = runaslist_matches(&cs->runasuserlist,
		    &cs->runasgrouplist);
		if (runas_match == ALLOW) {
		    cmnd_match = cmnd_matches(cs->cmnd);
		    if (cmnd_match != UNSPEC) {
			match = cmnd_match;
			tags = &cs->tags;
#ifdef HAVE_SELINUX
			/* Set role and type if not specified on command line. */
			if (user_role == NULL)
			    user_role = cs->role ? estrdup(cs->role) : def_role;
			if (user_type == NULL)
			    user_type = cs->type ? estrdup(cs->type) : def_type;
#endif /* HAVE_SELINUX */
			goto matched2;
		    }
		}
	    }
	}
    }
    matched2:
    if (match == ALLOW) {
	SET(validated, VALIDATE_OK);
	CLR(validated, VALIDATE_NOT_OK);
	if (tags != NULL) {
	    if (tags->nopasswd != UNSPEC)
		def_authenticate = !tags->nopasswd;
	    if (tags->noexec != UNSPEC)
		def_noexec = tags->noexec;
	    if (tags->setenv != UNSPEC)
		def_setenv = tags->setenv;
	}
    } else if (match == DENY) {
	SET(validated, VALIDATE_NOT_OK);
	CLR(validated, VALIDATE_OK);
    }
    set_perms(PERM_ROOT);
    return(validated);
}

#define	TAG_CHANGED(t) \
	(cs->tags.t != UNSPEC && cs->tags.t != IMPLIED && cs->tags.t != tags->t)

static void
sudo_file_append_cmnd(cs, tags, lbuf)
    struct cmndspec *cs;
    struct cmndtag *tags;
    struct lbuf *lbuf;
{
    struct member *m;

#ifdef HAVE_SELINUX
    if (cs->role)
	lbuf_append(lbuf, "ROLE=", cs->role, " ", NULL);
    if (cs->type)
	lbuf_append(lbuf, "TYPE=", cs->type, " ", NULL);
#endif /* HAVE_SELINUX */
    if (TAG_CHANGED(setenv)) {
	lbuf_append(lbuf, cs->tags.setenv ? "SETENV: " :
	    "NOSETENV: ", NULL);
	tags->setenv = cs->tags.setenv;
    }
    if (TAG_CHANGED(noexec)) {
	lbuf_append(lbuf, cs->tags.noexec ? "NOEXEC: " :
	    "EXEC: ", NULL);
	tags->noexec = cs->tags.noexec;
    }
    if (TAG_CHANGED(nopasswd)) {
	lbuf_append(lbuf, cs->tags.nopasswd ? "NOPASSWD: " :
	    "PASSWD: ", NULL);
	tags->nopasswd = cs->tags.nopasswd;
    }
    m = cs->cmnd;
    print_member(lbuf, m->name, m->type, m->negated,
	CMNDALIAS);
}

static int
sudo_file_display_priv_short(pw, us, lbuf)
    struct passwd *pw;
    struct userspec *us;
    struct lbuf *lbuf;
{
    struct cmndspec *cs;
    struct member *m;
    struct privilege *priv;
    struct cmndtag tags;
    int nfound = 0;

    tq_foreach_fwd(&us->privileges, priv) {
	if (hostlist_matches(&priv->hostlist) != ALLOW)
	    continue;
	tags.noexec = UNSPEC;
	tags.setenv = UNSPEC;
	tags.nopasswd = UNSPEC;
	lbuf_append(lbuf, "    ", NULL);
	tq_foreach_fwd(&priv->cmndlist, cs) {
	    if (cs != tq_first(&priv->cmndlist))
		lbuf_append(lbuf, ", ", NULL);
	    lbuf_append(lbuf, "(", NULL);
	    if (!tq_empty(&cs->runasuserlist)) {
		tq_foreach_fwd(&cs->runasuserlist, m) {
		    if (m != tq_first(&cs->runasuserlist))
			lbuf_append(lbuf, ", ", NULL);
		    print_member(lbuf, m->name, m->type, m->negated,
			RUNASALIAS);
		}
	    } else if (tq_empty(&cs->runasgrouplist)) {
		lbuf_append(lbuf, def_runas_default, NULL);
	    } else {
		lbuf_append(lbuf, pw->pw_name, NULL);
	    }
	    if (!tq_empty(&cs->runasgrouplist)) {
		lbuf_append(lbuf, " : ", NULL);
		tq_foreach_fwd(&cs->runasgrouplist, m) {
		    if (m != tq_first(&cs->runasgrouplist))
			lbuf_append(lbuf, ", ", NULL);
		    print_member(lbuf, m->name, m->type, m->negated,
			RUNASALIAS);
		}
	    }
	    lbuf_append(lbuf, ") ", NULL);
	    sudo_file_append_cmnd(cs, &tags, lbuf);
	    nfound++;
	}
	lbuf_print(lbuf);		/* forces a newline */
    }
    return(nfound);
}

static int
sudo_file_display_priv_long(pw, us, lbuf)
    struct passwd *pw;
    struct userspec *us;
    struct lbuf *lbuf;
{
    struct cmndspec *cs;
    struct member *m;
    struct privilege *priv;
    struct cmndtag tags;
    int nfound = 0;

    tq_foreach_fwd(&us->privileges, priv) {
	if (hostlist_matches(&priv->hostlist) != ALLOW)
	    continue;
	tags.noexec = UNSPEC;
	tags.setenv = UNSPEC;
	tags.nopasswd = UNSPEC;
	lbuf_print(lbuf);	/* force a newline */
	lbuf_append(lbuf, "Sudoers entry:", NULL);
	lbuf_print(lbuf);
	tq_foreach_fwd(&priv->cmndlist, cs) {
	    lbuf_append(lbuf, "    RunAsUsers: ", NULL);
	    if (!tq_empty(&cs->runasuserlist)) {
		tq_foreach_fwd(&cs->runasuserlist, m) {
		    if (m != tq_first(&cs->runasuserlist))
			lbuf_append(lbuf, ", ", NULL);
		    print_member(lbuf, m->name, m->type, m->negated,
			RUNASALIAS);
		}
	    } else if (tq_empty(&cs->runasgrouplist)) {
		lbuf_append(lbuf, def_runas_default, NULL);
	    } else {
		lbuf_append(lbuf, pw->pw_name, NULL);
	    }
	    lbuf_print(lbuf);
	    if (!tq_empty(&cs->runasgrouplist)) {
		lbuf_append(lbuf, "    RunAsGroups: ", NULL);
		tq_foreach_fwd(&cs->runasgrouplist, m) {
		    if (m != tq_first(&cs->runasgrouplist))
			lbuf_append(lbuf, ", ", NULL);
		    print_member(lbuf, m->name, m->type, m->negated,
			RUNASALIAS);
		}
		lbuf_print(lbuf);
	    }
	    lbuf_append(lbuf, "    Commands: ", NULL);
	    lbuf_print(lbuf);
	    lbuf_append(lbuf, "\t", NULL);
	    sudo_file_append_cmnd(cs, &tags, lbuf);
	    lbuf_print(lbuf);
	    nfound++;
	}
    }
    return(nfound);
}

int
sudo_file_display_privs(nss, pw, lbuf)
    struct sudo_nss *nss;
    struct passwd *pw;
    struct lbuf *lbuf;
{
    struct userspec *us;
    int nfound = 0;

    if (nss->handle == NULL)
	return(-1);

    tq_foreach_fwd(&userspecs, us) {
	if (userlist_matches(pw, &us->users) != ALLOW)
	    continue;

	if (long_list)
	    nfound += sudo_file_display_priv_long(pw, us, lbuf);
	else
	    nfound += sudo_file_display_priv_short(pw, us, lbuf);
    }
    return(nfound);
}

/*
 * Display matching Defaults entries for the given user on this host.
 */
int
sudo_file_display_defaults(nss, pw, lbuf)
    struct sudo_nss *nss;
    struct passwd *pw;
    struct lbuf *lbuf;
{
    struct defaults *d;
    char *prefix = NULL;
    int nfound = 0;

    if (nss->handle == NULL)
	return(-1);

    if (lbuf->len == 0)
	prefix = "    ";
    else
	prefix = ", ";

    tq_foreach_fwd(&defaults, d) {
	switch (d->type) {
	    case DEFAULTS_HOST:
		if (hostlist_matches(&d->binding) != ALLOW)
		    continue;
		break;
	    case DEFAULTS_USER:
		if (userlist_matches(pw, &d->binding) != ALLOW)
		    continue;
		break;
	    case DEFAULTS_RUNAS:
	    case DEFAULTS_CMND:
		continue;
	}
	lbuf_append(lbuf, prefix, NULL);
	if (d->val != NULL) {
	    lbuf_append(lbuf, d->var, d->op == '+' ? "+=" :
		d->op == '-' ? "-=" : "=", NULL);
	    if (strpbrk(d->val, " \t") != NULL) {
		lbuf_append(lbuf, "\"", NULL);
		lbuf_append_quoted(lbuf, "\"", d->val, NULL);
		lbuf_append(lbuf, "\"", NULL);
	    } else
		lbuf_append_quoted(lbuf, SUDOERS_QUOTED, d->val, NULL);
	} else
	    lbuf_append(lbuf, d->op == FALSE ? "!" : "", d->var, NULL);
	prefix = ", ";
	nfound++;
    }

    return(nfound);
}

/*
 * Display Defaults entries that are per-runas or per-command
 */
int
sudo_file_display_bound_defaults(nss, pw, lbuf)
    struct sudo_nss *nss;
    struct passwd *pw;
    struct lbuf *lbuf;
{
    int nfound = 0;

    /* XXX - should only print ones that match what the user can do. */
    nfound += display_bound_defaults(DEFAULTS_RUNAS, lbuf);
    nfound += display_bound_defaults(DEFAULTS_CMND, lbuf);

    return(nfound);
}

/*
 * Display Defaults entries of the given type.
 */
static int
display_bound_defaults(dtype, lbuf)
    int dtype;
    struct lbuf *lbuf;
{
    struct defaults *d;
    struct member *m, *binding = NULL;
    char *dname, *dsep;
    int atype, nfound = 0;

    switch (dtype) {
	case DEFAULTS_HOST:
	    atype = HOSTALIAS;
	    dname = "host";
	    dsep = "@";
	    break;
	case DEFAULTS_USER:
	    atype = USERALIAS;
	    dname = "user";
	    dsep = ":";
	    break;
	case DEFAULTS_RUNAS:
	    atype = RUNASALIAS;
	    dname = "runas";
	    dsep = ">";
	    break;
	case DEFAULTS_CMND:
	    atype = CMNDALIAS;
	    dname = "cmnd";
	    dsep = "!";
	    break;
	default:
	    return(-1);
    }
    /* printf("Per-%s Defaults entries:\n", dname); */
    tq_foreach_fwd(&defaults, d) {
	if (d->type != dtype)
	    continue;

	nfound++;
	if (binding != tq_first(&d->binding)) {
	    binding = tq_first(&d->binding);
	    lbuf_append(lbuf, "    Defaults", dsep, NULL);
	    for (m = binding; m != NULL; m = m->next) {
		if (m != binding)
		    lbuf_append(lbuf, ",", NULL);
		print_member(lbuf, m->name, m->type, m->negated, atype);
		lbuf_append(lbuf, " ", NULL);
	    }
	} else
	    lbuf_append(lbuf, ", ", NULL);
	if (d->val != NULL) {
	    lbuf_append(lbuf, d->var, d->op == '+' ? "+=" :
		d->op == '-' ? "-=" : "=", d->val, NULL);
	} else
	    lbuf_append(lbuf, d->op == FALSE ? "!" : "", d->var, NULL);
    }

    return(nfound);
}

int
sudo_file_display_cmnd(nss, pw)
    struct sudo_nss *nss;
    struct passwd *pw;
{
    struct cmndspec *cs;
    struct member *match;
    struct privilege *priv;
    struct userspec *us;
    int rval = 1;
    int host_match, runas_match, cmnd_match;

    if (nss->handle == NULL)
	return(rval);

    match = NULL;
    tq_foreach_rev(&userspecs, us) {
	if (userlist_matches(pw, &us->users) != ALLOW)
	    continue;

	tq_foreach_rev(&us->privileges, priv) {
	    host_match = hostlist_matches(&priv->hostlist);
	    if (host_match != ALLOW)
		continue;
	    tq_foreach_rev(&priv->cmndlist, cs) {
		runas_match = runaslist_matches(&cs->runasuserlist,
		    &cs->runasgrouplist);
		if (runas_match == ALLOW) {
		    cmnd_match = cmnd_matches(cs->cmnd);
		    if (cmnd_match != UNSPEC) {
			match = host_match && runas_match ?
			    cs->cmnd : NULL;
			goto matched;
		    }
		}
	    }
	}
    }
    matched:
    if (match != NULL && !match->negated) {
	printf("%s%s%s\n", safe_cmnd, user_args ? " " : "",
	    user_args ? user_args : "");
	rval = 0;
    }
    return(rval);
}

/*
 * Print the contents of a struct member to stdout
 */
static void
_print_member(lbuf, name, type, negated, alias_type)
    struct lbuf *lbuf;
    char *name;
    int type, negated, alias_type;
{
    struct alias *a;
    struct member *m;
    struct sudo_command *c;

    switch (type) {
	case ALL:
	    lbuf_append(lbuf, negated ? "!ALL" : "ALL", NULL);
	    break;
	case COMMAND:
	    c = (struct sudo_command *) name;
	    if (negated)
		lbuf_append(lbuf, "!", NULL);
	    lbuf_append_quoted(lbuf, SUDOERS_QUOTED, c->cmnd, NULL);
	    if (c->args) {
		lbuf_append(lbuf, " ", NULL);
		lbuf_append_quoted(lbuf, SUDOERS_QUOTED, c->args, NULL);
	    }
	    break;
	case ALIAS:
	    if ((a = alias_find(name, alias_type)) != NULL) {
		tq_foreach_fwd(&a->members, m) {
		    if (m != tq_first(&a->members))
			lbuf_append(lbuf, ", ", NULL);
		    _print_member(lbuf, m->name, m->type,
			negated ? !m->negated : m->negated, alias_type);
		}
		break;
	    }
	    /* FALLTHROUGH */
	default:
	    lbuf_append(lbuf, negated ? "!" : "", name, NULL);
	    break;
    }
}

static void
print_member(lbuf, name, type, negated, alias_type)
    struct lbuf *lbuf;
    char *name;
    int type, negated, alias_type;
{
    alias_seqno++;
    _print_member(lbuf, name, type, negated, alias_type);
}
