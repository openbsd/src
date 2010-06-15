/*	$OpenBSD: modify.c,v 1.2 2010/06/15 15:47:56 martinh Exp $ */

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ldapd.h"
#include "uuid.h"

int
ldap_delete(struct request *req)
{
	int			 rc;
	char			*dn;
	struct namespace	*ns;

	++stats.req_mod;

	if (ber_scanf_elements(req->op, "s", &dn) != 0)
		return ldap_respond(req, LDAP_PROTOCOL_ERROR);

	normalize_dn(dn);
	log_debug("deleting entry %s", dn);

	if ((ns = namespace_for_base(dn)) == NULL)
		return ldap_respond(req, LDAP_NAMING_VIOLATION);

	if (!authorized(req->conn, ns, ACI_WRITE, dn, LDAP_SCOPE_BASE))
		return ldap_respond(req, LDAP_INSUFFICIENT_ACCESS);

	if ((rc = namespace_begin(ns)) == BT_DEAD) {
		if (namespace_queue_request(ns, req) != 0)
			return ldap_respond(req, LDAP_BUSY);
		return LDAP_BUSY;
	} else if (rc != BT_SUCCESS)
		return ldap_respond(req, LDAP_OTHER);

	switch (namespace_del(ns, dn)) {
	case BT_NOTFOUND:
		rc = LDAP_NO_SUCH_OBJECT;
		break;
	case BT_SUCCESS:
		rc = LDAP_SUCCESS;
		break;
	default:
		rc = LDAP_OTHER;
		break;
	}

	namespace_commit(ns);
	if (rc >= 0)
		ldap_respond(req, rc);
	return rc;
}

int
ldap_add(struct request *req)
{
	char			 uuid_str[64];
	struct uuid		 uuid;
	char			*dn;
	struct ber_element	*attrs, *set;
	struct namespace	*ns;
	int			 rc;

	++stats.req_mod;

	if (ber_scanf_elements(req->op, "{se", &dn, &attrs) != 0)
		return ldap_respond(req, LDAP_PROTOCOL_ERROR);

	normalize_dn(dn);
	log_debug("adding entry %s", dn);

	if (*dn == '\0')
		return ldap_respond(req, LDAP_INVALID_DN_SYNTAX);

	if ((ns = namespace_for_base(dn)) == NULL)
		return ldap_respond(req, LDAP_NAMING_VIOLATION);

	if (!authorized(req->conn, ns, ACI_WRITE, dn, LDAP_SCOPE_BASE) != 0)
		return ldap_respond(req, LDAP_INSUFFICIENT_ACCESS);

	if ((rc = namespace_begin(ns)) == BT_DEAD) {
		if (namespace_queue_request(ns, req) != 0)
			return ldap_respond(req, LDAP_BUSY);
		return LDAP_BUSY;
	} else if (rc != BT_SUCCESS)
		return ldap_respond(req, LDAP_OTHER);

	/* add operational attributes
	 */
	set = ber_add_set(NULL);
	ber_add_string(set, req->conn->binddn ?: "");
	ldap_add_attribute(attrs, "creatorsName", set);

	set = ber_add_set(NULL);
	ber_add_string(set, ldap_now());
	ldap_add_attribute(attrs, "createTimestamp", set);

	uuid_create(&uuid);
	uuid_to_string(&uuid, uuid_str, sizeof(uuid_str));
	set = ber_add_set(NULL);
	ber_add_string(set, uuid_str);
	ldap_add_attribute(attrs, "entryUUID", set);

	if ((rc = validate_entry(dn, attrs, ns->relax)) != LDAP_SUCCESS)
		goto done;

	switch (namespace_add(ns, dn, attrs)) {
	case BT_SUCCESS:
		rc = LDAP_SUCCESS;
		break;
	case BT_EXISTS:
		rc = LDAP_ALREADY_EXISTS;
		break;
	default:
		rc = LDAP_OTHER;
		break;
	}

done:
	namespace_commit(ns);
	if (rc >= 0)
		ldap_respond(req, rc);
	return rc;
}

