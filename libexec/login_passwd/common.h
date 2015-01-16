/* $OpenBSD: common.h,v 1.5 2015/01/16 06:39:50 deraadt Exp $ */
/*-
 * Copyright (c) 2001 Hans Insulander <hin@openbsd.org>.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/types.h>
#include <sys/resource.h>

#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <login_cap.h>
#include <bsd_auth.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <err.h>
#include <util.h>
#include <limits.h>


#define MODE_LOGIN 0
#define MODE_CHALLENGE 1
#define MODE_RESPONSE 2

#define AUTH_OK 0
#define AUTH_FAILED -1

extern FILE *back;

#ifdef PASSWD
int pwd_login(char *, char *, char *, int, char *);
int pwd_gensalt(char *, int, login_cap_t *, char);
#endif
#ifdef KRB5
int krb5_login(char *, char *, char *, int, int, char *);
#endif

#endif /* !_COMMON_H_ */
