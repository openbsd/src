/*	$OpenBSD: stack_protector.c,v 1.2 2002/12/02 09:02:57 deraadt Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(list)
static char rcsid[] = "$OpenBSD: stack_protector.c,v 1.2 2002/12/02 09:02:57 deraadt Exp $";
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <syslog.h>

long __guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static void __guard_setup(void) __attribute__ ((constructor));
void __stack_smash_handler(char func[], int damaged __attribute__((unused)));

static void
__guard_setup(void)
{
	int i, mib[2];
	size_t len;

	if (__guard[0] != 0)
		return;

	mib[0] = CTL_KERN;
	mib[1] = KERN_ARND;

	len = 4;
	for (i = 0; i < sizeof(__guard) / 4; i++) {
		if (sysctl(mib, 2, (char *)&((int *)__guard)[i],
		    &len, NULL, 0) == -1)
			break;
	}

	if (i < sizeof(__guard) / 4) {
		/* If sysctl was unsuccessful, use the "terminator canary". */
		((char *)__guard)[0] = 0; ((char*)__guard)[1] = 0;
		((char *)__guard)[2] = '\n'; ((char *)__guard)[3] = 255;
	}
}

void
__stack_smash_handler(char func[], int damaged)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	const char message[] = "stack overflow in function %s";
	struct sigaction sa;

	/* this may fail on a chroot jail, though luck */
	syslog_r(LOG_CRIT, &sdata, message, func);

	bzero(sa, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;
	sigaction(SIGABRT, &sa, NULL);

	kill(getpid(), SIGABRT);

	_exit(127);
}
