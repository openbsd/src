/*	$OpenBSD: boot.c,v 1.13 2015/03/05 20:46:13 miod Exp $	*/
/*	$NetBSD: boot.c,v 1.2 1997/09/14 19:27:21 pk Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#define _KERNEL
#include <sys/fcntl.h>
#undef _KERNEL

#include <lib/libsa/loadfile.h>
#include <lib/libsa/stand.h>

#include <sparc/stand/common/promdev.h>

int debug;
int netif_debug;

#define	FOURMB		0x400000
#ifndef	RELOC2
#define	RELOC2		(RELOC + 0x40000)
#endif
#define	LOWSTACK	(16 * 1024)

/*
 * Boot device is derived from ROM provided information.
 */
#define	DEFAULT_KERNEL	"bsd"

extern char	*version;
char		fbuf[80], dbuf[128];
char		rnddata[BOOTRANDOM_MAX];

paddr_t	bstart, bend;	/* physical start & end address of the boot program */
int	compat = 1;	/* try to load in compat mode */
int	rnd_loaded = 0;

typedef void (*entry_t)(u_long, int, int, int, long, long);

int	fdloadfile(int, u_long *, int);
int	loadrandom(const char *, void *, size_t);

static paddr_t
getphysmem(u_long size)
{
	struct	memarr *pmemarr;	/* physical memory regions */
	u_int	npmemarr;		/* number of entries in pmemarr */
	struct memarr *mp;
	int i;
#ifdef DEBUG
	static int arrdpy;
#endif

	/*
	 * Get available physical memory from the prom.
	 */
	npmemarr = prom_makememarr(NULL, 0, MEMARR_AVAILPHYS);
	pmemarr = alloc(npmemarr*sizeof(struct memarr));
	if (pmemarr == NULL)
		return ((paddr_t)-1);
	npmemarr = prom_makememarr(pmemarr, npmemarr, MEMARR_AVAILPHYS);

#ifdef DEBUG
	if (arrdpy == 0) {
		arrdpy = 1;
		printf("Available physical memory:\n");
		for (mp = pmemarr, i = (int)npmemarr; --i >= 0; mp++) {
			uint64_t addr;
			addr = pmemarr[i].addr_hi;
			addr <<= 32;
			addr |= pmemarr[i].addr_lo;
			printf("%p at 0x%llx\n", pmemarr[i].len, addr);
		}
	}
#endif

	/*
	 * Find a suitable loading address.
	 */
	for (mp = pmemarr, i = (int)npmemarr; --i >= 0; mp++) {
		paddr_t pa;
		u_long len;

		/* Skip memory ranges the kernel can't use yet on sun4d */
		if (pmemarr[i].addr_hi != 0)
			continue;
		pa = (paddr_t)pmemarr[i].addr_lo;
		if (pa >= 0x80000000)
			continue;
		len = (u_long)pmemarr[i].len;
		if (len >= 0x80000000)
			len = 0x80000000;
		if (pa + len > 0x80000000)
			len = 0x80000000 - pa;

		if (len < size)
			continue;

		/* Check whether it will fit in front of us */
		if (pa < bstart && len >= size && (bstart - pa) >= size)
			return (pa);

		/* Skip the boot program memory */
		if (pa < bend) {
			if (len < bend - pa)
				/* Not large enough */
				continue;

			/* Shrink this segment */
			len -=  bend - pa;
			pa = bend;
		}

		/* Does it fit in the remainder of this segment? */
		if (len >= size)
			return (pa);
	}
	return ((paddr_t)-1);
}

static int
loadk(char *file, u_long *marks)
{
	int fd, error, flags;
	vaddr_t va;
	paddr_t pa;
	u_long minsize, size;
	vaddr_t extra;

	/*
	 * Regardless of the address where we load the kernel, we need to
	 * make sure it has enough valid space to use during pmap_bootstrap.
	 * locore.s tries to use the 512KB following the kernel image, and
	 * we need to make sure this extra room does not overwrite PROM data
	 * (such as the PROM page tables which are immediately below 4MB on
	 * most sun4c).
	 */
	extra = 512 * 1024;

	if ((fd = open(file, O_RDONLY)) < 0)
		return (errno ? errno : ENOENT);

	/*
	 * We need to know whether we are booting off a tape or not,
	 * because we can not seek backwards off tapes.
	 */

	if (files[fd].f_flags & F_RAW) {
		flags = (COUNT_KERNEL & ~COUNT_SYM) | (LOAD_KERNEL & ~LOAD_SYM);
		minsize = FOURMB;
		va = 0xf8000000;		/* KERNBASE */
#ifdef DEBUG
		printf("Tape boot: expecting a bsd.rd kernel smaller than %p\n",
		    minsize);
#endif
		/* compensate for extra room below */
		minsize -= extra;
	} else {
		/*
		 * If we did not load a random.seed file yet, try and load
		 * one.
		 */
		if (rnd_loaded == 0) {
			/*
			 * Some PROM do not like having a network device
			 * open()ed twice; better close and reopen after
			 * trying to get randomness.
			 */
			close(fd);

			rnd_loaded = loadrandom(BOOTRANDOM, rnddata,
			    sizeof(rnddata));

			if ((fd = open(file, O_RDONLY)) < 0)
				return (errno ? errno : ENOENT);
		}

		flags = LOAD_KERNEL;
		marks[MARK_START] = 0;

		/*
		 * Even though we just have opened the file, the gzip code
		 * has tried to read from it. Be sure to reset position in
		 * case the file is not compressed (transparent mode isn't
		 * so transparent...)
		 */
		if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
			error = errno;
			goto out;
		}

		if ((error = fdloadfile(fd, marks, COUNT_KERNEL)) != 0)
			goto out;

		/* rewind file for the actual load operation later */
		if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
			error = errno;
			goto out;
		}

		minsize = marks[MARK_END] - marks[MARK_START];

		/* We want that leading 16K in front of the kernel image */
		minsize += PROM_LOADADDR;
		va = marks[MARK_START] - PROM_LOADADDR;
	}

	/*
	 * If the kernel would entirely fit under the boot code, and the
	 * boot code has been loaded 1:1, we do not need to allocate
	 * breathing room after it.
	 */
	size = minsize + extra;
	if (CPU_ISSUN4M || CPU_ISSUN4D)
		size += 1024 * 1024;
	if (compat != 0) {
		if (size <= RELOC2 - LOWSTACK)
			size = RELOC2 - LOWSTACK;
		else
			compat = 0;
	}

	/* Get a physical load address */
