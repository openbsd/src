/*
 * Copyright (c) 2003-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * This code is derived from software contributed by Aaron Spangler.
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
 */

#include <config.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
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
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
# include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <errno.h>
#ifdef HAVE_LBER_H
# include <lber.h>
#endif
#include <ldap.h>

#include "sudo.h"
#include "parse.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: ldap.c,v 1.11.2.15 2007/07/18 11:13:50 millert Exp $";
#endif /* lint */

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

#ifndef LDAP_OPT_SUCCESS
# define LDAP_OPT_SUCCESS LDAP_SUCCESS
#endif

#if defined(LDAP_X_OPT_CONNECT_TIMEOUT) && !defined(LDAP_OPT_X_CONNECT_TIMEOUT)
#define LDAP_OPT_X_CONNECT_TIMEOUT	LDAP_OPT_X_CONNECT_TIMEOUT
#endif

#define	DPRINTF(args, level)	if (ldap_conf.debug >= level) warnx args

/* ldap configuration structure */
struct ldap_config {
    int port;
    int version;
    int debug;
    int tls_checkpeer;
    int timelimit;
    int bind_timelimit;
    char *host;
    char *uri;
    char *binddn;
    char *bindpw;
    char *rootbinddn;
    char *base;
    char *ssl;
    char *tls_cacertfile;
    char *tls_cacertdir;
    char *tls_random_file;
    char *tls_cipher_suite;
    char *tls_certfile;
    char *tls_keyfile;
} ldap_conf;

static void sudo_ldap_update_defaults __P((LDAP *));
static void sudo_ldap_close __P((LDAP *));
static LDAP *sudo_ldap_open __P((void));

/*
 * Walk through search results and return TRUE if we have a matching
 * netgroup, else FALSE.
 */
int
sudo_ldap_check_user_netgroup(ld, entry)
    LDAP *ld;
    LDAPMessage *entry;
{
    char **v = NULL, **p = NULL;
    int ret = FALSE;

    if (!entry)
	return(ret);

    /* get the values from the entry */
    v = ldap_get_values(ld, entry, "sudoUser");

    /* walk through values */
    for (p = v; p && *p && !ret; p++) {
	/* match any */
	if (netgr_matches(*p, NULL, NULL, user_name))
	    ret = TRUE;
	DPRINTF(("ldap sudoUser netgroup '%s' ... %s", *p,
	    ret ? "MATCH!" : "not"), 2);
    }

    if (v)
	ldap_value_free(v);	/* cleanup */

    return(ret);
}

/*
 * Walk through search results and return TRUE if we have a
 * host match, else FALSE.
 */
int
sudo_ldap_check_host(ld, entry)
    LDAP *ld;
    LDAPMessage *entry;
{
    char **v = NULL, **p = NULL;
    int ret = FALSE;

    if (!entry)
	return(ret);

    /* get the values from the entry */
    v = ldap_get_values(ld, entry, "sudoHost");

    /* walk through values */
    for (p = v; p && *p && !ret; p++) {
	/* match any or address or netgroup or hostname */
	if (!strcasecmp(*p, "ALL") || addr_matches(*p) ||
	    netgr_matches(*p, user_host, user_shost, NULL) ||
	    !hostname_matches(user_shost, user_host, *p))
	    ret = TRUE;
	DPRINTF(("ldap sudoHost '%s' ... %s", *p,
	    ret ? "MATCH!" : "not"), 2);
    }

    if (v)
	ldap_value_free(v);	/* cleanup */

    return(ret);
}

/*
 * Walk through search results and return TRUE if we have a runas match,
 * else FALSE.
 * Since the runas directive in /etc/sudoers is optional, so is sudoRunAs.
 */
