/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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

/* $arla: cred.h,v 1.33 2003/06/10 16:21:11 lha Exp $ */

#ifndef _CRED_H_
#define _CRED_H_

#include <sys/types.h>
#include <time.h>
#include <lock.h>
#ifdef HAVE_KRB4
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>
#endif /* HAVE_KRB4 */
#include "bool.h"
#include <nnpfs/nnpfs_message.h>

/* The cred-types we support */
#define CRED_NONE     0
#define CRED_KRB4     1
#define CRED_KRB5     2
#define CRED_GK_K5    3
#define CRED_MAX      CRED_GK_K5
#define CRED_ANY      (-1)

struct cred_rxkad {
    struct ClearToken ct;
    size_t ticket_len;
    unsigned char ticket[MAXKRB4TICKETLEN];
};

struct cred_rxgk {
    int type;
    union {
	struct {
	    int32_t kvno;
	    int32_t enctype;
	    size_t sessionkey_len;
	    void *sessionkey;
	    size_t ticket_len;
	    void *ticket;
	} k5;
    } t;
};

typedef struct {
    nnpfs_pag_t cred;
    uid_t uid;
    int type;
    int securityindex;
    long cell;
    time_t expire;
    void *cred_data;
    void (*cred_free_func)(void *);
    struct {
	unsigned killme : 1;
    } flags;
    unsigned refcount;
    union {
	List *list; 
	Listitem *li;
    } pag;
} CredCacheEntry;

void cred_init (unsigned nentries);

CredCacheEntry *
cred_get (long cell, nnpfs_pag_t cred, int type);

int
cred_list_pag(nnpfs_pag_t, int, 
	      int (*func)(CredCacheEntry *, void *),
	      void *);

void
cred_free (CredCacheEntry *ce);

CredCacheEntry *
cred_add (nnpfs_pag_t cred, int type, int securityindex, long cell,
	  time_t expire, void *cred_data, size_t cred_data_sz,
	  uid_t uid);

void
cred_delete (CredCacheEntry *ce);

void
cred_expire (CredCacheEntry *ce);

void cred_status (void);

void cred_remove (nnpfs_pag_t cred);

#endif /* _CRED_H_ */
