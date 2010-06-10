/*	$OpenBSD: machdep.c,v 1.24 2010/06/10 17:54:13 deraadt Exp $	*/
/*	$NetBSD: machdep.c,v 1.4 1996/10/16 19:33:11 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/bat.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/pio.h>
#include <machine/powerpc.h>
#include <machine/trap.h>

#include <dev/cons.h>

#include <dev/ic/comvar.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

/*
 * Global variables used here and there
 */
extern struct user *proc0paddr;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

struct bat battable[16];

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

int ppc_malloc_ok = 0;

char *bootpath;
char bootpathbuf[512];

struct bd_info {
	unsigned long	bi_memstart;
	unsigned long	bi_memsize;
	unsigned long	bi_flashstart;
	unsigned long	bi_flashsize;
	unsigned long	bi_flashoffset;
	unsigned long	bi_sramstart;
	unsigned long	bi_sramsize;
	unsigned long	bi_immr_base;
	unsigned long	bi_bootflags;
	unsigned long	bi_ip_addr;
	unsigned char	bi_enetaddr[6];
	unsigned long	bi_ethspeed;
} bootinfo;

extern struct bd_info **fwargsave;
extern struct fdt_head *fwfdtsave;

void uboot_mem_regions(struct mem_region **, struct mem_region **);
void uboot_vmon(void);

struct firmware uboot_firmware = {
	uboot_mem_regions,
	NULL,
	NULL,
	uboot_vmon
};

struct firmware *fw = &uboot_firmware;

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

void dumpsys(void);
int lcsplx(int ipl);
void myetheraddr(u_char *);

int bus_mem_add_mapping(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
bus_addr_t bus_space_unmap_p(bus_space_tag_t, bus_space_handle_t,  bus_size_t);
void bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 * Extent maps to manage I/O. Allocate storage for 8 regions in each,
 * initially. Later devio_malloc_safe will indicate that it's safe to
 * use malloc() to dynamically allocate region descriptors.
 */
static long devio_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof (long)];
struct extent *devio_ex;
static int devio_malloc_safe = 0;

void initppc(u_int, u_int, char *);

void prom_printf(const char *, ...);

extern struct ppc_bus_space mainbus_bus_space;

