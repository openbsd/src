/*	$OpenBSD: db_mp.c,v 1.3 2004/06/21 22:41:11 andreas Exp $	*/

/*
 * Copyright (c) 2003, 2004 Andreas Gunnarsson <andreas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/simplelock.h>

#include <machine/db_machdep.h>

#include <ddb/db_output.h>

struct SIMPLELOCK ddb_mp_slock;

volatile int ddb_state = DDB_STATE_NOT_RUNNING;	/* protected by ddb_mp_slock */
volatile cpuid_t ddb_active_cpu;		/* protected by ddb_mp_slock */

extern volatile boolean_t	db_switch_cpu;
extern volatile long		db_switch_to_cpu;

/*
 * All processors wait in db_enter_ddb() (unless explicitly started from
 * ddb) but only one owns ddb.  If the current processor should own ddb,
 * db_enter_ddb() returns 1.  If the current processor should keep
 * executing as usual (if ddb is exited or the processor is explicitly
 * started), db_enter_ddb returns 0.
 * If this is the first CPU entering ddb, db_enter_ddb() will stop all
 * other CPUs by sending IPIs.
 */
int
db_enter_ddb()
{
	int s, i;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);

	/* If we are first in, grab ddb and stop all other CPUs */
	if (ddb_state == DDB_STATE_NOT_RUNNING) {
		ddb_active_cpu = cpu_number();
		ddb_state = DDB_STATE_RUNNING;
		curcpu()->ci_ddb_paused = CI_DDB_INDDB;
		SIMPLE_UNLOCK(&ddb_mp_slock);
		for (i = 0; i < I386_MAXPROCS; i++) {
			if (cpu_info[i] != NULL && i != cpu_number() &&
			    cpu_info[i]->ci_ddb_paused != CI_DDB_STOPPED) {
				cpu_info[i]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
				i386_send_ipi(cpu_info[i], I386_IPI_DDB);
			}
		}
		splx(s);
		return (1);
	}

	/* Leaving ddb completely.  Start all other CPUs and return 0 */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_EXITING) {
		for (i = 0; i < I386_MAXPROCS; i++) {
			if (cpu_info[i] != NULL) {
				cpu_info[i]->ci_ddb_paused = CI_DDB_RUNNING;
			}
		}
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
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
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);

		/* Busy wait without locking, we'll confirm with lock later */
		while (ddb_active_cpu != cpu_number() &&
		    curcpu()->ci_ddb_paused != CI_DDB_RUNNING)
			;	/* Do nothing */

		s = splhigh();
		SIMPLE_LOCK(&ddb_mp_slock);
	}

	/* Either enter ddb or exit */
	if (ddb_active_cpu == cpu_number() && ddb_state == DDB_STATE_RUNNING) {
		curcpu()->ci_ddb_paused = CI_DDB_INDDB;
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
		return (1);
	} else {
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
		return (0);
	}
}

void
db_startcpu(int cpu)
{
	int s;

	if (cpu != cpu_number() && cpu_info[cpu] != NULL) {
		s = splhigh();
		SIMPLE_LOCK(&ddb_mp_slock);
		cpu_info[cpu]->ci_ddb_paused = CI_DDB_RUNNING;
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
	}
}

void
db_stopcpu(int cpu)
{
	int s;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);
	if (cpu != cpu_number() && cpu_info[cpu] != NULL &&
	    cpu_info[cpu]->ci_ddb_paused != CI_DDB_STOPPED) {
		cpu_info[cpu]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
		i386_send_ipi(cpu_info[cpu], I386_IPI_DDB);
	} else {
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
	}
}

void
i386_ipi_db(struct cpu_info *ci)
{
	Debugger();
}
