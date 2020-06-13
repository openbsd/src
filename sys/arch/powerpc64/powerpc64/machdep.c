/*	$OpenBSD: machdep.c,v 1.20 2020/06/13 22:58:42 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/msgbuf.h>
#include <sys/reboot.h>

#include <machine/cpufunc.h>
#include <machine/opal.h>
#include <machine/psl.h>
#include <machine/trap.h>

#include <uvm/uvm_extern.h>

#include <dev/ofw/fdt.h>
#include <dev/cons.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#endif

int cacheline_size = 128;

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

int cold = 1;
int safepri = 0;
int physmem;
paddr_t physmax;

struct vm_map *exec_map;
struct vm_map *phys_map;

char machine[] = MACHINE;

struct user *proc0paddr;

caddr_t esym;

extern char _start[], _end[];
extern char __bss_start[];

extern uint64_t opal_base;
extern uint64_t opal_entry;

extern char trapcode[], trapcodeend[];
extern char hvtrapcode[], hvtrapcodeend[];
extern char generictrap[];
extern char generichvtrap[];

struct fdt_reg memreg[VM_PHYSSEG_MAX];
int nmemreg;

#ifdef DDB
struct fdt_reg initrd_reg;
#endif

void memreg_add(const struct fdt_reg *);
void memreg_remove(const struct fdt_reg *);

paddr_t fdt_pa;
size_t fdt_size;

void
init_powernv(void *fdt, void *tocbase)
{
	struct fdt_reg reg;
	paddr_t trap;
	uint64_t msr;
	char *prop;
	void *node;
	int len;
	int i;

	/* Store pointer to our struct cpu_info. */
	__asm volatile ("mtsprg0 %0" :: "r"(&cpu_info_primary));
	__asm volatile ("mr %%r13, %0" :: "r"(&cpu_info_primary));

	/* Clear BSS. */
	memset(__bss_start, 0, _end - __bss_start);

	if (!fdt_init(fdt) || fdt_get_size(fdt) == 0)
		panic("no FDT");

	/* Get OPAL base and entry addresses from FDT. */
	node = fdt_find_node("/ibm,opal");
	if (node) {
		fdt_node_property(node, "opal-base-address", &prop);
		opal_base = bemtoh64((uint64_t *)prop);
		fdt_node_property(node, "opal-entry-address", &prop);
		opal_entry = bemtoh64((uint64_t *)prop);
		fdt_node_property(node, "compatible", &prop);
	}

	/* At this point we can call OPAL runtime services and use printf(9). */
	printf("Hello, World!\n");

	/* Stash these such that we can remap the FDT later. */
	fdt_pa = (paddr_t)fdt;
	fdt_size = fdt_get_size(fdt);

	/*
	 * Initialize all traps with the stub that calls the generic
	 * trap handler.
	 */
	for (trap = EXC_RST; trap < EXC_LAST; trap += 32)
		memcpy((void *)trap, trapcode, trapcodeend - trapcode);

	/* Hypervisor Virtualization interrupt needs special handling. */
	memcpy((void *)EXC_HVI, hvtrapcode, hvtrapcodeend - hvtrapcode);

	*((void **)TRAP_ENTRY) = generictrap;
	*((void **)TRAP_HVENTRY) = generichvtrap;

	/* Make the stubs visible to the CPU. */
	__syncicache(EXC_RSVD, EXC_LAST - EXC_RSVD);

	/* We're now ready to take traps. */
	msr = mfmsr();
	mtmsr(msr | PSL_RI);