int
sudo_ldap_check_runas(ld, entry)
    LDAP *ld;
    LDAPMessage *entry;
{
    char **v = NULL, **p = NULL;
    int ret = FALSE;

    if (!entry)
	return(ret);

    /* get the values from the entry */
    v = ldap_get_values(ld, entry, "sudoRunAs");

    /*
     * BUG:
     * 
     * if runas is not specified on the command line, the only information
     * as to which user to run as is in the runas_default option.  We should
     * check to see if we have the local option present.  Unfortunately we
     * don't parse these options until after this routine says yes or no.
     * The query has already returned, so we could peek at the attribute
     * values here though.
     * 
     * For now just require users to always use -u option unless its set
     * in the global defaults. This behaviour is no different than the global
     * /etc/sudoers.
     *
     * Sigh - maybe add this feature later
     *
     */

    /*
     * If there are no runas entries, match runas_default against
     * what the user specified on the command line.
     */
    if (!v)
	ret = !strcasecmp(*user_runas, def_runas_default);

    /* walk through values returned, looking for a match */
    for (p = v; p && *p && !ret; p++) {
	if (!strcasecmp(*p, *user_runas) || !strcasecmp(*p, "ALL")) 
	    ret = TRUE;
	DPRINTF(("ldap sudoRunAs '%s' ... %s", *p,
	    ret ? "MATCH!" : "not"), 2);
    }

    if (v)
	ldap_value_free(v);	/* cleanup */

    return(ret);
}

/*
 * Walk through search results and return TRUE if we have a command match.
 */
int
sudo_ldap_check_command(ld, entry)
    LDAP *ld;
    LDAPMessage *entry;
{
    char *allowed_cmnd, *allowed_args, **v = NULL, **p = NULL;
    int foundbang, ret = FALSE;

    if (!entry)
	return(ret);

    v = ldap_get_values(ld, entry, "sudoCommand");

    /* get_first_entry */
    for (p = v; p && *p && ret >= 0; p++) {
	/* Match against ALL ? */
	if (!strcasecmp(*p, "ALL")) {
	    ret = TRUE;
	    DPRINTF(("ldap sudoCommand '%s' ... MATCH!", *p), 2);
	    continue;
	}

	/* check for !command */
	if (**p == '!') {
	    foundbang = TRUE;
	    allowed_cmnd = estrdup(1 + *p);	/* !command */
	} else {
	    foundbang = FALSE;
	    allowed_cmnd = estrdup(*p);		/* command */
	}

	/* split optional args away from command */
	allowed_args = strchr(allowed_cmnd, ' ');
	if (allowed_args)
	    *allowed_args++ = '\0';

	/* check the command like normal */
	if (command_matches(allowed_cmnd, allowed_args)) {
	    /*
	     * If allowed (no bang) set ret but keep on checking.
	     * If disallowed (bang), exit loop.
	     */
	    ret = foundbang ? -1 : TRUE;
	}
	DPRINTF(("ldap sudoCommand '%s' ... %s", *p,
	    ret == TRUE ? "MATCH!" : "not"), 2);

	efree(allowed_cmnd);	/* cleanup */
    }

    if (v)
	ldap_value_free(v);	/* more cleanup */

    /* return TRUE if we found at least one ALLOW and no DENY */
    return(ret > 0);
}

/*
 * Read sudoOption and modify the defaults as we go.  This is used once
 * from the cn=defaults entry and also once when a final sudoRole is matched.
 */
void
sudo_ldap_parse_options(ld, entry)
    LDAP *ld;
    LDAPMessage *entry;
{
    char op, *var, *val, **v = NULL, **p = NULL;

    if (!entry)
	return;

    v = ldap_get_values(ld, entry, "sudoOption");

    /* walk through options */
    for (p = v; p && *p; p++) {

	DPRINTF(("ldap sudoOption: '%s'", *p), 2);
	var = estrdup(*p);

	/* check for equals sign past first char */
	val = strchr(var, '=');
	if (val > var) {
	    *val++ = '\0';	/* split on = and truncate var */
	    op = *(val - 2);	/* peek for += or -= cases */
	    if (op == '+' || op == '-') {
		*(val - 2) = '\0';	/* found, remove extra char */
		/* case var+=val or var-=val */
		set_default(var, val, (int) op);
	    } else {
		/* case var=val */
		set_default(var, val, TRUE);
	    }
	} else if (*var == '!') {
	    /* case !var Boolean False */
	    set_default(var + 1, NULL, FALSE);
	} else {
	    /* case var Boolean True */
	    set_default(var, NULL, TRUE);
	}
	efree(var);
    }

    if (v)
	ldap_value_free(v);
}

