/*	$OpenBSD: kadm_locl.h,v 1.5 1998/02/18 11:53:33 art Exp $	*/
/* $KTH: kadm_locl.h,v 1.26 1997/09/02 18:13:26 assar Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/wait.h>
#include <pwd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <syslog.h>

#include <err.h>

#include <com_err.h>
#include <sl.h>

#include <des.h>
#include <kerberosIV/krb.h>
#include <krb_err.h>
#include <kerberosIV/krb_db.h>
#include <kerberosIV/kadm.h>
#include <kadm_err.h>
#include <kerberosIV/acl.h>

#include <krb_log.h>

#include "kadm_server.h"
#include "pw_check.h"

/* from libacl */
/* int acl_check(char *acl, char *principal); */

/* GLOBALS */
extern char *acldir;
extern Kadm_Server server_parm;

/* Utils */
int kadm_change __P((char *, char *, char *, des_cblock));
int kadm_add_entry __P((char *, char *, char *, Kadm_vals *, Kadm_vals *));
int kadm_mod_entry __P((char *, char *, char *, Kadm_vals *, Kadm_vals *, Kadm_vals *));
int kadm_get_entry __P((char *, char *, char *, Kadm_vals *, u_char *, Kadm_vals *));
int kadm_delete_entry __P((char *, char *, char *, Kadm_vals *));
int kadm_ser_cpw __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_add __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_mod __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_get __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_delete __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_init __P((int inter, char realm[]));
int kadm_ser_in __P((u_char **, int *));

int get_pw_new_pwd  __P((char *pword, int pwlen, krb_principal *pr, int print_realm));

/* cracklib */
char *FascistCheck __P((char *password, char *path, char **strings));
