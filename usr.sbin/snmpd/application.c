/*	$OpenBSD: application.c,v 1.16 2022/09/13 10:22:07 martijn Exp $	*/

/*
 * Copyright (c) 2021 Martijn van Duren <martijn@openbsd.org>
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
#include <sys/tree.h>

#include <assert.h>
#include <errno.h>
#include <event.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "application.h"
#include "log.h"
#include "smi.h"
#include "snmp.h"
#include "snmpe.h"
#include "mib.h"

TAILQ_HEAD(, appl_context) contexts = TAILQ_HEAD_INITIALIZER(contexts);

struct appl_context {
	char ac_name[APPL_CONTEXTNAME_MAX + 1];

	RB_HEAD(appl_regions, appl_region) ac_regions;

	TAILQ_ENTRY(appl_context) ac_entries;
};

struct appl_region {
	struct ber_oid ar_oid;
	uint8_t ar_priority;
	int32_t ar_timeout;
	int ar_instance;
	int ar_subtree; /* Claim entire subtree */
	struct appl_backend *ar_backend;
	struct appl_region *ar_next; /* Sorted by priority */

	RB_ENTRY(appl_region) ar_entry;
};

struct appl_request_upstream {
	struct appl_context *aru_ctx;
	struct snmp_message *aru_statereference;
	int32_t aru_requestid; /* upstream requestid */
	int32_t aru_transactionid; /* RFC 2741 section 6.1 */
	int16_t aru_nonrepeaters;
	int16_t aru_maxrepetitions;
	struct appl_varbind_internal *aru_vblist;
	size_t aru_varbindlen;
	enum appl_error aru_error;
	int16_t aru_index;
	int aru_locked; /* Prevent recursion through appl_request_send */

	enum snmp_version aru_pduversion;
	struct ber_element *aru_pdu; /* Original requested pdu */
};

struct appl_request_downstream {
	struct appl_request_upstream *ard_request;
	struct appl_backend *ard_backend;
	enum snmp_pdutype ard_requesttype;
	int16_t ard_nonrepeaters;
	int16_t ard_maxrepetitions;
	int32_t ard_requestid;
	uint8_t ard_retries;

	struct appl_varbind_internal *ard_vblist;
	struct event ard_timer;

	RB_ENTRY(appl_request_downstream) ard_entry;
};

enum appl_varbind_state {
	APPL_VBSTATE_MUSTFILL,
	APPL_VBSTATE_NEW,
	APPL_VBSTATE_PENDING,
	APPL_VBSTATE_DONE
};

struct appl_varbind_internal {
	enum appl_varbind_state avi_state;
	struct appl_varbind avi_varbind;
	struct appl_region *avi_region;
	int16_t avi_index;
	struct appl_request_upstream *avi_request_upstream;
	struct appl_request_downstream *avi_request_downstream;
	struct appl_varbind_internal *avi_next;
	struct appl_varbind_internal *avi_sub;
};

/* SNMP-TARGET-MIB (RFC 3413) */
struct snmp_target_mib {
	uint32_t		snmp_unavailablecontexts;
	uint32_t		snmp_unknowncontexts;
} snmp_target_mib;

enum appl_error appl_region(struct appl_context *, uint32_t, uint8_t,
    struct ber_oid *, int, int, struct appl_backend *);
void appl_region_free(struct appl_context *, struct appl_region *);
struct appl_region *appl_region_find(struct appl_context *,
    const struct ber_oid *);
struct appl_region *appl_region_next(struct appl_context *,
    struct ber_oid *, struct appl_region *);
void appl_request_upstream_free(struct appl_request_upstream *);
void appl_request_downstream_free(struct appl_request_downstream *);
void appl_request_upstream_resolve(struct appl_request_upstream *);
void appl_request_downstream_send(struct appl_request_downstream *);
void appl_request_downstream_timeout(int, short, void *);
void appl_request_upstream_reply(struct appl_request_upstream *);
int appl_varbind_valid(struct appl_varbind *, struct appl_varbind *, int, int,
    int, const char **);
int appl_varbind_backend(struct appl_varbind_internal *);
void appl_varbind_error(struct appl_varbind_internal *, enum appl_error);
void appl_report(struct snmp_message *, int32_t, struct ber_oid *,
    struct ber_element *);
void appl_pdu_log(struct appl_backend *, enum snmp_pdutype, int32_t, uint16_t,
    uint16_t, struct appl_varbind *);
void ober_oid_nextsibling(struct ber_oid *);

int appl_region_cmp(struct appl_region *, struct appl_region *);
int appl_request_cmp(struct appl_request_downstream *,
    struct appl_request_downstream *);

RB_PROTOTYPE_STATIC(appl_regions, appl_region, ar_entry, appl_region_cmp);
RB_PROTOTYPE_STATIC(appl_requests, appl_request_downstream, ard_entry,
    appl_request_cmp);

#define APPL_CONTEXT_NAME(ctx) (ctx->ac_name[0] == '\0' ? NULL : ctx->ac_name)

void
appl(void)
{
	appl_agentx();
}

void
appl_init(void)
{
	appl_blocklist_init();
	appl_legacy_init();
	appl_agentx_init();
}

void
appl_shutdown(void)
{
	struct appl_context *ctx, *tctx;

	appl_blocklist_shutdown();
	appl_legacy_shutdown();
	appl_agentx_shutdown();

	TAILQ_FOREACH_SAFE(ctx, &contexts, ac_entries, tctx) {
		assert(RB_EMPTY(&(ctx->ac_regions)));
		TAILQ_REMOVE(&contexts, ctx, ac_entries);
		free(ctx);
	}
}

static struct appl_context *
appl_context(const char *name, int create)
{
	struct appl_context *ctx;

	if (name == NULL)
		name = "";

	if (strlen(name) > APPL_CONTEXTNAME_MAX) {
		errno = EINVAL;
		return NULL;
	}

	TAILQ_FOREACH(ctx, &contexts, ac_entries) {
		if (strcmp(name, ctx->ac_name) == 0)
			return ctx;
	}

	/* Always allow the default namespace */
	if (!create && name[0] != '\0') {
		errno = ENOENT;
		return NULL;
	}

	if ((ctx = malloc(sizeof(*ctx))) == NULL)
		return NULL;

	strlcpy(ctx->ac_name, name, sizeof(ctx->ac_name));
	RB_INIT(&(ctx->ac_regions));

	TAILQ_INSERT_TAIL(&contexts, ctx, ac_entries);
	return ctx;
}

enum appl_error
appl_region(struct appl_context *ctx, uint32_t timeout, uint8_t priority,
    struct ber_oid *oid, int instance, int subtree,
    struct appl_backend *backend)
{
	struct appl_region *region = NULL, *nregion;
	char oidbuf[1024], regionbuf[1024], subidbuf[11];
	size_t i;

