/*	$OpenBSD: ofw_machdep.c,v 1.11 2004/06/15 05:44:45 brad Exp $	*/
/*	$NetBSD: ofw_machdep.c,v 1.16 2001/07/20 00:07:14 eeh Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <machine/openfirm.h>

#include <dev/ofw/ofw_pci.h>

#if defined(FFS) && defined(CD9660)
#include <ufs/ffs/fs.h>
#endif

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <sys/stdarg.h>

#include <machine/sparc64.h>

int vsprintf(char *, const char *, va_list);

void dk_cleanup(void);

static u_int mmuh = -1, memh = -1;

static u_int get_mmu_handle(void);
static u_int get_memory_handle(void);

static u_int 
get_mmu_handle()
{
	u_int chosen;

	if ((chosen = OF_finddevice("/chosen")) == -1) {
		prom_printf("get_mmu_handle: cannot get /chosen\r\n");
		return -1;
	}
	if (OF_getprop(chosen, "mmu", &mmuh, sizeof(mmuh)) == -1) {
		prom_printf("get_mmu_handle: cannot get mmuh\r\n");
		return -1;
	}
	return mmuh;
}

static u_int 
get_memory_handle()
{
	u_int chosen;

	if ((chosen = OF_finddevice("/chosen")) == -1) {
		prom_printf("get_memory_handle: cannot get /chosen\r\n");
		return -1;
	}
	if (OF_getprop(chosen, "memory", &memh, sizeof(memh)) == -1) {
		prom_printf("get_memory_handle: cannot get memh\r\n");
		return -1;
	}
	return memh;
}


/* 
 * Point prom to our trap table.  This stops the prom from mapping us.
 */
int
prom_set_trap_table(tba)
	vaddr_t tba;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t tba;
	} args;

	args.name = ADR2CELL(&"SUNW,set-trap-table");
	args.nargs = 1;
	args.nreturns = 0;
	args.tba = ADR2CELL(tba);
	return openfirmware(&args);
}

/* 
 * Have the prom convert from virtual to physical addresses.
 *
 * Only works while the prom is actively mapping us.
 */
paddr_t
prom_vtop(vaddr)
	vaddr_t vaddr;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t vaddr;
		cell_t status;
		cell_t retaddr;
		cell_t mode;
		cell_t phys_hi;
		cell_t phys_lo;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_vtop: cannot get mmuh\r\n");
		return 0;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 3;
	args.nreturns = 5;
	args.method = ADR2CELL(&"translate");
	args.ihandle = HDL2CELL(mmuh);
	args.vaddr = ADR2CELL(vaddr);
	if(openfirmware(&args) == -1)
		return -1;
#if 0
	prom_printf("Called \"translate\", mmuh=%x, vaddr=%x, status=%x %x,\r\n retaddr=%x %x, mode=%x %x, phys_hi=%x %x, phys_lo=%x %x\r\n",
		    mmuh, vaddr, (int)(args.status>>32), (int)args.status, (int)(args.retaddr>>32), (int)args.retaddr, 
		    (int)(args.mode>>32), (int)args.mode, (int)(args.phys_hi>>32), (int)args.phys_hi,
		    (int)(args.phys_lo>>32), (int)args.phys_lo);
#endif
	return (paddr_t)((((paddr_t)args.phys_hi)<<32)|(u_int32_t)args.phys_lo); 
}

/* 
 * Grab some address space from the prom
 *
 * Only works while the prom is actively mapping us.
 */
vaddr_t
prom_claim_virt(vaddr, len)
	vaddr_t vaddr;
	int len;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t align;
		cell_t len;
		cell_t vaddr;
		cell_t status;
		cell_t retaddr;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_claim_virt: cannot get mmuh\r\n");
		return 0;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 5;
	args.nreturns = 2;
	args.method = ADR2CELL(&"claim");
	args.ihandle = HDL2CELL(mmuh);
	args.align = 0;
	args.len = len;
	args.vaddr = ADR2CELL(vaddr);
	if (openfirmware(&args) == -1)
		return -1;
	return (paddr_t)args.retaddr;
}

/* 
 * Request some address space from the prom
 *
 * Only works while the prom is actively mapping us.
 */
