/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
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
#include <netinet/in.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "subagentx_internal.h"
#include "subagentx.h"

enum subagentx_index_type {
	SAI_TYPE_NEW,
	SAI_TYPE_ANY,
	SAI_TYPE_VALUE,
	SAI_TYPE_DYNAMIC
};

#define SUBAGENTX_CONTEXT_CTX(sac) (sac->sac_name_default ? NULL : \
    &(sac->sac_name))

struct subagentx_agentcaps {
	struct subagentx_context *saa_sac;
	struct agentx_oid saa_oid;
	struct agentx_ostring saa_descr;
	enum subagentx_cstate saa_cstate;
	enum subagentx_dstate saa_dstate;
	TAILQ_ENTRY(subagentx_agentcaps) saa_sac_agentcaps;
};

struct subagentx_region {
	struct subagentx_context *sar_sac;
	struct agentx_oid sar_oid;
	uint8_t sar_timeout;
	uint8_t sar_priority;
	enum subagentx_cstate sar_cstate;
	enum subagentx_dstate sar_dstate;
	TAILQ_HEAD(, subagentx_index) sar_indices;
	TAILQ_HEAD(, subagentx_object) sar_objects;
	TAILQ_ENTRY(subagentx_region) sar_sac_regions;
};

struct subagentx_index {
	struct subagentx_region *sai_sar;
	enum subagentx_index_type sai_type;
	struct agentx_varbind sai_vb;
	struct subagentx_object **sai_object;
	size_t sai_objectlen;
	size_t sai_objectsize;
	enum subagentx_cstate sai_cstate;
	enum subagentx_dstate sai_dstate;
	TAILQ_ENTRY(subagentx_index) sai_sar_indices;
};

struct subagentx_object {
	struct subagentx_region *sao_sar;
	struct agentx_oid sao_oid;
	struct subagentx_index *sao_index[SUBAGENTX_OID_INDEX_MAX_LEN];
	size_t sao_indexlen;
	int sao_implied;
	uint8_t sao_timeout;
	/* Prevent freeing object while in use by get and set requesets */
	uint32_t sao_lock;
	void (*sao_get)(struct subagentx_varbind *);
	enum subagentx_cstate sao_cstate;
	enum subagentx_dstate sao_dstate;
	RB_ENTRY(subagentx_object) sao_sac_objects;
	TAILQ_ENTRY(subagentx_object) sao_sar_objects;
};

struct subagentx_varbind {
	struct subagentx_get *sav_sag;
	struct subagentx_object *sav_sao;
	struct subagentx_varbind_index {
		struct subagentx_index *sav_sai;
		union agentx_data sav_idata;
		uint8_t sav_idatacomplete;
	} sav_index[SUBAGENTX_OID_INDEX_MAX_LEN];
	size_t sav_indexlen;
	int sav_initialized;
	int sav_include;
	struct agentx_varbind sav_vb;
	struct agentx_oid sav_start;
	struct agentx_oid sav_end;
	enum agentx_pdu_error sav_error;
};

#define SUBAGENTX_GET_CTX(sag) (sag->sag_context_default ? NULL : \
    &(sag->sag_context))
struct subagentx_request {
	uint32_t sar_packetid;
	int (*sar_cb)(struct agentx_pdu *, void *);
	void *sar_cookie;
	RB_ENTRY(subagentx_request) sar_sa_requests;
};

static void subagentx_start(struct subagentx *);
static void subagentx_finalize(struct subagentx *, int);
static void subagentx_wantwritenow(struct subagentx *, int);
void (*subagentx_wantwrite)(struct subagentx *, int) =
    subagentx_wantwritenow;
static void subagentx_reset(struct subagentx *);
static void subagentx_free_finalize(struct subagentx *);
static int subagentx_session_start(struct subagentx_session *);
static int subagentx_session_finalize(struct agentx_pdu *, void *);
static int subagentx_session_close(struct subagentx_session *,
    enum agentx_close_reason);
static int subagentx_session_close_finalize(struct agentx_pdu *, void *);
static void subagentx_session_free_finalize(struct subagentx_session *);
static void subagentx_session_reset(struct subagentx_session *);
static void subagentx_context_start(struct subagentx_context *);
static void subagentx_context_free_finalize(struct subagentx_context *);
static void subagentx_context_reset(struct subagentx_context *);
static int subagentx_agentcaps_start(struct subagentx_agentcaps *);
static int subagentx_agentcaps_finalize(struct agentx_pdu *, void *);
static int subagentx_agentcaps_close(struct subagentx_agentcaps *);
static int subagentx_agentcaps_close_finalize(struct agentx_pdu *, void *);
static void subagentx_agentcaps_free_finalize(struct subagentx_agentcaps *);
static void subagentx_agentcaps_reset(struct subagentx_agentcaps *);
static int subagentx_region_start(struct subagentx_region *);
static int subagentx_region_finalize(struct agentx_pdu *, void *);
static int subagentx_region_close(struct subagentx_region *);
static int subagentx_region_close_finalize(struct agentx_pdu *, void *);
static void subagentx_region_free_finalize(struct subagentx_region *);
static void subagentx_region_reset(struct subagentx_region *);
static struct subagentx_index *subagentx_index(struct subagentx_region *,
    struct agentx_varbind *, enum subagentx_index_type);
static int subagentx_index_start(struct subagentx_index *);
static int subagentx_index_finalize(struct agentx_pdu *, void *);
static void subagentx_index_free_finalize(struct subagentx_index *);
static void subagentx_index_reset(struct subagentx_index *);
static int subagentx_index_close(struct subagentx_index *);
static int subagentx_index_close_finalize(struct agentx_pdu *, void *);
static int subagentx_object_start(struct subagentx_object *);
static int subagentx_object_finalize(struct agentx_pdu *, void *);
static int subagentx_object_lock(struct subagentx_object *);
static void subagentx_object_unlock(struct subagentx_object *);
static int subagentx_object_close(struct subagentx_object *);
static int subagentx_object_close_finalize(struct agentx_pdu *, void *);
static void subagentx_object_free_finalize(struct subagentx_object *);
static void subagentx_object_reset(struct subagentx_object *);
static int subagentx_object_cmp(struct subagentx_object *,
    struct subagentx_object *);
static void subagentx_get_start(struct subagentx_context *,
    struct agentx_pdu *);
static void subagentx_get_finalize(struct subagentx_get *);
static void subagentx_get_free(struct subagentx_get *);
static void subagentx_varbind_start(struct subagentx_varbind *);
static void subagentx_varbind_finalize(struct subagentx_varbind *);
static void subagentx_varbind_nosuchobject(struct subagentx_varbind *);
static void subagentx_varbind_nosuchinstance(struct subagentx_varbind *);
static void subagentx_varbind_endofmibview(struct subagentx_varbind *);
static void subagentx_varbind_error_type(struct subagentx_varbind *,
    enum agentx_pdu_error, int);
static int subagentx_request(struct subagentx *, uint32_t,
    int (*)(struct agentx_pdu *, void *), void *);
static int subagentx_request_cmp(struct subagentx_request *,
    struct subagentx_request *);
static int subagentx_strcat(char **, const char *);

RB_PROTOTYPE_STATIC(sa_requests, subagentx_request, sar_sa_requests,
    subagentx_request_cmp)
RB_PROTOTYPE_STATIC(sac_objects, subagentx_object, sao_sac_objects,
    subagentx_object_cmp)

struct subagentx *
subagentx(void (*nofd)(struct subagentx *, void *, int), void *cookie)
{
	struct subagentx *sa;

	if ((sa = calloc(1, sizeof(*sa))) == NULL)
		return NULL;

	sa->sa_nofd = nofd;
	sa->sa_cookie = cookie;
	sa->sa_fd = -1;
	sa->sa_cstate = SA_CSTATE_CLOSE;
	sa->sa_dstate = SA_DSTATE_OPEN;
	TAILQ_INIT(&(sa->sa_sessions));
	TAILQ_INIT(&(sa->sa_getreqs));
	RB_INIT(&(sa->sa_requests));

	subagentx_start(sa);

	return sa;
}

/*
 * subagentx_finalize is not a suitable name for a public API,
 * but use it internally for consistency
 */
void
subagentx_connect(struct subagentx *sa, int fd)
{
	subagentx_finalize(sa, fd);
}

static void
subagentx_start(struct subagentx *sa)
{
#ifdef AGENTX_DEBUG
	if (sa->sa_cstate != SA_CSTATE_CLOSE ||
	    sa->sa_dstate != SA_DSTATE_OPEN)
		subagentx_log_sa_fatalx(sa, "%s: unexpected connect", __func__);
#endif
	sa->sa_cstate = SA_CSTATE_WAITOPEN;
	sa->sa_nofd(sa, sa->sa_cookie, 0);
}

static void
subagentx_finalize(struct subagentx *sa, int fd)
{
	struct subagentx_session *sas;

	if (sa->sa_cstate != SA_CSTATE_WAITOPEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sa_fatalx(sa, "%s: subagentx unexpected connect",
		    __func__);
#else
		subagentx_log_sa_warnx(sa,
		    "%s: subagentx unexpected connect: ignoring", __func__);
		return;
#endif
	}
	if ((sa->sa_ax = agentx_new(fd)) == NULL) {
		subagentx_log_sa_warn(sa, "failed to initialize");
		close(fd);
		subagentx_reset(sa);
		return;
	}

	subagentx_log_sa_info(sa, "new connection: %d", fd);

	sa->sa_fd = fd;
	sa->sa_cstate = SA_CSTATE_OPEN;

	TAILQ_FOREACH(sas, &(sa->sa_sessions), sas_sa_sessions) {
		if (subagentx_session_start(sas) == -1)
			break;
	}
}

static void
subagentx_wantwritenow(struct subagentx *sa, int fd)
{
	subagentx_write(sa);
}

static void
subagentx_reset(struct subagentx *sa)
{
	struct subagentx_session *sas, *tsas;
	struct subagentx_request *sar;
	struct subagentx_get *sag;

	agentx_free(sa->sa_ax);
	sa->sa_ax = NULL;
	sa->sa_fd = -1;

	sa->sa_cstate = SA_CSTATE_CLOSE;

	while ((sar = RB_MIN(sa_requests, &(sa->sa_requests))) != NULL) {
		RB_REMOVE(sa_requests, &(sa->sa_requests), sar);
		free(sar);
	}
	TAILQ_FOREACH_SAFE(sas, &(sa->sa_sessions), sas_sa_sessions, tsas)
		subagentx_session_reset(sas);
	while (!TAILQ_EMPTY(&(sa->sa_getreqs))) {
		sag = TAILQ_FIRST(&(sa->sa_getreqs));
		sag->sag_sac = NULL;
		TAILQ_REMOVE(&(sa->sa_getreqs), sag, sag_sa_getreqs);
	}

	if (sa->sa_dstate == SA_DSTATE_CLOSE) {
		subagentx_free_finalize(sa);
		return;
	}

	subagentx_start(sa);
}

void
subagentx_free(struct subagentx *sa)
{
	struct subagentx_session *sas, *tsas;

	if (sa == NULL)
		return;

	if (sa->sa_dstate == SA_DSTATE_CLOSE) {
/* Malloc throws abort on invalid pointers as well */
		subagentx_log_sa_fatalx(sa, "%s: double free", __func__);
	}
	sa->sa_dstate = SA_DSTATE_CLOSE;

	if (!TAILQ_EMPTY(&(sa->sa_sessions))) {
		TAILQ_FOREACH_SAFE(sas, &(sa->sa_sessions), sas_sa_sessions,
		    tsas) {
			if (sas->sas_dstate != SA_DSTATE_CLOSE)
				subagentx_session_free(sas);
		}
	} else
		subagentx_free_finalize(sa);
}

static void
subagentx_free_finalize(struct subagentx *sa)
{
#ifdef AGENTX_DEBUG
	if (sa->sa_dstate != SA_DSTATE_CLOSE)
		subagentx_log_sa_fatalx(sa, "%s: subagentx not closing",
		    __func__);
	if (!TAILQ_EMPTY(&(sa->sa_sessions)))
		subagentx_log_sa_fatalx(sa, "%s: subagentx still has sessions",
		    __func__);
	if (!RB_EMPTY(&(sa->sa_requests)))
		subagentx_log_sa_fatalx(sa,
		    "%s: subagentx still has pending requests", __func__);
#endif

	agentx_free(sa->sa_ax);
	sa->sa_nofd(sa, sa->sa_cookie, 1);
	free(sa);
}

struct subagentx_session *
subagentx_session(struct subagentx *sa, uint32_t oid[],
    size_t oidlen, const char *descr, uint8_t timeout)
{
	struct subagentx_session *sas;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sa_fatalx(sa, "%s: oidlen > %d", __func__,
		    SUBAGENTX_OID_MAX_LEN);
#else
		errno = EINVAL;
		return NULL;
#endif
	}
	if ((sas = calloc(1, sizeof(*sas))) == NULL)
		return NULL;

	sas->sas_sa = sa;
	sas->sas_timeout = timeout;
	for (i = 0; i < oidlen; i++)
		sas->sas_oid.aoi_id[i] = oid[i];
	sas->sas_oid.aoi_idlen = oidlen;
	sas->sas_descr.aos_string = (unsigned char *)strdup(descr);
	if (sas->sas_descr.aos_string == NULL) {
		free(sas);
		return NULL;
	}
	sas->sas_descr.aos_slen = strlen(descr);
	sas->sas_cstate = SA_CSTATE_CLOSE;
	sas->sas_dstate = SA_DSTATE_OPEN;
	TAILQ_INIT(&(sas->sas_contexts));
	TAILQ_INSERT_HEAD(&(sa->sa_sessions), sas, sas_sa_sessions);

	if (sa->sa_cstate == SA_CSTATE_OPEN)
		(void) subagentx_session_start(sas);

	return sas;
}

static int
subagentx_session_start(struct subagentx_session *sas)
{
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sa->sa_cstate != SA_CSTATE_OPEN ||
	    sas->sas_cstate != SA_CSTATE_CLOSE ||
	    sas->sas_dstate != SA_DSTATE_OPEN)
		subagentx_log_sa_fatalx(sa, "%s: unexpected session open",
		    __func__);
#endif
	packetid = agentx_open(sa->sa_ax, sas->sas_timeout, &(sas->sas_oid),
	    &(sas->sas_descr));
	if (packetid == 0) {
		subagentx_log_sa_warn(sa, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_OPEN));
		subagentx_reset(sa);
		return -1;
	}
	sas->sas_packetid = packetid;
	subagentx_log_sa_info(sa, "opening session");
	sas->sas_cstate = SA_CSTATE_WAITOPEN;
	return subagentx_request(sa, packetid, subagentx_session_finalize, sas);
}

static int
subagentx_session_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_session *sas = cookie;
	struct subagentx *sa = sas->sas_sa;
	struct subagentx_context *sac;

#ifdef AGENTX_DEBUG
	if (sas->sas_cstate != SA_CSTATE_WAITOPEN)
		subagentx_log_sa_fatalx(sa, "%s: not expecting new session",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		subagentx_log_sa_warnx(sa, "failed to open session: %s",
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		subagentx_reset(sa);
		return -1;
	}

	sas->sas_id = pdu->ap_header.aph_sessionid;
	sas->sas_cstate = SA_CSTATE_OPEN;

	if (sas->sas_dstate == SA_DSTATE_CLOSE) {
		subagentx_session_close(sas, AGENTX_CLOSE_SHUTDOWN);
		return 0;
	}

	subagentx_log_sas_info(sas, "open");

	TAILQ_FOREACH(sac, &(sas->sas_contexts), sac_sas_contexts)
		subagentx_context_start(sac);
	return 0;
}

