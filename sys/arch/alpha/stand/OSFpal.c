/*	$NetBSD: OSFpal.c,v 1.2 1996/04/12 06:09:30 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/types.h>

#include <machine/prom.h>
#include <machine/rpb.h>

void
OSFpal()
{
	struct rpb *r;
	struct ctb *t;
	struct pcs *p;
	long result;
	int offset;

	r = (struct rpb *)HWRPB_ADDR;
	offset = r->rpb_pcs_size * cpu_number();
	p = (struct pcs *)((u_int8_t *)r + r->rpb_pcs_off + offset);

	printf("VMS PAL revision: 0x%lx\n",
	    p->pcs_palrevisions[PALvar_OpenVMS]);
	printf("OSF PAL rev: 0x%lx\n", p->pcs_palrevisions[PALvar_OSF1]);
	(void)switch_palcode();
	printf("Switch to OSF PAL code succeeded.\n");
}

