/*	$OpenBSD: dir.c,v 1.66 2015/01/23 13:18:40 espie Exp $ */
/*	$NetBSD: dir.c,v 1.14 1997/03/29 16:51:26 christos Exp $	*/

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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
 */

#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ohash.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "lst.h"
#include "memory.h"
#include "buf.h"
#include "gnode.h"
#include "arch.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "timestamp.h"


/*	A search path consists of a Lst of PathEntry structures. A Path
 *	structure has in it the name of the directory and a hash table of all
 *	the files in the directory. This is used to cut down on the number of
 *	system calls necessary to find implicit dependents and their like.
 *	Since these searches are made before any actions are taken, we need not
 *	worry about the directory changing due to creation commands. If this
 *	hampers the style of some makefiles, they must be changed.
 *
 *	A list of all previously-read directories is kept in the
 *	knownDirectories cache.
 *
 *	The need for the caching of whole directories is brought about by
 *	the multi-level transformation code in suff.c, which tends to search
 *	for far more files than regular make does. In the initial
 *	implementation, the amount of time spent performing "stat" calls was
 *	truly astronomical. The problem with hashing at the start is,
 *	of course, that pmake doesn't then detect changes to these directories
 *	during the course of the make. Three possibilities suggest themselves:
 *
 *	    1) just use stat to test for a file's existence. As mentioned
 *	       above, this is very inefficient due to the number of checks
 *	       engendered by the multi-level transformation code.
 *	    2) use readdir() and company to search the directories, keeping
 *	       them open between checks. I have tried this and while it
 *	       didn't slow down the process too much, it could severely
 *	       affect the amount of parallelism available as each directory
 *	       open would take another file descriptor out of play for
 *	       handling I/O for another job. Given that it is only recently
 *	       that UNIX OS's have taken to allowing more than 20 or 32
 *	       file descriptors for a process, this doesn't seem acceptable
 *	       to me.
 *	    3) record the mtime of the directory in the PathEntry structure and
 *	       verify the directory hasn't changed since the contents were
 *	       hashed. This will catch the creation or deletion of files,
 *	       but not the updating of files. However, since it is the
 *	       creation and deletion that is the problem, this could be
 *	       a good thing to do. Unfortunately, if the directory (say ".")
 *	       were fairly large and changed fairly frequently, the constant
 *	       rehashing could seriously degrade performance. It might be
 *	       good in such cases to keep track of the number of rehashes
 *	       and if the number goes over a (small) limit, resort to using
 *	       stat in its place.
 *
 *	An additional thing to consider is that pmake is used primarily
 *	to create C programs and until recently pcc-based compilers refused
 *	to allow you to specify where the resulting object file should be
 *	placed. This forced all objects to be created in the current
 *	directory. This isn't meant as a full excuse, just an explanation of
 *	some of the reasons for the caching used here.
 *
 *	One more note: the location of a target's file is only performed
 *	on the downward traversal of the graph and then only for terminal
 *	nodes in the graph. This could be construed as wrong in some cases,
 *	but prevents inadvertent modification of files when the "installed"
 *	directory for a file is provided in the search path.
 *
 *	Another data structure maintained by this module is an mtime
 *	cache used when the searching of cached directories fails to find
 *	a file. In the past, Dir_FindFile would simply perform an access()
 *	call in such a case to determine if the file could be found using
 *	just the name given. When this hit, however, all that was gained
 *	was the knowledge that the file existed. Given that an access() is
 *	essentially a stat() without the copyout() call, and that the same
 *	filesystem overhead would have to be incurred in Dir_MTime, it made
 *	sense to replace the access() with a stat() and record the mtime
 *	in a cache for when Dir_MTime was actually called.  */


/* several data structures exist to handle caching of directory stuff.
 *
 * There is a global hash of directory names (knownDirectories), and each
 * read directory is kept there as one PathEntry instance. Such a structure
 * only contains the file names.
 *
 * There is a global hash of timestamps (modification times), so care must
 * be taken of giving the right file names to that structure.
 *
 * XXX A set of similar structure should exist at the Target level to properly
 * take care of VPATH issues.
 */


/* each directory is cached into a PathEntry structure. */
struct PathEntry {
	int refCount;		/* ref-counted, can participate to
				 * several paths */
	struct ohash files;	/* hash of name of files in the directory */
	char name[1];		/* directory name */
};

