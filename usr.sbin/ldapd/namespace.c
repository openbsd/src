/*	$OpenBSD: namespace.c,v 1.6 2010/06/15 15:47:56 martinh Exp $ */

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ldapd.h"

/* Maximum number of requests to queue per namespace during compaction.
 * After this many requests, we return LDAP_BUSY.
 */
#define MAX_REQUEST_QUEUE	 10000

static struct btval	*namespace_find(struct namespace *ns, char *dn);
static void		 namespace_queue_replay(int fd, short event, void *arg);
static int		 namespace_set_fd(struct namespace *ns,
			    struct btree **bt, int fd, unsigned int flags);

struct namespace *
namespace_new(const char *suffix)
{
	struct namespace		*ns;

	if ((ns = calloc(1, sizeof(*ns))) == NULL)
		return NULL;
	ns->suffix = strdup(suffix);
	ns->sync = 1;
	if (ns->suffix == NULL) {
		free(ns->suffix);
		free(ns);
		return NULL;
	}
	TAILQ_INIT(&ns->indices);
	TAILQ_INIT(&ns->request_queue);
	SIMPLEQ_INIT(&ns->acl);

	return ns;
}

int
namespace_begin_txn(struct namespace *ns, struct btree_txn **data_txn,
    struct btree_txn **indx_txn, int rdonly)
{
	int	rc = BT_FAIL;

	if (ns->data_db == NULL || ns->indx_db == NULL)
		return BT_DEAD;

	if ((*data_txn = btree_txn_begin(ns->data_db, rdonly)) == NULL ||
	    (*indx_txn = btree_txn_begin(ns->indx_db, rdonly)) == NULL) {
		if (errno == EAGAIN) {
			if (*data_txn == NULL)
				namespace_reopen_data(ns);
			else
				namespace_reopen_indx(ns);
			rc = BT_DEAD;
		}
		log_warn("failed to open transaction");
		btree_txn_abort(*data_txn);
		*data_txn = NULL;
		return rc;
	}

	return BT_SUCCESS;
}

int
namespace_begin(struct namespace *ns)
{
	return namespace_begin_txn(ns, &ns->data_txn, &ns->indx_txn, 0);
}

int
namespace_commit(struct namespace *ns)
{
	if (ns->indx_txn != NULL &&
	    btree_txn_commit(ns->indx_txn) != BT_SUCCESS) {
		log_warn("%s(indx): commit failed", ns->suffix);
		btree_txn_abort(ns->data_txn);
		ns->indx_txn = ns->data_txn = NULL;
		return -1;
	}
	ns->indx_txn = NULL;

	if (ns->data_txn != NULL &&
	    btree_txn_commit(ns->data_txn) != BT_SUCCESS) {
		log_warn("%s(data): commit failed", ns->suffix);
		ns->data_txn = NULL;
		return -1;
	}
	ns->data_txn = NULL;

	return 0;
}

void
namespace_abort(struct namespace *ns)
{
	btree_txn_abort(ns->data_txn);
	ns->data_txn = NULL;

	btree_txn_abort(ns->indx_txn);
	ns->indx_txn = NULL;
}

int
namespace_open(struct namespace *ns)
{
	unsigned int	 db_flags = 0;

	assert(ns);
	assert(ns->suffix);

	if (ns->sync == 0)
		db_flags |= BT_NOSYNC;

	if (asprintf(&ns->data_path, "%s/%s_data.db", DATADIR, ns->suffix) < 0)
		return -1;
	log_info("opening namespace %s", ns->suffix);
	ns->data_db = btree_open(ns->data_path, db_flags | BT_REVERSEKEY, 0644);
	if (ns->data_db == NULL)
		return -1;

	btree_set_cache_size(ns->data_db, ns->cache_size);

	if (asprintf(&ns->indx_path, "%s/%s_indx.db", DATADIR, ns->suffix) < 0)
		return -1;
	ns->indx_db = btree_open(ns->indx_path, db_flags, 0644);
	if (ns->indx_db == NULL)
		return -1;

	btree_set_cache_size(ns->indx_db, ns->index_cache_size);

	/* prepare request queue scheduler */
	evtimer_set(&ns->ev_queue, namespace_queue_replay, ns);

	return 0;
}

