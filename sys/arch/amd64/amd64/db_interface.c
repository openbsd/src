/*	$OpenBSD: db_interface.c,v 1.1 2004/01/28 01:39:38 mickey Exp $	*/
/*	$NetBSD: db_interface.c,v 1.1 2003/04/26 18:39:27 fvdl Exp $	*/

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

#include <machine/cpufunc.h>
#include <machine/db_machdep.h>
#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/atomic.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_var.h>

extern label_t *db_recover;
extern char *trap_type[];
extern int trap_types;

int	db_active;
db_regs_t ddb_regs;	/* register state */
db_regs_t *ddb_regp;

void db_mach_cpu (db_expr_t, int, db_expr_t, char *);

const struct db_command db_machine_command_table[] = {
#ifdef MULTIPROCESSOR
	{ "cpu",	db_mach_cpu,	0,	0 },
#endif
	{ (char *)0, },
};

void kdbprinttrap(int, int);
#ifdef MULTIPROCESSOR
extern void ddb_ipi(struct trapframe);
static void ddb_suspend(struct trapframe *);
int ddb_vec;
#endif

#define NOCPU	-1

int ddb_cpu = NOCPU;

typedef void (vector)(void);
extern vector Xintrddb;

void
db_machine_init()
{

#ifdef MULTIPROCESSOR
	ddb_vec = idt_vec_alloc(0xf0, 0xff);
	setgate((struct gate_descriptor *)&idt[ddb_vec], &Xintrddb, 1,
	    SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
}

#ifdef MULTIPROCESSOR

__cpu_simple_lock_t db_lock;

static int
db_suspend_others(void)
{
	int cpu_me = cpu_number();
	int win;

	if (ddb_vec == 0)
		return 1;

	__cpu_simple_lock(&db_lock);
	if (ddb_cpu == NOCPU)
		ddb_cpu = cpu_me;
	win = (ddb_cpu == cpu_me);
	__cpu_simple_unlock(&db_lock);
	if (win) {
		x86_ipi(ddb_vec, LAPIC_DEST_ALLEXCL, LAPIC_DLMODE_FIXED);
	}
	return win;
}

static void
db_resume_others(void)
{
	int i;

	__cpu_simple_lock(&db_lock);
	ddb_cpu = NOCPU;
	__cpu_simple_unlock(&db_lock);

	for (i=0; i < X86_MAXPROCS; i++) {
		struct cpu_info *ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_flags & CPUF_PAUSE)
			x86_atomic_clearbits_l(&ci->ci_flags, CPUF_PAUSE);
	}

}

#endif

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
	db_regs_t dbreg;

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

#ifdef MULTIPROCESSOR
	if (!db_suspend_others()) {
		ddb_suspend(regs);
	} else {
	curcpu()->ci_ddb_regs = &dbreg;
	ddb_regp = &dbreg;
#endif

	ddb_regs = *regs;

	ddb_regs.tf_cs &= 0xffff;
	ddb_regs.tf_ds &= 0xffff;
	ddb_regs.tf_es &= 0xffff;
	ddb_regs.tf_fs &= 0xffff;
	ddb_regs.tf_gs &= 0xffff;
	ddb_regs.tf_ss &= 0xffff;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);
#ifdef MULTIPROCESSOR  
	db_resume_others();
	}
#endif  
	ddb_regp = &dbreg;

	*regs = ddb_regs;

	return (1);
}

void
Debugger()
{
	breakpoint();
}

#ifdef MULTIPROCESSOR

/*
 * Called when we receive a debugger IPI (inter-processor interrupt).
 * As with trap() in trap.c, this function is called from an assembly
 * language IDT gate entry routine which prepares a suitable stack frame,
 * and restores this frame after the exception has been processed. Note
 * that the effect is as if the arguments were passed call by reference.
 */

void
ddb_ipi(struct trapframe frame)
{

	ddb_suspend(&frame);
}

static void
ddb_suspend(struct trapframe *frame)
{
	volatile struct cpu_info *ci = curcpu();
	db_regs_t regs;

	regs = *frame;

	ci->ci_ddb_regs = &regs;

	x86_atomic_setbits_l(&ci->ci_flags, CPUF_PAUSE);

	while (ci->ci_flags & CPUF_PAUSE)
		;
	ci->ci_ddb_regs = 0;
}


extern void cpu_debug_dump(void); /* XXX */

void
db_mach_cpu(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	struct cpu_info *ci;
	if (!have_addr) {
		cpu_debug_dump();
		return;
	}

	if ((addr < 0) || (addr >= X86_MAXPROCS)) {
		db_printf("%ld: cpu out of range\n", addr);
		return;
	}
	ci = cpu_info[addr];
	if (ci == NULL) {
		db_printf("cpu %ld not configured\n", addr);
		return;
	}
	if (ci != curcpu()) {
		if (!(ci->ci_flags & CPUF_PAUSE)) {
			db_printf("cpu %ld not paused\n", addr);
			return;
		}
	}
	if (ci->ci_ddb_regs == 0) {
		db_printf("cpu %ld has no saved regs\n", addr);
		return;
	}
	db_printf("using cpu %ld", addr);
	ddb_regp = ci->ci_ddb_regs;
}

#endif
