/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fnmatch.h>


/* Expand wildcards in argv.  We probably should be expanding wildcards
   via expand_wild instead; that way we could expand only filenames and
   not tag names and the like.  */

void
os2_initialize (pargc, pargv)
    int *pargc;
    char **pargv[];
{
    _wildcard (pargc, pargv);
}


/* Modifies 'stat' so that always the same inode is returned.  EMX never
   returns the same value for st_ino.  Without this modification,
   release_delete in module src/release.c refuses to work.  Care must
   be taken if someone is using the value of st_ino (but as far as I know,
   no callers are).  */

int
os2_stat (name, buffer)
    const char *name;
    struct stat *buffer;
{
    int rc = stat (name, buffer);

    /* There are no inodes on OS/2.  */
    buffer->st_ino = 42;

    return rc;
}


/* We must not only change the directory, but also the current drive.
   Otherwise it is be impossible to have the working directory and the
   repository on different drives.  */

int
os2_chdir (name)
    const char *name;
{
    return _chdir2 (name);
}


/* getwd must return a drive specification.  */

char *
xgetwd ()
{
    return _getcwd2 (NULL, 1);
}


/* fnmatch must recognize OS/2 filename conventions: Filename case
   must be preserved, but ignored in searches.  It would perhaps be better
   to just have CVS pick how to match based on FILENAMES_CASE_INSENSITIVE
   or something rather than having an OS/2-specific version of CVS_FNMATCH.
   Note that lib/fnmatch.c uses FOLD_FN_CHAR; that is how we get
   case-insensitivity on NT (and VMS, I think).  */

#define _FNM_OS2           1
#define _FNM_IGNORECASE    128

int
os2_fnmatch (pattern, name, flags)
    const char *pattern;
    const char *name;
    int flags;
{
    return fnmatch (pattern, name, _FNM_IGNORECASE | _FNM_OS2 | flags);
}
