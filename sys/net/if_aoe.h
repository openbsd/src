/* $OpenBSD: if_aoe.h,v 1.2 2010/08/21 06:50:42 blambert Exp $ */
/*
 * Copyright (c) 2007 Ted Unangst <tedu@openbsd.org>
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
#include <sys/workq.h>
#include <sys/timeout.h>	/* for struct timeout */

struct aoe_packet {
#define	AOE_F_ERROR	(1 << 2)
#define AOE_F_RESP	(1 << 3)
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char flags : 4;
	unsigned char vers : 4;
#else
	unsigned char vers : 4;
	unsigned char flags : 4;
#endif
	unsigned char error;
	unsigned short major;
	unsigned char minor;
	unsigned char command;
	unsigned int tag;
	union {
		/* command packet */
		struct {
#define			AOE_AF_WRITE	(1 << 0)
#define			AOE_AF_EXTENDED	(1 << 6)
			unsigned char aflags;
			unsigned char feature;
			unsigned char sectorcnt;
#define			AOE_READ	0x20
#define 		AOE_READ_EXT	0x24
#define 		AOE_WRITE	0x30
#define 		AOE_WRITE_EXT	0x34
			unsigned char cmd;
			unsigned char lba0;
			unsigned char lba1;
			unsigned char lba2;
#define			AOE_LBABIT	0x40
			unsigned char lba3;
			unsigned char lba4;
			unsigned char lba5;
			unsigned short reserved;
			unsigned char data[];
		} __packed;
		/* config packet */
		struct {
			unsigned short buffercnt;
			unsigned short firmwarevers;
			unsigned char configsectorcnt;
#if BYTE_ORDER == LITTLE_ENDIAN
			unsigned char ccmd : 4;
			unsigned char serververs : 4;
#else
			unsigned char serververs : 4;
			unsigned char ccmd : 4;
#endif
			unsigned short configstringlen;
			unsigned char configstring[1024];
		} __packed;
	};
} __packed;

#define AOE_BLK2HDR(blk, ap) do { \
	ap->lba0 = blk; \
        ap->lba1 = blk >> 8; \
	ap->lba2 = blk >> 16; \
} while (0)

#define AOE_HDR2BLK(ap, blk) do { \
	blk = 0; \
	blk |= ap->lba0; \
	blk |= ap->lba1 << 8; \
	blk |= ap->lba2 << 16; \
} while (0)


#define AOE_CFGHDRLEN 32
#define AOE_CMDHDRLEN 36

struct aoe_req {
	void *v;
	int tag;
	int len;
	TAILQ_ENTRY(aoe_req) next;
	struct timeout to;
};

struct aoe_handler {
	TAILQ_ENTRY(aoe_handler) next;
        unsigned short major;
	unsigned char minor;
	struct ifnet *ifp;
	workq_fn fn;
	struct workq_task task;
	TAILQ_HEAD(, aoe_req) reqs;
};

extern TAILQ_HEAD(aoe_handler_head, aoe_handler) aoe_handlers;
extern int aoe_waiting;

void aoe_input(struct ifnet *, struct mbuf *);
