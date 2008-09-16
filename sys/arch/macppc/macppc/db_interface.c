/*	$OpenBSD: db_interface.c,v 1.9 2008/09/16 04:20:42 drahn Exp $	*/
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
#include <sys/proc.h>
#include <sys/systm.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>

#ifdef MULTIPROCESSOR
struct mutex ddb_mp_mutex = MUTEX_INITIALIZER(IPL_HIGH);
volatile int ddb_state = DDB_STATE_NOT_RUNNING;
volatile cpuid_t ddb_active_cpu;
boolean_t        db_switch_cpu;
long             db_switch_to_cpu;
#endif

int db_active;
extern db_regs_t ddb_regs;

#ifdef MULTIPROCESSOR
void db_cpuinfo_cmd(db_expr_t, int, db_expr_t, char *);
void db_startproc_cmd(db_expr_t, int, db_expr_t, char *);
void db_stopproc_cmd(db_expr_t, int, db_expr_t, char *);
void db_ddbproc_cmd(db_expr_t, int, db_expr_t, char *);
#endif

int db_active = 0;

int ddb_trap_glue(struct trapframe *frame); /* called from locore */

void
Debugger()
{
	ddb_trap();
}

int
ddb_trap_glue(struct trapframe *frame)
{
	int s;

	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC
		|| (frame->exc == EXC_PGM && (frame->srr1 & 0x20000))
		|| frame->exc == EXC_BPT)) {

#ifdef MULTIPROCESSOR
		mtx_enter(&ddb_mp_mutex);
		if (ddb_state == DDB_STATE_EXITING)
			ddb_state = DDB_STATE_NOT_RUNNING;
		mtx_leave(&ddb_mp_mutex);

		while (db_enter_ddb()) {
#endif
			bcopy(frame->fixreg, DDB_REGS->tf.fixreg,
				32 * sizeof(u_int32_t));
			DDB_REGS->tf.srr0 = frame->srr0;
			DDB_REGS->tf.srr1 = frame->srr1;

			s = splhigh();
			db_active++;
			cnpollc(TRUE);
			db_trap(T_BREAKPOINT, 0);
			cnpollc(FALSE);
			db_active--;
			splx(s);

			bcopy(DDB_REGS->tf.fixreg, frame->fixreg,
				32 * sizeof(u_int32_t));
#ifdef MULTIPROCESSOR
			if (!db_switch_cpu)
				ddb_state = DDB_STATE_EXITING;
		}
#endif
		return 1;
	}
	return 0;
}

int
db_enter_ddb(void)
{
#ifdef MULTIPROCESSOR
	int i;
	struct cpu_info *ci = curcpu();

	mtx_enter(&ddb_mp_mutex);

#if 0
	printf("db_enter_ddb %d: state %x pause %x\n", ci->ci_cpuid,
	    ddb_state, ci->ci_ddb_paused);
#endif
	/* If we are first in, grab ddb and stop all other CPUs */
	if (ddb_state == DDB_STATE_NOT_RUNNING) {
		ddb_active_cpu = cpu_number();
		ddb_state = DDB_STATE_RUNNING;
		ci->ci_ddb_paused = CI_DDB_INDDB;
		for (i = 0; i < ncpus; i++) {
			if (i != cpu_number() &&
			    cpu_info[i].ci_ddb_paused != CI_DDB_STOPPED) {
				cpu_info[i].ci_ddb_paused = CI_DDB_SHOULDSTOP;
				ppc_send_ipi(&cpu_info[i], PPC_IPI_DDB);
			}
		}
		mtx_leave(&ddb_mp_mutex);
		return (1);
	}

	/* Leaving ddb completely.  Start all other CPUs and return 0 */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_EXITING) {
		for (i = 0; i < ncpus; i++) {
			cpu_info[i].ci_ddb_paused = CI_DDB_RUNNING;
		}
		mtx_leave(&ddb_mp_mutex);
		return (0);
	}

	/* We are switching to another CPU. ddb_ddbproc_cmd() has made sure
	 * it is waiting for ddb, we just have to set ddb_active_cpu. */
	if (ddb_active_cpu == cpu_number() && db_switch_cpu) {
		ci->ci_ddb_paused = CI_DDB_SHOULDSTOP;
		db_switch_cpu = 0;
		ddb_active_cpu = db_switch_to_cpu;
		cpu_info[db_switch_to_cpu].ci_ddb_paused = CI_DDB_ENTERDDB;
	}

	/* Wait until we should enter ddb or resume */
	while (ddb_active_cpu != cpu_number() &&
	    ci->ci_ddb_paused != CI_DDB_RUNNING) {
		if (ci->ci_ddb_paused == CI_DDB_SHOULDSTOP)
			ci->ci_ddb_paused = CI_DDB_STOPPED;
		mtx_leave(&ddb_mp_mutex);

		/* Busy wait without locking, we will confirm with lock later */
		while (ddb_active_cpu != cpu_number() &&
		    ci->ci_ddb_paused != CI_DDB_RUNNING)
			;	/* Do nothing */

		mtx_enter(&ddb_mp_mutex);
	}

	/* Either enter ddb or exit */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_RUNNING) {
		ci->ci_ddb_paused = CI_DDB_INDDB;
		mtx_leave(&ddb_mp_mutex);
		return (1);
	} else {
		mtx_leave(&ddb_mp_mutex);
		return (0);
	}
