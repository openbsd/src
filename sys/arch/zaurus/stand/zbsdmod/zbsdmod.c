/*	$OpenBSD: zbsdmod.c,v 1.7 2005/05/02 02:45:29 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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
 *
 * Zaurus OpenBSD bootstrap loader.
 */

#include "compat_linux.h"

#define BOOTARGS_BUFSIZ	256
#define BOOTARGS_MAGIC	0x4f425344

#define ZBOOTDEV_MAJOR	99
#define ZBOOTDEV_MODE	0222
#define ZBOOTDEV_NAME	"zboot"
#define ZBOOTMOD_NAME	"zbsdmod"

/* Prototypes */
void	elf32bsdboot(void);
int	init_module(void);
void	cleanup_module(void);

ssize_t	zbsdmod_write(struct file *, const char *, size_t, loff_t *);
int	zbsdmod_open(struct inode *, struct file *);
int	zbsdmod_close(struct inode *, struct file *);

static	struct file_operations fops = {
	0,			/* struct module *owner */
	0,			/* lseek */
	0,			/* read */
	zbsdmod_write,		/* write */
	0,			/* readdir */
	0,			/* poll */
	0,			/* ioctl */
	0,			/* mmap */
	zbsdmod_open,		/* open */
	0,			/* flush */
	zbsdmod_close,		/* release */
	0,			/* sync */
	0,			/* async */
	0,			/* check media change */
	0,			/* revalidate */
	0,			/* lock */
};

static	int isopen;
static	loff_t position;

/* Outcast local variables to avoid stack usage in elf32bsdboot(). */
static	int cpsr;
static	unsigned int sz;
static	int i;
static	vaddr_t minv, maxv, posv;
static	vaddr_t elfv, shpv;
static	int *addr;
static	vaddr_t *esymp;
static	Elf_Shdr *shp;
static	Elf_Off off;
static	int havesyms;

/* The maximum size of a kernel image is restricted to 5MB. */
static	int bsdimage[1310720];	/* XXX use kmalloc() */
static	char bootargs[BOOTARGS_BUFSIZ];

/*
 * Boot the loaded BSD kernel image, or return if an error is found.
 * Part of this routine is borrowed from sys/lib/libsa/loadfile.c.
 */
void
elf32bsdboot(void)
{

#define elf	((Elf32_Ehdr *)bsdimage)
#define phdr	((Elf32_Phdr *)((char *)elf + elf->e_phoff))

	if (memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0 ||
	    elf->e_ident[EI_CLASS] != ELFCLASS32)
		return;

	minv = (vaddr_t)~0;
	maxv = (vaddr_t)0;
	posv = (vaddr_t)0;
	esymp = 0;

	/*
	 * Get min and max addresses used by the loaded kernel.
	 */
	for (i = 0; i < elf->e_phnum; i++) {

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	((p.p_flags & PF_X) == 0)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if (IS_TEXT(phdr[i]) || IS_DATA(phdr[i])) {
			posv = phdr[i].p_vaddr;
			if (minv > posv)
				minv = posv;
			posv += phdr[i].p_filesz;
			if (maxv < posv)
				maxv = posv;
		}
		if (IS_DATA(phdr[i]) && IS_BSS(phdr[i])) {
			posv += phdr[i].p_memsz;
			if (maxv < posv)
				maxv = posv;
		}
		/*
		 * 'esym' is the first word in the .data section,
		 * and marks the end of the symbol table.
		 */
		if (IS_DATA(phdr[i]) && !IS_BSS(phdr[i]))
			esymp = (vaddr_t *)phdr[i].p_vaddr;
	}

	__asm__ volatile ("mrs %0, cpsr_all" : "=r" (cpsr));
	cpsr |= 0xc0;  /* set FI */
	__asm__ volatile ("msr cpsr_all, %0" :: "r" (cpsr));

	/*
	 * Copy the boot arguments.
	 */
	sz = BOOTARGS_BUFSIZ;
	while (sz > 0) {
		sz--;
		((char *)minv - BOOTARGS_BUFSIZ)[sz] = bootargs[sz];
	}

	/*
	 * Set up pointers to copied ELF and section headers.
	 */
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))
	elfv = maxv = roundup(maxv, sizeof(long));
	maxv += sizeof(Elf_Ehdr);

	sz = elf->e_shnum * sizeof(Elf_Shdr);
	shp = (Elf_Shdr *)((vaddr_t)elf + elf->e_shoff);
	shpv = maxv;
	maxv += roundup(sz, sizeof(long));

	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned, and offsets are relative to the
	 * copied ELF header.  Don't bother with string tables if
	 * there are no symbol sections.
	 */
	off = roundup((sizeof(Elf_Ehdr) + sz), sizeof(long));
	for (havesyms = i = 0; i < elf->e_shnum; i++)
		if (shp[i].sh_type == SHT_SYMTAB)
			havesyms = 1;
	for (i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type == SHT_SYMTAB ||
		    shp[i].sh_type == SHT_STRTAB) {
			if (havesyms) {
				sz = shp[i].sh_size;
				while (sz > 0) {
					sz--;
					((char *)maxv)[sz] =
					    ((char *)elf +
						shp[i].sh_offset)[sz];
				}
			}
			maxv += roundup(shp[i].sh_size, sizeof(long));
			shp[i].sh_offset = off;
			off += roundup(shp[i].sh_size, sizeof(long));
		}
	}

	/*
	 * Copy the ELF and section headers.
	 */
	sz = sizeof(Elf_Ehdr);
	while (sz > 0) {
		sz--;
		((char *)elfv)[sz] = ((char *)elf)[sz];
	}
	sz = elf->e_shnum * sizeof(Elf_Shdr);
	while (sz > 0) {
		sz--;
		((char *)shpv)[sz] = ((char *)shp)[sz];
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfv.
	 */
	((Elf_Ehdr *)elfv)->e_phoff = 0;
	((Elf_Ehdr *)elfv)->e_shoff = sizeof(Elf_Ehdr);
	((Elf_Ehdr *)elfv)->e_phentsize = 0;
	((Elf_Ehdr *)elfv)->e_phnum = 0;

	/*
	 * Tell locore.S where the symbol table ends, and arrange
	 * to skip esym when loading the data section.
	 */
	if (esymp != 0)
		*esymp = (vaddr_t)maxv;
	for (i = 0; esymp != 0 && i < elf->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;
		if (phdr[i].p_vaddr == (vaddr_t)esymp) {
			phdr[i].p_vaddr = (vaddr_t)((char *)phdr[i].p_vaddr + sizeof(long));
			phdr[i].p_offset = (vaddr_t)((char *)phdr[i].p_offset + sizeof(long));
			phdr[i].p_filesz -= sizeof(long);
			break;
		}
	}

	/*
	 * Load text and data.
	 */
	for (i = 0; i < elf->e_phnum; i++) {

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

		if (IS_TEXT(phdr[i]) || IS_DATA(phdr[i])) {
			sz = phdr[i].p_filesz;
			while (sz > 0) {
				sz--;
				((char *)phdr[i].p_vaddr)[sz] =
				    (((char *)elf) + phdr[i].p_offset)[sz];
			}
		}
	}

	addr = (int *)(elf->e_entry);
	__asm__ volatile (
		"mov  r0, %0;"
		"mov  r2, #0;"
		"mov  r1, #(0x00000010 | 0x00000020);"
		"mcr  15, 0, r1, c1, c0, 0;"
		"mcr  15, 0, r2, c8, c7, 0    /* nail I+D TLB on ARMv4 and greater */;"
		"mov  pc, r0" :: "r"(addr) : "r0","r1","r2");
}

