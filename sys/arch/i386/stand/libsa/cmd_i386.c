/*	$OpenBSD: cmd_i386.c,v 1.9 1997/10/09 22:23:00 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff, Tobias Weingartner
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
#include <machine/biosvar.h>
#include "debug.h"
#include "biosdev.h"
#include "libsa.h"
#include <cmd.h>

static int Xdiskinfo __P((void));
static int Xregs __P((void));

const struct cmd_table cmd_machine[] = {
	{ "diskinfo", CMDT_CMD, Xdiskinfo },
	{ "regs",     CMDT_CMD, Xregs },
	{ NULL, 0 }
};

static int
Xdiskinfo()
{
	u_int32_t di;
	int i;

	printf("Disk\tCylinders\tHeads\tSectors\n");
	for(i = 0x80; i < 0x84; i++){
		if ((di = biosdinfo(i)))
			printf("0x%x\t  %d   \t%d\t%d\n", i,
			       BIOSNTRACKS(di), BIOSNHEADS(di), BIOSNSECTS(di));
	}

	return 0;
}

static int
Xregs()
{
	DUMP_REGS;
	return 0;
}
