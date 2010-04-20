/*	$OpenBSD: bus_space.c,v 1.1 2010/04/20 22:53:20 miod Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Simple generic bus access primitives, for memory mapped space without
 * any access restriction or endianness conversion required.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

uint8_t
generic_space_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint8_t *)(h + o);
}

uint16_t
generic_space_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint16_t *)(h + o);
}

uint32_t
generic_space_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint32_t *)(h + o);
}

void
generic_space_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	*(volatile uint8_t *)(h + o) = v;
}

void
generic_space_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	*(volatile uint16_t *)(h + o) = v;
}

void
generic_space_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{
	*(volatile uint32_t *)(h + o) = v;
}

void
generic_space_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = *addr;
		buf += 2;
	}
}

void
generic_space_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*addr = *(uint16_t *)buf;
		buf += 2;
	}
}

void
generic_space_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = *addr;
		buf += 4;
	}
}

void
generic_space_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(uint32_t *)buf;
		buf += 4;
	}
}
