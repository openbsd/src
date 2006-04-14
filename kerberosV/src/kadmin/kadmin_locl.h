/*
 * Copyright (c) 1997-2004 Kungliga Tekniska Högskolan
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

/* 
 * $KTH: kadmin_locl.h,v 1.45 2004/06/27 15:04:07 joda Exp $
 */

#ifndef __ADMIN_LOCL_H__
#define __ADMIN_LOCL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif

#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <err.h>
#include <roken.h>
#include <krb5.h>
#include <krb5_locl.h>
#include <hdb.h>
#include <hdb_err.h>
#include <kadm5/admin.h>
#include <kadm5/private.h>
#include <kadm5/kadm5_err.h>
#include <parse_time.h>
#include <getarg.h>

extern krb5_context context;
extern void * kadm_handle;

#undef ALLOC
#define ALLOC(X) ((X) = malloc(sizeof(*(X))))

/* util.c */

void attributes2str(krb5_flags attributes, char *str, size_t len);
int  str2attributes(const char *str, krb5_flags *flags);
int  parse_attributes (const char *resp, krb5_flags *attr, int *mask, int bit);
int  edit_attributes (const char *prompt, krb5_flags *attr, int *mask,
		      int bit);

void time_t2str(time_t t, char *str, size_t len, int include_time);
int  str2time_t (const char *str, time_t *time);
int  parse_timet (const char *resp, krb5_timestamp *value, int *mask, int bit);
int  edit_timet (const char *prompt, krb5_timestamp *value, int *mask,
		 int bit);

void deltat2str(unsigned t, char *str, size_t len);
int  str2deltat(const char *str, krb5_deltat *delta);
int  parse_deltat (const char *resp, krb5_deltat *value, int *mask, int bit);
int  edit_deltat (const char *prompt, krb5_deltat *value, int *mask, int bit);

int edit_entry(kadm5_principal_ent_t ent, int *mask,
	       kadm5_principal_ent_t default_ent, int default_mask);
void set_defaults(kadm5_principal_ent_t ent, int *mask,
		  kadm5_principal_ent_t default_ent, int default_mask);
int set_entry(krb5_context context,
	      kadm5_principal_ent_t ent,
	      int *mask,
	      const char *max_ticket_life,
	      const char *max_renewable_life,
	      const char *expiration,
	      const char *pw_expiration,
	      const char *attributes);
int
foreach_principal(const char *exp, 
		  int (*func)(krb5_principal, void*), 
		  const char *funcname,
		  void *data);

int parse_des_key (const char *key_string,
		   krb5_key_data *key_data, const char **err);

/* server.c */

krb5_error_code
kadmind_loop (krb5_context, krb5_auth_context, krb5_keytab, int);

/* random_password.c */

void
random_password(char *pw, size_t len);

/* kadm_conn.c */

extern volatile sig_atomic_t term_flag, doing_useful_work;

void parse_ports(krb5_context, const char*);
int start_server(krb5_context);

/* server.c */

krb5_error_code
kadmind_loop (krb5_context, krb5_auth_context, krb5_keytab, int);

#endif /* __ADMIN_LOCL_H__ */
