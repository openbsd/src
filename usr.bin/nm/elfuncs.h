/*	$OpenBSD: elfuncs.h,v 1.1 2004/10/09 20:26:57 mickey Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

extern char *stab;

int	elf32_fix_header(Elf32_Ehdr *eh);
Elf32_Shdr*elf32_load_shdrs(const char *, FILE *, off_t, Elf32_Ehdr *);
int	elf32_fix_shdrs(Elf32_Ehdr *eh, Elf32_Shdr *shdr);
int	elf32_fix_phdrs(Elf32_Ehdr *eh, Elf32_Phdr *phdr);
int	elf32_fix_sym(Elf32_Ehdr *eh, Elf32_Sym *sym);
int	elf32_size(Elf32_Ehdr *, Elf32_Shdr *, u_long *, u_long *, u_long *);
int	elf32_symload(const char *, FILE *, off_t, Elf32_Ehdr *, Elf32_Shdr *,
	    struct nlist **, struct nlist ***, size_t *, int *);

int	elf64_fix_header(Elf64_Ehdr *eh);
Elf64_Shdr*elf64_load_shdrs(const char *, FILE *, off_t, Elf64_Ehdr *);
int	elf64_fix_shdrs(Elf64_Ehdr *eh, Elf64_Shdr *shdr);
int	elf64_fix_phdrs(Elf64_Ehdr *eh, Elf64_Phdr *phdr);
int	elf64_fix_sym(Elf64_Ehdr *eh, Elf64_Sym *sym);
int	elf64_size(Elf64_Ehdr *, Elf64_Shdr *, u_long *, u_long *, u_long *);
int	elf64_symload(const char *, FILE *, off_t, Elf64_Ehdr *, Elf64_Shdr *,
	    struct nlist **, struct nlist ***, size_t *, int *);
