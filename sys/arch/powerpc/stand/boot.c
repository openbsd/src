/*	$OpenBSD: boot.c,v 1.9 2000/01/14 05:42:17 rahnds Exp $	*/
/*	$NetBSD: boot.c,v 1.1 1997/04/16 20:29:17 thorpej Exp $	*/

/*
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

#define	ELFSIZE		32		/* We use 32-bit ELF. */

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>

#include <machine/cpu.h>
/*
#include <machine/machine_type.h>
*/

#include <powerpc/stand/ofdev.h>
#include <powerpc/stand/openfirm.h>

char bootdev[128];
char bootfile[128];
int boothowto;
int debug;

#ifdef POWERPC_BOOT_ELF
int	elf_exec __P((int, Elf32_Ehdr *, u_int32_t *, void **));
#endif

#ifdef POWERPC_BOOT_AOUT
int	aout_exec __P((int, struct exec *, u_int32_t *, void **));
#endif

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

static void
parseargs(str, howtop)
	char *str;
	int *howtop;
{
	char *cp;

	/* Allow user to drop back to the PROM. */
	if (strcmp(str, "exit") == 0)
		_rtt();

	*howtop = 0;
	for (cp = str; *cp; cp++)
		if (*cp == ' ' || *cp == '-')
			break;
	if (!*cp)
		return;
	
	*cp++ = 0;
	while (*cp) {
		switch (*cp++) {
		case 'a':
			*howtop |= RB_ASKNAME;
			break;
		case 's':
			*howtop |= RB_SINGLE;
			break;
		case 'd':
			*howtop |= RB_KDB;
			debug = 1;
			break;
		}
	}
}

static void
chain(entry, args, esym)
	void (*entry)();
	char *args;
	void *esym;
{
	extern char end[];
	int l, machine_tag;

	freeall();

	/*
	 * Stash pointer to end of symbol table after the argument
	 * strings.
	 */
	l = strlen(args) + 1;
	bcopy(&esym, args + l, sizeof(esym));
	l += sizeof(esym);

#ifdef __notyet__
	/*
	 * Tell the kernel we're an OpenFirmware system.
	 */
	machine_tag = POWERPC_MACHINE_OPENFIRMWARE;
	bcopy(&machine_tag, args + l, sizeof(machine_tag));
	l += sizeof(machine_tag);
#endif

	OF_chain((void *)RELOC, end - (char *)RELOC, entry, args, l);
	panic("chain");
}

int
loadfile(fd, args)
	int fd;
	char *args;
{
	union {
#ifdef POWERPC_BOOT_AOUT
		struct exec aout;
#endif
#ifdef POWERPC_BOOT_ELF
		Elf32_Ehdr elf;
#endif
	} hdr;
	int rval;
	u_int32_t entry;
	void *esym;

	rval = 1;
	esym = NULL;

	/* Load the header. */
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		printf("read header: %s\n", strerror(errno));
		goto err;
	}

	/* Determine file type, load kernel. */
#ifdef POWERPC_BOOT_AOUT
	if (N_BADMAG(hdr.aout) == 0 && N_GETMID(hdr.aout) == MID_POWERPC) {
		rval = aout_exec(fd, &hdr.aout, &entry, &esym);
	} else
#endif
#ifdef POWERPC_BOOT_ELF
	if (IS_ELF(hdr.elf)) {
		rval = elf_exec(fd, &hdr.elf, &entry, &esym);
	} else
#endif
	{
		printf("unknown executable format\n");
	}

	if (rval)
		goto err;

	printf(" start=0x%x\n", entry);

	close(fd);

	chain((void *)entry, args, esym);
	/* NOTREACHED */

 err:
	close(fd);
	return (rval);
}

