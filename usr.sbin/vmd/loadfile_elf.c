/* $NetBSD: loadfile.c,v 1.10 2000/12/03 02:53:04 tsutsui Exp $ */
/* $OpenBSD: loadfile_elf.c,v 1.34 2019/03/01 07:32:29 mlarkin Exp $ */

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

#include <sys/param.h>	/* PAGE_SIZE PAGE_MASK roundup */
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/exec.h>

#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stddef.h>

#include <machine/vmmvar.h>
#include <machine/biosvar.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/pte.h>

#include "loadfile.h"
#include "vmd.h"

union {
	Elf32_Ehdr elf32;
	Elf64_Ehdr elf64;
} hdr;

static void setsegment(struct mem_segment_descriptor *, uint32_t,
    size_t, int, int, int, int);
static int elf32_exec(FILE *, Elf32_Ehdr *, u_long *, int);
static int elf64_exec(FILE *, Elf64_Ehdr *, u_long *, int);
static size_t create_bios_memmap(struct vm_create_params *, bios_memmap_t *);
static uint32_t push_bootargs(bios_memmap_t *, size_t, bios_bootmac_t *);
static size_t push_stack(uint32_t, uint32_t, uint32_t, uint32_t);
static void push_gdt(void);
static void push_pt_32(void);
static void push_pt_64(void);
static void marc4random_buf(paddr_t, int);
static void mbzero(paddr_t, int);
static void mbcopy(void *, paddr_t, int);

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

	memset(&gdtpage, 0, sizeof(gdtpage));

	sd = (struct mem_segment_descriptor *)&gdtpage;

	/*
	 * Create three segment descriptors:
	 *
	 * GDT[0] : null desriptor. "Created" via memset above.
	 * GDT[1] (selector @ 0x8): Executable segment, for CS
	 * GDT[2] (selector @ 0x10): RW Data segment, for DS/ES/SS
	 */
	setsegment(&sd[1], 0, 0xffffffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&sd[2], 0, 0xffffffff, SDT_MEMRWA, SEL_KPL, 1, 1);

	write_mem(GDT_PAGE, gdtpage, PAGE_SIZE);
}

/*
 * push_pt_32
 *
 * Create an identity-mapped page directory hierarchy mapping the first
 * 4GB of physical memory. This is used during bootstrapping i386 VMs on
 * CPUs without unrestricted guest capability.
 */
static void
push_pt_32(void)
{
	uint32_t ptes[1024], i;

	memset(ptes, 0, sizeof(ptes));
	for (i = 0 ; i < 1024; i++) {
		ptes[i] = PG_V | PG_RW | PG_u | PG_PS | ((4096 * 1024) * i);
	}
	write_mem(PML3_PAGE, ptes, PAGE_SIZE);
}

/*
 * push_pt_64
 *
 * Create an identity-mapped page directory hierarchy mapping the first
 * 1GB of physical memory. This is used during bootstrapping 64 bit VMs on
 * CPUs without unrestricted guest capability.
 */
static void
push_pt_64(void)
{
	uint64_t ptes[512], i;

	/* PDPDE0 - first 1GB */
	memset(ptes, 0, sizeof(ptes));
	ptes[0] = PG_V | PML3_PAGE;
	write_mem(PML4_PAGE, ptes, PAGE_SIZE);

	/* PDE0 - first 1GB */
	memset(ptes, 0, sizeof(ptes));
	ptes[0] = PG_V | PG_RW | PG_u | PML2_PAGE;
	write_mem(PML3_PAGE, ptes, PAGE_SIZE);

	/* First 1GB (in 2MB pages) */
	memset(ptes, 0, sizeof(ptes));
	for (i = 0 ; i < 512; i++) {
		ptes[i] = PG_V | PG_RW | PG_u | PG_PS | ((2048 * 1024) * i);
	}
	write_mem(PML2_PAGE, ptes, PAGE_SIZE);
}

