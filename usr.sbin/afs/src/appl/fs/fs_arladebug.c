/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "fs_local.h"

RCSID("$arla: fs_arladebug.c,v 1.1 2001/09/24 23:50:11 mattiasa Exp $");

int
arladebug_cmd (int argc, char **argv)
{
    int ret;
    int flags;

    if ((argc > 1 && strncmp("-h", argv[1], 2) == 0) || argc > 2) {
	fprintf (stderr, "arladebug [-h] [");
	arla_log_print_levels (stderr);
	fprintf (stderr, "]\n");
	return 0;
    }

    ret = arla_debug (-1, &flags);
    if (ret) {
	fprintf (stderr, "arla_debug: %s\n", strerror(ret));
	return 0;
    }

    if (argc == 1) {
	char buf[1024];

	unparse_flags (flags, arla_deb_units, buf, sizeof(buf));
	printf ("arladebug is: %s\n", buf);
    } else if (argc == 2) {
	const char *textflags = argv[1];

	ret = parse_flags (textflags, arla_deb_units, flags);
	if (ret < 0) {
	    fprintf (stderr, "arladebug: unknown/bad flags `%s'\n",
		     textflags);
	    return 0;
	}

	flags = ret;
	ret = arla_debug (flags, NULL);
	if (ret)
	    fprintf (stderr, "arla_debug: %s\n", strerror(ret));
    }
    return 0;
}