/*
 * Concatenate strings, dynamically growing them as necessary.
 * Strings can be arbitrarily long and are allocated/reallocated on
 * the fly.  Make sure to free them when you are done.
 *
 * Usage:
 *
 * char *s=NULL;
 * size_t sz;
 *
 * ncat(&s,&sz,"This ");
 * ncat(&s,&sz,"is ");
 * ncat(&s,&sz,"an ");
 * ncat(&s,&sz,"arbitrarily ");
 * ncat(&s,&sz,"long ");
 * ncat(&s,&sz,"string!");
 *
 * printf("String Value='%s', but has %d bytes allocated\n",s,sz);
 *
 */
void
ncat(s, sz, src)
    char **s;
    size_t *sz;
    char *src;
{
    size_t nsz;

    /* handle initial alloc */
    if (*s == NULL) {
	*s = estrdup(src);
	*sz = strlen(src) + 1;
	return;
    }
    /* handle realloc */
    nsz = strlen(*s) + strlen(src) + 1;
    if (*sz < nsz)
	*s = erealloc((void *) *s, *sz = nsz * 2);
    strlcat(*s, src, *sz);
}

/*
 * builds together a filter to check against ldap
 */
char *
sudo_ldap_build_pass1()
{
    struct group *grp;
    size_t sz;
    char *b = NULL;
    int i;

    /* global OR */
    ncat(&b, &sz, "(|");

    /* build filter sudoUser=user_name */
    ncat(&b, &sz, "(sudoUser=");
    ncat(&b, &sz, user_name);
    ncat(&b, &sz, ")");

    /* Append primary group */
    grp = getgrgid(user_gid);
    if (grp != NULL) {
	ncat(&b, &sz, "(sudoUser=%");
	ncat(&b, &sz, grp -> gr_name);
	ncat(&b, &sz, ")");
    }

    /* Append supplementary groups */
    for (i = 0; i < user_ngroups; i++) {
	if ((grp = getgrgid(user_groups[i])) != NULL) {
	    ncat(&b, &sz, "(sudoUser=%");
	    ncat(&b, &sz, grp -> gr_name);
	    ncat(&b, &sz, ")");
	}
    }

    /* Add ALL to list */
    ncat(&b, &sz, "(sudoUser=ALL)");

    /* End of OR List */
    ncat(&b, &sz, ")");

    return(b);
}

/*
 * Map yes/true/on to TRUE, no/false/off to FALSE, else -1
 */
int
_atobool(s)
    const char *s;
{
    switch (*s) {
	case 'y':
	case 'Y':
	    if (strcasecmp(s, "yes") == 0)
		return(TRUE);
	    break;
	case 't':
	case 'T':
	    if (strcasecmp(s, "true") == 0)
		return(TRUE);
	    break;
	case 'o':
	case 'O':
	    if (strcasecmp(s, "on") == 0)
		return(TRUE);
	    if (strcasecmp(s, "off") == 0)
		return(FALSE);
	    break;
	case 'n':
	case 'N':
	    if (strcasecmp(s, "no") == 0)
		return(FALSE);
	    break;
	case 'f':
	case 'F':
	    if (strcasecmp(s, "false") == 0)
		return(FALSE);
	    break;
    }
    return(-1);
}

