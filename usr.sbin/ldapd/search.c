/*	$OpenBSD: search.c,v 1.7 2010/06/29 02:45:46 martinh Exp $ */

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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/tree.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ldapd.h"

#define	MAX_SEARCHES	 200

void			 filter_free(struct plan *filter);
static int		 search_result(const char *dn,
				size_t dnlen,
				struct ber_element *attrs,
				struct search *search);

static int
uniqdn_cmp(struct uniqdn *a, struct uniqdn *b)
{
	if (a->key.size < b->key.size)
		return -1;
	if (a->key.size > b->key.size)
		return +1;
	return memcmp(a->key.data, b->key.data, a->key.size);
}

RB_GENERATE(dn_tree, uniqdn, link, uniqdn_cmp);

/* Return true if the attribute is operational.
 */
static int
is_operational(char *adesc)
{
	struct attr_type	*at;

	at = lookup_attribute(conf->schema, adesc);
	if (at)
		return at->usage != USAGE_USER_APP;

	return 0;
}

/* Return true if attr should be included in search entry.
 */
static int
should_include_attribute(struct ber_element *attr, struct search *search)
{
	char			*adesc, *fdesc;
	struct ber_element	*elm;

	if (ber_get_string(attr->be_sub, &adesc) != 0)
		return 0;

	if (search->attrlist->be_sub->be_encoding == BER_TYPE_EOC) {
		/* An empty list with no attributes requests the return of
		 * all user attributes. */
		return !is_operational(adesc);
	}

	for (elm = search->attrlist->be_sub; elm; elm = elm->be_next) {
		if (ber_get_string(elm, &fdesc) != 0)
			continue;
		if (strcasecmp(fdesc, adesc) == 0)
			return 1;
		if (strcmp(fdesc, "*") == 0 && !is_operational(adesc))
			return 1;
		if (strcmp(fdesc, "+") == 0 && is_operational(adesc))
			return 1;
	}

	return 0;
}

static int
search_result(const char *dn, size_t dnlen, struct ber_element *attrs,
    struct search *search)
{
	int			 rc;
	struct conn		*conn = search->conn;
	struct ber_element	*root, *elm, *filtered_attrs = NULL, *link, *a;
	struct ber_element	*prev, *next;
	void			*buf;

	if ((root = ber_add_sequence(NULL)) == NULL)
		goto fail;

	if ((filtered_attrs = ber_add_sequence(NULL)) == NULL)
		goto fail;
	link = filtered_attrs;

	for (prev = NULL, a = attrs->be_sub; a; a = next) {
		if (should_include_attribute(a, search)) {
			next = a->be_next;
			if (prev != NULL)
				prev->be_next = a->be_next;	/* unlink a */
			else
				attrs->be_sub = a->be_next;
			a->be_next = NULL;			/* break chain*/
			ber_link_elements(link, a);
			link = a;
		} else {
			prev = a;
			next = a->be_next;
		}
	}

	elm = ber_printf_elements(root, "i{txe", search->req->msgid,
		BER_CLASS_APP, (unsigned long)LDAP_RES_SEARCH_ENTRY,
		dn, dnlen, filtered_attrs);
	if (elm == NULL)
		goto fail;

	rc = ber_write_elements(&conn->ber, root);
	ber_free_elements(root);

	if (rc < 0) {
		log_warn("failed to create search-entry response");
		return -1;
	}

	ber_get_writebuf(&conn->ber, &buf);
	if (bufferevent_write(conn->bev, buf, rc) != 0) {
		log_warn("failed to send ldap result");
		return -1;
	}

	return 0;
fail:
	log_warn("search result");
	if (root)
		ber_free_elements(root);
	return -1;
}

void
search_close(struct search *search)
{
	struct uniqdn	*dn, *next;

	for (dn = RB_MIN(dn_tree, &search->uniqdns); dn; dn = next) {
		next = RB_NEXT(dn_tree, &search->uniqdns, dn);
		RB_REMOVE(dn_tree, &search->uniqdns, dn);
		free(dn->key.data);
		free(dn);
	}

	btree_cursor_close(search->cursor);
	btree_txn_abort(search->data_txn);
	btree_txn_abort(search->indx_txn);

	if (search->req != NULL) {
		log_debug("finished search on msgid %lld", search->req->msgid);
		request_free(search->req);
	}
	TAILQ_REMOVE(&search->conn->searches, search, next);
	filter_free(search->plan);
	free(search);
	--stats.searches;
}

