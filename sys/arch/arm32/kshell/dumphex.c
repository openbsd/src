/* $NetBSD: dumphex.c,v 1.2 1996/03/18 20:32:31 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * dumphex.c
 *
 * Hex memory dump routines
 *
 * Created      : 17/09/94
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

/* dumpb - dumps memory in bytes*/

void
dumpb(addr, count)
	u_char *addr;
	int count;
{
	u_int byte;
	int loop;

	for (; count > 0; count -= 16) {
		printf("%08x: ", (int)addr);

		for (loop = 0; loop < 16; ++loop) {
			byte = addr[loop];
			printf("%02x ", byte);
		}

		printf(" ");

		for (loop = 0; loop < 16; ++loop) {
			byte = addr[loop];
			if (byte < 0x20)
				printf("\x1b[31m%c\x1b[0m", byte + '@');
			else if (byte == 0x7f)
				printf("\x1b[31m?\x1b[0m");
			else if (byte < 0x80)
				printf("%c", byte);
			else if (byte < 0xa0)
				printf("\x1b[32m%c\x1b[0m", byte - '@');
			else if (byte == 0xff)
				printf("\x1b[32m?\x1b[0m");
			else
				printf("%c", byte & 0x7f);
		}

		printf("\r\n");
		addr += 16;
	}
}


/* dumpw - dumps memory in bytes*/

void 
dumpw(addr, count)
	u_char *addr;
	int count;
{
	u_int byte;
	int loop;

	for (; count > 0; count -= 32) {
		printf("%08x: ", (int)addr);

		for (loop = 0; loop < 8; ++loop) {
			byte = ((u_int *)addr)[loop];
			printf("%08x ", byte);
		}

		printf(" ");

		for (loop = 0; loop < 32; ++loop) {
			byte = addr[loop];
			if (byte < 0x20)
				printf("\x1b[31m%c\x1b[0m", byte + '@');
			else if (byte == 0x7f)
				printf("\x1b[31m?\x1b[0m");
			else if (byte < 0x80)
				printf("%c", byte);
			else if (byte < 0xa0)
				printf("\x1b[32m%c\x1b[0m", byte - '@');
			else if (byte == 0xff)
				printf("\x1b[32m?\x1b[0m");
			else
				printf("%c", byte & 0x7f);
		}

		printf("\r\n");
		addr += 32;
	}
}

/* End of dumphex.c */
