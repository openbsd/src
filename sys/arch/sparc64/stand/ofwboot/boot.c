/*	$NetBSD: boot.c,v 1.3 2001/05/31 08:55:19 mrg Exp $	*/
#define DEBUG
/*
 * Copyright (c) 1997, 1999 Eduardo E. Horvath.  All rights reserved.
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * ELF support derived from NetBSD/alpha's boot loader, written
 * by Christopher G. Demetriou.
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

/*
 * First try for the boot code
 *
 * Input syntax is:
 *	[promdev[{:|,}partition]]/[filename] [flags]
 */

#ifdef ELFSIZE
#undef	ELFSIZE		/* We use both. */
#endif

#include <lib/libsa/stand.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <machine/boot_flag.h>

#include <machine/cpu.h>

#include "ofdev.h"
#include "openfirm.h"

#define	MEG	(1024*1024)

/*
 * Boot device is derived from ROM provided information, or if there is none,
 * this list is used in sequence, to find a kernel.
 */
char *kernels[] = {
	"bsd ",
	"obsd ",
	NULL
};

char *kernelname;
char bootdev[128];
char bootfile[128];
int boothowto;
int debug;


#ifdef SPARC_BOOT_ELF
int	elf32_exec __P((int, Elf32_Ehdr *, u_int64_t *, void **, void **));
int	elf64_exec __P((int, Elf64_Ehdr *, u_int64_t *, void **, void **));
#endif

#ifdef SPARC_BOOT_AOUT
int	aout_exec __P((int, struct exec *, u_int64_t *, void **));
#endif

#if 0
static void
prom2boot(dev)
	char *dev;
{
	char *cp, *lp = 0;
	int handle;
	char devtype[16];
	
	for (cp = dev; *cp; cp++)
		if (*cp == ':')
			lp = cp;
	if (!lp)
		lp = cp;
	*lp = 0;
}
#endif

static void
parseargs(str, howtop)
	char *str;
	int *howtop;
{
	char *cp;
	int i;

	/* Allow user to drop back to the PROM. */
	if (strcmp(str, "exit") == 0 || strcmp(str, "halt") == 0)
		_rtt();

	/* Insert the kernel name if it is not there. */
	if (str[0] == 0 || str[0] == '-') {
		/* Move args down the string */
		i=0;
		for (cp = str + strlen(kernelname); str[i]; i++)
			cp[i] = str[i];
		/* Copy over kernelname */
		for (i = 0; kernelname[i]; i++)
			str[i] = kernelname[i];
	}
	*howtop = 0;
	for (cp = str; *cp; cp++)
		if (*cp == ' ' || *cp == '-')
			break;
	if (!*cp)
		return;
	
	*cp++ = 0;
	while (*cp) {
		BOOT_FLAG(*cp, *howtop);
		/* handle specialties */
		switch (*cp++) {
		case 'd':
			if (!debug) debug = 1;
			break;
		case 'D':
			debug = 2;
			break;
		default:
			break;
		}
	}
}


static void
chain(pentry, args, ssym, esym)
	u_int64_t pentry;
	char *args;
	void *ssym;
	void *esym;
{
	extern char end[];
	void (*entry)();
	int l, machine_tag;
	long newargs[3];

	entry = (void*)(long)pentry;

	freeall();
	/*
	 * When we come in args consists of a pointer to the boot
	 * string.  We need to fix it so it takes into account
	 * other params such as romp.
	 */

	/*
	 * Stash pointer to end of symbol table after the argument
	 * strings.
	 */
	l = strlen(args) + 1;
	bcopy(&esym, args + l, sizeof(esym));
	l += sizeof(esym);

	/*
	 * Tell the kernel we're an OpenFirmware system.
	 */
#define SPARC_MACHINE_OPENFIRMWARE		0x44444230
	machine_tag = SPARC_MACHINE_OPENFIRMWARE;
	bcopy(&machine_tag, args + l, sizeof(machine_tag));
	l += sizeof(machine_tag);

	/* 
	 * Since we don't need the boot string (we can get it from /chosen)
	 * we won't pass it in.  Just pass in esym and magic #
	 */
	newargs[0] = SPARC_MACHINE_OPENFIRMWARE;
	newargs[1] = (long)esym;
	newargs[2] = (long)ssym;
	args = (char *)newargs;
	l = sizeof(newargs);

#ifdef DEBUG
	printf("chain: calling OF_chain(%x, %x, %x, %x, %x)\n",
	       (void *)RELOC, end - (char *)RELOC, entry, args, l);
#endif
	/* if -D is set then pause in the PROM. */
	if (debug > 1) OF_enter();
	OF_chain((void *)RELOC, ((end - (char *)RELOC)+NBPG)%NBPG, entry, args, l);
	panic("chain");
}

