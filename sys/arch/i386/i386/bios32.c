/*	$NetBSD: bios32.c,v 1.1 1999/11/17 00:55:50 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/*
 * Basic interface to BIOS32 services.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h> 
#include <sys/malloc.h>

#include <dev/isa/isareg.h>
#ifdef __NetBSD__
#include <machine/isa_machdep.h>
#include <machine/bios32.h>
#elif defined(__OpenBSD__)
#include <i386/isa/isa_machdep.h>
#include <machine/biosvar.h>
#endif

#include <machine/segments.h>

#define	BIOS32_START	0xe0000
#define	BIOS32_SIZE	0x20000
#define	BIOS32_END	(BIOS32_START + BIOS32_SIZE - 0x10)

struct bios32_entry bios32_entry;

/*
 * Initialize the BIOS32 interface.
 */
void
bios32_init()
{
	paddr_t entry = 0;
	caddr_t p;
	unsigned char cksum;
	int i;

	for (p = (caddr_t)ISA_HOLE_VADDR(BIOS32_START);
	     p < (caddr_t)ISA_HOLE_VADDR(BIOS32_END);
	     p += 16) {
		if (*(int *)p != BIOS32_MAKESIG('_', '3', '2', '_'))
			continue;

		cksum = 0;
		for (i = 0; i < 16; i++)
			cksum += *(unsigned char *)(p + i);
		if (cksum != 0)
			continue;

		if (*(p + 9) != 1)
			continue;

		entry = *(u_int32_t *)(p + 4);

		printf("BIOS32 rev. %d found at 0x%lx\n",
		    *(p + 8), entry);

		if (entry < BIOS32_START ||
		    entry >= BIOS32_END) {
			printf("BIOS32 entry point outside "
			    "allowable range\n");
			entry = 0;
		}
		break;
	}

	if (entry != 0) {
		bios32_entry.offset = (caddr_t)ISA_HOLE_VADDR(entry);
		bios32_entry.segment = GSEL(GCODE_SEL, SEL_KPL);
	}
}

/*
 * Call BIOS32 to locate the specified BIOS32 service, and fill
 * in the entry point information.
 */
int
bios32_service(service, e, ei)
	u_int32_t service;
	bios32_entry_t e;
	bios32_entry_info_t ei;
{
	u_int32_t eax, ebx, ecx, edx;
	paddr_t entry;

	if (bios32_entry.offset == 0)
		return (0);	/* BIOS32 not present */

	__asm __volatile("lcall (%%edi)"
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		: "0" (service), "1" (0), "D" (&bios32_entry));

	if ((eax & 0xff) != 0)
		return (0);	/* service not found */

	entry = ebx + edx;

	if (entry < BIOS32_START || entry >= BIOS32_END) {
		printf("bios32: entry point for service %c%c%c%c is outside "
		    "allowable range\n",
		    service & 0xff,
		    (service >> 8) & 0xff,
		    (service >> 16) & 0xff,
		    (service >> 24) & 0xff);
		return (0);
	}

	e->offset = (caddr_t)ISA_HOLE_VADDR(entry);
	e->segment = GSEL(GCODE_SEL, SEL_KPL);

	ei->bei_base = ebx;
	ei->bei_size = ecx;
	ei->bei_entry = entry;

	return (1);
}
