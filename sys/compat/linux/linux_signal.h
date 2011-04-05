/*	$OpenBSD: linux_signal.h,v 1.9 2011/04/05 22:54:31 pirofti Exp $	*/
/* 	$NetBSD: linux_signal.h,v 1.4 1995/08/27 20:51:51 fvdl Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_SIGNAL_H_
#define _LINUX_SIGNAL_H_

#define LINUX_SIGHUP	 1
#define LINUX_SIGINT	 2
#define LINUX_SIGQUIT	 3
#define LINUX_SIGILL	 4
#define LINUX_SIGTRAP	 5
#define LINUX_SIGABRT	 6
#define LINUX_SIGIOT	 6
#define LINUX_SIGBUS	 7
#define LINUX_SIGFPE	 8
#define LINUX_SIGKILL	 9
#define LINUX_SIGUSR1	10
#define LINUX_SIGSEGV	11
#define LINUX_SIGUSR2	12
#define LINUX_SIGPIPE	13
#define LINUX_SIGALRM	14
#define LINUX_SIGTERM	15
#define LINUX_SIGSTKFLT	16
#define LINUX_SIGCHLD	17
#define LINUX_SIGCONT	18
#define LINUX_SIGSTOP	19
#define LINUX_SIGTSTP	20
#define LINUX_SIGTTIN	21
#define LINUX_SIGTTOU	22
#define LINUX_SIGURG	23
#define LINUX_SIGXCPU	24
#define LINUX_SIGXFSZ	25
#define LINUX_SIGVTALRM	26
#define LINUX_SIGPROF	27
#define LINUX_SIGWINCH	28
#define LINUX_SIGIO	29
#define LINUX_SIGPWR	30
#define LINUX_SIGUNUSED	31
#define LINUX_NSIG	32

#define LINUX__NSIG 		64
#define LINUX__NSIG_BPW		32
#define LINUX__NSIG_WORDS	(LINUX__NSIG / LINUX__NSIG_BPW)

#define LINUX_SIG_BLOCK		0
#define LINUX_SIG_UNBLOCK	1
#define LINUX_SIG_SETMASK	2

typedef u_long	linux_old_sigset_t;
typedef struct {
	u_long sig[LINUX__NSIG_WORDS];
} linux_sigset_t;

typedef void	(*linux_handler_t)(int);

struct linux_old_sigaction {
	linux_handler_t		sa__handler;
	linux_old_sigset_t	sa_mask;
	u_long			sa_flags;
	void			(*sa_restorer)(void);
};

struct linux_sigaction {
	linux_handler_t		sa__handler;
	u_long			sa_flags;
	void			(*sa_restorer)(void);
	linux_sigset_t		sa_mask;
};

/* sa_flags */
#define LINUX_SA_NOCLDSTOP	0x00000001
#define LINUX_SA_SIGINFO	0x00000004
#define LINUX_SA_ONSTACK	0x08000000
#define LINUX_SA_RESTART	0x10000000
#define LINUX_SA_INTERRUPT	0x20000000
#define LINUX_SA_NOMASK		0x40000000
#define LINUX_SA_ONESHOT	0x80000000
#define LINUX_SA_ALLBITS	0xf8000001

struct linux_sigaltstack {
	void	*ss_sp;
	int	ss_flags;
	size_t	ss_size;
};

/* ss_flags */
#define LINUX_SS_ONSTACK	0x00000001
#define LINUX_SS_DISABLE	0x00000002

extern int bsd_to_linux_sig[];
extern int linux_to_bsd_sig[];

void linux_old_to_bsd_sigset(const linux_old_sigset_t *, sigset_t *);
void bsd_to_linux_old_sigset(const sigset_t *, linux_old_sigset_t *);

void linux_old_extra_to_bsd_sigset(const linux_old_sigset_t *,
    const unsigned long *, sigset_t *);
void bsd_to_linux_old_extra_sigset(const sigset_t *,
    linux_old_sigset_t *, unsigned long *);

void linux_to_bsd_sigset(const linux_sigset_t *, sigset_t *);
void bsd_to_linux_sigset(const sigset_t *, linux_sigset_t *);

void linux_old_to_bsd_sigaction(struct linux_old_sigaction *, 
    struct sigaction *);
void bsd_to_linux_old_sigaction(struct sigaction *, 
    struct linux_old_sigaction *);

void linux_to_bsd_sigaction(struct linux_sigaction *,
    struct sigaction *);
void bsd_to_linux_sigaction(struct sigaction *,
    struct linux_sigaction *);

int  linux_to_bsd_signal (int, int *);
int  bsd_to_linux_signal (int, int *);

#endif /* !_LINUX_SIGNAL_H_ */
