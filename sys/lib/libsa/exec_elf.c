/*	$OpenBSD: exec_elf.c,v 1.6 2002/07/09 01:45:47 mickey Exp $	*/

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
#include <ddb/db_aout.h>

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
	register Elf32_Phdr *ph;
	Elf32_Phdr phdr[8];	/* XXX hope this is enough */
	size_t phsize;
	register u_int pa;

#ifdef EXEC_DEBUG
	if (debug)
		printf("id=%x.%c%c%c.%x.%x, type=%x, mach=%x, ver=%x, size=%d\n"
		       "\tep=%x, flgs=%x, ph=%x[%dx%d], sh=%x[%dx%d], str=%x\n",
		       ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2],
		       ehdr->e_ident[3], ehdr->e_ident[4], ehdr->e_ident[5],
		       ehdr->e_type, ehdr->e_machine, ehdr->e_version,
		       ehdr->e_ehsize, ehdr->e_entry, ehdr->e_flags,
		       ehdr->e_phoff, ehdr->e_phnum, ehdr->e_phentsize,
		       ehdr->e_shoff, ehdr->e_shnum, ehdr->e_shentsize,
		       ehdr->e_shstrndx);
#endif

	xp->xp_entry = ehdr->e_entry;

	if (lseek(fd, ehdr->e_phoff, SEEK_SET) <= 0) {
#ifdef EXEC_DEBUG
		if (debug)
			printf("lseek failed (%d)\n", errno);
#endif
		return -1;
	}
	
	phsize = ehdr->e_phnum * ehdr->e_phentsize;
	if (phsize > sizeof(phdr) || read(fd, phdr, phsize) != phsize) {
#ifdef EXEC_DEBUG
		if (debug)
			printf("phdr read failed (%d)\n", errno);
#endif
		return -1;
	}

	pa = 0;
	for (ph = phdr; ph < &phdr[ehdr->e_phnum]; ph++) {
#ifdef EXEC_DEBUG
		if (debug)
			printf("ph%d: type=%x, off=%d, va=%x, fs=%d, ms=%d, "
			       "flags=%x\n", (ph - phdr), ph->p_type,
			       ph->p_offset, ph->p_vaddr, ph->p_filesz,
			       ph->p_memsz, ph->p_flags);
#endif
		if (ph->p_filesz && ph->p_flags & PF_X) {
			pa = ph->p_vaddr;
			xp->text.addr = ph->p_vaddr;
			xp->text.size = ph->p_filesz;
			xp->text.foff = ph->p_offset;
		} else if (ph->p_filesz && ph->p_flags & PF_W) {
			xp->data.addr = ph->p_vaddr;
			xp->data.size = ph->p_filesz;
			xp->data.foff = ph->p_offset;
			if (ph->p_filesz < ph->p_memsz) {
				xp->bss.addr = ph->p_vaddr + ph->p_filesz;
				xp->bss.size = ph->p_memsz - ph->p_filesz;
				xp->bss.foff = 0;
			}
		} else if (!ph->p_filesz) {
			xp->bss.addr = ph->p_vaddr;
			xp->bss.size = ph->p_memsz;
			xp->bss.foff = 0;
		}
	}

	return 0;
}

int
elf_ldsym(fd, xp)
	int fd;
	struct x_param *xp;
{
	register Elf32_Ehdr *ehdr = (Elf32_Ehdr *)xp->xp_hdr;
	Elf32_Sym elfsym;
	Elf32_Shdr shdr[32];	/* XXX hope this is enough */
	register Elf32_Shdr *sh;
	register struct nlist *nl;
	register u_int ss, shsize;

	if (lseek(fd, ehdr->e_shoff, SEEK_SET) <= 0) {
#ifdef DEBUG
		printf("ehdr lseek: %s\n", strerror(errno));
#endif
		return -1;
	}

	/* calc symbols location, scanning section headers */
	shsize = ehdr->e_shnum * ehdr->e_shentsize;
	sh = shdr;

	if (shsize > sizeof(shdr) || read(fd, shdr, shsize) != shsize) {
#ifdef DEBUG
		printf("shdr read: %s\n", strerror(errno));
#endif
		return -1;
	}

