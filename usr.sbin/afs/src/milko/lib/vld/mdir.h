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

/* $arla: mdir.h,v 1.8 2002/03/06 22:43:02 tol Exp $ */

/*
 * Interface to fdir directory handling routines
 */

/* $arla: mdir.h,v 1.8 2002/03/06 22:43:02 tol Exp $ */

#ifndef _MDIR_H_
#define _MDIR_H_

#include <fs.h>
#include <mnode.h>

int
mdir_lookup (struct mnode *node, const char *name, AFSFid *file);

int
mdir_emptyp (struct mnode *node);

int
mdir_readdir (struct mnode *node,
	      int (*func)(VenusFid *, const char *, void *),
	      void *arg,
	      VenusFid dir);

int
mdir_creat (struct mnode *node,
	    const char *filename,
	    AFSFid fid);

int
mdir_remove (struct mnode *node,
	     const char *name);

int
mdir_mkdir (struct mnode *node,
	    AFSFid dot,
	    AFSFid dot_dot);

int
mdir_rename(struct mnode *dir1, const char *name1, int32_t *len1,
	    struct mnode *dir2, const char *name2, int32_t *len2);

int
mdir_changefid(struct mnode *dir, const char *name, AFSFid fid);

#endif /* _MDIR_H_ */