static int
subagentx_session_close(struct subagentx_session *sas,
    enum agentx_close_reason reason)
{
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sas->sas_cstate != SA_CSTATE_OPEN)
		subagentx_log_sa_fatalx(sa, "%s: unexpected session close",
		    __func__);
#endif
	if ((packetid = agentx_close(sa->sa_ax, sas->sas_id, reason)) == 0) {
		subagentx_log_sas_warn(sas, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_CLOSE));
		subagentx_reset(sa);
		return -1;
	}

	subagentx_log_sas_info(sas, "closing session: %s",
	    agentx_closereason2string(reason));

	sas->sas_cstate = SA_CSTATE_WAITCLOSE;
	return subagentx_request(sa, packetid, subagentx_session_close_finalize,
	    sas);
}

static int
subagentx_session_close_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_session *sas = cookie;
	struct subagentx *sa = sas->sas_sa;
	struct subagentx_context *sac, *tsac;

#ifdef AGENTX_DEBUG
	if (sas->sas_cstate != SA_CSTATE_WAITCLOSE)
		subagentx_log_sas_fatalx(sas, "%s: not expecting session close",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		subagentx_log_sas_warnx(sas, "failed to close session: %s",
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		subagentx_reset(sa);
		return -1;
	}

	sas->sas_cstate = SA_CSTATE_CLOSE;

	subagentx_log_sas_info(sas, "closed");

	TAILQ_FOREACH_SAFE(sac, &(sas->sas_contexts), sac_sas_contexts, tsac)
		subagentx_context_reset(sac);

	if (sas->sas_dstate == SA_DSTATE_CLOSE)
		subagentx_session_free_finalize(sas);
	else {
		if (sa->sa_cstate == SA_CSTATE_OPEN)
			if (subagentx_session_start(sas) == -1)
				return -1;
	}
	return 0;
}

void
subagentx_session_free(struct subagentx_session *sas)
{
	struct subagentx_context *sac, *tsac;

	if (sas == NULL)
		return;

	if (sas->sas_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sas_fatalx(sas, "%s: double free", __func__);

	sas->sas_dstate = SA_DSTATE_CLOSE;

	if (sas->sas_cstate == SA_CSTATE_OPEN)
		(void) subagentx_session_close(sas, AGENTX_CLOSE_SHUTDOWN);

	TAILQ_FOREACH_SAFE(sac, &(sas->sas_contexts), sac_sas_contexts, tsac) {
		if (sac->sac_dstate != SA_DSTATE_CLOSE)
			subagentx_context_free(sac);
	}

	if (sas->sas_cstate == SA_CSTATE_CLOSE)
		subagentx_session_free_finalize(sas);
}

static void
subagentx_session_free_finalize(struct subagentx_session *sas)
{
	struct subagentx *sa = sas->sas_sa;

#ifdef AGENTX_DEBUG
	if (sas->sas_cstate != SA_CSTATE_CLOSE)
		subagentx_log_sas_fatalx(sas, "%s: free without closing",
		    __func__);
	if (!TAILQ_EMPTY(&(sas->sas_contexts)))
		subagentx_log_sas_fatalx(sas,
		    "%s: subagentx still has contexts", __func__);
#endif

	TAILQ_REMOVE(&(sa->sa_sessions), sas, sas_sa_sessions);
	free(sas->sas_descr.aos_string);
	free(sas);

	if (TAILQ_EMPTY(&(sa->sa_sessions)) && sa->sa_dstate == SA_DSTATE_CLOSE)
		subagentx_free_finalize(sa);
}

static void
subagentx_session_reset(struct subagentx_session *sas)
{
	struct subagentx_context *sac, *tsac;

	sas->sas_cstate = SA_CSTATE_CLOSE;

	TAILQ_FOREACH_SAFE(sac, &(sas->sas_contexts), sac_sas_contexts, tsac)
		subagentx_context_reset(sac);

	if (sas->sas_dstate == SA_DSTATE_CLOSE)
		subagentx_session_free_finalize(sas);
}

struct subagentx_context *
subagentx_context(struct subagentx_session *sas, const char *name)
{
	struct subagentx_context *sac;

	if (sas->sas_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sas_fatalx(sas, "%s: use after free", __func__);

	if ((sac = calloc(1, sizeof(*sac))) == NULL)
		return NULL;

	sac->sac_sas = sas;
	sac->sac_name_default = (name == NULL);
	if (name != NULL) {
		sac->sac_name.aos_string = (unsigned char *)strdup(name);
		if (sac->sac_name.aos_string == NULL) {
			free(sac);
			return NULL;
		}
		sac->sac_name.aos_slen = strlen(name);
	}
	sac->sac_cstate = sas->sas_cstate == SA_CSTATE_OPEN ?
	    SA_CSTATE_OPEN : SA_CSTATE_CLOSE;
	sac->sac_dstate = SA_DSTATE_OPEN;
	TAILQ_INIT(&(sac->sac_agentcaps));
	TAILQ_INIT(&(sac->sac_regions));

	TAILQ_INSERT_HEAD(&(sas->sas_contexts), sac, sac_sas_contexts);

	return sac;
}

static void
subagentx_context_start(struct subagentx_context *sac)
{
	struct subagentx_agentcaps *saa;
	struct subagentx_region *sar;

#ifdef AGENTX_DEBUG
	if (sac->sac_cstate != SA_CSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected context start",
		    __func__);
#endif
	sac->sac_cstate = SA_CSTATE_OPEN;

	TAILQ_FOREACH(saa, &(sac->sac_agentcaps), saa_sac_agentcaps) {
		if (subagentx_agentcaps_start(saa) == -1)
			return;
	}
	TAILQ_FOREACH(sar, &(sac->sac_regions), sar_sac_regions) {
		if (subagentx_region_start(sar) == -1)
			return;
	}
}

uint32_t
subagentx_context_uptime(struct subagentx_context *sac)
{
	struct timespec cur, res;

	if (sac->sac_sysuptimespec.tv_sec == 0 &&
	    sac->sac_sysuptimespec.tv_nsec == 0)
		return 0;

	(void) clock_gettime(CLOCK_MONOTONIC, &cur);

	timespecsub(&cur, &(sac->sac_sysuptimespec), &res);

	return sac->sac_sysuptime +
	    (uint32_t) ((res.tv_sec * 100) + (res.tv_nsec / 10000000));
}

struct subagentx_object *
subagentx_context_object_find(struct subagentx_context *sac,
    const uint32_t oid[], size_t oidlen, int active, int instance)
{
	struct subagentx_object *sao, sao_search;
	size_t i;

	for (i = 0; i < oidlen; i++)
		sao_search.sao_oid.aoi_id[i] = oid[i];
	sao_search.sao_oid.aoi_idlen = oidlen;

	sao = RB_FIND(sac_objects, &(sac->sac_objects), &sao_search);
	while (sao == NULL && !instance && sao_search.sao_oid.aoi_idlen > 0) {
		sao = RB_FIND(sac_objects, &(sac->sac_objects), &sao_search);
		sao_search.sao_oid.aoi_idlen--;
	}
	if (active && sao != NULL && sao->sao_cstate != SA_CSTATE_OPEN)
		return NULL;
	return sao;
}

struct subagentx_object *
subagentx_context_object_nfind(struct subagentx_context *sac,
    const uint32_t oid[], size_t oidlen, int active, int inclusive)
{
	struct subagentx_object *sao, sao_search;
	size_t i;

	for (i = 0; i < oidlen; i++)
		sao_search.sao_oid.aoi_id[i] = oid[i];
	sao_search.sao_oid.aoi_idlen = oidlen;

	sao = RB_NFIND(sac_objects, &(sac->sac_objects), &sao_search);
	if (!inclusive && sao != NULL &&
	    agentx_oid_cmp(&(sao_search.sao_oid), &(sao->sao_oid)) <= 0) {
		sao = RB_NEXT(sac_objects, &(sac->sac_objects), sao);
	}
	
	while (active && sao != NULL && sao->sao_cstate != SA_CSTATE_OPEN)
		sao = RB_NEXT(sac_objects, &(sac->sac_objects), sao);
	return sao;
}

void
subagentx_context_free(struct subagentx_context *sac)
{
	struct subagentx_agentcaps *saa, *tsaa;
	struct subagentx_region *sar, *tsar;

	if (sac == NULL)
		return;

#ifdef AGENTX_DEBUG
	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: double free", __func__);
#endif
	sac->sac_dstate = SA_DSTATE_CLOSE;

	TAILQ_FOREACH_SAFE(saa, &(sac->sac_agentcaps), saa_sac_agentcaps,
	    tsaa) {
		if (saa->saa_dstate != SA_DSTATE_CLOSE)
			subagentx_agentcaps_free(saa);
	}
	TAILQ_FOREACH_SAFE(sar, &(sac->sac_regions), sar_sac_regions, tsar) {
		if (sar->sar_dstate != SA_DSTATE_CLOSE)
			subagentx_region_free(sar);
	}
}

static void
subagentx_context_free_finalize(struct subagentx_context *sac)
{
	struct subagentx_session *sas = sac->sac_sas;

#ifdef AGENTX_DEBUG
	if (sac->sac_dstate != SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected context free",
		    __func__);
#endif
	if (!TAILQ_EMPTY(&(sac->sac_regions)) ||
	    !TAILQ_EMPTY(&(sac->sac_agentcaps)))
		return;
	TAILQ_REMOVE(&(sas->sas_contexts), sac, sac_sas_contexts);
	free(sac->sac_name.aos_string);
	free(sac);
}

static void
subagentx_context_reset(struct subagentx_context *sac)
{
	struct subagentx_agentcaps *saa, *tsaa;
	struct subagentx_region *sar, *tsar;

	sac->sac_cstate = SA_CSTATE_CLOSE;
	sac->sac_sysuptimespec.tv_sec = 0;
	sac->sac_sysuptimespec.tv_nsec = 0;

	TAILQ_FOREACH_SAFE(saa, &(sac->sac_agentcaps), saa_sac_agentcaps, tsaa)
		subagentx_agentcaps_reset(saa);
	TAILQ_FOREACH_SAFE(sar, &(sac->sac_regions), sar_sac_regions, tsar)
		subagentx_region_reset(sar);

	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_context_free_finalize(sac);
}

struct subagentx_agentcaps *
subagentx_agentcaps(struct subagentx_context *sac, uint32_t oid[],
    size_t oidlen, const char *descr)
{
	struct subagentx_agentcaps *saa;
	size_t i;

	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: use after free", __func__);

	if ((saa = calloc(1, sizeof(*saa))) == NULL)
		return NULL;

	saa->saa_sac = sac;
	for (i = 0; i < oidlen; i++)
		saa->saa_oid.aoi_id[i] = oid[i];
	saa->saa_oid.aoi_idlen = oidlen;
	saa->saa_descr.aos_string = (unsigned char *)strdup(descr);
	if (saa->saa_descr.aos_string == NULL) {
		free(saa);
		return NULL;
	}
	saa->saa_descr.aos_slen = strlen(descr);
	saa->saa_cstate = SA_CSTATE_CLOSE;
	saa->saa_dstate = SA_DSTATE_OPEN;

	TAILQ_INSERT_TAIL(&(sac->sac_agentcaps), saa, saa_sac_agentcaps);

	if (sac->sac_cstate == SA_CSTATE_OPEN)
		subagentx_agentcaps_start(saa);

	return saa;
}

static int
subagentx_agentcaps_start(struct subagentx_agentcaps *saa)
{
	struct subagentx_context *sac = saa->saa_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sac->sac_cstate != SA_CSTATE_OPEN ||
	    saa->saa_cstate != SA_CSTATE_CLOSE ||
	    saa->saa_dstate != SA_DSTATE_OPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: unexpected region registration", __func__);
#endif

	packetid = agentx_addagentcaps(sa->sa_ax, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), &(saa->saa_oid), &(saa->saa_descr));
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_ADDAGENTCAPS));
		subagentx_reset(sa);
		return -1;
	}
	subagentx_log_sac_info(sac, "agentcaps %s: opening",
	    agentx_oid2string(&(saa->saa_oid)));
	saa->saa_cstate = SA_CSTATE_WAITOPEN;
	return subagentx_request(sa, packetid, subagentx_agentcaps_finalize,
	    saa);
}

static int
subagentx_agentcaps_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_agentcaps *saa = cookie;
	struct subagentx_context *sac = saa->saa_sac;

#ifdef AGENTX_DEBUG
	if (saa->saa_cstate != SA_CSTATE_WAITOPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: not expecting agentcaps open", __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		/* Agentcaps failing is nothing too serious */
		subagentx_log_sac_warn(sac, "agentcaps %s: %s",
		    agentx_oid2string(&(saa->saa_oid)),
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		saa->saa_cstate = SA_CSTATE_CLOSE;
		return 0;
	}

	saa->saa_cstate = SA_CSTATE_OPEN;

	subagentx_log_sac_info(sac, "agentcaps %s: open",
	    agentx_oid2string(&(saa->saa_oid)));

	if (saa->saa_dstate == SA_DSTATE_CLOSE)
		subagentx_agentcaps_close(saa);

	return 0;
}

static int
subagentx_agentcaps_close(struct subagentx_agentcaps *saa)
{
	struct subagentx_context *sac = saa->saa_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (saa->saa_cstate != SA_CSTATE_OPEN)
		subagentx_log_sac_fatalx(sac, "%s: unexpected agentcaps close",
		    __func__);
#endif

	saa->saa_cstate = SA_CSTATE_WAITCLOSE;
	if (sas->sas_cstate == SA_CSTATE_WAITCLOSE)
		return 0;

	packetid = agentx_removeagentcaps(sa->sa_ax, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), &(saa->saa_oid));
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_REMOVEAGENTCAPS));
		subagentx_reset(sa);
		return -1;
	}
	subagentx_log_sac_info(sac, "agentcaps %s: closing",
	    agentx_oid2string(&(saa->saa_oid)));
	return subagentx_request(sa, packetid,
	    subagentx_agentcaps_close_finalize, saa);
}

static int
subagentx_agentcaps_close_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_agentcaps *saa = cookie;
	struct subagentx_context *sac = saa->saa_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;

#ifdef AGENTX_DEBUG
	if (saa->saa_cstate != SA_CSTATE_WAITCLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected agentcaps close",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		subagentx_log_sac_warnx(sac, "agentcaps %s: %s",
		    agentx_oid2string(&(saa->saa_oid)),
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		subagentx_reset(sa);
		return -1;
	}

	saa->saa_cstate = SA_CSTATE_CLOSE;

	subagentx_log_sac_info(sac, "agentcaps %s: closed",
	    agentx_oid2string(&(saa->saa_oid)));

	if (saa->saa_dstate == SA_DSTATE_CLOSE) {
		subagentx_agentcaps_free_finalize(saa);
		return 0;
	} else {
		if (sac->sac_cstate == SA_CSTATE_OPEN) {
			if (subagentx_agentcaps_start(saa) == -1)
				return -1;
		}
	}
	return 0;
}

