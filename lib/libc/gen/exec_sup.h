/*	$OpenBSD: exec_sup.h,v 1.1 1996/05/28 14:11:21 etheisen Exp $	*/
/*
 * Copyright (c) 1995, 1996 Erik Theisen
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

#ifndef _EXEC_SUP_H_
#define _EXEC_SUP_H_

#define DO_AOUT                 /* Always do a.out */

#if defined (__i386__) || defined (__mips__)
#define DO_ELF
#define ELF_TARG_VER            1 /* The ver for which this code is intended */
#include <elf_abi.h>
int __elf_is_okay__ (Elf32_Ehdr *ehdr); /* XXX - should this be hidden??? */
#endif /* ELF machines */

#if defined (__i386__)
  #define ELF_TARG_CLASS	ELFCLASS32
  #define ELF_TARG_DATA		ELFDATA2LSB
  #define ELF_TARG_MACH		EM_386

#elif defined (__mips__)
  #define ELF_TARG_CLASS	ELFCLASS32
  #define ELF_TARG_DATA		ELFDATA2LSB
  #define ELF_TARG_MACH		EM_MIPS

#elif defined (__alpha__)
  #define DO_ECOFF

#elif defined (pica)
  #define DO_ECOFF

#endif /* Machines */

#ifdef DO_AOUT
#include <sys/types.h>
#include <a.out.h>
#endif

#ifdef DO_ECOFF
#include <sys/exec_ecoff.h>
#endif

#endif /* _EXEC_SUP_H_ */