	/* Don't use smi_oid2string, because appl_register can't use it */
	oidbuf[0] = '\0';
	for (i = 0; i < oid->bo_n; i++) {
		if (i != 0)
			strlcat(oidbuf, ".", sizeof(oidbuf));
		snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32,
		    oid->bo_id[i]);
		strlcat(oidbuf, subidbuf, sizeof(oidbuf));
	}

	/*
	 * Don't allow overlap when subtree flag is set.
	 * This allows us to keep control of certain regions like system.
	 */
	region = appl_region_find(ctx, oid);
	if (region != NULL && region->ar_subtree &&
	    region->ar_backend != backend)
		goto overlap;

	if ((nregion = malloc(sizeof(*nregion))) == NULL) {
		log_warn("%s: Can't register %s: Processing error",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_PROCESSINGERROR;
	}
	nregion->ar_oid = *oid;
	nregion->ar_priority = priority;
	nregion->ar_timeout = timeout;
	nregion->ar_instance = instance;
	nregion->ar_subtree = subtree;
	nregion->ar_backend = backend;
	nregion->ar_next = NULL;

	region = RB_INSERT(appl_regions, &(ctx->ac_regions), nregion);
	if (region == NULL)
		return APPL_ERROR_NOERROR;

	if (region->ar_priority == priority)
		goto duplicate;
	if (region->ar_priority > priority) {
		RB_REMOVE(appl_regions, &(ctx->ac_regions), region);
		RB_INSERT(appl_regions, &(ctx->ac_regions), nregion);
		nregion->ar_next = region;
		return APPL_ERROR_NOERROR;
	}

	while (region->ar_next != NULL &&
	    region->ar_next->ar_priority < priority)
		region = region->ar_next;
	if (region->ar_next != NULL && region->ar_next->ar_priority == priority)
		goto duplicate;
	nregion->ar_next = region->ar_next;
	region->ar_next = nregion;

	return APPL_ERROR_NOERROR;
 duplicate:
	free(nregion);
	log_info("%s: %s priority %"PRId8": Duplicate registration",
	    backend->ab_name, oidbuf, priority);
	return APPL_ERROR_DUPLICATEREGISTRATION;
 overlap:
	regionbuf[0] = '\0';
	for (i = 0; i < region->ar_oid.bo_n; i++) {
		if (i != 0)
			strlcat(regionbuf, ".", sizeof(regionbuf));
		snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32,
		    region->ar_oid.bo_id[i]);
		strlcat(regionbuf, subidbuf, sizeof(regionbuf));
	}
	log_info("%s: %s overlaps with %s: Request denied",
	    backend->ab_name, oidbuf, regionbuf);
	return APPL_ERROR_REQUESTDENIED;
}

