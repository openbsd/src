/*	$OpenBSD: boot.c,v 1.18 2010/08/25 12:53:38 jsing Exp $	*/
/*	$NetBSD: boot.c,v 1.3 2001/05/31 08:55:19 mrg Exp $	*/
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

#define ELFSIZE 64

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

#ifdef BOOT_DEBUG
uint32_t	boot_debug = 0
		    /* | BOOT_D_OFDEV */
		    /* | BOOT_D_OFNET */
		;
#endif

#define	MEG	(1024*1024)

/*
 * Boot device is derived from ROM provided information, or if there is none,
 * this list is used in sequence, to find a kernel.
 */
char *kernels[] = {
	"bsd",
	NULL
};

char bootdev[128];
char bootfile[128];
int boothowto;
int debug;

int	elf64_exec(int, Elf64_Ehdr *, u_int64_t *, void **, void **);

#if 0
static void
prom2boot(char *dev)
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

/*
 *	parse:
 *		[kernel-name] [-options]
 *	leave kernel-name in passed-in string
 *	put options into *howtop
 *	return -1 iff syntax error (no - before options)
 */

static int
parseargs(char *str, int *howtop)
{
	char *cp;
	int i;

	*howtop = 0;
	cp = str;
	while (*cp == ' ')
		++cp;
	if (*cp != '-') {
		while (*cp && *cp != ' ')
			*str++ = *cp++;
		while (*cp == ' ')
			++cp;
	}
	*str = 0;
	switch(*cp) {
	default:
		printf ("boot options string <%s> must start with -\n", cp);
		return -1;
	case 0:
		return 0;
	case '-':
		break;
	}

	++cp;
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
		}
	}
	return 0;
}


static void
chain(u_int64_t pentry, char *args, void *ssym, void *esym)
{
	extern char end[];
	void (*entry)();
	int l, machine_tag;
	long newargs[3];

	entry = (void *)(long)pentry;

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
loadfile(int fd, char *args)
{
	union {
		Elf64_Ehdr elf64;
	} hdr;
	int rval;
	u_int64_t entry = 0;
	void *ssym;
	void *esym;

	ssym = NULL;
	esym = NULL;

	/* Load the header. */
#ifdef DEBUG
	printf("loadfile: reading header\n");
#endif
	if ((rval = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		if (rval == -1)
			printf("read header: %s\n", strerror(errno));
		else
			printf("read header: short read (only %d of %d)\n",
			    rval, sizeof(hdr));
		rval = 1;
		goto err;
	}

	/* Determine file type, load kernel. */
	if (bcmp(hdr.elf64.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf64.e_ident[EI_CLASS] == ELFCLASS64) {
		printf("Booting %s\n", opened_name);
		rval = elf64_exec(fd, &hdr.elf64, &entry, &ssym, &esym);
	} else {
		rval = 1;
		printf("unknown executable format\n");
	}

	if (rval)
		goto err;

	printf(" start=0x%lx\n", (unsigned long)entry);

	close(fd);

	chain(entry, args, ssym, esym);
	/* NOTREACHED */

 err:
	close(fd);
	return (rval);
}

int
main()
{
	extern char version[];
	int chosen;
	char bootline[512];		/* Should check size? */
	char *cp;
	int i, fd;
	char **bootlp;
	char *just_bootline[2];
	
	printf(">> OpenBSD BOOT %s\n", version);

	/*
	 * Get the boot arguments from Openfirmware
	 */
	if ((chosen = OF_finddevice("/chosen")) == -1 ||
	    OF_getprop(chosen, "bootpath", bootdev, sizeof bootdev) < 0 ||
	    OF_getprop(chosen, "bootargs", bootline, sizeof bootline) < 0) {
		printf("Invalid Openfirmware environment\n");
		exit();
	}

	/*
	 * case 1:	boot net -a
	 *			-> gets loop
	 * case 2:	boot net kernel [options]
	 *			-> boot kernel, gets loop
	 * case 3:	boot net [options]
	 *			-> iterate boot list, gets loop
	 */

	bootlp = kernels;
	if (parseargs(bootline, &boothowto) == -1 ||
	    (boothowto & RB_ASKNAME)) {
		bootlp = 0;
	} else if (*bootline) {
		just_bootline[0] = bootline;
		just_bootline[1] = 0;
		bootlp = just_bootline;
	}
	for (;;) {
		if (bootlp) {
			cp = *bootlp++;
			if (!cp) {
				printf("\n");
				bootlp = 0;
				kernels[0] = 0;	/* no more iteration */
			} else if (cp != bootline) {
				printf("Trying %s...\n", cp);
				strlcpy(bootline, cp, sizeof bootline);
			}
		}
		if (!bootlp) {
			printf("Boot: ");
			gets(bootline);
			if (parseargs(bootline, &boothowto) == -1)
				continue;
			if (!*bootline) {
				bootlp = kernels;
				continue;
			}
			if (strcmp(bootline, "exit") == 0 ||
			    strcmp(bootline, "halt") == 0) {
				_rtt();
			}
		}
		if ((fd = open(bootline, 0)) < 0) {
			printf("open %s: %s\n", opened_name, strerror(errno));
			continue;
		}
#ifdef	__notyet__
		OF_setprop(chosen, "bootpath", opened_name, strlen(opened_name) + 1);
		cp = bootline;
#else
		strlcpy(bootline, opened_name, sizeof bootline);
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
			*--cp = 0;
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
	}
	return 0;
}
