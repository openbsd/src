/*
 * Copyright (c) 2001 - 2002 Kungliga Tekniska Högskolan
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
 * The interface for the on disk storage of data when arla is down
 */

/* $arla: state.h,v 1.3 2002/07/24 06:06:13 lha Exp $ */

#ifndef _STORE_H_
#define _STORE_H_

/*
 *
 */
#define STORE_CELLNAMELENGTH	256

/*
 * This is magic cookie for the dump of the fcache.
 * It's supposed not to be able to be confused with an old-style
 * dump (with no header)
 */
#define FCACHE_MAGIC_COOKIE	0xff1201ff

/*
 * current version number of the dump file
 */
#define FCACHE_VERSION		0x5

struct fcache_store {
    char cell[STORE_CELLNAMELENGTH];
    AFSFid fid;
    unsigned refcount;
    size_t length;
    size_t fetched_length;
    AFSVolSync volsync;
    AFSFetchStatus status;
    uint32_t anonaccess;
    unsigned index;
    struct {
	unsigned attrp : 1;
	unsigned datap : 1;
	unsigned extradirp : 1;
	unsigned mountp : 1;
	unsigned fake_mp : 1;
	unsigned vol_root : 1;
    } flags;
    u_int tokens;
    char parentcell[STORE_CELLNAMELENGTH];
    AFSFid parent;
    Bool priority;
};    

/*
 * This is magic cookie for the dump of the volcache.
 * It's supposed not to be able to be confused with an old-style
 * dump (with no header)
 */

#define VOLCACHE_MAGIC_COOKIE	0x00120100

/*
 * current version number of the dump file
 */

#define VOLCACHE_VERSION	0x4

struct volcache_store {
    char cell[STORE_CELLNAMELENGTH];
    nvldbentry entry;
    AFSVolSync volsync;
    unsigned refcount;
};

enum { STORE_NEXT = 0, STORE_DONE = 1, STORE_SKIP = 2 };

typedef int (*store_fcache_fn)(struct fcache_store *, void *);
typedef int (*store_volcache_fn)(struct volcache_store *, void *);

int
state_recover_volcache(const char *file, store_volcache_fn func, void *ptr);

int
state_store_volcache(const char *fn, store_volcache_fn func, void *ptr);

int
state_recover_fcache(const char *file, store_fcache_fn func, void *ptr);

int
state_store_fcache(const char *fn, store_fcache_fn func, void *ptr);

#endif /* _STORE_H */
