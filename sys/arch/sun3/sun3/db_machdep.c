/*	$NetBSD: db_machdep.c,v 1.7 1996/02/16 20:08:44 gwr Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-dependent functions used by ddb
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <ddb/db_command.h>

#include <machine/pte.h>


static char *pgt_names[] = {
	"MEM", "OBIO", "VMES", "VMEL" };

void pte_print(pte)
	int pte;
{
	int t;

	if (pte & PG_VALID) {
		db_printf(" V");
		if (pte & PG_WRITE)
			db_printf(" W");
		if (pte & PG_SYSTEM)
			db_printf(" S");
		if (pte & PG_NC)
			db_printf(" NC");
		if (pte & PG_REF)
			db_printf(" Ref");
		if (pte & PG_MOD)
			db_printf(" Mod");

		t = (pte >> PG_TYPE_SHIFT) & 3;
		db_printf(" %s", pgt_names[t]);
		db_printf(" PA=0x%x\n", PG_PA(pte));
	}
	else db_printf(" INVALID\n");
}

static void
db_pagemap(addr)
	db_expr_t	addr;
{
	int pte, sme;

	sme = get_segmap(addr);
	if (sme == 0xFF) pte = 0;
	else pte = get_pte(addr);

	db_printf("0x%08x [%02x] 0x%08x", addr, sme, pte);
	pte_print(pte);
	db_next = addr + NBPG;
}

/*
 * Machine-specific ddb commands for the sun3:
 *    abort:	Drop into monitor via abort (allows continue)
 *    halt: 	Exit to monitor as in halt(8)
 *    reboot:	Reboot the machine as in reboot(8)
 *    pgmap:	Given addr, Print addr, segmap, pagemap, pte
 */

extern void sun3_mon_abort();
extern void sun3_mon_halt();

void
db_mon_reboot()
{
	sun3_mon_reboot("");
}

struct db_command db_machine_cmds[] = {
	{ "abort",	sun3_mon_abort,	0,	0 },
	{ "halt",	sun3_mon_halt,	0,	0 },
	{ "reboot",	db_mon_reboot,	0,	0 },
	{ "pgmap",	db_pagemap, 	CS_SET_DOT, 0 },
	{ (char *)0, }
};

/*
 * This is called before ddb_init() to install the
 * machine-specific command table. (see machdep.c)
 */
void
db_machine_init()
{
	db_machine_commands_install(db_machine_cmds);
}