#define LPCR_LPES	0x0000000000000008UL
#define LPCR_HVICE	0x0000000000000002UL

	mtlpcr(LPCR_LPES | LPCR_HVICE);
	isync();

	/* Add all memory. */
	node = fdt_find_node("/");
	for (node = fdt_child_node(node); node; node = fdt_next_node(node)) {
		len = fdt_node_property(node, "device_type", &prop);
		if (len <= 0)
			continue;
		if (strcmp(prop, "memory") != 0)
			continue;
		for (i = 0; nmemreg < nitems(memreg); i++) {
			if (fdt_get_reg(node, i, &reg))
				break;
			if (reg.size == 0)
				continue;
			memreg_add(&reg);
			physmem += atop(reg.size);
			physmax = MAX(physmax, reg.addr + reg.size);
		}
	}

	/* Remove reserved memory. */
	node = fdt_find_node("/reserved-memory");
	if (node) {
		for (node = fdt_child_node(node); node;
		     node = fdt_next_node(node)) {
			if (fdt_get_reg(node, 0, &reg))
				continue;
			if (reg.size == 0)
				continue;
			memreg_remove(&reg);
		}
	}

	/* Remove interrupt vectors. */
	reg.addr = trunc_page(EXC_RSVD);
	reg.size = round_page(EXC_LAST);
	memreg_remove(&reg);

	/* Remove kernel. */
	reg.addr = trunc_page((paddr_t)_start);
	reg.size = round_page((paddr_t)_end) - reg.addr;
	memreg_remove(&reg);

	/* Remove FDT. */
	reg.addr = trunc_page((paddr_t)fdt);
	reg.size = round_page((paddr_t)fdt + fdt_get_size(fdt)) - reg.addr;
	memreg_remove(&reg);

#ifdef DDB
	/* Load symbols from initrd. */
	db_machine_init();
	if (initrd_reg.size != 0)
		memreg_remove(&initrd_reg);
#endif

	pmap_bootstrap();
	uvm_setpagesize();

	for (i = 0; i < nmemreg; i++) {
		paddr_t start = memreg[i].addr;
		paddr_t end = start + memreg[i].size;

		uvm_page_physload(atop(start), atop(end),
		    atop(start), atop(end), 0);
	}

	/* Enable translation. */
	msr = mfmsr();
	mtmsr(msr | (PSL_DR|PSL_IR));
	isync();

	initmsgbuf((caddr_t)uvm_pageboot_alloc(MSGBUFSIZE), MSGBUFSIZE);
}

void
memreg_add(const struct fdt_reg *reg)
{
	memreg[nmemreg++] = *reg;
}

void
memreg_remove(const struct fdt_reg *reg)
{
	uint64_t start = reg->addr;
	uint64_t end = reg->addr + reg->size;
	int i, j;

	for (i = 0; i < nmemreg; i++) {
		uint64_t memstart = memreg[i].addr;
		uint64_t memend = memreg[i].addr + memreg[i].size;

		if (end <= memstart)
			continue;
		if (start >= memend)
			continue;

		if (start <= memstart)
			memstart = MIN(end, memend);
		if (end >= memend)
			memend = MAX(start, memstart);

		if (start > memstart && end < memend) {
			if (nmemreg < nitems(memreg)) {
				memreg[nmemreg].addr = end;
				memreg[nmemreg].size = memend - end;
				nmemreg++;
			}
			memend = start;
		}
		memreg[i].addr = memstart;
		memreg[i].size = memend - memstart;
	}

	/* Remove empty slots. */
	for (i = nmemreg - 1; i >= 0; i--) {
		if (memreg[i].size == 0) {
			for (j = i; (j + 1) < nmemreg; j++)
				memreg[j] = memreg[j + 1];
			nmemreg--;
		}
	}
}

#define R_PPC64_RELATIVE	22
#define ELF_R_TYPE_RELATIVE	R_PPC64_RELATIVE

/*
 * Disable optimization for this function to prevent clang from
 * generating jump tables that need relocation.
 */
__attribute__((optnone)) void
self_reloc(Elf_Dyn *dynamic, Elf_Addr base)
{
	Elf_Word relasz = 0, relaent = sizeof(Elf_RelA);
	Elf_RelA *rela = NULL;
	Elf_Addr *addr;
	Elf_Dyn *dynp;

	for (dynp = dynamic; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_RELA:
			rela = (Elf_RelA *)(dynp->d_un.d_ptr + base);
			break;
		case DT_RELASZ:
			relasz = dynp->d_un.d_val;
			break;
		case DT_RELAENT:
			relaent = dynp->d_un.d_val;
			break;
		}
	}

	while (relasz > 0) {
		switch (ELF_R_TYPE(rela->r_info)) {
		case ELF_R_TYPE_RELATIVE:
			addr = (Elf_Addr *)(base + rela->r_offset);
			*addr = base + rela->r_addend;
			break;
		}
		rela = (Elf_RelA *)((caddr_t)rela + relaent);
		relasz -= relaent;
	}
}

