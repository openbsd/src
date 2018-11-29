/*	$OpenBSD: ldapclient.c,v 1.11 2018/11/29 14:25:07 tedu Exp $	*/

/*
 * Copyright (c) 2018 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <resolv.h>
#include <signal.h>
#include <string.h>
#include <vis.h>

#include "aldap.h"
#include "log.h"

#define F_STARTTLS	0x01
#define F_TLS		0x02
#define F_NEEDAUTH	0x04
#define F_LDIF		0x08

#define LDAPHOST	"localhost"
#define LDAPFILTER	"(objectClass=*)"
#define LDIF_LINELENGTH	79
#define LDAPPASSMAX	1024

struct ldapc {
	struct aldap		*ldap_al;
	char			*ldap_host;
	int			 ldap_port;
	const char		*ldap_capath;
	char			*ldap_binddn;
	char			*ldap_secret;
	unsigned int		 ldap_flags;
	enum protocol_op	 ldap_req;
	enum aldap_protocol	 ldap_protocol;
	struct aldap_url	 ldap_url;
};

struct ldapc_search {
	int			 ls_sizelimit;
	int			 ls_timelimit;
	char			*ls_basedn;
	char			*ls_filter;
	int			 ls_scope;
	char			**ls_attr;
};

__dead void	 usage(void);
int		 ldapc_connect(struct ldapc *);
int		 ldapc_search(struct ldapc *, struct ldapc_search *);
int		 ldapc_printattr(struct ldapc *, const char *,
		    const struct ber_octetstring *);
void		 ldapc_disconnect(struct ldapc *);
int		 ldapc_parseurl(struct ldapc *, struct ldapc_search *,
		    const char *);
const char	*ldapc_resultcode(enum result_code);
const char	*url_decode(char *);

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr,
"usage: %s search [-LvWxZ] [-b basedn] [-c CAfile] [-D binddn] [-H host]\n"
"	    [-l timelimit] [-s scope] [-w secret] [-y secretfile] [-z sizelimit]\n"
"	    [filter] [attributes ...]\n",
	    __progname);

	exit(1);
}

int
main(int argc, char *argv[])
{
	char			 passbuf[LDAPPASSMAX];
	const char		*errstr, *url = NULL, *secretfile = NULL;
	struct stat		 st;
	struct ldapc		 ldap;
	struct ldapc_search	 ls;
	int			 ch;
	int			 verbose = 1;
	FILE			*fp;

	if (pledge("stdio inet unix tty rpath dns", NULL) == -1)
		err(1, "pledge");

	log_init(verbose, 0);

	memset(&ldap, 0, sizeof(ldap));
	memset(&ls, 0, sizeof(ls));
	ls.ls_scope = -1;
	ldap.ldap_port = -1;

	/*
	 * Check the command.  Currently only "search" is supported but
	 * it could be extended with others such as add, modify, or delete.
	 */
	if (argc < 2)
		usage();
	else if (strcmp("search", argv[1]) == 0)
		ldap.ldap_req = LDAP_REQ_SEARCH;
	else
		usage();
	argc--;
	argv++;

	while ((ch = getopt(argc, argv, "b:c:D:H:Ll:s:vWw:xy:Zz:")) != -1) {
		switch (ch) {
		case 'b':
			ls.ls_basedn = optarg;
			break;
		case 'c':
			ldap.ldap_capath = optarg;
			break;
		case 'D':
			ldap.ldap_binddn = optarg;
			ldap.ldap_flags |= F_NEEDAUTH;
			break;
		case 'H':
			url = optarg;
			break;
		case 'L':
			ldap.ldap_flags |= F_LDIF;
			break;
		case 'l':
			ls.ls_timelimit = strtonum(optarg, 0, INT_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "timelimit %s", errstr);
			break;
		case 's':
			if (strcasecmp("base", optarg) == 0)
				ls.ls_scope = LDAP_SCOPE_BASE;
			else if (strcasecmp("one", optarg) == 0)
				ls.ls_scope = LDAP_SCOPE_ONELEVEL;
			else if (strcasecmp("sub", optarg) == 0)
				ls.ls_scope = LDAP_SCOPE_SUBTREE;
			else
				errx(1, "invalid scope: %s", optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			ldap.ldap_secret = optarg;
			ldap.ldap_flags |= F_NEEDAUTH;
			break;
		case 'W':
			ldap.ldap_flags |= F_NEEDAUTH;
			break;
		case 'x':
			/* provided for compatibility */
			break;
		case 'y':
			secretfile = optarg;
			ldap.ldap_flags |= F_NEEDAUTH;
			break;
		case 'Z':
			ldap.ldap_flags |= F_STARTTLS;
			break;
		case 'z':
			ls.ls_sizelimit = strtonum(optarg, 0, INT_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "sizelimit %s", errstr);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	log_setverbose(verbose);

	if (url != NULL && ldapc_parseurl(&ldap, &ls, url) == -1)
		errx(1, "ldapurl");

	/* Set the default after parsing URL and/or options */
	if (ldap.ldap_host == NULL)
		ldap.ldap_host = LDAPHOST;
	if (ldap.ldap_port == -1)
		ldap.ldap_port = ldap.ldap_protocol == LDAPS ?
		    LDAPS_PORT : LDAP_PORT;
	if (ldap.ldap_protocol == LDAP && (ldap.ldap_flags & F_STARTTLS))
		ldap.ldap_protocol = LDAPTLS;
	if (ldap.ldap_capath == NULL)
		ldap.ldap_capath = tls_default_ca_cert_file();
	if (ls.ls_basedn == NULL)
		ls.ls_basedn = "";
	if (ls.ls_scope == -1)
		ls.ls_scope = LDAP_SCOPE_SUBTREE;
	if (ls.ls_filter == NULL)
		ls.ls_filter = LDAPFILTER;

	if (ldap.ldap_flags & F_NEEDAUTH) {
		if (ldap.ldap_binddn == NULL) {
			log_warnx("missing -D binddn");
			usage();
		}
		if (secretfile != NULL) {
			if (ldap.ldap_secret != NULL)
				errx(1, "conflicting -w/-y options");

			/* read password from stdin or file (first line) */
			if (strcmp(secretfile, "-") == 0)
				fp = stdin;
			else if (stat(secretfile, &st) == -1)
				err(1, "failed to access %s", secretfile);
			else if (S_ISREG(st.st_mode) && (st.st_mode & S_IROTH))
				errx(1, "%s is world-readable", secretfile);
			else if ((fp = fopen(secretfile, "r")) == NULL)
				err(1, "failed to open %s", secretfile);
			if (fgets(passbuf, sizeof(passbuf), fp) == NULL)
				err(1, "failed to read %s", secretfile);
			if (fp != stdin)
				fclose(fp);

			passbuf[strcspn(passbuf, "\n")] = '\0';
			ldap.ldap_secret = passbuf;
		}
		if (ldap.ldap_secret == NULL) {
			if (readpassphrase("Password: ",
			    passbuf, sizeof(passbuf), RPP_REQUIRE_TTY) == NULL)
				errx(1, "failed to read LDAP password");
			ldap.ldap_secret = passbuf;
		}
	}

	if (pledge("stdio inet unix rpath dns", NULL) == -1)
		err(1, "pledge");

	/* optional search filter */
	if (argc && strchr(argv[0], '=') != NULL) {
		ls.ls_filter = argv[0];
		argc--;
		argv++;
	}
	/* search attributes */
	if (argc)
		ls.ls_attr = argv;

	if (ldapc_connect(&ldap) == -1)
		errx(1, "LDAP connection failed");

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (ldapc_search(&ldap, &ls) == -1)
		errx(1, "LDAP search failed");

	ldapc_disconnect(&ldap);
	aldap_free_url(&ldap.ldap_url);

	return (0);
}

int
ldapc_search(struct ldapc *ldap, struct ldapc_search *ls)
{
	struct aldap_page_control	*pg = NULL;
	struct aldap_message		*m;
	const char			*errstr;
	const char			*searchdn, *dn = NULL;
	char				*outkey;
	struct aldap_stringset		*outvalues;
	int				 ret, code, fail = 0;
	size_t				 i;

	if (ldap->ldap_flags & F_LDIF)
		printf("version: 1\n");
	do {
		if (aldap_search(ldap->ldap_al, ls->ls_basedn, ls->ls_scope,
		    ls->ls_filter, ls->ls_attr, 0, ls->ls_sizelimit,
		    ls->ls_timelimit, pg) == -1) {
			aldap_get_errno(ldap->ldap_al, &errstr);
			log_warnx("LDAP search failed: %s", errstr);
			return (-1);
		}

		if (pg != NULL) {
			aldap_freepage(pg);
			pg = NULL;
		}

		while ((m = aldap_parse(ldap->ldap_al)) != NULL) {
			if (ldap->ldap_al->msgid != m->msgid) {
				goto fail;
			}

			if ((code = aldap_get_resultcode(m)) != LDAP_SUCCESS) {
				log_warnx("LDAP search failed: %s(%d)",
				    ldapc_resultcode(code), code);
				break;
			}

			if (m->message_type == LDAP_RES_SEARCH_RESULT) {
				if (m->page != NULL && m->page->cookie_len != 0)
					pg = m->page;
				else
					pg = NULL;

				aldap_freemsg(m);
				break;
			}

			if (m->message_type != LDAP_RES_SEARCH_ENTRY) {
				goto fail;
			}

			if (aldap_count_attrs(m) < 1) {
				aldap_freemsg(m);
				continue;
			}

			if ((searchdn = aldap_get_dn(m)) == NULL)
				goto fail;

			if (dn != NULL)
				printf("\n");
			else
				dn = ls->ls_basedn;
			if (strcmp(dn, searchdn) != 0)
				printf("dn: %s\n", searchdn);

			for (ret = aldap_first_attr(m, &outkey, &outvalues);
			    ret != -1;
			    ret = aldap_next_attr(m, &outkey, &outvalues)) {
				for (i = 0; i < outvalues->len; i++) {
					if (ldapc_printattr(ldap, outkey,
					    &(outvalues->str[i])) == -1) {
						fail = 1;
						break;
					}
				}
			}
			free(outkey);
			aldap_free_attr(outvalues);

			aldap_freemsg(m);
		}
	} while (pg != NULL && fail == 0);

	if (fail)
		return (-1);
	return (0);
 fail:
	ldapc_disconnect(ldap);
	return (-1);
}

int
ldapc_printattr(struct ldapc *ldap, const char *key,
    const struct ber_octetstring *value)
{
	char			*p = NULL, *out;
	const unsigned char	*cp;
	int			 encode;
	size_t			 i, inlen, outlen, left;

	if (ldap->ldap_flags & F_LDIF) {
		/* OpenLDAP encodes the userPassword by default */
		if (strcasecmp("userPassword", key) == 0)
			encode = 1;
		else
			encode = 0;

		/*
		 * The LDIF format a set of characters that can be included
		 * in SAFE-STRINGs. String value that do not match the
		 * criteria must be encoded as Base64.
		 */
		cp = (const unsigned char *)value->ostr_val;
		/* !SAFE-INIT-CHAR: SAFE-CHAR minus %x20 %x3A %x3C */
		if (*cp == ' ' ||
		    *cp == ':' ||
		    *cp == '<')
			encode = 1;
		for (i = 0; encode == 0 && i < value->ostr_len - 1; i++) {
			/* !SAFE-CHAR %x01-09 / %x0B-0C / %x0E-7F */
			if (cp[i] > 127 ||
			    cp[i] == '\0' ||
			    cp[i] == '\n' ||
			    cp[i] == '\r')
				encode = 1;
		}

		if (!encode) {
			if (asprintf(&p, "%s: %s", key,
			    (const char *)value->ostr_val) == -1) {
				log_warnx("asprintf");
				return (-1);
			}
		} else {
			outlen = (((value->ostr_len + 2) / 3) * 4) + 1;

			if ((out = calloc(1, outlen)) == NULL ||
			    b64_ntop(value->ostr_val, value->ostr_len, out,
			    outlen) == -1) {
				log_warnx("Base64 encoding failed");
				free(p);
				return (-1);
			}

			/* Base64 is indicated with a double-colon */
			if (asprintf(&p, "%s:: %s", key, out) == -1) {
				log_warnx("asprintf");
				free(out);
				return (-1);
			}
			free(out);
		}

		/* Wrap lines */
		for (outlen = 0, inlen = strlen(p);
		    outlen < inlen;
		    outlen += LDIF_LINELENGTH - 1) {
			if (outlen)
				putchar(' ');
			if (outlen > LDIF_LINELENGTH)
				outlen--;
			/* max. line length - newline - optional indent */
			left = MIN(inlen - outlen, outlen ?
			    LDIF_LINELENGTH - 2 :
			    LDIF_LINELENGTH - 1);
			fwrite(p + outlen, left, 1, stdout);
			putchar('\n');
		}
	} else {
		/*
		 * Use vis(1) instead of base64 encoding of non-printable
		 * values.  This is much nicer as it always prdocues a
		 * human-readable visual output.  This can safely be done
		 * on all values no matter if they include non-printable
		 * characters.
		 */
		p = calloc(1, 4 * value->ostr_len + 1);
		if (strvisx(p, value->ostr_val, value->ostr_len,
		    VIS_SAFE|VIS_NL) == -1) {
			log_warn("visual encoding failed");
			return (-1);
		}

		printf("%s: %s\n", key, p);
		free(p);
	}

	free(p);
	return (0);
}

int
ldapc_connect(struct ldapc *ldap)
{
	struct addrinfo		 ai, *res, *res0;
	struct sockaddr_un	 un;
	int			 ret = -1, saved_errno, fd = -1, code;
	struct aldap_message	*m;
	const char		*errstr;
	struct tls_config	*tls_config;
	char			 port[6];

	if (ldap->ldap_protocol == LDAPI) {
		memset(&un, 0, sizeof(un));
		un.sun_family = AF_UNIX;
		if (strlcpy(un.sun_path, ldap->ldap_host,
		    sizeof(un.sun_path)) >= sizeof(un.sun_path)) {
			log_warnx("socket '%s' too long", ldap->ldap_host);
			goto done;
		}
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ||
		    connect(fd, (struct sockaddr *)&un, sizeof(un)) == -1)
			goto done;
		goto init;
	}

	memset(&ai, 0, sizeof(ai));
	ai.ai_family = AF_UNSPEC;
	ai.ai_socktype = SOCK_STREAM;
	ai.ai_protocol = IPPROTO_TCP;
	(void)snprintf(port, sizeof(port), "%u", ldap->ldap_port);
	if ((code = getaddrinfo(ldap->ldap_host, port,
	    &ai, &res0)) != 0) {
		log_warnx("%s", gai_strerror(code));
		goto done;
	}
	for (res = res0; res; res = res->ai_next, fd = -1) {
		if ((fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) == -1)
			continue;

		if (connect(fd, res->ai_addr, res->ai_addrlen) >= 0)
			break;

		saved_errno = errno;
		close(fd);
		errno = saved_errno;
	}
	freeaddrinfo(res0);
	if (fd == -1)
		goto done;

 init:
	if ((ldap->ldap_al = aldap_init(fd)) == NULL) {
		warn("LDAP init failed");
		close(fd);
		goto done;
	}

	if (ldap->ldap_flags & F_STARTTLS) {
		log_debug("%s: requesting STARTTLS", __func__);
		if (aldap_req_starttls(ldap->ldap_al) == -1) {
			log_warnx("failed to request STARTTLS");
			goto done;
		}

		if ((m = aldap_parse(ldap->ldap_al)) == NULL) {
			log_warnx("failed to parse STARTTLS response");
			goto done;
		}

		if (ldap->ldap_al->msgid != m->msgid ||
		    (code = aldap_get_resultcode(m)) != LDAP_SUCCESS) {
			log_warnx("STARTTLS failed: %s(%d)",
			    ldapc_resultcode(code), code);
			aldap_freemsg(m);
			goto done;
		}
		aldap_freemsg(m);
	}

	if (ldap->ldap_flags & (F_STARTTLS | F_TLS)) {
		log_debug("%s: starting TLS", __func__);

		if ((tls_config = tls_config_new()) == NULL) {
			log_warnx("TLS config failed");
			goto done;
		}

		if (tls_config_set_ca_file(tls_config,
		    ldap->ldap_capath) == -1) {
			log_warnx("unable to set CA %s", ldap->ldap_capath);
			goto done;
		}

		if (aldap_tls(ldap->ldap_al, tls_config, ldap->ldap_host) < 0) {
			aldap_get_errno(ldap->ldap_al, &errstr);
			log_warnx("TLS failed: %s", errstr);
			goto done;
		}
	}

	if (ldap->ldap_flags & F_NEEDAUTH) {
		log_debug("%s: bind request", __func__);
		if (aldap_bind(ldap->ldap_al, ldap->ldap_binddn,
		    ldap->ldap_secret) == -1) {
			log_warnx("bind request failed");
			goto done;
		}

		if ((m = aldap_parse(ldap->ldap_al)) == NULL) {
			log_warnx("failed to parse bind response");
			goto done;
		}

		if (ldap->ldap_al->msgid != m->msgid ||
		    (code = aldap_get_resultcode(m)) != LDAP_SUCCESS) {
			log_warnx("bind failed: %s(%d)",
			    ldapc_resultcode(code), code);
			aldap_freemsg(m);
			goto done;
		}
		aldap_freemsg(m);
	}

	log_debug("%s: connected", __func__);

	ret = 0;
 done:
	if (ret != 0)
		ldapc_disconnect(ldap);
	if (ldap->ldap_secret != NULL)
		explicit_bzero(ldap->ldap_secret,
		    strlen(ldap->ldap_secret));
	return (ret);
}

void
ldapc_disconnect(struct ldapc *ldap)
{
	if (ldap->ldap_al == NULL)
		return;
	aldap_close(ldap->ldap_al);
	ldap->ldap_al = NULL;
}

const char *
ldapc_resultcode(enum result_code code)
{
#define CODE(_X)	case _X:return (#_X)
	switch (code) {
	CODE(LDAP_SUCCESS);
	CODE(LDAP_OPERATIONS_ERROR);
	CODE(LDAP_PROTOCOL_ERROR);
	CODE(LDAP_TIMELIMIT_EXCEEDED);
	CODE(LDAP_SIZELIMIT_EXCEEDED);
	CODE(LDAP_COMPARE_FALSE);
	CODE(LDAP_COMPARE_TRUE);
	CODE(LDAP_STRONG_AUTH_NOT_SUPPORTED);
	CODE(LDAP_STRONG_AUTH_REQUIRED);
	CODE(LDAP_REFERRAL);
	CODE(LDAP_ADMINLIMIT_EXCEEDED);
	CODE(LDAP_UNAVAILABLE_CRITICAL_EXTENSION);
	CODE(LDAP_CONFIDENTIALITY_REQUIRED);
	CODE(LDAP_SASL_BIND_IN_PROGRESS);
	CODE(LDAP_NO_SUCH_ATTRIBUTE);
	CODE(LDAP_UNDEFINED_TYPE);
	CODE(LDAP_INAPPROPRIATE_MATCHING);
	CODE(LDAP_CONSTRAINT_VIOLATION);
	CODE(LDAP_TYPE_OR_VALUE_EXISTS);
	CODE(LDAP_INVALID_SYNTAX);
	CODE(LDAP_NO_SUCH_OBJECT);
	CODE(LDAP_ALIAS_PROBLEM);
	CODE(LDAP_INVALID_DN_SYNTAX);
	CODE(LDAP_ALIAS_DEREF_PROBLEM);
	CODE(LDAP_INAPPROPRIATE_AUTH);
	CODE(LDAP_INVALID_CREDENTIALS);
	CODE(LDAP_INSUFFICIENT_ACCESS);
	CODE(LDAP_BUSY);
	CODE(LDAP_UNAVAILABLE);
	CODE(LDAP_UNWILLING_TO_PERFORM);
	CODE(LDAP_LOOP_DETECT);
	CODE(LDAP_NAMING_VIOLATION);
	CODE(LDAP_OBJECT_CLASS_VIOLATION);
	CODE(LDAP_NOT_ALLOWED_ON_NONLEAF);
	CODE(LDAP_NOT_ALLOWED_ON_RDN);
	CODE(LDAP_ALREADY_EXISTS);
	CODE(LDAP_NO_OBJECT_CLASS_MODS);
	CODE(LDAP_AFFECTS_MULTIPLE_DSAS);
	CODE(LDAP_OTHER);
	default:
		return ("UNKNOWN_ERROR");
	}
};

int
ldapc_parseurl(struct ldapc *ldap, struct ldapc_search *ls, const char *url)
{
	struct aldap_url	*lu = &ldap->ldap_url;
	size_t			 i;

	memset(lu, 0, sizeof(*lu));
	lu->scope = -1;

	if (aldap_parse_url(url, lu) == -1) {
		log_warnx("failed to parse LDAP URL");
		return (-1);
	}

	/* The protocol part is optional and we default to ldap:// */
	if (lu->protocol == -1)
		lu->protocol = LDAP;
	else if (lu->protocol == LDAPI) {
		if (lu->port != 0 ||
		    url_decode(lu->host) == NULL) {
			log_warnx("invalid ldapi:// URL");
			return (-1);
		}
	} else if ((ldap->ldap_flags & F_STARTTLS) &&
	    lu->protocol != LDAPTLS) {
		log_warnx("conflicting protocol arguments");
		return (-1);
	} else if (lu->protocol == LDAPTLS)
		ldap->ldap_flags |= F_TLS|F_STARTTLS;
	else if (lu->protocol == LDAPS)
		ldap->ldap_flags |= F_TLS;
	ldap->ldap_protocol = lu->protocol;

	ldap->ldap_host = lu->host;
	if (lu->port)
		ldap->ldap_port = lu->port;

	/* The distinguished name has to be URL-encoded */
	if (lu->dn != NULL && ls->ls_basedn != NULL &&
	    strcasecmp(ls->ls_basedn, lu->dn) != 0) {
		log_warnx("conflicting basedn arguments");
		return (-1);
	}
	if (lu->dn != NULL) {
		if (url_decode(lu->dn) == NULL)
			return (-1);
		ls->ls_basedn = lu->dn;
	}

	if (lu->scope != -1) {
		if (ls->ls_scope != -1 && (ls->ls_scope != lu->scope)) {
			log_warnx("conflicting scope arguments");
			return (-1);
		}
		ls->ls_scope = lu->scope;
	}

	/* URL-decode optional attributes and the search filter */
	if (lu->attributes[0] != NULL) {
		for (i = 0; i < MAXATTR && lu->attributes[i] != NULL; i++)
			if (url_decode(lu->attributes[i]) == NULL)
				return (-1);
		ls->ls_attr = lu->attributes;
	}
	if (lu->filter != NULL) {
		if (url_decode(lu->filter) == NULL)
			return (-1);
		ls->ls_filter = lu->filter;
	}

	return (0);
}

/* From usr.sbin/httpd/httpd.c */
const char *
url_decode(char *url)
{
	char		*p, *q;
	char		 hex[3];
	unsigned long	 x;

	hex[2] = '\0';
	p = q = url;

	while (*p != '\0') {
		switch (*p) {
		case '%':
			/* Encoding character is followed by two hex chars */
			if (!(isxdigit((unsigned char)p[1]) &&
			    isxdigit((unsigned char)p[2])))
				return (NULL);

			hex[0] = p[1];
			hex[1] = p[2];

			/*
			 * We don't have to validate "hex" because it is
			 * guaranteed to include two hex chars followed by nul.
			 */
			x = strtoul(hex, NULL, 16);
			*q = (char)x;
			p += 2;
			break;
		default:
			*q = *p;
			break;
		}
		p++;
		q++;
	}
	*q = '\0';

	return (url);
}
