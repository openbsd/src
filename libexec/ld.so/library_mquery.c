/*	$OpenBSD: library_mquery.c,v 1.48 2015/01/16 16:18:07 deraadt Exp $ */

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
	Elf_Addr align = _dl_pagesz - 1;

	while (load_list != NULL) {
		if (load_list->start != NULL)
			_dl_munmap(load_list->start,
			    ((load_list->size) + align) & ~align);
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
		_dl_remove_object(object);
	}
}


elf_object_t *
_dl_tryload_shlib(const char *libname, int type, int flags)
{
	int libfile, i;
	struct load_list *ld, *lowld = NULL;
	elf_object_t *object;
	Elf_Dyn *dynp = 0;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdp;
	Elf_Addr load_end = 0;
	Elf_Addr align = _dl_pagesz - 1, off, size;
	struct stat sb;
	void *prebind_data;
	char hbuf[4096];

#define ROUND_PG(x) (((x) + align) & ~(align))
#define TRUNC_PG(x) ((x) & ~(align))

	libfile = _dl_open(libname, O_RDONLY | O_CLOEXEC);
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
			object->obj_flags |= flags & DF_1_GLOBAL;
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

	/* Insertion sort */
#define LDLIST_INSERT(ld) do { \
	struct load_list **_ld; \
	for (_ld = &lowld; *_ld != NULL; _ld = &(*_ld)->next) \
		if ((*_ld)->moff > ld->moff) \
			break; \
	ld->next = *_ld; \
	*_ld = ld; \
} while (0)
	/*
	 *  Alright, we might have a winner!
	 *  Figure out how much VM space we need and set up the load
	 *  list that we'll use to find free VM space.
	 */
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
		case PT_LOAD:
			off = (phdp->p_vaddr & align);
			size = off + phdp->p_filesz;

			if (size != 0) {
				ld = _dl_malloc(sizeof(struct load_list));
				if (ld == NULL)
					_dl_exit(7);
				ld->start = NULL;
				ld->size = size;
				ld->moff = TRUNC_PG(phdp->p_vaddr);
				ld->foff = TRUNC_PG(phdp->p_offset);
				ld->prot = PFLAGS(phdp->p_flags);
				LDLIST_INSERT(ld);
			}

			if ((PFLAGS(phdp->p_flags) & PROT_WRITE) == 0 ||
			    ROUND_PG(size) == ROUND_PG(off + phdp->p_memsz))
				break;
			/* This phdr has a zfod section */
			ld = _dl_calloc(1, sizeof(struct load_list));
			if (ld == NULL)
				_dl_exit(7);
			ld->start = NULL;
			ld->size = ROUND_PG(off + phdp->p_memsz) -
			    ROUND_PG(size);
			ld->moff = TRUNC_PG(phdp->p_vaddr) +
			    ROUND_PG(size);
			ld->foff = -1;
			ld->prot = PFLAGS(phdp->p_flags);
			LDLIST_INSERT(ld);
			break;
		case PT_DYNAMIC:
			dynp = (Elf_Dyn *)phdp->p_vaddr;
			break;
		case PT_TLS:
			_dl_printf("%s: unsupported TLS program header in %s\n",
			    _dl_progname, libname);
			_dl_close(libfile);
			_dl_errno = DL_CANT_LOAD_OBJ;
			return(0);
		default:
			break;
		}
	}

#define LOFF ((Elf_Addr)lowld->start - lowld->moff)

retry:
	for (ld = lowld; ld != NULL; ld = ld->next) {
		off_t foff;
		int fd, flags;
		void *res;

		flags = MAP_PRIVATE;
		if (LOFF + ld->moff != 0)
			flags |= MAP_FIXED | __MAP_NOREPLACE;

		if (ld->foff < 0) {
			fd = -1;
			foff = 0;
			flags |= MAP_ANON;
		} else {
			fd = libfile;
			foff = ld->foff;
		}

		res = _dl_mmap((void *)(LOFF + ld->moff), ROUND_PG(ld->size),
		    ld->prot, flags, fd, foff);
		if (_dl_mmap_error(res)) {
			/*
			 * The mapping we wanted isn't free, so we do an
			 * mquery without MAP_FIXED to get the next free
			 * mapping, adjust the base mapping address to match
			 * this free mapping and restart the process again.
			 *
			 * XXX - we need some kind of boundary condition
			 * here, or fix mquery to not run into the stack
			 */
			res = _dl_mquery((void *)(LOFF + ld->moff),
			    ROUND_PG(ld->size), ld->prot,
			    flags & ~(MAP_FIXED | __MAP_NOREPLACE), fd, foff);

			/*
			 * If ld == lowld, then ld->start is just a hint and
			 * thus shouldn't be unmapped.
			 */
			ld->start = NULL;

			/* Unmap any mappings that we did get in. */
			for (ld = lowld; ld != NULL; ld = ld->next) {
				if (ld->start == NULL)
					break;
				_dl_munmap(ld->start, ROUND_PG(ld->size));
				ld->start = NULL;
			}

			/* if the mquery failed, give up */
			if (_dl_mmap_error(res))
				goto fail;

			/* otherwise, reset the start of the base mapping */
			lowld->start = res - ld->moff + lowld->moff;
			goto retry;
		}

		ld->start = res;
	}

	for (ld = lowld; ld != NULL; ld = ld->next) {
		/* Zero out everything past the EOF */
		if ((ld->prot & PROT_WRITE) != 0 && (ld->size & align) != 0)
			_dl_memset((char *)ld->start + ld->size, 0,
			    _dl_pagesz - (ld->size & align));
		load_end = (Elf_Addr)ld->start + ROUND_PG(ld->size);
	}

	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++, phdp++)
		if (phdp->p_type == PT_OPENBSD_RANDOMIZE)
			_dl_randombuf((char *)(phdp->p_vaddr + LOFF),
			    phdp->p_memsz);

	prebind_data = prebind_load_fd(libfile, libname);

	_dl_close(libfile);

	dynp = (Elf_Dyn *)((unsigned long)dynp + LOFF);
	object = _dl_finalize_object(libname, dynp, 
	    (Elf_Phdr *)((char *)lowld->start + ehdr->e_phoff), ehdr->e_phnum,
	    type, (Elf_Addr)lowld->start, LOFF);
	if (object) {
		object->prebind_data = prebind_data;
		object->load_size = (Elf_Addr)load_end - (Elf_Addr)lowld->start;
		object->load_list = lowld;
		/* set inode, dev from stat info */
		object->dev = sb.st_dev;
		object->inode = sb.st_ino;
		object->obj_flags |= flags;
		_dl_set_sod(object->load_name, &object->sod);
	} else {
		_dl_load_list_free(lowld);
	}
	return(object);
fail:
	_dl_printf("%s: rtld mmap failed mapping %s.\n",
	    _dl_progname, libname);
	_dl_close(libfile);
	_dl_errno = DL_CANT_MMAP;
	_dl_load_list_free(lowld);
	return(0);
}