/* PathEntry kept on knownDirectories */
static struct ohash_info dir_info = {
	offsetof(struct PathEntry, name), NULL, hash_calloc, hash_free,
	element_alloc
};

static struct ohash   knownDirectories;	/* cache all open directories */


/* file names kept in a path entry */
static struct ohash_info file_info = {
	0, NULL, hash_calloc, hash_free, element_alloc
};


/* Global structure used to cache mtimes.  XXX We don't cache an mtime
 * before a caller actually looks up for the given time, because of the
 * possibility a caller might update the file and invalidate the cache
 * entry, and we don't look up in this cache except as a last resort.
 */
struct file_stamp {
	struct timespec mtime;		/* time stamp... */
	char name[1];			/* ...for that file.  */
};

static struct ohash mtimes;


static struct ohash_info stamp_info = {
	offsetof(struct file_stamp, name), NULL, hash_calloc, hash_free,
	element_alloc
};



static LIST   theDefaultPath;		/* main search path */
Lst	      defaultPath= &theDefaultPath;
struct PathEntry *dot; 			/* contents of current directory */



/* add_file(path, name): add a file name to a path hash structure. */
static void add_file(struct PathEntry *, const char *);
/* n = find_file_hashi(p, name, end, hv): retrieve name in a path hash
 * 	structure. */
static char *find_file_hashi(struct PathEntry *, const char *, const char *,
    uint32_t);

/* stamp = find_stampi(name, end): look for (name, end) in the global
 *	cache. */
static struct file_stamp *find_stampi(const char *, const char *);
/* record_stamp(name, timestamp): record timestamp for name in the global
 * 	cache. */
static void record_stamp(const char *, struct timespec);

static bool read_directory(struct PathEntry *);
/* p = DirReaddiri(name, end): read an actual directory, caching results
 * 	as we go.  */
static struct PathEntry *create_PathEntry(const char *, const char *);
/* Debugging: show a dir name in a path. */
static void DirPrintDir(void *);

/***
 *** timestamp handling
 ***/

static void
record_stamp(const char *file, struct timespec t)
{
	unsigned int slot;
	const char *end = NULL;
	struct file_stamp *n;

	slot = ohash_qlookupi(&mtimes, file, &end);
	n = ohash_find(&mtimes, slot);
	if (n)
		n->mtime = t;
	else {
		n = ohash_create_entry(&stamp_info, file, &end);
		n->mtime = t;
		ohash_insert(&mtimes, slot, n);
	}
}

static struct file_stamp *
find_stampi(const char *file, const char *efile)
{
	return ohash_find(&mtimes, ohash_qlookupi(&mtimes, file, &efile));
}

/***
 *** PathEntry handling
 ***/

static void
add_file(struct PathEntry *p, const char *file)
{
	unsigned int	slot;
	const char	*end = NULL;
	char		*n;
	struct ohash 	*h = &p->files;

	slot = ohash_qlookupi(h, file, &end);
	n = ohash_find(h, slot);
	if (n == NULL) {
		n = ohash_create_entry(&file_info, file, &end);
		ohash_insert(h, slot, n);
	}
}

static char *
find_file_hashi(struct PathEntry *p, const char *file, const char *efile,
    uint32_t hv)
{
	struct ohash 	*h = &p->files;

	return ohash_find(h, ohash_lookup_interval(h, file, efile, hv));
}