vaddr_t
prom_alloc_virt(len, align)
	int len;
	int align;
{
	static int retaddr;
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t align;
		cell_t len;
		cell_t status;
		cell_t retaddr;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_alloc_virt: cannot get mmuh\r\n");
		return -1LL;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 4;
	args.nreturns = 2;
	args.method = ADR2CELL(&"claim");
	args.ihandle = HDL2CELL(mmuh);
	args.align = align;
	args.len = len;
	args.retaddr = ADR2CELL(&retaddr);
	if (openfirmware(&args) != 0)
		return -1;
	return retaddr; /* Kluge till we go 64-bit */
}

/* 
 * Release some address space to the prom
 *
 * Only works while the prom is actively mapping us.
 */
int
prom_free_virt(vaddr, len)
	vaddr_t vaddr;
	int len;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t len;
		cell_t vaddr;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_free_virt: cannot get mmuh\r\n");
		return -1;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 4;
	args.nreturns = 0;
	args.method = ADR2CELL(&"release");
	args.ihandle = HDL2CELL(mmuh);
	args.vaddr = ADR2CELL(vaddr);
	args.len = len;
	return openfirmware(&args);
}


/* 
 * Unmap some address space
 *
 * Only works while the prom is actively mapping us.
 */
int
prom_unmap_virt(vaddr, len)
	vaddr_t vaddr;
	int len;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t len;
		cell_t vaddr;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_unmap_virt: cannot get mmuh\r\n");
		return -1;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 4;
	args.nreturns = 0;
	args.method = ADR2CELL(&"unmap");
	args.ihandle = HDL2CELL(mmuh);
	args.vaddr = ADR2CELL(vaddr);
	args.len = len;
	return openfirmware(&args);
}

/* 
 * Have prom map in some memory
 *
 * Only works while the prom is actively mapping us.
 */
int
prom_map_phys(paddr, size, vaddr, mode)
	paddr_t paddr;
	off_t size;
	vaddr_t vaddr;
	int mode;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t mode;
		cell_t size;
		cell_t vaddr;
		cell_t phys_hi;
		cell_t phys_lo;
		cell_t status;
		cell_t retaddr;
	} args;

	if (mmuh == -1 && ((mmuh = get_mmu_handle()) == -1)) {
		prom_printf("prom_map_phys: cannot get mmuh\r\n");
		return 0;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 7;
	args.nreturns = 1;
	args.method = ADR2CELL(&"map");
	args.ihandle = HDL2CELL(mmuh);
	args.mode = mode;
	args.size = size;
	args.vaddr = ADR2CELL(vaddr);
	args.phys_hi = HDL2CELL(paddr>>32); 
	args.phys_lo = HDL2CELL(paddr);

	if (openfirmware(&args) == -1)
		return -1;
	if (args.status)
		return -1;
	return args.retaddr;
}


/* 
 * Request some RAM from the prom
 *
 * Only works while the prom is actively mapping us.
 */
paddr_t
prom_alloc_phys(len, align)
	int len;
	int align;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t align;
		cell_t len;
		cell_t status;
		cell_t phys_hi;
		cell_t phys_lo;
	} args;

	if (memh == -1 && ((memh = get_memory_handle()) == -1)) {
		prom_printf("prom_alloc_phys: cannot get memh\r\n");
		return -1;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 4;
	args.nreturns = 3;
	args.method = ADR2CELL(&"claim");
	args.ihandle = HDL2CELL(memh);
	args.align = align;
	args.len = len;
	if (openfirmware(&args) != 0)
		return -1;
	return (paddr_t)((((paddr_t)args.phys_hi)<<32)|(u_int32_t)args.phys_lo);
}

/* 
 * Request some specific RAM from the prom
 *
 * Only works while the prom is actively mapping us.
 */
paddr_t
prom_claim_phys(phys, len)
	paddr_t phys;
	int len;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t align;
		cell_t len;
		cell_t phys_hi;
		cell_t phys_lo;
		cell_t status;
		cell_t rphys_hi;
		cell_t rphys_lo;
	} args;

	if (memh == -1 && ((memh = get_memory_handle()) == -1)) {
		prom_printf("prom_claim_phys: cannot get memh\r\n");
		return -1;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 6;
	args.nreturns = 3;
	args.method = ADR2CELL(&"claim");
	args.ihandle = HDL2CELL(memh);
	args.align = 0;
	args.len = len;
	args.phys_hi = HDL2CELL(phys>>32);
	args.phys_lo = HDL2CELL(phys);
	if (openfirmware(&args) != 0)
		return -1;
	return (paddr_t)((((paddr_t)args.rphys_hi)<<32)|(u_int32_t)args.rphys_lo);
}

