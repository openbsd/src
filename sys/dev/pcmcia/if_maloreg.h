/*	$OpenBSD: if_maloreg.h,v 1.3 2007/05/26 11:11:54 mglocker Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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

/* I/O registers */
#define MALO_REG_HOST_STATUS		0x00
#define MALO_REG_CARD_INTR_CAUSE	0x02
#define MALO_REG_HOST_INTR_MASK		0x04
#define MALO_REG_CMD_READ		0x12
#define MALO_REG_CMD_WRITE_LEN		0x18
#define MALO_REG_CMD_WRITE		0x1a
#define MALO_REG_CARD_STATUS		0x20
#define MALO_REG_HOST_INTR_CAUSE	0x22
#define MALO_REG_RBAL			0x28
#define MALO_REG_CMD_READ_LEN		0x30
#define MALO_REG_SCRATCH		0x3f
#define MALO_REG_CARD_INTR_MASK		0x44

/* register values */
#define MALO_VAL_SCRATCH_READY		0x00
#define MALO_VAL_SCRATCH_FW_LOADED	0x5a
#define MALO_VAL_HOST_INTR_MASK_ON	0x001f
#define MALO_VAL_DNLD_OVER		(1 << 2)

/* interrupt reasons */
#define MALO_VAL_HOST_INTR_RX		(1 << 0)
#define MALO_VAL_HOST_INTR_CMD		(1 << 3)

/* FW commands */
#define MALO_VAL_CMD_RESP		0x8000
#define MALO_VAL_CMD_HWSPEC		0x0003
#define MALO_VAL_CMD_RESET		0x0005
#define MALO_VAL_CMD_CHANNEL		0x001d
