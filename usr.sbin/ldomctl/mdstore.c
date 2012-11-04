/*	$OpenBSD: mdstore.c,v 1.3 2012/11/04 23:30:38 kettenis Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis
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

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds.h"
#include "mdstore.h"
#include "util.h"

void	mdstore_start(struct ldc_conn *, uint64_t);
void	mdstore_rx_data(struct ldc_conn *, uint64_t, void *, size_t);

struct ds_service mdstore_service = {
	"mdstore", 1, 0, mdstore_start, mdstore_rx_data
};

#define MDSET_BEGIN_REQUEST	0x0001
#define MDSET_END_REQUEST	0x0002
#define MD_TRANSFER_REQUEST	0x0003
#define MDSET_LIST_REQUEST	0x0004
#define MDSET_SELECT_REQUEST	0x0005
#define MDSET_DELETE_REQUEST	0x0006
#define MDSET_RETREIVE_REQUEST	0x0007

struct mdstore_msg {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint16_t	command;
} __packed;

struct mdstore_begin_end_req {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint16_t	command;
	uint16_t	nmds;
	uint32_t	namelen;
	char		name[1];
} __packed;

struct mdstore_transfer_req {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint16_t	command;
	uint16_t	type;
	uint32_t	size;
	uint64_t	offset;
	char		md[];
} __packed;

#define MDSTORE_PRI_TYPE	0x01
#define MDSTORE_HV_MD_TYPE	0x02
#define MDSTORE_CTL_DOM_MD_TYPE	0x04
#define MDSTORE_SVC_DOM_MD_TYPE	0x08

struct mdstore_sel_del_req {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint16_t	command;
	uint16_t	reserved;
	uint32_t	namelen;
	char		name[1];
} __packed;

#define MDSET_LIST_REPLY	0x0104

struct mdstore_list_resp {
	uint32_t	msg_type;
	uint32_t	payload_len;
	uint64_t	svc_handle;
	uint64_t	reqnum;
	uint32_t	result;
	uint16_t	booted_set;
	uint16_t	boot_set;
	char		sets[1];
} __packed;

#define MDST_SUCCESS		0x0
#define MDST_FAILURE		0x1
#define MDST_INVALID_MSG	0x2
#define MDST_MAX_MDS_ERR	0x3
#define MDST_BAD_NAME_ERR	0x4
#define MDST_SET_EXISTS_ERR	0x5
#define MDST_ALLOC_SET_ERR	0x6
#define MDST_ALLOC_MD_ERR	0x7
#define MDST_MD_COUNT_ERR	0x8
#define MDST_MD_SIZE_ERR	0x9
#define MDST_MD_TYPE_ERR	0xa
#define MDST_NOT_EXIST_ERR	0xb

struct mdstore_set_head mdstore_sets = TAILQ_HEAD_INITIALIZER(mdstore_sets);
uint64_t mdstore_reqnum;
uint64_t mdstore_command;

void
mdstore_start(struct ldc_conn *lc, uint64_t svc_handle)
{
	struct mdstore_msg mm;

	bzero(&mm, sizeof(mm));
	mm.msg_type = DS_DATA;
	mm.payload_len = sizeof(mm) - 8;
	mm.svc_handle = svc_handle;
	mm.reqnum = mdstore_reqnum++;
	mm.command = mdstore_command = MDSET_LIST_REQUEST;
	ds_send_msg(lc, &mm, sizeof(mm));
}

void
mdstore_rx_data(struct ldc_conn *lc, uint64_t svc_handle, void *data,
    size_t len)
{
	struct mdstore_list_resp *mr = data;
	struct mdstore_set *set;
	int idx;

	if (mr->result != MDST_SUCCESS) {
		DPRINTF(("Unexpected result 0x%x\n", mr->result));
		return;
	}

	switch (mdstore_command) {
	case MDSET_LIST_REQUEST:
		for (idx = 0, len = 0; len < mr->payload_len - 24; idx++) {
			set = xmalloc(sizeof(*set));
			set->name = xstrdup(&mr->sets[len]);
			set->booted_set = (idx == mr->booted_set);
			set->boot_set = (idx == mr->boot_set);
			TAILQ_INSERT_TAIL(&mdstore_sets, set, link);
			len += strlen(&mr->sets[len]) + 1;
		}
		break;
	}

	mdstore_command = 0;
}

void
mdstore_begin(struct ds_conn *dc, uint64_t svc_handle, const char *name)
{
	struct mdstore_begin_end_req *mr;
	size_t len = sizeof(*mr) + strlen(name);

	mr = xzalloc(len);
	mr->msg_type = DS_DATA;
	mr->payload_len = len - 8;
	mr->svc_handle = svc_handle;
	mr->reqnum = mdstore_reqnum++;
	mr->command = mdstore_command = MDSET_BEGIN_REQUEST;
	mr->nmds = 3;		/* XXX */
	mr->namelen = strlen(name);
	memcpy(mr->name, name, strlen(name));

	ds_send_msg(&dc->lc, mr, len);
	free(mr);

	while (mdstore_command == MDSET_BEGIN_REQUEST)
		ds_conn_handle(dc);
}

