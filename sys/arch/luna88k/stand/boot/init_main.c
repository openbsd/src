/*	$OpenBSD: init_main.c,v 1.3 2013/10/29 21:49:07 miod Exp $	*/
/*	$NetBSD: init_main.c,v 1.6 2013/03/05 15:34:53 tsutsui Exp $	*/

/*
 * Copyright (c) 1992 OMRON Corporation.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)init_main.c	8.2 (Berkeley) 8/15/93
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * OMRON Corporation.
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
 *	@(#)init_main.c	8.2 (Berkeley) 8/15/93
 */

#include <sys/param.h>
#include <machine/board.h>
#include <luna88k/stand/boot/samachdep.h>
#include <luna88k/stand/boot/status.h>
#include <lib/libsa/loadfile.h>
#include "dev_net.h"

static void get_fuse_rom_data(void);
static int get_plane_numbers(void);

int cpuspeed;	/* for DELAY() macro */
int machtype;
char default_file[64];

uint16_t dipswitch = 0;
int nplane;

/* for command parser */

#define BUFFSIZE 100
#define MAXARGS  30

char buffer[BUFFSIZE];

int   argc;
char *argv[MAXARGS];

#define BOOT_TIMEOUT 5
int boot_timeout = BOOT_TIMEOUT;

static const char prompt[] = "boot> ";

int debug;

/*
 * FUSE ROM and NVRAM data
 */
struct fuse_rom_byte {
	u_int32_t h;
	u_int32_t l;
};
#define	FUSE_ROM_SPACE		1024
#define FUSE_ROM_BYTES		(FUSE_ROM_SPACE / sizeof(struct fuse_rom_byte))
char fuse_rom_data[FUSE_ROM_BYTES];

int
main(void)
{
	int status = ST_NORMAL;
	const char *machstr;
	int unit, part;

	/* Determine the machine type from FUSE ROM data.  */
	get_fuse_rom_data();
	if (strncmp(fuse_rom_data, "MNAME=LUNA88K+", 14) == 0)
		machtype = LUNA_88K2;
	else
		machtype = LUNA_88K;

	/*
	 * Initialize the console before we print anything out.
	 */
	if (machtype == LUNA_88K) {
		machstr  = "luna88k";
		cpuspeed = MHZ_25;
	} else {
		machstr  = "luna88k-2";
		cpuspeed = MHZ_33;
	}

	nplane   = get_plane_numbers();

	cninit();

	printf("\nOpenBSD/%s boot 0.2\n\n", machstr);

	/*
	 * IO configuration
	 */

#ifdef SUPPORT_ETHERNET
	try_bootp = 1;
#endif

	unit = 0;	/* XXX should parse monitor's Boot-file constant */
	part = 0;
	snprintf(default_file, sizeof(default_file),
	    "sd(%d,%d)%s", unit, part, "bsd");

	/* auto-boot? (SW1) */
	if ((dipswitch & 0x8000) != 0) {
		char c;

		printf("Press return to boot now,"
		    " any other key for boot menu\n");
		printf("booting %s - starting in ", default_file);
		c = awaitkey("%d seconds. ", boot_timeout, 1);
		if (c == '\r' || c == '\n' || c == 0) {
			printf("auto-boot %s\n", default_file);
			bootunix(default_file);
		}
	}

	/*
	 * Main Loop
	 */

	printf("type \"help\" for help.\n");

	do {
		memset(buffer, 0, BUFFSIZE);
		if (getline(prompt, buffer) > 0) {
			argc = getargs(buffer, argv, sizeof(argv)/sizeof(char *));

			status = parse(argc, argv);
			if (status == ST_NOTFOUND)
				printf("unknown command \"%s\"\n", argv[0]);
		}
	} while (status != ST_EXIT);

	_rtt();
	/* NOTREACHED */
}

int
get_plane_numbers(void)
{
	int r = *((int *)0x1114);
	int n = 0;

	for (; r ; r >>= 1)
		if (r & 0x1)
			n++;

	return(n);
}

/* Get data from FUSE ROM */

void
get_fuse_rom_data(void)
{
	int i;
	struct fuse_rom_byte *p = (struct fuse_rom_byte *)FUSE_ROM_ADDR;

	for (i = 0; i < FUSE_ROM_BYTES; i++) {
		fuse_rom_data[i] =
		    (char)((((p->h) >> 24) & 0x000000f0) |
		           (((p->l) >> 28) & 0x0000000f));
		p++;                                                                            
	}
}

void
_rtt(void)
{
	*(volatile unsigned int *)RESET_CPU_ALL = 0;
	for (;;) ;
	/* NOTREACHED */
}
