/*	$OpenBSD: iop.c,v 1.1 2006/01/22 13:53:16 miod Exp $	*/
/*	$NetBSD: iop.c,v 1.10 2005/12/24 23:24:00 perry Exp $	*/

/*
 * Copyright (c) 2000 Allen Briggs.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/iop.h>
#include <machine/viareg.h>

IOPHW	*mac68k_iops[2];

void
iop_serial_compatible()
{
	IOPHW *ioph;

	switch (current_mac_model->machineid) {
	case MACH_MACQ900:
	case MACH_MACQ950:
		mac68k_iops[SCC_IOP] = (IOPHW *)((u_char *)IOBase +  0xc000);
		mac68k_iops[ISM_IOP] = (IOPHW *)((u_char *)IOBase + 0x1e000);
		break;
	case MACH_MACIIFX:
		mac68k_iops[SCC_IOP] = (IOPHW *)((u_char *)IOBase +  0x4000);
		mac68k_iops[ISM_IOP] = (IOPHW *)((u_char *)IOBase + 0x12000);
		break;
	default:
		return;
	}       

	ioph = mac68k_iops[SCC_IOP];
	ioph->control_status = 0;		/* Reset */
	ioph->control_status = IOP_BYPASS;	/* Set to bypass */

	ioph = mac68k_iops[ISM_IOP];
	ioph->control_status = 0;		/* Reset */
}
