/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
 * Header for credetial cache
 */

/* $KTH: cred.h,v 1.26 2000/10/02 22:31:15 lha Exp $ */

#ifndef _CRED_H_
#define _CRED_H_

#include <sys/types.h>
#include <time.h>
#include <lock.h>
#ifdef KERBEROS
#include <des.h>
#include <krb.h>
#endif /* KERBEROS */
#include "bool.h"
#include <xfs/xfs_message.h>

/* The cred-types we support */
#define CRED_NONE     0
#define CRED_KRB4     1
#define CRED_KRB5     2
#define CRED_MAX      CRED_KRB5
#define CRED_ANY      (-1)

#ifdef KERBEROS
typedef struct {
    CREDENTIALS c;
} krbstruct;
#endif

typedef struct {
    xfs_pag_t cred;
    uid_t uid;
    int type;
    int securityindex;
    long cell;
    time_t expire;
    void *cred_data;
    struct {
	unsigned killme : 1;
    } flags;
    unsigned refcount;
} CredCacheEntry;

void cred_init (unsigned nentries);

CredCacheEntry *
cred_get (long cell, xfs_pag_t cred, int type);

void
cred_free (CredCacheEntry *ce);

CredCacheEntry *
cred_add (xfs_pag_t cred, int type, int securityindex, long cell,
	  time_t expire, void *cred_data, size_t cred_data_sz,
	  uid_t uid);

void
cred_delete (CredCacheEntry *ce);

void
cred_expire (CredCacheEntry *ce);

#ifdef KERBEROS
CredCacheEntry * cred_add_krb4 (xfs_pag_t cred, uid_t uid, CREDENTIALS *c);
#endif

void cred_status (void);

void cred_remove (xfs_pag_t cred);

#endif /* _CRED_H_ */
