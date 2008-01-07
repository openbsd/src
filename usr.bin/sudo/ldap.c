/*
 * Copyright (c) 2003-2008 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <limits.h>
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
__unused static const char rcsid[] = "$Sudo: ldap.c,v 1.11.2.32 2008/01/05 23:27:10 millert Exp $";
#endif /* lint */

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

#ifndef LDAP_OPT_SUCCESS
# define LDAP_OPT_SUCCESS LDAP_SUCCESS
#endif

#define	DPRINTF(args, level)	if (ldap_conf.debug >= level) warnx args

#define CONF_BOOL	0
#define CONF_INT	1
#define CONF_STR	2

#define SUDO_LDAP_SSL		1
#define SUDO_LDAP_STARTTLS	2

struct ldap_config_table {
    const char *conf_str;	/* config file string */
    short type;			/* CONF_BOOL, CONF_INT, CONF_STR */
    short connected;		/* connection-specific value? */
    int opt_val;		/* LDAP_OPT_* (or -1 for sudo internal) */
    void *valp;			/* pointer into ldap_conf */
};

/* ldap configuration structure */
struct ldap_config {
    int port;
    int version;
    int debug;
    int ldap_debug;
    int tls_checkpeer;
    int timelimit;
    int bind_timelimit;
    int ssl_mode;
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

struct ldap_config_table ldap_conf_table[] = {
    { "sudoers_debug", CONF_INT, FALSE, -1, &ldap_conf.debug },
    { "host", CONF_STR, FALSE, -1, &ldap_conf.host },
    { "port", CONF_INT, FALSE, -1, &ldap_conf.port },
    { "ssl", CONF_STR, FALSE, -1, &ldap_conf.ssl },
    { "sslpath", CONF_STR, FALSE, -1, &ldap_conf.tls_certfile },
    { "uri", CONF_STR, FALSE, -1, &ldap_conf.uri },
#ifdef LDAP_OPT_DEBUG_LEVEL
    { "debug", CONF_INT, FALSE, LDAP_OPT_DEBUG_LEVEL, &ldap_conf.ldap_debug },
#endif
#ifdef LDAP_OPT_PROTOCOL_VERSION
    { "ldap_version", CONF_INT, TRUE, LDAP_OPT_PROTOCOL_VERSION,
	&ldap_conf.version },
#endif
#ifdef LDAP_OPT_X_TLS_REQUIRE_CERT
    { "tls_checkpeer", CONF_BOOL, FALSE, LDAP_OPT_X_TLS_REQUIRE_CERT,
	&ldap_conf.tls_checkpeer },
#endif
#ifdef LDAP_OPT_X_TLS_CACERTFILE
    { "tls_cacertfile", CONF_STR, FALSE, LDAP_OPT_X_TLS_CACERTFILE,
	&ldap_conf.tls_cacertfile },
#endif
#ifdef LDAP_OPT_X_TLS_CACERTDIR
    { "tls_cacertdir", CONF_STR, FALSE, LDAP_OPT_X_TLS_CACERTDIR,
	&ldap_conf.tls_cacertdir },
#endif
#ifdef LDAP_OPT_X_TLS_RANDOM_FILE
    { "tls_randfile", CONF_STR, FALSE, LDAP_OPT_X_TLS_RANDOM_FILE,
	&ldap_conf.tls_random_file },
#endif
#ifdef LDAP_OPT_X_TLS_CIPHER_SUITE
    { "tls_ciphers", CONF_STR, FALSE, LDAP_OPT_X_TLS_CIPHER_SUITE,
	&ldap_conf.tls_cipher_suite },
#endif
#ifdef LDAP_OPT_X_TLS_CERTFILE
    { "tls_cert", CONF_STR, FALSE, LDAP_OPT_X_TLS_CERTFILE,
	&ldap_conf.tls_certfile },
#else
    { "tls_cert", CONF_STR, FALSE, -1, &ldap_conf.tls_certfile },
#endif
#ifdef LDAP_OPT_X_TLS_KEYFILE
    { "tls_key", CONF_STR, FALSE, LDAP_OPT_X_TLS_KEYFILE,
	&ldap_conf.tls_keyfile },
#else
    { "tls_key", CONF_STR, FALSE, -1, &ldap_conf.tls_keyfile },
#endif
#ifdef LDAP_OPT_NETWORK_TIMEOUT
    { "bind_timelimit", CONF_INT, TRUE, -1 /* needs timeval, set manually */,
	&ldap_conf.bind_timelimit },
#elif defined(LDAP_X_OPT_CONNECT_TIMEOUT)
    { "bind_timelimit", CONF_INT, TRUE, LDAP_X_OPT_CONNECT_TIMEOUT,
	&ldap_conf.bind_timelimit },
#endif
    { "timelimit", CONF_INT, TRUE, LDAP_OPT_TIMELIMIT, &ldap_conf.timelimit },
    { "binddn", CONF_STR, FALSE, -1, &ldap_conf.binddn },
    { "bindpw", CONF_STR, FALSE, -1, &ldap_conf.bindpw },
    { "rootbinddn", CONF_STR, FALSE, -1, &ldap_conf.rootbinddn },
    { "sudoers_base", CONF_STR, FALSE, -1, &ldap_conf.base },
    { NULL }
};

static void sudo_ldap_update_defaults __P((LDAP *));
static void sudo_ldap_close __P((LDAP *));
static LDAP *sudo_ldap_open __P((void));

#ifndef HAVE_LDAP_INITIALIZE
/*
 * For each uri, convert to host:port pairs.  For ldaps:// enable SSL
 * Accepts: uris of the form ldap:/// or ldap://hostname:portnum/
 * where the trailing slash is optional.
 */
static int
sudo_ldap_parse_uri(uri_list)
    const char *uri_list;
{
    char *buf, *uri, *host, *cp, *port;
    char hostbuf[LINE_MAX];
    int nldap = 0, nldaps = 0;
    int rc = -1;

    buf = estrdup(uri_list);
    hostbuf[0] = '\0';
    for ((uri = strtok(buf, " \t")); uri != NULL; (uri = strtok(NULL, " \t"))) {
	if (strncasecmp(uri, "ldap://", 7) == 0) {
	    nldap++;
	    host = uri + 7;
	} else if (strncasecmp(uri, "ldaps://", 8) == 0) {
	    nldaps++;
	    host = uri + 8;
	} else {
	    warnx("unsupported LDAP uri type: %s", uri);
	    goto done;
	}

	/* trim optional trailing slash */
	if ((cp = strrchr(host, '/')) != NULL && cp[1] == '\0') {
	    *cp = '\0';
	}

	if (hostbuf[0] != '\0') {
	    if (strlcat(hostbuf, " ", sizeof(hostbuf)) >= sizeof(hostbuf))
		goto toobig;
	}

	if (*host == '\0')
	    host = "localhost";		/* no host specified, use localhost */

	if (strlcat(hostbuf, host, sizeof(hostbuf)) >= sizeof(hostbuf))
	    goto toobig;

	/* If using SSL and no port specified, add port 636 */
	if (nldaps) {
	    if ((port = strrchr(host, ':')) == NULL || !isdigit(port[1]))
		if (strlcat(hostbuf, ":636", sizeof(hostbuf)) >= sizeof(hostbuf))
		    goto toobig;
	}
    }
    if (hostbuf[0] == '\0') {
	warnx("invalid uri: %s", uri_list);
	goto done;
    }

    if (nldaps != 0) {
	if (nldap != 0) {
	    warnx("cannot mix ldap and ldaps URIs");
	    goto done;
	}
	if (ldap_conf.ssl_mode == SUDO_LDAP_STARTTLS) {
	    warnx("cannot mix ldaps and starttls");
	    goto done;
	}
	ldap_conf.ssl_mode = SUDO_LDAP_SSL;
    }

    free(ldap_conf.host);
    ldap_conf.host = estrdup(hostbuf);
    rc = 0;

done:
    efree(buf);
    return(rc);

toobig:
    errx(1, "sudo_ldap_parse_uri: out of space building hostbuf");
}
#endif /* HAVE_LDAP_INITIALIZE */

static int
sudo_ldap_init(ldp, host, port)
    LDAP **ldp;
    const char *host;
    int port;
{
    LDAP *ld = NULL;
    int rc = LDAP_CONNECT_ERROR;

#ifdef HAVE_LDAPSSL_INIT
    if (ldap_conf.ssl_mode == SUDO_LDAP_SSL) {
	DPRINTF(("ldapssl_clientauth_init(%s, %s)",
	    ldap_conf.tls_certfile ? ldap_conf.tls_certfile : "NULL",
	    ldap_conf.tls_keyfile ? ldap_conf.tls_keyfile : "NULL"), 2);
	rc = ldapssl_clientauth_init(ldap_conf.tls_certfile, NULL,
	    ldap_conf.tls_keyfile != NULL, ldap_conf.tls_keyfile, NULL);
	if (rc != LDAP_SUCCESS) {
	    warnx("unable to initialize SSL cert and key db: %s",
		ldapssl_err2string(rc));
	    goto done;
	}

	DPRINTF(("ldapssl_init(%s, %d, 1)", host, port), 2);
	if ((ld = ldapssl_init(host, port, 1)) == NULL)
	    goto done;
    } else
#endif
    {
	DPRINTF(("ldap_init(%s, %d)", host, port), 2);
	if ((ld = ldap_init(host, port)) == NULL)
	    goto done;
    }
    rc = LDAP_SUCCESS;

done:
    *ldp = ld;
    return(rc);
}

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
	if (!strcmp(*p, "ALL") || addr_matches(*p) ||
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
	ret = !strcasecmp(runas_pw->pw_name, def_runas_default);

    /* walk through values returned, looking for a match */
    for (p = v; p && *p && !ret; p++) {
	switch (*p[0]) {
	case '+':
	    if (netgr_matches(*p, NULL, NULL, runas_pw->pw_name))
		ret = TRUE;
	    break;
	case '%':
	    if (usergr_matches(*p, runas_pw->pw_name, runas_pw))
		ret = TRUE;
	    break;
	case 'A':
	    if (strcmp(*p, "ALL") == 0) {
		ret = TRUE;
		break;
	    }
	    /* FALLTHROUGH */
	default:
	    if (strcasecmp(*p, runas_pw->pw_name) == 0)
		ret = TRUE;
	    break;
	}
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
sudo_ldap_check_command(ld, entry, setenv_implied)
    LDAP *ld;
    LDAPMessage *entry;
    int *setenv_implied;
{
    char *allowed_cmnd, *allowed_args, **v = NULL, **p = NULL;
    int foundbang, ret = FALSE;

    if (!entry)
	return(ret);

    v = ldap_get_values(ld, entry, "sudoCommand");

    /* get_first_entry */
    for (p = v; p && *p && ret >= 0; p++) {
	/* Match against ALL ? */
	if (!strcmp(*p, "ALL")) {
	    ret = TRUE;
	    if (setenv_implied != NULL)
		*setenv_implied = TRUE;
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
	if (user_groups[i] == user_gid)
	    continue;
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
    struct ldap_config_table *cur;

    /* defaults */
    ldap_conf.version = 3;
    ldap_conf.port = -1;
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

	/* Look up keyword in config table. */
	for (cur = ldap_conf_table; cur->conf_str != NULL; cur++) {
	    if (strcasecmp(keyword, cur->conf_str) == 0) {
		switch (cur->type) {
		case CONF_BOOL:
		    *(int *)(cur->valp) = _atobool(value);
		    break;
		case CONF_INT:
		    *(int *)(cur->valp) = atoi(value);
		    break;
		case CONF_STR:
		    efree(*(char **)(cur->valp));
		    *(char **)(cur->valp) = estrdup(value);
		    break;
		}
		break;
	    }
	}
    }
    fclose(f);

    if (!ldap_conf.host)
	ldap_conf.host = "localhost";

    if (ldap_conf.bind_timelimit > 0)
	ldap_conf.bind_timelimit *= 1000;	/* convert to ms */

    if (ldap_conf.debug > 1) {
	fprintf(stderr, "LDAP Config Summary\n");
	fprintf(stderr, "===================\n");
	if (ldap_conf.uri) {
	    fprintf(stderr, "uri          %s\n", ldap_conf.uri);
	} else {
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
	if (ldap_conf.bind_timelimit > 0)
	    fprintf(stderr, "bind_timelimit  %d\n", ldap_conf.bind_timelimit);
	if (ldap_conf.timelimit > 0)
	    fprintf(stderr, "timelimit    %d\n", ldap_conf.timelimit);
	fprintf(stderr, "ssl          %s\n", ldap_conf.ssl ?
	    ldap_conf.ssl : "(no)");
	if (ldap_conf.tls_checkpeer != -1)
	    fprintf(stderr, "tls_checkpeer    %s\n", ldap_conf.tls_checkpeer ?
		"(yes)" : "(no)");
	if (ldap_conf.tls_cacertfile != NULL)
	    fprintf(stderr, "tls_cacertfile   %s\n", ldap_conf.tls_cacertfile);
	if (ldap_conf.tls_cacertdir != NULL)
	    fprintf(stderr, "tls_cacertdir    %s\n", ldap_conf.tls_cacertdir);
	if (ldap_conf.tls_random_file != NULL)
	    fprintf(stderr, "tls_random_file  %s\n", ldap_conf.tls_random_file);
	if (ldap_conf.tls_cipher_suite != NULL)
	    fprintf(stderr, "tls_cipher_suite %s\n", ldap_conf.tls_cipher_suite);
	if (ldap_conf.tls_certfile != NULL)
	    fprintf(stderr, "tls_certfile     %s\n", ldap_conf.tls_certfile);
	if (ldap_conf.tls_keyfile != NULL)
	    fprintf(stderr, "tls_keyfile      %s\n", ldap_conf.tls_keyfile);
	fprintf(stderr, "===================\n");
    }
    if (!ldap_conf.base)
	return(FALSE);		/* if no base is defined, ignore LDAP */

    /*
     * Interpret SSL option
     */
    if (ldap_conf.ssl != NULL) {
	    if (strcasecmp(ldap_conf.ssl, "start_tls") == 0)
		ldap_conf.ssl_mode = SUDO_LDAP_STARTTLS;
	    else if (_atobool(ldap_conf.ssl))
		ldap_conf.ssl_mode = SUDO_LDAP_SSL;
    }

#ifndef HAVE_LDAP_INITIALIZE
    /* Convert uri list to host list if no ldap_initialize(). */
    if (ldap_conf.uri) {
	if (sudo_ldap_parse_uri(ldap_conf.uri) != 0)
	    return(FALSE);
	free(ldap_conf.uri);
	ldap_conf.uri = NULL;
	ldap_conf.port = LDAP_PORT;
    }
#endif

    /* Use port 389 for plaintext LDAP and port 636 for SSL LDAP */
    if (!ldap_conf.uri && ldap_conf.port < 0)
	ldap_conf.port =
	    ldap_conf.ssl_mode == SUDO_LDAP_SSL ? LDAPS_PORT : LDAP_PORT;

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
	    ncat(&b, &sz, sep);	/* append separator */
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

/*
 * Set LDAP options based on the config table.
 */
int
sudo_ldap_set_options(ld)
    LDAP *ld;
{
    struct ldap_config_table *cur;
    int rc;

    /* Set ber options */
#ifdef LBER_OPT_DEBUG_LEVEL
    if (ldap_conf.ldap_debug)
	ber_set_option(NULL, LBER_OPT_DEBUG_LEVEL, &ldap_conf.ldap_debug);
#endif

    /* Set simple LDAP options */
    for (cur = ldap_conf_table; cur->conf_str != NULL; cur++) {
	LDAP *conn;
	int ival;
	char *sval;

	if (cur->opt_val == -1)
	    continue;

	conn = cur->connected ? ld : NULL;
	switch (cur->type) {
	case CONF_BOOL:
	case CONF_INT:
	    ival = *(int *)(cur->valp);
	    if (ival >= 0) {
		rc = ldap_set_option(conn, cur->opt_val, &ival);
		if (rc != LDAP_OPT_SUCCESS) {
		    warnx("ldap_set_option: %s -> %d: %s",
			cur->conf_str, ival, ldap_err2string(rc));
		    return(-1);
		}
		DPRINTF(("ldap_set_option: %s -> %d", cur->conf_str, ival), 1);
	    }
	    break;
	case CONF_STR:
	    sval = *(char **)(cur->valp);
	    if (sval != NULL) {
		rc = ldap_set_option(conn, cur->opt_val, sval);
		if (rc != LDAP_OPT_SUCCESS) {
		    warnx("ldap_set_option: %s -> %s: %s",
			cur->conf_str, sval, ldap_err2string(rc));
		    return(-1);
		}
		DPRINTF(("ldap_set_option: %s -> %s", cur->conf_str, sval), 1);
	    }
	    break;
	}
    }

#ifdef LDAP_OPT_NETWORK_TIMEOUT
    /* Convert bind_timelimit to a timeval */
    if (ldap_conf.bind_timelimit > 0) {
	struct timeval tv;
	tv.tv_sec = ldap_conf.bind_timelimit / 1000;
	tv.tv_usec = 0;
	rc = ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &tv);
	if (rc != LDAP_OPT_SUCCESS) {
	    warnx("ldap_set_option(NETWORK_TIMEOUT, %ld): %s",
		(long)tv.tv_sec, ldap_err2string(rc));
	    return(-1);
	}
	DPRINTF(("ldap_set_option(LDAP_OPT_NETWORK_TIMEOUT, %ld)\n",
	    (long)tv.tv_sec), 1);
    }
#endif

#if defined(LDAP_OPT_X_TLS) && !defined(HAVE_LDAPSSL_INIT)
    if (ldap_conf.ssl_mode == SUDO_LDAP_SSL) {
	int val = LDAP_OPT_X_TLS_HARD;
	rc = ldap_set_option(ld, LDAP_OPT_X_TLS, &val);
	if (rc != LDAP_SUCCESS) {
	    warnx("ldap_set_option(LDAP_OPT_X_TLS, LDAP_OPT_X_TLS_HARD): %s",
		ldap_err2string(rc));
	    return(-1);
	}
	DPRINTF(("ldap_set_option(LDAP_OPT_X_TLS, LDAP_OPT_X_TLS_HARD)\n"), 1);
    }
#endif
    return(0);
}

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

    /* Connect to LDAP server */
#ifdef HAVE_LDAP_INITIALIZE
    if (ldap_conf.uri != NULL) {
	DPRINTF(("ldap_initialize(ld, %s)", ldap_conf.uri), 2);
	rc = ldap_initialize(&ld, ldap_conf.uri);
    } else
#endif /* HAVE_LDAP_INITIALIZE */
	rc = sudo_ldap_init(&ld, ldap_conf.host, ldap_conf.port);
    if (rc != LDAP_SUCCESS) {
	warnx("unable to initialize LDAP: %s", ldap_err2string(rc));
	return(NULL);
    }

    /* Set LDAP options */
    if (sudo_ldap_set_options(ld) < 0)
	return(NULL);

    if (ldap_conf.ssl_mode == SUDO_LDAP_STARTTLS) {
#ifdef HAVE_LDAP_START_TLS_S
	rc = ldap_start_tls_s(ld, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
	    warnx("ldap_start_tls_s(): %s", ldap_err2string(rc));
	    ldap_unbind(ld);
	    return(NULL);
	}
	DPRINTF(("ldap_start_tls_s() ok"), 1);
#else
	warnx("start_tls specified but LDAP libs do not support ldap_start_tls_s()");
#endif /* HAVE_LDAP_START_TLS_S */
    }

    /* Actually connect */
    if ((rc = ldap_simple_bind_s(ld, ldap_conf.binddn, ldap_conf.bindpw))) {
	warnx("ldap_simple_bind_s: %s", ldap_err2string(rc));
	return(NULL);
    }
    DPRINTF(("ldap_simple_bind_s() ok"), 1);

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
    if (rc == LDAP_SUCCESS && (entry = ldap_first_entry(ld, result))) {
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
    int setenv_implied;
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
    setenv_implied = FALSE;
    for (do_netgr = 0; !ret && do_netgr < 2; do_netgr++) {
	filt = do_netgr ? estrdup("sudoUser=+*") : sudo_ldap_build_pass1();
	DPRINTF(("ldap search '%s'", filt), 1);
	rc = ldap_search_s(ld, ldap_conf.base, LDAP_SCOPE_SUBTREE, filt,
	    NULL, 0, &result);
	if (rc != LDAP_SUCCESS)
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
		sudo_ldap_check_command(ld, entry, &setenv_implied) &&
	    /* verify runas match */
		sudo_ldap_check_runas(ld, entry)
		) {
		/* We have a match! */
		DPRINTF(("Perfect Matched!"), 1);
		/* pick up any options */
		if (setenv_implied)
		    def_setenv = TRUE;
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
