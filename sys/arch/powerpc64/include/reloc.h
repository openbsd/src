/*	$OpenBSD: reloc.h,v 1.1 2020/06/25 04:21:25 drahn Exp $	*/
/*	$NetBSD: reloc.h,v 1.1 1996/09/30 16:34:33 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_POWERPC64_RELOC_H_
#define	_POWERPC64_RELOC_H_

/*
 * Quite a number of relocation types
 */
enum reloc_type {
	RELOC_NONE,
	RELOC_32,
	RELOC_24,
	RELOC_16,
	RELOC_16_LO,
	RELOC_16_HI,	/* RELOC_ADDIS = 5 */
	RELOC_16_HA,
	RELOC_14,
	RELOC_14_TAKEN,
	RELOC_14_NTAKEN,
	RELOC_REL24,	/* RELOC_BRANCH = 10 */
	RELOC_REL14,
	RELOC_REL14_TAKEN,
	RELOC_REL14_NTAKEN,
	RELOC_GOT16,
	RELOC_GOT16_LO,
	RELOC_GOT16_HI,
	RELOC_GOT16_HA,
	RELOC_PLT24,
	RELOC_COPY,
	RELOC_GLOB_DAT,
	RELOC_JMP_SLOT,
	RELOC_RELATIVE,
	RELOC_LOCAL24PC,
	RELOC_U32,
	RELOC_U16,
	RELOC_REL32,
	RELOC_PLT32,
	RELOC_PLTREL32,
	RELOC_PLT16_LO,
	RELOC_PLT16_HI,
	RELOC_PLT16_HA,
	RELOC_SDAREL, /* ABI defines this as 32nd entry, but we ignore this, at least for now */
	RELOC_SECTOFF,
	RELOC_SECTOFF_LO,
	RELOC_SECTOFF_HI,
	RELOC_SECTOFF_HA,
	RELOC_ADDR30,
	RELOC_ADDR64,
	RELOC_ADDR16_HIGHER,
	RELOC_ADDR16_HIGHERA,
	RELOC_ADDR16_HIGHEST,
	RELOC_ADDR16_HIGHESTA,
	RELOC_UADDR64,
	RELOC_REL64,
	RELOC_PLT64,
	RELOC_PLTREL64,
	RELOC_TOC16,
	RELOC_TOC16_LO,
	RELOC_TOC16_HI,
	RELOC_TOC16_HA,
	RELOC_TOC,
	RELOC_PLTGOT16,
	RELOC_PLTGOT16_LO,
	RELOC_PLTGOT16_HI,
	RELOC_PLTGOT16_HA,
	RELOC_ADDR16_DS,
	RELOC_ADDR16_LO_DS,
	RELOC_GOT16_DS,
	RELOC_GOT16_LO_DS,
	RELOC_PLT16_LO_DS,
	RELOC_SECTOFF_DS,
	RELOC_SECTOFF_LO_DS,
	RELOC_TOC16_DS,
	RELOC_TOC16_LO_DS,
	RELOC_PLTGOT16_DS,
	RELOC_PLTGOT16_LO_DS,

	RELOC_TLSC = 67,
	RELOC_DTPMOD32,
	RELOC_TPREL16,
	RELOC_TPREL16_LO,
	RELOC_TPREL16_HI,
	RELOC_TPREL16_HA,
	RELOC_TPREL32,
	RELOC_DTPREL16,
	RELOC_DTPREL16_LO,
	RELOC_DTPREL16_HI,
	RELOC_DTPREL16_HA,
	RELOC_DTPREL32,
	RELOC_MAX
};

#endif	/* _POWERPC64_RELOC_H_ */
