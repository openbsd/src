/*	$OpenBSD: filter.c,v 1.1 2010/05/31 17:36:31 martinh Exp $ */

/*
 * Copyright (c) 2009 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/types.h>

#include <string.h>
#include <stdint.h>

#include "ldapd.h"

static int		 ldap_filt_eq(struct ber_element *root,
				struct ber_element *filter);
static int		 ldap_filt_subs(struct ber_element *root,
				struct ber_element *filter);
static int		 ldap_filt_and(struct ber_element *root,
				struct ber_element *filter);
static int		 ldap_filt_or(struct ber_element *root,
				struct ber_element *filter);
static int		 ldap_filt_not(struct ber_element *root,
				struct ber_element *filter);

static int
ldap_filt_eq(struct ber_element *root, struct ber_element *filter)
{
	const char		*key, *cmp;
	struct ber_element	*a, *vals, *v;

	if (ber_scanf_elements(filter, "{ss", &key, &cmp) != 0)
		return -1;

	a = ldap_get_attribute(root, key);
	if (a == NULL) {
		log_debug("no attribute [%s] found", key);
		return -1;
	}

	vals = a->be_next;
	if (vals == NULL)
		return -1;

	for (v = vals->be_sub; v; v = v->be_next) {
		char *vs;
		if (ber_get_string(v, &vs) != 0)
			continue;
		if (strcasecmp(cmp, vs) == 0)
			return 0;
	}

	return -1;
}

static int
ldap_filt_subs_value(struct ber_element *v, struct ber_element *sub)
{
	int		 class;
	unsigned long	 type;
	const char	*cmpval;
	char		*vs, *p, *end;

	if (ber_get_string(v, &vs) != 0)
		return -1;

	for (; sub; sub = sub->be_next) {
		if (ber_scanf_elements(sub, "ts", &class, &type, &cmpval) != 0)
			return -1;

		if (class != BER_CLASS_CONTEXT)
			return -1;

		switch (type) {
		case LDAP_FILT_SUBS_INIT:
			if (strncasecmp(cmpval, vs, strlen(cmpval)) == 0)
				vs += strlen(cmpval);
			else
				return 1; /* no match */
			break;
		case LDAP_FILT_SUBS_ANY:
			if ((p = strcasestr(vs, cmpval)) != NULL)
				vs = p + strlen(cmpval);
			else
				return 1; /* no match */
			break;
		case LDAP_FILT_SUBS_FIN:
			if (strlen(vs) < strlen(cmpval))
				return 1; /* no match */
			end = vs + strlen(vs) - strlen(cmpval);
			if (strcasecmp(end, cmpval) == 0)
				vs = end + strlen(cmpval);
			else
				return 1; /* no match */
			break;
		default:
			log_warnx("invalid subfilter type %d", type);
			return -1;
		}
	}

	return 0; /* match */
}

static int
ldap_filt_subs(struct ber_element *root, struct ber_element *filter)
{
	const char		*key, *attr;
	struct ber_element	*sub, *a, *v;

	if (ber_scanf_elements(filter, "{s{e", &key, &sub) != 0)
		return -1;

	a = ldap_get_attribute(root, key);
	if (a == NULL)
		return -1; /* attribute not found, false or undefined? */

	if (ber_scanf_elements(a, "s(e", &attr, &v) != 0)
		return -1; /* internal failure, false or undefined? */

	/* Loop through all values, stop if any matches.
	 */
	for (; v; v = v->be_next) {
		/* All substrings must match. */
		switch (ldap_filt_subs_value(v, sub)) {
		case 0:
			return 0;
		case -1:
			return -1;
		default:
			break;
		}
	}

	/* All values checked, no match. */
	return -1;
}

static int
ldap_filt_and(struct ber_element *root, struct ber_element *filter)
{
	struct ber_element	*elm;

	if (ber_scanf_elements(filter, "{e", &elm) != 0)
		return -1;

	for (; elm; elm = elm->be_next) {
		if (ldap_matches_filter(root, elm) != 0)
			return -1;
	}

	return 0;
}

static int
ldap_filt_or(struct ber_element *root, struct ber_element *filter)
{
	struct ber_element	*elm;

	if (ber_scanf_elements(filter, "{e", &elm) != 0) {
		log_warnx("failed to parse search filter");
		return -1;
	}

	for (; elm; elm = elm->be_next) {
		if (ldap_matches_filter(root, elm) == 0)
			return 0;
	}

	return -1;
}

static int
ldap_filt_not(struct ber_element *root, struct ber_element *filter)
{
	struct ber_element	*elm;

	if (ber_scanf_elements(filter, "{e", &elm) != 0) {
		log_warnx("failed to parse search filter");
		return -1;
	}

	for (; elm; elm = elm->be_next) {
		if (ldap_matches_filter(root, elm) != 0)
			return 0;
	}

	return -1;
}

static int
ldap_filt_presence(struct ber_element *root, struct ber_element *filter)
{
	const char		*key;
	struct ber_element	*a;

	if (ber_scanf_elements(filter, "s", &key) != 0) {
		log_warnx("failed to parse presence filter");
		return -1;
	}

	a = ldap_get_attribute(root, key);
	if (a == NULL) {
		log_debug("attribute %s not found", key);
		return -1; /* attribute not found */
	}

	return 0;
}

int
ldap_matches_filter(struct ber_element *root, struct ber_element *filter)
{
	if (filter == NULL)
		return 0;

	if (filter->be_class != BER_CLASS_CONTEXT) {
		log_warnx("invalid class %d in filter", filter->be_class);
		return -1;
	}

	switch (filter->be_type) {
	case LDAP_FILT_EQ:
	case LDAP_FILT_APPR:
		return ldap_filt_eq(root, filter);
	case LDAP_FILT_SUBS:
		return ldap_filt_subs(root, filter);
	case LDAP_FILT_AND:
		return ldap_filt_and(root, filter);
	case LDAP_FILT_OR:
		return ldap_filt_or(root, filter);
	case LDAP_FILT_NOT:
		return ldap_filt_not(root, filter);
	case LDAP_FILT_PRES:
		return ldap_filt_presence(root, filter);
	default:
		log_warnx("filter type %d not implemented", filter->be_type);
		return -1;
	}
}