/* 
 * Free some RAM to prom
 *
 * Only works while the prom is actively mapping us.
 */
int
prom_free_phys(phys, len)
	paddr_t phys;
	int len;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t len;
		cell_t phys_hi;
		cell_t phys_lo;
	} args;

	if (memh == -1 && ((memh = get_memory_handle()) == -1)) {
		prom_printf("prom_free_phys: cannot get memh\r\n");
		return -1;
	}
	args.name = ADR2CELL(&"call-method");
	args.nargs = 5;
	args.nreturns = 0;
	args.method = ADR2CELL(&"release");
	args.ihandle = HDL2CELL(memh);
	args.len = len;
	args.phys_hi = HDL2CELL(phys>>32);
	args.phys_lo = HDL2CELL(phys);
	return openfirmware(&args);
}

/* 
 * Get the msgbuf from the prom.  Only works once.
 *
 * Only works while the prom is actively mapping us.
 */
paddr_t
prom_get_msgbuf(len, align)
	int len;
	int align;
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t ihandle;
		cell_t align;
		cell_t len;
		cell_t id;
		cell_t status;
		cell_t phys_hi;
		cell_t phys_lo;
	} args;
	paddr_t addr;
	int rooth;
	int is_e250 = 1;

	/* E250s tend to have buggy PROMs that break on test-method */
	if ((rooth = OF_finddevice("/")) != -1) {
		char name[80];

		if ((OF_getprop(rooth, "name", &name, sizeof(name))) != -1) {
			if (strcmp(name, "SUNW,Ultra-250") && strcmp(name, "SUNW,Ultra-4")) 
				is_e250 = 0;
		} else prom_printf("prom_get_msgbuf: cannot get \"name\"\r\n");
	} else prom_printf("prom_get_msgbuf: cannot open root device \r\n");

	if (memh == -1 && ((memh = get_memory_handle()) == -1)) {
		prom_printf("prom_get_msgbuf: cannot get memh\r\n");
		return -1;
	}
	if (is_e250) {
		prom_printf("prom_get_msgbuf: Cannot recover msgbuf on E250/450\r\n");
	} else if (OF_test("test-method") == 0) {
		if (OF_test_method(memh, "SUNW,retain") != 0) {
			args.name = ADR2CELL(&"call-method");
			args.nargs = 5;
			args.nreturns = 3;
			args.method = ADR2CELL(&"SUNW,retain");
			args.id = ADR2CELL(&"msgbuf");
			args.ihandle = HDL2CELL(memh);
			args.len = len;
			args.align = align;
			args.status = -1;
			if (openfirmware(&args) == 0 && args.status == 0) {
				return (((paddr_t)args.phys_hi<<32)|
					(u_int32_t)args.phys_lo);
			} else prom_printf("prom_get_msgbuf: SUNW,retain failed\r\n");
		} else prom_printf("prom_get_msgbuf: test-method failed\r\n");
	} else prom_printf("prom_get_msgbuf: test failed\r\n");
	/* Allocate random memory -- page zero avail?*/
	addr = prom_claim_phys(0x000, len);
	prom_printf("prom_get_msgbuf: allocated new buf at %08x\r\n", (int)addr); 
	if (addr == -1) {
		prom_printf("prom_get_msgbuf: cannot get allocate physmem\r\n");
		return -1;
	}
	prom_printf("prom_get_msgbuf: claiming new buf at %08x\r\n", (int)addr);
	{ int i; for (i=0; i<200000000; i++); }
	return addr; /* Kluge till we go 64-bit */
}

/* 
 * Low-level prom I/O routines.
 */

static u_int stdin = 0;
static u_int stdout = 0;

int 
OF_stdin() 
{
	u_int chosen;

	if (stdin != 0) 
		return stdin;
		
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdin", &stdin, sizeof(stdin));
	return stdin;
}

int
OF_stdout()
{
	u_int chosen;

	if (stdout != 0) 
		return stdout;
		
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	return stdout;
}


/*
 * print debug info to prom. 
 * This is not safe, but then what do you expect?
 */
