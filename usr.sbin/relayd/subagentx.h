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

#include <stdint.h>
#include <stddef.h>

struct subagentx;
struct subagentx_session;
struct subagentx_context;
struct subagentx_agentcaps;
struct subagentx_region;
struct subagentx_index;
struct subagentx_object;
struct subagentx_varbind;

enum subagentx_request_type {
	SUBAGENTX_REQUEST_TYPE_GET,
	SUBAGENTX_REQUEST_TYPE_GETNEXT,
	SUBAGENTX_REQUEST_TYPE_GETNEXTINCLUSIVE
};

#define SUBAGENTX_AGENTX_MASTER "/var/agentx/master"
#define SUBAGENTX_OID_MAX_LEN 128
#define SUBAGENTX_OID_INDEX_MAX_LEN 10
#define SUBAGENTX_MIB2 1, 3, 6, 1, 2, 1
#define SUBAGENTX_ENTERPRISES 1, 3, 6, 1, 4, 1
#define SUBAGENTX_OID(...) (uint32_t []) { __VA_ARGS__ }, \
    (sizeof((uint32_t []) { __VA_ARGS__ }) / sizeof(uint32_t))

extern void (*subagentx_log_fatal)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
extern void (*subagentx_log_warn)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
extern void (*subagentx_log_info)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
extern void (*subagentx_log_debug)(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));

struct subagentx *subagentx(void (*)(struct subagentx *, void *, int), void *);
void subagentx_connect(struct subagentx *, int);
void subagentx_read(struct subagentx *);
void subagentx_write(struct subagentx *);
extern void (*subagentx_wantwrite)(struct subagentx *, int);
void subagentx_free(struct subagentx *);
struct subagentx_session *subagentx_session(struct subagentx *,
    uint32_t[], size_t, const char *, uint8_t);
void subagentx_session_free(struct subagentx_session *);
struct subagentx_context *subagentx_context(struct subagentx_session *,
    const char *);
struct subagentx_object *subagentx_context_object_find(
    struct subagentx_context *, const uint32_t[], size_t, int, int);
struct subagentx_object *subagentx_context_object_nfind(
    struct subagentx_context *, const uint32_t[], size_t, int, int);
uint32_t subagentx_context_uptime(struct subagentx_context *);
void subagentx_context_free(struct subagentx_context *);
struct subagentx_agentcaps *subagentx_agentcaps(struct subagentx_context *,
    uint32_t[], size_t, const char *);
void subagentx_agentcaps_free(struct subagentx_agentcaps *);
struct subagentx_region *subagentx_region(struct subagentx_context *,
    uint32_t[], size_t, uint8_t);
void subagentx_region_free(struct subagentx_region *);
struct subagentx_index *subagentx_index_integer_new(struct subagentx_region *,
    uint32_t[], size_t);
struct subagentx_index *subagentx_index_integer_any(struct subagentx_region *,
    uint32_t[], size_t);
struct subagentx_index *subagentx_index_integer_value(struct subagentx_region *,
    uint32_t[], size_t, uint32_t);
struct subagentx_index *subagentx_index_integer_dynamic(
    struct subagentx_region *, uint32_t[], size_t);
struct subagentx_index *subagentx_index_string_dynamic(
    struct subagentx_region *, uint32_t[], size_t);
struct subagentx_index *subagentx_index_nstring_dynamic(
    struct subagentx_region *, uint32_t[], size_t, size_t);
struct subagentx_index *subagentx_index_oid_dynamic(struct subagentx_region *,
    uint32_t[], size_t);
struct subagentx_index *subagentx_index_noid_dynamic(struct subagentx_region *,
    uint32_t[], size_t, size_t);
struct subagentx_index *subagentx_index_ipaddress_dynamic(
    struct subagentx_region *, uint32_t[], size_t);
void subagentx_index_free(struct subagentx_index *);
struct subagentx_object *subagentx_object(struct subagentx_region *, uint32_t[],
    size_t, struct subagentx_index *[], size_t, int,
    void (*)(struct subagentx_varbind *));
void subagentx_object_free(struct subagentx_object *);

void subagentx_varbind_integer(struct subagentx_varbind *, uint32_t);
void subagentx_varbind_string(struct subagentx_varbind *, const char *);
void subagentx_varbind_nstring(struct subagentx_varbind *,
    const unsigned char *, size_t);
void subagentx_varbind_printf(struct subagentx_varbind *, const char *, ...)
    __attribute__((__format__ (printf, 2, 3)));
void subagentx_varbind_null(struct subagentx_varbind *);
void subagentx_varbind_oid(struct subagentx_varbind *, const uint32_t[],
    size_t);
void subagentx_varbind_object(struct subagentx_varbind *,
    struct subagentx_object *);
void subagentx_varbind_index(struct subagentx_varbind *,
    struct subagentx_index *);
void subagentx_varbind_ipaddress(struct subagentx_varbind *,
    const struct in_addr *);
void subagentx_varbind_counter32(struct subagentx_varbind *, uint32_t);
void subagentx_varbind_gauge32(struct subagentx_varbind *, uint32_t);
void subagentx_varbind_timeticks(struct subagentx_varbind *, uint32_t);
void subagentx_varbind_opaque(struct subagentx_varbind *, const char *, size_t);
void subagentx_varbind_counter64(struct subagentx_varbind *, uint64_t);
void subagentx_varbind_notfound(struct subagentx_varbind *);
void subagentx_varbind_error(struct subagentx_varbind *);

enum subagentx_request_type subagentx_varbind_request(
    struct subagentx_varbind *);
struct subagentx_object *
    subagentx_varbind_get_object(struct subagentx_varbind *);
uint32_t subagentx_varbind_get_index_integer(struct subagentx_varbind *,
    struct subagentx_index *);
const unsigned char *subagentx_varbind_get_index_string(
    struct subagentx_varbind *, struct subagentx_index *, size_t *, int *);
const uint32_t *subagentx_varbind_get_index_oid(struct subagentx_varbind *,
    struct subagentx_index *, size_t *, int *);
const struct in_addr *subagentx_varbind_get_index_ipaddress(
    struct subagentx_varbind *, struct subagentx_index *);
void subagentx_varbind_set_index_integer(struct subagentx_varbind *,
    struct subagentx_index *, uint32_t);
void subagentx_varbind_set_index_string(struct subagentx_varbind *,
    struct subagentx_index *, const char *);
void subagentx_varbind_set_index_nstring(struct subagentx_varbind *,
    struct subagentx_index *, const unsigned char *, size_t);
void subagentx_varbind_set_index_oid(struct subagentx_varbind *,
    struct subagentx_index *, const uint32_t *, size_t);
void subagentx_varbind_set_index_object(struct subagentx_varbind *,
    struct subagentx_index *, struct subagentx_object *);
void subagentx_varbind_set_index_ipaddress(struct subagentx_varbind *,
    struct subagentx_index *, const struct in_addr *);