void
initppc(u_int startkernel, u_int endkernel, char *args)
{
	extern void *trapcode; extern int trapsize;
	extern void *extint; extern int extsize;
	extern void *dsitrap; extern int dsisize;
	extern void *isitrap; extern int isisize;
	extern void *alitrap; extern int alisize;
	extern void *decrint; extern int decrsize;
	extern void *tlbimiss; extern int tlbimsize;
	extern void *tlbdlmiss; extern int tlbdlmsize;
	extern void *tlbdsmiss; extern int tlbdsmsize;
#ifdef DDB
	extern void *ddblow; extern int ddbsize;
#endif
	extern void *msgbuf_addr;
	int exc, scratch;
	void *node;

	extern char __bss_start[], __end[];
	bzero(__bss_start, __end - __bss_start);

	/* Make a copy of the args! */
	strlcpy(bootpathbuf, args ? args : "wd0a", sizeof bootpathbuf);

	if (fwfdtsave == NULL) {
		/*
		 * We were loaded by an old U-Boot that didn't provide
		 * a flattened device tree.  It should have provided a
		 * valid bootinfo structure which we'll use to build
		 * such a device tree ourselves.
		 *
		 * XXX We don't build a flattened device tree yet.
		 */
		memcpy(&bootinfo, *fwargsave, sizeof bootinfo);

		extern uint8_t dt_blob_start[];
		fdt_init(&dt_blob_start);
	}

	if (fwfdtsave && fwfdtsave->fh_magic == FDT_MAGIC) {
		/* 
		 * Save the FDT firmware blob passed by the bootloader
		 * before we zero all memory.
		 * 
		 */
		void *fdt = (void *)endkernel;
		memcpy(fdt, fwfdtsave, fwfdtsave->fh_size);
		endkernel += fwfdtsave->fh_size;

		fdt_init(fdt);

		/*
		 * XXX Create a fake bootinfo structure if we were
		 * loaded by RouterBOOT.
		 */
		node = fdt_find_node("/memory");
		if (node) {
			char *reg;

			if (fdt_node_property(node, "reg", &reg)) {
				bootinfo.bi_memstart = *(u_int32_t *)reg;
				bootinfo.bi_memsize = *((u_int32_t *)reg + 1);
			}
		}
		node = fdt_find_node("/soc8343");
		if (node) {
			char *reg;

			if (fdt_node_property(node, "reg", &reg))
				bootinfo.bi_immr_base = *(u_int32_t *)reg;
		}
		node = fdt_find_node("/soc8343/ethernet");
		if (node) {
			char *addr;

			if (fdt_node_property(node, "mac-address", &addr))
				memcpy(bootinfo.bi_enetaddr, addr, 6);
		}
	}

	proc0.p_cpu = &cpu_info[0];
	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);

	curpcb = &proc0paddr->u_pcb;

	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

	ppc_check_procid();

	/*
	 * Initialize BAT registers to unmapped to not generate
	 * overlapping mappings below.
	 */
	ppc_mtibat0u(0);
	ppc_mtibat1u(0);
	ppc_mtibat2u(0);
	ppc_mtibat3u(0);
	ppc_mtdbat0u(0);
	ppc_mtdbat1u(0);
	ppc_mtdbat2u(0);
	ppc_mtdbat3u(0);

	/*
	 * Set up initial BAT table to only map the lowest 256 MB area
	 */
	battable[0].batl = BATL(0x00000000, BAT_M);
#if 0
	battable[0].batu = BATU(0x00000000);
#else
	battable[0].batu = 0x0ffe; /* XXX only map 128MB for now */
