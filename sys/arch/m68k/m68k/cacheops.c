/*	$OpenBSD: cacheops.c,v 1.1 1997/07/06 07:46:27 downsj Exp $	*/
/*	$NetBSD: cacheops.c,v 1.1 1997/06/02 20:26:57 leo Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Leo Weppelman
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <m68k/cpu.h>
#include <m68k/cacheops.h>
#include <machine/cpu.h>

#if defined(_MULTI_CPU)

void _TBIA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		TBIA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		TBIA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		TBIA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		TBIA_60();
		break;
#endif
	}
}

void _TBIAS()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		TBIAS_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		TBIAS_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		TBIAS_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		TBIAS_60();
		break;
#endif
	}
}

void _TBIAU()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		TBIAU_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		TBIAU_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		TBIAU_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		TBIAU_60();
		break;
#endif
	}
}

void _ICIA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		ICIA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		ICIA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		ICIA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		ICIA_60();
		break;
#endif
	}
}

void _ICPA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		ICPA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		ICPA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		ICPA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		ICPA_60();
		break;
#endif
	}
}

void _DCIA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCIA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCIA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		DCIA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCIA_60();
		break;
#endif
	}
}

void _DCIS()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCIS_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCIS_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		DCIS_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCIS_60();
		break;
#endif
	}
}

void _DCIU()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCIU_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCIU_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		DCIU_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCIU_60();
		break;
#endif
	}
}

void _PCIA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		PCIA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		PCIA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		PCIA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		PCIA_60();
		break;
#endif
	}
}

void _DCFA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCFA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCFA_30();
		break;
#endif
#ifdef M68040
	case CPU_68040:
		DCFA_40();
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCFA_60();
		break;
#endif
	}
}

void _TBIS(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		TBIS_20(va);
		break;
#endif
#ifdef M68030
	case CPU_68030:
		TBIS_30(va);
		break;
#endif
#ifdef M68040
	case CPU_68040:
		TBIS_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		TBIS_60(va);
		break;
#endif
	}
}

void _DCIAS(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCIAS_20(va);
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCIAS_30(va);
		break;
#endif
#ifdef M68040
	case CPU_68040:
		DCIAS_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCIAS_60(va);
		break;
#endif
	}
}

void _DCPA()
{
	switch (cputype) {
	default:
#ifdef M68020
	case CPU_68020:
		DCPA_20();
		break;
#endif
#ifdef M68030
	case CPU_68030:
		DCPA_30();
		break;
#endif
	}
}

void _ICPL(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		ICPL_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		ICPL_60(va);
		break;
#endif
	}
}

void _ICPP(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		ICPP_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		ICPP_60(va);
		break;
#endif
	}
}

void _DCPL(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		DCPL_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCPL_60(va);
		break;
#endif
	}
}

void _DCPP(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		DCPP_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCPP_60(va);
		break;
#endif
	}
}

void _DCFL(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		DCFL_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCFL_60(va);
		break;
#endif
	}
}

void _DCFP(va)
	vm_offset_t	va;
{
	switch (cputype) {
	default:
#ifdef M68040
	case CPU_68040:
		DCFP_40(va);
		break;
#endif
#ifdef M68060
	case CPU_68060:
		DCFP_60(va);
		break;
#endif
	}
}

#endif /* defined(_TBIA) */
