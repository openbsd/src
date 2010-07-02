/*	$OpenBSD: cmd_i386.c,v 1.30 2010/07/02 00:36:52 weingart Exp $	*/

/*
 * Copyright (c) 1997-1999 Michael Shalayeff
 * Copyright (c) 1997 Tobias Weingartner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/biosvar.h>
#include <sys/disklabel.h>
#include "disk.h"
#include "debug.h"
#include "biosdev.h"
#include "libsa.h"
#include <cmd.h>


extern const char version[];

int Xboot(void);
int Xdiskinfo(void);
int Xmemory(void);
int Xregs(void);

/* From gidt.S */
int bootbuf(void *, int);

const struct cmd_table cmd_machine[] = {
	{ "boot",	CMDT_CMD, Xboot },
	{ "diskinfo",	CMDT_CMD, Xdiskinfo },
	{ "memory",	CMDT_CMD, Xmemory },
#ifdef DEBUG
	{ "regs",	CMDT_CMD, Xregs },
#endif
	{ NULL, 0 }
};

int
Xdiskinfo(void)
{
#ifndef _TEST
	dump_diskinfo();
#endif
	return 0;
}

#ifdef DEBUG
int
Xregs(void)
{
	DUMP_REGS;
	return 0;
}
#endif

int
Xboot(void)
{
#ifndef _TEST
	int dev, part, st;
	bios_diskinfo_t *bd = NULL;
	char buf[DEV_BSIZE], *dest = (void *)BOOTBIOS_ADDR;

	if (cmd.argc != 2) {
		printf("machine boot {fd,hd}<0123>[abcd]\n");
		printf("Where [0123] is the disk number,"
		    " and [abcd] is the partition.\n");
		return 0;
	}

	/* Check arg */
	if (cmd.argv[1][0] != 'f' && cmd.argv[1][0] != 'h')
		goto bad;
	if (cmd.argv[1][1] != 'd')
		goto bad;
	if (cmd.argv[1][2] < '0' || cmd.argv[1][2] > '3')
		goto bad;
	if ((cmd.argv[1][3] < 'a' || cmd.argv[1][3] > 'd') &&
	    cmd.argv[1][3] != '\0')
		goto bad;

	printf("Booting from %s ", cmd.argv[1]);

	dev = (cmd.argv[1][0] == 'h')?0x80:0;
	dev += (cmd.argv[1][2] - '0');
	part = (cmd.argv[1][3] - 'a');

	if (part > 0)
		printf("[%x,%d]\n", dev, part);
	else
		printf("[%x]\n", dev);

	/* Read boot sector from device */
	bd = bios_dklookup(dev);
	st = biosd_io(F_READ, bd, 0, 1, buf);
	if (st)
		goto bad;

	/* Frob boot flag in buffer from HD */
	if ((dev & 0x80) && (part > 0)){
		int i, j;

		for (i = 0, j = DOSPARTOFF; i < 4; i++, j += 16)
			if (part == i)
				buf[j] |= 0x80;
			else
				buf[j] &= ~0x80;
	}

	/* Load %dl, ljmp */
	bcopy(buf, dest, DEV_BSIZE);
	bootbuf(dest, dev);

bad:
	printf("Invalid device!\n");
#endif
	return 0;
}

int
Xmemory(void)
{
	if (cmd.argc >= 2) {
		int i;
		/* parse the memory specs */

		for (i = 1; i < cmd.argc; i++) {
			char *p;
			long long addr, size;

			p = cmd.argv[i];

			size = strtoll(p + 1, &p, 0);
			/* Size the size */
			switch(*p) {
				case 'G':
					size *= 1024;
				case 'M':
					size *= 1024;
				case 'K':
					size *= 1024;
					p++;
			}

			/* Handle (possibly non-existant) address part */
			switch(*p) {
				case '@':
					addr = strtoll(p + 1, NULL, 0);
					break;

				/* Adjust address if we don't need it */
				default:
					if (cmd.argv[i][0] == '=')
						addr = -1;
					else
						addr = 0;
			}

			if (addr == 0 || size == 0) {
				printf("bad language\n");
				return 0;
			} else {
				switch (cmd.argv[i][0]) {
				case '-':
					mem_delete(addr, addr + size);
					break;
				case '+':
					mem_add(addr, addr + size);
					break;
				case '=':
					mem_limit(size);
					break;
				default :
					printf("bad OP\n");
					return 0;
				}
			}
		}
	}

	dump_biosmem(NULL);

	return 0;
}
