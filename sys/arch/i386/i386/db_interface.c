/*	$OpenBSD: db_interface.c,v 1.12 2003/05/18 02:43:12 andreas Exp $	*/
/*	$NetBSD: db_interface.c,v 1.22 1996/05/03 19:42:00 christos Exp $	*/

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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_var.h>

extern label_t	*db_recover;
extern char *trap_type[];
extern int trap_types;

int	db_active = 0;

void kdbprinttrap(int, int);
void db_sysregs_cmd(db_expr_t, int, db_expr_t, char *);

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(type, code, regs)
	int type, code;
	db_regs_t *regs;
{
	int s;

	switch (type) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
	case T_NMI:	/* NMI */
	case -1:	/* keyboard interrupt */
		break;
	default:
		if (!db_panic)
			return (0);

		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	ddb_regs = *regs;
	if (KERNELMODE(regs->tf_cs, regs->tf_eflags)) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
		asm("movw %%ss,%w0" : "=r" (ddb_regs.tf_ss));
	}

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	regs->tf_es     = ddb_regs.tf_es & 0xffff;
	regs->tf_ds     = ddb_regs.tf_ds & 0xffff;
	regs->tf_edi    = ddb_regs.tf_edi;
	regs->tf_esi    = ddb_regs.tf_esi;
	regs->tf_ebp    = ddb_regs.tf_ebp;
	regs->tf_ebx    = ddb_regs.tf_ebx;
	regs->tf_edx    = ddb_regs.tf_edx;
	regs->tf_ecx    = ddb_regs.tf_ecx;
	regs->tf_eax    = ddb_regs.tf_eax;
	regs->tf_eip    = ddb_regs.tf_eip;
	regs->tf_cs     = ddb_regs.tf_cs & 0xffff;
	regs->tf_eflags = ddb_regs.tf_eflags;
	if (!KERNELMODE(regs->tf_cs, regs->tf_eflags)) {
		/* ring transit - saved esp and ss valid */
		regs->tf_esp    = ddb_regs.tf_esp;
		regs->tf_ss     = ddb_regs.tf_ss & 0xffff;
	}

	return (1);
}

void
db_sysregs_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	int64_t idtr, gdtr;
	uint32_t cr;
	uint16_t ldtr, tr;

	__asm__ __volatile__("sidt %0" : "=m" (idtr));
	db_printf("idtr:   0x%08x/%04x\n",
	    (unsigned int)(idtr >> 16), idtr & 0xffff);

	__asm__ __volatile__("sgdt %0" : "=m" (gdtr));
	db_printf("gdtr:   0x%08x/%04x\n",
	    (unsigned int)(gdtr >> 16), gdtr & 0xffff);

	__asm__ __volatile__("sldt %0" : "=g" (ldtr));
	db_printf("ldtr:   0x%04x\n", ldtr);

	__asm__ __volatile__("str %0" : "=g" (tr));
	db_printf("tr:     0x%04x\n", tr);

	__asm__ __volatile__("movl %%cr0,%0" : "=r" (cr));
	db_printf("cr0:    0x%08x\n", cr);

	__asm__ __volatile__("movl %%cr2,%0" : "=r" (cr));
	db_printf("cr2:    0x%08x\n", cr);

	__asm__ __volatile__("movl %%cr3,%0" : "=r" (cr));
	db_printf("cr3:    0x%08x\n", cr);

	__asm__ __volatile__("movl %%cr4,%0" : "=r" (cr));
	db_printf("cr4:    0x%08x\n", cr);
}

struct db_command db_machine_command_table[] = {
	{ "sysregs",	db_sysregs_cmd,		0,	0 },
	{ (char *)0, }
};

void
db_machine_init()
{

	db_machine_commands_install(db_machine_command_table);
}

void
Debugger()
{
	asm("int $3");
}