void
prom_printf(const char *fmt, ...)
{
	int len;
	static char buf[256];
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	OF_write(OF_stdout(), buf, len);
}

#ifdef DEBUG
int ofmapintrdebug = 0;
#define	DPRINTF(x)	if (ofmapintrdebug) printf x
#else
#define DPRINTF(x)
#endif


/*
 * Recursively hunt for a property.
 */
int
OF_searchprop(int node, char *prop, void *buf, int buflen)
{
	int len;

	for( ; node; node = OF_parent(node)) {
		len = OF_getprop(node, prop, buf, buflen);
		if (len >= 0)
			return (len);
	}
	/* Error -- not found */
	return (-1);
}


/*
 * Compare a sequence of cells with a mask,
 *  return 1 if they match and 0 if they don't.
 */
static int compare_cells (int *cell1, int *cell2, int *mask, int ncells);
static int
compare_cells(int *cell1, int *cell2, int *mask, int ncells) 
{
	int i;

	for (i=0; i<ncells; i++) {
		DPRINTF(("src %x ^ dest %x -> %x & mask %x -> %x\n",
			cell1[i], cell2[i], (cell1[i] ^ cell2[i]),
			mask[i], ((cell1[i] ^ cell2[i]) & mask[i])));
		if (((cell1[i] ^ cell2[i]) & mask[i]) != 0)
			return (0);
	}
	return (1);
}

/*
 * Find top pci bus host controller for a node.
 */
static int
find_pci_host_node(int node)
{
	char dev_type[16];
	int pch = 0;
	int len;

	for (; node; node = OF_parent(node)) {
		len = OF_getprop(node, "device_type",
				 &dev_type, sizeof(dev_type));
		if (len <= 0)
			continue;
		if (!strcmp(dev_type, "pci"))
			pch = node;
	}
	return pch;
}

/*
 * Follow the OFW algorithm and return an interrupt specifier.
 *
 * Pass in the interrupt specifier you want mapped and the node
 * you want it mapped from.  validlen is the number of cells in
 * the interrupt specifier, and buflen is the number of cells in
 * the buffer.
 */
