/*	@(#)ptrace.h 1.1 86/07/07 SMI	*/

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

/*
modification history
--------------------
01a,05jun90,llk  borrowed.
*/

#ifndef _PTRACE_
#define _PTRACE_

/*
 * Request values for the ptrace system call
 */
enum ptracereq {
	PTRACE_TRACEME = 0,		/* 0, by tracee to begin tracing */
	PTRACE_CHILDDONE = 0,		/* 0, tracee is done with his half */
	PTRACE_PEEKTEXT,		/* 1, read word from text segment */
	PTRACE_PEEKDATA,		/* 2, read word from data segment */
	PTRACE_PEEKUSER,		/* 3, read word from user struct */
	PTRACE_POKETEXT,		/* 4, write word into text segment */
	PTRACE_POKEDATA,		/* 5, write word into data segment */
	PTRACE_POKEUSER,		/* 6, write word into user struct */
	PTRACE_CONT,			/* 7, continue process */
	PTRACE_KILL,			/* 8, terminate process */
	PTRACE_SINGLESTEP,		/* 9, single step process */
	PTRACE_ATTACH,			/* 10, attach to an existing process */
	PTRACE_DETACH,			/* 11, detach from a process */
	PTRACE_GETREGS,			/* 12, get all registers */
	PTRACE_SETREGS,			/* 13, set all registers */
	PTRACE_GETFPREGS,		/* 14, get all floating point regs */
	PTRACE_SETFPREGS,		/* 15, set all floating point regs */
	PTRACE_READDATA,		/* 16, read data segment */
	PTRACE_WRITEDATA,		/* 17, write data segment */
	PTRACE_READTEXT,		/* 18, read text segment */
	PTRACE_WRITETEXT,		/* 19, write text segment */
	PTRACE_GETFPAREGS,		/* 20, get all fpa regs */
	PTRACE_SETFPAREGS		/* 21, set all fpa regs */
};
#endif !_PTRACE
