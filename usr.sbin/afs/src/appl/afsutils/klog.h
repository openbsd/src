/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

/* $arla: klog.h,v 1.4 2000/10/03 00:06:24 lha Exp $ */

/* Function prototypes for klog.c */

int get_afs_id(void);

/* prototype for die et al */

void
die (int retcode)
    __attribute__ ((noreturn));

void
diet (int retcode, char *fmt, ...)
    __attribute__ ((noreturn, format (printf, 2, 3)));

void
dietx (int retcode, char *fmt, ...)
    __attribute__ ((noreturn, format (printf, 2, 3)));

int get_k4_ticket(void);

int get_afs_token(void);

int do_timeout( int (*function)(void) );

char *randfilename(void);


/* Feel free to correct these definitions with the actual numbers. */

#define CELL_MAX	REALM_SZ
#define REALM_MAX	REALM_SZ

/* According to code in arla/rxdef, max principal name length is 64. */
#define PRINCIPAL_MAX	64

#define PASSWD_MAX	200
#define PW_PROMPT_MAX	256

/* We use this and make it mode 1733; you should too */
#define KLOG_TKT_ROOT	"/ticket/"