/* Returns true (1) if key is a direct subordinate of base.
 */
int
is_child_of(struct btval *key, const char *base)
{
	size_t		 ksz, bsz;
	char		*p;

	if ((p = memchr(key->data, ',', key->size)) == NULL)
		return 0;
	p++;
	ksz = key->size - (p - (char *)key->data);
	bsz = strlen(base);
	return (ksz == bsz && bcmp(p, base, ksz) == 0);
}

static int
check_search_entry(struct btval *key, struct btval *val, struct search *search)
{
	int			 rc;
	char			*dn0;
	struct ber_element	*elm;

	/* verify entry is a direct subordinate of basedn */
	if (search->scope == LDAP_SCOPE_ONELEVEL &&
	    !is_child_of(key, search->basedn)) {
		log_debug("not a direct subordinate of base");
		return 0;
	}

	if ((dn0 = malloc(key->size + 1)) == NULL) {
		log_warn("malloc");
		return 0;
	}
	strncpy(dn0, key->data, key->size);
	dn0[key->size] = 0;
	if (!authorized(search->conn, search->ns, ACI_READ, dn0,
	    LDAP_SCOPE_BASE)) {
		/* LDAP_INSUFFICIENT_ACCESS */
		free(dn0);
		return 0;
	}
	free(dn0);

	if ((elm = namespace_db2ber(search->ns, val)) == NULL) {
		log_warnx("failed to parse entry [%.*s]",
		    (int)key->size, (char *)key->data);
		return 0;
	}

	if (ldap_matches_filter(elm, search->filter) != 0) {
		ber_free_elements(elm);
		return 0;
	}

	rc = search_result(key->data, key->size, elm, search);
	ber_free_elements(elm);

	if (rc == 0)
		search->nmatched++;

	return rc;
}

static int
mk_dup(struct search *search, struct btval *key)
{
	struct uniqdn		*udn;

	if ((udn = calloc(1, sizeof(*udn))) == NULL)
		return BT_FAIL;

	if ((udn->key.data = malloc(key->size)) == NULL) {
		free(udn);
		return BT_FAIL;
	}
	bcopy(key->data, udn->key.data, key->size);
	udn->key.size = key->size;
	RB_INSERT(dn_tree, &search->uniqdns, udn);
	return BT_SUCCESS;
}

/* check if this entry was already sent */
static int
is_dup(struct search *search, struct btval *key)
{
	struct uniqdn		find;

	find.key.data = key->data;
	find.key.size = key->size;
	return RB_FIND(dn_tree, &search->uniqdns, &find) != NULL;
}