int
loadfile(fd, args)
	int fd;
	char *args;
{
	union {
#ifdef SPARC_BOOT_AOUT
		struct exec aout;
#endif
#ifdef SPARC_BOOT_ELF
		Elf32_Ehdr elf32;
		Elf64_Ehdr elf64;
#endif
	} hdr;
	int rval;
	u_int64_t entry = 0;
	void *ssym;
	void *esym;

	rval = 1;
	ssym = NULL;
	esym = NULL;

	/* Load the header. */
#ifdef DEBUG
	printf("loadfile: reading header\n");
#endif
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		printf("read header: %s\n", strerror(errno));
		goto err;
	}

	/* Determine file type, load kernel. */
#ifdef SPARC_BOOT_AOUT
	if (N_BADMAG(hdr.aout) == 0 && N_GETMID(hdr.aout) == MID_SPARC) {
		rval = aout_exec(fd, &hdr.aout, &entry, &esym);
	} else
#endif
#ifdef SPARC_BOOT_ELF
	if (bcmp(hdr.elf32.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
		rval = elf32_exec(fd, &hdr.elf32, &entry, &ssym, &esym);
	} else
	if (bcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
		rval = elf64_exec(fd, &hdr.elf64, &entry, &ssym, &esym);
	} else
#endif
	{
		printf("unknown executable format\n");
	}

	if (rval)
		goto err;

	printf(" start=0x%lx\n", (unsigned long)entry);

	close(fd);

	/* XXX this should be replaced w/ a mountroothook. */
	if (floppyboot) {
		printf("Please insert root disk and press ENTER ");
		getchar();
		printf("\n");
	}

	chain(entry, args, ssym, esym);
	/* NOTREACHED */

 err:
	close(fd);
	return (rval);
}

#ifdef SPARC_BOOT_AOUT
int
aout_exec(fd, hdr, entryp, esymp)
	int fd;
	struct exec *hdr;
	u_int64_t *entryp;
	void **esymp;
{
	void *addr;
	int n, *paddr;

#ifdef DEBUG
	printf("auout_exec: ");
#endif
	/* Display the load address (entry point) for a.out. */
	printf("Booting %s @ 0x%lx\n", opened_name, hdr->a_entry);
	addr = (void *)((u_int64_t)hdr->a_entry);

	/*
	 * Determine memory needed for kernel and allocate it from
	 * the firmware.
	 */
	n = hdr->a_text + hdr->a_data + hdr->a_bss + hdr->a_syms + sizeof(int);
	if ((paddr = OF_claim(addr, n, 0)) == (int *)-1)
		panic("cannot claim memory");

	/* Load text. */
	lseek(fd, N_TXTOFF(*hdr), SEEK_SET);
	printf("%lu", hdr->a_text);
	if (read(fd, paddr, hdr->a_text) != hdr->a_text) {
		printf("read text: %s\n", strerror(errno));
		return (1);
	}
	syncicache((void *)paddr, hdr->a_text);

	/* Load data. */
	printf("+%lu", hdr->a_data);
	if (read(fd, (void *)paddr + hdr->a_text, hdr->a_data) != hdr->a_data) {
		printf("read data: %s\n", strerror(errno));
		return (1);
	}

	/* Zero BSS. */
	printf("+%lu", hdr->a_bss);
	bzero((void *)paddr + hdr->a_text + hdr->a_data, hdr->a_bss);

	/* Symbols. */
	*esymp = paddr;
	paddr = (int *)((void *)paddr + hdr->a_text + hdr->a_data + hdr->a_bss);
	*paddr++ = hdr->a_syms;
	if (hdr->a_syms) {
		printf(" [%lu", hdr->a_syms);
		if (read(fd, paddr, hdr->a_syms) != hdr->a_syms) {
			printf("read symbols: %s\n", strerror(errno));
			return (1);
		}
		paddr = (int *)((void *)paddr + hdr->a_syms);
		if (read(fd, &n, sizeof(int)) != sizeof(int)) {
			printf("read symbols: %s\n", strerror(errno));
			return (1);
		}
		if (OF_claim((void *)paddr, n + sizeof(int), 0) == (void *)-1)
			panic("cannot claim memory");
		*paddr++ = n;
		if (read(fd, paddr, n - sizeof(int)) != n - sizeof(int)) {
			printf("read symbols: %s\n", strerror(errno));
			return (1);
		}
		printf("+%d]", n - sizeof(int));
		*esymp = paddr + (n - sizeof(int));
	}

	*entryp = hdr->a_entry;
	return (0);
}
#endif /* SPARC_BOOT_AOUT */