void
subagentx_agentcaps_free(struct subagentx_agentcaps *saa)
{
	if (saa == NULL)
		return;

	if (saa->saa_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(saa->saa_sac, "%s: double free",
		    __func__);

	saa->saa_dstate = SA_DSTATE_CLOSE;

	if (saa->saa_cstate == SA_CSTATE_OPEN) {
		if (subagentx_agentcaps_close(saa) == -1)
			return;
	}

	if (saa->saa_cstate == SA_CSTATE_CLOSE)
		subagentx_agentcaps_free_finalize(saa);
}

static void
subagentx_agentcaps_free_finalize(struct subagentx_agentcaps *saa)
{
	struct subagentx_context *sac = saa->saa_sac;

#ifdef AGENTX_DEBUG
	if (saa->saa_dstate != SA_DSTATE_CLOSE ||
	    saa->saa_cstate != SA_CSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected free", __func__);
#endif

	TAILQ_REMOVE(&(sac->sac_agentcaps), saa, saa_sac_agentcaps);
	free(saa->saa_descr.aos_string);
	free(saa);

	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_context_free_finalize(sac);
}

static void
subagentx_agentcaps_reset(struct subagentx_agentcaps *saa)
{
	saa->saa_cstate = SA_CSTATE_CLOSE;

	if (saa->saa_dstate == SA_DSTATE_CLOSE)
		subagentx_agentcaps_free_finalize(saa);
}

struct subagentx_region *
subagentx_region(struct subagentx_context *sac, uint32_t oid[],
    size_t oidlen, uint8_t timeout)
{
	struct subagentx_region *sar;
	struct agentx_oid tmpoid;
	size_t i;

	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: use after free", __func__);
	if (oidlen < 1) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sac, "%s: oidlen == 0", __func__);
#else
		errno = EINVAL;
		return NULL;
#endif
	}
	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sac, "%s: oidlen > %d", __func__,
		    SUBAGENTX_OID_MAX_LEN);
#else
		errno = EINVAL;
		return NULL;
#endif
	}

	for (i = 0; i < oidlen; i++)
		tmpoid.aoi_id[i] = oid[i];
	tmpoid.aoi_idlen = oidlen;
	TAILQ_FOREACH(sar, &(sac->sac_regions), sar_sac_regions) {
		if (agentx_oid_cmp(&(sar->sar_oid), &tmpoid) == 0) {
#ifdef AGENTX_DEBUG
			subagentx_log_sac_fatalx(sac,
			    "%s: duplicate region registration", __func__);
#else
			errno = EINVAL;
			return NULL;
#endif
		}
	}

	if ((sar = calloc(1, sizeof(*sar))) == NULL)
		return NULL;

	sar->sar_sac = sac;
	sar->sar_timeout = timeout;
	sar->sar_priority = AGENTX_PRIORITY_DEFAULT;
	bcopy(&tmpoid, &(sar->sar_oid), sizeof(sar->sar_oid));
	sar->sar_cstate = SA_CSTATE_CLOSE;
	sar->sar_dstate = SA_DSTATE_OPEN;
	TAILQ_INIT(&(sar->sar_indices));
	TAILQ_INIT(&(sar->sar_objects));

	TAILQ_INSERT_HEAD(&(sac->sac_regions), sar, sar_sac_regions);

	if (sac->sac_cstate == SA_CSTATE_OPEN)
		(void) subagentx_region_start(sar);

	return sar;
}

static int
subagentx_region_start(struct subagentx_region *sar)
{
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sac->sac_cstate != SA_CSTATE_OPEN ||
	    sar->sar_cstate != SA_CSTATE_CLOSE ||
	    sar->sar_dstate != SA_DSTATE_OPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: unexpected region registration", __func__);
#endif

	packetid = agentx_register(sa->sa_ax, 0, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), sar->sar_timeout, sar->sar_priority,
	    0, &(sar->sar_oid), 0);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_REGISTER));
		subagentx_reset(sa);
		return -1;
	}
	subagentx_log_sac_info(sac, "region %s: opening",
	    agentx_oid2string(&(sar->sar_oid)));
	sar->sar_cstate = SA_CSTATE_WAITOPEN;
	return subagentx_request(sa, packetid, subagentx_region_finalize, sar);
}

static int
subagentx_region_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_region *sar = cookie;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct subagentx_index *sai;
	struct subagentx_object *sao;

#ifdef AGENTX_DEBUG
	if (sar->sar_cstate != SA_CSTATE_WAITOPEN)
		subagentx_log_sac_fatalx(sac, "%s: not expecting region open",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error == AGENTX_PDU_ERROR_NOERROR) {
		sar->sar_cstate = SA_CSTATE_OPEN;
		subagentx_log_sac_info(sac, "region %s: open",
		    agentx_oid2string(&(sar->sar_oid)));
	} else if (pdu->ap_payload.ap_response.ap_error ==
	    AGENTX_PDU_ERROR_DUPLICATEREGISTRATION) {
		sar->sar_cstate = SA_CSTATE_CLOSE;
		/* Try at lower priority: first come first serve */
		if ((++sar->sar_priority) != 0) {
			subagentx_log_sac_warnx(sac, "region %s: duplicate, "
			    "reducing priority",
			    agentx_oid2string(&(sar->sar_oid)));
			return subagentx_region_start(sar);
		}
		subagentx_log_sac_info(sac, "region %s: duplicate, can't "
		    "reduce priority, ignoring", 
		    agentx_oid2string(&(sar->sar_oid)));
	} else if (pdu->ap_payload.ap_response.ap_error ==
	    AGENTX_PDU_ERROR_REQUESTDENIED) {
		sar->sar_cstate = SA_CSTATE_CLOSE;
		subagentx_log_sac_warnx(sac, "region %s: %s",
		     agentx_oid2string(&(sar->sar_oid)),
		     agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		/*
		 * If we can't register a region, related objects are useless.
		 * But no need to retry.
		 */
		return 0;
	} else {
		subagentx_log_sac_info(sac, "region %s: %s",
		    agentx_oid2string(&(sar->sar_oid)),
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		subagentx_reset(sa);
		return -1;
	}

	if (sar->sar_dstate == SA_DSTATE_CLOSE) {
		if (subagentx_region_close(sar) == -1)
			return -1;
	} else {
		TAILQ_FOREACH(sai, &(sar->sar_indices), sai_sar_indices) {
			if (subagentx_index_start(sai) == -1)
				return -1;
		}
		TAILQ_FOREACH(sao, &(sar->sar_objects), sao_sar_objects) {
			if (subagentx_object_start(sao) == -1)
				return -1;
		}
	}
	return 0;
}

static int
subagentx_region_close(struct subagentx_region *sar)
{
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sar->sar_cstate != SA_CSTATE_OPEN)
		subagentx_log_sac_fatalx(sac, "%s: unexpected region close",
		    __func__);
#endif

	sar->sar_cstate = SA_CSTATE_WAITCLOSE;
	if (sas->sas_cstate == SA_CSTATE_WAITCLOSE)
		return 0;

	packetid = agentx_unregister(sa->sa_ax, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), sar->sar_priority, 0, &(sar->sar_oid),
	    0);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_UNREGISTER));
		subagentx_reset(sa);
		return -1;
	}
	subagentx_log_sac_info(sac, "region %s: closing",
	    agentx_oid2string(&(sar->sar_oid)));
	return subagentx_request(sa, packetid, subagentx_region_close_finalize,
	    sar);
}

static int
subagentx_region_close_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_region *sar = cookie;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;

#ifdef AGENTX_DEBUG
	if (sar->sar_cstate != SA_CSTATE_WAITCLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected region close",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		subagentx_log_sac_warnx(sac, "closing %s: %s",
		    agentx_oid2string(&(sar->sar_oid)),
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		subagentx_reset(sa);
		return -1;
	}

	sar->sar_priority = AGENTX_PRIORITY_DEFAULT;
	sar->sar_cstate = SA_CSTATE_CLOSE;

	subagentx_log_sac_info(sac, "region %s: closed",
	    agentx_oid2string(&(sar->sar_oid)));

	if (sar->sar_dstate == SA_DSTATE_CLOSE) {
		subagentx_region_free_finalize(sar);
		return 0;
	} else {
		if (sac->sac_cstate == SA_CSTATE_OPEN) {
			if (subagentx_region_start(sar) == -1)
				return -1;
		}
	}
	return 0;
}

void
subagentx_region_free(struct subagentx_region *sar)
{
	struct subagentx_index *sai, *tsai;
	struct subagentx_object *sao, *tsao;

	if (sar == NULL)
		return;

	if (sar->sar_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: double free",
		    __func__);

	sar->sar_dstate = SA_DSTATE_CLOSE;

	TAILQ_FOREACH_SAFE(sai, &(sar->sar_indices), sai_sar_indices, tsai) {
		if (sai->sai_dstate != SA_DSTATE_CLOSE)
			subagentx_index_free(sai);
	}

	TAILQ_FOREACH_SAFE(sao, &(sar->sar_objects), sao_sar_objects, tsao) {
		if (sao->sao_dstate != SA_DSTATE_CLOSE)
			subagentx_object_free(sao);
	}

	if (sar->sar_cstate == SA_CSTATE_OPEN) {
		if (subagentx_region_close(sar) == -1)
			return;
	}

	if (sar->sar_cstate == SA_CSTATE_CLOSE)
		subagentx_region_free_finalize(sar);
}

static void
subagentx_region_free_finalize(struct subagentx_region *sar)
{
	struct subagentx_context *sac = sar->sar_sac;

#ifdef AGENTX_DEBUG
	if (sar->sar_dstate != SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected free", __func__);
#endif

	if (!TAILQ_EMPTY(&(sar->sar_indices)) ||
	    !TAILQ_EMPTY(&(sar->sar_objects)))
		return;

	if (sar->sar_cstate != SA_CSTATE_CLOSE)
		return;

	TAILQ_REMOVE(&(sac->sac_regions), sar, sar_sac_regions);
	free(sar);

	if (sac->sac_dstate == SA_DSTATE_CLOSE)
		subagentx_context_free_finalize(sac);
}

static void
subagentx_region_reset(struct subagentx_region *sar)
{
	struct subagentx_index *sai, *tsai;
	struct subagentx_object *sao, *tsao;

	sar->sar_cstate = SA_CSTATE_CLOSE;
	sar->sar_priority = AGENTX_PRIORITY_DEFAULT;

	TAILQ_FOREACH_SAFE(sai, &(sar->sar_indices), sai_sar_indices, tsai)
		subagentx_index_reset(sai);
	TAILQ_FOREACH_SAFE(sao, &(sar->sar_objects), sao_sar_objects, tsao)
		subagentx_object_reset(sao);

	if (sar->sar_dstate == SA_DSTATE_CLOSE)
		subagentx_region_free_finalize(sar);
}

struct subagentx_index *
subagentx_index_integer_new(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_INTEGER;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_uint32 = 0;

	return subagentx_index(sar, &vb, SAI_TYPE_NEW);
}

struct subagentx_index *
subagentx_index_integer_any(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_INTEGER;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_uint32 = 0;

	return subagentx_index(sar, &vb, SAI_TYPE_ANY);
}

struct subagentx_index *
subagentx_index_integer_value(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen, uint32_t value)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_INTEGER;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_uint32 = value;

	return subagentx_index(sar, &vb, SAI_TYPE_VALUE);
}

struct subagentx_index *
subagentx_index_integer_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_INTEGER;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

struct subagentx_index *
subagentx_index_string_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_OCTETSTRING;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_ostring.aos_slen = 0;
	vb.avb_data.avb_ostring.aos_string = NULL;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

struct subagentx_index *
subagentx_index_nstring_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen, size_t vlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}
	if (vlen == 0 || vlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: invalid string "
		    "length: %zu\n", __func__, vlen);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: invalid string "
		    "length: %zu\n", __func__, vlen);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_OCTETSTRING;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_ostring.aos_slen = vlen;
	vb.avb_data.avb_ostring.aos_string = NULL;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

struct subagentx_index *
subagentx_index_oid_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_OID;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_oid.aoi_idlen = 0;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

struct subagentx_index *
subagentx_index_noid_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen, size_t vlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}
	if (vlen == 0 || vlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: invalid string "
		    "length: %zu\n", __func__, vlen);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: invalid string "
		    "length: %zu\n", __func__, vlen);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_OID;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_oid.aoi_idlen = oidlen;
	vb.avb_data.avb_oid.aoi_idlen = vlen;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

struct subagentx_index *
subagentx_index_ipaddress_dynamic(struct subagentx_region *sar, uint32_t oid[],
    size_t oidlen)
{
	struct agentx_varbind vb;
	size_t i;

	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	vb.avb_type = AGENTX_DATA_TYPE_IPADDRESS;
	for (i = 0; i < oidlen; i++)
		vb.avb_oid.aoi_id[i] = oid[i];
	vb.avb_data.avb_ostring.aos_string = NULL;
	vb.avb_oid.aoi_idlen = oidlen;

	return subagentx_index(sar, &vb, SAI_TYPE_DYNAMIC);
}

static struct subagentx_index *
subagentx_index(struct subagentx_region *sar, struct agentx_varbind *vb,
    enum subagentx_index_type type)
{
	struct subagentx_index *sai;

	if (sar->sar_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: use after free",
		    __func__);
	if (agentx_oid_cmp(&(sar->sar_oid), &(vb->avb_oid)) != -2) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oid is not child "
		    "of region %s", __func__,
		    agentx_oid2string(&(vb->avb_oid)));
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oid is not child of "
		    "region %s", __func__, agentx_oid2string(&(vb->avb_oid)));
		errno = EINVAL;
		return NULL;
#endif
	}

	if ((sai = calloc(1, sizeof(*sai))) == NULL)
		return NULL;

	sai->sai_sar = sar;
	sai->sai_type = type;
	bcopy(vb, &(sai->sai_vb), sizeof(*vb));
	sai->sai_cstate = SA_CSTATE_CLOSE;
	sai->sai_dstate = SA_DSTATE_OPEN;
	TAILQ_INSERT_HEAD(&(sar->sar_indices), sai, sai_sar_indices);

	if (sar->sar_cstate == SA_CSTATE_OPEN)
		subagentx_index_start(sai);

	return sai;
}

static int
subagentx_index_start(struct subagentx_index *sai)
{
	struct subagentx_region *sar = sai->sai_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;
	int flags = 0;

#ifdef AGENTX_DEBUG
	if (sar->sar_cstate != SA_CSTATE_OPEN ||
	    sai->sai_cstate != SA_CSTATE_CLOSE ||
	    sai->sai_dstate != SA_DSTATE_OPEN)
		subagentx_log_sac_fatalx(sac, "%s: unexpected index allocation",
		    __func__);
#endif

	sai->sai_cstate = SA_CSTATE_WAITOPEN;

	if (sai->sai_type == SAI_TYPE_NEW)
		flags = AGENTX_PDU_FLAG_NEW_INDEX;
	else if (sai->sai_type == SAI_TYPE_ANY)
		flags = AGENTX_PDU_FLAG_ANY_INDEX;
	else if (sai->sai_type == SAI_TYPE_DYNAMIC) {
		subagentx_index_finalize(NULL, sai);
		return 0;
	}

	/* We might be able to bundle, but if we fail we'd have to reorganise */
	packetid = agentx_indexallocate(sa->sa_ax, flags, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), &(sai->sai_vb), 1);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_INDEXDEALLOCATE));
		subagentx_reset(sa);
		return -1;
	}
	if (sai->sai_type == SAI_TYPE_VALUE)
		subagentx_log_sac_info(sac, "index %s: allocating '%u'",
		    agentx_oid2string(&(sai->sai_vb.avb_oid)),
		    sai->sai_vb.avb_data.avb_uint32);
	else if (sai->sai_type == SAI_TYPE_ANY)
		subagentx_log_sac_info(sac, "index %s: allocating any index",
		    agentx_oid2string(&(sai->sai_vb.avb_oid)));
	else if (sai->sai_type == SAI_TYPE_NEW)
		subagentx_log_sac_info(sac, "index %s: allocating new index",
		    agentx_oid2string(&(sai->sai_vb.avb_oid)));

	return subagentx_request(sa, packetid, subagentx_index_finalize, sai);
}