int
OF_mapintr(int node, int *interrupt, int validlen, int buflen)
{
	int i, len;
	int address_cells, size_cells, interrupt_cells, interrupt_map_len;
	int interrupt_map[100];
	int interrupt_map_mask[10];
	int reg[10];
	char dev_type[32];
	int phc_node;
	int rc = -1;

	/* Don't need to map OBP interrupt, it's already */
	if (*interrupt & 0x20)
		return validlen;

	/*
	 * If there is no interrupt map in the bus node, we 
	 * need to convert the slot address to its parent
	 * bus format, and hunt up the parent bus to see if
	 * we need to remap.
	 *
	 * The specification for interrupt mapping is borken.
	 * You are supposed to query the interrupt parent in
	 * the interrupt-map specification to determine the
	 * number of address and interrupt cells, but we need
	 * to know how many address and interrupt cells to skip
	 * to find the phandle...
	 *
	 */
	if ((len = OF_getprop(node, "reg", &reg, sizeof(reg))) <= 0) {
		printf("OF_mapintr: no reg property?\n");
		return (-1);
	}

	phc_node = find_pci_host_node(node);

	for (; node; node = OF_parent(node)) {
#ifdef DEBUG
		char name[40];

		if (ofmapintrdebug) {
			OF_getprop(node, "name", &name, sizeof(name));
			printf("Node %s (%x), host %x\n", name,
			       node, phc_node);
		}
#endif

		if ((interrupt_map_len = OF_getprop(node,
			"interrupt-map", &interrupt_map,
			sizeof(interrupt_map))) <= 0) {

			/* Swizzle interrupt if this is a PCI bridge. */
			if (((len = OF_getprop(node, "device_type", &dev_type,
					      sizeof(dev_type))) > 0) &&
			    !strcmp(dev_type, "pci") &&
			    (node != phc_node)) {
				*interrupt = ((*interrupt +
				    OFW_PCI_PHYS_HI_DEVICE(reg[0]) - 1) & 3) + 1;
				DPRINTF(("OF_mapintr: interrupt %x, reg[0] %x\n",
					 *interrupt, reg[0]));
			}

			/* Get reg for next level compare. */
			reg[0] = 0;
			OF_getprop(node, "reg", &reg, sizeof(reg));
			continue;
		}
		/* Convert from bytes to cells. */
		interrupt_map_len = interrupt_map_len/sizeof(int);
		if ((len = (OF_searchprop(node, "#address-cells", &address_cells,
			sizeof(address_cells)))) <= 0) {
			/* How should I know. */
			address_cells = 2;
		}
		DPRINTF(("#address-cells = %d len %d", address_cells, len));
		if ((len = OF_searchprop(node, "#size-cells", &size_cells,
			sizeof(size_cells))) <= 0) {
			/* How should I know. */
			size_cells = 2;
		}
		DPRINTF(("#size-cells = %d len %d", size_cells, len));
		if ((len = OF_getprop(node, "#interrupt-cells", &interrupt_cells,
			sizeof(interrupt_cells))) <= 0) {
			/* How should I know. */
			interrupt_cells = 1;
		}
		DPRINTF(("#interrupt-cells = %d, len %d\n", interrupt_cells,
			len));
		if ((len = OF_getprop(node, "interrupt-map-mask", &interrupt_map_mask,
			sizeof(interrupt_map_mask))) <= 0) {
			/* Create a mask that masks nothing. */
			for (i = 0; i<(address_cells + interrupt_cells); i++)
				interrupt_map_mask[i] = -1;
		}
#ifdef DEBUG
		DPRINTF(("interrupt-map-mask len %d = ", len));
		for (i=0; i<(address_cells + interrupt_cells); i++)
			DPRINTF(("%x.", interrupt_map_mask[i]));
		DPRINTF(("reg = "));
		for (i=0; i<(address_cells); i++)
			DPRINTF(("%x.", reg[i]));
		DPRINTF(("interrupts = "));
		for (i=0; i<(interrupt_cells); i++)
			DPRINTF(("%x.", interrupt[i]));

#endif

		/* finally we can attempt the compare */
		i=0;
		while ( i < interrupt_map_len ) {
			int pintr_cells;
			int *imap = &interrupt_map[i];
			int *parent = &imap[address_cells + interrupt_cells];

#ifdef DEBUG
			DPRINTF(("\ninterrupt-map addr "));
			for (len=0; len<address_cells; len++)
				DPRINTF(("%x.", imap[len]));
			DPRINTF((" intr "));
			for (; len<(address_cells+interrupt_cells); len++)
				DPRINTF(("%x.", imap[len]));
			DPRINTF(("\nnode %x vs parent %x\n",
				imap[len], *parent));
#endif

			/* Find out how many cells we'll need to skip. */
			if ((len = OF_searchprop(*parent, "#interrupt-cells",
				&pintr_cells, sizeof(pintr_cells))) < 0) {
				pintr_cells = interrupt_cells;
			}
			DPRINTF(("pintr_cells = %d len %d\n", pintr_cells, len));

			if (compare_cells(imap, reg, 
				interrupt_map_mask, address_cells) &&
				compare_cells(&imap[address_cells], 
					interrupt,
					&interrupt_map_mask[address_cells], 
					interrupt_cells))
			{
				/* Bingo! */
				if (buflen < pintr_cells) {
					/* Error -- ran out of storage. */
					return (-1);
				}
				parent ++;
#ifdef DEBUG
				DPRINTF(("Match! using "));
				for (len=0; len<pintr_cells; len++)
					DPRINTF(("%x.", parent[len]));
#endif
				for (i=0; i<pintr_cells; i++)
					interrupt[i] = parent[i];
				rc = validlen = pintr_cells;
				break;
			}
			/* Move on to the next interrupt_map entry. */
#ifdef DEBUG
			DPRINTF(("skip %d cells:",
				address_cells + interrupt_cells +
				pintr_cells + 1));
			for (len=0; len<(address_cells +
				interrupt_cells + pintr_cells + 1); len++)
				DPRINTF(("%x.", imap[len]));
#endif
			i += address_cells + interrupt_cells + pintr_cells + 1;
		}

		/* Get reg for the next level search. */
		if ((len = OF_getprop(node, "reg", &reg, sizeof(reg))) <= 0) {
			DPRINTF(("OF_mapintr: no reg property?\n"));
			continue;
		}
		DPRINTF(("reg len %d\n", len));

	} 
	return (rc);
}
