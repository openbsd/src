/* $OpenBSD: reloc.h,v 1.1 2003/04/17 03:42:14 drahn Exp $ */

/*
 * Copyright (c) 2002,2003 Dale Rahn
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */
#ifndef  _MACH_RELOC_H 
#define  _MACH_RELOC_H 
#define R_TYPE(X)       __CONCAT(RELOC_,X)


#define RELOC_NONE      0   /* No reloc */
#define RELOC_32        1   /* Direct 32 bit  */
#define RELOC_PC32      2   /* PC relative 32 bit */
#define RELOC_GOT32     3   /* 32 bit GOT entry */
#define RELOC_PLT32     4   /* 32 bit PLT address */
#define RELOC_COPY      5   /* Copy symbol at runtime */
#define RELOC_GLOB_DAT  6   /* Create GOT entry */
#define RELOC_JUMP_SLOT 7   /* Create PLT entry */
#define RELOC_RELATIVE  8   /* Adjust by program base */
#define RELOC_GOTOFF    9   /* 32 bit offset to GOT */
#define RELOC_GOTPC     10   /* 32 bit PC relative offset to GOT */
#define RELOC_16        20 
#define RELOC_PC16      21 
#define RELOC_8         22 
#define RELOC_PC8       23 

#endif /* _MACH_RELOC_H */


