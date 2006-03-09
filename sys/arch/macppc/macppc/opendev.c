/*	$OpenBSD: opendev.c,v 1.9 2006/03/09 23:06:19 miod Exp $	*/
/*	$NetBSD: openfirm.c,v 1.1 1996/09/30 16:34:52 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stdarg.h>
#include <machine/psl.h>

#include <dev/ofw/openfirm.h>

extern void ofw_stack(void);
extern void ofbcopy(const void *, void *, size_t);

int
OF_instance_to_package(int ihandle)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int ihandle;
		int phandle;
	} args = {
		"instance-to-package",
		1,
		1,
	};

	ofw_stack();
	args.ihandle = ihandle;
	if (openfirmware(&args) == -1)
		return -1;
	return args.phandle;
}

int
OF_package_to_path(int phandle, char *buf, int buflen)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int phandle;
		char *buf;
		int buflen;
		int length;
	} args = {
		"package-to-path",
		3,
		1,
	};

	ofw_stack();
	if (buflen > PAGE_SIZE)
		return -1;
	args.phandle = phandle;
	args.buf = OF_buf;
	args.buflen = buflen;
	if (openfirmware(&args) < 0)
		return -1;
	if (args.length > 0)
		ofbcopy(OF_buf, buf, args.length);
	return args.length;
}


int
OF_call_method(char *method, int ihandle, int nargs, int nreturns, ...)
{
	va_list ap;
	static struct {
		char *name;
		int nargs;
		int nreturns;
		char *method;
		int ihandle;
		int args_n_results[12];
	} args = {
		"call-method",
		2,
		1,
	};
	int *ip, n;

	if (nargs > 6)
		return -1;
	args.nargs = nargs + 2;
	args.nreturns = nreturns + 1;
	args.method = method;
	args.ihandle = ihandle;
	va_start(ap, nreturns);
	for (ip = args.args_n_results + (n = nargs); --n >= 0;)
		*--ip = va_arg(ap, int);
	ofw_stack();
	if (openfirmware(&args) == -1) {
		va_end(ap);
		return -1;
	}
	if (args.args_n_results[nargs]) {
		va_end(ap);
		return args.args_n_results[nargs];
	}
	for (ip = args.args_n_results + nargs + (n = args.nreturns); --n > 0;)
		*va_arg(ap, int *) = *--ip;
	va_end(ap);
	return 0;
}
int
OF_call_method_1(char *method, int ihandle, int nargs, ...)
{
	va_list ap;
	static struct {
		char *name;
		int nargs;
		int nreturns;
		char *method;
		int ihandle;
		int args_n_results[8];
	} args = {
		"call-method",
		2,
		2,
	};
	int *ip, n;

	if (nargs > 6)
		return -1;
	args.nargs = nargs + 2;
	args.method = method;
	args.ihandle = ihandle;
	va_start(ap, nargs);
	for (ip = args.args_n_results + (n = nargs); --n >= 0;)
		*--ip = va_arg(ap, int);
	va_end(ap);
	ofw_stack();
	if (openfirmware(&args) == -1)
		return -1;
	if (args.args_n_results[nargs])
		return -1;
	return args.args_n_results[nargs + 1];
}

int
OF_open(char *dname)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		char *dname;
		int handle;
	} args = {
		"open",
		1,
		1,
	};
	int l;

	ofw_stack();
	if ((l = strlen(dname)) >= PAGE_SIZE)
		return -1;
	ofbcopy(dname, OF_buf, l + 1);
	args.dname = OF_buf;
	if (openfirmware(&args) == -1)
		return -1;
	return args.handle;
}

void
OF_close(int handle)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int handle;
	} args = {
		"close",
		1,
		0,
	};

	ofw_stack();
	args.handle = handle;
	openfirmware(&args);
}

/*
 * This assumes that character devices don't read in multiples of PAGE_SIZE.
 */
int
OF_read(int handle, void *addr, int len)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int ihandle;
		void *addr;
		int len;
		int actual;
	} args = {
		"read",
		3,
		1,
	};
	int l, act = 0;

	ofw_stack();
	args.ihandle = handle;
	args.addr = OF_buf;
	for (; len > 0; len -= l, addr += l) {
		l = min(PAGE_SIZE, len);
		args.len = l;
		if (openfirmware(&args) == -1)
			return -1;
		if (args.actual > 0) {
			ofbcopy(OF_buf, addr, args.actual);
			act += args.actual;
		}
		if (args.actual < l) {
			if (act)
				return act;
			else
				return args.actual;
		}
	}
	return act;
}

int
OF_write(int handle, void *addr, int len)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int ihandle;
		void *addr;
		int len;
		int actual;
	} args = {
		"write",
		3,
		1,
	};
	int l, act = 0;

	ofw_stack();
	args.ihandle = handle;
	args.addr = OF_buf;
	for (; len > 0; len -= l, addr += l) {
		l = min(PAGE_SIZE, len);
		ofbcopy(addr, OF_buf, l);
		args.len = l;
		if (openfirmware(&args) == -1)
			return -1;
		l = args.actual;
		act += l;
	}
	return act;
}

int
OF_seek(int handle, u_quad_t pos)
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
		int handle;
		int poshi;
		int poslo;
		int status;
	} args = {
		"seek",
		3,
		1,
	};

	ofw_stack();
	args.handle = handle;
	args.poshi = (int)(pos >> 32);
	args.poslo = (int)pos;
	if (openfirmware(&args) == -1)
		return -1;
	return args.status;
}
