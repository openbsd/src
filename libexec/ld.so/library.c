/*	$OpenBSD: library.c,v 1.89 2022/12/04 15:42:07 deraadt Exp $ */

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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "syscall.h"
#include "util.h"
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
	elf_object_t *load_object = object->load_object;

	/*
	 * If our load object has become unreferenced then we lost the
	 * last group reference to it, so the entire group should be taken
	 * down.  The current object is somewhere below load_object in
	 * the child_vec tree, so it'll get cleaned up by the recursion.
	 * That means we can just switch here to the load object.
	 */
	if (load_object != object && OBJECT_REF_CNT(load_object) == 0 &&
	    (load_object->status & STAT_UNLOADED) == 0) {
		DL_DEB(("unload_shlib switched from %s to %s\n",
		    object->load_name, load_object->load_name));
		object = load_object;
		goto unload;
	}

	DL_DEB(("unload_shlib called on %s\n", object->load_name));
	if (OBJECT_REF_CNT(object) == 0 &&
	    (object->status & STAT_UNLOADED) == 0) {
		struct object_vector vec;
		int i;
unload:
		object->status |= STAT_UNLOADED;
		for (vec = object->child_vec, i = 0; i < vec.len; i++)
			_dl_unload_shlib(vec.vec[i]);
		TAILQ_FOREACH(n, &object->grpref_list, next_sib)
			_dl_unload_shlib(n->data);
		DL_DEB(("unload_shlib unloading on %s\n", object->load_name));
		_dl_load_list_free(object->load_list);
		_dl_munmap((void *)object->load_base, object->load_size);
		_dl_remove_object(object);
	}
}

