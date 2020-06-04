/*	$OpenBSD: db_interface.c,v 1.1 2020/05/27 22:22:04 gkoehler Exp $	*/
/*      $NetBSD: db_interface.c,v 1.12 2001/07/22 11:29:46 wiz Exp $ */

/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *      db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/cons.h>
#include <dev/ofw/fdt.h>

#include <machine/db_machdep.h>
#include <ddb/db_elf.h>
#include <ddb/db_extern.h>
#include <ddb/db_sym.h>

extern db_regs_t ddb_regs; /* db_trace.c */
extern db_symtab_t db_symtab; /* ddb/db_elf.c */
extern struct fdt_reg initrd_reg; /* machdep.c */

void
db_machine_init(void)
{
	db_expr_t val;
	uint64_t a, b;
	char *prop_start, *prop_end;
	void *node;

	/*
	 * petitboot loads the kernel without symbols.
	 * If an initrd exists, try to load symbols from there.
	 */
	node = fdt_find_node("/chosen");
	if (fdt_node_property(node, "linux,initrd-start", &prop_start) != 8 &&
	    fdt_node_property(node, "linux,initrd-end", &prop_end) != 8) {
		printf("[ no initrd ]\n");
		return;
	}

	a = bemtoh64((uint64_t *)prop_start);
	b = bemtoh64((uint64_t *)prop_end);
	initrd_reg.addr = trunc_page(a);
	initrd_reg.size = round_page(b) - initrd_reg.addr;
	db_elf_sym_init(b - a, (char *)a, (char *)b, "initrd");

	/* The kernel is PIE. Add an offset to most symbols. */
	if (db_symbol_by_name("db_machine_init", &val) != NULL) {
		Elf_Sym *symp, *symtab_start, *symtab_end;
		Elf_Addr offset;
		
		symtab_start = STAB_TO_SYMSTART(&db_symtab);
		symtab_end = STAB_TO_SYMEND(&db_symtab);
		
		offset = (Elf_Addr)db_machine_init - (Elf_Addr)val;
		for (symp = symtab_start; symp < symtab_end; symp++) {
			if (symp->st_shndx != SHN_ABS)
				symp->st_value += offset;
		}
	}
}

void
db_ktrap(int type, db_regs_t *frame)
{
	int s;

	ddb_regs = *frame;

	s = splhigh();
	db_active++;
	cnpollc(1);
	db_trap(type, 0);
	cnpollc(0);
	db_active--;
	splx(s);

	*frame = ddb_regs;
}
