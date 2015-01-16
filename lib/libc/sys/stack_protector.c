/*	$OpenBSD: stack_protector.c,v 1.16 2015/01/16 16:48:51 deraadt Exp $	*/

/*
 * Copyright (c) 2002 Hiroaki Etoh, Federico G. Schwindt, and Miodrag Vallat.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

void __stack_smash_handler(char func[], int damaged __attribute__((unused)));

/*ARGSUSED*/
void
__stack_smash_handler(char func[], int damaged)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	const char message[] = "stack overflow in function %s";
	struct sigaction sa;
	sigset_t mask;

	/* Immediately block all signal handlers from running code */
	sigfillset(&mask);
	sigdelset(&mask, SIGABRT);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	/* This may fail on a chroot jail... */
	syslog_r(LOG_CRIT, &sdata, message, func);

	bzero(&sa, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;
	sigaction(SIGABRT, &sa, NULL);

	kill(getpid(), SIGABRT);

	_exit(127);
}
