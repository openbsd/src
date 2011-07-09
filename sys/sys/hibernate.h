/*	$OpenBSD: hibernate.h,v 1.8 2011/07/09 00:27:31 mlarkin Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@stack.nl>
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

#ifndef _SYS_HIBERNATE_H_
#define _SYS_HIBERNATE_H_

#include <sys/types.h>
#include <sys/tree.h>
#include <lib/libz/zlib.h>
#include <machine/vmparam.h>

#define HIBERNATE_CHUNK_USED 1
#define HIBERNATE_CHUNK_CONFLICT 2
#define HIBERNATE_CHUNK_PLACED 4

struct hiballoc_entry;

/*
 * Hibernate allocator.
 *
 * Allocator operates from an arena, that is pre-allocated by the caller.
 */
struct hiballoc_arena
{
	RB_HEAD(hiballoc_addr, hiballoc_entry)
				hib_addrs;
};

/*
 * struct hibernate_state
 *
 * Describes a zlib compression stream and its associated hiballoc area
 */
struct hibernate_state {
        z_stream hib_stream;
        struct hiballoc_arena hiballoc_arena;
};

/*
 * struct hibernate_memory_range
 *
 * Describes a range of physical memory on the machine
 */
struct hibernate_memory_range {
	paddr_t		base;
	paddr_t		end;
};

/*
 * struct hibernate_disk_chunk
 *
 * Describes a hibernate chunk structure, used when splitting the memory
 * image of the machine into easy-to-manage pieces.
 */
struct hibernate_disk_chunk {
	paddr_t		base;		/* Base of chunk */
	paddr_t		end; 		/* End of chunk */		
	size_t		offset;		/* Abs. disk block locating chunk */
	size_t		compressed_size; /* Compressed size on disk */
	short		flags;		/* Flags */
};

/*
 * union hibernate_info
 *
 * Used to store information about the hibernation state of the machine, 
 * such as memory range count and extents, disk sector size, and various
 * offsets where things are located on disk.
 */
union hibernate_info {
	struct {
		size_t 		nranges;		
		size_t		image_size;
		size_t		chunk_ctr;
		u_int32_t	secsize;
		dev_t		device;
		daddr_t		swap_offset;
		daddr_t		sig_offset;
		daddr_t		image_offset;
		paddr_t		piglet_base;
		struct hibernate_memory_range ranges[VM_PHYSSEG_MAX];
		char		kernel_version[128];
		int (*io_func)(dev_t, daddr_t, vaddr_t, size_t, int, void *);
	};

	/* XXX - remove restriction to have this union fit in a single block */
	char pad[512]; /* Pad to 512 bytes */
};

void	*hib_alloc(struct hiballoc_arena*, size_t);
void	 hib_free(struct hiballoc_arena*, void*);
int	 hiballoc_init(struct hiballoc_arena*, void*, size_t len);
void	 uvm_pmr_zero_everything(void);
void	 uvm_pmr_dirty_everything(void);
int	 uvm_pmr_alloc_pig(paddr_t*, psize_t);
int	 uvm_pmr_alloc_piglet(paddr_t*, psize_t, paddr_t);
psize_t	 uvm_page_rle(paddr_t);

void	*get_hibernate_io_function(void);
int	get_hibernate_info(union hibernate_info *);

void	*hibernate_zlib_alloc(void *, int, int);
void	hibernate_zlib_free(void *, void *);

#endif /* _SYS_HIBERNATE_H_ */
