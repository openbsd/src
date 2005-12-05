/*	$OpenBSD: db_interface.c,v 1.6 2005/12/05 21:31:23 miod Exp $	*/
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
extern boolean_t db_cmd_loop_done;

#ifdef MULTIPROCESSOR
struct mutex ddb_mp_mutex = MUTEX_INITIALIZER(IPL_HIGH);
volatile int ddb_state = DDB_STATE_NOT_RUNNING;
volatile cpuid_t ddb_active_cpu;
boolean_t	 db_switch_cpu;
long		 db_switch_to_cpu;
#endif

int	db_active;
db_regs_t ddb_regs;

void kdbprinttrap(int, int);
#ifdef MULTIPROCESSOR
void db_cpuinfo_cmd(db_expr_t, int, db_expr_t, char *);
void db_startproc_cmd(db_expr_t, int, db_expr_t, char *);
void db_stopproc_cmd(db_expr_t, int, db_expr_t, char *);
void db_ddbproc_cmd(db_expr_t, int, db_expr_t, char *);
int db_cpuid2apic(int);
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
	mtx_enter(&ddb_mp_mutex);
	if (ddb_state == DDB_STATE_EXITING)
		ddb_state = DDB_STATE_NOT_RUNNING;
	mtx_leave(&ddb_mp_mutex);
	while (db_enter_ddb()) {
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

	*regs = ddb_regs;

#ifdef MULTIPROCESSOR
		if (!db_switch_cpu)
			ddb_state = DDB_STATE_EXITING;
	}
#endif
	return (1);
}


#ifdef MULTIPROCESSOR
int
db_cpuid2apic(int id)
{
	int apic;

	for (apic = 0; apic < X86_MAXPROCS; apic++) {
		if (cpu_info[apic] != NULL &&
		    CPU_INFO_UNIT(cpu_info[apic]) == id)
			return (apic);
	}
	return (-1);
}

void
db_cpuinfo_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int i;

	for (i = 0; i < X86_MAXPROCS; i++) {
		if (cpu_info[i] != NULL) {
			db_printf("%c%4d: ", (i == cpu_number()) ? '*' : ' ',
			    CPU_INFO_UNIT(cpu_info[i]));
			switch(cpu_info[i]->ci_ddb_paused) {
			case CI_DDB_RUNNING:
				db_printf("running\n");
				break;
			case CI_DDB_SHOULDSTOP:
				db_printf("stopping\n");
				break;
			case CI_DDB_STOPPED:
				db_printf("stopped\n");
				break;
			case CI_DDB_ENTERDDB:
				db_printf("entering ddb\n");
				break;
			case CI_DDB_INDDB:
				db_printf("ddb\n");
				break;
			default:
				db_printf("? (%d)\n",
				    cpu_info[i]->ci_ddb_paused);
				break;
			}
		}
	}
}

void
db_startproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int apic;

	if (have_addr) {
		apic = db_cpuid2apic(addr);
		if (apic >= 0 && apic < X86_MAXPROCS &&
		    cpu_info[apic] != NULL && apic != cpu_number())
			db_startcpu(apic);
		else
			db_printf("Invalid cpu %d\n", (int)addr);
	} else {
		for (apic = 0; apic < X86_MAXPROCS; apic++) {
			if (cpu_info[apic] != NULL && apic != cpu_number()) {
				db_startcpu(apic);
			}
		}
	}
}

void
db_stopproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int apic;

	if (have_addr) {
		apic = db_cpuid2apic(addr);
		if (apic >= 0 && apic < X86_MAXPROCS &&
		    cpu_info[apic] != NULL && apic != cpu_number())
			db_stopcpu(apic);
		else
			db_printf("Invalid cpu %d\n", (int)addr);
	} else {
		for (apic = 0; apic < X86_MAXPROCS; apic++) {
			if (cpu_info[apic] != NULL && apic != cpu_number()) {
				db_stopcpu(apic);
			}
		}
	}
}

void
db_ddbproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int apic;

	if (have_addr) {
		apic = db_cpuid2apic(addr);
		if (apic >= 0 && apic < X86_MAXPROCS &&
		    cpu_info[apic] != NULL && apic != cpu_number()) {
			db_stopcpu(apic);
			db_switch_to_cpu = apic;
			db_switch_cpu = 1;
			db_cmd_loop_done = 1;
		} else {
			db_printf("Invalid cpu %d\n", (int)addr);
		}
	} else {
		db_printf("CPU not specified\n");
	}
}

