/*	$OpenBSD: zbsdmod.c,v 1.3 2005/01/10 21:50:54 deraadt Exp $	*/

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

#define ZBOOTDEV_MAJOR	99
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
static	Elf32_Phdr phdr[32];
static	unsigned int sz;
static	int i;
static	int *addr;

/* The maximum size of a kernel image is restricted to 8MB. */
static	int bsdimage[2097152];	/* XXX use kmalloc() */

/*
 * Boot the loaded BSD kernel image, or return if an error is found.
 * Part of this routine is borrowed from sys/lib/libsa/loadfile.c.
 */
void
elf32bsdboot(void)
{
	int cpsr;

#define elf ((Elf32_Ehdr *)bsdimage)

	if (memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0 ||
	    elf->e_ident[EI_CLASS] != ELFCLASS32)
		return;

	sz = elf->e_phnum * sizeof(Elf32_Phdr);
	while (sz > 0) {
		sz--;
		((char *)phdr)[sz] = (((char *)elf) + elf->e_phoff)[sz];
	}

	__asm__ volatile ("mrs %0, cpsr_all" : "=r" (cpsr));
	cpsr |= 0xc0;  /* set FI */
	__asm__ volatile ("msr cpsr_all, %0" :: "r" (cpsr));

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
			sz = phdr[i].p_filesz;
			while (sz > 0) {
				sz--;
				((char *)phdr[i].p_vaddr)[sz] =
				    (((char *)elf) + phdr[i].p_offset)[sz];
			}
		}
	}

	addr = (int *)0xa0200000;
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
	int rc;

	rc = register_chrdev(ZBOOTDEV_MAJOR, ZBOOTDEV_NAME, &fops);
	if (rc != 0) {
		printk("%s: register_chrdev(%d, ...): error %d\n",
		    ZBOOTMOD_NAME, -rc);
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
#ifndef _TEST
			elf32bsdboot();
			printk("%s: boot failed\n", ZBOOTDEV_NAME);
#else
			printk("/* boot() */\n");
#endif
		}
		isopen = 0;
		return 0;
	}

	return -EBUSY;
}