int
sudo_ldap_read_config()
{
    FILE *f;
    char buf[LINE_MAX], *c, *keyword, *value;

    /* defaults */
    ldap_conf.version = 3;
    ldap_conf.port = 389;
    ldap_conf.tls_checkpeer = -1;
    ldap_conf.timelimit = -1;
    ldap_conf.bind_timelimit = -1;

    if ((f = fopen(_PATH_LDAP_CONF, "r")) == NULL)
	return(FALSE);
    while (fgets(buf, sizeof(buf), f)) {
	/* ignore text after comment character */
	if ((c = strchr(buf, '#')) != NULL)
	    *c = '\0';

	/* skip leading whitespace */
	for (c = buf; isspace((unsigned char) *c); c++)
	    /* nothing */;

	if (*c == '\0' || *c == '\n')
	    continue;		/* skip empty line */

	/* properly terminate keyword string */
	keyword = c;
	while (*c && !isspace((unsigned char) *c))
	    c++;
	if (*c)
	    *c++ = '\0';	/* terminate keyword */

	/* skip whitespace before value */
	while (isspace((unsigned char) *c))
	    c++;
	value = c;

	/* trim whitespace after value */
	while (*c)
	    c++;		/* wind to end */
	while (--c > value && isspace((unsigned char) *c))
	    *c = '\0';

	/* The following macros make the code much more readable */

#define MATCH_S(x,y) if (!strcasecmp(keyword,x)) \
    { efree(y); y=estrdup(value); }
#define MATCH_I(x,y) if (!strcasecmp(keyword,x)) { y=atoi(value); }
#define MATCH_B(x,y) if (!strcasecmp(keyword,x)) { y=_atobool(value); }

	/*
	 * Parse values using a continues chain of if else if else if else if
	 * else ...
	 */
	MATCH_S("host", ldap_conf.host)
	    else
	MATCH_I("port", ldap_conf.port)
	    else
	MATCH_S("ssl", ldap_conf.ssl)
	    else
	MATCH_B("tls_checkpeer", ldap_conf.tls_checkpeer)
	    else
	MATCH_S("tls_cacertfile", ldap_conf.tls_cacertfile)
	    else
	MATCH_S("tls_cacertdir", ldap_conf.tls_cacertdir)
	    else
	MATCH_S("tls_randfile", ldap_conf.tls_random_file)
	    else
	MATCH_S("tls_ciphers", ldap_conf.tls_cipher_suite)
	    else
	MATCH_S("tls_cert", ldap_conf.tls_certfile)
	    else
	MATCH_S("tls_key", ldap_conf.tls_keyfile)
	    else
	MATCH_I("ldap_version", ldap_conf.version)
	    else
	MATCH_I("bind_timelimit", ldap_conf.bind_timelimit)
	    else
	MATCH_I("timelimit", ldap_conf.timelimit)
	    else
	MATCH_S("uri", ldap_conf.uri)
	    else
	MATCH_S("binddn", ldap_conf.binddn)
	    else
	MATCH_S("bindpw", ldap_conf.bindpw)
	    else
	MATCH_S("rootbinddn", ldap_conf.rootbinddn)
	    else
	MATCH_S("sudoers_base", ldap_conf.base)
	    else
	MATCH_I("sudoers_debug", ldap_conf.debug)
	    else {

	    /*
	     * The keyword was unrecognized.  Since this config file is
	     * shared by multiple programs, it is appropriate to silently
	     * ignore options this program does not understand
	     */
	}

    }
    fclose(f);

    if (!ldap_conf.host)
	ldap_conf.host = estrdup("localhost");

    if (ldap_conf.bind_timelimit > 0)
	ldap_conf.bind_timelimit *= 1000;	/* convert to ms */

    if (ldap_conf.debug > 1) {
	fprintf(stderr, "LDAP Config Summary\n");
	fprintf(stderr, "===================\n");
#ifdef HAVE_LDAP_INITIALIZE
	if (ldap_conf.uri) {
	    fprintf(stderr, "uri          %s\n", ldap_conf.uri);
	} else
#endif
	{
	    fprintf(stderr, "host         %s\n", ldap_conf.host ?
		ldap_conf.host : "(NONE)");
	    fprintf(stderr, "port         %d\n", ldap_conf.port);
	}
	fprintf(stderr, "ldap_version %d\n", ldap_conf.version);

	fprintf(stderr, "sudoers_base %s\n", ldap_conf.base ?
	    ldap_conf.base : "(NONE) <---Sudo will ignore ldap)");
	fprintf(stderr, "binddn       %s\n", ldap_conf.binddn ?
	    ldap_conf.binddn : "(anonymous)");
	fprintf(stderr, "bindpw       %s\n", ldap_conf.bindpw ?
	    ldap_conf.bindpw : "(anonymous)");
	fprintf(stderr, "bind_timelimit  %d\n", ldap_conf.bind_timelimit);
	fprintf(stderr, "timelimit    %d\n", ldap_conf.timelimit);
#ifdef HAVE_LDAP_START_TLS_S
	fprintf(stderr, "ssl          %s\n", ldap_conf.ssl ?
	    ldap_conf.ssl : "(no)");
#endif
	fprintf(stderr, "===================\n");
    }
    if (!ldap_conf.base)
	return(FALSE);		/* if no base is defined, ignore LDAP */

    /* If rootbinddn set, read in /etc/ldap.secret if it exists. */
    if (ldap_conf.rootbinddn) {
	if ((f = fopen(_PATH_LDAP_SECRET, "r")) != NULL) {
	    if (fgets(buf, sizeof(buf), f) != NULL) {
		/* removing trailing newlines */
		for (c = buf; *c != '\0'; c++)
		    continue;
		while (--c > buf && *c == '\n')
		    *c = '\0';
		/* copy to bindpw and binddn */
		efree(ldap_conf.bindpw);
		ldap_conf.bindpw = estrdup(buf);
		efree(ldap_conf.binddn);
		ldap_conf.binddn = ldap_conf.rootbinddn;
		ldap_conf.rootbinddn = NULL;
	    }
	    fclose(f);
	}
    }
    return(TRUE);
}

