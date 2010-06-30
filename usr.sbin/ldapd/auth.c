/*	$OpenBSD: auth.c,v 1.5 2010/06/30 19:26:39 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <errno.h>
#include <openssl/sha.h>
#include <pwd.h>
#include <resolv.h>		/* for b64_pton */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ldapd.h"

static int
aci_matches(struct aci *aci, struct conn *conn, struct namespace *ns,
    char *dn, int rights, enum scope scope)
{
	struct btval	 key;

	if ((rights & aci->rights) != rights)
		return 0;

	if (dn == NULL)
		return 0;

	if (aci->target != NULL) {
		key.size = strlen(dn);
		key.data = dn;

		if (scope == LDAP_SCOPE_BASE) {
			switch (aci->scope) {
			case LDAP_SCOPE_BASE:
				if (strcmp(dn, aci->target) != 0)
					return 0;
				break;
			case LDAP_SCOPE_ONELEVEL:
				if (!is_child_of(&key, aci->target))
					return 0;
				break;
			case LDAP_SCOPE_SUBTREE:
				if (!has_suffix(&key, aci->target))
					return 0;
				break;
			}
		} else if (scope == LDAP_SCOPE_ONELEVEL) {
			switch (aci->scope) {
			case LDAP_SCOPE_BASE:
				return 0;
			case LDAP_SCOPE_ONELEVEL:
				if (strcmp(dn, aci->target) != 0)
					return 0;
				break;
			case LDAP_SCOPE_SUBTREE:
				if (!has_suffix(&key, aci->target))
					return 0;
				break;
			}
		} else if (scope == LDAP_SCOPE_SUBTREE) {
			switch (aci->scope) {
			case LDAP_SCOPE_BASE:
			case LDAP_SCOPE_ONELEVEL:
				return 0;
			case LDAP_SCOPE_SUBTREE:
				if (!has_suffix(&key, aci->target))
					return 0;
				break;
			}
		}
	}

	if (aci->subject != NULL) {
		if (conn->binddn == NULL)
			return 0;
		if (strcmp(aci->subject, "@") == 0) {
			if (strcmp(dn, conn->binddn) != 0)
				return 0;
		} else if (strcmp(aci->subject, conn->binddn) != 0)
			return 0;
	}

	return 1;
}

/* Returns true (1) if conn is authorized for op on dn in namespace.
 */
int
authorized(struct conn *conn, struct namespace *ns, int rights, char *dn,
    int scope)
{
	struct aci	*aci;
	int		 type = ACI_ALLOW;

	/* Root DN is always allowed. */
	if (conn->binddn != NULL) {
		if (conf->rootdn != NULL &&
		    strcasecmp(conn->binddn, conf->rootdn) == 0)
			return 1;
		if (ns != NULL && ns->rootdn != NULL &&
		    strcasecmp(conn->binddn, ns->rootdn) == 0)
			return 1;
	}

	/* Default to deny for write access. */
	if ((rights & (ACI_WRITE | ACI_CREATE)) != 0)
		type = ACI_DENY;

	log_debug("requesting %02X access to %s by %s, in namespace %s",
	    rights,
	    dn ?: "any",
	    conn->binddn ?: "any",
	    ns ? ns->suffix : "global");

	SIMPLEQ_FOREACH(aci, &conf->acl, entry) {
		if (aci_matches(aci, conn, ns, dn, rights, scope)) {
			type = aci->type;
			log_debug("%s by: %s %02X access to %s by %s",
			    type == ACI_ALLOW ? "allowed" : "denied",
			    aci->type == ACI_ALLOW ? "allow" : "deny",
			    aci->rights,
			    aci->target ?: "any",
			    aci->subject ?: "any");
		}
	}

	if (ns != NULL) {
		SIMPLEQ_FOREACH(aci, &ns->acl, entry) {
			if (aci_matches(aci, conn, ns, dn, rights, scope)) {
				type = aci->type;
				log_debug("%s by: %s %02X access to %s by %s",
				    type == ACI_ALLOW ? "allowed" : "denied",
				    aci->type == ACI_ALLOW ? "allow" : "deny",
				    aci->rights,
				    aci->target ?: "any",
				    aci->subject ?: "any");
			}
		}
	}

	return type == ACI_ALLOW ? 1 : 0;
}

