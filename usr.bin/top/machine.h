/* $OpenBSD: machine.h,v 1.15 2007/05/29 00:56:56 otto Exp $	 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file defines the interface between top and the machine-dependent
 *  module.  It is NOT machine dependent and should not need to be changed
 *  for any specific machine.
 */

/*
 * the statics struct is filled in by machine_init
 */
struct statics {
	char          **procstate_names;
	char          **cpustate_names;
	char          **memory_names;
	char          **order_names;
};

/*
 * the system_info struct is filled in by a machine dependent routine.
 */

struct system_info {
	pid_t           last_pid;
	double          load_avg[NUM_AVERAGES];
	int             p_total;
	int             p_active;	/* number of procs considered
					 * "active" */
	int            *procstates;
	int64_t        *cpustates;
	int            *memory;
};

/*
 * cpu_states is an array of percentages * 10.  For example, the (integer)
 * value 105 is 10.5% (or .105).
 */

/*
 * the process_select struct tells get_process_info what processes we
 * are interested in seeing
 */

struct process_select {
	int             idle;	/* show idle processes */
	int             system;	/* show system processes */
	int             threads;	/* show threads */
	uid_t           uid;	/* only this uid (unless uid == -1) */
	pid_t           pid;	/* only this pid (unless pid == -1) */
	char           *command;/* only this command (unless == NULL) */
};

/* prototypes */
extern int      display_init(struct statics *);

/* machine.c */
extern int      machine_init(struct statics *);
extern char    *format_header(char *);
extern void     get_system_info(struct system_info *);
extern caddr_t
get_process_info(struct system_info *, struct process_select *,
		 int (*) (const void *, const void *));
extern char    *format_next_process(caddr_t, char *(*)(uid_t), pid_t *);
extern uid_t    proc_owner(pid_t);

extern struct kinfo_proc2	*getprocs(int, int, int *);
