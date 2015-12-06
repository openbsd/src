/* $NetBSD: loadfile.c,v 1.10 2000/12/03 02:53:04 tsutsui Exp $ */
/* $OpenBSD: loadfile_elf.c,v 1.4 2015/12/06 17:42:15 mlarkin Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stddef.h>

#include <sys/param.h>
#include <sys/exec.h>

#include "loadfile.h"
#include "vmd.h"
#include <sys/exec_elf.h>
#include <machine/vmmvar.h>
#include <machine/biosvar.h>
#include <machine/segments.h>

#define BOOTARGS_PAGE 0x2000
#define GDT_PAGE 0x10000
#define STACK_PAGE 0xF000

#define LOWMEM_KB 636

union {
	Elf32_Ehdr elf32;
	Elf64_Ehdr elf64;
} hdr;

static void setsegment(struct mem_segment_descriptor *, uint32_t,
    size_t, int, int, int, int);
int loadelf_main(int fd, int, int);
int elf32_exec(int, Elf32_Ehdr *, u_long *, int);
int elf64_exec(int, Elf64_Ehdr *, u_long *, int);
static void push_bootargs(int);
static void push_stack(int, uint32_t);
static void push_gdt(void);
static size_t mread(int, uint32_t, size_t);
static void marc4random_buf(uint32_t, int);
static void mbzero(uint32_t, int);
static void mbcopy(char *, char *, int);

extern char *__progname;
extern int vm_id;

/*
 * setsegment
 *
 * Initializes a segment selector entry with the provided descriptor.
 * For the purposes of the bootloader mimiced by vmd(8), we only need
 * memory-type segment descriptor support.
 *
 * This function was copied from machdep.c
 *
 * Parameters:
 *  sd: Address of the entry to initialize
 *  base: base of the segment
 *  limit: limit of the segment
 *  type: type of the segment
 *  dpl: privilege level of the egment
 *  def32: default 16/32 bit size of the segment
 *  gran: granularity of the segment (byte/page)
 */
static void
setsegment(struct mem_segment_descriptor *sd, uint32_t base, size_t limit,
    int type, int dpl, int def32, int gran)
{
	sd->sd_lolimit = (int)limit;
	sd->sd_lobase = (int)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (int)limit >> 16;
	sd->sd_avl = 0;
	sd->sd_long = 0;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (int)base >> 24;
}

/*
 * push_gdt
 *
 * Allocates and populates a page in the guest phys memory space to hold
 * the boot-time GDT. Since vmd(8) is acting as the bootloader, we need to
 * create the same GDT that a real bootloader would have created.
 * This is loaded into the guest phys RAM space at address GDT_PAGE.
 */
static void
push_gdt(void)
{
	uint8_t gdtpage[PAGE_SIZE];
	struct mem_segment_descriptor *sd;

	bzero(&gdtpage, sizeof(gdtpage));
	sd = (struct mem_segment_descriptor *)&gdtpage;

	/*
	 * Create three segment descriptors:
	 *
	 * GDT[0] : null desriptor. "Created" via bzero above.
	 * GDT[1] (selector @ 0x8): Executable segment, for CS
	 * GDT[2] (selector @ 0x10): RW Data segment, for DS/ES/SS
	 */
	setsegment(&sd[1], 0, 0xffffffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&sd[2], 0, 0xffffffff, SDT_MEMRWA, SEL_KPL, 1, 1);

	write_page(GDT_PAGE, gdtpage, PAGE_SIZE, 1);
}

/*
 * loadelf_main
 *
 * Loads an ELF kernel to it's defined load address in the guest VM whose
 * ID is provided in 'vm_id_in'. The kernel is loaded to its defined start
 * point as set in the ELF header.
 *
 * Parameters:
 *  fd: file descriptor of a kernel file to load
 *  vm_id_in: ID of the VM to load the kernel into
 *  mem_sz: memory size in MB assigned to the guest (passed through to
 *      push_bootargs)
 *
 * Return values:
 *  0 if successful
 *  various error codes returned from read(2) or loadelf functions
 */
