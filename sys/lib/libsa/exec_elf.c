/*	$OpenBSD: exec_elf.c,v 1.2 1998/07/14 14:56:05 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
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

#include "libsa.h"
#include <lib/libsa/exec.h>
#include <sys/exec_elf.h>

int
elf_probe(fd, hdr)
	int fd;
	union x_header *hdr;
{
	return IS_ELF(hdr->x_elf);
}

int
elf_load(fd, xp)
	int fd;
	struct x_param *xp;
{
	register Elf32_Ehdr *ehdr = (Elf32_Ehdr *)xp->xp_hdr;
	register Elf32_Phdr *phdr, *ph;
	register Elf32_Shdr *shdr, *sh;
	size_t phsize, shsize;
	u_int pa;

	xp->xp_entry = ehdr->e_entry;

	if (lseek(fd, ehdr->e_phoff, SEEK_SET) <= 0)
		return -1;
	
	phsize = ehdr->e_phnum * ehdr->e_phentsize;
	phdr = alloca(phsize);

	if (read(fd, phdr, phsize) != phsize)
		return -1;

	pa = 0;
	for (ph = phdr; ph < &phdr[ehdr->e_phnum]; ph++) {
		if (ntohl(ph->p_filesz) && ntohl(ph->p_flags) & PF_X) {
			pa = ph->p_vaddr;
			xp->text.addr = 0;
			xp->text.size = ph->p_filesz;
			xp->text.foff = ph->p_offset;
		} else if (ntohl(ph->p_filesz) && ntohl(ph->p_flags) & PF_W) {
			xp->data.addr = ph->p_vaddr - pa;
			xp->data.size = ph->p_filesz;
			xp->data.foff = ph->p_offset;
		} else if (!ntohl(ph->p_filesz)) {
			xp->bss.addr = ph->p_vaddr - pa;
			xp->bss.size = ph->p_memsz;
			xp->bss.foff = 0;
		}
	}

	if (lseek(fd, ehdr->e_shoff, SEEK_SET))
		return -1;

	shsize = ehdr->e_shnum * ehdr->e_shentsize;
	shdr = (Elf32_Shdr *) alloca(shsize);

	if (read(fd, shdr, shsize) != shsize)
		return -1;

	for (sh = shdr; sh < &shdr[ehdr->e_shnum]; sh++) {
		switch (sh->sh_type) {
		case SHT_SYMTAB:
			xp->sym.foff = sh->sh_offset;
			xp->sym.size = sh->sh_size;
		case SHT_STRTAB:
			xp->str.foff = sh->sh_offset;
			xp->str.size = sh->sh_size;
		}
		break;
	}

	return 0;
}