static int
namespace_reopen(const char *path)
{
	struct open_req		 req;

	log_debug("asking parent to open %s", path);

	bzero(&req, sizeof(req));
	if (strlcpy(req.path, path, sizeof(req.path)) >= sizeof(req.path)) {
		log_warnx("%s: path truncated", __func__);
		return -1;
	}

	return imsg_compose_event(iev_ldapd, IMSG_LDAPD_OPEN, 0, 0, -1, &req,
	    sizeof(req));
}

int
namespace_reopen_data(struct namespace *ns)
{
	if (ns->data_db != NULL) {
		btree_close(ns->data_db);
		ns->data_db = NULL;
		return namespace_reopen(ns->data_path);
	}
	return 1;
}

int
namespace_reopen_indx(struct namespace *ns)
{
	if (ns->indx_db != NULL) {
		btree_close(ns->indx_db);
		ns->indx_db = NULL;
		return namespace_reopen(ns->indx_path);
	}
	return 1;
}

static int
namespace_set_fd(struct namespace *ns, struct btree **bt, int fd,
    unsigned int flags)
{
	log_info("reopening namespace %s (entries)", ns->suffix);
	btree_close(*bt);
	if (ns->sync == 0)
		flags |= BT_NOSYNC;
	*bt = btree_open_fd(fd, flags);
	if (*bt == NULL)
		return -1;
	return 0;
}

int
namespace_set_data_fd(struct namespace *ns, int fd)
{
	return namespace_set_fd(ns, &ns->data_db, fd, BT_REVERSEKEY);
}

int
namespace_set_indx_fd(struct namespace *ns, int fd)
{
	return namespace_set_fd(ns, &ns->indx_db, fd, 0);
}

void
namespace_close(struct namespace *ns)
{
	struct conn		*conn;
	struct search		*search, *next;
	struct request		*req;

	/* Cancel any queued requests for this namespace.
	 */
	if (ns->queued_requests > 0) {
		log_warnx("cancelling %u queued requests on namespace %s",
		    ns->queued_requests, ns->suffix);
		while ((req = TAILQ_FIRST(&ns->request_queue)) != NULL) {
			TAILQ_REMOVE(&ns->request_queue, req, next);
			ldap_respond(req, LDAP_UNAVAILABLE);
		}
	}

	/* Cancel any searches on this namespace.
	 */
	TAILQ_FOREACH(conn, &conn_list, next) {
		for (search = TAILQ_FIRST(&conn->searches); search != NULL;
		    search = next) {
			next = TAILQ_NEXT(search, next);
			if (search->ns == ns)
				search_close(search);
		}
	}

	free(ns->suffix);
	btree_close(ns->data_db);
	btree_close(ns->indx_db);
	if (evtimer_pending(&ns->ev_queue, NULL))
		evtimer_del(&ns->ev_queue);
	free(ns->data_path);
	free(ns->indx_path);
	free(ns);
}

void
namespace_remove(struct namespace *ns)
{
	TAILQ_REMOVE(&conf->namespaces, ns, next);
	namespace_close(ns);
}

static struct btval *
namespace_find(struct namespace *ns, char *dn)
{
	struct btval		 key;
	static struct btval	 val;

	bzero(&key, sizeof(key));
	bzero(&val, sizeof(val));

	key.data = dn;
	key.size = strlen(dn);

	switch (btree_txn_get(ns->data_db, ns->data_txn, &key, &val)) {
	case BT_FAIL:
		log_warn("%s", dn);
		return NULL;
	case BT_DEAD:
		log_warn("%s", dn);
		namespace_reopen_data(ns);
		return NULL;
	case BT_NOTFOUND:
		log_debug("%s: dn not found", dn);
		return NULL;
	}

	return &val;
}

