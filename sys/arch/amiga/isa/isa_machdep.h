/*	$NetBSD: isa_machdep.h,v 1.1 1995/07/21 23:07:17 niklas Exp $	*/

/*
 * Copyright (c) 1995 Niklas Hallqvist
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
 *      This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _ISA_MACHDEP_H_
#define _ISA_MACHDEP_H_

#include <machine/endian.h>

void	isa_insb __P((int port, void *addr, int));
void	isa_outsb __P((int port, void *addr, int));
void	isa_insw __P((int port, void *addr, int));
void	isa_outsw __P((int port, void *addr, int));

/*
 * The link between the ISA device drivers and the bridgecard used.
 */
struct isa_link {
	struct	device *il_dev;
	void	(*il_stb)(struct device *, int, u_char);
	u_char	(*il_ldb)(struct device *, int);
	void	(*il_stw)(struct device *, int, u_short);
	u_short	(*il_ldw)(struct device *, int);
};

extern struct isa_link *isa;
extern struct cfdriver isacd;

static __inline void
stb(addr, val)
	int addr;
	u_char val;
{
	(*isa->il_stb)(isa->il_dev, addr, val);
}

static __inline u_char
ldb(addr)
	int addr;
{
	return (*isa->il_ldb)(isa->il_dev, addr);
}

static __inline void
stw(addr, val)
	int addr;
	u_short val;
{
	(*isa->il_stw)(isa->il_dev, addr, val);
}

static __inline u_short
ldw(addr)
	int addr;
{
	return (*isa->il_ldw)(isa->il_dev, addr);
}

/*
 * Should these be out-of-line instead?  If so, move them to isa.c!
 * How about unaligned word accesses?  Does the '020 allow them?  If not
 * we have to do odd to even moves and vice versa bytewise instead of
 * wordwise.
 */
static __inline void
copy_from_isa (void *from, void *to, int cnt)
{
	int a = (int)from;

	if (a & 1 && cnt) {
		*(u_char *)to = ldb(a++);
		to = ((u_char *)to) + 1;
		cnt--;
	}
	/* Maybe use Duff's device here... */
	while (cnt > 1) {
		*(u_short *)to = ldw(a);
		a += sizeof(u_short);
		to = ((u_short *)to) + 1;
		cnt -= 2;
	}
	if (cnt)
		*(u_char *)to = ldb(a);
}

static __inline void
copy_to_isa (const void *from, void *to, int cnt)
{
	int a = (int)to;

	if (a & 1 && cnt) {
		stb(a++, *(u_char *)from);
		from = ((u_char *)from) + 1;
		cnt--;
	}
	/* Maybe use Duff's device here... */
	while (cnt > 1) {
		stw(a, *(u_short *)from);
		a += sizeof(u_short);
		from = ((u_short *)from) + 1;
		cnt -= 2;
	}
	if (cnt)
		stb(a, *(u_char *)from);
}

static __inline void
zero_isa (void *addr, int cnt)
{
	int a = (int)addr;

	if (a & 1 && cnt) {
		stb(a++, 0);
		cnt--;
	}
	/* Maybe use Duff's device here... */
	while (cnt > 1) {
		stw(a, 0);
		a += sizeof(u_short);
		cnt -= 2;
	}
	if (cnt)
		stb(a, 0);
}

/*
 * These inlines convert shorts from/to isa (intel) byte order to host
 * byte-order.  I know both are exactly equal, but I think it make code
 * more readable to have separate names for them as they indeed have
 * distinctive functionalities.
 */
static __inline u_short
swap(u_short x)
{
	__asm("rolw #8,%0" : "=r" (x) : "0" (x));
	return x;
}

static __inline u_short
itohs(u_short x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	return x;
#else
	return swap(x);
#endif
}

static __inline u_short
htois(u_short x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	return x;
#else
	return swap(x);
#endif
}

/*
 * Given a physical address in the "hole",
 * return a kernel virtual address.
 */
#define	ISA_HOLE_VADDR(p)  ((caddr_t)p)

#endif