/*
 * loadfile_elf
 *
 * Loads an ELF kernel to it's defined load address in the guest VM.
 * The kernel is loaded to its defined start point as set in the ELF header.
 *
 * Parameters:
 *  fp: file of a kernel file to load
 *  vcp: the VM create parameters, holding the exact memory map
 *  (out) vrs: register state to set on init for this kernel
 *  bootdev: the optional non-default boot device
 *  howto: optional boot flags for the kernel
 *
 * Return values:
 *  0 if successful
 *  various error codes returned from read(2) or loadelf functions
 */
int
loadfile_elf(FILE *fp, struct vm_create_params *vcp,
    struct vcpu_reg_state *vrs, uint32_t bootdev, uint32_t howto,
    unsigned int bootdevice)
{
	int r, is_i386 = 0;
	uint32_t bootargsz;
	size_t n, stacksize;
	u_long marks[MARK_MAX];
	bios_memmap_t memmap[VMM_MAX_MEM_RANGES + 1];
	bios_bootmac_t bm, *bootmac = NULL;

	if ((r = fread(&hdr, 1, sizeof(hdr), fp)) != sizeof(hdr))
		return 1;

	memset(&marks, 0, sizeof(marks));
	if (memcmp(hdr.elf32.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
		r = elf32_exec(fp, &hdr.elf32, marks, LOAD_ALL);
		is_i386 = 1;
	} else if (memcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
		r = elf64_exec(fp, &hdr.elf64, marks, LOAD_ALL);
	} else
		errno = ENOEXEC;

	if (r)
		return (r);

	push_gdt();

	if (is_i386) {
		push_pt_32();
		/* Reconfigure the default flat-64 register set for 32 bit */
		vrs->vrs_crs[VCPU_REGS_CR3] = PML3_PAGE;
		vrs->vrs_crs[VCPU_REGS_CR4] = CR4_PSE;
		vrs->vrs_msrs[VCPU_REGS_EFER] = 0ULL;
	}
	else
		push_pt_64();

	if (bootdevice & VMBOOTDEV_NET) {
		bootmac = &bm;
		memcpy(bootmac, vcp->vcp_macs[0], ETHER_ADDR_LEN);
	}
	n = create_bios_memmap(vcp, memmap);
	bootargsz = push_bootargs(memmap, n, bootmac);
	stacksize = push_stack(bootargsz, marks[MARK_END], bootdev, howto);

	vrs->vrs_gprs[VCPU_REGS_RIP] = (uint64_t)marks[MARK_ENTRY];
	vrs->vrs_gprs[VCPU_REGS_RSP] = (uint64_t)(STACK_PAGE + PAGE_SIZE) - stacksize;
	vrs->vrs_gdtr.vsi_base = GDT_PAGE;

	log_debug("%s: loaded ELF kernel", __func__);

	return (0);
}

/*
 * create_bios_memmap
 *
 * Construct a memory map as returned by the BIOS INT 0x15, e820 routine.
 *
 * Parameters:
 *  vcp: the VM create parameters, containing the memory map passed to vmm(4)
 *   memmap (out): the BIOS memory map
 *
 * Return values:
 * Number of bios_memmap_t entries, including the terminating nul-entry.
 */
static size_t
create_bios_memmap(struct vm_create_params *vcp, bios_memmap_t *memmap)
{
	size_t i, n = 0, sz;
	paddr_t gpa;
	struct vm_mem_range *vmr;

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		gpa = vmr->vmr_gpa;
		sz = vmr->vmr_size;

		/*
		 * Make sure that we do not mark the ROM/video RAM area in the
		 * low memory as physcal memory available to the kernel.
		 */
		if (gpa < 0x100000 && gpa + sz > LOWMEM_KB * 1024) {
			if (gpa >= LOWMEM_KB * 1024)
				sz = 0;
			else
				sz = LOWMEM_KB * 1024 - gpa;
		}

		if (sz != 0) {
			memmap[n].addr = gpa;
			memmap[n].size = sz;
			memmap[n].type = 0x1;	/* Type 1 : Normal memory */
			n++;
		}
	}

	/* Null mem map entry to denote the end of the ranges */
	memmap[n].addr = 0x0;
	memmap[n].size = 0x0;
	memmap[n].type = 0x0;
	n++;

	return (n);
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
 *  memmap: the BIOS memory map
 *  n: number of entries in memmap
 *
 * Return values:
 *  The size of the bootargs
 */
static uint32_t
push_bootargs(bios_memmap_t *memmap, size_t n, bios_bootmac_t *bootmac)
{
	uint32_t memmap_sz, consdev_sz, bootmac_sz, i;
	bios_consdev_t consdev;
	uint32_t ba[1024];

	memmap_sz = 3 * sizeof(int) + n * sizeof(bios_memmap_t);
	ba[0] = 0x0;    /* memory map */
	ba[1] = memmap_sz;
	ba[2] = memmap_sz;	/* next */
	memcpy(&ba[3], memmap, n * sizeof(bios_memmap_t));
	i = memmap_sz / sizeof(int);

	/* Serial console device, COM1 @ 0x3f8 */
	consdev.consdev = makedev(8, 0);	/* com1 @ 0x3f8 */
	consdev.conspeed = 115200;
	consdev.consaddr = 0x3f8;
	consdev.consfreq = 0;

	consdev_sz = 3 * sizeof(int) + sizeof(bios_consdev_t);
	ba[i] = 0x5;   /* consdev */
	ba[i + 1] = consdev_sz;
	ba[i + 2] = consdev_sz;
	memcpy(&ba[i + 3], &consdev, sizeof(bios_consdev_t));
	i += consdev_sz / sizeof(int);

	if (bootmac) {
		bootmac_sz = 3 * sizeof(int) + (sizeof(bios_bootmac_t) + 3) & ~3;
		ba[i] = 0x7;   /* bootmac */
		ba[i + 1] = bootmac_sz;
		ba[i + 2] = bootmac_sz;
		memcpy(&ba[i + 3], bootmac, sizeof(bios_bootmac_t));
		i += bootmac_sz / sizeof(int);
	} 

	ba[i++] = 0xFFFFFFFF; /* BOOTARG_END */

	write_mem(BOOTARGS_PAGE, ba, PAGE_SIZE);

	return (i * sizeof(int));
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
 *  TOS - 0x8	size of low memory (biosbasemem: kernel uses BIOS map only if 0)
 *  TOS - 0xc	size of high memory (biosextmem, not used by kernel at all)
 *  TOS - 0x10	kernel 'end' symbol value
 *  TOS - 0x14	version of bootarg API
 *
 * Parameters:
 *  bootargsz: size of boot arguments
 *  end: kernel 'end' symbol value
 *  bootdev: the optional non-default boot device
 *  howto: optional boot flags for the kernel
 *
 * Return values:
 *  size of the stack
 */
static size_t
push_stack(uint32_t bootargsz, uint32_t end, uint32_t bootdev, uint32_t howto)
{
	uint32_t stack[1024];
	uint16_t loc;

	memset(&stack, 0, sizeof(stack));
	loc = 1024;

	if (bootdev == 0)
		bootdev = MAKEBOOTDEV(0x4, 0, 0, 0, 0); /* bootdev: sd0a */

	stack[--loc] = BOOTARGS_PAGE;
	stack[--loc] = bootargsz;
	stack[--loc] = 0; /* biosbasemem */
	stack[--loc] = 0; /* biosextmem */
	stack[--loc] = end;
	stack[--loc] = 0x0e;
	stack[--loc] = bootdev;
	stack[--loc] = howto;

	write_mem(STACK_PAGE, &stack, PAGE_SIZE);

	return (1024 - (loc - 1)) * sizeof(uint32_t);
}

/*
 * mread
 *
 * Reads 'sz' bytes from the file whose descriptor is provided in 'fd'
 * into the guest address space at paddr 'addr'.
 *
 * Parameters:
 *  fd: file descriptor of the kernel image file to read from.
 *  addr: guest paddr_t to load to
 *  sz: number of bytes to load
 *
 * Return values:
 *  returns 'sz' if successful, or 0 otherwise.
 */
size_t
mread(FILE *fp, paddr_t addr, size_t sz)
{
	size_t ct;
	size_t i, rd, osz;
	char buf[PAGE_SIZE];

	/*
	 * break up the 'sz' bytes into PAGE_SIZE chunks for use with
	 * write_mem
	 */
	ct = 0;
	rd = 0;
	osz = sz;
	if ((addr & PAGE_MASK) != 0) {
		memset(buf, 0, sizeof(buf));
		if (sz > PAGE_SIZE)
			ct = PAGE_SIZE - (addr & PAGE_MASK);
		else
			ct = sz;

		if (fread(buf, 1, ct, fp) != ct) {
			log_warn("%s: error %d in mread", __progname, errno);
			return (0);
		}
		rd += ct;

		if (write_mem(addr, buf, ct))
			return (0);

		addr += ct;
	}

	sz = sz - ct;

	if (sz == 0)
		return (osz);

	for (i = 0; i < sz; i += PAGE_SIZE, addr += PAGE_SIZE) {
		memset(buf, 0, sizeof(buf));
		if (i + PAGE_SIZE > sz)
			ct = sz - i;
		else
			ct = PAGE_SIZE;

		if (fread(buf, 1, ct, fp) != ct) {
			log_warn("%s: error %d in mread", __progname, errno);
			return (0);
		}
		rd += ct;

		if (write_mem(addr, buf, ct))
			return (0);
	}

	return (osz);
}

/*
 * marc4random_buf
 *
 * load 'sz' bytes of random data into the guest address space at paddr
 * 'addr'.
 *
 * Parameters:
 *  addr: guest paddr_t to load random bytes into
 *  sz: number of random bytes to load
 *
 * Return values:
 *  nothing
 */
static void
marc4random_buf(paddr_t addr, int sz)
{
	int i, ct;
	char buf[PAGE_SIZE];

	/*
	 * break up the 'sz' bytes into PAGE_SIZE chunks for use with
	 * write_mem
	 */
	ct = 0;
	if (addr % PAGE_SIZE != 0) {
		memset(buf, 0, sizeof(buf));
		ct = PAGE_SIZE - (addr % PAGE_SIZE);

		arc4random_buf(buf, ct);

		if (write_mem(addr, buf, ct))
			return;

		addr += ct;
	}

	for (i = 0; i < sz; i+= PAGE_SIZE, addr += PAGE_SIZE) {
		memset(buf, 0, sizeof(buf));
		if (i + PAGE_SIZE > sz)
			ct = sz - i;
		else
			ct = PAGE_SIZE;

		arc4random_buf(buf, ct);

		if (write_mem(addr, buf, ct))
			return;
	}
}

/*
 * mbzero
 *
 * load 'sz' bytes of zeros into the guest address space at paddr
 * 'addr'.
 *
 * Parameters:
 *  addr: guest paddr_t to zero
 *  sz: number of zero bytes to store
 *
 * Return values:
 *  nothing
 */
static void
mbzero(paddr_t addr, int sz)
{
	if (write_mem(addr, NULL, sz))
		return;
}

/*
 * mbcopy
 *
 * copies 'sz' bytes from buffer 'src' to guest paddr 'dst'.
 *
 * Parameters:
 *  src: source buffer to copy from
 *  dst: destination guest paddr_t to copy to
 *  sz: number of bytes to copy
 *
 * Return values:
 *  nothing
 */
static void
mbcopy(void *src, paddr_t dst, int sz)
{
	write_mem(dst, src, sz);
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
static int
elf64_exec(FILE *fp, Elf64_Ehdr *elf, u_long *marks, int flags)
{
	Elf64_Shdr *shp;
	Elf64_Phdr *phdr;
	Elf64_Off off;
	int i;
	size_t sz;
	int first;
	int havesyms, havelines;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp;

	sz = elf->e_phnum * sizeof(Elf64_Phdr);
	phdr = malloc(sz);

	if (fseeko(fp, (off_t)elf->e_phoff, SEEK_SET) == -1)  {
		free(phdr);
		return 1;
	}

	if (fread(phdr, 1, sz, fp) != sz) {
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
			if (fseeko(fp, (off_t)phdr[i].p_offset,
			    SEEK_SET) == -1) {
				free(phdr);
				return 1;
			}
			if (mread(fp, phdr[i].p_paddr, phdr[i].p_filesz) !=
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
	elfp = maxp = roundup(maxp, sizeof(Elf64_Addr));
	if (flags & (LOAD_HDR | COUNT_HDR))
		maxp += sizeof(Elf64_Ehdr);

	if (flags & (LOAD_SYM | COUNT_SYM)) {
		if (fseeko(fp, (off_t)elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf64_Shdr);
		shp = malloc(sz);

		if (fread(shp, 1, sz, fp) != sz) {
			free(shp);
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(Elf64_Addr));

		size_t shstrsz = shp[elf->e_shstrndx].sh_size;
		char *shstr = malloc(shstrsz);
		if (fseeko(fp, (off_t)shp[elf->e_shstrndx].sh_offset,
		    SEEK_SET) == -1) {
			free(shstr);
			free(shp);
			return 1;
		}
		if (fread(shstr, 1, shstrsz, fp) != shstrsz) {
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
			    !strcmp(shstr + shp[i].sh_name, ".debug_line") ||
			    !strcmp(shstr + shp[i].sh_name, ELF_CTF)) {
				if (havesyms && (flags & LOAD_SYM)) {
					if (fseeko(fp, (off_t)shp[i].sh_offset,
					    SEEK_SET) == -1) {
						free(shstr);
						free(shp);
						return 1;
					}
					if (mread(fp, maxp,
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
			mbcopy(shp, shpp, sz);
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
		mbcopy(elf, elfp, sizeof(*elf));
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
static int
elf32_exec(FILE *fp, Elf32_Ehdr *elf, u_long *marks, int flags)
{
	Elf32_Shdr *shp;
	Elf32_Phdr *phdr;
	Elf32_Off off;
	int i;
	size_t sz;
	int first;
	int havesyms, havelines;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp;

	sz = elf->e_phnum * sizeof(Elf32_Phdr);
	phdr = malloc(sz);

	if (fseeko(fp, (off_t)elf->e_phoff, SEEK_SET) == -1)  {
		free(phdr);
		return 1;
	}

	if (fread(phdr, 1, sz, fp) != sz) {
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
			if (fseeko(fp, (off_t)phdr[i].p_offset,
			    SEEK_SET) == -1) {
				free(phdr);
				return 1;
			}
			if (mread(fp, phdr[i].p_paddr, phdr[i].p_filesz) !=
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
		if (fseeko(fp, (off_t)elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf32_Shdr);
		shp = malloc(sz);

		if (fread(shp, 1, sz, fp) != sz) {
			free(shp);
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(Elf32_Addr));

		size_t shstrsz = shp[elf->e_shstrndx].sh_size;
		char *shstr = malloc(shstrsz);
		if (fseeko(fp, (off_t)shp[elf->e_shstrndx].sh_offset,
		    SEEK_SET) == -1) {
			free(shstr);
			free(shp);
			return 1;
		}
		if (fread(shstr, 1, shstrsz, fp) != shstrsz) {
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
					if (fseeko(fp, (off_t)shp[i].sh_offset,
					    SEEK_SET) == -1) {
						free(shstr);
						free(shp);
						return 1;
					}
					if (mread(fp, maxp,
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
			mbcopy(shp, shpp, sz);
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
		mbcopy(elf, elfp, sizeof(*elf));
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);

	return 0;
}