struct ber_element *
namespace_get(struct namespace *ns, char *dn)
{
	struct ber_element	*elm;
	struct btval		*val;

	if ((val = namespace_find(ns, dn)) == NULL)
		return NULL;

	elm = namespace_db2ber(ns, val);
	btval_reset(val);
	return elm;
}

int
namespace_exists(struct namespace *ns, char *dn)
{
	struct btval		*val;

	if ((val = namespace_find(ns, dn)) == NULL)
		return 0;
	btval_reset(val);
	return 1;
}

int
namespace_ber2db(struct namespace *ns, struct ber_element *root,
    struct btval *val)
{
	int			 rc;
	ssize_t			 len;
	uLongf			 destlen;
	Bytef			*dest;
	void			*buf;
	struct ber		 ber;

	bzero(val, sizeof(*val));

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_write_elements(&ber, root);

	if ((len = ber_get_writebuf(&ber, &buf)) == -1)
		return -1;

	if (ns->compression_level > 0) {
		val->size = compressBound(len);
		val->data = malloc(val->size + sizeof(uint32_t));
		if (val->data == NULL) {
			log_warn("malloc(%u)", val->size + sizeof(uint32_t));
			ber_free(&ber);
			return -1;
		}
		dest = (char *)val->data + sizeof(uint32_t);
		destlen = val->size - sizeof(uint32_t);
		if ((rc = compress2(dest, &destlen, buf, len,
		    ns->compression_level)) != Z_OK) {
			log_warn("compress returned %i", rc);
			free(val->data);
			ber_free(&ber);
			return -1;
		}
		log_debug("compressed entry from %u -> %u byte",
		    len, destlen + sizeof(uint32_t));

		*(uint32_t *)val->data = len;
		val->size = destlen + sizeof(uint32_t);
		val->free_data = 1;
	} else {
		val->data = buf;
		val->size = len;
		val->free_data = 1;	/* XXX: take over internal br_wbuf */
		ber.br_wbuf = NULL;
	}

	ber_free(&ber);

	return 0;
}

struct ber_element *
namespace_db2ber(struct namespace *ns, struct btval *val)
{
	int			 rc;
	uLongf			 len;
	void			*buf;
	Bytef			*src;
	uLong			 srclen;
	struct ber_element	*elm;
	struct ber		 ber;

	assert(ns != NULL);
	assert(val != NULL);

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	if (ns->compression_level > 0) {
		if (val->size < sizeof(uint32_t))
			return NULL;

		len = *(uint32_t *)val->data;
		if ((buf = malloc(len)) == NULL) {
			log_warn("malloc(%u)", len);
			return NULL;
		}

		src = (char *)val->data + sizeof(uint32_t);
		srclen = val->size - sizeof(uint32_t);
		rc = uncompress(buf, &len, src, srclen);
		if (rc != Z_OK) {
			log_warnx("dbt_to_ber: uncompress returned %i", rc);
			free(buf);
			return NULL;
		}

		log_debug("uncompressed entry from %u -> %u byte",
		    val->size, len);

		ber_set_readbuf(&ber, buf, len);
		elm = ber_read_elements(&ber, NULL);
		free(buf);
		return elm;
	} else {
		ber_set_readbuf(&ber, val->data, val->size);
		return ber_read_elements(&ber, NULL);
	}
}

static int
namespace_put(struct namespace *ns, char *dn, struct ber_element *root,
    int update)
{
	int			 rc;
	struct btval		 key, val;

	assert(ns != NULL);
	assert(ns->data_txn != NULL);
	assert(ns->indx_txn != NULL);

	bzero(&key, sizeof(key));
	key.data = dn;
	key.size = strlen(dn);

	if (namespace_ber2db(ns, root, &val) != 0)
		return BT_FAIL;

	rc = btree_txn_put(NULL, ns->data_txn, &key, &val,
	    update ? 0 : BT_NOOVERWRITE);
	if (rc != BT_SUCCESS) {
		if (rc == BT_EXISTS)
			log_debug("%s: already exists", dn);
		else
			log_warn("%s", dn);
		goto done;
	}

	/* FIXME: if updating, try harder to just update changed indices.
	 */
	if (update && unindex_entry(ns, &key, root) != BT_SUCCESS)
		goto done;

	rc = index_entry(ns, &key, root);

done:
	btval_reset(&val);
	return rc;
}