#endif

	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* IBAT0 used for initial 256 MB segment */
	ppc_mtibat0l(battable[0].batl);
	ppc_mtibat0u(battable[0].batu);

	/* DBAT0 used similar */
	ppc_mtdbat0l(battable[0].batl);
	ppc_mtdbat0u(battable[0].batu);

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100) {
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)EXC_DSI, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)EXC_ISI, (size_t)&isisize);
			break;
		case EXC_ALI:
			bcopy(&alitrap, (void *)EXC_ALI, (size_t)&alisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)EXC_IMISS, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)EXC_DLMISS, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)EXC_DSMISS, (size_t)&tlbdsmsize);
			break;
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
#if defined(DDB)
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
#endif
			break;
		}
	}

	syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);


	uvmexp.pagesize = 4096;
	uvm_setpagesize();

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	/* now that we know physmem size, map physical memory with BATs */
	if (physmem > atop(0x10000000)) {
		battable[0x1].batl = BATL(0x10000000, BAT_M);
		battable[0x1].batu = BATU(0x10000000);
	}
	if (physmem > atop(0x20000000)) {
		battable[0x2].batl = BATL(0x20000000, BAT_M);
		battable[0x2].batu = BATU(0x20000000);
	}
	if (physmem > atop(0x30000000)) {
		battable[0x3].batl = BATL(0x30000000, BAT_M);
		battable[0x3].batu = BATU(0x30000000);
	}
	if (physmem > atop(0x40000000)) {
		battable[0x4].batl = BATL(0x40000000, BAT_M);
		battable[0x4].batu = BATU(0x40000000);
	}
	if (physmem > atop(0x50000000)) {
		battable[0x5].batl = BATL(0x50000000, BAT_M);
		battable[0x5].batu = BATU(0x50000000);
	}
	if (physmem > atop(0x60000000)) {
		battable[0x6].batl = BATL(0x60000000, BAT_M);
		battable[0x6].batu = BATU(0x60000000);
	}
	if (physmem > atop(0x70000000)) {
		battable[0x7].batl = BATL(0x70000000, BAT_M);
		battable[0x7].batu = BATU(0x70000000);
	}

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	(fw->vmon)();

	__asm__ volatile ("eieio; mfmsr %0; ori %0,%0,%1; mtmsr %0; sync;isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

	/*
	 * use the memory provided by pmap_bootstrap for message buffer
	 */
	initmsgbuf(msgbuf_addr, MSGBUFSIZE);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_AUTOBOOT;

	/*
	 * Parse arg string.
	 */
	bootpath = &bootpathbuf[0];
	while (*++bootpath && *bootpath != ' ');
	if (*bootpath) {
		*bootpath++ = 0;
		while (*bootpath) {
			switch (*bootpath++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			case 'c':
				boothowto |= RB_CONFIG;
				break;
			default:
				break;
			}
		}
	}
	bootpath = &bootpathbuf[0];

#ifdef DDB
	ddb_init();
#endif

	/*
	 * Adjust base of internal memory mapped registers.
	 */
	mainbus_bus_space.bus_base = bootinfo.bi_immr_base;

	devio_ex = extent_create("devio", 0x80000000, 0xffffffff, M_DEVBUF,
		(caddr_t)devio_ex_storage, sizeof(devio_ex_storage),
		EX_NOCOALESCE|EX_NOWAIT);

	/*
	 * Initialize console.
	 */
	extern int comconsrate;
	comconsfreq = 266666666;
	comconsrate = 115200;
	comconsaddr = 0x00004500;
	comconsiot = &mainbus_bus_space;

	node = fdt_find_node("/chosen");
	if (node) {
		char *console;

		fdt_node_property(node, "linux,stdout-path", &console);
		node = fdt_find_node(console);
		if (node) {
			char *freq;
			char *reg;

			if (fdt_node_property(node, "clock-frequency", &freq))
				comconsfreq = *(u_int32_t *)freq;
			if (fdt_node_property(node, "reg", &reg))
				comconsaddr = *(u_int32_t *)reg;
		}
	}

	consinit();

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
}

void
dumpsys(void)
{
}

int imask[IPL_NUM];

int
lcsplx(int ipl)
{
	return spllower(ipl);
}

