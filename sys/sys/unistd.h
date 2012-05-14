/*	$OpenBSD: unistd.h,v 1.21 2012/05/14 23:21:35 matthew Exp $	*/
/*	$NetBSD: unistd.h,v 1.10 1994/06/29 06:46:06 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)unistd.h	8.2 (Berkeley) 1/7/94
 */

#ifndef _SYS_UNISTD_H_
#define	_SYS_UNISTD_H_

#include <sys/cdefs.h>

#define	_POSIX_VDISABLE		(0377)

/* Define the POSIX.1 version we target for compliance. */
#define	_POSIX_VERSION		199009L

/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */

/* whence values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#if __BSD_VISIBLE
/* old BSD whence values for lseek(2); renamed by POSIX 1003.1 */
#define	L_SET		SEEK_SET
#define	L_INCR		SEEK_CUR
#define	L_XTND		SEEK_END

/* the parameters argument passed to the __tfork() syscall */
struct __tfork {
	void	*tf_tcb;
	pid_t	*tf_tid;
	int	tf_flags;
};
#endif

/* the pathconf(2) and sysconf(3) variable values are part of the ABI */

/* configurable pathname variables */
#define	_PC_LINK_MAX		 1
#define	_PC_MAX_CANON		 2
#define	_PC_MAX_INPUT		 3
#define	_PC_NAME_MAX		 4
#define	_PC_PATH_MAX		 5
#define	_PC_PIPE_BUF		 6
#define	_PC_CHOWN_RESTRICTED	 7
#define	_PC_NO_TRUNC		 8
#define	_PC_VDISABLE		 9

