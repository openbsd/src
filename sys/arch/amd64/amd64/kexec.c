/*	$OpenBSD: kexec.c,v 1.1 2025/11/12 11:34:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2019-2020 Visa Hankala
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2025 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/exec_elf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include <stand/boot/bootarg.h>

#include <uvm/uvm_extern.h>

#include <machine/kexec.h>
#include <machine/pte.h>
#include <machine/pmap.h>

int	 kexec_kexec(struct kexec_args *, struct proc *);
int	 kexec_read(struct kexec_args *, void *, size_t, off_t);
void	*kexec_prepare(vaddr_t, size_t, int, vaddr_t *, vaddr_t *);
void	(*kexec)(vaddr_t, vaddr_t, paddr_t, size_t, vaddr_t);

void
kexecattach(int num)
{
}

int
kexecopen(dev_t dev, int flags, int mode, struct proc *p)
{
	return (0);
}

int
kexecclose(dev_t dev, int flags, int mode, struct proc *p)
{
	return (0);
}

int
kexecioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int error;

	error = suser(p);
	if (error != 0)
		return error;

	switch (cmd) {
	case KIOC_KEXEC:
		error = kexec_kexec((struct kexec_args *)data, p);
		break;

	case KIOC_GETBOOTDUID:
		memcpy(data, bootduid, sizeof(bootduid));
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

int
kexec_kexec(struct kexec_args *kargs, struct proc *p)
{
	struct kmem_pa_mode kp_kexec = {
		.kp_constraint = &no_constraint,
		.kp_maxseg = 1,
		.kp_zero = 1
	};
	Elf_Ehdr eh, *elfp;
	Elf_Phdr *ph = NULL;
	Elf_Shdr *sh = NULL, *shp;
	Elf64_Off off;
	vaddr_t start = -1;
	vaddr_t end = 0;
	vaddr_t symend, rwva, spva;
	paddr_t shpp, maxp;
	vsize_t align = 0;
	caddr_t addr = NULL;
	size_t phsize, shsize, strsize, size;
	char *shstr, *shstrp = NULL;
	int error, random, i;

	/*
	 * Read the headers and validate them.
	 */
	error = kexec_read(kargs, &eh, sizeof(eh), 0);
	if (error != 0)
		goto fail;

	/* Load program headers. */
	ph = mallocarray(eh.e_phnum, sizeof(Elf_Phdr), M_TEMP, M_NOWAIT);
	if (ph == NULL) {
		error = ENOMEM;
		goto fail;
	}
	phsize = eh.e_phnum * sizeof(Elf_Phdr);
	error = kexec_read(kargs, ph, phsize, eh.e_phoff);
	if (error != 0)
		goto fail;

	/* Load section headers. */
	sh = mallocarray(eh.e_shnum, sizeof(Elf_Shdr), M_TEMP, M_NOWAIT);
	if (sh == NULL) {
		error = ENOMEM;
		goto fail;
	}
	shsize = eh.e_shnum * sizeof(Elf_Shdr);
	error = kexec_read(kargs, sh, shsize, eh.e_shoff);
	if (error != 0)
		goto fail;
	shp = sh;

	/* Load string table */
	strsize = sh[eh.e_shstrndx].sh_size;
	shstrp = malloc(strsize, M_TEMP, M_NOWAIT);
	if (shstrp == NULL) {
		error = ENOMEM;
		goto fail;
	}
	error = kexec_read(kargs, shstrp, strsize,
	    sh[eh.e_shstrndx].sh_offset);
	if (error != 0)
		goto fail;
	shstr = shstrp;

	/* Calculate kernel size */
	for (i = 0; i < eh.e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;
		start = MIN(start, ph[i].p_vaddr);
		align = MAX(align, ph[i].p_align);
		end = MAX(end, ph[i].p_vaddr + ph[i].p_memsz);
	}
	symend = roundup(end, sizeof(Elf64_Addr));

	/* Reserve space for ELF and section header */
	symend += roundup(sizeof(Elf64_Ehdr), sizeof(Elf64_Addr));
	symend += roundup(shsize, sizeof(Elf64_Addr));

	/* Add space for symbols */
	for (i = 0; i < eh.e_shnum; i++) {
		if (shp[i].sh_type == SHT_SYMTAB ||
		    shp[i].sh_type == SHT_STRTAB ||
		    strcmp(shstr + shp[i].sh_name, ".debug_line") == 0 ||
		    strcmp(shstr + shp[i].sh_name, ELF_CTF) == 0) {
			symend += roundup(shp[i].sh_size, sizeof(Elf64_Addr));
		}
	}
	size = round_page(symend) - start;

	/*
	 * Allocate physical memory and load the segments.
	 */

	kp_kexec.kp_align = align;
	addr = km_alloc(size, &kv_any, &kp_kexec, &kd_nowait);
	if (addr == NULL) {
		error = ENOMEM;
		goto fail;
	}

	/* Load code, data, BSS */
	for (i = 0; i < eh.e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;

		error = kexec_read(kargs, addr + (ph[i].p_vaddr - start),
		    ph[i].p_filesz, ph[i].p_offset);
		if (error != 0)
			goto fail;

		/* Clear any BSS. */
		if (ph[i].p_memsz > ph[i].p_filesz) {
			memset(addr + (ph[i].p_vaddr + ph[i].p_filesz) - start,
			    0, ph[i].p_memsz - ph[i].p_filesz);
		}
	}

	/* Fill random section */
	random = 0;
	for (i = 0; i < eh.e_phnum; i++) {
		if (ph[i].p_type != PT_OPENBSD_RANDOMIZE)
			continue;

		/* Assume that the segment is inside a LOAD segment. */
		arc4random_buf(addr + ph[i].p_vaddr - start, ph[i].p_filesz);
		random = 1;
	}

	if (random == 0)
		kargs->boothowto &= ~RB_GOODRANDOM;

	/* Reserve ELF and section header */
	elfp = (Elf_Ehdr *)roundup((vaddr_t)addr + (end - start),
	    sizeof(Elf64_Addr));
	maxp = (paddr_t)elfp + sizeof(Elf64_Ehdr);
	shpp = maxp;
	maxp += roundup(shsize, sizeof(Elf64_Addr));

	/* Load symbols */
	off = maxp - (paddr_t)elfp;
	for (i = 0; i < eh.e_shnum; i++) {
		if (shp[i].sh_type == SHT_SYMTAB ||
		    shp[i].sh_type == SHT_STRTAB ||
		    strcmp(shstr + shp[i].sh_name, ".debug_line") == 0 ||
		    strcmp(shstr + shp[i].sh_name, ELF_CTF) == 0) {
			if (shp[i].sh_offset + shp[i].sh_size <= size) {
				error = kexec_read(kargs, (void *)maxp,
				    shp[i].sh_size, sh[i].sh_offset);
				if (error != 0)
					goto fail;
				maxp += roundup(shp[i].sh_size,
				    sizeof(Elf64_Addr));
				shp[i].sh_offset = off;
				shp[i].sh_flags |= SHF_ALLOC;
				off += roundup(shp[i].sh_size,
				    sizeof(Elf64_Addr));
			}
		}
	}

	/* Copy section headers */
	memcpy((void *)shpp, shp, shsize);

	/* Copy and adjust ELF header */
	*elfp = eh;
	elfp->e_phoff = 0;
	elfp->e_shoff = sizeof(Elf64_Ehdr);
	elfp->e_phentsize = 0;
	elfp->e_phnum = 0;

	vfs_shutdown(p);

	printf("launching kernel\n");

	config_suspend_all(DVACT_POWERDOWN);

	intr_disable();

	kexec = kexec_prepare(symend - start + PA_KERN, size,
	    kargs->boothowto, &spva, &rwva);

	kexec(rwva, (vaddr_t)addr, PA_KERN + (eh.e_entry - start), size,
	    spva);

	while (1)
		__asm("hlt");

