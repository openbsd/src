/*
 * Copyright (c) 1995 - 2001, 2003 Kungliga Tekniska Högskolan
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

/* $KTH: kafs.h,v 1.39.2.1 2003/04/23 18:03:21 lha Exp $ */

#ifndef __KAFS_H
#define __KAFS_H

/* XXX must include krb5.h or krb.h */

/* sys/ioctl.h must be included manually before kafs.h */

/*
 */

#include<xfs/xfs_pioctl.h>

#ifdef __STDC__
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

/* Use k_hasafs() to probe if the machine supports AFS syscalls.
   The other functions will generate a SIGSYS if AFS is not supported */

int k_hasafs __P((void));

int krb_afslog __P((const char *cell, const char *realm));
int krb_afslog_uid __P((const char *cell, const char *realm, uid_t uid));
int krb_afslog_home __P((const char *cell, const char *realm,
			 const char *homedir));
int krb_afslog_uid_home __P((const char *cell, const char *realm, uid_t uid,
			     const char *homedir));

int krb_realm_of_cell __P((const char *cell, char **realm));

/* compat */
#define k_afsklog krb_afslog
#define k_afsklog_uid krb_afslog_uid

int k_pioctl __P((char *a_path,
		  int o_opcode,
		  struct ViceIoctl *a_paramsP,
		  int a_followSymlinks));
int k_unlog __P((void));
int k_setpag __P((void));
int k_afs_cell_of_file __P((const char *path, char *cell, int len));



/* XXX */
#ifdef KFAILURE
#define KRB_H_INCLUDED
#endif

#ifdef KRB5_RECVAUTH_IGNORE_VERSION
#define KRB5_H_INCLUDED
#endif

void kafs_set_verbose __P((void (*kafs_verbose)(void *, const char *), void *));
int kafs_settoken_rxkad __P((const char *, struct ClearToken *,
			     void *ticket, size_t ticket_len));
#ifdef KRB_H_INCLUDED
int kafs_settoken __P((const char*, uid_t, CREDENTIALS*));
#endif
#ifdef KRB5_H_INCLUDED
int kafs_settoken5 __P((krb5_context, const char*, uid_t, krb5_creds*));
#endif


#ifdef KRB5_H_INCLUDED
krb5_error_code krb5_afslog_uid __P((krb5_context context,
				     krb5_ccache id,
				     const char *cell,
				     krb5_const_realm realm,
				     uid_t uid));
krb5_error_code krb5_afslog __P((krb5_context context,
				 krb5_ccache id, 
				 const char *cell,
				 krb5_const_realm realm));
krb5_error_code krb5_afslog_uid_home __P((krb5_context context,
					  krb5_ccache id,
					  const char *cell,
					  krb5_const_realm realm,
					  uid_t uid,
					  const char *homedir));

krb5_error_code krb5_afslog_home __P((krb5_context context,
				      krb5_ccache id,
				      const char *cell,
				      krb5_const_realm realm,
				      const char *homedir));

krb5_error_code krb5_realm_of_cell __P((const char *cell, char **realm));

#endif

#define _PATH_VICE		"/etc/afs/"
#define _PATH_THISCELL 		_PATH_VICE "ThisCell"
#define _PATH_CELLSERVDB 	_PATH_VICE "CellServDB"
#define _PATH_THESECELLS	_PATH_VICE "TheseCells"

#define _PATH_ARLA_VICE		"/etc/afs/"
#define _PATH_ARLA_THISCELL	_PATH_ARLA_VICE "ThisCell"
#define _PATH_ARLA_CELLSERVDB 	_PATH_ARLA_VICE "CellServDB"
#define _PATH_ARLA_THESECELLS	_PATH_ARLA_VICE "TheseCells"

#if 0
#define _PATH_OPENAFS_DEBIAN_VICE		"/etc/openafs/"
#define _PATH_OPENAFS_DEBIAN_THISCELL		_PATH_OPENAFS_DEBIAN_VICE "ThisCell"
#define _PATH_OPENAFS_DEBIAN_CELLSERVDB 	_PATH_OPENAFS_DEBIAN_VICE "CellServDB"
#define _PATH_OPENAFS_DEBIAN_THESECELLS		_PATH_OPENAFS_DEBIAN_VICE "TheseCells"
#endif

#define _PATH_ARLA_DEBIAN_VICE			"/etc/arla/"
#define _PATH_ARLA_DEBIAN_THISCELL		_PATH_ARLA_DEBIAN_VICE "ThisCell"
#define _PATH_ARLA_DEBIAN_CELLSERVDB		_PATH_ARLA_DEBIAN_VICE "CellServDB"
#define _PATH_ARLA_DEBIAN_THESECELLS		_PATH_ARLA_DEBIAN_VICE "TheseCells"

extern int _kafs_debug;

#endif /* __KAFS_H */
