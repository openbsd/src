/*	$OpenPackages$ */
/*	$OpenBSD: dir.h,v 1.14 2001/05/03 13:41:04 espie Exp $	*/
/*	$NetBSD: dir.h,v 1.4 1996/11/06 17:59:05 christos Exp $ */

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)dir.h 8.1 (Berkeley) 6/6/93
 */

/* dir.h --
 */

#ifndef DIR_H
#define DIR_H

typedef struct Path_ {
    int 	  refCount;	/* Number of paths with this directory */
    int 	  hits; 	/* the number of times a file in this
				 * directory has been found */
    struct ohash   files;	/* Hash table of files in directory */
    char	  name[1];	/* Name of directory */
} Path;

extern void Dir_Init(void);
extern void Dir_End(void);
extern Boolean Dir_HasWildcards(const char *);
extern void Dir_Expand(const char *, Lst, Lst);
extern char *Dir_FindFilei(const char *, const char *, Lst);
#define Dir_FindFile(n, e) Dir_FindFilei(n, strchr(n, '\0'), e)
extern TIMESTAMP Dir_MTime(GNode *);
extern void Dir_AddDir(Lst, const char *, const char *);
extern char *Dir_MakeFlags(const char *, Lst);
extern void Dir_Concat(Lst, Lst);
extern void Dir_PrintDirectories(void);
extern void Dir_PrintPath(Lst);
extern void Dir_Destroy(void *);
extern void *Dir_CopyDir(void *);
extern int set_times(const char *);

#endif /* DIR_H */
