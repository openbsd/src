/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$arla: fs_setcrypt.c,v 1.3 2002/09/07 19:22:46 mattiasa Exp $");

static void
usage()
{
    fprintf(stderr, "Usage: setcrypt <on|off>\n");
    return;
}

int
setcrypt_cmd (int argc, char **argv)
{
    uint32_t n;
    int ret ;

    --argc;
    ++argv;

    if (argc != 1) {
	usage();
	return 0;
    }
    if (strcasecmp(argv[0], "on") == 0)
	n = 1;
    else if(strcasecmp(argv[0], "off") == 0)
	n = 0;
    else if(strcasecmp(argv[0], "-help") ==0) {
	usage();
	return 0;
    }
    else {
	printf ("setcrypt: Unknown parameter '%s'\n", argv[0]);
	return 0;
    }
    ret = fs_setcrypt (n);
    if (ret) 
	fserr(PROGNAME, ret, NULL);

    return 0;
}