static int
subagentx_index_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_index *sai = cookie;
	struct subagentx_region *sar = sai->sai_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct agentx_pdu_response *resp;
	size_t i;

#ifdef AGENTX_DEBUG
	if (sai->sai_cstate != SA_CSTATE_WAITOPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: not expecting index allocate", __func__);
#endif
	if (sai->sai_type == SAI_TYPE_DYNAMIC) {
		sai->sai_cstate = SA_CSTATE_OPEN;
		return 0;
	}

	resp = &(pdu->ap_payload.ap_response);
	if (resp->ap_error != AGENTX_PDU_ERROR_NOERROR) {
		sai->sai_cstate = SA_CSTATE_CLOSE;
		subagentx_log_sac_warnx(sac, "index %s: %s",
		    agentx_oid2string(&(sar->sar_oid)),
		    agentx_error2string(resp->ap_error));
		return 0;
	}
	sai->sai_cstate = SA_CSTATE_OPEN;
	if (resp->ap_nvarbind != 1) {
		subagentx_log_sac_warnx(sac, "index %s: unexpected number of "
		    "indices", agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}
	if (resp->ap_varbindlist[0].avb_type != sai->sai_vb.avb_type) {
		subagentx_log_sac_warnx(sac, "index %s: unexpected index type",
		    agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}
	if (agentx_oid_cmp(&(resp->ap_varbindlist[0].avb_oid),
	    &(sai->sai_vb.avb_oid)) != 0) {
		subagentx_log_sac_warnx(sac, "index %s: unexpected oid",
		    agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}

	switch (sai->sai_vb.avb_type) {
	case AGENTX_DATA_TYPE_INTEGER:
		if (sai->sai_type == SAI_TYPE_NEW ||
		    sai->sai_type == SAI_TYPE_ANY)
			sai->sai_vb.avb_data.avb_uint32 =
			    resp->ap_varbindlist[0].avb_data.avb_uint32;
		else if (sai->sai_vb.avb_data.avb_uint32 !=
		    resp->ap_varbindlist[0].avb_data.avb_uint32) {
			subagentx_log_sac_warnx(sac, "index %s: unexpected "
			    "index value", agentx_oid2string(&(sar->sar_oid)));
			subagentx_reset(sa);
			return -1;
		}
		subagentx_log_sac_info(sac, "index %s: allocated '%u'",
		    agentx_oid2string(&(sai->sai_vb.avb_oid)),
		    sai->sai_vb.avb_data.avb_uint32);
		break;
	default:
		subagentx_log_sac_fatalx(sac, "%s: Unsupported index type",
		    __func__);
	}

	if (sai->sai_dstate == SA_DSTATE_CLOSE)
		return subagentx_index_close(sai);

	/* TODO Make use of range_subid register */
	for (i = 0; i < sai->sai_objectlen; i++) {
		if (sai->sai_object[i]->sao_dstate == SA_DSTATE_OPEN) {
			if (subagentx_object_start(sai->sai_object[i]) == -1) 
				return -1;
		}
	}
	return 0;
}

void
subagentx_index_free(struct subagentx_index *sai)
{
	size_t i;
	struct subagentx_object *sao;

	if (sai == NULL)
		return;

	if (sai->sai_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sai->sai_sar->sar_sac,
		    "%s: double free", __func__);

	/* TODO Do a range_subid unregister before freeing */
	for (i = 0; i < sai->sai_objectlen; i++) {
		sao = sai->sai_object[i];
		if (sao->sao_dstate != SA_DSTATE_CLOSE) {
			subagentx_object_free(sao);
			if (sai->sai_object[i] != sao)
				i--;
		}
	}

	sai->sai_dstate = SA_DSTATE_CLOSE;

	if (sai->sai_cstate == SA_CSTATE_OPEN)
		(void) subagentx_index_close(sai);
	else if (sai->sai_cstate == SA_CSTATE_CLOSE)
		subagentx_index_free_finalize(sai);
}

static void
subagentx_index_free_finalize(struct subagentx_index *sai)
{
	struct subagentx_region *sar = sai->sai_sar;

#ifdef AGENTX_DEBUG
	if (sai->sai_dstate != SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: unexpected free",
		    __func__);
	if (sai->sai_cstate != SA_CSTATE_CLOSE)
		subagentx_log_sac_fatalx(sar->sar_sac,
		    "%s: free without deallocating", __func__);
#endif

	if (sai->sai_objectlen != 0)
		return;

	TAILQ_REMOVE(&(sar->sar_indices), sai, sai_sar_indices);
	agentx_varbind_free(&(sai->sai_vb));
	free(sai->sai_object);
	free(sai);
	if (sar->sar_dstate == SA_DSTATE_CLOSE)
		subagentx_region_free_finalize(sar);
}

static void
subagentx_index_reset(struct subagentx_index *sai)
{
	sai->sai_cstate = SA_CSTATE_CLOSE;

	if (sai->sai_dstate == SA_DSTATE_CLOSE)
		subagentx_index_free_finalize(sai);
}

static int
subagentx_index_close(struct subagentx_index *sai)
{
	struct subagentx_region *sar = sai->sai_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	uint32_t packetid;

#ifdef AGENTX_DEBUG
	if (sai->sai_cstate != SA_CSTATE_OPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: unexpected index deallocation", __func__);
#endif

	sai->sai_cstate = SA_CSTATE_WAITCLOSE;
	if (sas->sas_cstate == SA_CSTATE_WAITCLOSE)
		return 0;

	/* We might be able to bundle, but if we fail we'd have to reorganise */
	packetid = agentx_indexdeallocate(sa->sa_ax, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), &(sai->sai_vb), 1);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_INDEXDEALLOCATE));
		subagentx_reset(sa);
		return -1;
	}
	subagentx_log_sac_info(sac, "index %s: deallocating",
	    agentx_oid2string(&(sai->sai_vb.avb_oid)));
	return subagentx_request(sa, packetid, subagentx_index_close_finalize,
	    sai);
}

static int
subagentx_index_close_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_index *sai = cookie;
	struct subagentx_region *sar = sai->sai_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct agentx_pdu_response *resp = &(pdu->ap_payload.ap_response);

#ifdef AGENTX_DEBUG
	if (sai->sai_cstate != SA_CSTATE_WAITCLOSE)
		subagentx_log_sac_fatalx(sac, "%s: unexpected indexdeallocate",
		    __func__);
#endif

	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		subagentx_log_sac_warnx(sac,
		    "index %s: couldn't deallocate: %s",
		    agentx_oid2string(&(sai->sai_vb.avb_oid)),
		    agentx_error2string(resp->ap_error));
		subagentx_reset(sa);
		return -1;
	}

	if (resp->ap_nvarbind != 1) {
		subagentx_log_sac_warnx(sac,
		    "index %s: unexpected number of indices",
		    agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}
	if (resp->ap_varbindlist[0].avb_type != sai->sai_vb.avb_type) {
		subagentx_log_sac_warnx(sac, "index %s: unexpected index type",
		    agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}
	if (agentx_oid_cmp(&(resp->ap_varbindlist[0].avb_oid),
	    &(sai->sai_vb.avb_oid)) != 0) {
		subagentx_log_sac_warnx(sac, "index %s: unexpected oid",
		    agentx_oid2string(&(sar->sar_oid)));
		subagentx_reset(sa);
		return -1;
	}
	switch (sai->sai_vb.avb_type) {
	case AGENTX_DATA_TYPE_INTEGER:
		if (sai->sai_vb.avb_data.avb_uint32 !=
		    resp->ap_varbindlist[0].avb_data.avb_uint32) {
			subagentx_log_sac_warnx(sac,
			    "index %s: unexpected index value",
			    agentx_oid2string(&(sar->sar_oid)));
			subagentx_reset(sa);
			return -1;
		}
		break;
	default:
		subagentx_log_sac_fatalx(sac, "%s: Unsupported index type",
		    __func__);
	}

	sai->sai_cstate = SA_CSTATE_CLOSE;

	subagentx_log_sac_info(sac, "index %s: deallocated",
	    agentx_oid2string(&(sai->sai_vb.avb_oid)));

	if (sai->sai_dstate == SA_DSTATE_CLOSE) {
		subagentx_index_free_finalize(sai);
	} else if (sar->sar_cstate == SA_CSTATE_OPEN) {
		if (subagentx_index_start(sai) == -1)
			return -1;
	}
	return 0;
}

struct subagentx_object *
subagentx_object(struct subagentx_region *sar, uint32_t oid[], size_t oidlen,
    struct subagentx_index *sai[], size_t sailen, int implied,
    void (*get)(struct subagentx_varbind *))
{
	struct subagentx_object *sao, **tsao, sao_search;
	struct subagentx_index *lsai;
	int ready = 1;
	size_t i, j;

	if (sar->sar_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: use after free",
		    __func__);
	if (oidlen < 1) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen == 0",
		    __func__);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen == 0",
		    __func__);
		errno = EINVAL;
		return NULL;
#endif
	}
	if (oidlen > SUBAGENTX_OID_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: oidlen > %d",
		    __func__, SUBAGENTX_OID_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}
	if (sailen > SUBAGENTX_OID_INDEX_MAX_LEN) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: indexlen > %d",
		    __func__, SUBAGENTX_OID_INDEX_MAX_LEN);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: indexlen > %d",
		    __func__, SUBAGENTX_OID_INDEX_MAX_LEN);
		errno = EINVAL;
		return NULL;
#endif
	}

	for (i = 0; i < oidlen; i++)
		sao_search.sao_oid.aoi_id[i] = oid[i];
	sao_search.sao_oid.aoi_idlen = oidlen;

	do {
		if (RB_FIND(sac_objects, &(sar->sar_sac->sac_objects),
		    &sao_search) != NULL) {
#ifdef AGENTX_DEBUG
			subagentx_log_sac_fatalx(sar->sar_sac, "%s: invalid "
			    "parent child object relationship", __func__);
#else
			subagentx_log_sac_warnx(sar->sar_sac, "%s: invalid "
			    "parent child object relationship", __func__);
			errno = EINVAL;
			return NULL;
#endif
		}
		sao_search.sao_oid.aoi_idlen--;
	} while (sao_search.sao_oid.aoi_idlen > 0);
	sao_search.sao_oid.aoi_idlen = oidlen;
	sao = RB_NFIND(sac_objects, &(sar->sar_sac->sac_objects), &sao_search);
	if (sao != NULL &&
	    agentx_oid_cmp(&(sao->sao_oid), &(sao_search.sao_oid)) == 2) {
#ifdef AGENTX_DEBUG
		subagentx_log_sac_fatalx(sar->sar_sac, "%s: invalid parent "
		    "child object relationship", __func__);
#else
		subagentx_log_sac_warnx(sar->sar_sac, "%s: invalid parent "
		    "child object relationship", __func__);
		errno = EINVAL;
		return NULL;
#endif
	}
	if (implied == 1) {
		lsai = sai[sailen - 1];
		if (lsai->sai_vb.avb_type == AGENTX_DATA_TYPE_OCTETSTRING) {
			if (lsai->sai_vb.avb_data.avb_ostring.aos_slen != 0) {
#ifdef AGENTX_DEBUG
				subagentx_log_sac_fatalx(sar->sar_sac,
				    "%s: implied can only be used on strings "
				    "of dynamic length", __func__);
#else
				subagentx_log_sac_warnx(sar->sar_sac,
				    "%s: implied can only be used on strings "
				    "of dynamic length", __func__);
				errno = EINVAL;
				return NULL;
#endif
			}
		} else if (lsai->sai_vb.avb_type == AGENTX_DATA_TYPE_OID) {
			if (lsai->sai_vb.avb_data.avb_oid.aoi_idlen != 0) {
#ifdef AGENTX_DEBUG
				subagentx_log_sac_fatalx(sar->sar_sac,
				    "%s: implied can only be used on oids of "
				    "dynamic length", __func__);
#else
				subagentx_log_sac_warnx(sar->sar_sac,
				    "%s: implied can only be used on oids of "
				    "dynamic length", __func__);
				errno = EINVAL;
				return NULL;
#endif
			}
		} else {
#ifdef AGENTX_DEBUG
			subagentx_log_sac_fatalx(sar->sar_sac, "%s: implied "
			    "can only be set on oid and string indices",
			    __func__);
#else
			subagentx_log_sac_warnx(sar->sar_sac, "%s: implied can "
			    "only be set on oid and string indices", __func__);
			errno = EINVAL;
			return NULL;
#endif
		}
	}

	ready = sar->sar_cstate == SA_CSTATE_OPEN;
	if ((sao = calloc(1, sizeof(*sao))) == NULL)
		return NULL;
	sao->sao_sar = sar;
	bcopy(&(sao_search.sao_oid), &(sao->sao_oid), sizeof(sao->sao_oid));
	for (i = 0; i < sailen; i++) {
		sao->sao_index[i] = sai[i];
		if (sai[i]->sai_objectlen == sai[i]->sai_objectsize) {
			tsao = recallocarray(sai[i]->sai_object,
			    sai[i]->sai_objectlen, sai[i]->sai_objectlen + 1,
			    sizeof(*sai[i]->sai_object));
			if (tsao == NULL) {
				free(sao);
				return NULL;
			}
			sai[i]->sai_object = tsao;
			sai[i]->sai_objectsize = sai[i]->sai_objectlen + 1;
		}
		for (j = 0; j < sai[i]->sai_objectlen; j++) {
			if (agentx_oid_cmp(&(sao->sao_oid),
			    &(sai[i]->sai_object[j]->sao_oid)) < 0) {
				memmove(&(sai[i]->sai_object[j + 1]),
				    &(sai[i]->sai_object[j]),
				    sizeof(*(sai[i]->sai_object)) *
				    (sai[i]->sai_objectlen - j));
				break;
			}
		}
		sai[i]->sai_object[j] = sao;
		sai[i]->sai_objectlen++;
		if (sai[i]->sai_cstate != SA_CSTATE_OPEN)
			ready = 0;
	}
	sao->sao_indexlen = sailen;
	sao->sao_implied = implied;
	sao->sao_timeout = 0;
	sao->sao_lock = 0;
	sao->sao_get = get;
	sao->sao_cstate = SA_CSTATE_CLOSE;
	sao->sao_dstate = SA_DSTATE_OPEN;

	TAILQ_INSERT_TAIL(&(sar->sar_objects), sao, sao_sar_objects);
	RB_INSERT(sac_objects, &(sar->sar_sac->sac_objects), sao);

	if (ready)
		subagentx_object_start(sao);

	return sao;
}