void
conn_search(struct search *search)
{
	int			 i, rc = BT_SUCCESS;
	unsigned int		 reason = LDAP_SUCCESS;
	unsigned int		 op = BT_NEXT;
	time_t			 now;
	struct conn		*conn;
	struct btree_txn	*txn;
	struct btval		 key, ikey, val;

	conn = search->conn;

	bzero(&key, sizeof(key));
	bzero(&val, sizeof(val));

	if (search->plan->indexed)
		txn = search->indx_txn;
	else
		txn = search->data_txn;

	if (!search->init) {
		search->cursor = btree_txn_cursor_open(NULL, txn);
		if (search->cursor == NULL) {
			log_warn("btree_cursor_open");
			search_close(search);
			return;
		}

		if (search->plan->indexed) {
			search->cindx = TAILQ_FIRST(&search->plan->indices);
			key.data = search->cindx->prefix;
			log_debug("init index scan on [%s]", key.data);
		} else {
			if (*search->basedn)
				key.data = search->basedn;
			log_debug("init full scan");
		}

		if (key.data) {
			key.size = strlen(key.data);
			op = BT_CURSOR;
		}

		search->init = 1;
	}

	for (i = 0; i < 10 && rc == BT_SUCCESS; i++) {
		rc = btree_cursor_get(search->cursor, &key, &val, op);
		op = BT_NEXT;

		if (rc == BT_SUCCESS && search->plan->indexed) {
			log_debug("found index %.*s", key.size, key.data);

			if (!has_prefix(&key, search->cindx->prefix)) {
				log_debug("scanned past index prefix [%s]",
				    search->cindx->prefix);
				btval_reset(&val);
				btval_reset(&key);
				rc = BT_FAIL;
				errno = ENOENT;
			}
		}

		if (rc == BT_FAIL && errno == ENOENT &&
		    search->plan->indexed > 1) {
			search->cindx = TAILQ_NEXT(search->cindx, next);
			if (search->cindx != NULL) {
				rc = BT_SUCCESS;
				bzero(&key, sizeof(key));
				key.data = search->cindx->prefix;
				key.size = strlen(key.data);
				log_debug("re-init cursor on [%s]", key.data);
				op = BT_CURSOR;
				continue;
			}
		}

		if (rc != BT_SUCCESS) {
			if (errno != ENOENT) {
				log_warnx("btree failure");
				reason = LDAP_OTHER;
			}
			break;
		}

		search->nscanned++;

		if (search->plan->indexed) {
			bcopy(&key, &ikey, sizeof(key));
			bzero(&key, sizeof(key));
			btval_reset(&val);

			rc = index_to_dn(search->ns, &ikey, &key);
			btval_reset(&ikey);
			if (rc != 0) {
				reason = LDAP_OTHER;
				break;
			}

			log_debug("lookup indexed key [%.*s]",
			    (int)key.size, (char *)key.data);

			/* verify entry is a direct subordinate */
			if (search->scope == LDAP_SCOPE_ONELEVEL &&
			    !is_child_of(&key, search->basedn)) {
				log_debug("not a direct subordinate of base");
				btval_reset(&key);
				continue;
			}

			if (search->plan->indexed > 1 && is_dup(search, &key)) {
				log_debug("skipping duplicate dn %.*s",
				    (int)key.size, (char *)key.data);
				search->ndups++;
				btval_reset(&key);
				continue;
			}

			rc = btree_txn_get(NULL, search->data_txn, &key, &val);
			if (rc == BT_FAIL) {
				if (errno == ENOENT) {
					log_warnx("indexed key [%.*s]"
					    " doesn't exist!",
					    (int)key.size, (char *)key.data);
					btval_reset(&key);
					rc = BT_SUCCESS;
					continue;
				}
				log_warnx("btree failure");
				btval_reset(&key);
				reason = LDAP_OTHER;
				break;
			}
		}

		log_debug("found dn %.*s", (int)key.size, (char *)key.data);

		if (!has_suffix(&key, search->basedn)) {
			btval_reset(&val);
			btval_reset(&key);
			if (search->plan->indexed)
				continue;
			else {
				log_debug("scanned past basedn suffix");
				rc = 1;
				break;
			}
		}

		rc = check_search_entry(&key, &val, search);
		btval_reset(&val);
		if (rc == BT_SUCCESS && search->plan->indexed > 1)
			rc = mk_dup(search, &key);

		btval_reset(&key);

		/* Check if we have passed the size limit. */
		if (rc == BT_SUCCESS && search->szlim > 0 &&
		    search->nmatched >= search->szlim) {
			log_debug("search %d/%lld has reached size limit (%u)",
			    search->conn->fd, search->req->msgid,
			    search->szlim);
			reason = LDAP_SIZELIMIT_EXCEEDED;
			rc = BT_FAIL;
		}
	}

	/* Check if we have passed the time limit. */
	now = time(0);
	if (rc == 0 && search->tmlim > 0 &&
	    search->started_at + search->tmlim <= now) {
		log_debug("search %d/%lld has reached time limit (%u)",
		    search->conn->fd, search->req->msgid,
		    search->tmlim);
		reason = LDAP_TIMELIMIT_EXCEEDED;
		rc = 1;
		++stats.timeouts;
	}

	if (rc == 0) {
		bufferevent_enable(search->conn->bev, EV_WRITE);
	} else {
		log_debug("%u scanned, %u matched, %u dups",
		    search->nscanned, search->nmatched, search->ndups);
		send_ldap_result(conn, search->req->msgid,
		    LDAP_RES_SEARCH_RESULT, reason);
		if (errno != ENOENT)
			log_debug("search failed: %s", strerror(errno));
		search_close(search);
	}
}

