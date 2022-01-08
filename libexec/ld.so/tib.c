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

#include "syscall.h"
#include "util.h"
#include "resolve.h"

/* If we need the syscall, use our local syscall definition */
#define	__set_tcb(tcb)	_dl___set_tcb(tcb)

__dso_hidden void *allocate_tib(size_t);

#define MAX(a,b)	(((a)>(b))?(a):(b))

#ifdef TIB_EXTRA_ALIGN
# define TIB_ALIGN	MAX(__alignof__(struct tib), TIB_EXTRA_ALIGN)
#else
# define TIB_ALIGN	__alignof__(struct tib)
#endif


/* size of static TLS allocation */
static int	static_tls_size;
/* alignment of static TLS allocation */
static int	static_tls_align;
/* base-offset alignment of (first) static TLS allocation */
static int	static_tls_align_offset;

int		_dl_tib_static_done;

/*
 * Allocate a TIB for passing to __tfork for a new thread.  'extra'
 * is the amount of space to allocate on the side of the TIB opposite
 * of the TLS data: before the TIB for variant 1 and after the TIB
 * for variant 2.  If non-zero, tib_thread is set to point to that area.
 */
void *
allocate_tib(size_t extra)
{
	char *base;
	struct tib *tib;
	char *thread = NULL;
	struct elf_object *obj;

#if TLS_VARIANT == 1
	/* round up the extra size to align the TIB and TLS data after it */
	size_t unpad_extra = (extra <= static_tls_align_offset) ? 0 :
	    ELF_ROUND(extra - static_tls_align_offset, static_tls_align);
	base = _dl_aligned_alloc(static_tls_align, unpad_extra +
	    static_tls_align_offset + sizeof *tib + static_tls_size);
	if (base == NULL)
		return NULL;
	tib = (struct tib *)(base + unpad_extra + static_tls_align_offset);
	if (extra)
		thread = base;
#define TLS_ADDR(tibp, offset)	((char *)(tibp) + sizeof(struct tib) + (offset))

#elif TLS_VARIANT == 2
	/* round up the TIB size to align the extra area after it */
	base = _dl_aligned_alloc(static_tls_align, static_tls_size +
	    static_tls_align_offset + ELF_ROUND(sizeof *tib, TIB_EXTRA_ALIGN) +
	    extra);
	if (base == NULL)
		return NULL;
	base += static_tls_align_offset;
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
__strong_alias(_dl_allocate_tib, allocate_tib);

void
_dl_free_tib(void *tib, size_t extra)
{
	size_t tib_offset;

#if TLS_VARIANT == 1
	tib_offset = (extra <= static_tls_align_offset) ? 0 :
	    ELF_ROUND(extra - static_tls_align_offset, static_tls_align);
#elif TLS_VARIANT == 2
	tib_offset = static_tls_size;
#endif
	tib_offset += static_tls_align_offset;

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
allocate_tls_offset(Elf_Addr msize, Elf_Addr align, int for_exe)
{
	Elf_Addr offset;

	if (for_exe && static_tls_size != 0)
		_dl_die("TLS allocation before executable!");

#if TLS_VARIANT == 1
	if (for_exe) {
		/*
		 * Variant 1 places the data after the TIB.  If the
		 * TLS alignment is larger than the TIB alignment
		 * then we may need to pad in front of the TIB to
		 * place the TLS data on the proper alignment.
		 * Example: p_align=16 sizeof(TIB)=52 align(TIB)=4
		 * - need to offset the TIB 12 bytes from the start
		 * - to place ths TLS data at offset 64
		 */
		static_tls_align = MAX(align, TIB_ALIGN);
		static_tls_align_offset =
		    ELF_ROUND(sizeof(struct tib), static_tls_align) -
		    sizeof(struct tib);
		offset = 0;
		static_tls_size = msize;
	} else {
		/*
		 * If a later object increases the alignment, realign the
		 * existing sections.  We push as much padding as possible
		 * to the start there it can overlap the thread structure
		 */
		if (static_tls_align < align) {
			static_tls_align_offset += align - static_tls_align;
			static_tls_align = align;
		}

		/*
		 * Round up to the required alignment, taking into account
		 * the leading padding and TIB, then allocate the space.
		 */
		offset = static_tls_align_offset + sizeof(struct tib) +
		    static_tls_size;
		offset = ELF_ROUND(offset, align) - static_tls_align_offset
		    - sizeof(struct tib);
		static_tls_size = offset + msize;
	}
#elif TLS_VARIANT == 2
	/* Realignment is automatic for variant II */
	if (static_tls_align < align)
		static_tls_align = align;

	/*
	 * Variant 2 places the data before the TIB so we need to round up
	 * the size to the TLS data alignment TIB's alignment.
	 * Example A: p_memsz=24 p_align=16 align(TIB)=8
	 * - need to allocate 32 bytes for TLS as compiler
	 * - will give the first TLS symbol an offset of -32
	 * Example B: p_memsz=4 p_align=4 align(TIB)=8
	 * - need to allocate 8 bytes so that the TIB is
	 * - properly aligned
	 * So: allocate the space, then round up to the alignment
	 * (these are negative offsets, so rounding up really
	 * rounds the address down)
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

	static_tls_align = TIB_ALIGN;
	for (obj = _dl_objects; obj != NULL; obj = obj->next) {
		if (obj->tls_msize != 0) {
			obj->tls_offset = allocate_tls_offset(obj->tls_msize,
			    obj->tls_align, obj->obj_type == OBJTYPE_EXE);
		}
	}

#if TLS_VARIANT == 2
	static_tls_align_offset = ELF_ROUND(static_tls_size, static_tls_align)
	    - static_tls_size;
#endif

	/* no more static TLS allocations after this */
	_dl_tib_static_done = 1;

	DL_DEB(("static tls size=%x align=%x offset=%x\n",
	    static_tls_size, static_tls_align, static_tls_align_offset));
}

/*
 * Allocate the TIB + TLS for the initial thread.
 */
void
_dl_allocate_first_tib(void)
{
	struct tib *tib;

	tib = allocate_tib(0);
	tib->tib_tid = _dl_getthrid();

	TCB_SET(TIB_TO_TCB(tib));
}