elf_object_t *
_dl_tryload_shlib(const char *libname, int type, int flags, int nodelete)
{
	struct mutate imut[MAXMUT], mut[MAXMUT];
	int	libfile, i;
	struct load_list *next_load, *load_list = NULL;
	Elf_Addr maxva = 0, minva = ELF_NO_ADDR;
	Elf_Addr libaddr, loff, align = _dl_pagesz - 1;
	Elf_Addr relro_addr = 0, relro_size = 0;
	elf_object_t *object;
	char	hbuf[4096], *exec_start = 0;
	size_t exec_size = 0;
	Elf_Dyn *dynp = NULL;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdp;
	Elf_Phdr *ptls = NULL;
	struct stat sb;

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
			_dl_close(libfile);
			_dl_handle_already_loaded(object, flags);
			return(object);
		}
	}
	if (flags & DF_1_NOOPEN) {
		_dl_close(libfile);
		return NULL;

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

	_dl_memset(&mut, 0, sizeof mut);
	_dl_memset(&imut, 0, sizeof imut);

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
		case PT_TLS:
			if (phdp->p_filesz > phdp->p_memsz) {
				_dl_printf("%s: invalid tls data in %s.\n",
				    __progname, libname);
				_dl_close(libfile);
				_dl_errno = DL_CANT_LOAD_OBJ;
				return(0);
			}
			if (!_dl_tib_static_done) {
				ptls = phdp;
				break;
			}
			_dl_printf("%s: unsupported TLS program header in %s\n",
			    __progname, libname);
			_dl_close(libfile);
			_dl_errno = DL_CANT_LOAD_OBJ;
			return(0);
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
		_dl_printf("%s: ld.so mmap failed mapping %s.\n",
		    __progname, libname);
		_dl_close(libfile);
		_dl_errno = DL_CANT_MMAP;
		return(0);
	}

	loff = libaddr - minva;
	phdp = (Elf_Phdr *)(hbuf + ehdr->e_phoff);

	/* Entire mapping can become immutable, minus exceptions chosen later */
	_dl_defer_immut(imut, loff, maxva - minva);

	for (i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch (phdp->p_type) {
		case PT_LOAD: {
			char *start = (char *)(TRUNC_PG(phdp->p_vaddr)) + loff;
			Elf_Addr off = (phdp->p_vaddr & align);
			Elf_Addr size = off + phdp->p_filesz;
			int flags = PFLAGS(phdp->p_flags);
			void *res;

			/*
			 * Initially map W|X segments without X
			 * permission.  After we're done with the
			 * initial relocation processing, we will make
			 * these segments read-only and add back the X
			 * permission.  This way we maintain W^X at
			 * all times.
			 */
			if ((flags & PROT_WRITE) && (flags & PROT_EXEC))
				flags &= ~PROT_EXEC;

			if (size != 0) {
				res = _dl_mmap(start, ROUND_PG(size), flags,
				    MAP_FIXED|MAP_PRIVATE, libfile,
				    TRUNC_PG(phdp->p_offset));
			} else
				res = NULL;	/* silence gcc */
			next_load = _dl_calloc(1, sizeof(struct load_list));
			if (next_load == NULL)
				_dl_oom();
			next_load->next = load_list;
			load_list = next_load;
			next_load->start = start;
			next_load->size = size;
			next_load->prot = PFLAGS(phdp->p_flags);
			if (size != 0 && _dl_mmap_error(res)) {
				_dl_printf("%s: ld.so mmap failed mapping %s.\n",
				    __progname, libname);
				_dl_close(libfile);
				_dl_errno = DL_CANT_MMAP;
				_dl_munmap((void *)libaddr, maxva - minva);
				_dl_load_list_free(load_list);
				return(0);
			}
			if ((flags & PROT_EXEC) && exec_start == 0) {
				exec_start = start;
				exec_size = ROUND_PG(size);
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
				res = _dl_mmap(start, size, flags,
				    MAP_FIXED|MAP_PRIVATE|MAP_ANON, -1, 0);
				if (_dl_mmap_error(res)) {
					_dl_printf("%s: ld.so mmap failed mapping %s.\n",
					    __progname, libname);
					_dl_close(libfile);
					_dl_errno = DL_CANT_MMAP;
					_dl_munmap((void *)libaddr, maxva - minva);
					_dl_load_list_free(load_list);
					return(0);
				}
			}
			break;
		}

		case PT_OPENBSD_RANDOMIZE:
			_dl_arc4randombuf((char *)(phdp->p_vaddr + loff),
			    phdp->p_memsz);
			break;

		case PT_GNU_RELRO:
			relro_addr = phdp->p_vaddr + loff;
			relro_size = phdp->p_memsz;
			_dl_defer_mut(mut, phdp->p_vaddr + loff, phdp->p_memsz);
			break;

		case PT_OPENBSD_MUTABLE:
			_dl_defer_mut(mut, phdp->p_vaddr + loff, phdp->p_memsz);
			break;

		default:
			break;
		}
	}

	_dl_close(libfile);

	dynp = (Elf_Dyn *)((unsigned long)dynp + loff);
	object = _dl_finalize_object(libname, dynp,
	    (Elf_Phdr *)((char *)libaddr + ehdr->e_phoff), ehdr->e_phnum,type,
	    libaddr, loff);
	if (object) {
		char *soname = (char *)object->Dyn.info[DT_SONAME];

		object->load_size = maxva - minva;	/*XXX*/
		object->load_list = load_list;
		/* set inode, dev from stat info */
		object->dev = sb.st_dev;
		object->inode = sb.st_ino;
		object->obj_flags |= flags;
		object->nodelete = nodelete;
		object->relro_addr = relro_addr;
		object->relro_size = relro_size;
		_dl_set_sod(object->load_name, &object->sod);
		if (ptls != NULL && ptls->p_memsz)
			_dl_set_tls(object, ptls, libaddr, libname);

		/* Request permission for system calls in libc.so's text segment */
		if (soname != NULL &&
		    _dl_strncmp(soname, "libc.so.", 8) == 0) {
			if (_dl_msyscall(exec_start, exec_size) == -1)
				_dl_printf("msyscall %lx %lx error\n",
				    exec_start, exec_size);
		}
		_dl_bcopy(mut, object->mut, sizeof mut);
		_dl_bcopy(imut, object->imut, sizeof imut);
	} else {
		_dl_munmap((void *)libaddr, maxva - minva);
		_dl_load_list_free(load_list);
	}
	return(object);
}
