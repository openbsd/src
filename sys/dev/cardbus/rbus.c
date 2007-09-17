/*	$OpenBSD: rbus.c,v 1.11 2007/09/17 20:29:47 miod Exp $	*/
/*	$NetBSD: rbus.c,v 1.3 1999/11/06 06:20:53 soren Exp $	*/
/*
 * Copyright (c) 1999
 *     HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/extent.h>

#include <machine/bus.h>

#include <dev/cardbus/rbus.h>

/* #define RBUS_DEBUG */

#if defined RBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#ifdef DDB
#define STATIC
#else
#define STATIC static
#endif
#define DPRINTF(a)
#endif


STATIC rbus_tag_t rbus_new_body(bus_space_tag_t, rbus_tag_t, struct extent *,
		      bus_addr_t, bus_addr_t, bus_addr_t, int);

int
rbus_space_alloc(rbus_tag_t rbt, bus_addr_t addr, bus_size_t size,
    bus_addr_t mask, bus_addr_t align, int flags, bus_addr_t *addrp,
    bus_space_handle_t *bshp)
{
	return (rbus_space_alloc_subregion(rbt, rbt->rb_start, rbt->rb_end,
	    addr, size, mask, align, flags, addrp, bshp));
}

int
rbus_space_alloc_subregion(rbus_tag_t rbt, bus_addr_t substart,
    bus_addr_t subend, bus_addr_t addr, bus_size_t size,
    bus_addr_t mask, bus_addr_t align, int flags, bus_addr_t *addrp,
    bus_space_handle_t *bshp)
{
	bus_addr_t decodesize = mask + 1;
	bus_addr_t boundary, search_addr;
	int val;
	u_long result;
	int exflags = EX_FAST | EX_NOWAIT | EX_MALLOCOK;

	DPRINTF(("rbus_space_alloc: addr %lx, size %lx, mask %lx, align %lx\n",
	    (u_long)addr, (u_long)size, (u_long)mask, (u_long)align));

	addr += rbt->rb_offset;

	if (mask == 0) {
		/* FULL Decode */
		decodesize = 0;
	}

	if (rbt->rb_flags == RBUS_SPACE_ASK_PARENT) {
		return (rbus_space_alloc(rbt->rb_parent, addr, size, mask,
		    align, flags, addrp, bshp));
	} else if (rbt->rb_flags == RBUS_SPACE_SHARE ||
	    rbt->rb_flags == RBUS_SPACE_DEDICATE) {
		/* rbt has its own sh_extent */

		/* sanity check: the subregion [substart, subend] should be
		   smaller than the region included in sh_extent */
		if (substart < rbt->rb_ext->ex_start ||
		    subend > rbt->rb_ext->ex_end) {
			DPRINTF(("rbus: out of range\n"));
			return (1);
		}

		if (decodesize == align) {
			if (extent_alloc_subregion(rbt->rb_ext, substart,
			    subend, size, align, 0, 0, exflags, &result))
				return (1);
		} else if (decodesize == 0) {
			/* maybe, the register is overflowed. */

			if (extent_alloc_subregion(rbt->rb_ext, addr,
			    addr + size, size, 1, 0, 0, exflags, &result))
				return (1);
		} else {
			boundary = decodesize > align ? decodesize : align;

			search_addr = (substart & ~(boundary - 1)) + addr;

			if (search_addr < substart)
				search_addr += boundary;

			val = 1;
			for (; search_addr + size <= subend;
			    search_addr += boundary) {
				val = extent_alloc_subregion(
				    rbt->rb_ext,search_addr,
				    search_addr + size, size, align, 0, 0,
				    exflags, &result);
				DPRINTF(("rbus: trying [%lx:%lx] %lx\n",
				    (u_long)search_addr,
				    (u_long)search_addr + size,
				    (u_long)align));
				if (val == 0)
					break;
			}

			if (val != 0) {
				/* no space found */
				DPRINTF(("rbus: no space found\n"));
				return (1);
			}
		}

		if (md_space_map(rbt->rb_bt, result, size, flags, bshp)) {
			/* map failed */
			extent_free(rbt->rb_ext, result, size, exflags);
			return (1);
		}

		if (addrp != NULL)
			*addrp = result + rbt->rb_offset;
		return (0);
	} else {
		/* error!! */
		DPRINTF(("rbus: no rbus type\n"));
		return (1);
	}
}

int
rbus_space_free(rbus_tag_t rbt, bus_space_handle_t bsh, bus_size_t size,
    bus_addr_t *addrp)
{
	int exflags = EX_FAST | EX_NOWAIT;
	bus_addr_t addr;
	int status = 1;

	if (rbt->rb_flags == RBUS_SPACE_ASK_PARENT) {
		status = rbus_space_free(rbt->rb_parent, bsh, size, &addr);
	} else if (rbt->rb_flags == RBUS_SPACE_SHARE ||
	    rbt->rb_flags == RBUS_SPACE_DEDICATE) {
		md_space_unmap(rbt->rb_bt, bsh, size, &addr);

		extent_free(rbt->rb_ext, addr, size, exflags);

		status = 0;
	} else {
		/* error. INVALID rbustag */
		status = 1;
	}

	if (addrp != NULL)
		*addrp = addr;

	return (status);
}

