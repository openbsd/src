/*	$OpenBSD: rtr_config.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

#define DEFAULT_PORT		323

#define READ_BLOCK		65536
#define WRITE_BLOCK		65536

/* MUST be less than or equal to READ_BLOCK or WRITE_BLOCK */
#define UNIX_READ_BLOCK		4096
#define UNIX_WRITE_BLOCK	4096

#define SENDQ_BLOCK		524288		/*   0x80000 */

#define SENDQ_MAX		41943040	/* 0x2800000 */

#define CACHE_FRAME_COUNT	10

#define REFRESH_INTERVAL	3600	/*   1 -  86400 seconds */
#define RETRY_INTERVAL		600	/*   1 -   7200 seconds */
#define EXPIRE_INTERVAL		7200	/* 600 - 172800 seconds */

#define CONTROLLER_FILENAME	"/var/run/rtrd.sock"
#define IMPORT_FILENAME		"/var/db/rpki-client/openbgpd"

#define CLEANUP_INTERVAL	180

#define HANDSHAKE_TIMEOUT	30
#define IDLE_TIMEOUT		3600

#define RTRD_USER		"_rtrd"