#ifdef DEBUG
	printf("kernel footprint %p, requesting %p\n", minsize, size);
#endif
	pa = getphysmem(size);
	if (pa == (paddr_t)-1) {
		/*
		 * The extra bootstrap memory estimate might have been
		 * too much, if physical memory doesn't have any contiguous
		 * large chunks (e.g. on sun4c systems with 4MB regions).
		 * If that increase caused us to cross a 4MB boundary, try
		 * to limit ourselves to a 4MB multiple.
		 */
		if (compat == 0 && size / FOURMB != minsize / FOURMB) {
			size = roundup(minsize, FOURMB);
#ifdef DEBUG
			printf("now trying %p\n", size);
#endif
			pa = getphysmem(size);
		}
		if (pa == (paddr_t)-1) {
			error = EFBIG;
			goto out;
		}
	}

	printf("Loading at physical address %lx\n", pa);
	if (pmap_map(va, pa, size) != 0) {
		error = EFAULT;
		goto out;
	}

	/* try and double-map at VA 0 for compatibility */
	if (pa + size > bstart) {
#ifdef DEBUG
		printf("WARNING: %s is too large for compat mode.\n"
		    "If your kernel is too old, it will not run correctly.\n",
		    file);
#endif
	} else {
		if (pa != 0 && pmap_map(0, pa, size) != 0) {
			error = EFAULT;
			goto out;
		}
	}

	marks[MARK_START] = 0;
	error = fdloadfile(fd, marks, flags);
out:
	close(fd);
	return (error);
}

int
main(int argc, char *argv[])
{
	int	error;
	char	*file;
	u_long	marks[MARK_MAX];
	extern char start[];		/* top of stack (see srt0.S) */
	vaddr_t	bstart_va;

	prom_init();
	mmu_init();

	printf(">> OpenBSD BOOT %s\n", version);

	/*
	 * Find the physical memory area that's in use by the boot loader.
	 * Our stack grows down from label `start'; assume we need no more
	 * than 16K of stack space.
	 * The top of the boot loader is the next 4MB boundary.
	 */
	bstart_va = (vaddr_t)start - LOWSTACK;
	if (pmap_extract(bstart_va, &bstart) != 0)
		panic("can't figure out where we have been loaded");

	if (bstart != bstart_va)
		compat = 0;

	bend = roundup(bstart, FOURMB);
#ifdef DEBUG
	printf("bstart %p bend %p\n", bstart, bend);
#endif

	file = prom_bootfile;
	if (file == 0 || *file == 0)
		file = DEFAULT_KERNEL;

	for (;;) {
		if (prom_boothow & RB_ASKNAME) {
			printf("device[%s]: ", prom_bootdevice);
			gets(dbuf);
			if (dbuf[0])
				prom_bootdevice = dbuf;
			printf("boot: ");
			gets(fbuf);
			if (fbuf[0])
				file = fbuf;
		}

		printf("Booting %s\n", file);
		if ((error = loadk(file, marks)) == 0)
			break;

		printf("Cannot load %s: error=%d\n", file, error);
		prom_boothow |= RB_ASKNAME;
	}

	/* Note: args 2-4 not used due to conflicts with SunOS loaders */
	(*(entry_t)marks[MARK_ENTRY])(cputyp == CPU_SUN4 ?
	    PROM_LOADADDR : (u_long)promvec, 0, 0, 0,
	    marks[MARK_END], DDB_MAGIC1);

	_rtt();
}

int
loadrandom(const char *path, void *buf, size_t buflen)
{
	struct stat sb;
	int fd;
	int rc = 0;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (errno != EPERM)
			printf("cannot open %s: %s\n", path, strerror(errno));
		return 0;
	}
	if (fstat(fd, &sb) == -1 || sb.st_uid != 0 || !S_ISREG(sb.st_mode) ||
	    (sb.st_mode & (S_IWOTH|S_IROTH)))
		goto fail;
	(void) read(fd, buf, buflen);
	rc = 1;
fail:
	close(fd);
	return rc;
}