/* BUS functions */
int
bus_space_map(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	int error;

	if  (POWERPC_BUS_TAG_BASE(t) == 0) {
		/* if bus has base of 0 fail. */
		return EINVAL;
	}
	bpa |= POWERPC_BUS_TAG_BASE(t);
	if ((error = extent_alloc_region(devio_ex, bpa, size, EX_NOWAIT |
	    (ppc_malloc_ok ? EX_MALLOCOK : 0))))
		return error;

	if ((error = bus_mem_add_mapping(bpa, size, flags, bshp))) {
		if (extent_free(devio_ex, bpa, size, EX_NOWAIT |
			(ppc_malloc_ok ? EX_MALLOCOK : 0)))
		{
			printf("bus_space_map: pa 0x%lx, size 0x%x\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}
	return 0;
}

bus_addr_t
bus_space_unmap_p(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	bus_addr_t paddr;

	pmap_extract(pmap_kernel(), bsh, &paddr);
	bus_space_unmap((t), (bsh), (size));
	return paddr ;
}

void
bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	bus_addr_t sva;
	bus_size_t off, len;
	bus_addr_t bpa;

	/* should this verify that the proper size is freed? */
	sva = trunc_page(bsh);
	off = bsh - sva;
	len = size+off;

	if (pmap_extract(pmap_kernel(), sva, &bpa) == TRUE) {
		if (extent_free(devio_ex, bpa | (bsh & PAGE_MASK), size, EX_NOWAIT |
			(ppc_malloc_ok ? EX_MALLOCOK : 0)))
		{
			printf("bus_space_map: pa 0x%lx, size 0x%x\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}
	/* do not free memory which was stolen from the vm system */
	if (ppc_malloc_ok &&
	    ((sva >= VM_MIN_KERNEL_ADDRESS) && (sva < VM_MAX_KERNEL_ADDRESS)))
		uvm_km_free(phys_map, sva, len);
	else {
		pmap_remove(pmap_kernel(), sva, sva+len);
		pmap_update(pmap_kernel());
	}
}

vaddr_t ppc_kvm_stolen = VM_KERN_ADDRESS_SIZE;

int
bus_mem_add_mapping(bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	bus_addr_t vaddr;
	bus_addr_t spa, epa;
	bus_size_t off;
	int len;

	spa = trunc_page(bpa);
	epa = bpa + size;
	off = bpa - spa;
	len = size+off;

#if 0
	if (epa <= spa) {
		panic("bus_mem_add_mapping: overflow");
	}
#endif
	if (ppc_malloc_ok == 0) {
		bus_size_t alloc_size;

		/* need to steal vm space before kernel vm is initialized */
		alloc_size = round_page(len);

		vaddr = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
		ppc_kvm_stolen += alloc_size;
		if (ppc_kvm_stolen > PPC_SEGMENT_LENGTH) {
			panic("ppc_kvm_stolen, out of space");
		}
	} else {
		vaddr = uvm_km_kmemalloc(phys_map, NULL, len,
		    UVM_KMF_NOWAIT|UVM_KMF_VALLOC);
		if (vaddr == 0)
			return (ENOMEM);
	}
	*bshp = vaddr + off;
#ifdef DEBUG_BUS_MEM_ADD_MAPPING
	printf("mapping %x size %x to %x vbase %x\n",
		bpa, size, *bshp, spa);
#endif
	for (; len > 0; len -= PAGE_SIZE) {
		pmap_kenter_cache(vaddr, spa, VM_PROT_READ | VM_PROT_WRITE,
		    (flags & BUS_SPACE_MAP_CACHEABLE) ?
		      PMAP_CACHE_WT : PMAP_CACHE_CI);
		spa += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}
	return 0;
}

int
bus_space_alloc(bus_space_tag_t tag, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *addrp, bus_space_handle_t *handlep)
{

	panic("bus_space_alloc: unimplemented");
}

void
bus_space_free(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{

	panic("bus_space_free: unimplemented");
}

void *
mapiodev(paddr_t pa, psize_t len)
{
	paddr_t spa;
	vaddr_t vaddr, va;
	int off;
	int size;

	spa = trunc_page(pa);
	off = pa - spa;
	size = round_page(off+len);
	if (ppc_malloc_ok == 0) {
		/* need to steal vm space before kernel vm is initialized */
		va = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
		ppc_kvm_stolen += size;
		if (ppc_kvm_stolen > PPC_SEGMENT_LENGTH) {
			panic("ppc_kvm_stolen, out of space");
		}
	} else {
		va = uvm_km_kmemalloc(phys_map, NULL, size,
		    UVM_KMF_NOWAIT|UVM_KMF_VALLOC);
	}

	if (va == 0)
		return NULL;

	for (vaddr = va; size > 0; size -= PAGE_SIZE) {
		pmap_kenter_cache(vaddr, spa,
			VM_PROT_READ | VM_PROT_WRITE, PMAP_CACHE_DEFAULT);
		spa += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}
	return (void *) (va+off);
}

void
unmapiodev(void *kva, psize_t p_size)
{
	vaddr_t vaddr;
	int size;

	size = p_size;

	vaddr = trunc_page((vaddr_t)kva);

	uvm_km_free_wakeup(phys_map, vaddr, size);

	for (; size > 0; size -= PAGE_SIZE) {
		pmap_remove(pmap_kernel(), vaddr,  vaddr+PAGE_SIZE-1);
		vaddr += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}


/*
 * probably should be ppc_space_copy
 */

#define _CONCAT(A,B) A ## B
#define __C(A,B)	_CONCAT(A,B)

#define BUS_SPACE_COPY_N(BYTES,TYPE)					\
void									\
__C(bus_space_copy_,BYTES)(void *v, bus_space_handle_t h1,		\
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2,		\
    bus_size_t c)							\
{									\
	TYPE *src, *dst;						\
	int i;								\
									\
	src = (TYPE *) (h1+o1);						\
	dst = (TYPE *) (h2+o2);						\
									\
	if (h1 == h2 && o2 > o1)					\
		for (i = c-1; i >= 0; i--)				\
			dst[i] = src[i];				\
	else								\
		for (i = 0; i < c; i++)					\
			dst[i] = src[i];				\
}
BUS_SPACE_COPY_N(1,u_int8_t)
BUS_SPACE_COPY_N(2,u_int16_t)
BUS_SPACE_COPY_N(4,u_int32_t)

void
bus_space_set_region_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int8_t val, bus_size_t c)
{
	u_int8_t *dst;
	int i;

	dst = (u_int8_t *) (h+o);

	for (i = 0; i < c; i++)
		dst[i] = val;
}

void
bus_space_set_region_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int16_t val, bus_size_t c)
{
	u_int16_t *dst;
	int i;

	dst = (u_int16_t *) (h+o);
	val = swap16(val);

	for (i = 0; i < c; i++)
		dst[i] = val;
}
void
bus_space_set_region_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int32_t val, bus_size_t c)
{
	u_int32_t *dst;
	int i;

	dst = (u_int32_t *) (h+o);
	val = swap32(val);

	for (i = 0; i < c; i++)
		dst[i] = val;
}

#define BUS_SPACE_READ_RAW_MULTI_N(BYTES,SHIFT,TYPE)			\
void									\
__C(bus_space_read_raw_multi_,BYTES)(bus_space_tag_t bst,		\
    bus_space_handle_t h, bus_addr_t o, u_int8_t *dst, bus_size_t size)	\
{									\
	TYPE *src;							\
	TYPE *rdst = (TYPE *)dst;					\
	int i;								\
	int count = size >> SHIFT;					\
									\
	src = (TYPE *)(h+o);						\
	for (i = 0; i < count; i++) {					\
		rdst[i] = *src;						\
		__asm__("eieio");					\
	}								\
}
BUS_SPACE_READ_RAW_MULTI_N(2,1,u_int16_t)
BUS_SPACE_READ_RAW_MULTI_N(4,2,u_int32_t)

#define BUS_SPACE_WRITE_RAW_MULTI_N(BYTES,SHIFT,TYPE)			\
void									\
__C(bus_space_write_raw_multi_,BYTES)( bus_space_tag_t bst,		\
    bus_space_handle_t h, bus_addr_t o, const u_int8_t *src,		\
    bus_size_t size)							\
{									\
	int i;								\
	TYPE *dst;							\
	TYPE *rsrc = (TYPE *)src;					\
	int count = size >> SHIFT;					\
									\
	dst = (TYPE *)(h+o);						\
	for (i = 0; i < count; i++) {					\
		*dst = rsrc[i];						\
		__asm__("eieio");					\
	}								\
}

BUS_SPACE_WRITE_RAW_MULTI_N(2,1,u_int16_t)
BUS_SPACE_WRITE_RAW_MULTI_N(4,2,u_int32_t)

int
bus_space_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	vaddr_t minaddr, maxaddr;

	proc0.p_addr = proc0paddr;

	printf("%s", version);

	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr, 16 * NCARGS,
	    VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);
	ppc_malloc_ok = 1;

	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024 / 1024);

	/*
	 * Set up the buffers.
	 */
	bufinit();

	devio_malloc_safe = 1;
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	static int cons_initted = 0;

	if (cons_initted)
		return;
	cninit();
	cons_initted = 1;
}

/*
 * Clear registers on exec
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	u_int32_t newstack;
	u_int32_t pargs;
	u_int32_t args[4];

	struct trapframe *tf = trapframe(p);
	pargs = -roundup(-stack + 8, 16);
	newstack = (u_int32_t)(pargs - 32);

	copyin ((void *)(VM_MAX_ADDRESS-0x10), &args, 0x10);

	bzero(tf, sizeof *tf);
	tf->fixreg[1] = newstack;
	tf->fixreg[3] = retval[0] = args[1];	/* XXX */
	tf->fixreg[4] = retval[1] = args[0];	/* XXX */
	tf->fixreg[5] = args[2];		/* XXX */
	tf->fixreg[6] = args[3];		/* XXX */
	tf->srr0 = pack->ep_entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

/*
 * Send a signal to process.
 */
void
sendsig(sig_t catcher, int sig, int mask, u_long code, int type,
    union sigval val)
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oldonstack;

	frame.sf_signum = sig;

	tf = trapframe(p);
	oldonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate stack space for signal handler.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK)
	    && !oldonstack
	    && (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp
					 + psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)tf->fixreg[1];

	fp = (struct sigframe *)((int)(fp - 1) & ~0xf);

	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_onstack = oldonstack;
	frame.sf_sc.sc_mask = mask;
	frame.sf_sip = NULL;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (psp->ps_siginfo & sigmask(sig)) {
		frame.sf_sip = &fp->sf_si;
		initsiginfo(&frame.sf_si, sig, code, type, val);
	}
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);


	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : 0;
	tf->fixreg[5] = (int)&fp->sf_sc;
	tf->srr0 = p->p_sigcode;

