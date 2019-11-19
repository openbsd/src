/*	$OpenBSD: t_syscall.c,v 1.1.1.1 2019/11/19 19:57:04 bluhm Exp $	*/
/*	$NetBSD: t_syscall.c,v 1.3 2018/05/28 07:55:56 martin Exp $	*/

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "macros.h"

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_syscall.c,v 1.3 2018/05/28 07:55:56 martin Exp $");


#include "atf-c.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/endian.h>
#include <sys/syscall.h>

#if !defined(_LP64) && BYTE_ORDER == _BIG_ENDIAN
#define __SYSCALL_TO_UINTPTR_T(V)	((uintptr_t)((V)>>32))
#else
#define __SYSCALL_TO_UINTPTR_T(V)	((uintptr_t)(V))
#endif

static const char secrect_data[1024] = {
	"my secret key\n"
};

#define	FILE_NAME	"dummy"

#ifndef _LP64
ATF_TC(mmap_syscall);

ATF_TC_HEAD(mmap_syscall, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests mmap(2) via syscall(2)");
}

ATF_TC_BODY(mmap_syscall, tc)
{
	int fd;
	const char *p;

	fd = open(FILE_NAME, O_RDWR|O_CREAT|O_TRUNC, 0666);
	ATF_REQUIRE(fd != -1);

	write(fd, secrect_data, sizeof(secrect_data));

	p = (const char *)syscall(SYS_mmap,
		0, sizeof(secrect_data), PROT_READ, MAP_PRIVATE, fd, 0, 0, 0);
	ATF_REQUIRE(p != NULL);

 	ATF_REQUIRE(strcmp(p, secrect_data) == 0);
}
#endif

ATF_TC(mmap___syscall);

ATF_TC_HEAD(mmap___syscall, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests mmap(2) via __syscall(2)");
}

ATF_TC_BODY(mmap___syscall, tc)
{
	int fd;
	const char *p;

	fd = open(FILE_NAME, O_RDWR|O_CREAT|O_TRUNC, 0666);
	ATF_REQUIRE(fd != -1);

	write(fd, secrect_data, sizeof(secrect_data));

	p = (const char *)__SYSCALL_TO_UINTPTR_T(__syscall(SYS_mmap,
		0, sizeof(secrect_data), PROT_READ, MAP_PRIVATE, fd,
		/* pad*/ 0, (off_t)0));
	ATF_REQUIRE(p != NULL);

	ATF_REQUIRE(strcmp(p, secrect_data) == 0);
}

ATF_TP_ADD_TCS(tp)
{

#ifndef _LP64
	ATF_TP_ADD_TC(tp, mmap_syscall);
#endif
	ATF_TP_ADD_TC(tp, mmap___syscall);

	return atf_no_error();
}
