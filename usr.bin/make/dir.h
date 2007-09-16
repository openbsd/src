#ifndef DIR_H
#define DIR_H

/*	$OpenPackages$ */
/*	$OpenBSD: dir.h,v 1.20 2007/09/16 12:30:35 espie Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef TIMESTAMP_TYPE
#include "timestamp_t.h"
#endif

/* dir --
 *	Directory searching using wildcards and/or normal names...
 *	Used both for source wildcarding in the Makefile and for finding
 *	implicit sources.
 */

/* Dir_Init()
 *	Initialize the module.
 */
extern void Dir_Init(void);

/* Dir_End()
 *	Cleanup the module.
 */
#ifdef CLEANUP
extern void Dir_End(void);
#else
#define Dir_End()
#endif

/*
 * Manipulating paths. By convention, the empty path always allows for
 * finding files in the current directory.
 */

/* Dir_AddDiri(path, name, end);
 *	Add directory (name, end) to a search path.
 */
extern void Dir_AddDiri(Lst, const char *, const char *);
#define Dir_AddDir(l, n)	Dir_AddDiri(l, n, NULL)

/* Dir_Concat(p1, p2);
 *	Concatenate two paths, adding dirs in p2 to the end of p1, but
 *	avoiding duplicates.
 */
extern void Dir_Concat(Lst, Lst);

/* Dir_Destroy(d);	    
 *	Destroy a directory in a search path. 
 */
extern void Dir_Destroy(void *);

/* p2 = Dir_CopyDir(p);
 * 	Return a copy of a directory. Callback to duplicate search paths.
 */
extern void *Dir_CopyDir(void *);

/* Dir_PrintPath(p);
 *	Print the directory names along a given path.
 */
extern void Dir_PrintPath(Lst);


/*
 * Handling file names, and looking them up in paths
 */

/* fullname = Dir_FindFileComplexi(name, end, path, checkCurdirFirst)
 *	Searches for a file (name, end) on a given search path.  If it exists, 
 *	return the fullname of the file, otherwise NULL.
 *	The fullname is always a copy, and the caller is responsible for
 *	free()ing it.
 *	Looking for a simple name always looks in the current directory,
 *	unless checkCurdirFirst is false.
 *	For complex names, the current directory search only occurs for
 *	paths with dot in them.
 */
extern char *Dir_FindFileComplexi(const char *, const char *, Lst, bool);
#define Dir_FindFilei(n, e, p) Dir_FindFileComplexi(n, e, p, true)
#define Dir_FindFileNoDoti(n, e, p) Dir_FindFileComplexi(n, e, p, false)
#define Dir_FindFile(n, p) Dir_FindFilei(n, strchr(n, '\0'), p)
#define Dir_FindFileNoDot(n, p) Dir_FindFileNoDoti(n, strchr(n, '\0'), p)

/* stamp = Dir_MTime(gn);
 *	Return the modification time of node gn, searching along
 *	the default search path. 
 *	Side effect: the path and mtime fields of gn are filled in.
 *	Return specific value if file can't be found, to be tested by
 *	is_out_of_date().
 */
extern TIMESTAMP Dir_MTime(GNode *);




/* 
 * Misc
 */

/* string = Dir_MakeFlags(flag, path);
 *	Given a search path and a command flag, create a string with each 
 *	of the directories in the path preceded by the command flag and all 
 *	of them separated by spaces.
 */
extern char *Dir_MakeFlags(const char *, Lst);


#ifdef DEBUG_DIRECTORY_CACHE
/* Dir_PrintDirectories();
 *	Print stats about the directory cache.
 */
extern void Dir_PrintDirectories(void);
#endif

/* List of directories to search when looking for targets. */
extern Lst	defaultPath;	


/* communication between dir.c and direxpand.c */
struct PathEntry;
extern struct PathEntry *dot;
/* Handles wildcard expansion on a given directory. */
extern  void Dir_MatchFilesi(const char *, const char *, struct PathEntry *, 
    Lst);
#endif /* DIR_H */