/* configurable system variables */
#define	_SC_ARG_MAX		 1
#define	_SC_CHILD_MAX		 2
#define	_SC_CLK_TCK		 3
#define	_SC_NGROUPS_MAX		 4
#define	_SC_OPEN_MAX		 5
#define	_SC_JOB_CONTROL		 6
#define	_SC_SAVED_IDS		 7
#define	_SC_VERSION		 8
#define	_SC_BC_BASE_MAX		 9
#define	_SC_BC_DIM_MAX		10
#define	_SC_BC_SCALE_MAX	11
#define	_SC_BC_STRING_MAX	12
#define	_SC_COLL_WEIGHTS_MAX	13
#define	_SC_EXPR_NEST_MAX	14
#define	_SC_LINE_MAX		15
#define	_SC_RE_DUP_MAX		16
#define	_SC_2_VERSION		17
#define	_SC_2_C_BIND		18
#define	_SC_2_C_DEV		19
#define	_SC_2_CHAR_TERM		20
#define	_SC_2_FORT_DEV		21
#define	_SC_2_FORT_RUN		22
#define	_SC_2_LOCALEDEF		23
#define	_SC_2_SW_DEV		24
#define	_SC_2_UPE		25
#define	_SC_STREAM_MAX		26
#define	_SC_TZNAME_MAX		27
#define	_SC_PAGESIZE		28
#define	_SC_PAGE_SIZE		_SC_PAGESIZE	/* 1170 compatibility */
#define	_SC_FSYNC		29
#define	_SC_XOPEN_SHM		30
#define	_SC_SEM_NSEMS_MAX	31
#define	_SC_SEM_VALUE_MAX	32
#define	_SC_HOST_NAME_MAX	33
#define	_SC_MONOTONIC_CLOCK	34
#define	_SC_2_PBS		35
#define	_SC_2_PBS_ACCOUNTING	36
#define	_SC_2_PBS_CHECKPOINT	37
#define	_SC_2_PBS_LOCATE	38
#define	_SC_2_PBS_MESSAGE	39
#define	_SC_2_PBS_TRACK		40
#define	_SC_ADVISORY_INFO	41
#define	_SC_AIO_LISTIO_MAX	42
#define	_SC_AIO_MAX		43
#define	_SC_AIO_PRIO_DELTA_MAX	44
#define	_SC_ASYNCHRONOUS_IO	45
#define	_SC_ATEXIT_MAX		46
#define	_SC_BARRIERS		47
#define	_SC_CLOCK_SELECTION	48
#define	_SC_CPUTIME		49
#define	_SC_DELAYTIMER_MAX	50
#define	_SC_IOV_MAX		51
#define	_SC_IPV6		52
#define	_SC_MAPPED_FILES	53
#define	_SC_MEMLOCK		54
#define	_SC_MEMLOCK_RANGE	55
#define	_SC_MEMORY_PROTECTION	56
#define	_SC_MESSAGE_PASSING	57
#define	_SC_MQ_OPEN_MAX		58
#define	_SC_MQ_PRIO_MAX		59
#define	_SC_PRIORITIZED_IO	60
#define	_SC_PRIORITY_SCHEDULING	61
#define	_SC_RAW_SOCKETS		62
#define	_SC_READER_WRITER_LOCKS	63
#define	_SC_REALTIME_SIGNALS	64
#define	_SC_REGEXP		65
#define	_SC_RTSIG_MAX		66
#define	_SC_SEMAPHORES		67
#define	_SC_SHARED_MEMORY_OBJECTS 68
#define	_SC_SHELL		69
#define	_SC_SIGQUEUE_MAX	70
#define	_SC_SPAWN		71
#define	_SC_SPIN_LOCKS		72
#define	_SC_SPORADIC_SERVER	73
#define	_SC_SS_REPL_MAX		74
#define	_SC_SYNCHRONIZED_IO	75
#define	_SC_SYMLOOP_MAX		76
#define	_SC_THREAD_ATTR_STACKADDR 77
#define	_SC_THREAD_ATTR_STACKSIZE 78
#define	_SC_THREAD_CPUTIME	79
#define	_SC_THREAD_DESTRUCTOR_ITERATIONS 80
#define	_SC_THREAD_KEYS_MAX	81
#define	_SC_THREAD_PRIO_INHERIT	82
#define	_SC_THREAD_PRIO_PROTECT	83
#define	_SC_THREAD_PRIORITY_SCHEDULING 84
#define	_SC_THREAD_PROCESS_SHARED 85
#define	_SC_THREAD_ROBUST_PRIO_INHERIT 86
#define	_SC_THREAD_ROBUST_PRIO_PROTECT 87
#define	_SC_THREAD_SPORADIC_SERVER 88
#define	_SC_THREAD_STACK_MIN	89
#define	_SC_THREAD_THREADS_MAX	90
#define	_SC_THREADS		91
#define	_SC_TIMEOUTS		92
#define	_SC_TIMER_MAX		93
#define	_SC_TIMERS		94
#define	_SC_TRACE		95
#define	_SC_TRACE_EVENT_FILTER	96
#define	_SC_TRACE_EVENT_NAME_MAX 97
#define	_SC_TRACE_INHERIT	98
#define	_SC_TRACE_LOG		99
#define	_SC_GETGR_R_SIZE_MAX	100
#define	_SC_GETPW_R_SIZE_MAX	101
#define	_SC_LOGIN_NAME_MAX	102
#define	_SC_THREAD_SAFE_FUNCTIONS 103
#define	_SC_TRACE_NAME_MAX      104
#define	_SC_TRACE_SYS_MAX       105
#define	_SC_TRACE_USER_EVENT_MAX 106
#define	_SC_TTY_NAME_MAX	107
#define	_SC_TYPED_MEMORY_OBJECTS 108
#define	_SC_V6_ILP32_OFF32	109
#define	_SC_V6_ILP32_OFFBIG	110
#define	_SC_V6_LP64_OFF64	111
#define	_SC_V6_LPBIG_OFFBIG	112
#define	_SC_V7_ILP32_OFF32	113
#define	_SC_V7_ILP32_OFFBIG	114
#define	_SC_V7_LP64_OFF64	115
#define	_SC_V7_LPBIG_OFFBIG	116
#define	_SC_XOPEN_CRYPT		117
#define	_SC_XOPEN_ENH_I18N	118
#define	_SC_XOPEN_LEGACY	119
#define	_SC_XOPEN_REALTIME	120
#define	_SC_XOPEN_REALTIME_THREADS 121
#define	_SC_XOPEN_STREAMS	122
#define	_SC_XOPEN_UNIX		123
#define	_SC_XOPEN_UUCP		124
#define	_SC_XOPEN_VERSION	125

#define	_SC_PHYS_PAGES		500
#define	_SC_AVPHYS_PAGES	501
#define	_SC_NPROCESSORS_CONF	502
#define	_SC_NPROCESSORS_ONLN	503

/* configurable system strings */
#define	_CS_PATH		 1

#endif /* !_SYS_UNISTD_H_ */