void
opal_printf(const char *fmt, ...)
{
	static char buf[256];
	uint64_t len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (len == (uint64_t)-1)
		len = 0;
	else if (len >= sizeof(buf))
		 len = sizeof(buf) - 1;
	va_end(ap);

	opal_console_write(0, &len, buf);
}

void
opal_cnprobe(struct consdev *cd)
{
}

void
opal_cninit(struct consdev *cd)
{
}

int
opal_cngetc(dev_t dev)
{
	uint64_t len;
	char ch;

	for (;;) {
		len = 1;
		opal_console_read(0, &len, &ch);
		if (len)
			return ch;
		opal_poll_events(NULL);
	}
}

void
opal_cnputc(dev_t dev, int c)
{
	uint64_t len = 1;
	char ch = c;

	opal_console_write(0, &len, &ch);
}

void
opal_cnpollc(dev_t dev, int on)
{
}

struct consdev opal_consdev = {
	.cn_probe = opal_cnprobe,
	.cn_init = opal_cninit,
	.cn_getc = opal_cngetc,
	.cn_putc = opal_cnputc,
	.cn_pollc = opal_cnpollc,
};

struct consdev *cn_tab = &opal_consdev;

int
copyin(const void *src, void *dst, size_t size)
{
	printf("%s\n", __func__);
	return EFAULT;
}

int
copyout(const void *src, void *dst, size_t size)
{
	printf("%s\n", __func__);
	return EFAULT;
}

int
copystr(const void *src, void *dst, size_t len, size_t *lenp)
{
	printf("%s\n", __func__);
	return EFAULT;
}

int
copyinstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	printf("%s\n", __func__);
	return EFAULT;
}

int
copyoutstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	printf("%s\n", __func__);
	return EFAULT;
}

int
kcopy(const void *src, void *dst, size_t size)
{
	printf("%s\n", __func__);
	return EFAULT;
}

void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;
}

void
cpu_startup(void)
{
	paddr_t pa, epa;
	vaddr_t va;
	void *fdt;
	
	printf("%s", version);

	bufinit();

	/* Remap the FDT. */
	pa = trunc_page(fdt_pa);
	epa = round_page(fdt_pa + fdt_size);
	va = (vaddr_t)km_alloc(epa - pa, &kv_any, &kp_none, &kd_waitok);
	fdt = (void *)(va + (fdt_pa & PAGE_MASK));
	while (pa < epa) {
		pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}

	if (!fdt_init(fdt) || fdt_get_size(fdt) == 0)
		panic("can't remap FDT");
}

void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	printf("%s\n", __func__);
}

void
sendsig(sig_t catcher, int sig, sigset_t mask, const siginfo_t *ksip)
{
	printf("%s\n", __func__);
}

int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	printf("%s\n", __func__);
	return EJUSTRETURN;
}

/*
 * machine dependent system variables.
 */

int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;		/* overloaded */

	switch (name[0]) {
	default:
		return EOPNOTSUPP;
	}
	/* NOTREACHED */
}

void
consinit(void)
{
}

void
opal_powerdown(void)
{
	int64_t error;

	do {
		error = opal_cec_power_down(0);
		if (error == OPAL_BUSY_EVENT)
			opal_poll_events(NULL);
	} while (error == OPAL_BUSY || error == OPAL_BUSY_EVENT);

	if (error != OPAL_SUCCESS)
		return;

	/* Wait for the actual powerdown to happen. */
	for (;;)
		opal_poll_events(NULL);
}

__dead void
boot(int howto)
{
	if ((howto & RB_HALT) != 0) {
		if ((howto & RB_POWERDOWN) != 0)
			opal_powerdown();
	}

	printf("rebooting...\n");
	opal_cec_reboot();

	for (;;)
		continue;
	/* NOTREACHED */
}

unsigned int
cpu_rnd_messybits(void)
{
	struct timespec ts;

	nanotime(&ts);
	return (ts.tv_nsec ^ (ts.tv_sec << 20));
}
