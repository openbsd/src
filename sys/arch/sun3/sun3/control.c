/*	$NetBSD: control.c,v 1.12.2.1 1995/11/18 06:56:12 gwr Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 */

#include <sys/param.h>
#include <sys/types.h>

#include <machine/pte.h>
#include <machine/control.h>

#define CONTROL_ALIGN(x) (x & CONTROL_ADDR_MASK)
#define CONTROL_ADDR_BUILD(space, va) (CONTROL_ALIGN(va)|space)

int get_context()
{
	int c;

	c = get_control_byte((char *) CONTEXT_REG);
	return (c & CONTEXT_MASK);
}

void set_context(int c)
{
	set_control_byte((char *) CONTEXT_REG, c & CONTEXT_MASK);
}

vm_offset_t get_pte(va)
	vm_offset_t va;
{
	return (vm_offset_t)
		get_control_word((char *) CONTROL_ADDR_BUILD(PGMAP_BASE, va));
}

void set_pte(va, pte)
	vm_offset_t va, pte;
{
	set_control_word((char *) CONTROL_ADDR_BUILD(PGMAP_BASE, va),
			 (unsigned int) pte);
}

unsigned char get_segmap(va)
	vm_offset_t va;
{
	return get_control_byte((char *) CONTROL_ADDR_BUILD(SEGMAP_BASE, va));
}

void set_segmap(va, sme)
	vm_offset_t va;
	unsigned char sme;
{
	set_control_byte((char *) CONTROL_ADDR_BUILD(SEGMAP_BASE, va), sme);
}

/*
 * Set a segmap entry in all contexts.
 * (i.e. somewhere in kernel space.)
 * XXX - Should optimize:  "(get|set)_control_(word|byte)"
 * calls so this does save/restore of sfc/dfc only once!
 */
void set_segmap_allctx(va, sme)
	vm_offset_t va;
	unsigned char sme;	/* segmap entry */
{
	register char ctx, oldctx;

	/* Inline get_context() */
	oldctx = get_control_byte((char *) CONTEXT_REG);
	oldctx &= CONTEXT_MASK;

	for (ctx = 0; ctx < NCONTEXT; ctx++) {
		/* Inlined set_context() */
		set_control_byte((char *) CONTEXT_REG, ctx);
		/* Inlined set_segmap() */
		set_control_byte((char *) CONTROL_ADDR_BUILD(SEGMAP_BASE, va), sme);
	}

	/* Inlined set_context(ctx); */
	set_control_byte((char *) CONTEXT_REG, oldctx);
}
