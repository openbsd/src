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
 * Interface to directory handling routines
 */

/* $arla: fdir.h,v 1.7 2002/03/06 21:41:43 tol Exp $ */

#ifndef _FDIR_H_
#define _FDIR_H_

#ifndef _KERNEL
#include <roken.h>
#endif

#include <fids.h>
#include <fbuf.h>

int
fdir_lookup (fbuf *the_fbuf,
	     const VenusFid *dir,
	     const char *name,
	     VenusFid *file);


int
fdir_changefid (fbuf *the_fbuf,
		const char *name,
		const VenusFid *file);

int
fdir_emptyp (fbuf *dir);

typedef int (*fdir_readdir_func)(VenusFid *, const char *, void *);

int
fdir_readdir (fbuf *the_fbuf,
	      fdir_readdir_func func,
	      void *arg,
	      VenusFid dir,
	      uint32_t *offset);

int
fdir_creat (fbuf *dir,
	    const char *filename,
	    AFSFid fid);

int
fdir_remove (fbuf *dir,
	     const char *name,
	     AFSFid *fid);

int
fdir_mkdir (fbuf *dir,
	    AFSFid dot,
	    AFSFid dot_dot);

#endif /* _FDIR_H_ */
