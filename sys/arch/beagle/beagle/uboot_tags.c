/* $OpenBSD: uboot_tags.c,v 1.3 2011/11/08 22:41:41 krw Exp $ */
/*
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
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
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bootconfig.h>

struct uboot_tag_header {
	uint32_t	size;
	uint32_t	tag;
};
struct uboot_tag_core {
	uint32_t	flags;
	uint32_t	pagesize;
	uint32_t	rootdev;
};
struct uboot_tag_serialnr {
	uint32_t	low;
	uint32_t	high;
};
struct uboot_tag_revision {
	uint32_t	rev;
};
struct uboot_tag_mem32 {
	uint32_t	size;
	uint32_t	start;
};
struct uboot_tag_cmdline {
	char		cmdline[1];
};

#define ATAG_CORE	0x54410001
#define	ATAG_MEM	0x54410002
#define	ATAG_CMDLINE	0x54410009
#define	ATAG_SERIAL	0x54410006
#define	ATAG_REVISION	0x54410007
#define	ATAG_NONE	0x00000000
struct uboot_tag {
	struct uboot_tag_header hdr;
	union {
		struct uboot_tag_core		core;
		struct uboot_tag_mem32		mem;
		struct uboot_tag_revision	rev;
		struct uboot_tag_serialnr	serialnr;
		struct uboot_tag_cmdline	cmdline;
	} u;
};

int parse_uboot_tags(void *handle);
int
parse_uboot_tags(void *handle)
{
	uint32_t *p;
	struct uboot_tag *tag;
	int i;

	p = handle;
	tag = (struct uboot_tag *)p;

	while(tag != NULL && tag->hdr.size < 4096 &&
	    tag->hdr.tag != ATAG_NONE) {
		switch (tag->hdr.tag) {
		case ATAG_CORE:
			printf("atag core flags %x pagesize %x rootdev %x\n",
			    tag->u.core.flags,
			    tag->u.core.pagesize,
			    tag->u.core.rootdev);
			break;
		case ATAG_MEM:
			printf("atag mem start 0x%08x size 0x%x\n",
			    tag->u.mem.start,
			    tag->u.mem.size);

			i = bootconfig.dramblocks -1;
			if (bootconfig.dramblocks != 0 &&
			    (tag->u.mem.start == bootconfig.dram[i].address +
			    (bootconfig.dram[i].pages * PAGE_SIZE))) {
				bootconfig.dram[i].pages =
				    bootconfig.dram[i].pages +
				    tag->u.mem.size / PAGE_SIZE;
			} else { 
				i = bootconfig.dramblocks;
				bootconfig.dram[i].address = tag->u.mem.start;
				bootconfig.dram[i].pages = tag->u.mem.size
				    / PAGE_SIZE;
				bootconfig.dramblocks = i + 1;
			}

			break;
		case ATAG_CMDLINE:
			printf("atag cmdline [%s]\n",
			    tag->u.cmdline.cmdline);
			strncpy(bootconfig.bootstring, tag->u.cmdline.cmdline,
			    sizeof(bootconfig.bootstring));
			break;
		case ATAG_SERIAL:
			printf("atag serial 0x%08x:%08x\n",
			    tag->u.serialnr.high,
			    tag->u.serialnr.low);
			break;
		case ATAG_REVISION:
			printf("atag revision %08x\n",
			    tag->u.rev.rev);
			break;
		default:
			printf("uboot tag unknown 0x%08x size %d\n",
			    tag->hdr.tag,
			    tag->hdr.size);
		}
		p = p + tag->hdr.size;
		tag = (struct uboot_tag *)p;
	}
	
	return 0;
}
