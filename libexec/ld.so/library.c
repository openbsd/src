/*	$OpenBSD: library.c,v 1.60 2010/11/16 18:59:00 drahn Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 *
 */

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dl_prebind.h"

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"
#include "sod.h"

#define PFLAGS(X) ((((X) & PF_R) ? PROT_READ : 0) | \
		   (((X) & PF_W) ? PROT_WRITE : 0) | \
		   (((X) & PF_X) ? PROT_EXEC : 0))

void
_dl_load_list_free(struct load_list *load_list)
{
	struct load_list *next;

	while (load_list != NULL) {
		next = load_list->next;
		_dl_free(load_list);
		load_list = next;
	}
}

void
_dl_unload_shlib(elf_object_t *object)
{
	struct dep_node *n;
	DL_DEB(("unload_shlib called on %s\n", object->load_name));
	if (OBJECT_REF_CNT(object) == 0 &&
	    (object->status & STAT_UNLOADED) == 0) {
		object->status |= STAT_UNLOADED;
		TAILQ_FOREACH(n, &object->child_list, next_sib)
			_dl_unload_shlib(n->data);
		TAILQ_FOREACH(n, &object->grpref_list, next_sib)
			_dl_unload_shlib(n->data);
		DL_DEB(("unload_shlib unloading on %s\n", object->load_name));
		_dl_load_list_free(object->load_list);
		_dl_munmap((void *)object->load_base, object->load_size);
		_dl_remove_object(object);
	}
}

