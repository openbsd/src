/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: buffer.c,v 1.8 2020/02/26 18:47:59 florian Exp $ */

/*! \file */

#include <stdlib.h>
#include <isc/buffer.h>

#include <isc/region.h>
#include <string.h>
#include <isc/util.h>

void
isc__buffer_init(isc_buffer_t *b, void *base, unsigned int length) {
	/*
	 * Make 'b' refer to the 'length'-byte region starting at 'base'.
	 * XXXDCL see the comment in buffer.h about base being const.
	 */

	REQUIRE(b != NULL);

	ISC__BUFFER_INIT(b, base, length);
}

void
isc__buffer_invalidate(isc_buffer_t *b) {
	/*
	 * Make 'b' an invalid buffer.
	 */

	REQUIRE(!ISC_LINK_LINKED(b, link));

	ISC__BUFFER_INVALIDATE(b);
}

void
isc__buffer_usedregion(isc_buffer_t *b, isc_region_t *r) {
	/*
	 * Make 'r' refer to the used region of 'b'.
	 */

	REQUIRE(r != NULL);

	ISC__BUFFER_USEDREGION(b, r);
}

void
isc__buffer_availableregion(isc_buffer_t *b, isc_region_t *r) {
	/*
	 * Make 'r' refer to the available region of 'b'.
	 */

	REQUIRE(r != NULL);

	ISC__BUFFER_AVAILABLEREGION(b, r);
}

void
isc__buffer_add(isc_buffer_t *b, unsigned int n) {
	/*
	 * Increase the 'used' region of 'b' by 'n' bytes.
	 */

	REQUIRE(b->used + n <= b->length);

	ISC__BUFFER_ADD(b, n);
}

void
isc__buffer_subtract(isc_buffer_t *b, unsigned int n) {
	/*
	 * Decrease the 'used' region of 'b' by 'n' bytes.
	 */

	REQUIRE(b->used >= n);

	ISC__BUFFER_SUBTRACT(b, n);
}

void
isc__buffer_clear(isc_buffer_t *b) {
	/*
	 * Make the used region empty.
	 */

	ISC__BUFFER_CLEAR(b);
}

void
isc__buffer_remainingregion(isc_buffer_t *b, isc_region_t *r) {
	/*
	 * Make 'r' refer to the remaining region of 'b'.
	 */

	REQUIRE(r != NULL);

	ISC__BUFFER_REMAININGREGION(b, r);
}

void
isc__buffer_activeregion(isc_buffer_t *b, isc_region_t *r) {
	/*
	 * Make 'r' refer to the active region of 'b'.
	 */

	REQUIRE(r != NULL);

	ISC__BUFFER_ACTIVEREGION(b, r);
}

void
isc__buffer_setactive(isc_buffer_t *b, unsigned int n) {
	/*
	 * Sets the end of the active region 'n' bytes after current.
	 */

	REQUIRE(b->current + n <= b->used);

	ISC__BUFFER_SETACTIVE(b, n);
}

void
isc__buffer_first(isc_buffer_t *b) {
	/*
	 * Make the consumed region empty.
	 */

	ISC__BUFFER_FIRST(b);
}

void
isc__buffer_forward(isc_buffer_t *b, unsigned int n) {
	/*
	 * Increase the 'consumed' region of 'b' by 'n' bytes.
	 */

	REQUIRE(b->current + n <= b->used);

	ISC__BUFFER_FORWARD(b, n);
}

void
isc_buffer_compact(isc_buffer_t *b) {
	unsigned int length;
	void *src;

	/*
	 * Compact the used region by moving the remaining region so it occurs
	 * at the start of the buffer.  The used region is shrunk by the size
	 * of the consumed region, and the consumed region is then made empty.
	 */

	src = isc_buffer_current(b);
	length = isc_buffer_remaininglength(b);
	(void)memmove(b->base, src, (size_t)length);

	if (b->active > b->current)
		b->active -= b->current;
	else
		b->active = 0;
	b->current = 0;
	b->used = length;
}

uint8_t
isc_buffer_getuint8(isc_buffer_t *b) {
	unsigned char *cp;
	uint8_t result;

	/*
	 * Read an unsigned 8-bit integer from 'b' and return it.
	 */

	REQUIRE(b->used - b->current >= 1);

	cp = isc_buffer_current(b);
	b->current += 1;
	result = ((uint8_t)(cp[0]));

	return (result);
}

void
isc__buffer_putuint8(isc_buffer_t *b, uint8_t val) {
	REQUIRE(b->used + 1 <= b->length);

	ISC__BUFFER_PUTUINT8(b, val);
}