static int
check_password(const char *stored_passwd, const char *passwd)
{
	int		 sz;
	char		*encpw;
	unsigned char	*salt;
	unsigned char	 md[SHA_DIGEST_LENGTH];
	unsigned char	 tmp[128];
	SHA_CTX		 ctx;

	if (stored_passwd == NULL)
		return -1;

	if (strncmp(stored_passwd, "{SHA}", 5) == 0) {
		sz = b64_pton(stored_passwd + 5, tmp, sizeof(tmp));
		if (sz != SHA_DIGEST_LENGTH)
			return -1;
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, passwd, strlen(passwd));
		SHA1_Final(md, &ctx);
		return memcmp(md, tmp, SHA_DIGEST_LENGTH);
	}
	else if (strncmp(stored_passwd, "{SSHA}", 6) == 0) {
		sz = b64_pton(stored_passwd + 6, tmp, sizeof(tmp));
		if (sz <= SHA_DIGEST_LENGTH)
			return -1;
		salt = tmp + SHA_DIGEST_LENGTH;
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, passwd, strlen(passwd));
		SHA1_Update(&ctx, salt, sz - SHA_DIGEST_LENGTH);
		SHA1_Final(md, &ctx);
		return memcmp(md, tmp, SHA_DIGEST_LENGTH);
	}
	else if (strncmp(stored_passwd, "{CRYPT}", 7) == 0) {
		encpw = crypt(passwd, stored_passwd + 7);
		if (encpw == NULL)
			return -1;
		return strcmp(encpw, stored_passwd + 7);
	}
	else
		return strcmp(stored_passwd, passwd);
}

static int
ldap_auth_sasl(struct request *req, char *binddn, struct ber_element *params)
{
	size_t			 len;
	char			*method;
	char			*authzid, *authcid, *password;
	char			*creds;
	struct auth_req		 auth_req;

	if (ber_scanf_elements(params, "{sx", &method, &creds, &len) != 0)
		return LDAP_PROTOCOL_ERROR;

	if (strcmp(method, "PLAIN") != 0)
		return LDAP_STRONG_AUTH_NOT_SUPPORTED;

	if ((req->conn->s_flags & F_SECURE) == 0) {
		log_info("refusing plain sasl bind on insecure connection");
		return LDAP_CONFIDENTIALITY_REQUIRED;
	}

	authzid = creds;
	authcid = memchr(creds, '\0', len);
	if (authcid == NULL || authcid + 2 >= creds + len)
		return LDAP_PROTOCOL_ERROR;
	authcid++;
	password = memchr(authcid, '\0', len - (authcid - creds));
	if (password == NULL || password + 2 >= creds + len)
		return LDAP_PROTOCOL_ERROR;
	password++;

	log_debug("sasl authorization id = [%s]", authzid);
	log_debug("sasl authentication id = [%s]", authcid);

	bzero(&auth_req, sizeof(auth_req));
	if (strlcpy(auth_req.name, authcid, sizeof(auth_req.name)) >=
	    sizeof(auth_req.name))
		return LDAP_OPERATIONS_ERROR;
	if (strlcpy(auth_req.password, password, sizeof(auth_req.password)) >=
	    sizeof(auth_req.password))
		return LDAP_OPERATIONS_ERROR;
	auth_req.fd = req->conn->fd;
	auth_req.msgid = req->msgid;
	bzero(password, strlen(password));

	if (imsg_compose(&iev_ldapd->ibuf, IMSG_LDAPD_AUTH, 0, 0, -1, &auth_req,
	    sizeof(auth_req)) == -1)
		return LDAP_OPERATIONS_ERROR;
	imsg_event_add(iev_ldapd);

	req->conn->bind_req = req;

	return LDAP_SUCCESS;
}