#if WHEN_WE_ONLY_FLUSH_DATA_WHEN_DOING_PMAP_ENTER
	pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),tf->srr0, &pa);
	syncicache(pa, (p->p_emul->e_esigcode - p->p_emul->e_sigcode));
#endif
}

/*
 * System call to cleanup state after a signal handler returns.
 */
int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc;
	struct trapframe *tf;
	int error;

	if ((error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc)))
		return error;
	tf = trapframe(p);
	if ((sc.sc_frame.srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC))
		return EINVAL;
	bcopy(&sc.sc_frame, tf, sizeof *tf);
	if (sc.sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = sc.sc_mask & ~sigcantmask;
	return EJUSTRETURN;
}

/*
 * Machine dependent system variables.
 * None for now.
 */
int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return ENOTDIR;
	switch (name[0]) {
	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case CPU_ALTIVEC:
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
	default:
		return EOPNOTSUPP;
	}
}

u_long dumpmag = 0x04959fca;			/* magic number */
int dumpsize = 0;			/* size of dump in pages */
long dumplo = -1;			/* blocks */

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void dumpconf(void);

void
dumpconf(void)
{
	int nblks;	/* size of dump area */
	int i;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	/* Always skip the first block, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

        for (i = 0; i < ndumpmem; i++)
		dumpsize = max(dumpsize, dumpmem[i].end);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo - 1))
		dumpsize = dtoc(nblks - dumplo - 1);
	if (dumplo < nblks - ctod(dumpsize) - 1)
		dumplo = nblks - ctod(dumpsize) - 1;

}

#define BYTES_PER_DUMP  (PAGE_SIZE)  /* must be a multiple of pagesize */
vaddr_t dumpspace;

int
reserve_dumppages(caddr_t p)
{
	dumpspace = (vaddr_t)p;
	return BYTES_PER_DUMP;
}

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
void
boot(int howto)
{
	static int syncing;

	boothowto = howto;
	if (!cold && !(howto & RB_NOSYNC) && !syncing) {
		syncing = 1;
		vfs_shutdown();		/* sync */

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	uvm_shutdown();
	splhigh();
	if (howto & RB_HALT) {
		doshutdownhooks();
		if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
			;
		}

		printf("halted\n\n");
		(fw->exit)();
	}
	if (!cold && (howto & RB_DUMP))
		dumpsys();
	doshutdownhooks();
	printf("rebooting\n\n");

	{
		volatile int32_t *reset;
		int32_t v;

		reset = mapiodev(0xe0000900, 0x100);
		reset[6] = 0x52535445;
		v = reset[6];
		reset[7] = 0x00000002;
	}

	printf("boot failed, spinning\n");
	while(1) /* forever */;
}

extern void ipic_do_pending_int(void);

void
do_pending_int(void)
{
	struct cpu_info *ci = curcpu();
	int pcpl, s;

	if (ci->ci_iactive)
		return;

	ci->ci_iactive = 1;
	s = ppc_intr_disable();
	pcpl = ci->ci_cpl;

	ipic_do_pending_int();

	do {
		if((ci->ci_ipending & SINT_CLOCK) & ~pcpl) {
			ci->ci_ipending &= ~SINT_CLOCK;
			ci->ci_cpl = SINT_CLOCK|SINT_NET|SINT_TTY;
			ppc_intr_enable(1);
			KERNEL_LOCK();
			softintr_dispatch(SI_SOFTCLOCK);
			KERNEL_UNLOCK();
			ppc_intr_disable();
			continue;
		}
		if((ci->ci_ipending & SINT_NET) & ~pcpl) {
			ci->ci_ipending &= ~SINT_NET;
			ci->ci_cpl = SINT_NET|SINT_TTY;
			ppc_intr_enable(1);
			KERNEL_LOCK();
			softintr_dispatch(SI_SOFTNET);
			KERNEL_UNLOCK();
			ppc_intr_disable();
			continue;
		}
		if((ci->ci_ipending & SINT_TTY) & ~pcpl) {
			ci->ci_ipending &= ~SINT_TTY;
			ci->ci_cpl = SINT_TTY;
			ppc_intr_enable(1);
			KERNEL_LOCK();
			softintr_dispatch(SI_SOFTTTY);
			KERNEL_UNLOCK();
			ppc_intr_disable();
			continue;
		}
	} while ((ci->ci_ipending & SINT_ALLMASK) & ~pcpl);
	ci->ci_cpl = pcpl;	/* Don't use splx... we are here already! */

	ci->ci_iactive = 0;
	ppc_intr_enable(s);
}

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
void
signotify(struct proc *p)
{
	aston(p);
}

/* bcopy(), error on fault */
int
kcopy(const void *from, void *to, size_t size)
{
	faultbuf env;
	void *oldh = curproc->p_addr->u_pcb.pcb_onfault;

	if (setfault(&env)) {
		curproc->p_addr->u_pcb.pcb_onfault = oldh;
		return EFAULT;
	}
	bcopy(from, to, size);
	curproc->p_addr->u_pcb.pcb_onfault = oldh;

	return 0;
}

struct mem_region uboot_mem[2], uboot_avail[4];

void
uboot_mem_regions(struct mem_region **memp, struct mem_region **availp)
{
	uboot_mem[0].start = bootinfo.bi_memstart;
	uboot_mem[0].size = bootinfo.bi_memsize;

	/* Reserve memory used for exception vectors. */
	uboot_avail[0] = uboot_mem[0];
	if (uboot_mem[0].start < EXC_LAST + 0x100) {
		uboot_avail[0].size -= (EXC_LAST + 0x100 - uboot_mem[0].start);
		uboot_avail[0].start = EXC_LAST + 0x100;
	}

	*memp = uboot_mem;
	*availp = uboot_avail;
}

void
uboot_vmon(void)
{
}

void
myetheraddr(u_char *cp)
{
	bcopy(bootinfo.bi_enetaddr, cp, sizeof bootinfo.bi_enetaddr);
	bootinfo.bi_enetaddr[5]++;
}

/* prototype for locore function */
void cpu_switchto_asm(struct proc *oldproc, struct proc *newproc);

void cpu_switchto(struct proc *oldproc, struct proc *newproc)
{
	cpu_switchto_asm(oldproc, newproc);
}
