/*	$NetBSD: eisa_machdep.c,v 1.2 1996/04/11 22:15:08 cgd Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

/*
 * Machine-specific functions for EISA autoconfiguration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <i386/isa/icu.h>
#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

void
eisa_attach_hook(parent, self, eba)
	struct device *parent, *self;
	struct eisabus_attach_args *eba;
{

	/* Nothing to do */
}

int
eisa_maxslots(ec)
	eisa_chipset_tag_t ec;
{

	/*
	 * Always try 16 slots.
	 */
	return (16);
}

int
eisa_intr_map(ec, irq, ihp)
	eisa_chipset_tag_t ec;
	u_int irq;
	eisa_intr_handle_t *ihp;
{

	if (irq >= ICU_LEN) {
		printf("eisa_intr_map: bad IRQ %d\n", irq);
		*ihp = -1;
		return 1;
	}
	if (irq == 2) {
		printf("eisa_intr_map: changed IRQ 2 to IRQ 9\n");
		irq = 9;
	}

	*ihp = irq;
	return 0;
}

const char *
eisa_intr_string(ec, ih)
	eisa_chipset_tag_t ec;
	eisa_intr_handle_t ih;
{
	static char irqstr[8];		/* 4 + 2 + NULL + sanity */

	if (ih == 0 || ih >= ICU_LEN || ih == 2)
		panic("eisa_intr_string: bogus handle 0x%x\n", ih);

	sprintf(irqstr, "irq %d", ih);
	return (irqstr);
	
}

void *
eisa_intr_establish(ec, ih, type, level, func, arg, what)
	eisa_chipset_tag_t ec;
	eisa_intr_handle_t ih;
	int type, level, (*func) __P((void *));
	void *arg;
	char *what;
{

	if (ih == 0 || ih >= ICU_LEN || ih == 2)
		panic("eisa_intr_establish: bogus handle 0x%x\n", ih);

	return isa_intr_establish(NULL, ih, type, level, func, arg, what);
}

void
eisa_intr_disestablish(ec, cookie)
	eisa_chipset_tag_t ec;
	void *cookie;
{

	return isa_intr_disestablish(NULL, cookie);
}
