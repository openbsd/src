/*	$OpenBSD: db_mp.c,v 1.2 2004/06/13 21:49:15 niklas Exp $	*/

/*
 * Copyright (c) 2003 Andreas Gunnarsson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/simplelock.h>

#include <machine/db_machdep.h>

#include <ddb/db_output.h>

#define DDB_STATE_NOT_RUNNING	0
#define DDB_STATE_RUNNING	1

struct SIMPLELOCK ddb_mp_slock;

volatile int ddb_state = DDB_STATE_NOT_RUNNING;	/* protected by ddb_mp_slock */
volatile cpuid_t ddb_active_cpu;		/* protected by ddb_mp_slock */

/*
 * ddb_enter_ddb() is called when ddb is entered to stop the other
 * CPUs. If another cpu is already in ddb we'll wait until it's finished.
 */
void
db_enter_ddb()
{
	int s, i;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);

	while (ddb_state == DDB_STATE_RUNNING
	    && ddb_active_cpu != cpu_number()) {
		db_printf("CPU %d waiting to enter ddb\n", cpu_number());
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);

		/* Busy wait without locking, we'll confirm with lock later */
		while (ddb_state == DDB_STATE_RUNNING
		   && ddb_active_cpu != cpu_number())
			;	/* Do nothing */

		s = splhigh();
		SIMPLE_LOCK(&ddb_mp_slock);
	}

	ddb_state = DDB_STATE_RUNNING;
	ddb_active_cpu = cpu_number();

	for (i = 0; i < I386_MAXPROCS; i++) {
		if (cpu_info[i] != NULL) {
			if (i == cpu_number())
				cpu_info[i]->ci_ddb_paused = CI_DDB_INDDB;
			else if (cpu_info[i]->ci_ddb_paused
			    != CI_DDB_STOPPED) {
				cpu_info[i]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
				db_printf("Sending IPI to cpu %d\n", i);
				i386_send_ipi(cpu_info[i], I386_IPI_DDB);
			}
		}
	}
	db_printf("CPU %d entering ddb\n", cpu_number());
	SIMPLE_UNLOCK(&ddb_mp_slock);
	splx(s);
}

void
db_leave_ddb()
{
	int s, i;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);
	db_printf("CPU %d leaving ddb\n", cpu_number());
	for (i = 0; i < I386_MAXPROCS; i++) {
		if (cpu_info[i] != NULL) {
			cpu_info[i]->ci_ddb_paused = CI_DDB_RUNNING;
		}
	}
	ddb_state = DDB_STATE_NOT_RUNNING;
	SIMPLE_UNLOCK(&ddb_mp_slock);
	splx(s);
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

	if (cpu != cpu_number() && cpu_info[cpu] != NULL) {
		s = splhigh();
		SIMPLE_LOCK(&ddb_mp_slock);
		cpu_info[cpu]->ci_ddb_paused = CI_DDB_SHOULDSTOP;
		db_printf("Sending IPI to cpu %d\n", cpu);
		SIMPLE_UNLOCK(&ddb_mp_slock);
		splx(s);
		i386_send_ipi(cpu_info[cpu], I386_IPI_DDB);
	}
}

void
db_movetocpu(int cpu)
{
	int s;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);
	cpu_info[cpu]->ci_ddb_paused = CI_DDB_ENTERDDB;
	db_printf("Sending IPI to cpu %d\n", cpu);
	SIMPLE_UNLOCK(&ddb_mp_slock);
	splx(s);
	/* XXX If other CPU was running and IPI is lost, we lose. */
	i386_send_ipi(cpu_info[cpu], I386_IPI_DDB);
}

void
i386_ipi_db(struct cpu_info *ci)
{
	int s;

	s = splhigh();
	SIMPLE_LOCK(&ddb_mp_slock);
	db_printf("CPU %d received ddb IPI\n", cpu_number());
	while (ci->ci_ddb_paused == CI_DDB_SHOULDSTOP
	    || ci->ci_ddb_paused == CI_DDB_STOPPED) {
		if (ci->ci_ddb_paused == CI_DDB_SHOULDSTOP)
			ci->ci_ddb_paused = CI_DDB_STOPPED;
		SIMPLE_UNLOCK(&ddb_mp_slock);
		while (ci->ci_ddb_paused == CI_DDB_STOPPED)
			;	/* Do nothing */
		SIMPLE_LOCK(&ddb_mp_slock);
	}
	if (ci->ci_ddb_paused == CI_DDB_ENTERDDB) {
		ddb_state = DDB_STATE_RUNNING;
		ddb_active_cpu = cpu_number();
		ci->ci_ddb_paused = CI_DDB_INDDB;
		db_printf("CPU %d grabbing ddb\n", cpu_number());
		SIMPLE_UNLOCK(&ddb_mp_slock);
		Debugger();
		SIMPLE_LOCK(&ddb_mp_slock);
		ci->ci_ddb_paused = CI_DDB_RUNNING;
	}
	db_printf("CPU %d leaving ddb IPI handler\n", cpu_number());
	SIMPLE_UNLOCK(&ddb_mp_slock);
	splx(s);
}