static void
ldap_search_root_dse(struct search *search)
{
	struct namespace	*ns;
	struct ber_element	*root, *elm, *key, *val;

	if ((root = ber_add_sequence(NULL)) == NULL) {
		return;
	}

	elm = ber_add_sequence(root);
	key = ber_add_string(elm, "objectClass");
	val = ber_add_set(key);
	ber_add_string(val, "top");

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "supportedLDAPVersion");
	val = ber_add_set(key);
	ber_add_string(val, "3");

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "namingContexts");
	val = ber_add_set(key);
	TAILQ_FOREACH(ns, &conf->namespaces, next)
		val = ber_add_string(val, ns->suffix);

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "supportedExtension");
	val = ber_add_set(key);
	val = ber_add_string(val, "1.3.6.1.4.1.1466.20037");	/* StartTLS */

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "supportedFeatures");
	val = ber_add_set(key);
	/* All Operational Attributes (RFC 3673) */
	val = ber_add_string(val, "1.3.6.1.4.1.4203.1.5.1");

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "subschemaSubentry");
	val = ber_add_set(key);
	ber_add_string(val, "cn=schema");

	if ((search->conn->s_flags & F_SECURE) == F_SECURE) {
		elm = ber_add_sequence(elm);
		key = ber_add_string(elm, "supportedSASLMechanisms");
		val = ber_add_set(key);
		ber_add_string(val, "PLAIN");
	}

	search_result("", 0, root, search);
	ber_free_elements(root);
	send_ldap_result(search->conn, search->req->msgid,
	    LDAP_RES_SEARCH_RESULT, LDAP_SUCCESS);
	search_close(search);
}

static void
ldap_search_subschema(struct search *search)
{
	struct ber_element	*root, *elm, *key, *val;

	if ((root = ber_add_sequence(NULL)) == NULL) {
		return;
	}

	elm = ber_add_sequence(root);
	key = ber_add_string(elm, "objectClass");
	val = ber_add_set(key);
	val = ber_add_string(val, "top");
	ber_add_string(val, "subschema");

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "createTimestamp");
	val = ber_add_set(key);
	ber_add_string(val, ldap_strftime(stats.started_at));

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "modifyTimestamp");
	val = ber_add_set(key);
	ber_add_string(val, ldap_strftime(stats.started_at));

	elm = ber_add_sequence(elm);
	key = ber_add_string(elm, "subschemaSubentry");
	val = ber_add_set(key);
	ber_add_string(val, "cn=schema");

	search_result("cn=schema", 9, root, search);
	ber_free_elements(root);
	send_ldap_result(search->conn, search->req->msgid,
	    LDAP_RES_SEARCH_RESULT, LDAP_SUCCESS);
	search_close(search);
}

static int
add_index(struct plan *plan, const char *fmt, ...)
{
	struct index		*indx;
	va_list			 ap;

	if ((indx = calloc(1, sizeof(*indx))) == NULL)
		return -1;

	va_start(ap, fmt);
	vasprintf(&indx->prefix, fmt, ap);
	va_end(ap);

	normalize_dn(indx->prefix);

	TAILQ_INSERT_TAIL(&plan->indices, indx, next);
	plan->indexed++;

	return 0;
}

