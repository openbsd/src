/* $OpenBSD: regress.c,v 1.7 2010/07/19 19:46:44 espie Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Code written for the OpenBSD project.
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

/* regression tests */
#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "str.h"

int main(void);
#define CHECK(s)		\
do {				\
    printf("%-65s", #s);	\
    if (s)			\
	printf("ok\n"); 	\
    else {			\
	printf("failed\n");	\
	errors++;		\
    }				\
} while (0);

int
main(void)
{
    unsigned int errors = 0;

    CHECK(Str_Match("string", "string") == true);
    CHECK(Str_Match("string", "string2") == false);
    CHECK(Str_Match("string", "string*") == true);
    CHECK(Str_Match("Long string", "Lo*ng") == true);
    CHECK(Str_Match("Long string", "Lo*ng ") == false);
    CHECK(Str_Match("Long string", "Lo*ng *") == true);
    CHECK(Str_Match("string", "stri?g") == true);
    CHECK(Str_Match("str?ng", "str\\?ng") == true);
    CHECK(Str_Match("striiiing", "str?*ng") == true);
    CHECK(Str_Match("Very long string just to see", "******a****") == false);
    CHECK(Str_Match("d[abc?", "d\\[abc\\?") == true);
    CHECK(Str_Match("d[abc!", "d\\[abc\\?") == false);
    CHECK(Str_Match("dwabc?", "d\\[abc\\?") == false);
    CHECK(Str_Match("da0", "d[bcda]0") == true);
    CHECK(Str_Match("da0", "d[z-a]0") == true);
    CHECK(Str_Match("d-0", "d[-a-z]0") == true);
    CHECK(Str_Match("dy0", "d[a\\-z]0") == false);
    CHECK(Str_Match("d-0", "d[a\\-z]0") == true);
    CHECK(Str_Match("dz0", "d[a\\]z]0") == true);

    if (errors != 0)
	printf("Errors: %d\n", errors);
    exit(0);
}