int
ldap_modify(struct request *req)
{
	int			 rc;
	char			*dn;
	long long		 op;
	const char		*attr;
	struct ber_element	*mods, *entry, *mod, *vals, *a, *set;
	struct namespace	*ns;
	struct attr_type	*at;

	++stats.req_mod;

	if (ber_scanf_elements(req->op, "{se", &dn, &mods) != 0)
		return ldap_respond(req, LDAP_PROTOCOL_ERROR);

	normalize_dn(dn);
	log_debug("modifying dn %s", dn);

	if (*dn == 0)
		return ldap_respond(req, LDAP_INVALID_DN_SYNTAX);

	if ((ns = namespace_for_base(dn)) == NULL)
		return ldap_respond(req, LDAP_NAMING_VIOLATION);

	if (!authorized(req->conn, ns, ACI_WRITE, dn, LDAP_SCOPE_BASE) != 0)
		return ldap_respond(req, LDAP_INSUFFICIENT_ACCESS);

	if ((rc = namespace_begin(ns)) == BT_DEAD) {
		if (namespace_queue_request(ns, req) != 0)
			return ldap_respond(req, LDAP_BUSY);
		return LDAP_BUSY;
	} else if (rc != BT_SUCCESS)
		return ldap_respond(req, LDAP_OTHER);

	if ((entry = namespace_get(ns, dn)) == NULL) {
		rc = LDAP_NO_SUCH_OBJECT;
		goto done;
	}

	for (mod = mods->be_sub; mod; mod = mod->be_next) {
		if (ber_scanf_elements(mod, "{E{se", &op, &attr, &vals) != 0) {
			rc = LDAP_PROTOCOL_ERROR;
			goto done;
		}

		if ((at = lookup_attribute(attr)) == NULL && !ns->relax) {
			log_debug("unknown attribute type %s", attr);
			rc = LDAP_NO_SUCH_ATTRIBUTE;
			goto done;
		}
		if (at != NULL && at->immutable) {
			log_debug("attempt to modify immutable attribute %s",
			    attr);
			rc = LDAP_CONSTRAINT_VIOLATION;
			goto done;
		}
		if (at != NULL && at->usage != USAGE_USER_APP) {
			log_debug("attempt to modify operational attribute %s",
			    attr);
			rc = LDAP_CONSTRAINT_VIOLATION;
			goto done;
		}

		a = ldap_get_attribute(entry, attr);

		switch (op) {
		case LDAP_MOD_ADD:
			if (a == NULL)
				ldap_add_attribute(entry, attr, vals);
			else
				ldap_merge_values(a, vals);
			break;
		case LDAP_MOD_DELETE:
			if (vals->be_sub &&
			    vals->be_sub->be_type == BER_TYPE_SET)
				ldap_del_values(a, vals);
			else
				ldap_del_attribute(entry, attr);
			break;
		case LDAP_MOD_REPLACE:
			if (vals->be_sub) {
				if (a == NULL)
					ldap_add_attribute(entry, attr, vals);
				else
					ldap_set_values(a, vals);
			} else if (a == NULL)
				ldap_del_attribute(entry, attr);
			break;
		}
	}

	if ((rc = validate_entry(dn, entry, ns->relax)) != LDAP_SUCCESS)
		goto done;

	set = ber_add_set(NULL);
	ber_add_string(set, req->conn->binddn ?: "");
	if ((a = ldap_get_attribute(entry, "modifiersName")) != NULL)
		ldap_set_values(a, set);
	else
		ldap_add_attribute(entry, "modifiersName", set);

	set = ber_add_set(NULL);
	ber_add_string(set, ldap_now());
	if ((a = ldap_get_attribute(entry, "modifyTimestamp")) != NULL)
		ldap_set_values(a, set);
	else
		ldap_add_attribute(entry, "modifyTimestamp", set);

	switch (namespace_update(ns, dn, entry)) {
	case BT_SUCCESS:
		rc = LDAP_SUCCESS;
		break;
	case BT_EXISTS:
		rc = LDAP_ALREADY_EXISTS;
		break;
	default:
		rc = LDAP_OTHER;
		break;
	}

done:
	namespace_commit(ns);
	if (rc >= 0)
		ldap_respond(req, rc);
	return rc;
}

