/*	$OpenBSD: afs_dir.h,v 1.1.1.1 1998/09/14 21:52:54 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/*
 * The directory structure used by AFS.
 */

/* $KTH: afs_dir.h,v 1.4 1998/02/21 17:19:59 assar Exp $ */

#ifndef _AFS_DIR_
#define _AFS_DIR_

#define AFSDIR_PAGESIZE 2048
#define ADIRHASHSIZE 128
#define ENTRIESPERPAGE 64

#define MAXPAGES 128

/* Is this really so? */

/*
 * We only save vnode and unique in the directory. Everything else is
 * the same as in the parent directory.
 */

typedef struct {
     u_int32_t Vnode;
     u_int32_t Unique;
} DirFid;

typedef struct {
     union {
	  struct {
	      u_int16_t pgcount;
	      u_int16_t tag;	/* Should be 1234 in net-order */
	      u_int8_t freecount;
	      u_int8_t bitmap[ENTRIESPERPAGE / 8];
	  } s;
	  u_int8_t pad[32];
     } u;
} PageHeader;

#define AFSDIRMAGIC 1234

#define pg_tag		u.s.tag
#define pg_freecount	u.s.freecount
#define pg_pgcount	u.s.pgcount
#define pg_bitmap	u.s.bitmap

typedef struct {
     u_int8_t map[MAXPAGES];
     u_int16_t hash[ADIRHASHSIZE];
} DirHeader;

typedef struct {
     u_int8_t flag;
     u_int8_t length;
     u_int16_t next;
     DirFid fid;
     char name[16];		/* If name overflows into fill, then we */
     char fill[4];		/* should use next entry too.  */
} DirEntry;

typedef struct {
     PageHeader header;
     DirHeader  dheader;
     DirEntry   entry[1];
} DirPage0;

typedef struct {
     PageHeader header;
     DirEntry   entry[1];
} DirPage1;

#endif /* _AFS_DIR_ */
