/*	$OpenBSD: pdcall.c,v 1.1 1998/09/12 02:53:25 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/param.h>

#include <machine/stdarg.h>
#include <machine/cpufunc.h>
#include <machine/pdc.h>
#include <machine/psl.h>
#include <machine/reg.h>

int
pdc_call(func, pdc_flag)
	iodcio_t func;
	int pdc_flag;
{
	register register_t ret, psw;
	va_list va;
	int args[11], i;

	va_start(va, pdc_flag);
	for (i = 0; i < sizeof(args)/sizeof(args[0]); i++)
		args[i] = va_arg(va, int);
	va_end(va);
	
	if (kernelmapped) {
		psw = PSW_Q;
		
		if (!pdc_flag && args[0] == PDC_PIM)
			psw |= PSW_M;

		set_psw(psw);
	}

	ret = (func)((void *)args[0], args[1], args[2], args[3], args[4],
		     args[5], args[6], args[7], args[8], args[9], args[10]);

	if (kernelmapped) {
		set_psw(KERNEL_PSW);
	}

	return ret;
}