elf_object_t *
_dl_tryload_shlib(const char *libname, int type, int flags)
{
	int	libfile, i;
	struct load_list *next_load, *load_list = NULL;
	Elf_Addr maxva = 0, minva = ELFDEFNNAME(NO_ADDR);
	Elf_Addr libaddr, loff, align = _dl_pagesz - 1;
	elf_object_t *object;
	char	hbuf[4096];
	Elf_Dyn *dynp = 0;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdp;
	struct stat sb;
	void *prebind_data;

#define ROUND_PG(x) (((x) + align) & ~(align))
#define TRUNC_PG(x) ((x) & ~(align))

	libfile = _dl_open(libname, O_RDONLY);
	if (libfile < 0) {
		_dl_errno = DL_CANT_OPEN;
		return(0);
	}

	if ( _dl_fstat(libfile, &sb) < 0) {
		_dl_errno = DL_CANT_OPEN;
		return(0);
	}

	for (object = _dl_objects; object != NULL; object = object->next) {
		if (object->dev == sb.st_dev &&
		    object->inode == sb.st_ino) {
			object->obj_flags |= flags & RTLD_GLOBAL;
			_dl_close(libfile);
			if (_dl_loading_object == NULL)
				_dl_loading_object = object;
			if (object->load_object != _dl_objects &&
			    object->load_object != _dl_loading_object) {
				_dl_link_grpref(object->load_object,
				    _dl_loading_object);
			}
			return(object);
		}
	}

	_dl_read(libfile, hbuf, sizeof(hbuf));
	ehdr = (Elf_Ehdr *)hbuf;
	if (ehdr->e_ident[0] != ELFMAG0  || ehdr->e_ident[1] != ELFMAG1 ||
	    ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3 ||
	    ehdr->e_type != ET_DYN || ehdr->e_machine != MACHID) {
		_dl_close(libfile);
		_dl_errno = DL_NOT_ELF;
		return(0);
	}

	/*
	 *  Alright, we might have a winner!
	 *  Figure out how much VM space we need.
	 */
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
		case PT_LOAD:
			if (phdp->p_vaddr < minva)
				minva = phdp->p_vaddr;
			if (phdp->p_vaddr + phdp->p_memsz > maxva)
				maxva = phdp->p_vaddr + phdp->p_memsz;
			break;
		case PT_DYNAMIC:
			dynp = (Elf_Dyn *)phdp->p_vaddr;
			break;
		default:
			break;
		}
	}
	minva = TRUNC_PG(minva);
	maxva = ROUND_PG(maxva);

	/*
	 * We map the entire area to see that we can get the VM
	 * space required. Map it unaccessible to start with.
	 *
	 * We must map the file we'll map later otherwise the VM
	 * system won't be able to align the mapping properly
	 * on VAC architectures.
	 */
	libaddr = (Elf_Addr)_dl_mmap(0, maxva - minva, PROT_NONE,
	    MAP_PRIVATE|MAP_FILE, libfile, 0);
	if (_dl_mmap_error(libaddr)) {
		_dl_printf("%s: rtld mmap failed mapping %s.\n",
		    _dl_progname, libname);
		_dl_close(libfile);
		_dl_errno = DL_CANT_MMAP;
		return(0);
	}

	loff = libaddr - minva;
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		if (phdp->p_type == PT_LOAD) {
			char *start = (char *)(TRUNC_PG(phdp->p_vaddr)) + loff;
			Elf_Addr off = (phdp->p_vaddr & align);
			Elf_Addr size = off + phdp->p_filesz;
			void *res;

			res = _dl_mmap(start, ROUND_PG(size),
			    PFLAGS(phdp->p_flags),
			    MAP_FIXED|MAP_PRIVATE, libfile,
			    TRUNC_PG(phdp->p_offset));
			next_load = _dl_malloc(sizeof(struct load_list));
			next_load->next = load_list;
			load_list = next_load;
			next_load->start = start;
			next_load->size = size;
			next_load->prot = PFLAGS(phdp->p_flags);
			if (_dl_mmap_error(res)) {
				_dl_printf("%s: rtld mmap failed mapping %s.\n",
				    _dl_progname, libname);
				_dl_close(libfile);
				_dl_errno = DL_CANT_MMAP;
				_dl_munmap((void *)libaddr, maxva - minva);
				_dl_load_list_free(load_list);
				return(0);
			}
			if (phdp->p_flags & PF_W) {
				/* Zero out everything past the EOF */
				if ((size & align) != 0)
					_dl_memset(start + size, 0,
					    _dl_pagesz - (size & align));
				if (ROUND_PG(size) ==
				    ROUND_PG(off + phdp->p_memsz))
					continue;
				start = start + ROUND_PG(size);
				size = ROUND_PG(off + phdp->p_memsz) -
				    ROUND_PG(size);
				res = _dl_mmap(start, size,
				    PFLAGS(phdp->p_flags),
				    MAP_FIXED|MAP_PRIVATE|MAP_ANON, -1, 0);
				if (_dl_mmap_error(res)) {
					_dl_printf("%s: rtld mmap failed mapping %s.\n",
					    _dl_progname, libname);
					_dl_close(libfile);
					_dl_errno = DL_CANT_MMAP;
					_dl_munmap((void *)libaddr, maxva - minva);
					_dl_load_list_free(load_list);
					return(0);
				}
			}
		}
	}

	prebind_data = prebind_load_fd(libfile, libname);

	_dl_close(libfile);

	dynp = (Elf_Dyn *)((unsigned long)dynp + loff);
	object = _dl_finalize_object(libname, dynp,
	    (Elf_Phdr *)((char *)libaddr + ehdr->e_phoff), ehdr->e_phnum,type,
	    libaddr, loff);
	if (object) {
		object->prebind_data = prebind_data;
		object->load_size = maxva - minva;	/*XXX*/
		object->load_list = load_list;
		/* set inode, dev from stat info */
		object->dev = sb.st_dev;
		object->inode = sb.st_ino;
		object->obj_flags |= flags;
		_dl_build_sod(object->load_name, &object->sod);
	} else {
		/* XXX not possible. object cannot come back NULL */
		_dl_munmap((void *)libaddr, maxva - minva);
		_dl_load_list_free(load_list);
	}
	return(object);
}
