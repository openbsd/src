/*	$NetBSD: db_machdep.c,v 1.5.2.1 1995/10/23 21:53:16 gwr Exp $	*/

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

#undef	DEBUG

#ifdef	DEBUG
int db_machdep_debug;
#endif

/*
 * Interface to the debugger for virtual memory read/write.
 *
 * To write in the text segment, we have to first make
 * the page writable, do the write, then restore the PTE.
 * For writes outside the text segment, and all reads,
 * just do the access -- if it causes a fault, the debugger
 * will recover with a longjmp to an appropriate place.
 *
 * ALERT!  If you want to access device registers with a
 * specific size, then the read/write functions have to
 * make sure to do the correct sized pointer access.
 */

/*
 * Read bytes from kernel address space for debugger.
 * This used to check for valid PTEs, but now that
 * traps in DDB work correctly, "Just Do It!"
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;
	register char	incr;

#ifdef	DEBUG
	if (db_machdep_debug)
		printf("db_read_bytes: addr=0x%x, size=%d\n", addr, size);
#endif

	if (size == 4) {
		*((int*)data) = *((int*)addr);
		return;
	}

	if (size == 2) {
		*((short*)data) = *((short*)addr);
		return;
	}

	src = (char *)addr;
	while (size > 0) {
		--size;
		*data++ = *src++;
	}
}

/*
 * Write one byte somewhere in kernel text.
 * It does not matter if this is slow.
 */
static void
db_write_text(dst, ch)
	char *dst;
	int ch;
{
	int		oldpte, tmppte;
	vm_offset_t pgva = sun3_trunc_page((long)dst);
	extern int cache_size;

	/* Flush read-only VAC entry so we'll see the new one. */
#ifdef	HAVECACHE
	if (cache_size)
		cache_flush_page(pgva);
#endif
	oldpte = get_pte(pgva);
	if ((oldpte & PG_VALID) == 0) {
		db_printf(" address 0x%x not a valid page\n", dst);
		return;
	}
	tmppte = oldpte | PG_WRITE | PG_NC;

	set_pte(pgva, tmppte);

	/* Now we can write in this page of kernel text... */
	*dst = (char) ch;

	/* Temporary PTE was non-cacheable; no flush needed. */
	set_pte(pgva, oldpte);
	ICIA();
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	int	size;
	char	*data;
{
	extern char	kernel_text[], etext[] ;
	register char	*dst = (char *)addr;

#ifdef	DEBUG
	if (db_machdep_debug)
		printf("db_write_bytes: addr=0x%x, size=%d ", addr, size);
#endif

	/* If any part is in kernel text, use db_write_text() */
	if ((dst < etext) && ((dst + size) > kernel_text)) {
		/* This is slow, but is only used for breakpoints. */
#ifdef	DEBUG
		if (db_machdep_debug)
			printf("(in text)\n");
#endif
		while (size > 0) {
			--size;
			db_write_text(dst, *data);
			dst++; data++;
		}
		return;
	}

#ifdef	DEBUG
		if (db_machdep_debug)
			printf("(in data)\n");
#endif

	if (size == 4) {
		*((int*)addr) = *((int*)data);
		return;
	}

	if (size == 2) {
		*((short*)addr) = *((short*)data);
		return;
	}

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
}

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