/* Name from RFC 2741 section 6.2.3 */
enum appl_error
appl_register(const char *ctxname, uint32_t timeout, uint8_t priority,
    struct ber_oid *oid, int instance, int subtree, uint8_t range_subid,
    uint32_t upper_bound, struct appl_backend *backend)
{
	struct appl_context *ctx;
	struct appl_region *region, search;
	char oidbuf[1024], subidbuf[11];
	enum appl_error error;
	size_t i;
	uint32_t lower_bound;

	oidbuf[0] = '\0';
	/* smi_oid2string can't do ranges */
	for (i = 0; i < oid->bo_n; i++) {
		snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32, oid->bo_id[i]);
		if (i != 0)
			strlcat(oidbuf, ".", sizeof(oidbuf));
		if (range_subid == i + 1) {
			strlcat(oidbuf, "[", sizeof(oidbuf));
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
			strlcat(oidbuf, "-", sizeof(oidbuf));
			snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32,
			    upper_bound);
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
			strlcat(oidbuf, "]", sizeof(oidbuf));
		} else
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
	}

	if (ctxname == NULL)
		ctxname = "";
	log_info("%s: Registering %s%s context(%s) priority(%"PRIu8") "
	    "timeout(%"PRIu32".%02us)", backend->ab_name, oidbuf,
	     instance ? "(instance)" : "", ctxname, priority,
	     timeout/100, timeout % 100);

	if ((ctx = appl_context(ctxname, 0)) == NULL) {
		if (errno == ENOMEM) {
			log_warn("%s: Can't register %s: Processing error",
			    backend->ab_name, oidbuf);
			return APPL_ERROR_PROCESSINGERROR;
		}
		log_info("%s: Can't register %s: Unsupported context \"%s\"",
		    backend->ab_name, oidbuf, ctxname);
		return APPL_ERROR_UNSUPPORTEDCONTEXT;
	}
	/* Default timeouts should be handled by backend */
	if (timeout == 0)
		fatalx("%s: Timeout can't be 0", __func__);
	if (priority == 0) {
		log_warnx("%s: Can't register %s: priority can't be 0",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}
	if (range_subid > oid->bo_n) {
		log_warnx("%s: Can't register %s: range_subid too large",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}
	if (range_subid != 0 && oid->bo_id[range_subid] >= upper_bound) {
		log_warnx("%s: Can't register %s: upper bound smaller or equal "
		    "to range_subid", backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}
	if (range_subid != 0)
		lower_bound = oid->bo_id[range_subid];

	if (range_subid == 0)
		return appl_region(ctx, timeout, priority, oid, instance,
		    subtree, backend);

	do {
		if ((error = appl_region(ctx, timeout, priority, oid, instance,
		    subtree, backend)) != APPL_ERROR_NOERROR)
			goto fail;
	} while (oid->bo_id[range_subid] != upper_bound);
	if ((error = appl_region(ctx, timeout, priority, oid, instance, subtree,
	    backend)) != APPL_ERROR_NOERROR)
		goto fail;

	return APPL_ERROR_NOERROR;
 fail:
	search.ar_oid = *oid;
	if (search.ar_oid.bo_id[range_subid] == lower_bound)
		return error;
	
	for (search.ar_oid.bo_id[range_subid]--;
	    search.ar_oid.bo_id[range_subid] != lower_bound;
	    search.ar_oid.bo_id[range_subid]--) {
		region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
		while (region->ar_priority != priority)
			region = region->ar_next;
		appl_region_free(ctx, region);
	}
	region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
	while (region->ar_priority != priority)
		region = region->ar_next;
	appl_region_free(ctx, region);
	return error;
}

/* Name from RFC 2741 section 6.2.4 */
enum appl_error
appl_unregister(const char *ctxname, uint8_t priority, struct ber_oid *oid,
    uint8_t range_subid, uint32_t upper_bound, struct appl_backend *backend)
{
	struct appl_region *region, search;
	struct appl_context *ctx;
	char oidbuf[1024], subidbuf[11];
	size_t i;

	oidbuf[0] = '\0';
	for (i = 0; i < oid->bo_n; i++) {
		snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32, oid->bo_id[i]);
		if (i != 0)
			strlcat(oidbuf, ".", sizeof(oidbuf));
		if (range_subid == i + 1) {
			strlcat(oidbuf, "[", sizeof(oidbuf));
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
			strlcat(oidbuf, "-", sizeof(oidbuf));
			snprintf(subidbuf, sizeof(subidbuf), "%"PRIu32,
			    upper_bound);
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
			strlcat(oidbuf, "]", sizeof(oidbuf));
		} else
			strlcat(oidbuf, subidbuf, sizeof(oidbuf));
	}

	if (ctxname == NULL)
		ctxname = "";
	log_info("%s: Unregistering %s context(%s) priority(%"PRIu8")",
	     backend->ab_name, oidbuf,ctxname, priority);

	if ((ctx = appl_context(ctxname, 0)) == NULL) {
		if (errno == ENOMEM) {
			log_warn("%s: Can't unregister %s: Processing error",
			    backend->ab_name, oidbuf);
			return APPL_ERROR_PROCESSINGERROR;
		}
		log_info("%s: Can't unregister %s: Unsupported context \"%s\"",
		    backend->ab_name, oidbuf, ctxname);
		return APPL_ERROR_UNSUPPORTEDCONTEXT;
	}

	if (priority == 0) {
		log_warnx("%s: Can't unregister %s: priority can't be 0",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}

	if (range_subid > oid->bo_n) {
		log_warnx("%s: Can't unregiser %s: range_subid too large",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}
	if (range_subid != 0 && oid->bo_id[range_subid] >= upper_bound) {
		log_warnx("%s: Can't unregister %s: upper bound smaller or "
		    "equal to range_subid", backend->ab_name, oidbuf);
		return APPL_ERROR_PARSEERROR;
	}

	search.ar_oid = *oid;
	while (range_subid != 0 &&
	    search.ar_oid.bo_id[range_subid] != upper_bound) {
		region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
		while (region != NULL && region->ar_priority < priority)
			region = region->ar_next;
		if (region == NULL || region->ar_priority != priority) {
			log_warnx("%s: Can't unregister %s: region not found",
			    backend->ab_name, oidbuf);
			return APPL_ERROR_UNKNOWNREGISTRATION;
		}
		if (region->ar_backend != backend) {
			log_warnx("%s: Can't unregister %s: region not owned "
			    "by backend", backend->ab_name, oidbuf);
			return APPL_ERROR_UNKNOWNREGISTRATION;
		}
	}
	region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
	while (region != NULL && region->ar_priority < priority)
		region = region->ar_next;
	if (region == NULL || region->ar_priority != priority) {
		log_warnx("%s: Can't unregister %s: region not found",
		    backend->ab_name, oidbuf);
		return APPL_ERROR_UNKNOWNREGISTRATION;
	}
	if (region->ar_backend != backend) {
		log_warnx("%s: Can't unregister %s: region not owned "
		    "by backend", backend->ab_name, oidbuf);
		return APPL_ERROR_UNKNOWNREGISTRATION;
	}

	search.ar_oid = *oid;
	while (range_subid != 0 &&
	    search.ar_oid.bo_id[range_subid] != upper_bound) {
		region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
		while (region != NULL && region->ar_priority != priority)
			region = region->ar_next;
		appl_region_free(ctx, region);
	}
	region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
	while (region != NULL && region->ar_priority != priority)
		region = region->ar_next;
	appl_region_free(ctx, region);

	return APPL_ERROR_NOERROR;
}

void
appl_region_free(struct appl_context *ctx, struct appl_region *region)
{
	struct appl_region *pregion;

	pregion = RB_FIND(appl_regions, &(ctx->ac_regions), region);

	if (pregion == region) {
		RB_REMOVE(appl_regions, &(ctx->ac_regions), region);
		if (region->ar_next != NULL)
			RB_INSERT(appl_regions, &(ctx->ac_regions),
			    region->ar_next);
	} else {
		while (pregion->ar_next != region)
			pregion = pregion->ar_next;
		pregion->ar_next = region->ar_next;
	}

	free(region);
}

/* backend is owned by the sub-application, just release application.c stuff */
void
appl_close(struct appl_backend *backend)
{
	struct appl_context *ctx;
	struct appl_region *region, *tregion, *nregion;
	struct appl_request_downstream *request, *trequest;

	TAILQ_FOREACH(ctx, &contexts, ac_entries) {
		RB_FOREACH_SAFE(region, appl_regions,
		    &(ctx->ac_regions), tregion) {
			while (region != NULL) {
				nregion = region->ar_next;
				if (region->ar_backend == backend)
					appl_region_free(ctx, region);
				region = nregion;
			}
		}
	}

	RB_FOREACH_SAFE(request, appl_requests,
	    &(backend->ab_requests), trequest)
		appl_request_downstream_free(request);
}

struct appl_region *
appl_region_find(struct appl_context *ctx,
    const struct ber_oid *oid)
{
	struct appl_region *region, search;

	search.ar_oid = *oid;
	while (search.ar_oid.bo_n > 0) {
		region = RB_FIND(appl_regions, &(ctx->ac_regions), &search);
		if (region != NULL)
			return region;
		search.ar_oid.bo_n--;
	}
	return NULL;
}

struct appl_region *
appl_region_next(struct appl_context *ctx, struct ber_oid *oid,
    struct appl_region *cregion)
{
	struct appl_region search, *nregion, *pregion;
	int cmp;

	search.ar_oid = *oid;
	nregion = RB_NFIND(appl_regions, &(ctx->ac_regions), &search);

	if (cregion == nregion)
		nregion = RB_NEXT(appl_regions, &(ctx->ac_regions), nregion);
	/* Past last element in tree, we might still have a parent */
	if (nregion == NULL) {
		search.ar_oid = cregion->ar_oid;
		search.ar_oid.bo_n--;
		return appl_region_find(ctx, &(search.ar_oid));
	}
	cmp = appl_region_cmp(cregion, nregion);
	if (cmp >= 0)
		fatalx("%s: wrong OID order", __func__);
	/* Direct descendant */
	if (cmp == -2)
		return nregion;

	/* cmp == -1 */
	search.ar_oid = cregion->ar_oid;
	/* Find direct next sibling */
	ober_oid_nextsibling(&(search.ar_oid));
	if (ober_oid_cmp(&(nregion->ar_oid), &(search.ar_oid)) == 0)
		return nregion;
	/* Sibling gaps go to parent, or end end at border */
	search.ar_oid = cregion->ar_oid;
	search.ar_oid.bo_n--;
	pregion = appl_region_find(ctx, &(search.ar_oid));

	return pregion != NULL ? pregion : nregion;
}

/* Name from RFC 3413 section 3.2 */
void
appl_processpdu(struct snmp_message *statereference, const char *ctxname,
    enum snmp_version pduversion, struct ber_element *pdu)
{
	struct appl_context *ctx;
	struct appl_request_upstream *ureq;
	struct ber_oid oid;
	struct ber_element *value, *varbind, *varbindlist;
	long long nonrepeaters, maxrepetitions;
	static uint32_t transactionid;
	int32_t requestid;
	size_t i, varbindlen = 0, repeaterlen;

	/* pdu must be ASN.1 validated in snmpe.c */
	(void) ober_scanf_elements(pdu, "{diie", &requestid, &nonrepeaters,
	    &maxrepetitions, &varbindlist);

	/* RFC 3413, section 3.2, processPDU, item 5, final bullet */
	if ((ctx = appl_context(ctxname, 0)) == NULL) {
		oid = BER_OID(MIB_snmpUnknownContexts, 0);
		snmp_target_mib.snmp_unknowncontexts++;
		if ((value = ober_add_integer(NULL,
		    snmp_target_mib.snmp_unknowncontexts)) == NULL)
			fatal("ober_add_integer");
		appl_report(statereference, requestid, &oid, value);
		return;
	}

	if ((ureq = malloc(sizeof(*ureq))) == NULL)
		fatal("malloc");

	ureq->aru_ctx = ctx;
	ureq->aru_statereference = statereference;
	ureq->aru_transactionid = transactionid++;
	ureq->aru_requestid = requestid;
	ureq->aru_error = APPL_ERROR_NOERROR;
	ureq->aru_index = 0;
	ureq->aru_nonrepeaters = nonrepeaters;
	ureq->aru_maxrepetitions = maxrepetitions;
	ureq->aru_varbindlen = 0;
	ureq->aru_locked = 0;
	ureq->aru_pduversion = pduversion;
	ureq->aru_pdu = pdu;

	varbind = varbindlist->be_sub;
	for (; varbind != NULL; varbind = varbind->be_next)
		varbindlen++;

	repeaterlen = varbindlen - nonrepeaters;
	if (pdu->be_type == SNMP_C_GETBULKREQ)
		ureq->aru_varbindlen = nonrepeaters +
		    (repeaterlen * maxrepetitions);
	else
		ureq->aru_varbindlen = varbindlen;
	if ((ureq->aru_vblist = calloc(ureq->aru_varbindlen,
	    sizeof(*ureq->aru_vblist))) == NULL)
		fatal("malloc");

	varbind = varbindlist->be_sub;
	/* Use aru_varbindlen in case maxrepetitions == 0 */
	for (i = 0; i < ureq->aru_varbindlen; i++) {
		ureq->aru_vblist[i].avi_request_upstream = ureq;
		ureq->aru_vblist[i].avi_index = i + 1;
		ureq->aru_vblist[i].avi_state = APPL_VBSTATE_NEW;
		/* This can only happen with bulkreq */
		if (varbind == NULL) {
			ureq->aru_vblist[i - repeaterlen].avi_sub =
			    &(ureq->aru_vblist[i]);
			ureq->aru_vblist[i].avi_state = APPL_VBSTATE_MUSTFILL;
			continue;
		}
		ober_get_oid(varbind->be_sub,
		    &(ureq->aru_vblist[i].avi_varbind.av_oid));
		if (i + 1 < ureq->aru_varbindlen) {
			ureq->aru_vblist[i].avi_next =
			    &(ureq->aru_vblist[i + 1]);
			ureq->aru_vblist[i].avi_varbind.av_next =
			    &(ureq->aru_vblist[i + 1].avi_varbind);
		} else {
			ureq->aru_vblist[i].avi_next = NULL;
			ureq->aru_vblist[i].avi_varbind.av_next = NULL;
		}
		varbind = varbind->be_next;
	}

	appl_pdu_log(NULL, pdu->be_type, requestid, nonrepeaters,
	    maxrepetitions, &(ureq->aru_vblist[0].avi_varbind));

	appl_request_upstream_resolve(ureq);
}

void
appl_request_upstream_free(struct appl_request_upstream *ureq)
{
	size_t i;
	struct appl_varbind_internal *vb;

	if (ureq == NULL)
		return;

	for (i = 0; i < ureq->aru_varbindlen && ureq->aru_vblist != NULL; i++) {
		vb = &(ureq->aru_vblist[i]);
		ober_free_elements(vb->avi_varbind.av_value);
		appl_request_downstream_free(vb->avi_request_downstream);
	}
	free(ureq->aru_vblist);

	assert(ureq->aru_statereference == NULL);

	free(ureq);
}

void
appl_request_downstream_free(struct appl_request_downstream *dreq)
{
	struct appl_varbind_internal *vb;
	int retry = 0;

	if (dreq == NULL)
		return;

	RB_REMOVE(appl_requests, &(dreq->ard_backend->ab_requests), dreq);
	evtimer_del(&(dreq->ard_timer));

	for (vb = dreq->ard_vblist; vb != NULL; vb = vb->avi_next) {
		vb->avi_request_downstream = NULL;
		if (vb->avi_state == APPL_VBSTATE_PENDING) {
			vb->avi_state = APPL_VBSTATE_NEW;
			retry = 1;
		}
	}

	if (retry)
		appl_request_upstream_resolve(dreq->ard_request);
	free(dreq);
}

void
appl_request_upstream_resolve(struct appl_request_upstream *ureq)
{
	struct appl_varbind_internal *vb, *lvb, *tvb;
	struct appl_request_downstream *dreq;
	struct appl_region *region, *lregion;
	struct timeval tv;
	int done;
	size_t i;
	int32_t maxrepetitions;
	int32_t timeout;

	if (ureq->aru_locked)
		return;
	ureq->aru_locked = 1;

	if (ureq->aru_pdu->be_type == SNMP_C_SETREQ) {
		ureq->aru_error = APPL_ERROR_NOTWRITABLE;
		ureq->aru_index = 1;
		appl_request_upstream_reply(ureq);
		return;
	}

 next:
	dreq = NULL;
	lvb = NULL;
	done = 1;
	timeout = 0;

	if (ureq->aru_error != APPL_ERROR_NOERROR) {
		appl_request_upstream_reply(ureq);
		return;
	}
	for (i = 0; i < ureq->aru_varbindlen; i++) {
		vb = &(ureq->aru_vblist[i]);

		switch (vb->avi_state) {
		case APPL_VBSTATE_MUSTFILL:
		case APPL_VBSTATE_PENDING:
			done = 0;
			continue;
		case APPL_VBSTATE_DONE:
			continue;
		case APPL_VBSTATE_NEW:
			break;
		}
		if (appl_varbind_backend(vb) == -1)
			fatal("appl_varbind_backend");
		if (vb->avi_state != APPL_VBSTATE_DONE)
			done = 0;
	}

	for (i = 0; i < ureq->aru_varbindlen; i++) {
		vb = &(ureq->aru_vblist[i]);

		if (vb->avi_state != APPL_VBSTATE_NEW)
			continue;

		vb = &(ureq->aru_vblist[i]);
		region = vb->avi_region;
		lregion = lvb != NULL ? lvb->avi_region : NULL;
		if (lvb != NULL && region->ar_backend != lregion->ar_backend)
			continue;

		vb->avi_varbind.av_next = NULL;
		vb->avi_next = NULL;
		tvb = vb;
		for (maxrepetitions = 0; tvb != NULL; tvb = tvb->avi_sub)
			maxrepetitions++;
		if (dreq == NULL) {
			if ((dreq = malloc(sizeof(*dreq))) == NULL)
				fatal("malloc");

			dreq->ard_request = ureq;
			dreq->ard_vblist = vb;
			dreq->ard_backend = vb->avi_region->ar_backend;
			dreq->ard_retries = dreq->ard_backend->ab_retries;
			dreq->ard_requesttype = ureq->aru_pdu->be_type;
			/*
			 * We don't yet fully handle bulkrequest responses.
			 * It's completely valid to map onto getrequest.
			 * maxrepetitions calculated in preparation of support.
			 */
			if (dreq->ard_requesttype == SNMP_C_GETBULKREQ &&
			    dreq->ard_backend->ab_fn->ab_getbulk == NULL)
				dreq->ard_requesttype = SNMP_C_GETNEXTREQ;
			/*
			 * If first varbind is nonrepeater, set maxrepetitions
			 * to 0, so that the next varbind with
			 * maxrepetitions > 1 determines length.
			 */
			if (maxrepetitions == 1) {
				dreq->ard_maxrepetitions = 0;
				dreq->ard_nonrepeaters = 1;
			} else {
				dreq->ard_maxrepetitions = maxrepetitions;
				dreq->ard_nonrepeaters = 0;
			}
			do {
				dreq->ard_requestid = arc4random();
			} while (RB_INSERT(appl_requests,
			    &(dreq->ard_backend->ab_requests), dreq) != NULL);
			lvb = vb;
		/* avi_sub isn't set on !bulkrequest, so we always enter here */
		} else if (maxrepetitions == 1) {
			dreq->ard_nonrepeaters++;
			vb->avi_varbind.av_next =
			    &(dreq->ard_vblist->avi_varbind);
			vb->avi_next = dreq->ard_vblist;
			dreq->ard_vblist = vb;
		} else {
			lvb->avi_varbind.av_next = &(vb->avi_varbind);
			lvb->avi_next = vb;
			/* RFC 2741 section 7.2.1.3:
			 * The value of g.max_repetitions in the GetBulk-PDU may
			 * be less than (but not greater than) the value in the
			 * original request PDU.
			 */
			if (dreq->ard_maxrepetitions > maxrepetitions ||
			    dreq->ard_maxrepetitions == 0)
				dreq->ard_maxrepetitions = maxrepetitions;
			lvb = vb;
		}
		vb->avi_request_downstream = dreq;
		vb->avi_state = APPL_VBSTATE_PENDING;
		if (region->ar_timeout > timeout)
			timeout = region->ar_timeout;
	}

	if (dreq == NULL) {
		ureq->aru_locked = 0;
		if (done)
			appl_request_upstream_reply(ureq);
		return;
	}

	tv.tv_sec = timeout / 100;
	tv.tv_usec = (timeout % 100) * 10000;
	evtimer_set(&(dreq->ard_timer), appl_request_downstream_timeout, dreq);
	evtimer_add(&(dreq->ard_timer), &tv);

	appl_request_downstream_send(dreq);
	goto next;
}

void
appl_request_downstream_send(struct appl_request_downstream *dreq)
{

	appl_pdu_log(dreq->ard_backend, dreq->ard_requesttype,
	    dreq->ard_requestid, 0, 0, &(dreq->ard_vblist->avi_varbind));

	if (dreq->ard_requesttype == SNMP_C_GETREQ) {
		dreq->ard_backend->ab_fn->ab_get(dreq->ard_backend,
		    dreq->ard_request->aru_transactionid,
		    dreq->ard_requestid,
		    APPL_CONTEXT_NAME(dreq->ard_request->aru_ctx),
		    &(dreq->ard_vblist->avi_varbind));
	} else if (dreq->ard_requesttype == SNMP_C_GETNEXTREQ) {
		dreq->ard_backend->ab_fn->ab_getnext(dreq->ard_backend,
		    dreq->ard_request->aru_transactionid,
		    dreq->ard_requestid,
		    APPL_CONTEXT_NAME(dreq->ard_request->aru_ctx),
		    &(dreq->ard_vblist->avi_varbind));
	}
}

void
appl_request_downstream_timeout(__unused int fd, __unused short event,
    void *cookie)
{
	struct appl_request_downstream *dreq = cookie;

	log_info("%s: %"PRIu32" timed out%s",
	    dreq->ard_backend->ab_name, dreq->ard_requestid,
	    dreq->ard_retries > 0 ? ": retrying" : "");
	if (dreq->ard_retries > 0) {
		dreq->ard_retries--;
		appl_request_downstream_send(dreq);
	} else
		appl_response(dreq->ard_backend, dreq->ard_requestid,
		    APPL_ERROR_GENERR, 1, &(dreq->ard_vblist->avi_varbind));
}

void
appl_request_upstream_reply(struct appl_request_upstream *ureq)
{
	struct ber_element *varbindlist = NULL, *varbind = NULL, *value;
	struct appl_varbind_internal *vb;
	size_t i, repvarbinds, varbindlen;
	ssize_t match = -1;

	varbindlen = ureq->aru_varbindlen;

	if (ureq->aru_pduversion == SNMP_V1) {
		/* RFC 3584 section 4.2.2.2 Map exceptions */
		for (i = 0; i < varbindlen; i++) {
			vb = &(ureq->aru_vblist[i]);
			value = vb->avi_varbind.av_value;
			if (value != NULL &&
			    value->be_class == BER_CLASS_CONTEXT)
				appl_varbind_error(vb, APPL_ERROR_NOSUCHNAME);
		}
		/* RFC 3584 section 4.4 Map errors */
		switch (ureq->aru_error) {
		case APPL_ERROR_WRONGVALUE:
		case APPL_ERROR_WRONGENCODING:
		case APPL_ERROR_WRONGTYPE:
		case APPL_ERROR_WRONGLENGTH:
		case APPL_ERROR_INCONSISTENTVALUE:
			ureq->aru_error = APPL_ERROR_BADVALUE;
			break;
		case APPL_ERROR_NOACCESS:
		case APPL_ERROR_NOTWRITABLE:
		case APPL_ERROR_NOCREATION:
		case APPL_ERROR_INCONSISTENTNAME:
		case APPL_ERROR_AUTHORIZATIONERROR:
			ureq->aru_error = APPL_ERROR_NOSUCHNAME;
			break;
		case APPL_ERROR_RESOURCEUNAVAILABLE:
		case APPL_ERROR_COMMITFAILED:
		case APPL_ERROR_UNDOFAILED:
			ureq->aru_error = APPL_ERROR_GENERR;
			break;
		default:
			break;
		}
	}
	/* RFC 3416 section 4.2.{1,2,3} reset original varbinds */
	if (ureq->aru_error != APPL_ERROR_NOERROR) {
		ober_scanf_elements(ureq->aru_pdu, "{SSS{e", &varbind);
		for (varbindlen = 0; varbind != NULL;
		    varbindlen++, varbind = varbind->be_next) {
			vb = &(ureq->aru_vblist[varbindlen]);
			ober_get_oid(varbind->be_sub,
			    &(vb->avi_varbind.av_oid));
			ober_free_elements(vb->avi_varbind.av_value);
			vb->avi_varbind.av_value = ober_add_null(NULL);;
		}
	/* RFC 3416 section 4.2.3: Strip excessive EOMV */
	} else if (ureq->aru_pdu->be_type == SNMP_C_GETBULKREQ) {
		repvarbinds = (ureq->aru_varbindlen - ureq->aru_nonrepeaters) /
		    ureq->aru_maxrepetitions;
		for (i = ureq->aru_nonrepeaters;
		    i < ureq->aru_varbindlen - repvarbinds; i++) {
			value = ureq->aru_vblist[i].avi_varbind.av_value;
			if ((i - ureq->aru_nonrepeaters) % repvarbinds == 0 &&
			    value->be_class == BER_CLASS_CONTEXT &&
			    value->be_type == APPL_EXC_ENDOFMIBVIEW) {
				if (match != -1)
					break;
				match = i;
			}
			if (value->be_class != BER_CLASS_CONTEXT ||
			    value->be_type != APPL_EXC_ENDOFMIBVIEW)
				match = -1;
		}
		if (match != -1)
			varbindlen = match + repvarbinds;
	}

	for (i = 0; i < varbindlen; i++) {
		vb = &(ureq->aru_vblist[i]);
		vb->avi_varbind.av_next =
		    &(ureq->aru_vblist[i + 1].avi_varbind);
	}

	ureq->aru_vblist[i - 1].avi_varbind.av_next = NULL;
	appl_pdu_log(NULL, SNMP_C_RESPONSE, ureq->aru_requestid,
	    ureq->aru_error, ureq->aru_index,
	    &(ureq->aru_vblist[0].avi_varbind));

	for (i = 0; i < varbindlen; i++) {
		varbind = ober_printf_elements(varbind, "{Oe}",
		    &(ureq->aru_vblist[i].avi_varbind.av_oid),
		    ureq->aru_vblist[i].avi_varbind.av_value);
		ureq->aru_vblist[i].avi_varbind.av_value = NULL;
		if (varbind == NULL)
			fatal("ober_printf_elements");
		if (varbindlist == NULL)
			varbindlist = varbind;
	}

	snmpe_send(ureq->aru_statereference, SNMP_C_RESPONSE,
	    ureq->aru_requestid, ureq->aru_error, ureq->aru_index, varbindlist);
	ureq->aru_statereference = NULL;
	appl_request_upstream_free(ureq);
}

/* Name from RFC 2741 section 6.2.16 */
void
appl_response(struct appl_backend *backend, int32_t requestid,
    enum appl_error error, int16_t index, struct appl_varbind *vblist)
{
	struct appl_request_downstream *dreq, search;
	struct appl_request_upstream *ureq = NULL;
	const char *errstr;
	char oidbuf[1024];
	enum snmp_pdutype pdutype;
	struct appl_varbind *vb;
	struct appl_varbind_internal *origvb = NULL;
	int invalid = 0;
	int next = 0, eomv;
	int32_t i;

	appl_pdu_log(backend, SNMP_C_RESPONSE, requestid, error, index, vblist);

	search.ard_requestid = requestid;
	dreq = RB_FIND(appl_requests, &(backend->ab_requests), &search);
	if (dreq == NULL) {
		log_debug("%s: %"PRIu32" not outstanding",
		    backend->ab_name, requestid);
		/* Continue to verify validity */
	} else {
		ureq = dreq->ard_request;
		pdutype = ureq->aru_pdu->be_type;
		next = pdutype == SNMP_C_GETNEXTREQ ||
		    pdutype == SNMP_C_GETBULKREQ;
		origvb = dreq->ard_vblist;
	}

	vb = vblist;
	for (i = 1; vb != NULL; vb = vb->av_next, i++) {
                if (!appl_varbind_valid(vb, origvb == NULL ?
		    NULL : &(origvb->avi_varbind), next,
                    error != APPL_ERROR_NOERROR, backend->ab_range, &errstr)) {
			smi_oid2string(&(vb->av_oid), oidbuf,
			    sizeof(oidbuf), 0);
			log_warnx("%s: %"PRIu32" %s: %s",
			    backend->ab_name, requestid, oidbuf, errstr);
			invalid = 1;
		}
		/* Transfer av_value */
		if (origvb != NULL) {
			if (error != APPL_ERROR_NOERROR && i == index)
				appl_varbind_error(origvb, error);
			origvb->avi_state = APPL_VBSTATE_DONE;
			origvb->avi_varbind.av_oid = vb->av_oid;

			eomv = vb->av_value != NULL &&
			    vb->av_value->be_class == BER_CLASS_CONTEXT &&
			    vb->av_value->be_type == APPL_EXC_ENDOFMIBVIEW;
			/*
			 * Treat results past av_oid_end for backends that
			 * don't support searchranges as EOMV
			 */
			eomv |= !backend->ab_range && next &&
			    ober_oid_cmp(&(vb->av_oid),
			    &(origvb->avi_varbind.av_oid_end)) > 0;
			/* RFC 3584 section 4.2.2.1 */
			if (ureq->aru_pduversion == SNMP_V1 &&
			    vb->av_value != NULL &&
			    vb->av_value->be_class == BER_CLASS_APPLICATION &&
			    vb->av_value->be_type == SNMP_COUNTER64) {
				if (next)
					eomv = 1;
				else
					appl_varbind_error(origvb,
					    APPL_ERROR_NOSUCHNAME);
			}

			if (eomv) {
				ober_free_elements(vb->av_value);
				origvb->avi_varbind.av_oid =
				    origvb->avi_varbind.av_oid_end;
				origvb->avi_varbind.av_include = 1;
				vb->av_value = NULL;
				origvb->avi_state = APPL_VBSTATE_NEW;
			}
			origvb->avi_varbind.av_value = vb->av_value;
			if (origvb->avi_varbind.av_next == NULL &&
			    vb->av_next != NULL) {
				log_warnx("%s: Request %"PRIu32" returned more "
				    "varbinds then requested",
				    backend->ab_name, requestid);
				invalid = 1;
			}
			if (origvb->avi_sub != NULL &&
			    origvb->avi_state == APPL_VBSTATE_DONE) {
				origvb->avi_sub->avi_varbind.av_oid =
				    origvb->avi_varbind.av_oid;
				origvb->avi_sub->avi_state = APPL_VBSTATE_NEW;
			}
			origvb = origvb->avi_next;
		} else {
			ober_free_elements(vb->av_value);
			vb->av_value = NULL;
		}
	}
	if (error != APPL_ERROR_NOERROR && (index <= 0 || index >= i)) {
		log_warnx("Invalid error index");
		invalid = 1;
	}
/* amavisd-snmp-subagent sets index to 1, no reason to crash over it. */
#if PEDANTIC
	if (error == APPL_ERROR_NOERROR && index != 0) {
		log_warnx("error index with no error");
		invalid = 1;
	}
#endif
	if (vb == NULL && origvb != NULL) {
		log_warnx("%s: Request %"PRIu32" returned less varbinds then "
		    "requested", backend->ab_name, requestid);
		invalid = 1;
	}

	if (dreq != NULL) {
		if (invalid)
			appl_varbind_error(dreq->ard_vblist, APPL_ERROR_GENERR);
		appl_request_downstream_free(dreq);
	}

	if (invalid && backend->ab_fn->ab_close != NULL) {
		log_warnx("%s: Closing: Too many parse errors",
		    backend->ab_name);
		backend->ab_fn->ab_close(backend, APPL_CLOSE_REASONPARSEERROR);
	}

	if (ureq != NULL)
		appl_request_upstream_resolve(ureq);
}

int
appl_varbind_valid(struct appl_varbind *varbind, struct appl_varbind *request,
    int next, int null, int range, const char **errstr)
{
	int cmp;
	int eomv = 0;

	if (varbind->av_value == NULL) {
		if (!null) {
			*errstr = "missing value";
			return 0;
		}
		return 1;
	}
	if (varbind->av_value->be_class == BER_CLASS_UNIVERSAL) {
		switch (varbind->av_value->be_type) {
		case BER_TYPE_NULL:
			if (null)
				break;
			*errstr = "not expecting null value";
			return 0;
		case BER_TYPE_INTEGER:
		case BER_TYPE_OCTETSTRING:
		case BER_TYPE_OBJECT:
			if (!null)
				break;
			/* FALLTHROUGH */
		default:
			*errstr = "invalid value";
			return 0;
		}
	} else if (varbind->av_value->be_class == BER_CLASS_APPLICATION) {
		switch (varbind->av_value->be_type) {
		case SNMP_T_IPADDR:
		case SNMP_T_COUNTER32:
		case SNMP_T_GAUGE32:
		case SNMP_T_TIMETICKS:
		case SNMP_T_OPAQUE:
		case SNMP_T_COUNTER64:
			if (!null)
				break;
			/* FALLTHROUGH */
		default:
			*errstr = "expecting null value";
			return 0;
		}
	} else if (varbind->av_value->be_class == BER_CLASS_CONTEXT) {
		switch (varbind->av_value->be_type) {
		case APPL_EXC_NOSUCHOBJECT:
			if (next && request != NULL) {
				*errstr = "Unexpected noSuchObject";
				return 0;
			}
			/* FALLTHROUGH */
		case APPL_EXC_NOSUCHINSTANCE:
			if (null) {
				*errstr = "expecting null value";
				return 0;
			}
			if (next && request != NULL) {
				*errstr = "Unexpected noSuchInstance";
				return 0;
			}
			break;
		case APPL_EXC_ENDOFMIBVIEW:
			if (null) {
				*errstr = "expecting null value";
				return 0;
			}
			if (!next && request != NULL) {
				*errstr = "Unexpected endOfMibView";
				return 0;
			}
			eomv = 1;
			break;
		default:
			*errstr = "invalid exception";
			return 0;
		}
	} else {
		*errstr = "invalid value";
		return 0;
	}

	if (request == NULL)
		return 1;

	cmp = ober_oid_cmp(&(request->av_oid), &(varbind->av_oid));
	if (next && !eomv) {
		if (request->av_include) {
			if (cmp > 0) {
				*errstr = "oid not incrementing";
				return 0;
			}
		} else {
			if (cmp >= 0) {
				*errstr = "oid not incrementing";
				return 0;
			}
		}
		if (range && ober_oid_cmp(&(varbind->av_oid),
		    &(request->av_oid_end)) > 0) {
			*errstr = "end oid not honoured";
			return 0;
		}
	} else {
		if (cmp != 0) {
			*errstr = "oids not equal";
			return 0;
		}
	}
	return 1;
}

int
appl_varbind_backend(struct appl_varbind_internal *ivb)
{
	struct appl_request_upstream *ureq = ivb->avi_request_upstream;
	struct appl_region search, *region, *pregion;
	struct appl_varbind *vb = &(ivb->avi_varbind);
	struct ber_oid oid, nextsibling;
	int next, cmp;

	next = ureq->aru_pdu->be_type == SNMP_C_GETNEXTREQ ||
	    ureq->aru_pdu->be_type == SNMP_C_GETBULKREQ;

	region = appl_region_find(ureq->aru_ctx, &(vb->av_oid));
	if (region == NULL) {
		if (!next) {
			vb->av_value = appl_exception(APPL_EXC_NOSUCHOBJECT);
			ivb->avi_state = APPL_VBSTATE_DONE;
			if (vb->av_value == NULL)
				return -1;
			return 0;
		}
		search.ar_oid = vb->av_oid;
		region = RB_NFIND(appl_regions,
		    &(ureq->aru_ctx->ac_regions), &search);
		if (region == NULL)
			goto eomv;
		vb->av_oid = region->ar_oid;
		vb->av_include = 1;
	}
	cmp = ober_oid_cmp(&(region->ar_oid), &(vb->av_oid));
	if (cmp == -2) {
		if (region->ar_instance) {
			if (!next) {
				vb->av_value =
				    appl_exception(APPL_EXC_NOSUCHINSTANCE);
				ivb->avi_state = APPL_VBSTATE_DONE;
				if (vb->av_value == NULL)
					return -1;
				return 0;
			}
			if ((region = appl_region_next(ureq->aru_ctx,
			    &(vb->av_oid), region)) == NULL)
				goto eomv;
			vb->av_oid = region->ar_oid;
			vb->av_include = 1;
		}
	} else if (cmp == 0) {
		if (region->ar_instance && next && !vb->av_include) {
			if ((region = appl_region_next(ureq->aru_ctx,
			    &(vb->av_oid), region)) == NULL)
				goto eomv;
			vb->av_oid = region->ar_oid;
			vb->av_include = 1;
		}
	}
	ivb->avi_region = region;
	if (next) {
		oid = vb->av_oid;
		/*
		 * For the searchrange end we only want contiguous regions.
		 * This means directly connecting, or overlapping with the same
		 * backend.
		 */
		do {
			pregion = region;
			region = appl_region_next(ureq->aru_ctx, &oid, pregion);
			if (region == NULL) {
				oid = pregion->ar_oid;
				ober_oid_nextsibling(&oid);
				break;
			}
			cmp = ober_oid_cmp(&(region->ar_oid), &oid);
			if (cmp == 2)
				oid = region->ar_oid;
			else if (cmp == 1) {
				/* Break out if we find a gap */
				nextsibling = pregion->ar_oid;
				ober_oid_nextsibling(&nextsibling);
				if (ober_oid_cmp(&(region->ar_oid),
				    &nextsibling) != 0) {
					oid = pregion->ar_oid;
					ober_oid_nextsibling(&oid);
					break;
				}
				oid = region->ar_oid;
			} else if (cmp == -2) {
				oid = pregion->ar_oid;
				ober_oid_nextsibling(&oid);
			} else
				fatalx("We can't stop/move back on getnext");
		} while (region->ar_backend == pregion->ar_backend);
		vb->av_oid_end = oid;
	}
	return 0;

 eomv:
	do {
		ivb->avi_varbind.av_value =
		    appl_exception(APPL_EXC_ENDOFMIBVIEW);
		ivb->avi_state = APPL_VBSTATE_DONE;
		if (ivb->avi_varbind.av_value == NULL)
			return -1;
		if (ivb->avi_sub != NULL)
			ivb->avi_sub->avi_varbind.av_oid =
			    ivb->avi_varbind.av_oid;
		ivb = ivb->avi_sub;
	} while (ivb != NULL);

	return 0;
}

void
appl_varbind_error(struct appl_varbind_internal *avi, enum appl_error error)
{
	struct appl_request_upstream *ureq = avi->avi_request_upstream;

	if (ureq->aru_error == APPL_ERROR_GENERR)
		return;
	if (ureq->aru_error != APPL_ERROR_NOERROR && error != APPL_ERROR_GENERR)
		return;
	ureq->aru_error = error;
	ureq->aru_index = avi->avi_index;
}

void
appl_report(struct snmp_message *msg, int32_t requestid, struct ber_oid *oid,
    struct ber_element *value)
{
	struct ber_element *varbind;

	varbind = ober_printf_elements(NULL, "{Oe}", oid, value);
	if (varbind == NULL) {
		log_warn("%"PRId32": ober_printf_elements", requestid);
		ober_free_elements(value);
		snmp_msgfree(msg);
		return;
	}

	snmpe_send(msg, SNMP_C_REPORT, requestid, 0, 0, varbind);
}

struct ber_element *
appl_exception(enum appl_exception type)
{
	struct ber_element *value;

	if ((value = ober_add_null(NULL)) == NULL) {
		log_warn("malloc");
		return NULL;
	}
	ober_set_header(value, BER_CLASS_CONTEXT, type);

	return value;
}

void
appl_pdu_log(struct appl_backend *backend, enum snmp_pdutype pdutype,
    int32_t requestid, uint16_t error, uint16_t index,
    struct appl_varbind *vblist)
{
	struct appl_varbind *vb;
	char buf[1024], oidbuf[1024], *str;
	int next;

	if (log_getverbose() < 2)
		return;

	next = (pdutype == SNMP_C_GETNEXTREQ || pdutype == SNMP_C_GETBULKREQ);

	buf[0] = '\0';
	for (vb = vblist; vb != NULL; vb = vb->av_next) {
		strlcat(buf, "{", sizeof(buf));
		strlcat(buf, smi_oid2string(&(vb->av_oid), oidbuf,
		    sizeof(oidbuf), 0), sizeof(buf));
		if (next) {
			if (vb->av_include)
				strlcat(buf, "(incl)", sizeof(buf));
			if (vb->av_oid_end.bo_n > 0) {
				strlcat(buf, "-", sizeof(buf));
				strlcat(buf, smi_oid2string(&(vb->av_oid_end),
				    oidbuf, sizeof(oidbuf), 0), sizeof(buf));
			}
		}
		strlcat(buf, ":", sizeof(buf));
		if (vb->av_value != NULL) {
			str = smi_print_element(vb->av_value);
			strlcat(buf, str == NULL ? "???" : str, sizeof(buf));
			free(str);
		} else
			strlcat(buf, "null", sizeof(buf));
		strlcat(buf, "}", sizeof(buf));
	}
	log_debug("%s%s%s{%"PRId32", %"PRIu16", %"PRIu16", {%s}}",
	    backend != NULL ? backend->ab_name : "",
	    backend != NULL ? ": " : "",
	    snmpe_pdutype2string(pdutype), requestid, error, index, buf);
}

void
ober_oid_nextsibling(struct ber_oid *oid)
{
	while (oid->bo_n > 0) {
		oid->bo_id[oid->bo_n - 1]++;
		/* Overflow check */
		if (oid->bo_id[oid->bo_n - 1] != 0)
			return;
		oid->bo_n--;
	}
}

int
appl_region_cmp(struct appl_region *r1, struct appl_region *r2)
{
	return ober_oid_cmp(&(r1->ar_oid), &(r2->ar_oid));
}

int
appl_request_cmp(struct appl_request_downstream *r1,
    struct appl_request_downstream *r2)
{
	return r1->ard_requestid < r2->ard_requestid ? -1 :
	    r1->ard_requestid > r2->ard_requestid;
}

RB_GENERATE_STATIC(appl_regions, appl_region, ar_entry, appl_region_cmp);
RB_GENERATE_STATIC(appl_requests, appl_request_downstream, ard_entry,
    appl_request_cmp);