static int
subagentx_object_start(struct subagentx_object *sao)
{
	struct subagentx_region *sar = sao->sao_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct agentx_oid oid;
	char oids[1024];
	size_t i;
	int needregister = 0;
	uint32_t packetid;
	uint8_t flags = AGENTX_PDU_FLAG_INSTANCE_REGISTRATION;

#ifdef AGENTX_DEBUG
	if (sar->sar_cstate != SA_CSTATE_OPEN ||
	    sao->sao_cstate != SA_CSTATE_CLOSE ||
	    sao->sao_dstate != SA_DSTATE_OPEN)
		subagentx_log_sac_fatalx(sac,
		    "%s: unexpected object registration", __func__);
#endif

	if (sao->sao_timeout != 0)
		needregister = 1;
	for (i = 0; i < sao->sao_indexlen; i++) {
		if (sao->sao_index[i]->sai_cstate != SA_CSTATE_OPEN)
			return 0;
		if (sao->sao_index[i]->sai_type != SAI_TYPE_DYNAMIC)
			needregister = 1;
	}
	if (!needregister) {
		sao->sao_cstate = SA_CSTATE_WAITOPEN;
		subagentx_object_finalize(NULL, sao);
		return 0;
	}

	bcopy(&(sao->sao_oid), &(oid), sizeof(oid));
	for (i = 0; i < sao->sao_indexlen; i++) {
		if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC) {
			flags = 0;
			break;
		}
#ifdef AGENTX_DEBUG
		if (sao->sao_index[i]->sai_vb.avb_type !=
		    AGENTX_DATA_TYPE_INTEGER)
			subagentx_log_sac_fatalx(sac,
			    "%s: Unsupported allocated index type", __func__);
#endif
		oid.aoi_id[oid.aoi_idlen++] =
		    sao->sao_index[i]->sai_vb.avb_data.avb_uint32;
	}
	packetid = agentx_register(sa->sa_ax, flags, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), sao->sao_timeout,
	    AGENTX_PRIORITY_DEFAULT, 0, &oid, 0);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_REGISTER));
		subagentx_reset(sa);
		return -1;
	}
	strlcpy(oids, agentx_oid2string(&(sao->sao_oid)), sizeof(oids));
	subagentx_log_sac_info(sac, "object %s (%s %s): opening",
	    oids, flags ? "instance" : "region", agentx_oid2string(&(oid)));
	sao->sao_cstate = SA_CSTATE_WAITOPEN;
	return subagentx_request(sa, packetid, subagentx_object_finalize, sao);
}

static int
subagentx_object_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_object *sao = cookie;
	struct subagentx_context *sac = sao->sao_sar->sar_sac;
	struct agentx_oid oid;
	char oids[1024];
	size_t i;
	uint8_t flags = 1;

#ifdef AGENTX_DEBUG
	if (sao->sao_cstate != SA_CSTATE_WAITOPEN)
		subagentx_log_sac_fatalx(sac, "%s: not expecting object open",
		    __func__);
#endif

	if (pdu == NULL) {
		sao->sao_cstate = SA_CSTATE_OPEN;
		return 0;
	}

	bcopy(&(sao->sao_oid), &oid, sizeof(oid));
	for (i = 0; i < sao->sao_indexlen; i++) {
		if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC) {
			flags = 0;
			break;
		}
#ifdef AGENTX_DEBUG
		if (sao->sao_index[i]->sai_vb.avb_type !=
		    AGENTX_DATA_TYPE_INTEGER)
			subagentx_log_sac_fatalx(sac,
			    "%s: Unsupported allocated index type", __func__);
#endif

		oid.aoi_id[oid.aoi_idlen++] =
		    sao->sao_index[i]->sai_vb.avb_data.avb_uint32;
	}
	strlcpy(oids, agentx_oid2string(&(sao->sao_oid)), sizeof(oids));

	/*
	 * We should only be here for table objects with registered indices.
	 * If we fail here something is misconfigured and the admin should fix
	 * it.
	 */
	if (pdu->ap_payload.ap_response.ap_error != AGENTX_PDU_ERROR_NOERROR) {
		sao->sao_cstate = SA_CSTATE_CLOSE;
		subagentx_log_sac_info(sac, "object %s (%s %s): %s",
		    oids, flags ? "instance" : "region", agentx_oid2string(&oid),
		    agentx_error2string(pdu->ap_payload.ap_response.ap_error));
		if (sao->sao_dstate == SA_DSTATE_CLOSE)
			return subagentx_object_close_finalize(NULL, sao);
		return 0;
	}
	sao->sao_cstate = SA_CSTATE_OPEN;
	subagentx_log_sac_info(sac, "object %s (%s %s): open", oids,
	    flags ? "instance" : "region", agentx_oid2string(&oid));

	if (sao->sao_dstate == SA_DSTATE_CLOSE)
		return subagentx_object_close(sao);

	return 0;
}

static int
subagentx_object_lock(struct subagentx_object *sao)
{
	if (sao->sao_lock == UINT32_MAX) {
		subagentx_log_sac_warnx(sao->sao_sar->sar_sac,
		    "%s: sao_lock == %u", __func__, UINT32_MAX);
		return -1;
	}
	sao->sao_lock++;
	return 0;
}

static void
subagentx_object_unlock(struct subagentx_object *sao)
{
#ifdef AGENTX_DEBUG
	if (sao->sao_lock == 0)
		subagentx_log_sac_fatalx(sao->sao_sar->sar_sac,
		    "%s: sao_lock == 0", __func__);
#endif
	sao->sao_lock--;
	if (sao->sao_lock == 0 && sao->sao_dstate == SA_DSTATE_CLOSE &&
	    sao->sao_cstate == SA_CSTATE_CLOSE)
		subagentx_object_free_finalize(sao);
}

static int
subagentx_object_close(struct subagentx_object *sao)
{
	struct subagentx_context *sac = sao->sao_sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct agentx_oid oid;
	char oids[1024];
	size_t i;
	int needclose = 0;
	uint32_t packetid;
	uint8_t flags = 1;

#ifdef AGENTX_DEBUG
	if (sao->sao_cstate != SA_CSTATE_OPEN)
		subagentx_log_sac_fatalx(sac, "%s: unexpected object close",
		    __func__);
#endif

	for (i = 0; i < sao->sao_indexlen; i++) {
#ifdef AGENTX_DEBUG
		if (sao->sao_index[i]->sai_cstate != SA_CSTATE_OPEN)
			subagentx_log_sac_fatalx(sac,
			    "%s: Object open while index closed", __func__);
#endif
		if (sao->sao_index[i]->sai_type != SAI_TYPE_DYNAMIC)
			needclose = 1;
	}
	sao->sao_cstate = SA_CSTATE_WAITCLOSE;
	if (sas->sas_cstate == SA_CSTATE_WAITCLOSE)
		return 0;
	if (!needclose) {
		subagentx_object_close_finalize(NULL, sao);
		return 0;
	}

	bcopy(&(sao->sao_oid), &(oid), sizeof(oid));
	for (i = 0; i < sao->sao_indexlen; i++) {
		if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC) {
			flags = 0;
			break;
		}
#ifdef AGENTX_DEBUG
		if (sao->sao_index[i]->sai_vb.avb_type !=
		    AGENTX_DATA_TYPE_INTEGER)
			subagentx_log_sac_fatalx(sac,
			    "%s: Unsupported allocated index type", __func__);
#endif
		oid.aoi_id[oid.aoi_idlen++] =
		    sao->sao_index[i]->sai_vb.avb_data.avb_uint32;
	}
	packetid = agentx_unregister(sa->sa_ax, sas->sas_id,
	    SUBAGENTX_CONTEXT_CTX(sac), AGENTX_PRIORITY_DEFAULT, 0, &oid, 0);
	if (packetid == 0) {
		subagentx_log_sac_warn(sac, "couldn't generate %s",
		    agentx_pdutype2string(AGENTX_PDU_TYPE_UNREGISTER));
		subagentx_reset(sa);
		return -1;
	}
	strlcpy(oids, agentx_oid2string(&(sao->sao_oid)), sizeof(oids));
	subagentx_log_sac_info(sac, "object %s (%s %s): closing",
	    oids, flags ? "instance" : "region", agentx_oid2string(&(oid)));
	return subagentx_request(sa, packetid, subagentx_object_close_finalize,
	    sao);
}

static int
subagentx_object_close_finalize(struct agentx_pdu *pdu, void *cookie)
{
	struct subagentx_object *sao = cookie;
	struct subagentx_region *sar = sao->sao_sar;
	struct subagentx_context *sac = sar->sar_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct agentx_oid oid;
	char oids[1024];
	uint8_t flags = 1;
	size_t i;

#ifdef AGENTX_DEBUG
	if (sao->sao_cstate != SA_CSTATE_WAITCLOSE)
		subagentx_log_sac_fatalx(sac,
		    "%s: unexpected object unregister", __func__);
#endif

	if (pdu != NULL) {
		bcopy(&(sao->sao_oid), &(oid), sizeof(oid));
		for (i = 0; i < sao->sao_indexlen; i++) {
			if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC) {
				flags = 0;
				break;
			}
#ifdef AGENTX_DEBUG
			if (sao->sao_index[i]->sai_vb.avb_type !=
			    AGENTX_DATA_TYPE_INTEGER)
				subagentx_log_sac_fatalx(sac,
				    "%s: Unsupported allocated index type",
				    __func__);
#endif
			oid.aoi_id[oid.aoi_idlen++] =
			    sao->sao_index[i]->sai_vb.avb_data.avb_uint32;
		}
		strlcpy(oids, agentx_oid2string(&(sao->sao_oid)), sizeof(oids));
		if (pdu->ap_payload.ap_response.ap_error !=
		    AGENTX_PDU_ERROR_NOERROR) {
			subagentx_log_sac_warnx(sac,
			    "closing object %s (%s %s): %s", oids,
			    flags ? "instance" : "region",
			    agentx_oid2string(&oid), agentx_error2string(
			    pdu->ap_payload.ap_response.ap_error));
			subagentx_reset(sa);
			return -1;
		}
		subagentx_log_sac_info(sac, "object %s (%s %s): closed", oids,
		    flags ? "instance" : "region", agentx_oid2string(&oid));
	}

	if (sao->sao_dstate == SA_DSTATE_CLOSE)
		subagentx_object_free_finalize(sao);
	else {
		if (sar->sar_cstate == SA_CSTATE_OPEN)
			if (subagentx_object_start(sao) == -1)
				return -1;
	}

	return 0;
}

void
subagentx_object_free(struct subagentx_object *sao)
{
	if (sao == NULL)
		return;

	if (sao->sao_dstate == SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sao->sao_sar->sar_sac,
		    "%s: double free", __func__);

	sao->sao_dstate = SA_DSTATE_CLOSE;

	if (sao->sao_cstate == SA_CSTATE_OPEN) {
		if (subagentx_object_close(sao) == -1)
			return;
	}
	if (sao->sao_cstate == SA_CSTATE_CLOSE)
		subagentx_object_free_finalize(sao);
}

static void
subagentx_object_free_finalize(struct subagentx_object *sao)
{
#ifdef AGENTX_DEBUG
	struct subagentx *sa = sao->sao_sar->sar_sac->sac_sas->sas_sa;
#endif
	size_t i, j;
	int found;

#ifdef AGENTX_DEBUG
	if (sao->sao_dstate != SA_DSTATE_CLOSE)
		subagentx_log_sac_fatalx(sao->sao_sar->sar_sac,
		    "%s: unexpected free", __func__);
#endif

	if (sao->sao_lock != 0) {
#ifdef AGENTX_DEBUG
		if (TAILQ_EMPTY(&(sa->sa_getreqs)))
			subagentx_log_sac_fatalx(sao->sao_sar->sar_sac,
			    "%s: %s sao_lock == %u", __func__,
			    agentx_oid2string(&(sao->sao_oid)), sao->sao_lock);
#endif
		return;
	}

	RB_REMOVE(sac_objects, &(sao->sao_sar->sar_sac->sac_objects), sao);
	TAILQ_REMOVE(&(sao->sao_sar->sar_objects), sao, sao_sar_objects);

	for (i = 0; i < sao->sao_indexlen; i++) {
		found = 0;
		for (j = 0; j < sao->sao_index[i]->sai_objectlen; j++) {
			if (sao->sao_index[i]->sai_object[j] == sao)
				found = 1;
			if (found && j + 1 != sao->sao_index[i]->sai_objectlen)
				sao->sao_index[i]->sai_object[j] =
				    sao->sao_index[i]->sai_object[j + 1];
		}
#ifdef AGENTX_DEBUG
		if (!found)
			subagentx_log_sac_fatalx(sao->sao_sar->sar_sac,
			    "%s: object not found in index", __func__);
#endif
		sao->sao_index[i]->sai_objectlen--;
		if (sao->sao_index[i]->sai_dstate == SA_DSTATE_CLOSE &&
		    sao->sao_index[i]->sai_cstate == SA_CSTATE_CLOSE)
			subagentx_index_free_finalize(sao->sao_index[i]);
	}

	free(sao);
}

static void
subagentx_object_reset(struct subagentx_object *sao)
{
	sao->sao_cstate = SA_CSTATE_CLOSE;

	if (sao->sao_dstate == SA_DSTATE_CLOSE)
		subagentx_object_free_finalize(sao);
}

static int
subagentx_object_cmp(struct subagentx_object *o1, struct subagentx_object *o2)
{
	return agentx_oid_cmp(&(o1->sao_oid), &(o2->sao_oid));
}

static int
subagentx_object_implied(struct subagentx_object *sao,
    struct subagentx_index *sai)
{
	size_t i = 0;

