/*	$OpenBSD: elf_abi.h,v 1.2 1999/01/27 04:46:05 imp Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MIPS_ELF_ABI_H
#define _MIPS_ELF_ABI_H

/* From MIPS ABI supplemental */

/* Architecture dependent Segment types - p_type */ 
#define PT_MIPS_REGINFO 0x70000000      /* Register usage information */

/* Architecture dependent d_tag field for Elf32_Dyn.  */
#define DT_MIPS_RLD_VERSION  0x70000001 /* Runtime Linker Interface ID */
#define DT_MIPS_TIME_STAMP   0x70000002 /* Timestamp */
#define DT_MIPS_ICHECKSUM    0x70000003 /* Cksum of ext. str. and com. sizes */
#define DT_MIPS_IVERSION     0x70000004 /* Version string (string tbl index) */
#define DT_MIPS_FLAGS        0x70000005 /* Flags */
#define DT_MIPS_BASE_ADDRESS 0x70000006 /* Segment base address */
#define DT_MIPS_CONFLICT     0x70000008 /* Adr of .conflict section */
#define DT_MIPS_LIBLIST      0x70000009 /* Address of .liblist section */
#define DT_MIPS_LOCAL_GOTNO  0x7000000a /* Number of local .GOT entries */
#define DT_MIPS_CONFLICTNO   0x7000000b /* Number of .conflict entries */
#define DT_MIPS_LIBLISTNO    0x70000010 /* Number of .liblist entries */
#define DT_MIPS_SYMTABNO     0x70000011 /* Number of .dynsym entries */
#define DT_MIPS_UNREFEXTNO   0x70000012 /* First external DYNSYM */
#define DT_MIPS_GOTSYM       0x70000013 /* First GOT entry in .dynsym */
#define DT_MIPS_HIPAGENO     0x70000014 /* Number of GOT page table entries */
#define DT_MIPS_RLD_MAP      0x70000016 /* Address of debug map pointer */

#define DT_PROCNUM (DT_MIPS_HIPAGENO - DT_LOPROC + 1)

#endif /* _MIPS_ELF_ABI_H */
