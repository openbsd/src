/*	$OpenBSD: nxstack-mprotect.c,v 1.1 2002/07/27 06:52:37 mickey Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t fail;

void
testfly()
{
}

void
sigsegv(int sig)
{
	_exit(fail);
}

int
main(void)
{
	u_int64_t buf[256];	/* assuming the testfly() will fit */

	signal(SIGSEGV, sigsegv);
	memcpy(buf, &testfly, sizeof(buf));

	printf("making it execute\n");
	/* here we must fail on segv since we said it gets executable */
	fail = 1;
	mprotect(buf, sizeof(buf), PROT_READ|PROT_WRITE|PROT_EXEC);
	((void (*)(void))&buf)();

	printf("making it catch a signal\n");
	/* here we are successfull on segv and fail if it still executes */
	fail = 0;
	mprotect(buf, sizeof(buf), PROT_READ|PROT_WRITE);
	((void (*)(void))&buf)();

	exit(1);
}
