/*	$OpenBSD: cacheops.h,v 1.1 1997/07/06 07:46:23 downsj Exp $	*/
/*	$NetBSD: cacheops.h,v 1.1 1997/06/02 20:26:37 leo Exp $	*/

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

#if notyet /* XXX */
#include <machine/cpuconf.h>
#endif

#include <m68k/cacheops_20.h>
#include <m68k/cacheops_30.h>
#include <m68k/cacheops_40.h>
#include <m68k/cacheops_60.h>

#if defined(M68020) && !(defined(M68030)||defined(M68040)||defined(M68060))

#define	TBIA()		TBIA_20()
#define	TBIS(va)	TBIS_20((va))
#define	TBIAS()		TBIAS_20()
#define	TBIAU()		TBIAU_20()
#define	ICIA()		ICIA_20()
#define	ICPA()		ICPA_20()
#define	DCIA()		DCIA_20()
#define	DCIS()		DCIS_20()
#define	DCIU()		DCIU_20()
#define	DCIAS()		DCIAS_20()
#define	PCIA()		PCIA_20()

#elif defined(M68030) && !(defined(M68020)||defined(M68040)||defined(M68060))

#define	TBIS(va)	TBIS_30((va))
#define	TBIAS()		TBIAS_30()
#define	TBIAU()		TBIAU_30()
#define	ICIA()		ICIA_30()
#define	ICPA()		ICPA_30()
#define	DCIA()		DCIA_30()
#define	DCIS()		DCIS_30()
#define	DCIU()		DCIU_30()
#define	DCIAS()		DCIAS_30()
#define	PCIA()		PCIA_30()

#elif defined(M68040) && !(defined(M68020)||defined(M68030)||defined(M68060))

#define	TBIA()		TBIA_40()
#define	TBIS(va)	TBIS_40((va))
#define	TBIAS()		TBIAS_40()
#define	TBIAU()		TBIAU_40()
#define	ICIA()		ICIA_40()
#define	ICPA()		ICPA_40()
#define	DCIA()		DCIA_40()
#define	DCIS()		DCIS_40()
#define	DCIU()		DCIU_40()
#define	DCIAS(va)	DCIAS_40((va))
#define	PCIA()		PCIA_40()
#define	ICPL(va)	ICPL_40((va))
#define	ICPP(va)	ICPP_40((va))
#define	DCPL(va)	DCPL_40((va))
#define	DCPP(va)	DCPP_40((va))
#define	DCPA()		DCPA_40()
#define	DCFL(va)	DCFL_40((va))
#define	DCFP(va)	DCFP_40((va))

#elif defined(M68060) && !(defined(M68020)||defined(M68030)||defined(M68040))

#define	TBIA()		TBIA_60()
#define	TBIS(va)	TBIS_60((va))
#define	TBIAS()		TBIAS_60()
#define	TBIAU()		TBIAU_60()
#define	ICIA()		ICIA_60()
#define	ICPA()		ICPA_60()
#define	DCIA()		DCIA_60()
#define	DCIS()		DCIS_60()
#define	DCIU()		DCIU_60()
#define	DCIAS(va)	DCIAS_60((va))
#define	PCIA()		PCIA_60()
#define	ICPL(va)	ICPL_60((va))
#define	ICPP(va)	ICPP_60((va))
#define	DCPL(va)	DCPL_60((va))
#define	DCPP(va)	DCPP_60((va))
#define	DCPA()		DCPA_60()
#define	DCFL(va)	DCFL_60((va))
#define	DCFP(va)	DCFP_60((va))

#else /* Multi-CPU config */

/* XXX: From cpuconf.h? */
#ifndef _MULTI_CPU
#define	_MULTI_CPU
#endif

void	_TBIA __P((void));
void	_TBIS __P((vm_offset_t));
void	_TBIAS __P((void));
void	_TBIAU __P((void));
void	_ICIA __P((void));
void	_ICPA __P((void));
void	_DCIA __P((void));
void	_DCIS __P((void));
void	_DCIU __P((void));
void	_DCIAS __P((vm_offset_t));

#define	TBIA()		_TBIA()
#define	TBIS(va)	_TBIS((va))
#define	TBIAS()		_TBIAS()
#define	TBIAU()		_TBIAU()
#define	ICIA()		_ICIA()
#define	ICPA()		_ICPA()
#define	DCIA()		_DCIA()
#define	DCIS()		_DCIS()
#define	DCIU()		_DCIU()
#define	DCIAS(va)	_DCIAS((va))

#if defined(M68040)||defined(M68060)

void	_PCIA __P((void));
void	_DCFA __P((void));
void	_ICPL __P((vm_offset_t));
void	_ICPP __P((vm_offset_t));
void	_DCPL __P((vm_offset_t));
void	_DCPP __P((vm_offset_t));
void	_DCPA __P((void));
void	_DCFL __P((vm_offset_t));
void	_DCFP __P((vm_offset_t));

#define	PCIA()		_PCIA()
#define	DCFA()		_DCFA()
#define	ICPL(va)	_ICPL((va))
#define	ICPP(va)	_ICPP((va))
#define	DCPL(va)	_DCPL((va))
#define	DCPP(va)	_DCPP((va))
#define	DCPA()		_DCPA()
#define	DCFL(va)	_DCFL((va))
#define	DCFP(va)	_DCFP((va))

#endif /* defined(M68040)||defined(M68060) */

#endif
