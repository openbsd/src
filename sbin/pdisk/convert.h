/*	$OpenBSD: convert.h,v 1.11 2016/01/23 23:25:58 krw Exp $	*/

/*
 * convert.h - Little-endian conversion
 *
 * Written by Eryk Vershen
 *
 * The approach taken to conversion is fairly simply.
 * Keep the in-memory copy in the machine's normal form and
 * Convert as necessary when reading and writing.
 */

/*
 * Copyright 1996,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __convert__
#define __convert__

int convert_block0(struct block0 *, int);
int convert_dpme(struct dpme *, int);

#endif /* __convert__ */