	for (i = 0; i < sao->sao_indexlen; i++) {
		if (sao->sao_index[i] == sai) {
			if (sai->sai_vb.avb_data.avb_ostring.aos_slen != 0)
				return 1;
			else if (i == sao->sao_indexlen - 1)
				return sao->sao_implied;
			return 0;
		}
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sac_fatalx(sao->sao_sar->sar_sac, "%s: unsupported index",
	    __func__);
#endif
	return 0;
}

static void
subagentx_get_start(struct subagentx_context *sac, struct agentx_pdu *pdu)
{
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	struct subagentx_get *sag, tsag;
	struct agentx_pdu_searchrangelist *srl;
	char *logmsg = NULL;
	size_t i, j;
	int fail = 0;

	if ((sag = calloc(1, sizeof(*sag))) == NULL) {
		tsag.sag_sessionid = pdu->ap_header.aph_sessionid;
		tsag.sag_transactionid = pdu->ap_header.aph_transactionid;
		tsag.sag_packetid = pdu->ap_header.aph_packetid;
		tsag.sag_context_default = sac->sac_name_default;
		tsag.sag_fd = sac->sac_sas->sas_sa->sa_fd;
		subagentx_log_sag_warn(&tsag, "Couldn't parse request");
		subagentx_reset(sa);
		return;
	}

	sag->sag_sessionid = pdu->ap_header.aph_sessionid;
	sag->sag_transactionid = pdu->ap_header.aph_transactionid;
	sag->sag_packetid = pdu->ap_header.aph_packetid;
	sag->sag_context_default = sac->sac_name_default;
	sag->sag_fd = sac->sac_sas->sas_sa->sa_fd;
	if (!sac->sac_name_default) {
		sag->sag_context.aos_string =
		    (unsigned char *)strdup((char *)sac->sac_name.aos_string);
		if (sag->sag_context.aos_string == NULL) {
			subagentx_log_sag_warn(sag, "Couldn't parse request");
			free(sag);
			subagentx_reset(sa);
			return;
		}
	}
	sag->sag_context.aos_slen = sac->sac_name.aos_slen;
	sag->sag_type = pdu->ap_header.aph_type;
	sag->sag_sac = sac;
	TAILQ_INSERT_TAIL(&(sa->sa_getreqs), sag, sag_sa_getreqs);
	if (sag->sag_type == AGENTX_PDU_TYPE_GET ||
	    sag->sag_type == AGENTX_PDU_TYPE_GETNEXT) {
		srl = &(pdu->ap_payload.ap_srl);
		sag->sag_nvarbind = srl->ap_nsr;
	} else {
		sag->sag_nonrep = pdu->ap_payload.ap_getbulk.ap_nonrep;
		sag->sag_maxrep = pdu->ap_payload.ap_getbulk.ap_maxrep;
		srl = &(pdu->ap_payload.ap_getbulk.ap_srl);
		sag->sag_nvarbind = ((srl->ap_nsr - sag->sag_nonrep) *
		    sag->sag_maxrep) + sag->sag_nonrep;
	}

	if ((sag->sag_varbind = calloc(sag->sag_nvarbind,
	    sizeof(*(sag->sag_varbind)))) == NULL) {
		subagentx_log_sag_warn(sag, "Couldn't parse request");
		subagentx_get_free(sag);
		subagentx_reset(sa);
		return;
	}

	/* XXX net-snmp doesn't use getbulk, so untested */
	/* Two loops: varbind after needs to be initialized */
	for (i = 0; i < srl->ap_nsr; i++) {
		if (i < sag->sag_nonrep ||
		    sag->sag_type != AGENTX_PDU_TYPE_GETBULK)
			j = i;
		else if (sag->sag_maxrep == 0)
			break;
		else
			j = (sag->sag_maxrep * i) + sag->sag_nonrep;
		bcopy(&(srl->ap_sr[i].asr_start),
		    &(sag->sag_varbind[j].sav_vb.avb_oid),
		    sizeof(srl->ap_sr[i].asr_start));
		bcopy(&(srl->ap_sr[i].asr_start),
		    &(sag->sag_varbind[j].sav_start),
		    sizeof(srl->ap_sr[i].asr_start));
		bcopy(&(srl->ap_sr[i].asr_stop),
		    &(sag->sag_varbind[j].sav_end),
		    sizeof(srl->ap_sr[i].asr_stop));
		sag->sag_varbind[j].sav_initialized = 1;
		sag->sag_varbind[j].sav_sag = sag;
		sag->sag_varbind[j].sav_include =
		    srl->ap_sr[i].asr_start.aoi_include;
		if (j == 0)
			fail |= subagentx_strcat(&logmsg, " {");
		else
			fail |= subagentx_strcat(&logmsg, ",{");
		fail |= subagentx_strcat(&logmsg,
		    agentx_oid2string(&(srl->ap_sr[i].asr_start)));
		if (srl->ap_sr[i].asr_start.aoi_include)
			fail |= subagentx_strcat(&logmsg, " (inclusive)");
		if (srl->ap_sr[i].asr_stop.aoi_idlen != 0) {
			fail |= subagentx_strcat(&logmsg, " - ");
			fail |= subagentx_strcat(&logmsg, 
			    agentx_oid2string(&(srl->ap_sr[i].asr_stop)));
		}
		fail |= subagentx_strcat(&logmsg, "}");
		if (fail) {
			subagentx_log_sag_warn(sag, "Couldn't parse request");
			free(logmsg);
			subagentx_get_free(sag);
			subagentx_reset(sa);
			return;
		}
	}

	subagentx_log_sag_debug(sag, "%s:%s",
	    agentx_pdutype2string(sag->sag_type), logmsg);
	free(logmsg);

	for (i = 0; i < srl->ap_nsr; i++) {
		if (i < sag->sag_nonrep ||
		    sag->sag_type != AGENTX_PDU_TYPE_GETBULK)
			j = i;
		else if (sag->sag_maxrep == 0)
			break;
		else
			j = (sag->sag_maxrep * i) + sag->sag_nonrep;
		subagentx_varbind_start(&(sag->sag_varbind[j]));
	}
}

static void
subagentx_get_finalize(struct subagentx_get *sag)
{
	struct subagentx_context *sac = sag->sag_sac;
	struct subagentx_session *sas = sac->sac_sas;
	struct subagentx *sa = sas->sas_sa;
	size_t i, j, nvarbind = 0;
	uint16_t error = 0, index = 0;
	struct agentx_varbind *vbl;
	char *logmsg = NULL;
	int fail = 0;

	for (i = 0; i < sag->sag_nvarbind; i++) {
		if (sag->sag_varbind[i].sav_initialized) {
			if (sag->sag_varbind[i].sav_vb.avb_type == 0)
				return;
			nvarbind++;
		}
	}

	if (sag->sag_sac == NULL) {
		subagentx_get_free(sag);
		return;
	}

	if ((vbl = calloc(nvarbind, sizeof(*vbl))) == NULL) {
		subagentx_log_sag_warn(sag, "Couldn't parse request");
		subagentx_get_free(sag);
		subagentx_reset(sa);
		return;
	}
	for (i = 0, j = 0; i < sag->sag_nvarbind; i++) {
		if (sag->sag_varbind[i].sav_initialized) {
			memcpy(&(vbl[j]), &(sag->sag_varbind[i].sav_vb),
			    sizeof(*vbl));
			if (error == 0 && sag->sag_varbind[i].sav_error !=
			    AGENTX_PDU_ERROR_NOERROR) {
				error = sag->sag_varbind[i].sav_error;
				index = j + 1;
			}
			if (j == 0)
				fail |= subagentx_strcat(&logmsg, " {");
			else
				fail |= subagentx_strcat(&logmsg, ",{");
			fail |= subagentx_strcat(&logmsg,
			    agentx_varbind2string(&(vbl[j])));
			if (sag->sag_varbind[i].sav_error !=
			    AGENTX_PDU_ERROR_NOERROR) {
				fail |= subagentx_strcat(&logmsg, "(");
				fail |= subagentx_strcat(&logmsg,
				    agentx_error2string(
				    sag->sag_varbind[i].sav_error));
				fail |= subagentx_strcat(&logmsg, ")");
			}
			fail |= subagentx_strcat(&logmsg, "}");
			if (fail) {
				subagentx_log_sag_warn(sag,
				    "Couldn't parse request");
				free(logmsg);
				subagentx_get_free(sag);
				return;
			}
			j++;
		}
	}
	subagentx_log_sag_debug(sag, "response:%s", logmsg);
	free(logmsg);

	if (agentx_response(sa->sa_ax, sas->sas_id, sag->sag_transactionid,
	    sag->sag_packetid, SUBAGENTX_CONTEXT_CTX(sac), 0, error, index,
	    vbl, nvarbind) == -1) {
		subagentx_log_sag_warn(sag, "Couldn't parse request");
		subagentx_reset(sa);
	} else
		subagentx_wantwrite(sa, sa->sa_fd);
	free(vbl);
	subagentx_get_free(sag);
}

void
subagentx_get_free(struct subagentx_get *sag)
{
	struct subagentx_varbind *sav;
	struct subagentx_object *sao;
	struct subagentx *sa = sag->sag_sac->sac_sas->sas_sa;
	struct subagentx_varbind_index *index;
	size_t i, j;

	if (sag->sag_sac != NULL)
		TAILQ_REMOVE(&(sa->sa_getreqs), sag, sag_sa_getreqs);

	for (i = 0; i < sag->sag_nvarbind; i++) {
		sav = &(sag->sag_varbind[i]);
		for (j = 0; sav->sav_sao != NULL &&
		    j < sav->sav_sao->sao_indexlen; j++) {
			sao = sav->sav_sao;
			index = &(sav->sav_index[j]);
			if (sao->sao_index[j]->sai_vb.avb_type == 
			    AGENTX_DATA_TYPE_OCTETSTRING ||
			    sao->sao_index[j]->sai_vb.avb_type ==
			    AGENTX_DATA_TYPE_IPADDRESS)
				free(index->sav_idata.avb_ostring.aos_string);
		}
		agentx_varbind_free(&(sag->sag_varbind[i].sav_vb));
	}

	free(sag->sag_context.aos_string);
	free(sag->sag_varbind);
	free(sag);
}

static void
subagentx_varbind_start(struct subagentx_varbind *sav)
{
	struct subagentx_get *sag = sav->sav_sag;
	struct subagentx_context *sac = sag->sag_sac;
	struct subagentx_object *sao, sao_search;
	struct subagentx_varbind_index *index;
	struct subagentx_index *sai;
	struct agentx_oid *oid;
	union agentx_data *data;
	struct in_addr *ipaddress;
	unsigned char *ipbytes;
	size_t i, j, k;
	int overflow = 0, dynamic;

#ifdef AGENTX_DEBUG
	if (!sav->sav_initialized)
		subagentx_log_sag_fatalx(sav->sav_sag,
		    "%s: sav_initialized not set", __func__);
#endif

	bcopy(&(sav->sav_vb.avb_oid), &(sao_search.sao_oid),
	    sizeof(sao_search.sao_oid));

	do {
		sao = RB_FIND(sac_objects, &(sac->sac_objects), &sao_search);
		if (sao_search.sao_oid.aoi_idlen > 0)
			sao_search.sao_oid.aoi_idlen--;
	} while (sao == NULL && sao_search.sao_oid.aoi_idlen > 0);
	if (sao == NULL || sao->sao_cstate != SA_CSTATE_OPEN) {
		sav->sav_include = 1;
		if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET) {
			subagentx_varbind_nosuchobject(sav);
			return;
		}
		bcopy(&(sav->sav_vb.avb_oid), &(sao_search.sao_oid),
		    sizeof(sao_search.sao_oid));
		sao = RB_NFIND(sac_objects, &(sac->sac_objects), &sao_search);
getnext:
		while (sao != NULL && sao->sao_cstate != SA_CSTATE_OPEN)
			sao = RB_NEXT(sac_objects, &(sac->sac_objects), sao);
		if (sao == NULL) {
			subagentx_varbind_endofmibview(sav);
			return;
		}
		bcopy(&(sao->sao_oid), &(sav->sav_vb.avb_oid),
		    sizeof(sao->sao_oid));
	}
	sav->sav_sao = sao;
	sav->sav_indexlen = sao->sao_indexlen;
	if (subagentx_object_lock(sao) == -1) {
		subagentx_varbind_error_type(sav,
		    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
		return;
	}

	oid = &(sav->sav_vb.avb_oid);
	if (sao->sao_indexlen == 0) {
		if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET) {
			if (oid->aoi_idlen != sao->sao_oid.aoi_idlen + 1 ||
			    oid->aoi_id[oid->aoi_idlen - 1] != 0) {
				subagentx_varbind_nosuchinstance(sav);
				return;
			}
		} else {
			if (oid->aoi_idlen == sao->sao_oid.aoi_idlen) {
				oid->aoi_id[oid->aoi_idlen++] = 0;
				sav->sav_include = 1;
			} else {
				sav->sav_sao = NULL;
				subagentx_object_unlock(sao);
				sao = RB_NEXT(sac_objects, &(sac->sac_objects),
				    sao);
				goto getnext;
			}
		}
	}
	j = sao->sao_oid.aoi_idlen;
/*
 * We can't trust what the client gives us, so sometimes we need to map it to
 * index type.
 * - AGENTX_PDU_TYPE_GET: we always return AGENTX_DATA_TYPE_NOSUCHINSTANCE
 * - AGENTX_PDU_TYPE_GETNEXT:
 *   - Missing OID digits to match indices will result in the indices to be NUL-
 *     initialized and the request type will be set to
 *     SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE
 *   - An overflow can happen on AGENTX_DATA_TYPE_OCTETSTRING and
 *     AGENTX_DATA_TYPE_IPADDRESS. This results in request type being set to
 *     SUBAGENTX_REQUEST_TYPE_GETNEXT and will set the index to its maximum
 *     value:
 *     - AGENTX_DATA_TYPE_INTEGER: UINT32_MAX
 *     - AGENTX_DATA_TYPE_OCTETSTRING: aos_slen = UINT32_MAX and
 *       aos_string = NULL
 *     - AGENTX_DATA_TYPE_OID: aoi_idlen = UINT32_MAX and aoi_id[x] = UINT32_MAX
 *     - AGENTX_DATA_TYPE_IPADDRESS: 255.255.255.255
 */
	for (dynamic = 0, i = 0; i < sao->sao_indexlen; i++) {
		index = &(sav->sav_index[i]);
		index->sav_sai = sao->sao_index[i];
		data = &(index->sav_idata);
		if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC)
			dynamic = 1;
		if (j >= sav->sav_vb.avb_oid.aoi_idlen && !overflow &&
		    sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC)
			continue;
		switch (sao->sao_index[i]->sai_vb.avb_type) {
		case AGENTX_DATA_TYPE_INTEGER:
/* Dynamic index: normal copy paste */
			if (sao->sao_index[i]->sai_type == SAI_TYPE_DYNAMIC) {
				data->avb_uint32 = overflow ?
				    UINT32_MAX : sav->sav_vb.avb_oid.aoi_id[j];
				j++;
				index->sav_idatacomplete = 1;
				break;
			}
			sai = sao->sao_index[i];
/* With a GET-request we need an exact match */
			if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET) {
				if (sai->sai_vb.avb_data.avb_uint32 !=
				    sav->sav_vb.avb_oid.aoi_id[j]) {
					subagentx_varbind_nosuchinstance(sav);
					return;
				}
				index->sav_idatacomplete = 1;
				j++;
				break;
			}
/* A higher value automatically moves us to the next value */
			if (overflow ||
			    sav->sav_vb.avb_oid.aoi_id[j] >
			    sai->sai_vb.avb_data.avb_uint32) {
/* If we're !dynamic up until now the rest of the oid doesn't matter */
				if (!dynamic) {
					subagentx_varbind_endofmibview(sav);
					return;
				}
/*
 * Else we just pick the max value and make sure we don't return
 * SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE
 */
				data->avb_uint32 = UINT32_MAX;
				index->sav_idatacomplete = 1;
				overflow = 1;
				j++;
				break;
/*
 * A lower value automatically moves to the set value and counts as a short oid
 */
			} else if (sav->sav_vb.avb_oid.aoi_id[j] <
			    sai->sai_vb.avb_data.avb_uint32) {
				data->avb_uint32 =
				    sai->sai_vb.avb_data.avb_uint32;
				j = sav->sav_vb.avb_oid.aoi_idlen;
				break;
			}
/* Normal match, except we already matched overflow at higher value */
			data->avb_uint32 = sav->sav_vb.avb_oid.aoi_id[j];
			j++;
			index->sav_idatacomplete = 1;
			break;
		case AGENTX_DATA_TYPE_OCTETSTRING:
			if (!subagentx_object_implied(sao, index->sav_sai)) {
				if (overflow || sav->sav_vb.avb_oid.aoi_id[j] > 
				    SUBAGENTX_OID_MAX_LEN -
				    sav->sav_vb.avb_oid.aoi_idlen) {
					overflow = 1;
					data->avb_ostring.aos_slen = UINT32_MAX;
					index->sav_idatacomplete = 1;
					continue;
				}
				data->avb_ostring.aos_slen =
				    sav->sav_vb.avb_oid.aoi_id[j++];
			} else {
				if (overflow) {
					data->avb_ostring.aos_slen = UINT32_MAX;
					index->sav_idatacomplete = 1;
					continue;
				}
				data->avb_ostring.aos_slen =
				    sav->sav_vb.avb_oid.aoi_idlen - j;
			}
			data->avb_ostring.aos_string =
			    calloc(data->avb_ostring.aos_slen + 1, 1);
			if (data->avb_ostring.aos_string == NULL) {
				subagentx_log_sag_warn(sag,
				    "Failed to bind string index");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
				return;
			}
			for (k = 0; k < data->avb_ostring.aos_slen; k++, j++) {
				if (!overflow &&
				    j == sav->sav_vb.avb_oid.aoi_idlen)
					break;

				if (sav->sav_vb.avb_oid.aoi_id[j] > 255)
					overflow = 1;

				data->avb_ostring.aos_string[k] = overflow ?
				    0xff : sav->sav_vb.avb_oid.aoi_id[j];
			}
			if (k == data->avb_ostring.aos_slen)
				index->sav_idatacomplete = 1;
			break;
		case AGENTX_DATA_TYPE_OID:
			if (!subagentx_object_implied(sao, index->sav_sai)) {
				if (overflow || sav->sav_vb.avb_oid.aoi_id[j] > 
				    SUBAGENTX_OID_MAX_LEN -
				    sav->sav_vb.avb_oid.aoi_idlen) {
					overflow = 1;
					data->avb_oid.aoi_idlen = UINT32_MAX;
					index->sav_idatacomplete = 1;
					continue;
				}
				data->avb_oid.aoi_idlen =
				    sav->sav_vb.avb_oid.aoi_id[j++];
			} else {
				if (overflow) {
					data->avb_oid.aoi_idlen = UINT32_MAX;
					index->sav_idatacomplete = 1;
					continue;
				}
				data->avb_oid.aoi_idlen =
				    sav->sav_vb.avb_oid.aoi_idlen - j;
			}
			for (k = 0; k < data->avb_oid.aoi_idlen; k++, j++) {
				if (!overflow &&
				    j == sav->sav_vb.avb_oid.aoi_idlen) {
					data->avb_oid.aoi_id[k] = 0;
					continue;
				}
				data->avb_oid.aoi_id[k] = overflow ?
				    UINT32_MAX : sav->sav_vb.avb_oid.aoi_id[j];
			}
			if (j <= sav->sav_vb.avb_oid.aoi_idlen)
				index->sav_idatacomplete = 1;
			break;
		case AGENTX_DATA_TYPE_IPADDRESS:
			ipaddress = calloc(1, sizeof(*ipaddress));
			if (ipaddress == NULL) {
				subagentx_log_sag_warn(sag,
				    "Failed to bind ipaddress index");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
				return;
			}
			ipbytes = (unsigned char *)ipaddress;
			for (k = 0; k < 4; k++, j++) {
				if (!overflow &&
				    j == sav->sav_vb.avb_oid.aoi_idlen)
					break;

				if (sav->sav_vb.avb_oid.aoi_id[j] > 255)
					overflow = 1;

				ipbytes[k] = overflow ? 255 :
				    sav->sav_vb.avb_oid.aoi_id[j];
			}
			if (j <= sav->sav_vb.avb_oid.aoi_idlen)
				index->sav_idatacomplete = 1;
			data->avb_ostring.aos_slen = sizeof(*ipaddress);
			data->avb_ostring.aos_string =
			    (unsigned char *)ipaddress;
			break;
		default:
#ifdef AGENTX_DEBUG
			subagentx_log_sag_fatalx(sag,
			    "%s: unexpected index type", __func__);
#else
			subagentx_log_sag_warnx(sag,
			    "%s: unexpected index type", __func__);
			subagentx_varbind_error_type(sav,
			    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
			return;
#endif
		}
	}
	if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET) {
		if ((sao->sao_indexlen > 0 &&
		    !sav->sav_index[sao->sao_indexlen - 1].sav_idatacomplete) ||
		    j != sav->sav_vb.avb_oid.aoi_idlen || overflow) {
			subagentx_varbind_nosuchinstance(sav);
			return;
		}
	}

	if (overflow || j > sav->sav_vb.avb_oid.aoi_idlen)
		sav->sav_include = 0;

