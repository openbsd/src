/*	$OpenBSD: db_interface.c,v 1.3 2017/04/30 16:45:45 mpi Exp $	*/
/*	$NetBSD: db_interface.c,v 1.34 2003/10/26 23:11:15 chris Exp $	*/

/*
 * Copyright (c) 1996 Scott K. Stevens
 *
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
 *	From: db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>	/* just for boothowto */
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <arm64/db_machdep.h>
#include <machine/pmap.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <dev/cons.h>

//static long nil;

int db_access_und_sp (struct db_variable *, db_expr_t *, int);
int db_access_abt_sp (struct db_variable *, db_expr_t *, int);
int db_access_irq_sp (struct db_variable *, db_expr_t *, int);
u_int db_fetch_reg (int, db_regs_t *);

int db_trapper (vaddr_t, u_int, trapframe_t *, int);

struct db_variable db_regs[] = {
	{ "x0", (long *)&DDB_REGS->tf_x[0], FCN_NULL, },
	{ "x1", (long *)&DDB_REGS->tf_x[1], FCN_NULL, },
	{ "x2", (long *)&DDB_REGS->tf_x[2], FCN_NULL, },
	{ "x3", (long *)&DDB_REGS->tf_x[3], FCN_NULL, },
	{ "x4", (long *)&DDB_REGS->tf_x[4], FCN_NULL, },
	{ "x5", (long *)&DDB_REGS->tf_x[5], FCN_NULL, },
	{ "x6", (long *)&DDB_REGS->tf_x[6], FCN_NULL, },
	{ "x7", (long *)&DDB_REGS->tf_x[7], FCN_NULL, },
	{ "x8", (long *)&DDB_REGS->tf_x[8], FCN_NULL, },
	{ "x9", (long *)&DDB_REGS->tf_x[9], FCN_NULL, },
	{ "x10", (long *)&DDB_REGS->tf_x[10], FCN_NULL, },
	{ "x11", (long *)&DDB_REGS->tf_x[11], FCN_NULL, },
	{ "x12", (long *)&DDB_REGS->tf_x[12], FCN_NULL, },
	{ "x13", (long *)&DDB_REGS->tf_x[13], FCN_NULL, },
	{ "x14", (long *)&DDB_REGS->tf_x[14], FCN_NULL, },
	{ "x15", (long *)&DDB_REGS->tf_x[15], FCN_NULL, },
	{ "x16", (long *)&DDB_REGS->tf_x[16], FCN_NULL, },
	{ "x17", (long *)&DDB_REGS->tf_x[17], FCN_NULL, },
	{ "x18", (long *)&DDB_REGS->tf_x[18], FCN_NULL, },
	{ "x19", (long *)&DDB_REGS->tf_x[19], FCN_NULL, },
	{ "x20", (long *)&DDB_REGS->tf_x[20], FCN_NULL, },
	{ "x21", (long *)&DDB_REGS->tf_x[21], FCN_NULL, },
	{ "x22", (long *)&DDB_REGS->tf_x[22], FCN_NULL, },
	{ "x23", (long *)&DDB_REGS->tf_x[23], FCN_NULL, },
	{ "x24", (long *)&DDB_REGS->tf_x[24], FCN_NULL, },
	{ "x25", (long *)&DDB_REGS->tf_x[25], FCN_NULL, },
	{ "x26", (long *)&DDB_REGS->tf_x[26], FCN_NULL, },
	{ "x27", (long *)&DDB_REGS->tf_x[27], FCN_NULL, },
	{ "x28", (long *)&DDB_REGS->tf_x[28], FCN_NULL, },
	{ "x29", (long *)&DDB_REGS->tf_x[29], FCN_NULL, },
	{ "x30", (long *)&DDB_REGS->tf_x[30], FCN_NULL, },
	{ "sp", (long *)&DDB_REGS->tf_sp, FCN_NULL, },
	{ "spsr", (long *)&DDB_REGS->tf_spsr, FCN_NULL, },
	{ "elr", (long *)&DDB_REGS->tf_elr, FCN_NULL, },
	{ "lr", (long *)&DDB_REGS->tf_lr, FCN_NULL, },
};