fail:
	if (addr)
		km_free(addr, size, &kv_any, &kp_kexec);
	if (ph)
		free(ph, M_TEMP, phsize);
	if (sh)
		free(sh, M_TEMP, shsize);
	if (shstrp)
		free(shstrp, M_TEMP, strsize);
	return error;
}

int
kexec_read(struct kexec_args *kargs, void *buf, size_t size, off_t off)
{
	if (off + size < off || off + size > kargs->klen)
		return ENOEXEC;
	return copyin(kargs->kimg + off, buf, size);
}

void *
kexec_prepare(vaddr_t end, size_t ksz, int boothowto, vaddr_t *spva,
    vaddr_t *rwkva)
{
	extern char bootinfo[];
	extern int bootinfo_size;
	extern dev_t bootdev;
	vaddr_t	va_trampoline, va_data, kstart, va;
	paddr_t pa, kend;
	uint32_t *stack;
	int i;

	KASSERT(spva != NULL);
	KASSERT(rwkva != NULL);

	/*
	 * Map trampoline:
	 * - use two low physical pages previously reserved
	 * - identity map in low memory; to be used in 32bit mode
	 *
	 * Use PMAP_EFI to clear PG_u, thus making these pages
	 * kernel space pages.
	 */
	va_trampoline = KEXEC_TRAMPOLINE;
	pa = KEXEC_TRAMPOLINE;

	pmap_enter(curproc->p_vmspace->vm_map.pmap, pa, (vaddr_t)pa,
	    PROT_READ | PROT_WRITE | PROT_EXEC,
	    PROT_READ | PROT_WRITE | PROT_EXEC | PMAP_WIRED | PMAP_EFI);

	memset((caddr_t)va_trampoline, 0xcc, PAGE_SIZE);
	memcpy((caddr_t)va_trampoline, kexec_tramp, kexec_size);

	va_data = KEXEC_TRAMP_DATA;
	pa = KEXEC_TRAMP_DATA;

	pmap_enter(curproc->p_vmspace->vm_map.pmap, pa, (vaddr_t)pa,
	    PROT_READ | PROT_WRITE,
	    PROT_READ | PROT_WRITE | PMAP_WIRED | PMAP_EFI);

	memset((caddr_t)va_data, 0xcc, PAGE_SIZE);

	/* Copy bootinfo to bottom of data page */
	memcpy((caddr_t)va_data, bootinfo, bootinfo_size);

	/* Set up stack at top of data page */
	stack = (uint32_t *)va_data;
	i = PAGE_SIZE / sizeof(uint32_t) - 1;
	stack[i--] = va_data;
	stack[i--] = bootinfo_size;
	stack[i--] = 0;
	stack[i--] = 0;
	stack[i--] = end;
	stack[i--] = BOOTARG_APIVER;
	stack[i--] = bootdev;
	stack[i] = boothowto;
	*spva = (vaddr_t)&stack[i];

	/* Map current kernel read/write */
	kstart = (vaddr_t)km_alloc(ksz, &kv_any, &kp_none, &kd_waitok);
	kend = PA_KERN + round_page(ksz);

	for (pa = PA_KERN, va = kstart; pa < kend; pa += PAGE_SIZE,
	    va += PAGE_SIZE)
		pmap_kenter_pa(va, pa, PROT_READ | PROT_WRITE);
	*rwkva = kstart;

	return (void *)KEXEC_TRAMPOLINE;
}