int
namespace_add(struct namespace *ns, char *dn, struct ber_element *root)
{
	return namespace_put(ns, dn, root, 0);
}

int
namespace_update(struct namespace *ns, char *dn, struct ber_element *root)
{
	return namespace_put(ns, dn, root, 1);
}

int
namespace_del(struct namespace *ns, char *dn)
{
	int			 rc;
	struct ber_element	*root;
	struct btval		 key, data;

	assert(ns != NULL);
	assert(ns->indx_txn != NULL);
	assert(ns->data_txn != NULL);

	bzero(&key, sizeof(key));
	bzero(&data, sizeof(data));

	key.data = dn;
	key.size = strlen(key.data);

	rc = btree_txn_del(NULL, ns->data_txn, &key, &data);
	if (rc == BT_SUCCESS && (root = namespace_db2ber(ns, &data)) != NULL)
		rc = unindex_entry(ns, &key, root);

	btval_reset(&data);
	return rc;
}

struct namespace *
namespace_for_base(const char *basedn)
{
	size_t			 blen, slen;
	struct namespace	*ns;

	assert(basedn);
	blen = strlen(basedn);

	TAILQ_FOREACH(ns, &conf->namespaces, next) {
		slen = strlen(ns->suffix);
		if (blen >= slen &&
		    bcmp(basedn + blen - slen, ns->suffix, slen) == 0)
			return ns;
	}

	return NULL;
}

int
namespace_has_index(struct namespace *ns, const char *attr,
    enum index_type type)
{
	struct attr_index	*ai;

	assert(ns);
	assert(attr);
	TAILQ_FOREACH(ai, &ns->indices, next) {
		if (strcasecmp(attr, ai->attr) == 0 && ai->type == type)
			return 1;
	}

	return 0;
}

/* Queues modification requests while the namespace is being reopened.
 */
int
namespace_queue_request(struct namespace *ns, struct request *req)
{
	if (ns->queued_requests > MAX_REQUEST_QUEUE) {
		log_warn("%u requests alreay queued, sorry");
		return -1;
	}

	TAILQ_INSERT_TAIL(&ns->request_queue, req, next);
	ns->queued_requests++;
	return 0;
}

static void
namespace_queue_replay(int fd, short event, void *data)
{
	struct namespace	*ns = data;
	struct request		*req;

	if (ns->data_db == NULL || ns->indx_db == NULL) {
		log_debug("%s: database is being reopened", ns->suffix);
		return;		/* Database is being reopened. */
	}

	if ((req = TAILQ_FIRST(&ns->request_queue)) == NULL)
		return;
	TAILQ_REMOVE(&ns->request_queue, req, next);

	log_debug("replaying queued request");
	req->replayed = 1;
	request_dispatch(req);
	ns->queued_requests--;
}

void
namespace_queue_schedule(struct namespace *ns)
{
	struct timeval	 tv;

	if (!evtimer_pending(&ns->ev_queue, NULL)) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		evtimer_add(&ns->ev_queue, &tv);
	}
}

/* Cancel all queued requests from the given connection. Drops matching
 * requests from all namespaces without sending a response.
 */
void
namespace_cancel_conn(struct conn *conn)
{
	struct namespace	*ns;
	struct request		*req, *next;

	TAILQ_FOREACH(ns, &conf->namespaces, next) {
		for (req = TAILQ_FIRST(&ns->request_queue);
		    req != TAILQ_END(&ns->request_queue); req = next) {
			next = TAILQ_NEXT(req, next);

			if (req->conn == conn) {
				TAILQ_REMOVE(&ns->request_queue, req, next);
				request_free(req);
			}
		}
	}
}