#else
	return (1);
#endif
}

#ifdef MULTIPROCESSOR
void
ppc_ipi_db(struct cpu_info *ci)
{
	Debugger();
}

void
db_cpuinfo_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int i;

	for (i = 0; i < ncpus; i++) {
		db_printf("%c%4d: ", (i == cpu_number()) ? '*' : ' ',
		    cpu_info[i].ci_cpuid);
		switch(cpu_info[i].ci_ddb_paused) {
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
			    cpu_info[i].ci_ddb_paused);
			break;
		}
	}
}
#endif

struct db_command db_machine_command_table[] = {
#ifdef MULTIPROCESSOR
	{ "cpuinfo",    db_cpuinfo_cmd,         0,      NULL },
	{ "startcpu",   db_startproc_cmd,       0,      NULL },
	{ "stopcpu",    db_stopproc_cmd,        0,      NULL },
	{ "ddbcpu",     db_ddbproc_cmd,         0,      NULL },
#endif   
	{ (char *)NULL }
};

void
db_machine_init(void)   
{
#ifdef MULTIPROCESSOR
	int i;
#endif
		    
	db_machine_commands_install(db_machine_command_table);
#ifdef MULTIPROCESSOR
	for (i = 0; i < ncpus; i++) {
		cpu_info[i].ci_ddb_paused = CI_DDB_RUNNING;
	}
#endif
}

#ifdef MULTIPROCESSOR
void
db_ddbproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int cpu_n;

	if (have_addr) {
		cpu_n = addr;
		if (cpu_n >= 0 && cpu_n < ncpus &&
		    cpu_n != cpu_number()) {
			db_switch_to_cpu = cpu_n;
			db_switch_cpu = 1;
/*			db_cmd_loop_done = 1; */
		} else {
			db_printf("Invalid cpu %d\n", (int)addr);
		}
	} else {
		db_printf("CPU not specified\n");
	}
}

void
db_startproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int cpu_n;

	if (have_addr) {
		cpu_n = addr;
		if (cpu_n >= 0 && cpu_n < ncpus &&
		    cpu_n != cpu_number())
			db_startcpu(cpu_n);
		else
			db_printf("Invalid cpu %d\n", (int)addr);
	} else {
		for (cpu_n = 0; cpu_n < ncpus; cpu_n++) {
			if (cpu_n != cpu_number()) {
				db_startcpu(cpu_n);
			}
		}
	}
}

void
db_stopproc_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	int cpu_n;
	
	if (have_addr) {
		cpu_n = addr;
		if (cpu_n >= 0 && cpu_n < ncpus &&
		    cpu_n != cpu_number())
			db_stopcpu(cpu_n);
		else
			db_printf("Invalid cpu %d\n", (int)addr);
	} else {
		for (cpu_n = 0; cpu_n < ncpus; cpu_n++) {
			if (cpu_n != cpu_number()) {
				db_stopcpu(cpu_n);
			}
		}
	}
}

void
db_startcpu(int cpu)
{
	if (cpu != cpu_number() && cpu < ncpus) {
		mtx_enter(&ddb_mp_mutex);
		cpu_info[cpu].ci_ddb_paused = CI_DDB_RUNNING;
		mtx_leave(&ddb_mp_mutex);
	}
}   

void
db_stopcpu(int cpu)
{
	mtx_enter(&ddb_mp_mutex);
	if (cpu != cpu_number() && cpu < ncpus &&
	    cpu_info[cpu].ci_ddb_paused != CI_DDB_STOPPED) {
		cpu_info[cpu].ci_ddb_paused = CI_DDB_SHOULDSTOP;  
		mtx_leave(&ddb_mp_mutex);
		ppc_send_ipi(&cpu_info[cpu], PPC_IPI_DDB);
	} else {
		mtx_leave(&ddb_mp_mutex);
	}
}
#endif