#ifdef SPARC_BOOT_ELF
#if 1
/* New style */

#ifdef ELFSIZE
#undef ELFSIZE
#endif

#define ELFSIZE	32
#include "elfXX_exec.c"

#undef ELFSIZE
#define ELFSIZE	64
#include "elfXX_exec.c"

#else
/* Old style */
int
elf32_exec(fd, elf, entryp, ssymp, esymp)
	int fd;
	Elf32_Ehdr *elf;
	u_int64_t *entryp;
	void **ssymp;
	void **esymp;
{
	Elf32_Shdr *shp;
	Elf32_Off off;
	void *addr;
	size_t size;
	int i, first = 1;
	long align;
	int n;

	/*
	 * Don't display load address for ELF; it's encoded in
	 * each section.
	 */
#ifdef DEBUG
	printf("elf_exec: ");
#endif
	printf("Booting %s\n", opened_name);

	for (i = 0; i < elf->e_phnum; i++) {
		Elf32_Phdr phdr;
		(void)lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		printf("%s%lu@0x%lx", first ? "" : "+", phdr.p_filesz,
		    (u_long)phdr.p_vaddr);
		(void)lseek(fd, phdr.p_offset, SEEK_SET);

		/* 
		 * If the segment's VA is aligned on a 4MB boundary, align its
		 * request 4MB aligned physical memory.  Otherwise use default
		 * alignment.
		 */
		align = phdr.p_align;
		if ((phdr.p_vaddr & (4*MEG-1)) == 0)
			align = 4*MEG;
		if (OF_claim((void *)phdr.p_vaddr, phdr.p_memsz, phdr.p_align) ==
		    (void *)-1)
			panic("cannot claim memory");
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			printf("read segment: %s\n", strerror(errno));
			return (1);
		}
		syncicache((void *)phdr.p_vaddr, phdr.p_filesz);

		/* Zero BSS. */
		if (phdr.p_filesz < phdr.p_memsz) {
			printf("+%lu@0x%lx", phdr.p_memsz - phdr.p_filesz,
			    (u_long)(phdr.p_vaddr + phdr.p_filesz));
			bzero((void*)phdr.p_vaddr + phdr.p_filesz,
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	printf(" \n");

#if 1 /* I want to rethink this... --thorpej@netbsd.org */
	/*
	 * Compute the size of the symbol table.
	 */
	size = sizeof(Elf32_Ehdr) + (elf->e_shnum * sizeof(Elf32_Shdr));
	shp = addr = alloc(elf->e_shnum * sizeof(Elf32_Shdr));
	(void)lseek(fd, elf->e_shoff, SEEK_SET);
	if (read(fd, addr, elf->e_shnum * sizeof(Elf32_Shdr)) !=
	    elf->e_shnum * sizeof(Elf32_Shdr)) {
		printf("read section headers: %s\n", strerror(errno));
		return (1);
	}
	for (i = 0; i < elf->e_shnum; i++, shp++) {
		if (shp->sh_type == SHT_NULL)
			continue;
		if (shp->sh_type != SHT_SYMTAB
		    && shp->sh_type != SHT_STRTAB) {
			shp->sh_offset = 0; 
			shp->sh_type = SHT_NOBITS;
			continue;
		}
		size += shp->sh_size;
	}
	shp = addr;

	/*
	 * Reserve memory for the symbols.
	 */
	if ((addr = OF_claim(0, size, NBPG)) == (void *)-1)
		panic("no space for symbol table");

	/*
	 * Copy the headers.
	 */
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf32_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	bcopy(elf, addr, sizeof(Elf32_Ehdr));
	bcopy(shp, addr + sizeof(Elf32_Ehdr), elf->e_shnum * sizeof(Elf32_Shdr));
	free(shp, elf->e_shnum * sizeof(Elf32_Shdr));
	*ssymp = addr;

	/*
	 * Now load the symbol sections themselves.
	 */
	shp = addr + sizeof(Elf32_Ehdr);
	addr += sizeof(Elf32_Ehdr) + (elf->e_shnum * sizeof(Elf32_Shdr));
	off = sizeof(Elf32_Ehdr) + (elf->e_shnum * sizeof(Elf32_Shdr));
	for (first = 1, i = 0; i < elf->e_shnum; i++, shp++) {
		if (shp->sh_type == SHT_SYMTAB
		    || shp->sh_type == SHT_STRTAB) {
			if (first)
				printf("symbols @ 0x%lx ", (u_long)addr);
			printf("%s%d", first ? "" : "+", shp->sh_size);
			(void)lseek(fd, shp->sh_offset, SEEK_SET);
			if (read(fd, addr, shp->sh_size) != shp->sh_size) {
				printf("read symbols: %s\n", strerror(errno));
				return (1);
			}
			addr += (shp->sh_size+3)&(~3);
			shp->sh_offset = off;
			off += (shp->sh_size+3)&(~3);
			first = 0;
		}
	}
	*esymp = addr;
#endif /* 0 */

	*entryp = elf->e_entry;
	return (0);
}
#endif
#endif /* SPARC_BOOT_ELF */

void
main()
{
	extern char version[];
	int chosen;
	char bootline[512];		/* Should check size? */
	char *cp;
	int i, fd;
	
	/* Initialize kernelname */
	kernelname = kernels[0];

	printf(">> %s", version);

	/*
	 * Get the boot arguments from Openfirmware
	 */
	if ((chosen = OF_finddevice("/chosen")) == -1
	    || OF_getprop(chosen, "bootpath", bootdev, sizeof bootdev) < 0
	    || OF_getprop(chosen, "bootargs", bootline, sizeof bootline) < 0) {
		printf("Invalid Openfirmware environment\n");
		exit();
	}
	/*prom2boot(bootdev);*/
	kernelname = kernels[0];
	parseargs(bootline, &boothowto);
	for (i=0;;) {
		kernelname = kernels[i];
		if (boothowto & RB_ASKNAME) {
			printf("Boot: ");
			gets(bootline);
			parseargs(bootline, &boothowto);
		}
		if ((fd = open(bootline, 0)) >= 0)
			break;
		if (errno)
			printf("open %s: %s\n", opened_name, strerror(errno));
		/*
		 * if we have are not in askname mode, and we aren't using the
		 * prom bootfile, try the next one (if it exits).  otherwise,
		 * go into askname mode.
		 */
		if ((boothowto & RB_ASKNAME) == 0 &&
		    i != -1 && kernels[++i]) {
			printf(": trying %s...\n", kernels[i]);
		} else {
			printf("\n");
			boothowto |= RB_ASKNAME;
		}
	}
#ifdef	__notyet__
	OF_setprop(chosen, "bootpath", opened_name, strlen(opened_name) + 1);
	cp = bootline;
#else
	strcpy(bootline, opened_name);
	cp = bootline + strlen(bootline);
	*cp++ = ' ';
#endif
	*cp = '-';
	if (boothowto & RB_ASKNAME)
		*++cp = 'a';
	if (boothowto & RB_SINGLE)
		*++cp = 's';
	if (boothowto & RB_KDB)
		*++cp = 'd';
	if (*cp == '-')
#ifdef	__notyet__
		*cp = 0;
#else
		*--cp = 0;
#endif
	else
		*++cp = 0;
#ifdef	__notyet__
	OF_setprop(chosen, "bootargs", bootline, strlen(bootline) + 1);
#endif
	/* XXX void, for now */
#ifdef DEBUG
	if (debug)
		printf("main: Calling loadfile(fd, %s)\n", bootline);
#endif
	(void)loadfile(fd, bootline);

	_rtt();
}
