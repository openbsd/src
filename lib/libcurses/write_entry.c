/*	$OpenBSD: write_entry.c,v 1.1 1998/07/23 21:20:12 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/



/*
 *	write_entry.c -- write a terminfo structure onto the file system
 */

#include <curses.priv.h>

#include <sys/stat.h>

#include <tic.h>
#include <term.h>
#include <term_entry.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode & S_IFMT) == S_IFDIR)
#endif

MODULE_ID("$From: write_entry.c,v 1.25 1998/07/18 16:52:22 tom Exp $")

static int total_written;

static int write_object(FILE *, TERMTYPE *);

static void write_file(char *filename, TERMTYPE *tp)
{
	FILE *fp = fopen(filename, "wb");
	if (fp == 0) {
		perror(filename);
		_nc_syserr_abort("can't open %s/%s", _nc_tic_dir(0), filename);
	}
	DEBUG(1, ("Created %s", filename));

	if (write_object(fp, tp) == ERR) {
		_nc_syserr_abort("error writing %s/%s", _nc_tic_dir(0), filename);
	}
	fclose(fp);
}

/*
 *	make_directory(char *path)
 *
 *	Make a directory if it doesn't exist.
 */
static int make_directory(const char *path)
{
int	rc;
struct	stat	statbuf;
char	fullpath[PATH_MAX];
const char *destination = _nc_tic_dir(0);

	if (path == destination || *path == '/') {
		if (strlen(path) + 1 > sizeof(fullpath))
			return(-1);
		(void)strcpy(fullpath, path);
	} else {
		if (strlen(destination) + strlen(path) + 2 > sizeof(fullpath))
			return(-1);
		(void)sprintf(fullpath, "%s/%s", destination, path);
	}

	if ((rc = stat(path, &statbuf)) < 0) {
		rc = mkdir(path, 0777);
	} else {
		if (access(path, R_OK|W_OK|X_OK) < 0) {
			rc = -1;	/* permission denied */
		} else if (!(S_ISDIR(statbuf.st_mode))) {
			rc = -1;	/* not a directory */
		}
	}
	return rc;
}

void  _nc_set_writedir(char *dir)
/* set the write directory for compiled entries */
{
    const char *destination;

    if (dir != 0)
	(void) _nc_tic_dir(dir);
    else if (getenv("TERMINFO") != NULL)
	(void) _nc_tic_dir(getenv("TERMINFO"));

    destination = _nc_tic_dir(0);
    if (make_directory(destination) < 0)
    {
	char	*home;

	/* ncurses extension...fall back on user's private directory */
	if ((home = getenv("HOME")) != (char *)NULL &&
	    strlen(home) + sizeof(PRIVATE_INFO) <= PATH_MAX)
	{
	    char *temp = malloc(sizeof(PRIVATE_INFO) + strlen(home));
	    if (temp == NULL)
		_nc_err_abort("Out of memory");
	    (void) sprintf(temp, PRIVATE_INFO, home);
	    destination = temp;

	    if (make_directory(destination) < 0)
		_nc_err_abort("%s: permission denied (errno %d)",
			destination, errno);
	}
    }

    /*
     * Note: because of this code, this logic should be exercised
     * *once only* per run.
     */
    if (chdir(_nc_tic_dir(destination)) < 0)
	_nc_err_abort("%s: not a directory", destination);
}

/*
 *	check_writeable(char code)
 *
 *	Miscellaneous initialisations
 *
 *	Check for access rights to destination directories
 *	Create any directories which don't exist.
 *	Note: there's no reason to return the result of make_directory(), since
 *	this function is called only in instances where that has to succeed.
 *
 */