/*
 * like perl's join(sep,@ARGS)
 */
char *
 _ldap_join_values(sep, v)
    char *sep;
    char **v;
{
    char *b = NULL, **p = NULL;
    size_t sz = 0;

    /* paste values together */
    for (p = v; p && *p; p++) {
	if (p != v && sep != NULL)
	    ncat(&b, &sz, sep);	/* append seperator */
	ncat(&b, &sz, *p);	/* append value */
    }

    /* sanity check */
    if (b[0] == '\0') {
	/* something went wrong, put something here */
	ncat(&b, &sz, "(empty list)");	/* append value */
    }

    return(b);
}

char *sudo_ldap_cm_list = NULL;
size_t sudo_ldap_cm_list_size;

#define SAVE_LIST(x) ncat(&sudo_ldap_cm_list,&sudo_ldap_cm_list_size,(x))
/*
 * Walks through search result and returns TRUE if we have a
 * command match
 */
int
sudo_ldap_add_match(ld, entry, pwflag)
    LDAP *ld;
    LDAPMessage *entry;
    int pwflag;
{
    char *dn, **edn, **v = NULL;

    /* if we are not collecting matches, then don't save them */
    if (pwflag != I_LISTPW)
	return(TRUE);

    /* collect the dn, only show the rdn */
    dn = ldap_get_dn(ld, entry);
    edn = dn ? ldap_explode_dn(dn, 1) : NULL;
    SAVE_LIST("\nLDAP Role: ");
    SAVE_LIST((edn && *edn) ? *edn : "UNKNOWN");
    SAVE_LIST("\n");
    if (dn)
	ldap_memfree(dn);
    if (edn)
	ldap_value_free(edn);

    /* get the Runas Values from the entry */
    v = ldap_get_values(ld, entry, "sudoRunAs");
    if (v && *v) {
	SAVE_LIST("  RunAs: (");
	SAVE_LIST(_ldap_join_values(", ", v));
	SAVE_LIST(")\n");
    }
    if (v)
	ldap_value_free(v);

    /* get the Command Values from the entry */
    v = ldap_get_values(ld, entry, "sudoCommand");
    if (v && *v) {
	SAVE_LIST("  Commands:\n    ");
	SAVE_LIST(_ldap_join_values("\n    ", v));
	SAVE_LIST("\n");
    } else {
	SAVE_LIST("  Commands: NONE\n");
    }
    if (v)
	ldap_value_free(v);

    return(FALSE);		/* Don't stop at the first match */
}
#undef SAVE_LIST