#ifdef POWERPC_BOOT_AOUT
int
aout_exec(fd, hdr, entryp, esymp)
	int fd;
	struct exec *hdr;
	u_int32_t *entryp;
	void **esymp;
{
	void *addr;
	int n, *paddr;

	/* Display the load address (entry point) for a.out. */
	printf("Booting %s @ 0x%lx\n", opened_name, hdr->a_entry);
	addr = (void *)(hdr->a_entry);

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
#endif /* POWERPC_BOOT_AOUT */

#define LOAD_HDR 1
#define LOAD_SYM 2
#ifdef POWERPC_BOOT_ELF
int
elf_exec(fd, elf, entryp, esymp)
	int fd;
	Elf32_Ehdr *elf;
	u_int32_t *entryp;
	void **esymp;
	
{
	Elf32_Shdr *shp;
	Elf32_Off off;
	void *addr;
	size_t size;
	int i, first = 1;
	int n;
	u_int32_t maxp = 0; /*  correct type? */
	int flags = LOAD_HDR| LOAD_SYM;

	/*
	 * Don't display load address for ELF; it's encoded in
	 * each section.
	 */
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
		if (OF_claim((void *)phdr.p_vaddr, phdr.p_memsz, 0) ==
		    (void *)-1)
			panic("cannot claim memory");
		maxp = maxp > (phdr.p_vaddr+ phdr.p_memsz) ?
			maxp : (phdr.p_vaddr+ phdr.p_memsz);
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
			bzero((void *)(phdr.p_vaddr + phdr.p_filesz),
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}
	*esymp = 0; /* in case it is not set later */

#if 0
	/*
	 * Copy the ELF and section headers.
	 */
	maxp = roundup(maxp, sizeof(long));
	if (flags & (LOAD_HDR|COUNT_HDR)) {
		if (OF_claim((void *)maxp, sizeof(Elf_Ehdr), 0) ==
		    (void *)-1)
			panic("cannot claim memory");
		elfp = maxp;
		maxp += sizeof(Elf_Ehdr);
	}

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf_Shdr);

		if (OF_claim((void *)maxp, sizeof(Elf_Ehdr), 0) ==
		    (void *)-1)
			panic("cannot claim memory");
		shpp = maxp;
		maxp += roundup(sz, sizeof(long)); 

		if (read(fd, shpp, sz) != sz) {
			WARN(("read section headers"));
			return 1;
		}
		/*
		 * Now load the symbol sections themselves.  Make sure the
		 * sections are aligned. Don't bother with string tables if
		 * there are no symbol sections.
		 */
		off = roundup((sizeof(Elf_Ehdr) + sz), sizeof(long));

		for (havesyms = i = 0; i < elf->e_shnum; i++)
			if (shpp[i].sh_type == Elf_sht_symtab)
				havesyms = 1;

		for (first = 1, i = 0; i < elf->e_shnum; i++) {
			if (shpp[i].sh_type == Elf_sht_symtab ||
			    shpp[i].sh_type == Elf_sht_strtab) {
				if (havesyms && (flags & LOAD_SYM)) {
					PROGRESS(("%s%ld", first ? " [" : "+",
					    (u_long)shpp[i].sh_size));
					if (lseek(fd, shpp[i].sh_offset,
					    SEEK_SET) == -1) {
						WARN(("lseek symbols"));
						FREE(shp, sz);
						return 1;
					}
					if (READ(fd, maxp, shpp[i].sh_size) !=
					    shpp[i].sh_size) {
						WARN(("read symbols"));
						return 1;
					}
				}
				maxp += roundup(shpp[i].sh_size,
				    sizeof(long));
				shpp[i].sh_offset = off;
				off += roundup(shpp[i].sh_size, sizeof(long));
				first = 0;
			}
		}
		if (flags & LOAD_SYM) {
			BCOPY(shp, shpp, sz);

			if (first == 0)
				PROGRESS(("]"));
		}
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		BCOPY(elf, elfp, sizeof(*elf));
	}
#endif
	printf(" \n");


	*entryp = elf->e_entry;
	return (0);
}
#endif /* POWERPC_BOOT_ELF */

int
main()
{
	int chosen;
	char bootline[512];		/* Should check size? */
	char *cp;
	int fd;
	
	printf("\n>> OpenBSD/powerpc Boot tst \n");

	/*
	 * Get the boot arguments from Openfirmware
	 */
	if ((chosen = OF_finddevice("/chosen")) == -1
	    || OF_getprop(chosen, "bootpath", bootdev, sizeof bootdev) < 0
	    || OF_getprop(chosen, "bootargs", bootline, sizeof bootline) < 0) {
		printf("Invalid Openfirmware environment\n");
		exit();
	}
	prom2boot(bootdev);
	parseargs(bootline, &boothowto);
	for (;;) {
		if (boothowto & RB_ASKNAME) {
			printf("Boot: ");
			gets(bootline);
			parseargs(bootline, &boothowto);
		}
		if ((fd = open(bootline, 0)) >= 0)
			break;
		if (errno)
			printf("open %s: %s\n", opened_name, strerror(errno));
		boothowto |= RB_ASKNAME;
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
	(void)loadfile(fd, bootline);

	_rtt();
	return 0;
}
