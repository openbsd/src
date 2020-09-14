/*
 * Copyright (c) 2020 Martijn van Duren <martijn@openbsd.org>
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
#include <sys/time.h>
#include <sys/tree.h>

#include "agentx.h"

enum subagentx_cstate {		/* Current state */
	SA_CSTATE_CLOSE,	/* Closed */
	SA_CSTATE_WAITOPEN,	/* Connection requested */
	SA_CSTATE_OPEN,		/* Open */
	SA_CSTATE_WAITCLOSE	/* Close requested */
};

enum subagentx_dstate {		/* Desired state */
	SA_DSTATE_OPEN,		/* Open */
	SA_DSTATE_CLOSE		/* Close/free */
};

struct subagentx {
	void (*sa_nofd)(struct subagentx *, void *, int);
	void *sa_cookie;
	int sa_fd;
	enum subagentx_cstate sa_cstate;
	enum subagentx_dstate sa_dstate;
	struct agentx *sa_ax;
	TAILQ_HEAD(, subagentx_session) sa_sessions;
	TAILQ_HEAD(, subagentx_get) sa_getreqs;
	RB_HEAD(sa_requests, subagentx_request) sa_requests;
};

struct subagentx_session {
	struct subagentx *sas_sa;
	uint32_t sas_id;
	uint32_t sas_timeout;
	struct agentx_oid sas_oid;
	struct agentx_ostring sas_descr;
	enum subagentx_cstate sas_cstate;
	enum subagentx_dstate sas_dstate;
	uint32_t sas_packetid;
	TAILQ_HEAD(, subagentx_context) sas_contexts;
	TAILQ_ENTRY(subagentx_session) sas_sa_sessions;
};

struct subagentx_context {
	struct subagentx_session *sac_sas;
	int sac_name_default;
	struct agentx_ostring sac_name;
	uint32_t sac_sysuptime;
	struct timespec sac_sysuptimespec;
	enum subagentx_cstate sac_cstate;
	enum subagentx_dstate sac_dstate;
	TAILQ_HEAD(, subagentx_agentcaps) sac_agentcaps;
	TAILQ_HEAD(, subagentx_region) sac_regions;
	RB_HEAD(sac_objects, subagentx_object) sac_objects;
	TAILQ_ENTRY(subagentx_context) sac_sas_contexts;
};

struct subagentx_get {
	struct subagentx_context *sag_sac;
	int sag_fd;				/* Only used for logging */
	uint32_t sag_sessionid;
	uint32_t sag_transactionid;
	uint32_t sag_packetid;
	int sag_context_default;
	struct agentx_ostring sag_context;
	enum agentx_pdu_type sag_type;
	uint16_t sag_nonrep;
	uint16_t sag_maxrep;
	size_t sag_nvarbind;
	struct subagentx_varbind *sag_varbind;
	TAILQ_ENTRY(subagentx_get) sag_sa_getreqs;
};

__dead void subagentx_log_sa_fatalx(struct subagentx *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sa_warn(struct subagentx *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sa_warnx(struct subagentx *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sa_info(struct subagentx *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sa_debug(struct subagentx *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
__dead void subagentx_log_sas_fatalx(struct subagentx_session *, const char *,
    ...) __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sas_warnx(struct subagentx_session *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sas_warn(struct subagentx_session *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sas_info(struct subagentx_session *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
__dead void subagentx_log_sac_fatalx(struct subagentx_context *, const char *,
    ...) __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sac_warnx(struct subagentx_context *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sac_warn(struct subagentx_context *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sac_info(struct subagentx_context *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sac_debug(struct subagentx_context *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
__dead void subagentx_log_sag_fatalx(struct subagentx_get *, const char *,
    ...) __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sag_warnx(struct subagentx_get *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sag_warn(struct subagentx_get *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_log_sag_debug(struct subagentx_get *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