/*
 * SUBAGENTX_REQUEST_TYPE_GETNEXT request can !dynamic objects can just move to
 * the next object
 */
	if (subagentx_varbind_request(sav) == SUBAGENTX_REQUEST_TYPE_GETNEXT &&
	    !dynamic) {
		subagentx_varbind_endofmibview(sav);
		return;
	}

	sao->sao_get(sav);
}

void
subagentx_varbind_integer(struct subagentx_varbind *sav, uint32_t value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_INTEGER;
	sav->sav_vb.avb_data.avb_uint32 = value;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_string(struct subagentx_varbind *sav, const char *value)
{
	subagentx_varbind_nstring(sav, (const unsigned char *)value,
	    strlen(value));
}

void
subagentx_varbind_nstring(struct subagentx_varbind *sav,
    const unsigned char *value, size_t slen)
{
	sav->sav_vb.avb_data.avb_ostring.aos_string = malloc(slen);
	if (sav->sav_vb.avb_data.avb_ostring.aos_string == NULL) {
		subagentx_log_sag_warn(sav->sav_sag, "Couldn't bind string");
		subagentx_varbind_error_type(sav,
		    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
		return;
	}
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_OCTETSTRING;
	memcpy(sav->sav_vb.avb_data.avb_ostring.aos_string, value, slen);
	sav->sav_vb.avb_data.avb_ostring.aos_slen = slen;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_printf(struct subagentx_varbind *sav, const char *fmt, ...)
{
	va_list ap;
	int r;

	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_OCTETSTRING;
	va_start(ap, fmt);
	r = vasprintf((char **)&(sav->sav_vb.avb_data.avb_ostring.aos_string),
	    fmt, ap);
	va_end(ap);
	if (r == -1) {
		sav->sav_vb.avb_data.avb_ostring.aos_string = NULL;
		subagentx_log_sag_warn(sav->sav_sag, "Couldn't bind string");
		subagentx_varbind_error_type(sav,
		    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
		return;
	}
	sav->sav_vb.avb_data.avb_ostring.aos_slen = r;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_null(struct subagentx_varbind *sav)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_NULL;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_oid(struct subagentx_varbind *sav, const uint32_t oid[],
    size_t oidlen)
{
	size_t i;

	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_OID;

	for (i = 0; i < oidlen; i++)
		sav->sav_vb.avb_data.avb_oid.aoi_id[i] = oid[i];
	sav->sav_vb.avb_data.avb_oid.aoi_idlen = oidlen;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_object(struct subagentx_varbind *sav,
    struct subagentx_object *sao)
{
	subagentx_varbind_oid(sav, sao->sao_oid.aoi_id,
	    sao->sao_oid.aoi_idlen);
}

void
subagentx_varbind_index(struct subagentx_varbind *sav,
    struct subagentx_index *sai)
{
	subagentx_varbind_oid(sav, sai->sai_vb.avb_oid.aoi_id,
	    sai->sai_vb.avb_oid.aoi_idlen);
}


void
subagentx_varbind_ipaddress(struct subagentx_varbind *sav,
    const struct in_addr *value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_IPADDRESS;
	sav->sav_vb.avb_data.avb_ostring.aos_string = malloc(4);
	if (sav->sav_vb.avb_data.avb_ostring.aos_string == NULL) {
		subagentx_log_sag_warn(sav->sav_sag, "Couldn't bind ipaddress");
		subagentx_varbind_error_type(sav,
		    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
		return;
	}
	memcpy(sav->sav_vb.avb_data.avb_ostring.aos_string, value, 4);
	sav->sav_vb.avb_data.avb_ostring.aos_slen = 4;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_counter32(struct subagentx_varbind *sav, uint32_t value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_COUNTER32;
	sav->sav_vb.avb_data.avb_uint32 = value;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_gauge32(struct subagentx_varbind *sav, uint32_t value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_GAUGE32;
	sav->sav_vb.avb_data.avb_uint32 = value;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_timeticks(struct subagentx_varbind *sav, uint32_t value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_TIMETICKS;
	sav->sav_vb.avb_data.avb_uint32 = value;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_opaque(struct subagentx_varbind *sav, const char *string,
    size_t strlen)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_OPAQUE;
	sav->sav_vb.avb_data.avb_ostring.aos_string = malloc(strlen);
	if (sav->sav_vb.avb_data.avb_ostring.aos_string == NULL) {
		subagentx_log_sag_warn(sav->sav_sag, "Couldn't bind opaque");
		subagentx_varbind_error_type(sav,
		    AGENTX_PDU_ERROR_PROCESSINGERROR, 1);
		return;
	}
	memcpy(sav->sav_vb.avb_data.avb_ostring.aos_string, string, strlen);
	sav->sav_vb.avb_data.avb_ostring.aos_slen = strlen;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_counter64(struct subagentx_varbind *sav, uint64_t value)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_COUNTER64;
	sav->sav_vb.avb_data.avb_uint64 = value;

	subagentx_varbind_finalize(sav);
}

void
subagentx_varbind_notfound(struct subagentx_varbind *sav)
{
	if (sav->sav_indexlen == 0) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "%s invalid call",
		    __func__);
#else
		subagentx_log_sag_warnx(sav->sav_sag, "%s invalid call",
		    __func__);
		subagentx_varbind_error_type(sav, 
		    AGENTX_PDU_ERROR_GENERR, 1);
#endif
	} else if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET)
		subagentx_varbind_nosuchinstance(sav);
	else
		subagentx_varbind_endofmibview(sav);
}

void
subagentx_varbind_error(struct subagentx_varbind *sav)
{
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 1);
}

static void
subagentx_varbind_error_type(struct subagentx_varbind *sav,
    enum agentx_pdu_error error, int done)
{
	if (sav->sav_error == AGENTX_PDU_ERROR_NOERROR) {
		sav->sav_error = error;
	}

	if (done) {
		sav->sav_vb.avb_type = AGENTX_DATA_TYPE_NULL;

		subagentx_varbind_finalize(sav);
	}
}

static void
subagentx_varbind_finalize(struct subagentx_varbind *sav)
{
	struct subagentx_get *sag = sav->sav_sag;
	struct agentx_oid oid;
	union agentx_data *data;
	size_t i, j;
	int cmp;

	if (sav->sav_error != AGENTX_PDU_ERROR_NOERROR) {
		bcopy(&(sav->sav_start), &(sav->sav_vb.avb_oid),
		    sizeof(sav->sav_start));
		goto done;
	}
	bcopy(&(sav->sav_sao->sao_oid), &oid, sizeof(oid));
	if (sav->sav_indexlen == 0)
		agentx_oid_add(&oid, 0);
	for (i = 0; i < sav->sav_indexlen; i++) {
		data = &(sav->sav_index[i].sav_idata);
		switch (sav->sav_index[i].sav_sai->sai_vb.avb_type) {
		case AGENTX_DATA_TYPE_INTEGER:
			if (agentx_oid_add(&oid, data->avb_uint32) == -1)
				goto fail;
			break;
		case AGENTX_DATA_TYPE_OCTETSTRING:
			if (!subagentx_object_implied(sav->sav_sao,
			    sav->sav_index[i].sav_sai)) {
				if (agentx_oid_add(&oid,
				    data->avb_ostring.aos_slen) == -1)
					goto fail;
			}
			for (j = 0; j < data->avb_ostring.aos_slen; j++) {
				if (agentx_oid_add(&oid,
				    (uint8_t)data->avb_ostring.aos_string[j]) ==
				    -1)
					goto fail;
			}
			break;
		case AGENTX_DATA_TYPE_OID:
			if (!subagentx_object_implied(sav->sav_sao,
			    sav->sav_index[i].sav_sai)) {
				if (agentx_oid_add(&oid,
				    data->avb_oid.aoi_idlen) == -1)
					goto fail;
			}
			for (j = 0; j < data->avb_oid.aoi_idlen; j++) {
				if (agentx_oid_add(&oid,
				    data->avb_oid.aoi_id[j]) == -1)
					goto fail;
			}
			break;
		case AGENTX_DATA_TYPE_IPADDRESS:
			for (j = 0; j < 4; j++) {
				if (agentx_oid_add(&oid,
				    data->avb_ostring.aos_string == NULL ? 0 :
				    (uint8_t)data->avb_ostring.aos_string[j]) ==
				    -1)
					goto fail;
			}
			break;
		default:
#ifdef AGENTX_DEBUG
			subagentx_log_sag_fatalx(sag,
			    "%s: unsupported index type", __func__);
#else
			bcopy(&(sav->sav_start), &(sav->sav_vb.avb_oid),
			    sizeof(sav->sav_start));
			sav->sav_error = AGENTX_PDU_ERROR_PROCESSINGERROR;
			subagentx_object_unlock(sav->sav_sao);
			subagentx_get_finalize(sav->sav_sag);
			return;
#endif
		}
	}
	cmp = agentx_oid_cmp(&(sav->sav_vb.avb_oid), &oid);
	if ((subagentx_varbind_request(sav) == SUBAGENTX_REQUEST_TYPE_GETNEXT &&
	    cmp >= 0) || cmp > 0) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sag, "indices not incremented");
#else
		subagentx_log_sag_warnx(sag, "indices not incremented");
		bcopy(&(sav->sav_start), &(sav->sav_vb.avb_oid),
		    sizeof(sav->sav_start));
		sav->sav_error = AGENTX_PDU_ERROR_GENERR;
#endif
	} else
		bcopy(&oid, &(sav->sav_vb.avb_oid), sizeof(oid));
done:
	subagentx_object_unlock(sav->sav_sao);
	subagentx_get_finalize(sav->sav_sag);
	return;

fail:
	subagentx_log_sag_warnx(sag, "oid too large");
	bcopy(&(sav->sav_start), &(sav->sav_vb.avb_oid),
	    sizeof(sav->sav_start));
	sav->sav_error = AGENTX_PDU_ERROR_GENERR;
	subagentx_object_unlock(sav->sav_sao);
	subagentx_get_finalize(sav->sav_sag);
	
}

static void
subagentx_varbind_nosuchobject(struct subagentx_varbind *sav)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_NOSUCHOBJECT;

	if (sav->sav_sao != NULL)
		subagentx_object_unlock(sav->sav_sao);
	subagentx_get_finalize(sav->sav_sag);
}

static void
subagentx_varbind_nosuchinstance(struct subagentx_varbind *sav)
{
	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_NOSUCHINSTANCE;

	if (sav->sav_sao != NULL)
		subagentx_object_unlock(sav->sav_sao);
	subagentx_get_finalize(sav->sav_sag);
}

static void
subagentx_varbind_endofmibview(struct subagentx_varbind *sav)
{
	struct subagentx_object *sao;
	struct agentx_varbind *vb;
	struct subagentx_varbind_index *index;
	size_t i;

#ifdef AGENTX_DEBUG
	if (sav->sav_sag->sag_type != AGENTX_PDU_TYPE_GETNEXT &&
	    sav->sav_sag->sag_type != AGENTX_PDU_TYPE_GETBULK)
		subagentx_log_sag_fatalx(sav->sav_sag,
		    "%s: invalid request type", __func__);
#endif

	if (sav->sav_sao != NULL &&
	    (sao = RB_NEXT(sac_objects, &(sac->sac_objects),
	    sav->sav_sao)) != NULL &&
	    agentx_oid_cmp(&(sao->sao_oid), &(sav->sav_end)) < 0) {
		bcopy(&(sao->sao_oid), &(sav->sav_vb.avb_oid),
		    sizeof(sao->sao_oid));
		sav->sav_include = 1;
		for (i = 0; i < sav->sav_indexlen; i++) {
			index = &(sav->sav_index[i]);
			vb = &(index->sav_sai->sai_vb);
			if (vb->avb_type == AGENTX_DATA_TYPE_OCTETSTRING ||
			    vb->avb_type == AGENTX_DATA_TYPE_IPADDRESS)
				free(index->sav_idata.avb_ostring.aos_string);
		}
		bzero(&(sav->sav_index), sizeof(sav->sav_index));
		subagentx_object_unlock(sav->sav_sao);
		subagentx_varbind_start(sav);
		return;
	}

	sav->sav_vb.avb_type = AGENTX_DATA_TYPE_ENDOFMIBVIEW;

	if (sav->sav_sao != NULL)
		subagentx_object_unlock(sav->sav_sao);
	subagentx_get_finalize(sav->sav_sag);
}

enum subagentx_request_type
subagentx_varbind_request(struct subagentx_varbind *sav)
{
	if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET)
		return SUBAGENTX_REQUEST_TYPE_GET;
	if (sav->sav_include ||
	    (sav->sav_indexlen > 0 &&
	    !sav->sav_index[sav->sav_indexlen - 1].sav_idatacomplete))
		return SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE;
	return SUBAGENTX_REQUEST_TYPE_GETNEXT;
}

struct subagentx_object *
subagentx_varbind_get_object(struct subagentx_varbind *sav)
{
	return sav->sav_sao;
}

uint32_t
subagentx_varbind_get_index_integer(struct subagentx_varbind *sav,
    struct subagentx_index *sai)
{
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_INTEGER) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return 0;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai)
			return sav->sav_index[i].sav_idata.avb_uint32;
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
	return 0;
#endif
}