/*
 * Initialize the LKM.
 */
int
init_module()
{
	struct proc_dir_entry *entry;
	int rc;

	rc = register_chrdev(ZBOOTDEV_MAJOR, ZBOOTDEV_NAME, &fops);
	if (rc != 0) {
		printk("%s: register_chrdev(%d, ...): error %d\n",
		    ZBOOTMOD_NAME, -rc);
		return 1;
	}

	entry = proc_mknod(ZBOOTDEV_NAME, ZBOOTDEV_MODE | S_IFCHR,
	    &proc_root, MKDEV(ZBOOTDEV_MAJOR, 0));
	if (entry == (struct proc_dir_entry *)0) {
		(void)unregister_chrdev(ZBOOTDEV_MAJOR, ZBOOTDEV_NAME);
		return 1;
	}

	printk("%s: OpenBSD/" MACHINE " bootstrap device is %d,0\n",
	    ZBOOTMOD_NAME, ZBOOTDEV_MAJOR);

	return 0;
}

/*
 * Cleanup - undo whatever init_module did.
 */
void
cleanup_module()
{

	(void)unregister_chrdev(ZBOOTDEV_MAJOR, ZBOOTDEV_NAME);
	remove_proc_entry(ZBOOTDEV_NAME, &proc_root);

	printk("%s: OpenBSD/" MACHINE " bootstrap device unloaded\n",
	    ZBOOTMOD_NAME);
}


ssize_t
zbsdmod_write(struct file *f, const char *buf, size_t len, loff_t *offp)
{

	if (len < 1)
		return 0;

	if (*offp + len >= sizeof(bsdimage))
		return EFBIG;

	memcpy(((char *)bsdimage) + *offp, buf, len);

	*offp += len;
	if (*offp > position)
		position = *offp;

	return len;
}

int
zbsdmod_open(struct inode *ino, struct file *f)
{

	/* XXX superuser check */

	if (isopen)
		return -EBUSY;

	isopen = 1;
	position = 0;

	return 0;
}

int
zbsdmod_close(struct inode *ino, struct file *f)
{

	if (isopen) {
		if (position > 0) {
			printk("%s: loaded %d bytes\n", ZBOOTDEV_NAME,
			    position);

			if (position < BOOTARGS_BUFSIZ) {
				*(int *)bootargs = BOOTARGS_MAGIC;
				bootargs[position + sizeof(int)] = '\0';
				memcpy(bootargs + sizeof(int), bsdimage,
				    position);
			} else {
#ifndef _TEST
				elf32bsdboot();
				printk("%s: boot failed\n", ZBOOTDEV_NAME);       
#else
				printk("/* boot() */\n");
#endif
			}
		}
		isopen = 0;
		return 0;
	}

	return -EBUSY;
}