/*
 * STATIC rbus_tag_t
 * rbus_new_body(bus_space_tag_t bt, rbus_tag_t parent,
 *               struct extent *ex, bus_addr_t start, bus_size_t end,
 *               bus_addr_t offset, int flags)
 *
 */
STATIC rbus_tag_t
rbus_new_body(bus_space_tag_t bt, rbus_tag_t parent, struct extent *ex,
    bus_addr_t start, bus_addr_t end, bus_addr_t offset, int flags)
{
	rbus_tag_t rb;

	/* sanity check */
	if (parent != NULL) {
		if (start < parent->rb_start || end > parent->rb_end) {
			/* out of range: [start, size] should be contained
			 * in parent space
			 */
			return (0);
			/* Should I invoke panic? */
		}
	}

	if ((rb = (rbus_tag_t)malloc(sizeof(struct rbustag), M_DEVBUF,
	    M_NOWAIT)) == NULL) {
		panic("no memory for rbus instance");
	}

	rb->rb_bt = bt;
	rb->rb_parent = parent;
	rb->rb_start = start;
	rb->rb_end = end;
	rb->rb_offset = offset;
	rb->rb_flags = flags;
	rb->rb_ext = ex;

	DPRINTF(("rbus_new_body: [%lx, %lx] type %s name [%s]\n",
	    (u_long)start, (u_long)end,
	   flags == RBUS_SPACE_SHARE ? "share" :
	   flags == RBUS_SPACE_DEDICATE ? "dedicated" :
	   flags == RBUS_SPACE_ASK_PARENT ? "parent" : "invalid",
	   ex != NULL ? ex->ex_name : "noname"));

	return (rb);
}

/*
 * rbus_tag_t rbus_new(rbus_tag_t parent, bus_addr_t start, bus_size_t
 *                     size, bus_addr_t offset, int flags)
 *
 *  This function makes a new child rbus instance.
 */
rbus_tag_t
rbus_new(rbus_tag_t parent, bus_addr_t start, bus_size_t size,
    bus_addr_t offset, int flags)
{
	rbus_tag_t rb;
	struct extent *ex = NULL;
	bus_addr_t end = start + size;

	if (flags == RBUS_SPACE_SHARE) {
		ex = parent->rb_ext;
	} else if (flags == RBUS_SPACE_DEDICATE) {
		if ((ex = extent_create("rbus", start, end, M_DEVBUF, NULL, 0,
		    EX_NOCOALESCE|EX_NOWAIT)) == NULL)
			return (NULL);
	} else if (flags == RBUS_SPACE_ASK_PARENT) {
		ex = NULL;
	} else {
		/* Invalid flag */
		return (0);
	}

	rb = rbus_new_body(parent->rb_bt, parent, ex, start, start + size,
	    offset, flags);

	if ((rb == NULL) && (flags == RBUS_SPACE_DEDICATE))
		extent_destroy(ex);

	return (rb);
}

/*
 * rbus_tag_t rbus_new_root_delegate(bus_space_tag, bus_addr_t,
 *                                   bus_size_t, bus_addr_t offset)
 *
 *  This function makes a root rbus instance.
 */
rbus_tag_t
rbus_new_root_delegate(bus_space_tag_t bt, bus_addr_t start, bus_size_t size,
    bus_addr_t offset)
{
	rbus_tag_t rb;
	struct extent *ex;

	if ((ex = extent_create("rbus root", start, start + size, M_DEVBUF,
	    NULL, 0, EX_NOCOALESCE|EX_NOWAIT)) == NULL)
		return (NULL);

	rb = rbus_new_body(bt, NULL, ex, start, start + size, offset,
	    RBUS_SPACE_DEDICATE);

	if (rb == NULL)
		extent_destroy(ex);

	return (rb);
}

/*
 * rbus_tag_t rbus_new_root_share(bus_space_tag, struct extent *,
 *                                 bus_addr_t, bus_size_t, bus_addr_t offset)
 *
 *  This function makes a root rbus instance.
 */
rbus_tag_t
rbus_new_root_share(bus_space_tag_t bt, struct extent *ex, bus_addr_t start,
    bus_size_t size, bus_addr_t offset)
{
	/* sanity check */
	if (start < ex->ex_start || start + size > ex->ex_end) {
		/* out of range: [start, size] should be contained in
		 * parent space
		 */
		return (0);
		/* Should I invoke panic? */
	}

	return (rbus_new_body(bt, NULL, ex, start, start + size, offset,
	    RBUS_SPACE_SHARE));
}

/*
 * int rbus_delete (rbus_tag_t rb)
 *
 *   This function deletes the rbus structure pointed in the argument.
 */
int
rbus_delete(rbus_tag_t rb)
{
	DPRINTF(("rbus_delete called [%s]\n", rb->rb_ext != NULL ?
	    rb->rb_ext->ex_name : "noname"));

	if (rb->rb_flags == RBUS_SPACE_DEDICATE)
		extent_destroy(rb->rb_ext);

	free(rb, M_DEVBUF);

	return (0);
}
