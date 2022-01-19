/*	$OpenBSD: application_legacy.c,v 1.1 2022/01/19 10:59:35 martijn Exp $	*/

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

#include <event.h>
#include <stdlib.h>

#include "application.h"

#include "snmpd.h"

struct appl_varbind *appl_legacy_response(size_t);
void appl_legacy_get(struct appl_backend *, int32_t, int32_t, const char *,
    struct appl_varbind *);
void appl_legacy_getnext(struct appl_backend *, int32_t, int32_t, const char *,
    struct appl_varbind *);

struct appl_backend_functions appl_legacy_functions = {
	.ab_get = appl_legacy_get,
	.ab_getnext = appl_legacy_getnext,
	.ab_getbulk = NULL, /* Legacy getbulk implementation is broken */
};

struct appl_backend appl_legacy = {
	.ab_name = "legacy backend",
	.ab_cookie = NULL,
	.ab_retries = 0,
	.ab_fn = &appl_legacy_functions
};

static struct ber_element *root;
static struct appl_varbind *response = NULL;
static size_t responsesz = 0;

void
appl_legacy_init(void)
{
	struct oid *object = NULL;
	struct ber_oid oid;

	while ((object = smi_foreach(object, OID_RD)) != NULL) {
		oid = object->o_id;
		if (!(object->o_flags & OID_TABLE))
			oid.bo_id[oid.bo_n++] = 0;
		appl_register(NULL, 150, 1, &oid,
		    !(object->o_flags & OID_TABLE), 1, 0, 0, &appl_legacy);
	}

	if ((root = ober_add_sequence(NULL)) == NULL)
		fatal("%s: Failed to init root", __func__);
}

void
appl_legacy_shutdown(void)
{
	appl_close(&appl_legacy);

	ober_free_elements(root);
	free(response);
}

struct appl_varbind *
appl_legacy_response(size_t nvarbind)
{
	struct appl_varbind *tmp;
	size_t i;

	if (responsesz < nvarbind) {
		if ((tmp = recallocarray(response, responsesz, nvarbind,
		    sizeof(*response))) == NULL) {
			log_warn(NULL);
			return NULL;
		}
		responsesz = nvarbind;
		response = tmp;
	}
	for (i = 0; i < nvarbind; i++)
		response[i].av_next = i + 1 == nvarbind ?
		    NULL : &(response[i + 1]);
	return response;
}

void
appl_legacy_get(struct appl_backend *backend, __unused int32_t transactionid,
    int32_t requestid, __unused const char *ctx, struct appl_varbind *vblist)
{
	size_t i;
	struct ber_element *elm;
	struct appl_varbind *vb, *rvb, *rvblist;

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next)
		i++;
	if ((rvblist = appl_legacy_response(i)) == NULL) {
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vb);
		return;
	}
	rvb = rvblist;
	for (vb = vblist; vb != NULL; vb = vb->av_next, rvb = rvb->av_next) {
		(void)mps_getreq(NULL, root, &(vb->av_oid), 1);
		elm = ober_unlink_elements(root);
		ober_get_oid(elm, &(rvb->av_oid));
		rvb->av_value = ober_unlink_elements(elm);
		ober_free_elements(elm);
	}

	appl_response(backend, requestid, APPL_ERROR_NOERROR, 0, rvblist);
}

void
appl_legacy_getnext(struct appl_backend *backend,
    __unused int32_t transactionid, int32_t requestid, __unused const char *ctx,
    struct appl_varbind *vblist)
{
	size_t i;
	struct ber_element *elm;
	struct appl_varbind *vb, *rvb, *rvblist;
	struct snmp_message msg;
	int ret;

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next)
		i++;
	if ((rvblist = appl_legacy_response(i)) == NULL) {
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vb);
		return;
	}
	rvb = rvblist;
	i = 1;
	for (vb = vblist; vb != NULL; vb = vb->av_next, rvb = rvb->av_next) {
		ret = -1;
	 	if (vb->av_include) {
			ret = mps_getreq(NULL, root, &(vb->av_oid), 0);
			if (ret == -1)
				ober_free_elements(ober_unlink_elements(root));
		}
		rvb->av_oid = vb->av_oid;
		if (ret == -1) {
			msg.sm_version = 1;
			(void)mps_getnextreq(&msg, root, &(rvb->av_oid));
		}
		elm = ober_unlink_elements(root);
		ober_get_oid(elm, &(rvb->av_oid));
		if (ober_oid_cmp(&(rvb->av_oid), &(vb->av_oid_end)) > 0) {
			rvb->av_oid = vb->av_oid;
			rvb->av_value = appl_exception(APPL_EXC_ENDOFMIBVIEW);
			if (rvb->av_value == NULL) {
				log_warn("Failed to create endOfMibView");
				rvb->av_next = NULL;
				appl_response(backend, requestid,
				    APPL_ERROR_GENERR, i, rvblist);
				return;
			}
		} else
			rvb->av_value = ober_unlink_elements(elm);
		ober_free_elements(elm);
		i++;
	}

	appl_response(backend, requestid, APPL_ERROR_NOERROR, 0, rvblist);
}