static void check_writeable(int code)
{
static const char dirnames[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static bool verified[sizeof(dirnames)];

char		dir[2];
char		*s;

	if (code == 0 || (s = strchr(dirnames, code)) == 0)
	    _nc_err_abort("Illegal terminfo subdirectory \"%c\"", code);

	if (verified[s-dirnames])
	    return;

	dir[0] = code;
	dir[1] = '\0';
	if (make_directory(dir) < 0) {
		_nc_err_abort("%s/%s: permission denied", _nc_tic_dir(0), dir);
	}

	verified[s-dirnames] = TRUE;
}

/*
 *	_nc_write_entry()
 *
 *	Save the compiled version of a description in the filesystem.
 *
 *	make a copy of the name-list
 *	break it up into first-name and all-but-last-name
 *	creat(first-name)
 *	write object information to first-name
 *	close(first-name)
 *      for each name in all-but-last-name
 *	    link to first-name
 *
 *	Using 'time()' to obtain a reference for file timestamps is unreliable,
 *	e.g., with NFS, because the filesystem may have a different time
 *	reference.  We check for pre-existence of links by latching the first
 *	timestamp from a file that we create.
 *
 *	The _nc_warning() calls will report a correct line number only if
 *	_nc_curr_line is properly set before the write_entry() call.
 */

void _nc_write_entry(TERMTYPE *const tp)
{
struct stat	statbuf;
char		name_list[MAX_TERMINFO_LENGTH];
char		*first_name, *other_names;
char		*ptr;
char		filename[PATH_MAX];
char		linkname[PATH_MAX];
#if USE_SYMLINKS
char		symlinkname[PATH_MAX];
#endif /* USE_SYMLINKS */
static int	call_count;
static time_t	start_time;		/* time at start of writes */

	if (call_count++ == 0) {
		start_time = 0;
	}

	(void) strcpy(name_list, tp->term_names);
	DEBUG(7, ("Name list = '%s'", name_list));

	first_name = name_list;

	ptr = &name_list[strlen(name_list) - 1];
	other_names = ptr + 1;

	while (ptr > name_list  &&  *ptr != '|')
		ptr--;

	if (ptr != name_list) {
		*ptr = '\0';

		for (ptr = name_list; *ptr != '\0' && *ptr != '|'; ptr++)
			continue;

		if (*ptr == '\0')
			other_names = ptr;
		else {
			*ptr = '\0';
			other_names = ptr + 1;
		}
	}

	DEBUG(7, ("First name = '%s'", first_name));
	DEBUG(7, ("Other names = '%s'", other_names));

	_nc_set_type(first_name);

	if (strlen(first_name) > sizeof(filename)-3)
		_nc_warning("terminal name too long.");

	sprintf(filename, "%c/%s", first_name[0], first_name);

	/*
	 * Has this primary name been written since the first call to
	 * write_entry()?  If so, the newer write will step on the older,
	 * so warn the user.
	 */
	if (start_time > 0 &&
	    stat(filename, &statbuf) >= 0
	    && statbuf.st_mtime >= start_time)
	{
		_nc_warning("name multiply defined.");
	}

	check_writeable(first_name[0]);
	write_file(filename, tp);

	if (start_time == 0) {
		if (stat(filename, &statbuf) < 0
		 || (start_time = statbuf.st_mtime) == 0) {
			_nc_syserr_abort("error obtaining time from %s/%s",
				_nc_tic_dir(0), filename);
		}
	}
	while (*other_names != '\0') {
		ptr = other_names++;
		while (*other_names != '|'  &&	*other_names != '\0')
			other_names++;

		if (*other_names != '\0')
			*(other_names++) = '\0';

		if (strlen(ptr) > sizeof(linkname)-3) {
			_nc_warning("terminal alias %s too long.", ptr);
			continue;
		}
		if (strchr(ptr, '/') != 0) {
			_nc_warning("cannot link alias %s.", ptr);
			continue;
		}

		check_writeable(ptr[0]);
		sprintf(linkname, "%c/%s", ptr[0], ptr);

		if (strcmp(filename, linkname) == 0) {
			_nc_warning("self-synonym ignored");
		}
		else if (stat(linkname, &statbuf) >= 0	&&
						statbuf.st_mtime < start_time)
		{
			_nc_warning("alias %s multiply defined.", ptr);
		}
		else
#if HAVE_LINK
		{
#if USE_SYMLINKS
			strcpy(symlinkname, "../");
			strncat(symlinkname, filename, sizeof(symlinkname) - 4);
			symlinkname[sizeof(symlinkname) - 1] = '\0';
#endif /* USE_SYMLINKS */
#if HAVE_REMOVE
			remove(linkname);
#else
			unlink(linkname);
#endif
#if USE_SYMLINKS
			if (symlink(symlinkname, linkname) < 0)
#else
			if (link(filename, linkname) < 0)
#endif /* USE_SYMLINKS */
			    _nc_syserr_abort("can't link %s to %s", filename, linkname);
			DEBUG(1, ("Linked %s", linkname));
		}
#else /* just make copies */
		write_file(linkname, tp);
#endif /* HAVE_LINK */
	}
}

#undef LITTLE_ENDIAN	/* BSD/OS defines this as a feature macro */
#define HI(x)			((x) / 256)
#define LO(x)			((x) % 256)
#define LITTLE_ENDIAN(p, x)	(p)[0] = LO(x), (p)[1] = HI(x)

static int write_object(FILE *fp, TERMTYPE *tp)
{
char		*namelist;
size_t		namelen, boolmax, nummax, strmax;
char		zero = '\0';
size_t		i;
short		nextfree;
short		offsets[STRCOUNT];
unsigned char	buf[MAX_ENTRY_SIZE];

	namelist = tp->term_names;
	namelen = strlen(namelist) + 1;

	boolmax = 0;
	for (i = 0; i < BOOLWRITE; i++)
		if (tp->Booleans[i])
			boolmax = i+1;

	nummax = 0;
	for (i = 0; i < NUMWRITE; i++)
		if (tp->Numbers[i] != ABSENT_NUMERIC)
			nummax = i+1;

	strmax = 0;
	for (i = 0; i < STRWRITE; i++)
		if (tp->Strings[i] != ABSENT_STRING)
			strmax = i+1;

	nextfree = 0;
	for (i = 0; i < strmax; i++)
	    if (tp->Strings[i] == ABSENT_STRING)
		offsets[i] = -1;
	    else if (tp->Strings[i] == CANCELLED_STRING)
		offsets[i] = -2;
	    else
	    {
		offsets[i] = nextfree;
		nextfree += strlen(tp->Strings[i]) + 1;
	    }

	/* fill in the header */
	LITTLE_ENDIAN(buf,    MAGIC);
	LITTLE_ENDIAN(buf+2,  min(namelen, MAX_NAME_SIZE + 1));
	LITTLE_ENDIAN(buf+4,  boolmax);
	LITTLE_ENDIAN(buf+6,  nummax);
	LITTLE_ENDIAN(buf+8,  strmax);
	LITTLE_ENDIAN(buf+10, nextfree);

	/* write out the header */
	if (fwrite(buf, 12, 1, fp) != 1
		||  fwrite(namelist, sizeof(char), (size_t)namelen, fp) != namelen
		||  fwrite(tp->Booleans, sizeof(char), (size_t)boolmax, fp) != boolmax)
		return(ERR);

	/* the even-boundary padding byte */
	if ((namelen+boolmax) % 2 != 0 &&  fwrite(&zero, sizeof(char), 1, fp) != 1)
		return(ERR);

#ifdef SHOWOFFSET
	(void) fprintf(stderr, "Numerics begin at %04lx\n", ftell(fp));
#endif /* SHOWOFFSET */

	/* the numerics */
	for (i = 0; i < nummax; i++)
	{
		if (tp->Numbers[i] == -1)	/* HI/LO won't work */
			buf[2*i] = buf[2*i + 1] = 0377;
		else if (tp->Numbers[i] == -2)	/* HI/LO won't work */
			buf[2*i] = 0376, buf[2*i + 1] = 0377;
		else
			LITTLE_ENDIAN(buf + 2*i, tp->Numbers[i]);
	}
	if (fwrite(buf, 2, (size_t)nummax, fp) != nummax)
		return(ERR);

#ifdef SHOWOFFSET
	(void) fprintf(stderr, "String offets begin at %04lx\n", ftell(fp));
#endif /* SHOWOFFSET */

	/* the string offsets */
	for (i = 0; i < strmax; i++)
		if (offsets[i] == -1)	/* HI/LO won't work */
			buf[2*i] = buf[2*i + 1] = 0377;
		else if (offsets[i] == -2)	/* HI/LO won't work */
		{
			buf[2*i] = 0376;
			buf[2*i + 1] = 0377;
		}
		else
			LITTLE_ENDIAN(buf + 2*i, offsets[i]);
	if (fwrite(buf, 2, (size_t)strmax, fp) != strmax)
		return(ERR);

#ifdef SHOWOFFSET
	(void) fprintf(stderr, "String table begins at %04lx\n", ftell(fp));
#endif /* SHOWOFFSET */

	/* the strings */
	for (i = 0; i < strmax; i++)
	    if (tp->Strings[i] != ABSENT_STRING && tp->Strings[i] != CANCELLED_STRING)
		if (fwrite(tp->Strings[i], sizeof(char), strlen(tp->Strings[i]) + 1, fp) != strlen(tp->Strings[i]) + 1)
		    return(ERR);

	total_written++;
	return(OK);
}

/*
 * Returns the total number of entries written by this process
 */
int _nc_tic_written(void)
{
	return total_written;
}
