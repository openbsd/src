/*	$OpenBSD: lndir.c,v 1.3 1996/08/19 06:50:16 downsj Exp $	*/
/* $XConsortium: lndir.c /main/15 1995/08/30 10:56:18 gildea $ */
/* Create shadow link tree (after X11R4 script of the same name)
   Mark Reinhold (mbr@lcs.mit.edu)/3 January 1990 */

/* 
Copyright (c) 1990,  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

/* From the original /bin/sh script:

  Used to create a copy of the a directory tree that has links for all
  non-directories (except those named RCS, SCCS or CVS.adm).  If you are
  building the distribution on more than one machine, you should use
  this technique.

  If your master sources are located in /usr/local/src/X and you would like
  your link tree to be in /usr/local/src/new-X, do the following:

   	%  mkdir /usr/local/src/new-X
	%  cd /usr/local/src/new-X
   	%  lndir ../X
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <string.h>

int silent = 0;			/* -silent */
int ignore_links = 0;		/* -ignorelinks */

char *rcurdir;
char *curdir;

struct except {
	char *name;

	struct except *next;
};
struct except *exceptions = (struct except *)NULL;

void
quit (int code, char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf (stderr, fmt, args);
    va_end(args);
    putc ('\n', stderr);
    exit (code);
}

void
quiterr (code, s)
    char *s;
{
    perror (s);
    exit (code);
}

void
msg (char * fmt, ...)
{
    va_list args;
    if (curdir) {
	fprintf (stderr, "%s:\n", curdir);
	curdir = 0;
    }
    va_start(args, fmt);
    vfprintf (stderr, fmt, args);
    va_end(args);
    putc ('\n', stderr);
}

void
mperror (s)
    char *s;
{
    if (curdir) {
	fprintf (stderr, "%s:\n", curdir);
	curdir = 0;
    }
    perror (s);
}


int equivalent(lname, rname)
    char *lname;
    char *rname;
{
    char *s;

    if (!strcmp(lname, rname))
	return 1;
    for (s = lname; *s && (s = strchr(s, '/')); s++) {
	while (s[1] == '/')
	    strcpy(s+1, s+2);
    }
    return !strcmp(lname, rname);
}


addexcept(name)
    char *name;
{
    struct except *new;

    new = (struct except *)malloc(sizeof(struct except));
    if (new == (struct except *)NULL)
        quiterr(1, "addexcept");
    new->name = strdup(name);
    if (new->name == (char *)NULL)
        quiterr(1, "addexcept");
    
    new->next = exceptions;
    exceptions = new;
}


/* Recursively create symbolic links from the current directory to the "from"
   directory.  Assumes that files described by fs and ts are directories. */

dodir (fn, fs, ts, rel)
char *fn;			/* name of "from" directory, either absolute or
				   relative to cwd */
