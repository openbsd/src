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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "subagentx_internal.h"

#define SUBAGENTX_CONTEXT_NAME(sac) (sac->sac_name_default ? "<default>" : \
    (char *)sac->sac_name.aos_string)
#define SUBAGENTX_GET_CTXNAME(sag) (sag->sag_context_default ? "<default>" : \
    (char *)sag->sag_context.aos_string)

enum subagentx_log_type {
	SUBAGENTX_LOG_TYPE_FATAL,
	SUBAGENTX_LOG_TYPE_WARN,
	SUBAGENTX_LOG_TYPE_INFO,
	SUBAGENTX_LOG_TYPE_DEBUG
};

void (*subagentx_log_fatal)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2))) = NULL;
void (*subagentx_log_warn)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2))) = NULL;
void (*subagentx_log_info)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2))) = NULL;
void (*subagentx_log_debug)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2))) = NULL;


static void
subagentx_log_do(enum subagentx_log_type, const char *, va_list, int,
    struct subagentx *, struct subagentx_session *, struct subagentx_context *,
    struct subagentx_get *);

void
subagentx_log_sa_fatalx(struct subagentx *sa, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_FATAL, fmt, ap, 0, sa, NULL, NULL,
	    NULL);
	va_end(ap);
	abort();
}

void
subagentx_log_sa_warn(struct subagentx *sa, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 1, sa, NULL, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sa_warnx(struct subagentx *sa, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 0, sa, NULL, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sa_info(struct subagentx *sa, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_INFO, fmt, ap, 0, sa, NULL, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sa_debug(struct subagentx *sa, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_DEBUG, fmt, ap, 0, sa, NULL, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sas_fatalx(struct subagentx_session *sas, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_FATAL, fmt, ap, 0, NULL, sas, NULL,
	    NULL);
	va_end(ap);
	abort();
}

void
subagentx_log_sas_warnx(struct subagentx_session *sas, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 0, NULL, sas, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sas_warn(struct subagentx_session *sas, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 1, NULL, sas, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sas_info(struct subagentx_session *sas, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_INFO, fmt, ap, 0, NULL, sas, NULL,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sac_fatalx(struct subagentx_context *sac, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_FATAL, fmt, ap, 0, NULL, NULL, sac,
	    NULL);
	va_end(ap);
	abort();
}

void
subagentx_log_sac_warnx(struct subagentx_context *sac, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 0, NULL, NULL, sac,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sac_warn(struct subagentx_context *sac, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 1, NULL, NULL, sac,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sac_info(struct subagentx_context *sac, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_INFO, fmt, ap, 0, NULL, NULL, sac,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sac_debug(struct subagentx_context *sac, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_DEBUG, fmt, ap, 0, NULL, NULL, sac,
	    NULL);
	va_end(ap);
}

void
subagentx_log_sag_fatalx(struct subagentx_get *sag, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_FATAL, fmt, ap, 0, NULL, NULL, NULL,
	    sag);
	va_end(ap);
	abort();
}

void
subagentx_log_sag_warnx(struct subagentx_get *sag, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 0, NULL, NULL, NULL,
	    sag);
	va_end(ap);
}

void
subagentx_log_sag_warn(struct subagentx_get *sag, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_WARN, fmt, ap, 1, NULL, NULL, NULL,
	    sag);
	va_end(ap);
}

void
subagentx_log_sag_debug(struct subagentx_get *sag, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	subagentx_log_do(SUBAGENTX_LOG_TYPE_DEBUG, fmt, ap, 0, NULL, NULL, NULL,
	    sag);
	va_end(ap);
}

static void
subagentx_log_do(enum subagentx_log_type type, const char *fmt, va_list ap,
    int useerrno, struct subagentx *sa, struct subagentx_session *sas,
    struct subagentx_context *sac, struct subagentx_get *sag)
{
	void (*subagentx_log)(const char *, ...);
	char buf[1500];

	if (type == SUBAGENTX_LOG_TYPE_FATAL)
		subagentx_log = subagentx_log_fatal;
	else if (type == SUBAGENTX_LOG_TYPE_WARN)
		subagentx_log = subagentx_log_warn;
	else if (type == SUBAGENTX_LOG_TYPE_INFO)
		subagentx_log = subagentx_log_info;
	else
		subagentx_log = subagentx_log_debug;
	if (subagentx_log == NULL)
		return;

	vsnprintf(buf, sizeof(buf), fmt, ap);

	if (sag != NULL) {
		if (useerrno)
			subagentx_log("[fd:%d sess:%u ctx:%s trid:%u pid:%u]: "
			    "%s: %s", sag->sag_fd, sag->sag_sessionid,
			    SUBAGENTX_GET_CTXNAME(sag), sag->sag_transactionid,
			    sag->sag_packetid, buf, strerror(errno));
		else
			subagentx_log("[fd:%d sess:%u ctx:%s trid:%u pid:%u]: "
			    "%s", sag->sag_fd, sag->sag_sessionid,
			    SUBAGENTX_GET_CTXNAME(sag), sag->sag_transactionid,
			    sag->sag_packetid, buf);
	} else if (sac != NULL) {
		sas = sac->sac_sas;
		sa = sas->sas_sa;
		if (useerrno)
			subagentx_log("[fd:%d sess:%u ctx:%s]: %s: %s",
			    sa->sa_fd, sas->sas_id, SUBAGENTX_CONTEXT_NAME(sac),
			    buf, strerror(errno));
		else
			subagentx_log("[fd:%d sess:%u ctx:%s]: %s", sa->sa_fd,
			    sas->sas_id, SUBAGENTX_CONTEXT_NAME(sac), buf);
	} else if (sas != NULL) {
		sa = sas->sas_sa;
		if (useerrno)
			subagentx_log("[fd:%d sess:%u]: %s: %s", sa->sa_fd,
			    sas->sas_id, buf, strerror(errno));
		else
			subagentx_log("[fd:%d sess:%u]: %s", sa->sa_fd,
			    sas->sas_id, buf);
	} else if (sa->sa_fd == -1) {
		if (useerrno)
			subagentx_log("%s: %s", buf, strerror(errno));
		else
			subagentx_log("%s", buf);
	} else {
		if (useerrno)
			subagentx_log("[fd:%d]: %s: %s", sa->sa_fd, buf,
			    strerror(errno));
		else
			subagentx_log("[fd:%d]: %s", sa->sa_fd, buf);
	}
}
