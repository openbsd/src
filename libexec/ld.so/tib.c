/*
 * Copyright (c) 2016 Philip Guenther <guenther@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Thread Information Block (TIB) and Thread Local Storage (TLS) handling
 * (the TCB, Thread Control Block, is part of the TIB)
 */

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/exec_elf.h>

#include <tib.h>

#include "archdep.h"
#include "resolve.h"
#include "util.h"
#include "syscall.h"

/* If we need the syscall, use our local syscall definition */
#define	__set_tcb(tcb)	_dl_set_tcb(tcb)


static int	static_tls_size;

int		_dl_tib_static_done;

/*
 * Allocate a TIB for passing to __tfork for a new thread.  'extra'
 * is the amount of space to allocate on the side of the TIB opposite
 * of the TLS data: before the TIB for variant 1 and after the TIB
 * for variant 2.  If non-zero, tib_thread is set to point to that area.
 */
void *
_dl_allocate_tib(size_t extra)
{
	char *base;
	struct tib *tib;
	char *thread = NULL;
	struct elf_object *obj;

#if TLS_VARIANT == 1
	/* round up the extra size to align the tib after it */
	extra = ELF_ROUND(extra, sizeof(void *));
	base = _dl_malloc(extra + sizeof *tib + static_tls_size);
	tib = (struct tib *)(base + extra);
	if (extra)
		thread = base;
#define TLS_ADDR(tibp, offset)	((char *)(tibp) + sizeof(struct tib) + (offset))

#elif TLS_VARIANT == 2
	/* round up the tib size to align the extra area after it */
	base = _dl_malloc(ELF_ROUND(sizeof *tib, TIB_EXTRA_ALIGN) +
	    extra + static_tls_size);
	tib = (struct tib *)(base + static_tls_size);
	if (extra)
		thread = (char *)tib + ELF_ROUND(sizeof *tib, TIB_EXTRA_ALIGN);
#define TLS_ADDR(tibp, offset)	((char *)(tibp) - (offset))

#endif

	for (obj = _dl_objects; obj != NULL; obj = obj->next) {
		if (obj->tls_msize != 0) {
			char *addr = TLS_ADDR(tib, obj->tls_offset);

			_dl_memset(addr + obj->tls_fsize, 0,
			    obj->tls_msize - obj->tls_fsize);
			if (obj->tls_static_data != NULL)
				_dl_bcopy(obj->tls_static_data, addr,
				    obj->tls_fsize);
			DL_DEB(("\t%s has index %u addr %p msize %u fsize %u\n",
				obj->load_name, obj->tls_offset,
				(void *)addr, obj->tls_msize, obj->tls_fsize));
		}
	}

	TIB_INIT(tib, NULL, thread);

	DL_DEB(("tib new=%p\n", (void *)tib));

	return (tib);
}

void
_dl_free_tib(void *tib, size_t extra)
{
	size_t tib_offset;

#if TLS_VARIANT == 1
	tib_offset = ELF_ROUND(extra, sizeof(void *));
#elif TLS_VARIANT == 2
	tib_offset = static_tls_size;
#endif

	DL_DEB(("free tib=%p\n", (void *)tib));
	_dl_free((char *)tib - tib_offset);
}


/*
 * Record what's necessary for handling TLS for an object.
 */
void
_dl_set_tls(elf_object_t *object, Elf_Phdr *ptls, Elf_Addr libaddr,
    const char *libname)
{
	if (ptls->p_vaddr != 0 && ptls->p_filesz != 0)
		object->tls_static_data = (void *)(ptls->p_vaddr + libaddr);
	object->tls_fsize = ptls->p_filesz;
	object->tls_msize = ptls->p_memsz;
	object->tls_align = ptls->p_align;

	DL_DEB(("tls %x %x %x %x\n",
	    object->tls_static_data, object->tls_fsize, object->tls_msize,
	    object->tls_align));
}

static inline Elf_Addr
allocate_tls_offset(Elf_Addr msize, Elf_Addr align)
{
	Elf_Addr offset;

#if TLS_VARIANT == 1
	/* round up to the required alignment, then allocate the space */
	offset = ELF_ROUND(static_tls_size, align);
	static_tls_size += msize;
#elif TLS_VARIANT == 2
	/*
	 * allocate the space, then round up to the alignment
	 * (these are negative offsets, so rounding up really rounds the
	 * address down)
	 */
	static_tls_size = ELF_ROUND(static_tls_size + msize, align);
	offset = static_tls_size;
#else
# error "unknown TLS_VARIANT"
#endif
	return offset;
}

/*
 * Calculate the TLS offset for each object with static TLS.
 */
void
_dl_allocate_tls_offsets(void)
{
	struct elf_object *obj;

	for (obj = _dl_objects; obj != NULL; obj = obj->next) {
		if (obj->tls_msize != 0) {
			obj->tls_offset = allocate_tls_offset(obj->tls_msize,
			    obj->tls_align);
		}
	}

	/* no more static TLS allocations after this */
	_dl_tib_static_done = 1;
}

/*
 * Allocate the TIB + TLS for the initial thread.
 */
void
_dl_allocate_first_tib(void)
{
	struct tib *tib;

	tib = _dl_allocate_tib(0);
	tib->tib_tid = _dl_getthrid();

	TCB_SET(TIB_TO_TCB(tib));
}