static struct plan *
search_planner(struct namespace *ns, struct ber_element *filter)
{
	int			 class;
	unsigned long		 type;
	char			*s, *attr;
	struct ber_element	*elm;
	struct index		*indx;
	struct plan		*plan, *arg = NULL;

	if (filter->be_class != BER_CLASS_CONTEXT) {
		log_warnx("invalid class %d in filter", filter->be_class);
		return NULL;
	}

	if ((plan = calloc(1, sizeof(*plan))) == NULL) {
		log_warn("search_planner: calloc");
		return NULL;
	}
	TAILQ_INIT(&plan->args);
	TAILQ_INIT(&plan->indices);

	switch (filter->be_type) {
	case LDAP_FILT_EQ:
	case LDAP_FILT_APPR:
		if (ber_scanf_elements(filter, "{ss", &attr, &s) != 0)
			goto fail;
		if (namespace_has_index(ns, attr, INDEX_EQUAL))
			add_index(plan, "%s=%s,", attr, s);
		break;
	case LDAP_FILT_SUBS:
		if (ber_scanf_elements(filter, "{s{ts",
		    &attr, &class, &type, &s) != 0)
			goto fail;
		if (class == BER_CLASS_CONTEXT && type == LDAP_FILT_SUBS_INIT) {
			/* only prefix substrings usable for index */
			if (namespace_has_index(ns, attr, INDEX_EQUAL))
				add_index(plan, "%s=%s", attr, s);
		}
		break;
	case LDAP_FILT_PRES:
		if (ber_scanf_elements(filter, "s", &attr) != 0)
			goto fail;
		if (strcasecmp(attr, "objectClass") != 0) {
			if (namespace_has_index(ns, attr, INDEX_PRESENCE))
				add_index(plan, "!%s,", attr);
		}
		break;
	case LDAP_FILT_AND:
		if (ber_scanf_elements(filter, "{e", &elm) != 0)
			goto fail;
		for (; elm; elm = elm->be_next) {
			if ((arg = search_planner(ns, elm)) == NULL)
				goto fail;
			TAILQ_INSERT_TAIL(&plan->args, arg, next);
		}
		/* select an index to use */
		TAILQ_FOREACH(arg, &plan->args, next) {
			if (arg->indexed) {
				while ((indx = TAILQ_FIRST(&arg->indices))) {
					TAILQ_REMOVE(&arg->indices, indx, next);
					TAILQ_INSERT_TAIL(&plan->indices, indx,
					    next);
				}
				plan->indexed = arg->indexed;
				break;
			}
		}
		break;
	case LDAP_FILT_OR:
		if (ber_scanf_elements(filter, "{e", &elm) != 0)
			goto fail;
		for (; elm; elm = elm->be_next) {
			if ((arg = search_planner(ns, elm)) == NULL)
				goto fail;
			TAILQ_INSERT_TAIL(&plan->args, arg, next);
		}
		TAILQ_FOREACH(arg, &plan->args, next) {
			if (!arg->indexed) {
				plan->indexed = 0;
				break;
			}
			while ((indx = TAILQ_FIRST(&arg->indices))) {
				TAILQ_REMOVE(&arg->indices, indx, next);
				TAILQ_INSERT_TAIL(&plan->indices, indx,next);
				plan->indexed++;
			}
		}
		break;
	default:
		log_warnx("filter type %d not implemented", filter->be_type);
		break;
	}

	return plan;

fail:
	free(plan);
	return NULL;
}

void
filter_free(struct plan *filter)
{
	struct index		*indx;
	struct plan		*arg;

	if (filter) {
		while ((arg = TAILQ_FIRST(&filter->args)) != NULL) {
			TAILQ_REMOVE(&filter->args, arg, next);
			filter_free(arg);
		}
		while ((indx = TAILQ_FIRST(&filter->indices)) != NULL) {
			TAILQ_REMOVE(&filter->indices, indx, next);
			free(indx->prefix);
			free(indx);
		}
		free(filter);
	}
}

