/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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
 * 4. Neither the name of the Institute nor the names of its contributors 
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

/* $arla: agetarg.h,v 1.6 2001/01/22 08:06:53 lha Exp $ */

#ifndef __AGETARG_H__
#define __AGETARG_H__

#include <stddef.h>

#define AARG_DEFAULT     0x0    /* AARG_GNUSTYLE */
#define AARG_LONGARG     0x1    /* --foo=bar */
#define AARG_SHORTARG    0x2    /* -abc   a, b and c are all three flags */
#define AARG_TRANSLONG   0x4    /* Incompatible with {SHORT,LONG}ARG */
#define AARG_SWITCHLESS  0x8    /* No switches */
#define AARG_SUBOPTION   0xF    /* For manpage generation */
#define AARG_USEFIRST	0x10   /* Use first partial found instead of failing */

#define AARG_GNUSTYLE (AARG_LONGARG|AARG_SHORTARG)
#define AARG_AFSSTYLE (AARG_TRANSLONG|AARG_SWITCHLESS)

struct agetargs{
    const char *long_name;
    char short_name;
    enum { aarg_end = 0, aarg_integer, aarg_string, 
	   aarg_flag, aarg_negative_flag, aarg_strings,
           aarg_generic_string } type;
    void *value;
    const char *help;
    const char *arg_help;
    enum { aarg_optional = 0, 
	   aarg_mandatory,
	   aarg_optional_swless } mandatoryp;
};

enum {
    AARG_ERR_NO_MATCH  = 1,
    AARG_ERR_BAD_ARG,
    AARG_ERR_NO_ARG
};

typedef struct agetarg_strings {
    int num_strings;
    char **strings;
} agetarg_strings;

int agetarg(struct agetargs *args,
	    int argc, char **argv, int *optind, int style);

void aarg_printusage (struct agetargs *args,
		      const char *progname,
		      const char *extra_string,
		      int style);

#endif /* __AGETARG_H__ */
