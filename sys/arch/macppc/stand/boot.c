/*	$OpenBSD: boot.c,v 1.8 2002/09/15 09:01:59 deraadt Exp $	*/
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

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>


#include <machine/cpu.h>

#include <macppc/stand/ofdev.h>
#include <macppc/stand/openfirm.h>

char bootdev[128];
char bootfile[128];
int boothowto;
int debug;

static void
prom2boot(dev)
	char *dev;
{
	char *cp, *lp = 0;
	
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
	if (str[0] == '\0')
		return;

	cp = str;
	while (*cp != 0) {
		/* check for - */
		if (*cp == '-')
			break;	/* start of options found */

		while (*cp != 0 && *cp != ' ')
			cp++;	/* character in the middle of the name, skip */

		while (*cp == ' ')
			*cp++ = 0;
	}
	if (!*cp)
		return;
	
	*cp++ = 0;
	while (*cp) {
		switch (*cp++) {
		case 'a':
			*howtop |= RB_ASKNAME;
			break;
		case 'c':
			*howtop |= RB_CONFIG;
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
chain(entry, args, ssym, esym)
	void (*entry)();
	char *args;
	void *ssym;
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
	bcopy(&ssym, args + l, sizeof(ssym));
	l += sizeof(ssym);
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
main()
{
	int chosen;
	char bootline[512];		/* Should check size? */
	char *cp;
	u_long marks[MARK_MAX];
	u_int32_t entry;
	void *ssym, *esym;
	int fd;
	
	printf("\n>> OpenBSD/macppc Boot\n");

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
			printf("Boot (or \"exit\"): ");
			gets(bootline);
			parseargs(bootline, &boothowto);
		}
		marks[MARK_START] = 0;
		if (loadfile(bootline, marks, LOAD_ALL) >= 0)
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
	if (boothowto & RB_CONFIG)
		*++cp = 'c';
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
	entry = marks[MARK_ENTRY];
	ssym = (void *)marks[MARK_SYM];
	esym = (void *)marks[MARK_END];

	chain ((void *)entry, bootline, ssym, esym);

	_rtt();
	return 0;
}