void
mdstore_transfer(struct ds_conn *dc, uint64_t svc_handle, const char *path,
    uint16_t type, uint64_t offset)
{
	struct mdstore_transfer_req *mr;
	uint32_t size;
	size_t len;
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == NULL)
		err(1, "fopen");

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	len = sizeof(*mr) + size;
	mr = xzalloc(len);

	mr->msg_type = DS_DATA;
	mr->payload_len = len - 8;
	mr->svc_handle = svc_handle;
	mr->reqnum = mdstore_reqnum++;
	mr->command = mdstore_command = MD_TRANSFER_REQUEST;
	mr->type = type;
	mr->size = size;
	mr->offset = offset;
	if (fread(&mr->md, size, 1, fp) != 1)
		err(1, "fread");
	ds_send_msg(&dc->lc, mr, len);
	free(mr);

	fclose(fp);

	while (mdstore_command == MD_TRANSFER_REQUEST)
		ds_conn_handle(dc);
}

void
mdstore_end(struct ds_conn *dc, uint64_t svc_handle, const char *name)
{
	struct mdstore_begin_end_req *mr;
	size_t len = sizeof(*mr) + strlen(name);

	mr = xzalloc(len);
	mr->msg_type = DS_DATA;
	mr->payload_len = len - 8;
	mr->svc_handle = svc_handle;
	mr->reqnum = mdstore_reqnum++;
	mr->command = mdstore_command = MDSET_END_REQUEST;
	mr->nmds = 3;
	mr->namelen = strlen(name);
	memcpy(mr->name, name, strlen(name));

	ds_send_msg(&dc->lc, mr, len);
	free(mr);

	while (mdstore_command == MDSET_END_REQUEST)
		ds_conn_handle(dc);
}

void
mdstore_select(struct ds_conn *dc, const char *name)
{
	struct ds_conn_svc *dcs;
	struct mdstore_sel_del_req *mr;
	size_t len = sizeof(*mr) + strlen(name);

	TAILQ_FOREACH(dcs, &dc->services, link)
		if (strcmp(dcs->service->ds_svc_id, "mdstore") == 0)
			break;
	assert(dcs != TAILQ_END(&dc->services));

	mr = xzalloc(len);
	mr->msg_type = DS_DATA;
	mr->payload_len = len - 8;
	mr->svc_handle = dcs->svc_handle;
	mr->reqnum = mdstore_reqnum++;
	mr->command = mdstore_command = MDSET_SELECT_REQUEST;
	mr->namelen = strlen(name);
	memcpy(mr->name, name, strlen(name));

	ds_send_msg(&dc->lc, mr, len);
	free(mr);

	while (mdstore_command == MDSET_SELECT_REQUEST)
		ds_conn_handle(dc);
}

void
mdstore_delete(struct ds_conn *dc, const char *name)
{
	struct ds_conn_svc *dcs;
	struct mdstore_sel_del_req *mr;
	size_t len = sizeof(*mr) + strlen(name);

	TAILQ_FOREACH(dcs, &dc->services, link)
		if (strcmp(dcs->service->ds_svc_id, "mdstore") == 0)
			break;
	assert(dcs != TAILQ_END(&dc->services));

	mr = xzalloc(len);
	mr->msg_type = DS_DATA;
	mr->payload_len = len - 8;
	mr->svc_handle = dcs->svc_handle;
	mr->reqnum = mdstore_reqnum++;
	mr->command = mdstore_command = MDSET_DELETE_REQUEST;
	mr->namelen = strlen(name);
	memcpy(mr->name, name, strlen(name));

	ds_send_msg(&dc->lc, mr, len);
	free(mr);

	while (mdstore_command == MDSET_DELETE_REQUEST)
		ds_conn_handle(dc);
}

void
mdstore_download(struct ds_conn *dc, const char *name)
{
	struct ds_conn_svc *dcs;

	TAILQ_FOREACH(dcs, &dc->services, link)
		if (strcmp(dcs->service->ds_svc_id, "mdstore") == 0)
			break;
	assert(dcs != TAILQ_END(&dc->services));

	printf("begin\n");
	mdstore_begin(dc, dcs->svc_handle, name);
	printf("transfer 0\n");
	mdstore_transfer(dc, dcs->svc_handle, "primary.md",
	    MDSTORE_CTL_DOM_MD_TYPE, 0x100000);
	printf("transfer 1\n");
	mdstore_transfer(dc, dcs->svc_handle, "hv.md",
	    MDSTORE_HV_MD_TYPE, 0x80000);
	printf("transfer 2\n");
	mdstore_transfer(dc, dcs->svc_handle, "pri",
	    MDSTORE_PRI_TYPE, 0);
	printf("end\n");
	mdstore_end(dc, dcs->svc_handle, name);
	printf("done\n");
}
