/*
 * Copyright (c) 1995 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yppasswd.h,v 1.1 1995/11/01 17:40:35 deraadt Exp $
 */

#ifndef _YPPASSWD_H_RPCGEN
#define _YPPASSWD_H_RPCGEN

struct x_passwd {
	char *pw_name;
	char *pw_passwd;
	int pw_uid;
	int pw_gid;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
};
typedef struct x_passwd x_passwd;
#ifdef __cplusplus 
extern "C" bool_t xdr_x_passwd(XDR *, x_passwd*);
#elif __STDC__ 
extern  bool_t xdr_x_passwd(XDR *, x_passwd*);
#else /* Old Style C */ 
bool_t xdr_x_passwd();
#endif /* Old Style C */ 


struct yppasswd {
	char *oldpass;
	x_passwd newpw;
};
typedef struct yppasswd yppasswd;
#ifdef __cplusplus 
extern "C" bool_t xdr_yppasswd(XDR *, yppasswd*);
#elif __STDC__ 
extern  bool_t xdr_yppasswd(XDR *, yppasswd*);
#else /* Old Style C */ 
bool_t xdr_yppasswd();
#endif /* Old Style C */ 


#define YPPASSWDPROG ((u_long)100009)
#define YPPASSWDVERS ((u_long)1)

#ifdef __cplusplus
#define YPPASSWDPROC_UPDATE ((u_long)1)
extern "C" int * yppasswdproc_update_1(yppasswd *, CLIENT *);
extern "C" int * yppasswdproc_update_1_svc(yppasswd *, struct svc_req *, SVCXPRT *);

#elif __STDC__
#define YPPASSWDPROC_UPDATE ((u_long)1)
extern  int * yppasswdproc_update_1(yppasswd *, CLIENT *);
extern  int * yppasswdproc_update_1_svc(yppasswd *, struct svc_req *, SVCXPRT *);

#else /* Old Style C */ 
#define YPPASSWDPROC_UPDATE ((u_long)1)
extern  int * yppasswdproc_update_1();
extern  int * yppasswdproc_update_1_svc();
#endif /* Old Style C */ 

#endif /* !_YPPASSWD_H_RPCGEN */