static bool
read_directory(struct PathEntry *p)
{
	DIR *d;
	struct dirent *dp;

	if (DEBUG(DIR)) {
		printf("Caching %s...", p->name);
		fflush(stdout);
	}

	if ((d = opendir(p->name)) == NULL)
		return false;

	ohash_init(&p->files, 4, &file_info);

	while ((dp = readdir(d)) != NULL) {
		if (dp->d_name[0] == '.' &&
		    (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
			continue;
		add_file(p, dp->d_name);
	}
	(void)closedir(d);
	if (DEBUG(DIR))
		printf("done\n");
	return true;
}

/* Read a directory, either from the disk, or from the cache.  */
static struct PathEntry *
create_PathEntry(const char *name, const char *ename)
{
	struct PathEntry *p;
	unsigned int slot;

	slot = ohash_qlookupi(&knownDirectories, name, &ename);
	p = ohash_find(&knownDirectories, slot);

	if (p == NULL) {
		p = ohash_create_entry(&dir_info, name, &ename);
		p->refCount = 0;
		if (!read_directory(p)) {
			free(p);
			return NULL;
		}
		ohash_insert(&knownDirectories, slot, p);
	}
	p->refCount++;
	return p;
}

char *
PathEntry_name(struct PathEntry *p)
{
	return p->name;
}

/* Side Effects: cache the current directory */
void
Dir_Init(void)
{
	char *dotname = ".";

	Static_Lst_Init(defaultPath);
	ohash_init(&knownDirectories, 4, &dir_info);
	ohash_init(&mtimes, 4, &stamp_info);


	dot = create_PathEntry(dotname, dotname+1);

	if (!dot)
		Fatal("Can't access current directory");
}

/*-
 *-----------------------------------------------------------------------
 * Dir_MatchFilesi --
 *	Given a pattern and a PathEntry structure, see if any files
 *	match the pattern and add their names to the 'expansions' list if
 *	any do. This is incomplete -- it doesn't take care of patterns like
 *	src / *src / *.c properly (just *.c on any of the directories), but it
 *	will do for now.
 *-----------------------------------------------------------------------
 */
void
Dir_MatchFilesi(const char *word, const char *eword, struct PathEntry *p,
    Lst expansions)
{
	unsigned int search; 	/* Index into the directory's table */
	const char *entry; 	/* Current entry in the table */

	for (entry = ohash_first(&p->files, &search); entry != NULL;
	     entry = ohash_next(&p->files, &search)) {
		/* See if the file matches the given pattern. We follow the UNIX
		 * convention that dot files will only be found if the pattern
		 * begins with a dot (the hashing scheme doesn't hash . or ..,
		 * so they won't match `.*'.  */
		if (*word != '.' && *entry == '.')
			continue;
		if (Str_Matchi(entry, strchr(entry, '\0'), word, eword))
			Lst_AtEnd(expansions,
			    p == dot  ? estrdup(entry) :
			    Str_concat(p->name, entry, '/'));
	}
}

/*-
 * Side Effects:
 *	If the file is found in a directory which is not on the path
 *	already (either 'name' is absolute or it is a relative path
 *	[ dir1/.../dirn/file ] which exists below one of the directories
 *	already on the search path), its directory is added to the end
 *	of the path on the assumption that there will be more files in
 *	that directory later on.
 */
char *
Dir_FindFileComplexi(const char *name, const char *ename, Lst path,
    bool checkCurdirFirst)
{
	struct PathEntry *p;	/* current path member */
	char *p1;	/* pointer into p->name */
	const char *p2;	/* pointer into name */
	LstNode ln;	/* a list element */
	char *file;	/* the current filename to check */
	char *temp;	/* index into file */
	const char *basename;
	bool hasSlash;
	struct stat stb;/* Buffer for stat, if necessary */
	struct file_stamp *entry;
			/* Entry for mtimes table */
	uint32_t hv;	/* hash value for last component in file name */
	char *q;	/* Str_dupi(name, ename) */

	/* Find the final component of the name and note whether name has a
	 * slash in it */
	basename = Str_rchri(name, ename, '/');
	if (basename) {
		hasSlash = true;
		basename++;
	} else {
		hasSlash = false;
		basename = name;
	}

	hv = ohash_interval(basename, &ename);

	if (DEBUG(DIR))
		printf("Searching for %s...", name);
	/* Unless checkCurDirFirst is false, we always look for
	 * the file in the current directory before anywhere else
	 * and we always return exactly what the caller specified. */
	if (checkCurdirFirst &&
	    (!hasSlash || (basename - name == 2 && *name == '.')) &&
	    find_file_hashi(dot, basename, ename, hv) != NULL) {
		if (DEBUG(DIR))
			printf("in '.'\n");
		return Str_dupi(name, ename);
	}

	/* Then, we look through all the directories on path, seeking one
	 * containing the final component of name and whose final
	 * component(s) match name's initial component(s).
	 * If found, we concatenate the directory name and the
	 * final component and return the resulting string.  */
	for (ln = Lst_First(path); ln != NULL; ln = Lst_Adv(ln)) {
		p = (struct PathEntry *)Lst_Datum(ln);
		if (DEBUG(DIR))
			printf("%s...", p->name);
		if (find_file_hashi(p, basename, ename, hv) != NULL) {
			if (DEBUG(DIR))
				printf("here...");
			if (hasSlash) {
				/* If the name had a slash, its initial
				 * components and p's final components must
				 * match. This is false if a mismatch is
				 * encountered before all of the initial
				 * components have been checked (p2 > name at
				 * the end of the loop), or we matched only
				 * part of one of the components of p along
				 * with all the rest of them (*p1 != '/').  */
				p1 = p->name + strlen(p->name) - 1;
				p2 = basename - 2;
				while (p2 >= name && p1 >= p->name &&
				    *p1 == *p2) {
					p1--;
					p2--;
				}
				if (p2 >= name ||
				    (p1 >= p->name && *p1 != '/')) {
					if (DEBUG(DIR))
						printf("component mismatch -- continuing...");
					continue;
				}
			}
			file = Str_concati(p->name, strchr(p->name, '\0'), basename,
			    ename, '/');
			if (DEBUG(DIR))
				printf("returning %s\n", file);
			return file;
		} else if (hasSlash) {
			/* If the file has a leading path component and that
			 * component exactly matches the entire name of the
			 * current search directory, we assume the file
			 * doesn't exist and return NULL.  */
			for (p1 = p->name, p2 = name; *p1 && *p1 == *p2;
			    p1++, p2++)
				continue;
			if (*p1 == '\0' && p2 == basename - 1) {
				if (DEBUG(DIR))
					printf("has to be here but isn't -- returning NULL\n");
				return NULL;
			}
		}
	}

	/* We didn't find the file on any existing member of the path.
	 * If the name doesn't contain a slash, end of story.
	 * If it does contain a slash, however, it could be in a subdirectory
	 * of one of the members of the search path. (eg., for path=/usr/include
	 * and name=sys/types.h, the above search fails to turn up types.h
	 * in /usr/include, even though /usr/include/sys/types.h exists).
	 *
	 * We only perform this look-up for non-absolute file names.
	 *
	 * Whenever we score a hit, we assume there will be more matches from
	 * that directory, and append all but the last component of the
	 * resulting name onto the search path. */
	if (!hasSlash) {
		if (DEBUG(DIR))
			printf("failed.\n");
		return NULL;
	}

	if (*name != '/') {
		bool checkedDot = false;

		if (DEBUG(DIR))
			printf("failed. Trying subdirectories...");
		for (ln = Lst_First(path); ln != NULL; ln = Lst_Adv(ln)) {
			p = (struct PathEntry *)Lst_Datum(ln);
			if (p != dot)
				file = Str_concati(p->name,
				    strchr(p->name, '\0'), name, ename, '/');
			else {
				/* Checking in dot -- DON'T put a leading
				* ./ on the thing.  */
				file = Str_dupi(name, ename);
				checkedDot = true;
			}
			if (DEBUG(DIR))
				printf("checking %s...", file);

			if (stat(file, &stb) == 0) {
				struct timespec mtime;

				ts_set_from_stat(stb, mtime);
				if (DEBUG(DIR))
					printf("got it.\n");

				/* We've found another directory to search.
				 * We know there is a slash in 'file'. We
				 * call Dir_AddDiri to add the new directory
				 * onto the existing search path. Once that's
				 * done, we return the file name, knowing that
				 * should a file in this directory ever be
				 * referenced again in such a manner, we will
				 * find it without having to do numerous
				 * access calls.  */
				temp = strrchr(file, '/');
				Dir_AddDiri(path, file, temp);

				/* Save the modification time so if it's
				* needed, we don't have to fetch it again.  */
				if (DEBUG(DIR))
					printf("Caching %s for %s\n",
					    time_to_string(&mtime), file);
				record_stamp(file, mtime);
				return file;
			} else
				free(file);
		}

		if (DEBUG(DIR))
			printf("failed. ");

		if (checkedDot) {
			/* Already checked by the given name, since . was in
			 * the path, so no point in proceeding...  */
			if (DEBUG(DIR))
				printf("Checked . already, returning NULL\n");
			return NULL;
		}
	}

	/* Didn't find it that way, either. Last resort: look for the file
	 * in the global mtime cache, then on the disk.
	 * If this doesn't succeed, we finally return a NULL pointer.
	 *
	 * We cannot add this directory onto the search path because
	 * of this amusing case:
	 * $(INSTALLDIR)/$(FILE): $(FILE)
	 *
	 * $(FILE) exists in $(INSTALLDIR) but not in the current one.
	 * When searching for $(FILE), we will find it in $(INSTALLDIR)
	 * b/c we added it here. This is not good...  */
	q = Str_dupi(name, ename);
	if (DEBUG(DIR))
		printf("Looking for \"%s\"...", q);

	entry = find_stampi(name, ename);
	if (entry != NULL) {
		if (DEBUG(DIR))
			printf("got it (in mtime cache)\n");
		return q;
	} else if (stat(q, &stb) == 0) {
		struct timespec mtime;

		ts_set_from_stat(stb, mtime);
		if (DEBUG(DIR))
			printf("Caching %s for %s\n", time_to_string(&mtime), 
			    q);
		record_stamp(q, mtime);
		return q;
	} else {
	    if (DEBUG(DIR))
		    printf("failed. Returning NULL\n");
	    free(q);
	    return NULL;
	}
}

void
Dir_AddDiri(Lst path, const char *name, const char *ename)
{
	struct PathEntry	*p;

	p = create_PathEntry(name, ename);
	if (p == NULL)
		return;
	if (p->refCount == 1)
		Lst_AtEnd(path, p);
	else if (!Lst_AddNew(path, p))
		return;
}

void *
Dir_CopyDir(void *p)
{
	struct PathEntry *q = p;
	q->refCount++;
	return p;
}

void
Dir_Destroy(void *pp)
{
	struct PathEntry *p = pp;

	if (--p->refCount == 0) {
		ohash_remove(&knownDirectories,
		    ohash_qlookup(&knownDirectories, p->name));
		free_hash(&p->files);
		free(p);
	}
}

/*-
 *-----------------------------------------------------------------------
 * Dir_Concat --
 *	Concatenate two paths, adding the second to the end of the first.
 *	Makes sure to avoid duplicates.
 *
 * Side Effects:
 *	Reference counts for added dirs are upped.
 *-----------------------------------------------------------------------
 */
void
Dir_Concat(Lst path1, Lst path2)
{
	LstNode	ln;
	struct PathEntry *p;

	for (ln = Lst_First(path2); ln != NULL; ln = Lst_Adv(ln)) {
		p = (struct PathEntry *)Lst_Datum(ln);
		if (Lst_AddNew(path1, p))
			p->refCount++;
	}
}

static void
DirPrintDir(void *p)
{
	struct PathEntry *q = p;
	printf("%s ", q->name);
}

void
Dir_PrintPath(Lst path)
{
	Lst_Every(path, DirPrintDir);
}

struct timespec
Dir_MTime(GNode *gn)
{
	char *fullName;
	struct stat stb;
	struct file_stamp *entry;
	unsigned int slot;
	struct timespec	  mtime;

	if (gn->type & OP_PHONY)
		return gn->mtime;

	if (gn->type & OP_ARCHV)
		return Arch_MTime(gn);

	if (gn->path == NULL) {
		fullName = Dir_FindFile(gn->name, defaultPath);
		if (fullName == NULL)
			fullName = estrdup(gn->name);
	} else
		fullName = gn->path;

	slot = ohash_qlookup(&mtimes, fullName);
	entry = ohash_find(&mtimes, slot);
	if (entry != NULL) {
		/* Only do this once -- the second time folks are checking to
		 * see if the file was actually updated, so we need to
		 * actually go to the file system.	*/
		if (DEBUG(DIR))
			printf("Using cached time %s for %s\n",
			    time_to_string(&entry->mtime), fullName);
		mtime = entry->mtime;
		free(entry);
		ohash_remove(&mtimes, slot);
	} else if (stat(fullName, &stb) == 0)
		ts_set_from_stat(stb, mtime);
	else {
		if (gn->type & OP_MEMBER) {
			if (fullName != gn->path)
				free(fullName);
			return Arch_MemMTime(gn);
		} else
			ts_set_out_of_date(mtime);
	}
	if (fullName && gn->path == NULL)
		gn->path = fullName;

	gn->mtime = mtime;
	return gn->mtime;
}