	for (sh = shdr; sh < &shdr[ehdr->e_shnum]; sh++) {
#ifdef EXEC_DEBUG
		if (debug)
			printf ("sh%d: type=%x, flags=%x, addr=%x, "
				"foff=%x, sz=%x, link=%x, info=%x, "
				"allign=%x, esz=%x\n", sh - shdr,
				sh->sh_type, sh->sh_flags, sh->sh_addr,
				sh->sh_offset, sh->sh_size, sh->sh_link,
				sh->sh_info, sh->sh_addralign, sh->sh_entsize);
#endif
		if (sh->sh_type == SHT_SYMTAB) {
			xp->sym.addr = xp->bss.addr + xp->bss.size;
			xp->sym.foff = sh->sh_offset;
			xp->sym.size = sh->sh_size;
			if (sh->sh_link && sh->sh_link < ehdr->e_shnum &&
			    shdr[sh->sh_link].sh_type == SHT_STRTAB) {
				xp->str.foff = shdr[sh->sh_link].sh_offset;
				xp->str.size = shdr[sh->sh_link].sh_size;
			}
		}
	}

	if (!xp->sym.size || !xp->str.size)
		return 0;

	if (lseek(fd, xp->sym.foff, SEEK_SET) <= 0) {
#ifdef DEBUG
		printf("syms lseek: %s\n", strerror(errno));
#endif
		return -1;
	}

	nl = (struct nlist *)((long *)xp->xp_end + 1);
	for (ss = xp->sym.size; ss >= sizeof(elfsym);
	     ss -= sizeof(elfsym), nl++) {

		if (read(fd, &elfsym, sizeof(elfsym)) != sizeof(elfsym)) {
#ifdef DEBUG
			printf ("read elfsym: %s\n", strerror(errno));
#endif
			return -1;
		}
		nl->n_un.n_strx = (long)elfsym.st_name + sizeof(int);
		nl->n_value = elfsym.st_value;
		nl->n_desc = 0;
		nl->n_other = 0;
		switch (ELF32_ST_TYPE(elfsym.st_info)) {
		case STT_FILE:
			nl->n_type = N_FN;
			break;
		case STT_FUNC:
			nl->n_type = N_TEXT;
			break;
		case STT_OBJECT:
			nl->n_type = N_DATA;
			break;
		case STT_NOTYPE:
			if (elfsym.st_shndx == SHN_UNDEF) {
				nl->n_type = N_UNDF;
				break;
			} else if (elfsym.st_shndx == SHN_ABS) {
				nl->n_type = N_ABS;
				break;
			} else if (shdr[elfsym.st_shndx - 1].sh_type ==
				   SHT_NULL) {
				/* XXX this is probably bogus */
				nl->n_type = N_ABS;
				break;
			} else if (shdr[elfsym.st_shndx - 1].sh_type ==
				   SHT_PROGBITS) {
				/* XXX this is probably bogus */
				nl->n_type = N_BSS;
				break;
			}
#ifdef EXEC_DEBUG
			else
				printf ("sec[%d]=0x%x,val=0x%lx\n",
					elfsym.st_shndx,
					shdr[elfsym.st_shndx - 1].sh_type,
					nl->n_value);
#endif
		case STT_LOPROC:
		case STT_HIPROC:
		case STT_SECTION:
			nl--;
			continue;

		default:
#ifdef DEBUG
			printf ("elf_ldsym: unknown type %d\n",
				ELF32_ST_TYPE(elfsym.st_info));
#endif
			nl--;
			continue;
		}
		switch (ELF32_ST_BIND(elfsym.st_info)) {
		case STB_WEAK:
		case STB_GLOBAL:
			nl->n_type |= N_EXT;
			break;
		case STB_LOCAL:
			break;
		default:
#ifdef DEBUG
			printf ("elf_ldsym: unknown bind %d\n",
				ELF32_ST_BIND(elfsym.st_info));
#endif
			break;
		}
	}

	printf (" [%d", (char *)nl - (char *)xp->xp_end);
	*(long *)xp->xp_end = (char *)nl - (char *)xp->xp_end - sizeof(long);

	if (lseek(fd, xp->str.foff, SEEK_SET) <= 0) {
#ifdef DEBUG
		printf("strings lseek: %s\n", strerror(errno));
#endif
		return -1;
	}

	*((int *)nl)++ = xp->str.size + sizeof(int);
	if (read(fd, nl, xp->str.size) != xp->str.size) {
#ifdef DEBUG
		printf ("read strings: %s\n", strerror(errno));
#endif
		return -1;
	}

	printf ("+%d]", xp->str.size);

	xp->xp_end = ((u_int)nl + xp->str.size + 3) & ~3;

	return 0;
}

