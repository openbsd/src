/*	$OpenBSD: elfuncs.h,v 1.6 2018/12/14 19:56:02 guenther Exp $	*/

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

Elf32_Shdr*elf32_load_shdrs(const char *, FILE *, off_t, Elf32_Ehdr *);
int	elf32_size(Elf32_Ehdr *, Elf32_Shdr *, u_long *, u_long *, u_long *);
int	elf32_symload(const char *, FILE *, off_t, Elf32_Ehdr *, Elf32_Shdr *,
	    struct xnlist **, struct xnlist ***, size_t *, int *);

Elf64_Shdr*elf64_load_shdrs(const char *, FILE *, off_t, Elf64_Ehdr *);
int	elf64_size(Elf64_Ehdr *, Elf64_Shdr *, u_long *, u_long *, u_long *);
int	elf64_symload(const char *, FILE *, off_t, Elf64_Ehdr *, Elf64_Shdr *,
	    struct xnlist **, struct xnlist ***, size_t *, int *);