uint16_t
isc_buffer_getuint16(isc_buffer_t *b) {
	unsigned char *cp;
	uint16_t result;

	/*
	 * Read an unsigned 16-bit integer in network byte order from 'b',
	 * convert it to host byte order, and return it.
	 */

	REQUIRE(b->used - b->current >= 2);

	cp = isc_buffer_current(b);
	b->current += 2;
	result = ((unsigned int)(cp[0])) << 8;
	result |= ((unsigned int)(cp[1]));

	return (result);
}

void
isc__buffer_putuint16(isc_buffer_t *b, uint16_t val) {
	REQUIRE(b->used + 2 <= b->length);

	ISC__BUFFER_PUTUINT16(b, val);
}

uint32_t
isc_buffer_getuint32(isc_buffer_t *b) {
	unsigned char *cp;
	uint32_t result;

	/*
	 * Read an unsigned 32-bit integer in network byte order from 'b',
	 * convert it to host byte order, and return it.
	 */

	REQUIRE(b->used - b->current >= 4);

	cp = isc_buffer_current(b);
	b->current += 4;
	result = ((unsigned int)(cp[0])) << 24;
	result |= ((unsigned int)(cp[1])) << 16;
	result |= ((unsigned int)(cp[2])) << 8;
	result |= ((unsigned int)(cp[3]));

	return (result);
}

void
isc__buffer_putuint32(isc_buffer_t *b, uint32_t val) {
	REQUIRE(b->used + 4 <= b->length);

	ISC__BUFFER_PUTUINT32(b, val);
}

void
isc__buffer_putuint48(isc_buffer_t *b, uint64_t val) {
	uint16_t valhi;
	uint32_t vallo;

	REQUIRE(b->used + 6 <= b->length);

	valhi = (uint16_t)(val >> 32);
	vallo = (uint32_t)(val & 0xFFFFFFFF);
	ISC__BUFFER_PUTUINT16(b, valhi);
	ISC__BUFFER_PUTUINT32(b, vallo);
}

void
isc__buffer_putmem(isc_buffer_t *b, const unsigned char *base,
		   unsigned int length)
{
	REQUIRE(b->used + length <= b->length);

	ISC__BUFFER_PUTMEM(b, base, length);
}

void
isc__buffer_putstr(isc_buffer_t *b, const char *source) {
	unsigned int l;
	unsigned char *cp;

	REQUIRE(source != NULL);

	/*
	 * Do not use ISC__BUFFER_PUTSTR(), so strlen is only done once.
	 */
	l = strlen(source);

	REQUIRE(l <= isc_buffer_availablelength(b));

	cp = isc_buffer_used(b);
	memmove(cp, source, l);
	b->used += l;
}

isc_result_t
isc_buffer_copyregion(isc_buffer_t *b, const isc_region_t *r) {
	unsigned char *base;
	unsigned int available;

	REQUIRE(r != NULL);

	/*
	 * XXXDCL
	 */
	base = isc_buffer_used(b);
	available = isc_buffer_availablelength(b);
	if (r->length > available)
		return (ISC_R_NOSPACE);
	memmove(base, r->base, r->length);
	b->used += r->length;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_buffer_allocate(isc_buffer_t **dynbuffer,
		    unsigned int length)
{
	isc_buffer_t *dbuf;

	REQUIRE(dynbuffer != NULL);
	REQUIRE(*dynbuffer == NULL);

	dbuf = malloc(length + sizeof(isc_buffer_t));
	if (dbuf == NULL)
		return (ISC_R_NOMEMORY);

	isc_buffer_init(dbuf, ((unsigned char *)dbuf) + sizeof(isc_buffer_t),
			length);

	*dynbuffer = dbuf;

	return (ISC_R_SUCCESS);
}

void
isc_buffer_free(isc_buffer_t **dynbuffer) {
	isc_buffer_t *dbuf;

	REQUIRE(dynbuffer != NULL);
	dbuf = *dynbuffer;
	*dynbuffer = NULL;	/* destroy external reference */

	isc_buffer_invalidate(dbuf);

	free(dbuf);
}

isc_result_t
isc_mem_tobuffer(isc_buffer_t *target, void *base, unsigned int length) {
	isc_region_t tr;

	isc_buffer_availableregion(target, &tr);
	if (length > tr.length)
		return (ISC_R_NOSPACE);
	memmove(tr.base, base, length);
	isc_buffer_add(target, length);
	return (ISC_R_SUCCESS);
}

/* this used to be str_totext() in rdata.c etc. */
isc_result_t
isc_str_tobuffer(const char *source, isc_buffer_t *target) {
	unsigned int l;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	l = strlen(source);

	if (l > region.length)
		return (ISC_R_NOSPACE);

	memmove(region.base, source, l);
	isc_buffer_add(target, l);
	return (ISC_R_SUCCESS);
}