void
sudo_ldap_list_matches()
{
    if (sudo_ldap_cm_list != NULL)
	printf("%s", sudo_ldap_cm_list);
}

/* macros to set option, error on failure plus consistent debugging */
#define SET_OPTS(opt, val) do { \
    if (ldap_conf.val != NULL) { \
	if (ldap_conf.debug > 1) \
	    fprintf(stderr, \
		"ldap_set_option(LDAP_OPT_%s, \"%s\")\n", #opt, ldap_conf.val);\
	rc = ldap_set_option(ld, LDAP_OPT_ ## opt, ldap_conf.val); \
	if (rc != LDAP_OPT_SUCCESS) { \
	    fprintf(stderr,"ldap_set_option(LDAP_OPT_%s, \"%s\")=%d: %s\n", \
		#opt, ldap_conf.val, rc, ldap_err2string(rc)); \
	    return(NULL); \
	} \
    } \
} while(0)
#define SET_OPTI(opt, val) do { \
    if (ldap_conf.val >= 0) { \
	if (ldap_conf.debug > 1) \
	    fprintf(stderr, \
		"ldap_set_option(LDAP_OPT_%s, %d)\n", #opt, ldap_conf.val); \
	rc = ldap_set_option(ld, LDAP_OPT_ ## opt, &ldap_conf.val); \
	if (rc != LDAP_OPT_SUCCESS) { \
	    fprintf(stderr,"ldap_set_option(LDAP_OPT_%s, %d)=%d: %s\n", \
		#opt, ldap_conf.val, rc, ldap_err2string(rc)); \
	    return(NULL); \
	} \
    } \
} while(0)

/*
 * Open a connection to the LDAP server.
 */
static LDAP *
sudo_ldap_open()
{
    LDAP *ld = NULL;
    int rc;

    if (!sudo_ldap_read_config())
	return(NULL);

    /* attempt to setup ssl options */
#ifdef LDAP_OPT_X_TLS_CACERTFILE
    SET_OPTS(X_TLS_CACERTFILE, tls_cacertfile);
#endif /* LDAP_OPT_X_TLS_CACERTFILE */

#ifdef LDAP_OPT_X_TLS_CACERTDIR
    SET_OPTS(X_TLS_CACERTDIR, tls_cacertdir);
#endif /* LDAP_OPT_X_TLS_CACERTDIR */

#ifdef LDAP_OPT_X_TLS_CERTFILE
    SET_OPTS(X_TLS_CERTFILE, tls_certfile);
#endif /* LDAP_OPT_X_TLS_CERTFILE */

#ifdef LDAP_OPT_X_TLS_KEYFILE
    SET_OPTS(X_TLS_KEYFILE, tls_keyfile);
#endif /* LDAP_OPT_X_TLS_KEYFILE */

#ifdef LDAP_OPT_X_TLS_CIPHER_SUITE
    SET_OPTS(X_TLS_CIPHER_SUITE, tls_cipher_suite);
#endif /* LDAP_OPT_X_TLS_CIPHER_SUITE */

#ifdef LDAP_OPT_X_TLS_RANDOM_FILE
    SET_OPTS(X_TLS_RANDOM_FILE, tls_random_file);
#endif /* LDAP_OPT_X_TLS_RANDOM_FILE */

#ifdef LDAP_OPT_X_TLS_REQUIRE_CERT
    /* check the server certificate? */
    SET_OPTI(X_TLS_REQUIRE_CERT, tls_checkpeer);
#endif /* LDAP_OPT_X_TLS_REQUIRE_CERT */

    /* set timelimit options */
    SET_OPTI(TIMELIMIT, timelimit);

#ifdef LDAP_OPT_NETWORK_TIMEOUT
    if (ldap_conf.bind_timelimit > 0) {
	struct timeval tv;
	tv.tv_sec = ldap_conf.bind_timelimit / 1000;
	tv.tv_usec = 0;
	if (ldap_conf.debug > 1)
	    fprintf(stderr, "ldap_set_option(LDAP_OPT_NETWORK_TIMEOUT, %ld)\n",
	    tv.tv_sec);
	rc = ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &tv);
	if (rc != LDAP_OPT_SUCCESS) {
	    fprintf(stderr,"ldap_set_option(NETWORK_TIMEOUT, %ld)=%d: %s\n",
	    tv.tv_sec, rc, ldap_err2string(rc));
	    return(NULL);
	}
    }
#endif

    /* attempt connect */
#ifdef HAVE_LDAP_INITIALIZE
    if (ldap_conf.uri) {

	DPRINTF(("ldap_initialize(ld,%s)", ldap_conf.uri), 2);

	rc = ldap_initialize(&ld, ldap_conf.uri);
	if (rc) {
	    fprintf(stderr, "ldap_initialize()=%d : %s\n",
		rc, ldap_err2string(rc));
	    return(NULL);
	}
    } else
#endif /* HAVE_LDAP_INITIALIZE */
    if (ldap_conf.host) {

	DPRINTF(("ldap_init(%s,%d)", ldap_conf.host, ldap_conf.port), 2);

	if ((ld = ldap_init(ldap_conf.host, ldap_conf.port)) == NULL) {
	    fprintf(stderr, "ldap_init(): errno=%d : %s\n",
		errno, strerror(errno));
	    return(NULL);
	}
    }
#ifdef LDAP_OPT_PROTOCOL_VERSION

    /* Set the LDAP Protocol version */
    SET_OPTI(PROTOCOL_VERSION, version);

#endif /* LDAP_OPT_PROTOCOL_VERSION */

#ifdef HAVE_LDAP_START_TLS_S
    /* Turn on TLS */
    if (ldap_conf.ssl && !strcasecmp(ldap_conf.ssl, "start_tls")) {
	rc = ldap_start_tls_s(ld, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
	    fprintf(stderr, "ldap_start_tls_s(): %d: %s\n", rc,
		ldap_err2string(rc));
	    ldap_unbind(ld);
	    return(NULL);
	}
	DPRINTF(("ldap_start_tls_s() ok"), 1);
    }
#endif /* HAVE_LDAP_START_TLS_S */

    /* Actually connect */
    if ((rc = ldap_simple_bind_s(ld, ldap_conf.binddn, ldap_conf.bindpw))) {
	fprintf(stderr, "ldap_simple_bind_s()=%d : %s\n",
	    rc, ldap_err2string(rc));
	return(NULL);
    }
    DPRINTF(("ldap_bind() ok"), 1);

    return(ld);
}

static void
sudo_ldap_update_defaults(ld)
    LDAP *ld;
{
    LDAPMessage *entry = NULL, *result = NULL;	 /* used for searches */
    int rc;					 /* temp return value */

    rc = ldap_search_s(ld, ldap_conf.base, LDAP_SCOPE_SUBTREE,
	"cn=defaults", NULL, 0, &result);
    if (!rc && (entry = ldap_first_entry(ld, result))) {
	DPRINTF(("found:%s", ldap_get_dn(ld, entry)), 1);
	sudo_ldap_parse_options(ld, entry);
    } else
	DPRINTF(("no default options found!"), 1);

    if (result)
	ldap_msgfree(result);
}

/*
 * like sudoers_lookup() - only LDAP style
 */
int
sudo_ldap_check(pwflag)
    int pwflag;
{
    LDAP *ld;
    LDAPMessage *entry = NULL, *result = NULL;	/* used for searches */
    char *filt;					/* used to parse attributes */
    int rc, ret = FALSE, do_netgr;		/* temp/final return values */
    int ldap_user_matches = FALSE, ldap_host_matches = FALSE; /* flags */

    /* Open a connection to the LDAP server. */
    if ((ld = sudo_ldap_open()) == NULL)
	return(VALIDATE_ERROR);

    /* Parse Default options. */
    sudo_ldap_update_defaults(ld);

    /*
     * Okay - time to search for anything that matches this user
     * Lets limit it to only two queries of the LDAP server
     *
     * The first pass will look by the username, groups, and
     * the keyword ALL.  We will then inspect the results that
     * came back from the query.  We don't need to inspect the
     * sudoUser in this pass since the LDAP server already scanned
     * it for us.
     *
     * The second pass will return all the entries that contain
     * user netgroups.  Then we take the netgroups returned and
     * try to match them against the username.
     */

    for (do_netgr = 0; !ret && do_netgr < 2; do_netgr++) {
	filt = do_netgr ? estrdup("sudoUser=+*") : sudo_ldap_build_pass1();
	DPRINTF(("ldap search '%s'", filt), 1);
	rc = ldap_search_s(ld, ldap_conf.base, LDAP_SCOPE_SUBTREE, filt,
	    NULL, 0, &result);
	if (rc)
	    DPRINTF(("nothing found for '%s'", filt), 1);
	efree(filt);

	/* parse each entry returned from this most recent search */
	entry = rc ? NULL : ldap_first_entry(ld, result);
	while (entry != NULL) {
	    DPRINTF(("found:%s", ldap_get_dn(ld, entry)), 1);
	    if (
	    /* first verify user netgroup matches - only if in pass 2 */
		(!do_netgr || sudo_ldap_check_user_netgroup(ld, entry)) &&
	    /* remember that user matched */
		(ldap_user_matches = -1) &&
	    /* verify host match */
		sudo_ldap_check_host(ld, entry) &&
	    /* remember that host matched */
		(ldap_host_matches = -1) &&
	    /* add matches for listing later */
		sudo_ldap_add_match(ld, entry, pwflag) &&
	    /* verify command match */
		sudo_ldap_check_command(ld, entry) &&
	    /* verify runas match */
		sudo_ldap_check_runas(ld, entry)
		) {
		/* We have a match! */
		DPRINTF(("Perfect Matched!"), 1);
		/* pick up any options */
		sudo_ldap_parse_options(ld, entry);
		/* make sure we don't reenter loop */
		ret = VALIDATE_OK;
		/* break from inside for loop */
		break;
	    }
	    entry = ldap_next_entry(ld, entry);
	}
	if (result)
	    ldap_msgfree(result);
	result = NULL;
    }

    sudo_ldap_close(ld);		/* shut down connection */

    DPRINTF(("user_matches=%d", ldap_user_matches), 1);
    DPRINTF(("host_matches=%d", ldap_host_matches), 1);

    /* Check for special case for -v, -k, -l options */
    if (pwflag && ldap_user_matches && ldap_host_matches) {
	/*
         * Handle verifypw & listpw
         *
         * To be extra paranoid, since we haven't read any NOPASSWD options
         * in /etc/sudoers yet, but we have to make the decission now, lets
         * assume the worst and prefer to prompt for password unless the setting
         * is "never". (example verifypw=never or listpw=never)
         *
         */
	ret = VALIDATE_OK;
	if (pwflag == -1) {
	    SET(ret, FLAG_NOPASS);		/* -k or -K */
	} else {
	    switch (sudo_defs_table[pwflag].sd_un.tuple) {
	    case never:
		SET(ret, FLAG_NOPASS);
		break;
	    case always:
		if (def_authenticate)
		    SET(ret, FLAG_CHECK_USER);
		break;
	    default:
		break;
	    }
	}
    }
    if (ISSET(ret, VALIDATE_OK)) {
	/* we have a match, should we check the password? */
	if (!def_authenticate)
	    SET(ret, FLAG_NOPASS);
	if (def_noexec)
	    SET(ret, FLAG_NOEXEC);
	if (def_setenv)
	    SET(ret, FLAG_SETENV);
    } else {
	/* we do not have a match */
	ret = VALIDATE_NOT_OK;
	if (pwflag)
	    SET(ret, FLAG_NO_CHECK);
	else if (!ldap_user_matches)
	    SET(ret, FLAG_NO_USER);
	else if (!ldap_host_matches)
	    SET(ret, FLAG_NO_HOST);
    }
    DPRINTF(("sudo_ldap_check(%d)=0x%02x", pwflag, ret), 1);

    return(ret);
}

/*
 * shut down LDAP connection
 */
static void
sudo_ldap_close(LDAP *ld)
{
    if (ld)
	ldap_unbind_s(ld);
}