int
ldap_search(struct request *req)
{
	long long		 reason = LDAP_OTHER;
	struct search		*search = NULL;
	int			 rc;

	if (stats.searches > MAX_SEARCHES) {
		log_warnx("refusing more than %u concurrent searches",
		    MAX_SEARCHES);
		reason = LDAP_BUSY;
		goto done;
	}
	++stats.searches;
	++stats.req_search;

	if ((search = calloc(1, sizeof(*search))) == NULL)
		return -1;
	search->req = req;
	search->conn = req->conn;
	search->init = 0;
	search->started_at = time(0);
	TAILQ_INSERT_HEAD(&req->conn->searches, search, next);
	RB_INIT(&search->uniqdns);

	if (ber_scanf_elements(req->op, "{sEEiibeSeS",
	    &search->basedn,
	    &search->scope,
	    &search->deref,
	    &search->szlim,
	    &search->tmlim,
	    &search->typesonly,
	    &search->filter,
	    &search->attrlist) != 0)
	{
		log_warnx("failed to parse search request");
		reason = LDAP_PROTOCOL_ERROR;
		goto done;
	}

	normalize_dn(search->basedn);
	log_debug("base dn = %s, scope = %d", search->basedn, search->scope);

	if (*search->basedn == '\0') {
		/* request for the root DSE */
		if (!authorized(req->conn, NULL, ACI_READ, "",
		    LDAP_SCOPE_BASE)) {
			reason = LDAP_INSUFFICIENT_ACCESS;
			goto done;
		}
		if (search->scope != LDAP_SCOPE_BASE) {
			/* only base searches are valid */
			reason = LDAP_NO_SUCH_OBJECT;
			goto done;
		}
		/* TODO: verify filter is (objectClass=*) */
		ldap_search_root_dse(search);
		return 0;
	}

	if (strcasecmp(search->basedn, "cn=schema") == 0) {
		/* request for the subschema subentries */
		if (!authorized(req->conn, NULL, ACI_READ,
		    "cn=schema", LDAP_SCOPE_BASE)) {
			reason = LDAP_INSUFFICIENT_ACCESS;
			goto done;
		}
		if (search->scope != LDAP_SCOPE_BASE) {
			/* only base searches are valid */
			reason = LDAP_NO_SUCH_OBJECT;
			goto done;
		}
		/* TODO: verify filter is (objectClass=subschema) */
		ldap_search_subschema(search);
		return 0;
	}

	if ((search->ns = namespace_for_base(search->basedn)) == NULL) {
		log_debug("no database configured for suffix %s",
		    search->basedn);
		reason = LDAP_NO_SUCH_OBJECT;
		goto done;
	}

	if (!authorized(req->conn, search->ns, ACI_READ,
	    search->basedn, search->scope)) {
		reason = LDAP_INSUFFICIENT_ACCESS;
		goto done;
	}

	if ((rc = namespace_begin_txn(search->ns, &search->data_txn,
	    &search->indx_txn, 1)) != BT_SUCCESS) {
		if (errno == EBUSY) {
			if (namespace_queue_request(search->ns, req) != 0) {
				reason = LDAP_BUSY;
				goto done;
			}
			search->req = NULL;	/* keep the scheduled request */
			search_close(search);
			return 0;
		}
		reason = LDAP_OTHER;
		goto done;
	}

	if (search->scope == LDAP_SCOPE_BASE) {
		struct btval		 key, val;

		bzero(&key, sizeof(key));
		bzero(&val, sizeof(val));
		key.data = search->basedn;
		key.size = strlen(key.data);

		if (btree_txn_get(NULL, search->data_txn, &key, &val) == 0) {
			check_search_entry(&key, &val, search);
			btval_reset(&val);
			reason = LDAP_SUCCESS;
		} else if (errno == ENOENT)
			reason = LDAP_SUCCESS;
		else
			reason = LDAP_OTHER;
		goto done;
	}

	search->plan = search_planner(search->ns, search->filter);
	if (search->plan == NULL) {
		reason = LDAP_PROTOCOL_ERROR;
		goto done;
	}

	if (!search->plan->indexed && search->scope == LDAP_SCOPE_ONELEVEL) {
		int	 sz;
		sz = strlen(search->basedn) - strlen(search->ns->suffix);
		if (sz > 0 && search->basedn[sz - 1] == ',')
			sz--;
		add_index(search->plan, "@%.*s,", sz, search->basedn);
	}

	if (!search->plan->indexed)
		++stats.unindexed;

	bufferevent_enable(req->conn->bev, EV_WRITE);
	return 0;

done:
	send_ldap_result(req->conn, req->msgid, LDAP_RES_SEARCH_RESULT, reason);
	if (search)
		search_close(search);
	return 0;
}

