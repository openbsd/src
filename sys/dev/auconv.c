/*	$NetBSD: auconv.c,v 1.3 1999/11/01 18:12:19 augustss Exp $	*/

/*
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/audioio.h>
#include <dev/auconv.h>

void
change_sign8(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while (--cc >= 0) {
		*p ^= 0x80;
		++p;
	}
}

void
change_sign16_le(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while ((cc -= 2) >= 0) {
		p[1] ^= 0x80;
		p += 2;
	}
}

void
change_sign16_be(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	while ((cc -= 2) >= 0) {
		p[0] ^= 0x80;
		p += 2;
	}
}

void
swap_bytes(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char t;

	while ((cc -= 2) >= 0) {
		t = p[0];
		p[0] = p[1];
		p[1] = t;
		p += 2;
	}
}

void
swap_bytes_change_sign16_le(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char t;

	while ((cc -= 2) >= 0) {
		t = p[1];
		p[1] = p[0] ^ 0x80;
		p[0] = t;
		p += 2;
	}
}

void
swap_bytes_change_sign16_be(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char t;

	while ((cc -= 2) >= 0) {
		t = p[0];
		p[0] = p[1] ^ 0x80;
		p[1] = t;
		p += 2;
	}
}

void
change_sign16_swap_bytes_le(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	swap_bytes_change_sign16_be(v, p, cc);
}

void
change_sign16_swap_bytes_be(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	swap_bytes_change_sign16_le(v, p, cc);
}

void
linear8_to_linear16_le(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char *q = p;

	p += cc;
	q += cc * 2;
	while (--cc >= 0) {
		q -= 2;
		q[1] = *--p;
		q[0] = 0;
	}
}

void
linear8_to_linear16_be(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char *q = p;

	p += cc;
	q += cc * 2;
	while (--cc >= 0) {
		q -= 2;
		q[0] = *--p;
		q[1] = 0;
	}
}

void
linear16_to_linear8_le(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char *q = p;

	while ((cc -= 2) >= 0) {
		*q++ = p[1];
		p += 2;
	}
}

void
linear16_to_linear8_be(v, p, cc)
	void *v;
	u_char *p;
	int cc;
{
	u_char *q = p;

	while ((cc -= 2) >= 0) {
		*q++ = p[0];
		p += 2;
	}
}