struct stat *fs, *ts;		/* stats for the "from" directory and cwd */
int rel;			/* if true, prepend "../" to fn before using */
{
    DIR *df;
    struct dirent *dp;
    char buf[MAXPATHLEN + 1], *p;
    char symbuf[MAXPATHLEN + 1];
    char basesym[MAXPATHLEN + 1];
    struct stat sb, sc;
    int n_dirs;
    int symlen;
    int basesymlen = -1;
    char *ocurdir;
    struct except *cur;

    if ((fs->st_dev == ts->st_dev) && (fs->st_ino == ts->st_ino)) {
	msg ("%s: From and to directories are identical!", fn);
	return 1;
    }

    if (rel)
	strcpy (buf, "../");
    else
	buf[0] = '\0';
    strcat (buf, fn);
    
    if (!(df = opendir (buf))) {
	msg ("%s: Cannot opendir", buf);
	return 1;
    }

    p = buf + strlen (buf);
    *p++ = '/';
    n_dirs = fs->st_nlink;
    while (dp = readdir (df)) {
	if (dp->d_name[strlen(dp->d_name) - 1] == '~')
	    continue;
	for (cur = exceptions; cur != (struct except *)NULL;
	     cur = cur->next) {
	    if (!strcmp (dp->d_name, cur->name))
	    	goto next;	/* can't continue */
	}
	strcpy (p, dp->d_name);

	if (n_dirs > 0) {
	    if (stat (buf, &sb) < 0) {
		mperror (buf);
		continue;
	    }

	    if(S_ISDIR(sb.st_mode)) {
		/* directory */
		n_dirs--;
		if (dp->d_name[0] == '.' &&
		    (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' &&
					       dp->d_name[2] == '\0')))
		    continue;
		if (!strcmp (dp->d_name, "RCS"))
		    continue;
		if (!strcmp (dp->d_name, "SCCS"))
		    continue;
		if (!strcmp (dp->d_name, "CVS"))
		    continue;
		if (!strcmp (dp->d_name, "CVS.adm"))
		    continue;
		ocurdir = rcurdir;
		rcurdir = buf;
		curdir = silent ? buf : (char *)0;
		if (!silent)
		    printf ("%s:\n", buf);
		if ((stat (dp->d_name, &sc) < 0) && (errno == ENOENT)) {
		    if (mkdir (dp->d_name, 0777) < 0 ||
			stat (dp->d_name, &sc) < 0) {
			mperror (dp->d_name);
			curdir = rcurdir = ocurdir;
			continue;
		    }
		}
		if (readlink (dp->d_name, symbuf, sizeof(symbuf) - 1) >= 0) {
		    msg ("%s: is a link instead of a directory", dp->d_name);
		    curdir = rcurdir = ocurdir;
		    continue;
		}
		if (chdir (dp->d_name) < 0) {
		    mperror (dp->d_name);
		    curdir = rcurdir = ocurdir;
		    continue;
		}
		dodir (buf, &sb, &sc, (buf[0] != '/'));
		if (chdir ("..") < 0)
		    quiterr (1, "..");
		curdir = rcurdir = ocurdir;
		continue;
	    }
	}

	/* non-directory */
	symlen = readlink (dp->d_name, symbuf, sizeof(symbuf) - 1);
	if (symlen >= 0)
	    symbuf[symlen] = '\0';

	/* The option to ignore links exists mostly because
	   checking for them slows us down by 10-20%.
	   But it is off by default because this really is a useful check. */
	if (!ignore_links) {
	    /* see if the file in the base tree was a symlink */
	    basesymlen = readlink(buf, basesym, sizeof(basesym) - 1);
	    if (basesymlen >= 0)
		basesym[basesymlen] = '\0';
	}

	if (symlen >= 0) {
	    /* Link exists in new tree.  Print message if it doesn't match. */
	    if (!equivalent (basesymlen>=0 ? basesym : buf, symbuf))
		msg ("%s: %s", dp->d_name, symbuf);
	} else {
	    if (symlink (basesymlen>=0 ? basesym : buf, dp->d_name) < 0)
		mperror (dp->d_name);
	}
next:
    }

    closedir (df);
    return 0;
}


main (ac, av)
int ac;
char **av;
{
    char *prog_name = av[0];
    char *fn, *tn;
    struct stat fs, ts;

    while (++av, --ac) {
	if ((strcmp(*av, "-silent") == 0) || (strcmp(*av, "-s") == 0))
	    silent = 1;
	else if ((strcmp(*av, "-ignorelinks") == 0) || (strcmp(*av, "-i") == 0))
	    ignore_links = 1;
	else if (strcmp(*av, "-e") == 0) {
	    ++av, --ac;

	    if (ac < 2)
	        quit (1, "usage: %s [-e except] [-s] [-i] fromdir [todir]",
		      prog_name);
	    addexcept(*av);
	}
	else if (strcmp(*av, "--") == 0) {
	    ++av, --ac;
	    break;
	}
	else
	    break;
    }

    if (ac < 1 || ac > 2)
	quit (1, "usage: %s [-e except] [-s] [-i] fromdir [todir]",
	      prog_name);

    fn = av[0];
    if (ac == 2)
	tn = av[1];
    else
	tn = ".";

    /* to directory */
    if (stat (tn, &ts) < 0)
	quiterr (1, tn);
    if (!(S_ISDIR(ts.st_mode)))
	quit (2, "%s: Not a directory", tn);
    if (chdir (tn) < 0)
	quiterr (1, tn);

    /* from directory */
    if (stat (fn, &fs) < 0)
	quiterr (1, fn);
    if (!(S_ISDIR(fs.st_mode)))
	quit (2, "%s: Not a directory", fn);

    exit (dodir (fn, &fs, &ts, 0));
}
