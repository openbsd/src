/*	$OpenBSD: validate.c,v 1.2 2010/06/29 02:45:46 martinh Exp $ */

/*
 * Copyright (c) 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <stdlib.h>
#include <string.h>

#include "ldapd.h"

#define OBJ_NAME(obj)	 ((obj)->names ? SLIST_FIRST((obj)->names)->name : \
				(obj)->oid)
#define ATTR_NAME(at)	 OBJ_NAME(at)

static int
validate_required_attributes(struct ber_element *entry, struct object *obj)
{
	struct attr_ptr		*ap;
	struct attr_type	*at;

	log_debug("validating required attributes for object %s",
	    OBJ_NAME(obj));

	if (obj->must == NULL)
		return LDAP_SUCCESS;

	SLIST_FOREACH(ap, obj->must, next) {
		at = ap->attr_type;

		if (ldap_find_attribute(entry, at) == NULL) {
			log_debug("missing required attribute %s",
			    ATTR_NAME(at));
			return LDAP_OBJECT_CLASS_VIOLATION;
		}
	}

	return LDAP_SUCCESS;
}

static int
validate_attribute(struct attr_type *at, struct ber_element *vals)
{
	int			 nvals = 0;
	struct ber_element	*elm;

	if (vals == NULL) {
		log_debug("missing values");
		return LDAP_OTHER;
	}

	if (vals->be_type != BER_TYPE_SET) {
		log_debug("values should be a set");
		return LDAP_OTHER;
	}

	for (elm = vals->be_sub; elm != NULL; elm = elm->be_next) {
		if (elm->be_type != BER_TYPE_OCTETSTRING) {
			log_debug("attribute value not an octet-string");
			return LDAP_PROTOCOL_ERROR;
		}

		if (++nvals > 1 && at->single) {
			log_debug("multiple values for single-valued"
			    " attribute %s", ATTR_NAME(at));
			return LDAP_CONSTRAINT_VIOLATION;
		}
	}

	/* There must be at least one value in an attribute. */
	if (nvals == 0) {
		log_debug("missing value in attribute %s", ATTR_NAME(at));
		return LDAP_CONSTRAINT_VIOLATION;
	}

	return LDAP_SUCCESS;
}

static const char *
attribute_equality(struct attr_type *at)
{
	if (at == NULL)
		return NULL;
	if (at->equality != NULL)
		return at->equality;
	return attribute_equality(at->sup);
}

/* FIXME: doesn't handle escaped characters.
 */
static int
validate_dn(const char *dn, struct ber_element *entry)
{
	char			*copy;
	char			*sup_dn, *na, *dv, *p;
	struct namespace	*ns;
	struct attr_type	*at;
	struct ber_element	*vals;

	if ((copy = strdup(dn)) == NULL)
		return LDAP_OTHER;

	sup_dn = strchr(copy, ',');
	if (sup_dn++ == NULL)
		sup_dn = strrchr(copy, '\0');

	/* Validate naming attributes and distinguished values in the RDN.
	 */
	p = copy;
	for (;p < sup_dn;) {
		na = p;
		p = na + strcspn(na, "=");
		if (p == na || p >= sup_dn) {
			free(copy);
			return LDAP_INVALID_DN_SYNTAX;
		}
		*p = '\0';
		dv = p + 1;
		p = dv + strcspn(dv, "+,");
		if (p == dv) {
			free(copy);
			return LDAP_INVALID_DN_SYNTAX;
		}
		*p++ = '\0';

		log_debug("got naming attribute %s", na);
		log_debug("got distinguished value %s", dv);
		if ((at = lookup_attribute(conf->schema, na)) == NULL) {
			log_debug("attribute %s not defined in schema", na);
			goto fail;
		}
		if (at->usage != USAGE_USER_APP) {
			log_debug("naming attribute %s is operational", na);
			goto fail;
		}
		if (at->collective) {
			log_debug("naming attribute %s is collective", na);
			goto fail;
		}
		if (at->obsolete) {
			log_debug("naming attribute %s is obsolete", na);
			goto fail;
		}
		if (attribute_equality(at) == NULL) {
			log_debug("naming attribute %s doesn't define equality",
			    na);
			goto fail;
		}
		if ((vals = ldap_find_attribute(entry, at)) == NULL) {
			log_debug("missing distinguished value for %s", na);
			goto fail;
		}
		if (ldap_find_value(vals->be_next, dv) == NULL) {
			log_debug("missing distinguished value %s"
			    " in naming attribute %s", dv, na);
			goto fail;
		}
	}

	/* Check that the RDN immediate superior exists, or it is a
	 * top-level namespace.
	 */
	if (*sup_dn != '\0') {
		TAILQ_FOREACH(ns, &conf->namespaces, next) {
			if (strcmp(dn, ns->suffix) == 0)
				goto done;
		}
		log_debug("checking for presence of superior dn %s", sup_dn);
		ns = namespace_for_base(sup_dn);
		if (ns == NULL || !namespace_exists(ns, sup_dn)) {
			free(copy);
			return LDAP_NO_SUCH_OBJECT;
		}
	}

done:
	free(copy);
	return LDAP_SUCCESS;
fail:
	free(copy);
	return LDAP_NAMING_VIOLATION;
}

