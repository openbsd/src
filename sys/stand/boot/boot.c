/*	$OpenBSD: boot.c,v 1.2 1997/03/31 03:12:03 weingart Exp $	*/
/*
 * Copyright (c) 1997 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/stat.h>
#include <a.out.h>
#include <sys/disklabel.h>
#include <libsa.h>
#include "cmd.h"

/*
 * Boot program, loaded by boot block from remaing 7.5K of boot area.
 * Sifts through disklabel and attempts to load an program image of
 * a standalone program off the disk. If keyboard is hit during load,
 * or if an error is encounter, try alternate files.
 */

char *kernels[] = {
	"bsd", "bsd.gz",
	"obsd", "obsd.gz",
	"bsd.old", "bsd.old.gz",
	NULL
};

int	retry = 0;
extern	char version[];
extern dev_t bootdev;
extern int boothowto;
int	cnvmem, extmem, probemem;

void	boot ();
struct cmd_state cmd;

/*
 * Boot program... loads /boot out of filesystem indicated by arguements.
 * We assume an autoboot unless we detect a misconfiguration.
 */
void
boot()
{
	register char *bootfile = kernels[0];
	register int i;


	/* Get memory size */
	cnvmem = memsize(0);
	extmem = memsize(1);
	gateA20(1);
	probemem = memprobe();


	/* XXX init cmd here to cut on .data !!! */
	strncpy(cmd.bootdev,
#ifdef _TEST
		"/dev/rfd0a",
#else
		"fd(0,a)",
#endif
		sizeof(cmd.bootdev));
	cmd.image[0] = '\0';
	cmd.cwd[0] = '/';
	cmd.cwd[1] = '\0';
	cmd.addr = (void *)0x100000;
	cmd.timeout = 50000;

	printf("\n>> OpenBSD BOOT: %d/%d (%d) k [%s]\n",
		cnvmem, extmem, probemem, version);

	for (i = 0;;) {

		strncpy(cmd.image, bootfile, sizeof(cmd.image));

		do {
			printf("boot> ");
		} while(!getcmd(&cmd) && !execmd(&cmd));

		sprintf(cmd.path, "%s%s%s", cmd.bootdev, cmd.cwd, bootfile);
		printf("\nbooting %s: ", cmd.path);
		exec (cmd.path, cmd.addr, boothowto);

		if(kernels[++i] == NULL)
			bootfile = kernels[i=0];
		else
			bootfile = kernels[i];

		cmd.timeout += 20;

		printf(" failed(%d)\nwill try %s\n", errno, bootfile);
	}
}

#ifdef _TEST
int
main()
{
	boot();
	return 0;
}
#endif