const unsigned char *
subagentx_varbind_get_index_string(struct subagentx_varbind *sav,
    struct subagentx_index *sai, size_t *slen, int *implied)
{
	struct subagentx_varbind_index *index;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_OCTETSTRING) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		*slen = 0;
		*implied = 0;
		return NULL;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			index = &(sav->sav_index[i]);
			*slen = index->sav_idata.avb_ostring.aos_slen;
			*implied = subagentx_object_implied(sav->sav_sao, sai);
			return index->sav_idata.avb_ostring.aos_string;
		}
	}

#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
	*slen = 0;
	*implied = 0;
	return NULL;
#endif
}

const uint32_t *
subagentx_varbind_get_index_oid(struct subagentx_varbind *sav,
    struct subagentx_index *sai, size_t *oidlen, int *implied)
{
	struct subagentx_varbind_index *index;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_OID) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		*oidlen = 0;
		*implied = 0;
		return NULL;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			index = &(sav->sav_index[i]);
			*oidlen = index->sav_idata.avb_oid.aoi_idlen;
			*implied = subagentx_object_implied(sav->sav_sao, sai);
			return index->sav_idata.avb_oid.aoi_id;
		}
	}

#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
	*oidlen = 0;
	*implied = 0;
	return NULL;
#endif
}

const struct in_addr *
subagentx_varbind_get_index_ipaddress(struct subagentx_varbind *sav,
    struct subagentx_index *sai)
{
	static struct in_addr nuladdr = {0};
	struct subagentx_varbind_index *index;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_IPADDRESS) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return NULL;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			index = &(sav->sav_index[i]);
			if (index->sav_idata.avb_ostring.aos_string == NULL)
				return &nuladdr;
			return (struct in_addr *)
			    index->sav_idata.avb_ostring.aos_string;
		}
	}

#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
	return NULL;
#endif
}

void
subagentx_varbind_set_index_integer(struct subagentx_varbind *sav,
    struct subagentx_index *sai, uint32_t value)
{
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_INTEGER) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET &&
			    sav->sav_index[i].sav_idata.avb_uint32 != value) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "can't change index on GET");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "can't change index on GET");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			sav->sav_index[i].sav_idata.avb_uint32 = value;
			return;
		}
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
#endif
}

void
subagentx_varbind_set_index_string(struct subagentx_varbind *sav,
    struct subagentx_index *sai, const char *value)
{
	subagentx_varbind_set_index_nstring(sav, sai,
	    (const unsigned char *)value, strlen(value));
}

void
subagentx_varbind_set_index_nstring(struct subagentx_varbind *sav,
    struct subagentx_index *sai, const unsigned char *value, size_t slen)
{
	struct agentx_ostring *curvalue;
	unsigned char *nstring;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_OCTETSTRING) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			if (sai->sai_vb.avb_data.avb_ostring.aos_slen != 0 &&
			    sai->sai_vb.avb_data.avb_ostring.aos_slen != slen) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "invalid string length on explicit length "
				    "string");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "invalid string length on explicit length "
				    "string");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			curvalue = &(sav->sav_index[i].sav_idata.avb_ostring);
			if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET &&
			    (curvalue->aos_slen != slen ||
			    memcmp(curvalue->aos_string, value, slen) != 0)) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "can't change index on GET");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "can't change index on GET");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			if ((nstring = recallocarray(curvalue->aos_string,
			    curvalue->aos_slen + 1, slen + 1, 1)) == NULL) {
				subagentx_log_sag_warn(sav->sav_sag,
				    "Failed to bind string index");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_PROCESSINGERROR, 0);
				return;
			}
			curvalue->aos_string = nstring;
			memcpy(nstring, value, slen);
			curvalue->aos_slen = slen;
			return;
		}
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
#endif
}

void
subagentx_varbind_set_index_oid(struct subagentx_varbind *sav,
    struct subagentx_index *sai, const uint32_t *value, size_t oidlen)
{
	struct agentx_oid *curvalue, oid;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_OID) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			if (sai->sai_vb.avb_data.avb_oid.aoi_idlen != 0 &&
			    sai->sai_vb.avb_data.avb_oid.aoi_idlen != oidlen) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "invalid oid length on explicit length "
				    "oid");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "invalid oid length on explicit length "
				    "oid");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			curvalue = &(sav->sav_index[i].sav_idata.avb_oid);
			for (i = 0; i < oidlen; i++)
				oid.aoi_id[i] = value[i];
			oid.aoi_idlen = oidlen;
			if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET &&
			    agentx_oid_cmp(&oid, curvalue) != 0) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "can't change index on GET");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "can't change index on GET");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			for (i = 0; i < oidlen; i++)
				curvalue->aoi_id[i] = value[i];
			curvalue->aoi_idlen = oidlen;
			return;
		}
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
#endif
}

void
subagentx_varbind_set_index_object(struct subagentx_varbind *sav,
    struct subagentx_index *sai, struct subagentx_object *sao)
{
	subagentx_varbind_set_index_oid(sav, sai, sao->sao_oid.aoi_id,
	    sao->sao_oid.aoi_idlen);
}

void
subagentx_varbind_set_index_ipaddress(struct subagentx_varbind *sav,
    struct subagentx_index *sai, const struct in_addr *addr)
{
	struct agentx_ostring *curvalue;
	size_t i;

	if (sai->sai_vb.avb_type != AGENTX_DATA_TYPE_IPADDRESS) {
#ifdef AGENTX_DEBUG
		subagentx_log_sag_fatalx(sav->sav_sag, "invalid index type");
#else
		subagentx_log_sag_warnx(sav->sav_sag, "invalid index type");
		subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
		return;
#endif
	}

	for (i = 0; i < sav->sav_indexlen; i++) {
		if (sav->sav_index[i].sav_sai == sai) {
			curvalue = &(sav->sav_index[i].sav_idata.avb_ostring);
			if (curvalue->aos_string == NULL)
				curvalue->aos_string = calloc(1, sizeof(*addr));
			if (curvalue->aos_string == NULL) {
				subagentx_log_sag_warn(sav->sav_sag,
				    "Failed to bind ipaddress index");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_PROCESSINGERROR, 0);
				return;
			}
			if (sav->sav_sag->sag_type == AGENTX_PDU_TYPE_GET &&
			    memcmp(addr, curvalue->aos_string,
			    sizeof(*addr)) != 0) {
#ifdef AGENTX_DEBUG
				subagentx_log_sag_fatalx(sav->sav_sag,
				    "can't change index on GET");
#else
				subagentx_log_sag_warnx(sav->sav_sag,
				    "can't change index on GET");
				subagentx_varbind_error_type(sav,
				    AGENTX_PDU_ERROR_GENERR, 0);
				return;
#endif
			}
			bcopy(addr, curvalue->aos_string, sizeof(*addr));
			return;
		}
	}
#ifdef AGENTX_DEBUG
	subagentx_log_sag_fatalx(sav->sav_sag, "invalid index");
#else
	subagentx_log_sag_warnx(sav->sav_sag, "invalid index");
	subagentx_varbind_error_type(sav, AGENTX_PDU_ERROR_GENERR, 0);
#endif
}

static int
subagentx_request(struct subagentx *sa, uint32_t packetid,
    int (*cb)(struct agentx_pdu *, void *), void *cookie)
{
	struct subagentx_request *sar;

#ifdef AGENTX_DEBUG
	if (sa->sa_ax->ax_wblen == 0)
		subagentx_log_sa_fatalx(sa, "%s: no data to be written",
		    __func__);
#endif

	if ((sar = calloc(1, sizeof(*sar))) == NULL) {
		subagentx_log_sa_warn(sa, "couldn't create request context");
		subagentx_reset(sa);
		return -1;
	}

	sar->sar_packetid = packetid;
	sar->sar_cb = cb;
	sar->sar_cookie = cookie;
	if (RB_INSERT(sa_requests, &(sa->sa_requests), sar) != NULL) {
#ifdef AGENTX_DEBUG
		subagentx_log_sa_fatalx(sa, "%s: duplicate packetid", __func__);
#else
		subagentx_log_sa_warnx(sa, "%s: duplicate packetid", __func__);
		free(sar);
		subagentx_reset(sa);
		return -1;
#endif
	}

	subagentx_wantwrite(sa, sa->sa_fd);
	return 0;
}

static int
subagentx_request_cmp(struct subagentx_request *r1,
    struct subagentx_request *r2)
{
	return r1->sar_packetid < r2->sar_packetid ? -1 :
	    r1->sar_packetid > r2->sar_packetid;
}

static int
subagentx_strcat(char **dst, const char *src)
{
	char *tmp;
	size_t dstlen = 0, buflen = 0, srclen, nbuflen;

	if (*dst != NULL) {
		dstlen = strlen(*dst);
		buflen = ((dstlen / 512) + 1) * 512;
	}

	srclen = strlen(src);
	if (*dst == NULL || dstlen + srclen > buflen) {
		nbuflen = (((dstlen + srclen) / 512) + 1) * 512;
		tmp = recallocarray(*dst, buflen, nbuflen, sizeof(*tmp));
		if (tmp == NULL)
			return -1;
		*dst = tmp;
		buflen = nbuflen;
	}

	(void)strlcat(*dst, src, buflen);
	return 0;
}

void
subagentx_read(struct subagentx *sa)
{
	struct subagentx_session *sas;
	struct subagentx_context *sac;
	struct subagentx_request sar_search, *sar;
	struct agentx_pdu *pdu;
	int error;

	if ((pdu = agentx_recv(sa->sa_ax)) == NULL) {
		if (errno == EAGAIN)
			return;
		subagentx_log_sa_warn(sa, "lost connection");
		subagentx_reset(sa);
		return;
	}

	TAILQ_FOREACH(sas, &(sa->sa_sessions), sas_sa_sessions) {
		if (sas->sas_id == pdu->ap_header.aph_sessionid)
			break;
		if (sas->sas_cstate == SA_CSTATE_WAITOPEN &&
		    sas->sas_packetid == pdu->ap_header.aph_packetid)
			break;
	}
	if (sas == NULL) {
		subagentx_log_sa_warnx(sa, "received unexpected session: %d",
		    pdu->ap_header.aph_sessionid);
		agentx_pdu_free(pdu);
		subagentx_reset(sa);
		return;
	}
	TAILQ_FOREACH(sac, &(sas->sas_contexts), sac_sas_contexts) {
		if ((pdu->ap_header.aph_flags &
		    AGENTX_PDU_FLAG_NON_DEFAULT_CONTEXT) == 0 &&
		    sac->sac_name_default == 1)
			break;
		if (pdu->ap_header.aph_flags &
		    AGENTX_PDU_FLAG_NON_DEFAULT_CONTEXT &&
		    sac->sac_name_default == 0 &&
		    pdu->ap_context.aos_slen == sac->sac_name.aos_slen &&
		    memcmp(pdu->ap_context.aos_string,
		    sac->sac_name.aos_string, sac->sac_name.aos_slen) == 0)
			break;
	}
	if (pdu->ap_header.aph_type != AGENTX_PDU_TYPE_RESPONSE) {
		if (sac == NULL) {
			subagentx_log_sa_warnx(sa, "%s: invalid context",
			    pdu->ap_context.aos_string);
			agentx_pdu_free(pdu);
			subagentx_reset(sa);
			return;
		}
	}

	switch (pdu->ap_header.aph_type) {
	case AGENTX_PDU_TYPE_GET:
	case AGENTX_PDU_TYPE_GETNEXT:
	case AGENTX_PDU_TYPE_GETBULK:
		subagentx_get_start(sac, pdu);
		break;
	/* Add stubs for set functions */
	case AGENTX_PDU_TYPE_TESTSET:
	case AGENTX_PDU_TYPE_COMMITSET:
	case AGENTX_PDU_TYPE_UNDOSET:
		if (pdu->ap_header.aph_type == AGENTX_PDU_TYPE_TESTSET)
			error = AGENTX_PDU_ERROR_NOTWRITABLE;
		else if (pdu->ap_header.aph_type == AGENTX_PDU_TYPE_COMMITSET)
			error = AGENTX_PDU_ERROR_COMMITFAILED;
		else
			error = AGENTX_PDU_ERROR_UNDOFAILED;

		subagentx_log_sac_debug(sac, "unsupported call: %s",
		    agentx_pdutype2string(pdu->ap_header.aph_type));
		if (agentx_response(sa->sa_ax, sas->sas_id,
		    pdu->ap_header.aph_transactionid,
		    pdu->ap_header.aph_packetid,
		    sac == NULL ? NULL : SUBAGENTX_CONTEXT_CTX(sac),
		    0, error, 1, NULL, 0) == -1)
			subagentx_log_sac_warn(sac,
			    "transaction: %u packetid: %u: failed to send "
			    "reply", pdu->ap_header.aph_transactionid,
			    pdu->ap_header.aph_packetid);
		if (sa->sa_ax->ax_wblen > 0)
			subagentx_wantwrite(sa, sa->sa_fd);
		break;
	case AGENTX_PDU_TYPE_CLEANUPSET:
		subagentx_log_sa_debug(sa, "unsupported call: %s",
		    agentx_pdutype2string(pdu->ap_header.aph_type));
		break;
	case AGENTX_PDU_TYPE_RESPONSE:
		sar_search.sar_packetid = pdu->ap_header.aph_packetid;
		sar = RB_FIND(sa_requests, &(sa->sa_requests), &sar_search);
		if (sar == NULL) {
			if (sac == NULL)
				subagentx_log_sa_warnx(sa, "received "
				    "response on non-request");
			else
				subagentx_log_sac_warnx(sac, "received "
				    "response on non-request");
			break;
		}
		if (sac != NULL && pdu->ap_payload.ap_response.ap_error == 0) {
			sac->sac_sysuptime =
			    pdu->ap_payload.ap_response.ap_uptime;
			(void) clock_gettime(CLOCK_MONOTONIC,
			    &(sac->sac_sysuptimespec));
		}
		RB_REMOVE(sa_requests, &(sa->sa_requests), sar);
		(void) sar->sar_cb(pdu, sar->sar_cookie);
		free(sar);
		break;
	default:
		if (sac == NULL)
			subagentx_log_sa_warnx(sa, "unsupported call: %s",
			    agentx_pdutype2string(pdu->ap_header.aph_type));
		else
			subagentx_log_sac_warnx(sac, "unsupported call: %s",
			    agentx_pdutype2string(pdu->ap_header.aph_type));
		subagentx_reset(sa);
		break;
	}
	agentx_pdu_free(pdu);
}

void
subagentx_write(struct subagentx *sa)
{
	ssize_t send;

	if ((send = agentx_send(sa->sa_ax)) == -1) {
		if (errno == EAGAIN) {
			subagentx_wantwrite(sa, sa->sa_fd);
			return;
		}
		subagentx_log_sa_warn(sa, "lost connection");
		subagentx_reset(sa);
		return;
	}
	if (send > 0)
		subagentx_wantwrite(sa, sa->sa_fd);
}

RB_GENERATE_STATIC(sa_requests, subagentx_request, sar_sa_requests,
    subagentx_request_cmp)
RB_GENERATE_STATIC(sac_objects, subagentx_object, sao_sac_objects,
    subagentx_object_cmp)