int
loadelf_main(int fd, int vm_id_in, int mem_sz)
{
	int r;
	u_long marks[MARK_MAX];

	vm_id = vm_id_in;

	if ((r = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr))
		return 1;

	bzero(&marks, sizeof(marks));
	if (memcmp(hdr.elf32.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
		r = elf32_exec(fd, &hdr.elf32, marks, LOAD_ALL);
	} else if (memcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
		r = elf64_exec(fd, &hdr.elf64, marks, LOAD_ALL);
	}

	push_bootargs(mem_sz);
	push_gdt();
	push_stack(mem_sz, marks[MARK_END]);

	return r;
}

/*
 * push_bootargs
 *
 * Creates the boot arguments page in the guest address space.
 * Since vmd(8) is acting as the bootloader, we need to create the same boot
 * arguments page that a real bootloader would have created. This is loaded
 * into the guest phys RAM space at address BOOTARGS_PAGE.
 *
 * Parameters:
 *  mem_sz: guest memory size in MB
 *
 * Return values:
 *  nothing
 */
static void
push_bootargs(int mem_sz)
{
	size_t sz;
	bios_memmap_t memmap[3];
	bios_consdev_t consdev;
	uint32_t ba[1024];

	/* First memory region: 0 - LOWMEM_KB (DOS low mem) */
	memmap[0].addr = 0x0;
	memmap[0].size = LOWMEM_KB * 1024;
	memmap[0].type = 0x1;	/* Type 1 : Normal memory */

	/* Second memory region: 1MB - n, reserve top 1MB */
	memmap[1].addr = 0x100000;
	memmap[1].size = (mem_sz - 1) * 1024 * 1024;
	memmap[1].type = 0x1;	/* Type 1 : Normal memory */

	/* Null mem map entry to denote the end of the ranges */
	memmap[2].addr = 0x0;
	memmap[2].size = 0x0;
	memmap[2].type = 0x0;

	sz = 3 * sizeof(int) + 3 * sizeof(bios_memmap_t);
	ba[0] = 0x0;    /* memory map */
	ba[1] = sz;
	ba[2] = sz;     /* next */
	memcpy(&ba[3], &memmap, 3 * sizeof(bios_memmap_t));
	sz = sz / sizeof(int);

	/* Serial console device, COM1 @ 0x3f8 */
	consdev.consdev = makedev(8, 0);        /* com1 @ 0x3f8 */
	consdev.conspeed = 9600;
	consdev.consaddr = 0x3f8;
	consdev.consfreq = 0;

	ba[sz] = 0x5;   /* consdev */
	ba[sz + 1] = (int)sizeof(bios_consdev_t) + 3 * sizeof(int);
	ba[sz + 2] = (int)sizeof(bios_consdev_t) + 3 * sizeof(int);
	memcpy(&ba[sz + 3], &consdev, sizeof(bios_consdev_t));

	write_page(BOOTARGS_PAGE, ba, PAGE_SIZE, 1);
}

/*
 * push_stack
 *
 * Creates the boot stack page in the guest address space. When using a real
 * bootloader, the stack will be prepared using the following format before
 * transitioning to kernel start, so vmd(8) needs to mimic the same stack
 * layout. The stack content is pushed to the guest phys RAM at address
 * STACK_PAGE. The bootloader operates in 32 bit mode; each stack entry is
 * 4 bytes.
 *
 * Stack Layout: (TOS == Top Of Stack)
 *  TOS		location of boot arguments page
 *  TOS - 0x4	size of the content in the boot arguments page
 *  TOS - 0x8	size of low memory in KB
 *  TOS - 0xc	size of high memory in KB
 *  TOS - 0x10	kernel 'end' symbol value
 *  TOS - 0x14	version of bootarg API
 *
 * Parameters:
 *  mem_sz: size of guest VM memory, in MB
 *  end: kernel 'end' symbol value
 *
 * Return values:
 *  nothing
 */
static void
push_stack(int mem_sz, uint32_t end)
{
	uint32_t stack[1024];
	uint16_t loc;

	bzero(&stack, sizeof(stack));
	loc = 1024;

	stack[--loc] = BOOTARGS_PAGE;
	stack[--loc] = 3 * sizeof(bios_memmap_t) +
	    sizeof(bios_consdev_t) +
	    6 * sizeof(int);
	stack[--loc] = LOWMEM_KB;
	stack[--loc] = mem_sz * 1024 - LOWMEM_KB;
	stack[--loc] = end;
	stack[--loc] = 0x0e;
	stack[--loc] = 0x0;
	stack[--loc] = 0x0;

	write_page(STACK_PAGE, &stack, PAGE_SIZE, 1);
}

/*
 * mread
 *
 * Reads 'sz' bytes from the file whose descriptor is provided in 'fd'
 * into the guest address space at paddr 'addr'. Note that the guest
 * paddr is limited to 32 bit (4GB).
 *
 * Parameters:
 *  fd: file descriptor of the kernel image file to read from.
 *  addr: guest paddr_t to load to
 *  sz: number of bytes to load
 *
 * Return values:
 *  returns 'sz' if successful, or 0 otherwise.
 */
static size_t
mread(int fd, uint32_t addr, size_t sz)
{
	int ct;
	size_t i, rd, osz;
	char buf[PAGE_SIZE];

	/*
	 * break up the 'sz' bytes into PAGE_SIZE chunks for use with
	 * write_page
	 */
	ct = 0;
	rd = 0;
	osz = sz;
	if ((addr & PAGE_MASK) != 0) {
		bzero(buf, sizeof(buf));
		if (sz > PAGE_SIZE)
			ct = PAGE_SIZE - (addr & PAGE_MASK);
		else
			ct = sz;

		if (read(fd, buf, ct) != ct) {
			log_warn("%s: error %d in mread", __progname, errno);
			return (0);
		}
		rd += ct;

		if (write_page(addr, buf, ct, 1))
			return (0);

		addr += ct;
	}

	sz = sz - ct;

	if (sz == 0)
		return (osz);

	for (i = 0; i < sz; i += PAGE_SIZE, addr += PAGE_SIZE) {
		bzero(buf, sizeof(buf));
		if (i + PAGE_SIZE > sz)
			ct = sz - i;
		else
			ct = PAGE_SIZE;

		if (read(fd, buf, ct) != ct) {
			log_warn("%s: error %d in mread", __progname, errno);
			return (0);
		}
		rd += ct;

		if (write_page(addr, buf, ct, 1))
			return (0);
	}

	return (osz);
}

/*
 * marc4random_buf
 *
 * load 'sz' bytes of random data into the guest address space at paddr
 * 'addr'. Note that the guest paddr is limited to 32 bit (4GB).
 *
 * Parameters:
 *  addr: guest paddr_t to load random bytes into
 *  sz: number of random bytes to load
 *
 * Return values:
 *  nothing
 */
static void
marc4random_buf(uint32_t addr, int sz)
{
	int i, ct;
	char buf[PAGE_SIZE];

	/*
	 * break up the 'sz' bytes into PAGE_SIZE chunks for use with
	 * write_page
	 */
	ct = 0;
	if (addr % PAGE_SIZE != 0) {
		bzero(buf, sizeof(buf));
		ct = PAGE_SIZE - (addr % PAGE_SIZE);

		arc4random_buf(buf, ct);

		if (write_page(addr, buf, ct, 1))
			return;

		addr += ct;
	}

	for (i = 0; i < sz; i+= PAGE_SIZE, addr += PAGE_SIZE) {
		bzero(buf, sizeof(buf));
		if (i + PAGE_SIZE > sz)
			ct = sz - i;
		else
			ct = PAGE_SIZE;

		arc4random_buf(buf, ct);

		if (write_page(addr, buf, ct, 1))
			return;
	}
}

/*
 * mbzero
 *
 * load 'sz' bytes of zeros into the guest address space at paddr
 * 'addr'. Note that the guest paddr is limited to 32 bit (4GB)
 *
 * Parameters:
 *  addr: guest paddr_t to zero
 *  sz: number of zero bytes to store
 *
 * Return values:
 *  nothing
 */
static void
mbzero(uint32_t addr, int sz)
{
	int i, ct;
	char buf[PAGE_SIZE];

	/*
	 * break up the 'sz' bytes into PAGE_SIZE chunks for use with
	 * write_page
	 */
	ct = 0;
	bzero(buf, sizeof(buf));
	if (addr % PAGE_SIZE != 0) {
		ct = PAGE_SIZE - (addr % PAGE_SIZE);

		if (write_page(addr, buf, ct, 1))
			return;

		addr += ct;
	}

	for (i = 0; i < sz; i+= PAGE_SIZE, addr += PAGE_SIZE) {
		if (i + PAGE_SIZE > sz)
			ct = sz - i;
		else
			ct = PAGE_SIZE;

		if (write_page(addr, buf, ct, 1))
			return;
	}
}

/*
 * mbcopy
 *
 * copies 'sz' bytes from guest paddr 'src' to guest paddr 'dst'.
 * Both 'src' and 'dst' are limited to 32 bit (4GB)
 *
 * Parameters:
 *  src: source guest paddr_t to copy from
 *  dst: destination guest paddr_t to copy to
 *  sz: number of bytes to copy
 *
 * Return values:
 *  nothing
 *
 * XXX - unimplemented. this is used when loading symbols.
 */
static void
mbcopy(char *src, char *dst, int sz)
{
	log_warnx("warning: bcopy during ELF kernel load not supported");
}

/*
 * elf64_exec
 *
 * Load the kernel indicated by 'fd' into the guest physical memory
 * space, at the addresses defined in the ELF header.
 *
 * This function is used for 64 bit kernels.
 *
 * Parameters:
 *  fd: file descriptor of the kernel to load
 *  elf: ELF header of the kernel
 *  marks: array to store the offsets of various kernel structures
 *      (start, bss, etc)
 *  flags: flag value to indicate which section(s) to load (usually
 *      LOAD_ALL)
 *
 * Return values:
 *  0 if successful
 *  1 if unsuccessful
 */
int
elf64_exec(int fd, Elf64_Ehdr *elf, u_long *marks, int flags)
{
	Elf64_Shdr *shp;
	Elf64_Phdr *phdr;
	Elf64_Off off;
	int i;
	ssize_t sz;
	int first;
	int havesyms, havelines;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp;

	sz = elf->e_phnum * sizeof(Elf64_Phdr);
	phdr = malloc(sz);

	if (lseek(fd, (off_t)elf->e_phoff, SEEK_SET) == -1)  {
		free(phdr);
		return 1;
	}

	if (read(fd, phdr, sz) != sz) {
		free(phdr);
		return 1;
	}

	for (first = 1, i = 0; i < elf->e_phnum; i++) {
		if (phdr[i].p_type == PT_OPENBSD_RANDOMIZE) {
			int m;

			/* Fill segment if asked for. */
			if (flags & LOAD_RANDOM) {
				for (pos = 0; pos < phdr[i].p_filesz;
				    pos += m) {
					m = phdr[i].p_filesz - pos;
					marc4random_buf(phdr[i].p_paddr + pos,
					    m);
				}
			}
			if (flags & (LOAD_RANDOM | COUNT_RANDOM)) {
				marks[MARK_RANDOM] = LOADADDR(phdr[i].p_paddr);
				marks[MARK_ERANDOM] =
				    marks[MARK_RANDOM] + phdr[i].p_filesz;
			}
			continue;
		}

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	((p.p_flags & PF_X) == 0)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {

			/* Read in segment. */
			if (lseek(fd, (off_t)phdr[i].p_offset,
			    SEEK_SET) == -1) {
				free(phdr);
				return 1;
			}
			if (mread(fd, (uint32_t)(phdr[i].p_paddr -
			    0xffffffff80000000ULL), phdr[i].p_filesz) !=
			    phdr[i].p_filesz) {
				free(phdr);
				return 1;
			}

			first = 0;
		}

		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT | COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA | COUNT_TEXT)))) {
			pos = phdr[i].p_paddr;
			if (minp > pos)
				minp = pos;
			pos += phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out BSS. */
		if (IS_BSS(phdr[i]) && (flags & LOAD_BSS)) {
			mbzero((uint32_t)(phdr[i].p_paddr -
			    0xffffffff80000000 + phdr[i].p_filesz),
			    phdr[i].p_memsz - phdr[i].p_filesz);
		}
		if (IS_BSS(phdr[i]) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr[i].p_memsz - phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}
	free(phdr);

	/*
	 * Copy the ELF and section headers.
	 */
	elfp = maxp = roundup(maxp, sizeof(Elf64_Addr));
	if (flags & (LOAD_HDR | COUNT_HDR))
		maxp += sizeof(Elf64_Ehdr);

	if (flags & (LOAD_SYM | COUNT_SYM)) {
		if (lseek(fd, (off_t)elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf64_Shdr);
		shp = malloc(sz);

		if (read(fd, shp, sz) != sz) {
			free(shp);
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(Elf64_Addr));

		ssize_t shstrsz = shp[elf->e_shstrndx].sh_size;
		char *shstr = malloc(shstrsz);
		if (lseek(fd, (off_t)shp[elf->e_shstrndx].sh_offset,
		    SEEK_SET) == -1) {
			free(shstr);
			free(shp);
			return 1;
		}
		if (read(fd, shstr, shstrsz) != shstrsz) {
			free(shstr);
			free(shp);
			return 1;
		}

		/*
		 * Now load the symbol sections themselves. Make sure the
		 * sections are aligned. Don't bother with string tables if
		 * there are no symbol sections.
		 */
		off = roundup((sizeof(Elf64_Ehdr) + sz), sizeof(Elf64_Addr));

		for (havesyms = havelines = i = 0; i < elf->e_shnum; i++)
			if (shp[i].sh_type == SHT_SYMTAB)
				havesyms = 1;

		for (first = 1, i = 0; i < elf->e_shnum; i++) {
			if (shp[i].sh_type == SHT_SYMTAB ||
			    shp[i].sh_type == SHT_STRTAB ||
			    !strcmp(shstr + shp[i].sh_name, ".debug_line")) {
				if (havesyms && (flags & LOAD_SYM)) {
					if (lseek(fd, (off_t)shp[i].sh_offset,
					    SEEK_SET) == -1) {
						free(shstr);
						free(shp);
						return 1;
					}
					if (mread(fd, (uint32_t)maxp,
					    shp[i].sh_size) != shp[i].sh_size) {
						free(shstr);
						free(shp);
						return 1;
					}
				}
				maxp += roundup(shp[i].sh_size,
				    sizeof(Elf64_Addr));
				shp[i].sh_offset = off;
				shp[i].sh_flags |= SHF_ALLOC;
				off += roundup(shp[i].sh_size,
				    sizeof(Elf64_Addr));
				first = 0;
			}
		}
		if (flags & LOAD_SYM) {
			mbcopy((char *)shp, (char *)shpp, sz);
		}
		free(shstr);
		free(shp);
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf64_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		mbcopy((char *)elf, (char *)elfp, sizeof(*elf));
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);

	return 0;
}

/*
 * elf32_exec
 *
 * Load the kernel indicated by 'fd' into the guest physical memory
 * space, at the addresses defined in the ELF header.
 *
 * This function is used for 32 bit kernels.
 *
 * Parameters:
 *  fd: file descriptor of the kernel to load
 *  elf: ELF header of the kernel
 *  marks: array to store the offsets of various kernel structures
 *      (start, bss, etc)
 *  flags: flag value to indicate which section(s) to load (usually
 *      LOAD_ALL)
 *
 * Return values:
 *  0 if successful
 *  1 if unsuccessful
 */
int
elf32_exec(int fd, Elf32_Ehdr *elf, u_long *marks, int flags)
{
	Elf32_Shdr *shp;
	Elf32_Phdr *phdr;
	Elf32_Off off;
	int i;
	ssize_t sz;
	int first;
	int havesyms, havelines;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp;

	sz = elf->e_phnum * sizeof(Elf32_Phdr);
	phdr = malloc(sz);

	if (lseek(fd, (off_t)elf->e_phoff, SEEK_SET) == -1)  {
		free(phdr);
		return 1;
	}

	if (read(fd, phdr, sz) != sz) {
		free(phdr);
		return 1;
	}

	for (first = 1, i = 0; i < elf->e_phnum; i++) {
		if (phdr[i].p_type == PT_OPENBSD_RANDOMIZE) {
			int m;

			/* Fill segment if asked for. */
			if (flags & LOAD_RANDOM) {
				for (pos = 0; pos < phdr[i].p_filesz;
				    pos += m) {
					m = phdr[i].p_filesz - pos;
					marc4random_buf(phdr[i].p_paddr + pos,
					    m);
				}
			}
			if (flags & (LOAD_RANDOM | COUNT_RANDOM)) {
				marks[MARK_RANDOM] = LOADADDR(phdr[i].p_paddr);
				marks[MARK_ERANDOM] =
				    marks[MARK_RANDOM] + phdr[i].p_filesz;
			}
			continue;
		}

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	((p.p_flags & PF_X) == 0)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {

			/* Read in segment. */
			if (lseek(fd, (off_t)phdr[i].p_offset,
			    SEEK_SET) == -1) {
				free(phdr);
				return 1;
			}
			if (mread(fd, phdr[i].p_paddr, phdr[i].p_filesz) !=
			    phdr[i].p_filesz) {
				free(phdr);
				return 1;
			}

			first = 0;
		}

		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT | COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA | COUNT_TEXT)))) {
			pos = phdr[i].p_paddr;
			if (minp > pos)
				minp = pos;
			pos += phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out BSS. */
		if (IS_BSS(phdr[i]) && (flags & LOAD_BSS)) {
			mbzero((phdr[i].p_paddr + phdr[i].p_filesz),
			    phdr[i].p_memsz - phdr[i].p_filesz);
		}
		if (IS_BSS(phdr[i]) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr[i].p_memsz - phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}
	free(phdr);

	/*
	 * Copy the ELF and section headers.
	 */
	elfp = maxp = roundup(maxp, sizeof(Elf32_Addr));
	if (flags & (LOAD_HDR | COUNT_HDR))
		maxp += sizeof(Elf32_Ehdr);

	if (flags & (LOAD_SYM | COUNT_SYM)) {
		if (lseek(fd, (off_t)elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf32_Shdr);
		shp = malloc(sz);

		if (read(fd, shp, sz) != sz) {
			free(shp);
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(Elf32_Addr));

		ssize_t shstrsz = shp[elf->e_shstrndx].sh_size;
		char *shstr = malloc(shstrsz);
		if (lseek(fd, (off_t)shp[elf->e_shstrndx].sh_offset,
		    SEEK_SET) == -1) {
			free(shstr);
			free(shp);
			return 1;
		}
		if (read(fd, shstr, shstrsz) != shstrsz) {
			free(shstr);
			free(shp);
			return 1;
		}

		/*
		 * Now load the symbol sections themselves. Make sure the
		 * sections are aligned. Don't bother with string tables if
		 * there are no symbol sections.
		 */
		off = roundup((sizeof(Elf32_Ehdr) + sz), sizeof(Elf32_Addr));

		for (havesyms = havelines = i = 0; i < elf->e_shnum; i++)
			if (shp[i].sh_type == SHT_SYMTAB)
				havesyms = 1;

		for (first = 1, i = 0; i < elf->e_shnum; i++) {
			if (shp[i].sh_type == SHT_SYMTAB ||
			    shp[i].sh_type == SHT_STRTAB ||
			    !strcmp(shstr + shp[i].sh_name, ".debug_line")) {
				if (havesyms && (flags & LOAD_SYM)) {
					if (lseek(fd, (off_t)shp[i].sh_offset,
					    SEEK_SET) == -1) {
						free(shstr);
						free(shp);
						return 1;
					}
					if (mread(fd, (uint32_t)maxp,
					    shp[i].sh_size) != shp[i].sh_size) {
						free(shstr);
						free(shp);
						return 1;
					}
				}
				maxp += roundup(shp[i].sh_size,
				    sizeof(Elf32_Addr));
				shp[i].sh_offset = off;
				shp[i].sh_flags |= SHF_ALLOC;
				off += roundup(shp[i].sh_size,
				    sizeof(Elf32_Addr));
				first = 0;
			}
		}
		if (flags & LOAD_SYM) {
			mbcopy((void *)shp, (void *)shpp, sz);
		}
		free(shstr);
		free(shp);
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf32_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		mbcopy((void *)elf, (void *)elfp, sizeof(*elf));
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);

	return 0;
}