int
db_enter_ddb()
{
	int i;

	mtx_enter(&ddb_mp_mutex);

	/* If we are first in, grab ddb and stop all other CPUs */
	if (ddb_state == DDB_STATE_NOT_RUNNING) {
		ddb_active_cpu = cpu_number();
		ddb_state = DDB_STATE_RUNNING;
		curcpu()->ci_ddb_paused = CI_DDB_INDDB;
		mtx_leave(&ddb_mp_mutex);
		for (i = 0; i < X86_MAXPROCS; i++) {
			if (cpu_info[i] != NULL && i != cpu_number() &&
			    cpu_info[i]->ci_ddb_paused != CI_DDB_STOPPED) {
				cpu_info[i]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
				x86_send_ipi(cpu_info[i], X86_IPI_DDB);
			}
		}
		return (1);
	}

	/* Leaving ddb completely.  Start all other CPUs and return 0 */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_EXITING) {
		for (i = 0; i < X86_MAXPROCS; i++) {
			if (cpu_info[i] != NULL) {
				cpu_info[i]->ci_ddb_paused = CI_DDB_RUNNING;
			}
		}
		mtx_leave(&ddb_mp_mutex);
		return (0);
	}

	/* We're switching to another CPU.  db_ddbproc_cmd() has made sure
	 * it is waiting for ddb, we just have to set ddb_active_cpu. */
	if (ddb_active_cpu == cpu_number() && db_switch_cpu) {
		curcpu()->ci_ddb_paused = CI_DDB_SHOULDSTOP;
		db_switch_cpu = 0;
		ddb_active_cpu = db_switch_to_cpu;
		cpu_info[db_switch_to_cpu]->ci_ddb_paused = CI_DDB_ENTERDDB;
	}

	/* Wait until we should enter ddb or resume */
	while (ddb_active_cpu != cpu_number() &&
	    curcpu()->ci_ddb_paused != CI_DDB_RUNNING) {
		if (curcpu()->ci_ddb_paused == CI_DDB_SHOULDSTOP)
			curcpu()->ci_ddb_paused = CI_DDB_STOPPED;
		mtx_leave(&ddb_mp_mutex);

		/* Busy wait without locking, we'll confirm with lock later */
		while (ddb_active_cpu != cpu_number() &&
		    curcpu()->ci_ddb_paused != CI_DDB_RUNNING)
			;	/* Do nothing */

		mtx_enter(&ddb_mp_mutex);
	}

	/* Either enter ddb or exit */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_RUNNING) {
		curcpu()->ci_ddb_paused = CI_DDB_INDDB;
		mtx_leave(&ddb_mp_mutex);
		return (1);
	} else {
		mtx_leave(&ddb_mp_mutex);
		return (0);
	}
}

void
db_startcpu(int cpu)
{
	if (cpu != cpu_number() && cpu_info[cpu] != NULL) {
		mtx_enter(&ddb_mp_mutex);
		cpu_info[cpu]->ci_ddb_paused = CI_DDB_RUNNING;
		mtx_leave(&ddb_mp_mutex);
	}
}

void
db_stopcpu(int cpu)
{
	mtx_enter(&ddb_mp_mutex);
	if (cpu != cpu_number() && cpu_info[cpu] != NULL &&
	    cpu_info[cpu]->ci_ddb_paused != CI_DDB_STOPPED) {
		cpu_info[cpu]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
		mtx_leave(&ddb_mp_mutex);
		x86_send_ipi(cpu_info[cpu], X86_IPI_DDB);
	} else {
		mtx_leave(&ddb_mp_mutex);
	}
}

void
x86_ipi_db(struct cpu_info *ci)
{
	Debugger();
}
#endif /* MULTIPROCESSOR */

struct db_command db_machine_command_table[] = {
#ifdef MULTIPROCESSOR
	{ "cpuinfo",	db_cpuinfo_cmd,		0,	0 },
	{ "startcpu",	db_startproc_cmd,	0,	0 },
	{ "stopcpu",	db_stopproc_cmd,	0,	0 },
	{ "ddbcpu",	db_ddbproc_cmd,		0,	0 },
#endif
	{ (char *)0, },
};

void
db_machine_init()
{
#ifdef MULTIPROCESSOR
	int i;
#endif

	db_machine_commands_install(db_machine_command_table);
#ifdef MULTIPROCESSOR
	for (i = 0; i < X86_MAXPROCS; i++) {
		if (cpu_info[i] != NULL)
			cpu_info[i]->ci_ddb_paused = CI_DDB_RUNNING;
	}
#endif
}

void
Debugger()
{
	breakpoint();
}
