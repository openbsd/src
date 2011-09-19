/*	$OpenBSD: findcpu.c,v 1.16 2011/09/19 21:53:02 miod Exp $	*/
/*	$NetBSD: findcpu.c,v 1.5 1999/08/23 19:10:43 ragge Exp $	*/
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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


#include <sys/param.h>
#include <sys/device.h>

#include <machine/sid.h>
#include <machine/nexus.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>
#include <machine/cvax.h>

/* 
 * We set up some information about the machine we're
 * running on and thus initializes/uses vax_cputype and vax_boardtype.
 * There should be no need to change/reinitialize these variables
 * outside of this routine, they should be read only!
 */
int vax_cputype;	/* highest byte of SID register */
int vax_cpustype;	/* second byte of SIE register */
int vax_bustype;	/* holds/defines the main bus type on this machine */
int vax_boardtype;	/* machine dependent, combination of SID and SIE */
 
int vax_cpudata = 0;	/* contents of the SID register */
int vax_siedata = 0;	/* contents of the SIE register */
int vax_confdata;	/* machine dependent, configuration/setup data */

void	findcpu(void);

/*
 * Try to figure out which type of system this is.
 */
void
findcpu(void)
{
	vax_cpudata = mfpr(PR_SID);
	vax_cputype = vax_cpudata >> 24;
	vax_boardtype = vax_cputype << 24;

	switch (vax_cputype) {
	case VAX_TYP_UV2:
	case VAX_TYP_CVAX:
	case VAX_TYP_RIGEL:
	case VAX_TYP_MARIAH:
	case VAX_TYP_NVAX:
	case VAX_TYP_SOC:
		vax_siedata = *(int *)(0x20040004);	/* SIE address */
		vax_cpustype = (vax_siedata >> 8) & 0xff;
		vax_boardtype |= vax_siedata >> 24;

		switch (vax_boardtype) {
		case VAX_BTYP_420: /* They are very similar */
		case VAX_BTYP_410:
		case VAX_BTYP_43:
		case VAX_BTYP_46:
		case VAX_BTYP_48:
		case VAX_BTYP_IS1:	
			vax_confdata = *(int *)(0x20020000);
			vax_bustype = VAX_VSBUS;
			break;
		case VAX_BTYP_49:
			vax_confdata = *(int *)(0x25800000);
			vax_bustype = VAX_VSBUS;
			break;
		case VAX_BTYP_VXT:
			vax_confdata = *(int *)(0x200c0000);
			vax_bustype = VAX_VXTBUS;
			break;

		case VAX_BTYP_60:
			vax_confdata =
			    ((struct cvax_ssc *)CVAX_SSC)->ssc_terminfo;
			vax_bustype = VAX_MBUS;
			break;

		case VAX_BTYP_630:
		case VAX_BTYP_650:
		case VAX_BTYP_660:
		case VAX_BTYP_670:
		case VAX_BTYP_1301:
		case VAX_BTYP_1303:
		case VAX_BTYP_1305:
			vax_bustype = VAX_IBUS;
			break;

		default:
			/* CPU not supported, just give up */
			asm("halt");
		}
		break;

	default:
		/* CPU not supported, just give up */
		asm("halt");
	}
}
