/*	$OpenBSD: timestamp.c,v 1.10 2013/05/22 12:14:08 espie Exp $ */

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
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "timestamp.h"


struct timespec starttime;

int
set_times(const char *f)
{
    return utimes(f, NULL);
}

#define PLACEHOLDER "XXXXXXXXX "
char *
time_to_string(struct timespec *t)
{
	struct tm *parts;
	static char buf[128];
	char *s;

	parts = localtime(&t->tv_sec);
	strftime(buf, sizeof buf, "%H:%M:%S." PLACEHOLDER "%b %d, %Y", parts);
	s = strstr(buf, PLACEHOLDER);
	if (s) {
		snprintf(s, sizeof(PLACEHOLDER), "%09ld", t->tv_nsec);
		s[9] = ' ';
	}
	buf[sizeof(buf) - 1] = '\0';
	return buf;
}

