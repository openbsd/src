/*	$NetBSD: boot.c,v 1.2 1996/10/07 21:43:02 cgd Exp $	*/

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
/*
 * First try for the boot code
 *
 * Input syntax is:
 *	[promdev[{:|,}partition]]/[filename] [flags]
 */
#include <stand.h>

#include <sys/exec.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>

#include <sys/exec_elf.h>
#include <machine/cpu.h>

#include "ofdev.h"
#include "openfirm.h"

char bootdev[128];
char bootfile[128];
int boothowto;
int debug;

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
chain(entry, args)
	void (*entry)();
	char *args;
{
	extern end;

	freeall();
	OF_chain((void *)RELOC, (void *)&end - (void *)RELOC,
		 entry, args, strlen(args) + 1);
	panic("chain");
}

static void
loadfile(fd, addr, args)
	int fd;
	void *addr;
	char *args;
{
	struct exec hdr;
	int n;
	int *paddr;
	void *exec_addr;
	
	if (read(fd, &hdr, sizeof hdr) != sizeof(hdr))
		panic("short a.out file");
	if (
#if defined (EXEC_AOUT)
		(N_BADMAG(hdr) || N_GETMID(hdr) != MID_POWERPC)
#endif
#if defined(EXEC_AOUT) && defined (EXEC_ELF)
		&&
#endif
#if defined (EXEC_ELF)
		(((u_int *)&hdr)[0] != 0x7f454c46) /* 0x7f E L F */
#endif
		)
	{
		panic("invalid executable format");
	}
#ifdef EXEC_AOUT
	if (N_GETMID(hdr) == MID_POWERPC) {
		n = hdr.a_text + hdr.a_data + hdr.a_bss + hdr.a_syms
			+ sizeof(int);
		if ((paddr = OF_claim(addr, n, 0)) == (int *)-1)
			panic("cannot claim memory");
		lseek(fd, N_TXTOFF(hdr), SEEK_SET);
		if (read(fd, paddr, hdr.a_text + hdr.a_data) !=
			hdr.a_text + hdr.a_data)
		{
			panic("short a.out file");
		}
		bzero((void *)paddr + hdr.a_text + hdr.a_data, hdr.a_bss);
		paddr = (int *)((void *)paddr + hdr.a_text + hdr.a_data
			+ hdr.a_bss);
		*paddr++ = hdr.a_syms;
		if (hdr.a_syms) {
			if (read(fd, paddr, hdr.a_syms) != hdr.a_syms)
				panic("short a.out file");
			paddr = (int *)((void *)paddr + hdr.a_syms);
			if (read(fd, &n, sizeof(int)) != sizeof(int))
				panic("short a.out file");
			if (OF_claim((void *)paddr, n + sizeof(int), 0) ==
				(void *)-1)
			{
				panic("cannot claim memory");
			}
			*paddr++ = n;
			if (read(fd, paddr, n - sizeof(int)) != n - sizeof(int))
			{
				panic("short a.out file");
			}
		}
		exec_addr = hdr.a_text;
	}
#endif
#ifdef EXEC_ELF
	if (((u_int *)&hdr)[0] == 0x7f454c46) /* 0x7f E L F */ {
		Elf32_Ehdr ehdr;
		Elf32_Phdr phdr;
		int loc_phdr;
		int i;

		lseek (fd, 0, SEEK_SET);
		read (fd, &ehdr, sizeof (ehdr));
		loc_phdr = ehdr.e_phoff;
		for (i = 1; i <=ehdr.e_phnum; i++) {
			lseek(fd, loc_phdr, SEEK_SET);
			read (fd, &phdr, sizeof(phdr));
			loc_phdr += sizeof(phdr);
			
			if (phdr.p_type == PT_LOAD) {
				printf("phseg %d addr %x  size %x\n", i,
					phdr.p_vaddr, phdr.p_memsz);
				lseek(fd, phdr.p_offset, SEEK_SET);
				paddr = (int *)phdr.p_vaddr;
				n = phdr.p_memsz;
				if (OF_claim((void *)paddr, n , 0) == (int *)-1)
					panic("cannot claim memory");
				read (fd, paddr, phdr.p_filesz);
				if (phdr.p_filesz != phdr.p_memsz) {
					bzero (addr + n,
						phdr.p_memsz - phdr.p_filesz);
				}
			}
		}
		exec_addr = ehdr.e_entry;
	}
#endif
	close(fd);
	syncicache(addr, exec_addr);
	if (floppyboot) {
		printf("Please insert root disk and press ENTER ");
		getchar();
		printf("\n");
	}
	chain((void *)exec_addr, args);
}

char bootline[512];
void
main()
{
	int chosen;
	char *cp;
	int fd;
	
	printf(">> NetBSD BOOT\n");

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
	printf("Booting %s @ 0x%x\n", opened_name, LOADADDR);
#ifdef	__notyet__
	OF_setprop(chosen, "bootpath", opened_name, strlen(opened_name) + 1);
	cp = bootline;
#else
	strncpy(bootline, opened_name, 512);
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
	loadfile(fd, LOADADDR, bootline);

	_rtt();
}
