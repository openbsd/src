/*	$OpenBSD: dlfcn_stubs.c,v 1.8 2004/08/11 19:14:56 drahn Exp $	*/

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *	All functions here are just stubs that will be overridden
 *	by the real functions in ld.so when dynamic loading is
 *	performed at exec. The symbols here are provided as a link
 *	helper so we can link a program using the dl functions
 *	without getting any unresolved references.
 */

void	*dlopen(const char *libname, int how) __attribute__((weak));
int	 dlclose(void *handle) __attribute__((weak));
void	*dlsym(void *handle, const char *name) __attribute__((weak));
int	 dlctl(void *handle, int command, void *data) __attribute__((weak));
const char *	dlerror(void) __attribute__((weak));
int	dladdr(const void *addr, void *info) __attribute__((weak));

#include <stdio.h>

void *
dlopen(const char *libname, int how)
{
	printf("Wrong dl symbols!\n");
	return NULL;
}

int
dlclose(void *handle)
{
	printf("Wrong dl symbols!\n");
	return 0;
}

void *
dlsym(void *handle, const char *name)
{
	printf("Wrong dl symbols!\n");
	return NULL;
}

int
dlctl(void *handle, int command, void *data)
{
	return -1;
}

const char *
dlerror(void)
{
	return "Wrong dl symbols!\n";
}

int
dladdr(const void *addr, void *info)
{
	printf("Wrong dl symbols!\n");
	return -1;
}