extern label_t       *db_recover;

struct db_variable * db_eregs = db_regs + nitems(db_regs);

int	db_active = 0;

#ifdef DDB
/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(int type, db_regs_t *regs)
{
	int s;

	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
	case -1:		/* keyboard interrupt */
		break;
	default:
		if (db_recover != 0) {
			/* This will longjmp back into db_command_loop() */
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0/*code*/);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	*regs = ddb_regs;

	return (1);
}
#endif


#define INKERNEL(va)	(((vaddr_t)(va)) & (1ULL << 63))

static int db_validate_address(vaddr_t addr);

static int
db_validate_address(vaddr_t addr)
{
	struct proc *p = curproc;
	struct pmap *pmap;

	if (!p || !p->p_vmspace || !p->p_vmspace->vm_map.pmap ||
	    INKERNEL(addr))
		pmap = pmap_kernel();
	else
		pmap = p->p_vmspace->vm_map.pmap;

	return (pmap_extract(pmap, addr, NULL) == FALSE);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vaddr_t	addr;
	size_t	size;
	char	*data;
{
	char	*src = (char *)addr;

	if (db_validate_address((vaddr_t)src)) {
		db_printf("address %p is invalid\n", src);
		return;
	}

	if (size == 8 && (addr & 7) == 0 && ((vaddr_t)data & 7) == 0) {
		*((uint64_t*)data) = *((uint64_t*)src);
		return;
	}

	if (size == 4 && (addr & 3) == 0 && ((vaddr_t)data & 3) == 0) {
		*((int*)data) = *((int*)src);
		return;
	}

	if (size == 2 && (addr & 1) == 0 && ((vaddr_t)data & 1) == 0) {
		*((short*)data) = *((short*)src);
		return;
	}

	while (size-- > 0) {
		if (db_validate_address((vaddr_t)src)) {
			db_printf("address %p is invalid\n", src);
			return;
		}
		*data++ = *src++;
	}
}

#if 0
static void
db_write_text(vaddr_t addr, size_t size, char *data)
{
	// Implement
	return ;
}
#endif

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(vaddr_t addr, size_t size, char *data)
{
#if 0
	extern char etext[];
	extern char kernel_text[];
	char *dst;
	size_t loop;

	/* If any part is in kernel text, use db_write_text() */
	if (addr >= (vaddr_t) kernel_text && addr < (vaddr_t) etext) {
		db_write_text(addr, size, data);
		return;
	}

	dst = (char *)addr;
	loop = size;
	while (loop-- > 0) {
		if (db_validate_address((vaddr_t)dst)) {
			db_printf("address %p is invalid\n", dst);
			return;
		}
		*dst++ = *data++;
	}
	/* make sure the caches and memory are in sync */
	cpu_icache_sync_range(addr, size);

	/* In case the current page tables have been modified ... */
	cpu_tlb_flushID();
	cpu_cpwait();
#endif
}

void
db_enter(void)
{
	asm("brk 0");
}

struct db_command db_machine_command_table[] = {
	{ NULL, 	NULL, 			0, NULL }
};

int
db_trapper(vaddr_t addr, u_int inst, trapframe_t *frame, int fault_code)
{

	if (fault_code == EXCP_BRK) {
		kdb_trap(T_BREAKPOINT, frame);
		frame->tf_elr += INSN_SIZE;
	} else
		kdb_trap(-1, frame);
	return (0);
}

extern vaddr_t esym;
extern vaddr_t end;

void
db_machine_init(void)
{
	/*
	 * We get called before malloc() is available, so supply a static
	 * struct undefined_handler.
	 */
	//db_uh.uh_handler = db_trapper;
	//install_coproc_handler_static(0, &db_uh);

	db_machine_commands_install(db_machine_command_table);
}

db_addr_t
db_branch_taken(u_int insn, db_addr_t pc, db_regs_t *db_regs)
{
	// implment
	return pc + 4;
}
