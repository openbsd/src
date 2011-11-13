/*      $Id: mandocdb.h,v 1.1 2011/11/13 10:40:52 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

#define	MANDOC_DB	"mandoc.db"
#define	MANDOC_IDX	"mandoc.index"

#define	TYPE_An		0x01
#define	TYPE_Cd		0x02
#define	TYPE_Er		0x04
#define	TYPE_Ev		0x08
#define	TYPE_Fn		0x10
#define	TYPE_In		0x20
#define	TYPE_Nd		0x40
#define	TYPE_Nm		0x100
#define	TYPE_Pa		0x200
#define	TYPE_St		0x400
#define	TYPE_Va		0x1000
#define	TYPE_Xr		0x2000
