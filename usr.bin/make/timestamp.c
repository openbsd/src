/*	$OpenPackages$ */
/*	$OpenBSD: timestamp.c,v 1.3 2007/09/16 12:09:36 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "defines.h"
#include "timestamp.h"

#ifndef USE_TIMESPEC
#include <sys/types.h>
#include <utime.h>
#endif

TIMESTAMP now;		/* The time at the start of this whole process */

int
set_times(const char *f)
{
#ifdef USE_TIMESPEC
    struct timeval tv[2];

    TIMESPEC_TO_TIMEVAL(&tv[0], &now);
    TIMESPEC_TO_TIMEVAL(&tv[1], &now);
    return utimes(f, tv);
#else
    struct utimbuf times;

    times.actime = times.modtime = now;
    return utime(f, &times);
#endif
}

char *
time_to_string(TIMESTAMP time)
{
	struct tm *parts;
	static char buf[128];
	time_t t;

	t = timestamp2time_t(time);

	parts = localtime(&t);
	strftime(buf, sizeof buf, "%H:%M:%S %b %d, %Y", parts);
	buf[sizeof(buf) - 1] = '\0';
	return buf;
}

