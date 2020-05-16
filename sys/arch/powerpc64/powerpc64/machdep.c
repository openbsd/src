/*	$OpenBSD: machdep.c,v 1.1 2020/05/16 17:11:14 kettenis Exp $	*/

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
#include <sys/exec.h>
#include <sys/exec_elf.h>

#include <uvm/uvm_extern.h>

#include <dev/ofw/fdt.h>
#include <dev/cons.h>

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

int cold = 1;
int safepri = 0;
int physmem;

struct vm_map *exec_map;
struct vm_map *phys_map;

char machine[] = MACHINE;

struct user *proc0paddr;

caddr_t esym;

extern void opal_console_write(int64_t, int64_t *, const uint8_t *);
extern void opal_cec_reboot(void);

void opal_printf(const char *fmt, ...);

extern char __bss_start[];
extern char end[];

extern uint64_t opal_base;
extern uint64_t opal_entry;

void
init_powernv(void *fdt)
{
	char *prop;
	void *node;

	/* Clear BSS. */
	memset(__bss_start, 0, end - __bss_start);

	if (!fdt_init(fdt) || fdt_get_size(fdt) == 0)
		panic("%s: no FDT\r\n", __func__);

	node = fdt_find_node("/ibm,opal");
	if (node) {
		fdt_node_property(node, "opal-base-address", &prop);
		opal_base = bemtoh64((uint64_t *)prop);
		fdt_node_property(node, "opal-entry-address", &prop);
		opal_entry = bemtoh64((uint64_t *)prop);
		fdt_node_property(node, "compatible", &prop);
		printf("%s\n", prop);
	}

	node = fdt_find_node("/");
	fdt_node_property(node, "compatible", &prop);
	printf("%s\n", prop);

	fdt_node_property(node, "model", &prop);
	printf("%s\n", prop);

	uint32_t pvr;
	__asm volatile("mfspr %0,287" : "=r"(pvr));
	printf("PVR %x\n", pvr);

	uint32_t hid;
	__asm volatile("mfspr %0,1008" : "=r"(hid));
	printf("HID %x\n", hid);

	uint64_t lpcr;
	__asm volatile("mfspr %0,318" : "=r"(lpcr));
	printf("LPCR %llx\n", lpcr);

	uint64_t lpidr;
	__asm volatile("mfspr %0,319" : "=r"(lpidr));
	printf("LPIDR %llx\n", lpidr);

	uint64_t msr;
	__asm volatile("mfmsr %0" : "=r"(msr));
	printf("MSR %llx\n", msr);

	uint64_t toc;
	__asm volatile("mr %0, %%r2" : "=r"(toc));
	printf("TOC %llx\n", toc);

	printf("Hello, World!\n");
	opal_cec_reboot();
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
	return -1;
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
	return EFAULT;
}

int
copyout(const void *src, void *dst, size_t size)
{
	return EFAULT;
}

int
copystr(const void *src, void *dst, size_t len, size_t *lenp)
{
	return EFAULT;
}

int
copyinstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	return EFAULT;
}

int
copyoutstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	return EFAULT;
}

int
kcopy(const void *src, void *dst, size_t size)
{
	return EFAULT;
}

void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;
}

void
delay(u_int us)
{
}

void
cpu_startup(void)
{
}

void
cpu_initclocks(void)
{
}

void
setstatclockrate(int new)
{
}

void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
}

void
sendsig(sig_t catcher, int sig, sigset_t mask, const siginfo_t *ksip)
{
}

int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
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

__dead void
boot(int howto)
{
	for (;;)
		continue;
	/* NOTREACHED */
}