static int
ldap_auth_simple(struct request *req, char *binddn, struct ber_element *auth)
{
	int			 ok = 0;
	char			*password;
	char			*user_password;
	struct namespace	*ns;
	struct ber_element	*elm = NULL, *pw = NULL;

	if (*binddn == '\0') {
		free(req->conn->binddn);		/* anonymous bind */
		req->conn->binddn = NULL;
		log_debug("anonymous bind");
		return LDAP_SUCCESS;
	}

	if ((req->conn->s_flags & F_SECURE) == 0) {
		log_info("refusing non-anonymous bind on insecure connection");
		return LDAP_CONFIDENTIALITY_REQUIRED;
	}

	if (ber_scanf_elements(auth, "s", &password) != 0)
		return LDAP_PROTOCOL_ERROR;

	if (*password == '\0') {
		/* Unauthenticated Authentication Mechanism of Simple Bind */
		log_debug("refusing unauthenticated bind");
		return LDAP_UNWILLING_TO_PERFORM;
	}

	if (conf->rootdn != NULL && strcmp(conf->rootdn, binddn) == 0) {
		if (check_password(conf->rootpw, password) == 0)
			ok = 1;
	} else if ((ns = namespace_lookup_base(binddn, 1)) == NULL) {
		return LDAP_INVALID_CREDENTIALS;
	} else if (ns->rootdn != NULL && strcmp(ns->rootdn, binddn) == 0) {
		if (check_password(ns->rootpw, password) == 0)
			ok = 1;
	} else if (namespace_has_referrals(ns)) {
		return LDAP_INVALID_CREDENTIALS;
	} else {
		if (!authorized(req->conn, ns, ACI_BIND, binddn,
		    LDAP_SCOPE_BASE))
			return LDAP_INSUFFICIENT_ACCESS;

		elm = namespace_get(ns, binddn);
		if (elm == NULL && errno == ESTALE) {
			if (namespace_queue_request(ns, req) != 0)
				return LDAP_BUSY;
			return -1;	/* Database is being reopened. */
		}

		if (elm != NULL)
			pw = ldap_get_attribute(elm, "userPassword");
		if (pw != NULL) {
			for (elm = pw->be_next->be_sub; elm;
			    elm = elm->be_next) {
				if (ber_get_string(elm, &user_password) != 0)
					continue;
				if (check_password(user_password,
				    password) == 0) {
					ok = 1;
					break;
				}
			}
		}
	}

	if (ok) {
		free(req->conn->binddn);
		if ((req->conn->binddn = strdup(binddn)) == NULL)
			return LDAP_OTHER;
		log_debug("successfully authenticated as %s",
		    req->conn->binddn);
		return LDAP_SUCCESS;
	} else
		return LDAP_INVALID_CREDENTIALS;
}

void
ldap_bind_continue(struct conn *conn, int ok)
{
	int			 rc;

	if (ok)
		rc = LDAP_SUCCESS;
	else
		rc = LDAP_INVALID_CREDENTIALS;

	ldap_respond(conn->bind_req, rc);
	conn->bind_req = NULL;
}

int
ldap_bind(struct request *req)
{
	int			 rc = LDAP_OTHER;
	long long		 ver;
	char			*binddn;
	struct ber_element	*auth;

	++stats.req_bind;

	if (ber_scanf_elements(req->op, "{ise", &ver, &binddn, &auth) != 0) {
		rc = LDAP_PROTOCOL_ERROR;
		goto done;
	}

	if (req->conn->bind_req) {
		log_debug("aborting bind in progress with msgid %lld",
		    req->conn->bind_req->msgid);
		request_free(req->conn->bind_req);
		req->conn->bind_req = NULL;
	}

	normalize_dn(binddn);
	log_debug("bind dn = %s", binddn);

	switch (auth->be_type) {
	case LDAP_AUTH_SIMPLE:
		if ((rc = ldap_auth_simple(req, binddn, auth)) < 0)
			return rc;
		break;
	case LDAP_AUTH_SASL:
		if ((rc = ldap_auth_sasl(req, binddn, auth)) == LDAP_SUCCESS)
			return LDAP_SUCCESS;
		break;
	default:
		rc = LDAP_STRONG_AUTH_NOT_SUPPORTED;
		break;
	}

done:
	return ldap_respond(req, rc);
}