static int
validate_object_class(struct ber_element *entry, struct object *obj)
{
	struct obj_ptr		*sup;
	int			 rc;

	rc = validate_required_attributes(entry, obj);
	if (rc == LDAP_SUCCESS && obj->sup != NULL) {
		SLIST_FOREACH(sup, obj->sup, next) {
			rc = validate_object_class(entry, sup->object);
			if (rc != LDAP_SUCCESS)
				break;
		}
	}

	return rc;
}

int
validate_entry(const char *dn, struct ber_element *entry, int relax)
{
	int			 rc;
	char			*s;
	struct ber_element	*objclass, *a, *vals;
	struct object		*obj, *structural_obj = NULL;
	struct attr_type	*at;

	if (relax)
		goto rdn;

	/* There must be an objectClass attribute.
	 */
	objclass = ldap_get_attribute(entry, "objectClass");
	if (objclass == NULL) {
		log_debug("missing objectClass attribute");
		return LDAP_OBJECT_CLASS_VIOLATION;
	}

	/* Check objectClass(es) against schema.
	 */
	objclass = objclass->be_next;		/* skip attribute description */
	for (a = objclass->be_sub; a != NULL; a = a->be_next) {
		if (ber_get_string(a, &s) != 0)
			return LDAP_INVALID_SYNTAX;
		if ((obj = lookup_object(conf->schema, s)) == NULL) {
			log_debug("objectClass %s not defined in schema", s);
			return LDAP_NAMING_VIOLATION;
		}
		log_debug("object class %s has kind %d", s, obj->kind);
		if (obj->kind == KIND_STRUCTURAL)
			structural_obj = obj;

		rc = validate_object_class(entry, obj);
		if (rc != LDAP_SUCCESS)
			return rc;
	}

	/* Must have at least one structural object class.
	 */
	if (structural_obj == NULL) {
		log_debug("no structural object class defined");
		return LDAP_OBJECT_CLASS_VIOLATION;
	}

	/* Check all attributes against schema.
	 */
	for (a = entry->be_sub; a != NULL; a = a->be_next) {
		if (ber_scanf_elements(a, "{se{", &s, &vals) != 0)
			return LDAP_INVALID_SYNTAX;
		if ((at = lookup_attribute(conf->schema, s)) == NULL) {
			log_debug("attribute %s not defined in schema", s);
			return LDAP_NAMING_VIOLATION;
		}
		if ((rc = validate_attribute(at, vals)) != LDAP_SUCCESS)
			return rc;
	}

rdn:
	if ((rc = validate_dn(dn, entry)) != LDAP_SUCCESS)
		return rc;

	return LDAP_SUCCESS;
}

