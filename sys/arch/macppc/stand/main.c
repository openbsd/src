/*	$OpenBSD: main.c,v 1.2 2007/06/14 03:32:53 drahn Exp $	*/
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
#include <stand/boot/cmd.h>


#include <machine/cpu.h>

#include <macppc/stand/ofdev.h>
#include <macppc/stand/openfirm.h>

char bootdev[128];
int boothowto;
int debug;


void
get_alt_bootdev(char *, size_t, char *, size_t);

static void
prom2boot(char *dev)
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
parseargs(char *str, int *howtop)
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
chain(void (*entry)(), char *args, void *ssym, void *esym)
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

/*
 * XXX This limits the maximum size of the (uncompressed) bsd.rd to a
 * little under 11MB.
 */
#define CLAIM_LIMIT	0x00c00000

char bootline[512];

extern const char *bootfile;
int
main()
{
	int chosen;
	u_long marks[MARK_MAX];
	int fd;


	/*
	 * Get the boot arguments from Openfirmware
	 */
	if ((chosen = OF_finddevice("/chosen")) == -1 ||
	    OF_getprop(chosen, "bootpath", bootdev, sizeof bootdev) < 0 ||
	    OF_getprop(chosen, "bootargs", bootline, sizeof bootline) < 0) {
		printf("Invalid Openfirmware environment\n");
		exit();
	}
	prom2boot(bootdev);
	get_alt_bootdev(bootdev, sizeof(bootdev), bootline, sizeof(bootline));
	if (bootline[0] != '\0')
		bootfile = bootline;

	OF_claim((void *)0x00100000, CLAIM_LIMIT, 0); /* XXX */
	boot(0);
	return 0;
}

void
get_alt_bootdev(char *dev, size_t devsz, char *line, size_t linesz)
{
	char *p;
	int len;
	/*
	 * if the kernel image specified contains a ':' it is
	 * [device]:[kernel], so seperate the two fields.
	 */
	p = strrchr(line, ':');
	if (p == NULL)
		return;
	/* user specified boot device for kernel */
	len = p - line + 1; /* str len plus nil */
	strlcpy(dev, line, len > devsz ? devsz : len);

	strlcpy(line, p+1, linesz); /* rest of string ater ':' */
}


void
devboot(dev_t dev, char *p)
{
	strlcpy(p, bootdev, BOOTDEVLEN);
}

run_loadfile(u_long *marks, int howto)
{
	char bootline[512];		/* Should check size? */
	u_int32_t entry;
	char *cp;
	void *ssym, *esym;

	strlcpy(bootline, opened_name, sizeof bootline);
	cp = bootline + strlen(bootline);
	*cp++ = ' ';
        *cp = '-';
        if (howto & RB_ASKNAME)
                *++cp = 'a';
        if (howto & RB_CONFIG)
                *++cp = 'c';
        if (howto & RB_SINGLE)
                *++cp = 's';
        if (howto & RB_KDB)
                *++cp = 'd';
        if (*cp == '-')
		*--cp = 0;
	else
		*++cp = 0;

	entry = marks[MARK_ENTRY];
	ssym = (void *)marks[MARK_SYM];
	esym = (void *)marks[MARK_END];
	{
		u_int32_t lastpage;
		lastpage = roundup(marks[MARK_END], NBPG);
		OF_release((void*)lastpage, CLAIM_LIMIT - lastpage);
	}

	chain((void *)entry, bootline, ssym, esym);

	_rtt();
	return 0;

}

int
cnspeed(dev_t dev, int sp)
{
	return CONSPEED;
}

char ttyname_buf[8];

char *
ttyname(int fd)
{
        snprintf(ttyname_buf, sizeof ttyname_buf, "ofc0");

}

dev_t
ttydev(char *name)
{
	return makedev(0,0);
}
