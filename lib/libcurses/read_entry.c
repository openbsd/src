/*	$OpenBSD: read_entry.c,v 1.2 1998/07/27 03:37:34 millert Exp $	*/

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
 *	read_entry.c -- Routine for reading in a compiled terminfo file
 *
 */

#include <curses.priv.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <term.h>
#include <tic.h>

MODULE_ID("$From: read_entry.c,v 1.40 1998/07/25 20:11:11 tom Exp $")

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 *	int
 *	_nc_read_file_entry(filename, ptr)
 *
 *	Read the compiled terminfo entry in the given file into the
 *	structure pointed to by ptr, allocating space for the string
 *	table.
 */

#undef  BYTE
#define BYTE(p,n)	(unsigned char)((p)[n])

#define IS_NEG1(p)	((BYTE(p,0) == 0377) && (BYTE(p,1) == 0377))
#define IS_NEG2(p)	((BYTE(p,0) == 0376) && (BYTE(p,1) == 0377))
#define LOW_MSB(p)	(BYTE(p,0) + 256*BYTE(p,1))

static bool have_tic_directory = FALSE;

/*
 * Record the "official" location of the terminfo directory, according to
 * the place where we're writing to, or the normal default, if not.
 */
const char *_nc_tic_dir(const char *path)
{
	static const char *result = TERMINFO;

	if (path != 0) {
		result = path;
		have_tic_directory = TRUE;
	} else if (!have_tic_directory) {
		char *envp;
		if ((envp = getenv("TERMINFO")) != 0)
			return _nc_tic_dir(envp);
	}
	return result;
}

int _nc_read_file_entry(const char *const filename, TERMTYPE *ptr)
/* return 1 if read, 0 if not found or garbled */
{
    int		name_size, bool_count, num_count, str_count, str_size;
    int		i, fd, numread;
    char	buf[MAX_ENTRY_SIZE];

    if (_nc_access(filename, R_OK) < 0
     || (fd = open(filename, O_RDONLY|O_BINARY)) < 0)
	return(0);

    T(("read terminfo %s", filename));

    /* grab the header */
    (void) read(fd, buf, 12);
    if (LOW_MSB(buf) != MAGIC)
    {
	close(fd);
	return(0);
    }
    name_size  = LOW_MSB(buf + 2);
    bool_count = LOW_MSB(buf + 4);
    num_count  = LOW_MSB(buf + 6);
    str_count  = LOW_MSB(buf + 8);
    str_size   = LOW_MSB(buf + 10);

    if (str_size)
    {
	/* try to allocate space for the string table */
	ptr->str_table = malloc((unsigned)str_size);
	if (ptr->str_table == 0)
	{
	    close(fd);
	    return(0);
	}
    }

    /* grab the name */
    read(fd, buf, min(MAX_NAME_SIZE, (unsigned)name_size));
    buf[MAX_NAME_SIZE] = '\0';
    ptr->term_names = calloc(strlen(buf) + 1, sizeof(char));
    if (ptr->term_names == NULL) {
	if (str_size)
	    free(ptr->str_table);
	    close(fd);
	    return(0);
    }
    (void) strcpy(ptr->term_names, buf);
    if (name_size > MAX_NAME_SIZE)
	lseek(fd, (off_t) (name_size - MAX_NAME_SIZE), 1);

    /* grab the booleans */
    read(fd, ptr->Booleans, min(BOOLCOUNT, (unsigned)bool_count));
    if (bool_count > BOOLCOUNT)
	lseek(fd, (off_t) (bool_count - BOOLCOUNT), 1);
    else
	for (i=bool_count; i < BOOLCOUNT; i++)
	    ptr->Booleans[i] = 0;

    /*
     * If booleans end on an odd byte, skip it.  The machine they
     * originally wrote terminfo on must have been a 16-bit
     * word-oriented machine that would trap out if you tried a
     * word access off a 2-byte boundary.
     */
    if ((name_size + bool_count) % 2 != 0)
	read(fd, buf, 1);

    /* grab the numbers */
    (void) read(fd, buf, min(NUMCOUNT*2, (unsigned)num_count*2));
    for (i = 0; i < min(num_count, NUMCOUNT); i++)
    {
	if (IS_NEG1(buf + 2*i))
	    ptr->Numbers[i] = ABSENT_NUMERIC;
	else if (IS_NEG2(buf + 2*i))
	    ptr->Numbers[i] = CANCELLED_NUMERIC;
	else
	    ptr->Numbers[i] = LOW_MSB(buf + 2*i);
    }
    if (num_count > NUMCOUNT)
	lseek(fd, (off_t) (2 * (num_count - NUMCOUNT)), 1);
    else
	for (i=num_count; i < NUMCOUNT; i++)
	    ptr->Numbers[i] = ABSENT_NUMERIC;

    if (str_count)
    {
	if (str_count*2 >= MAX_ENTRY_SIZE)
	{
	    close(fd);
	    return(0);
	}
	/* grab the string offsets */
	numread = read(fd, buf, (unsigned)(str_count*2));
	if (numread < str_count*2)
	{
	    close(fd);
	    return(0);
	}
	for (i = 0; i < numread/2; i++)
	{
	    if (i >= STRCOUNT)
		break;
	    if (IS_NEG1(buf + 2*i))
		ptr->Strings[i] = ABSENT_STRING;
	    else if (IS_NEG2(buf + 2*i))
		ptr->Strings[i] = CANCELLED_STRING;
	    else
		ptr->Strings[i] = (LOW_MSB(buf+2*i) + ptr->str_table);
	}
    }

    if (str_count > STRCOUNT)
	lseek(fd, (off_t) (2 * (str_count - STRCOUNT)), 1);
    else
	for (i = str_count; i < STRCOUNT; i++)
	    ptr->Strings[i] = ABSENT_STRING;

    if (str_size)
    {
	/* finally, grab the string table itself */
	numread = read(fd, ptr->str_table, (unsigned)str_size);
	if (numread != str_size)
	{
	    close(fd);
	    return(0);
	}
    }

    close(fd);
    return(1);
}

/*
 * Build a terminfo pathname and try to read the data.  Returns 1 on success,
 * 0 on failure.
 */
static int _nc_read_tic_entry(char *const filename,
	const char *const dir, const char *ttn, TERMTYPE *const tp)
{
/* maximum safe length of terminfo root directory name */
#define MAX_TPATH	(PATH_MAX - MAX_ALIAS - 6)

	if (strlen(dir) > MAX_TPATH)
		return 0;
	(void) sprintf(filename, "%s/%s", dir, ttn);
	return _nc_read_file_entry(filename, tp);
}

/*
 * Process the list of :-separated directories, looking for the terminal type.
 * We don't use strtok because it does not show us empty tokens.
 */
static int _nc_read_terminfo_dirs(const char *dirs, char *const filename, const char *const ttn, TERMTYPE *const tp)
{
	char *list, *a;
	const char *b;
	int code = 0;

	/* we'll modify the argument, so we must copy */
	if ((b = a = list = malloc(strlen(dirs) + 1)) == NULL)
		return(0);
	(void) strcpy(list, dirs);

	for (;;) {
		int c = *a;
		if (c == 0 || c == ':') {
			*a = 0;
			if ((b + 1) >= a)
				b = TERMINFO;
			if (_nc_read_tic_entry(filename, b, ttn, tp) == 1) {
				code = 1;
				break;
			}
			b = a + 1;
			if (c == 0)
				break;
		}
		a++;
	}

	free(list);
	return(code);
}

/*
 *	_nc_read_entry(char *tn, char *filename, TERMTYPE *tp)
 *
 *	Find and read the compiled entry for a given terminal type,
 *	if it exists.  We take pains here to make sure no combination
 *	of environment variables and terminal type name can be used to
 *	overrun the file buffer.
 */

int _nc_read_entry(const char *const tn, char *const filename, TERMTYPE *const tp)
{
char		*envp;
char		ttn[MAX_ALIAS + 3];

	/* truncate the terminal name to prevent dangerous buffer airline */
	(void) sprintf(ttn, "%c/%.*s", *tn, MAX_ALIAS, tn);

	/* This is System V behavior, in conjunction with our requirements for
	 * writing terminfo entries.
	 */
	if (have_tic_directory
	 && _nc_read_tic_entry(filename, _nc_tic_dir(0), ttn, tp) == 1)
		return 1;

	if ((envp = getenv("TERMINFO")) != 0
	 && _nc_read_tic_entry(filename, _nc_tic_dir(envp), ttn, tp) == 1)
		return 1;

	/* this is an ncurses extension */
	if ((envp = getenv("HOME")) != 0 &&
	    strlen(envp) + sizeof(PRIVATE_INFO) <= PATH_MAX)
	{
		char *home = malloc(strlen(envp) + sizeof(PRIVATE_INFO));

		if (home == 0)
			return(0);
		(void) sprintf(home, PRIVATE_INFO, envp);
		if (_nc_read_tic_entry(filename, home, ttn, tp) == 1) {
			free(home);
			return(1);
		}
		free(home);
	}

	/* this is an ncurses extension */
	if ((envp = getenv("TERMINFO_DIRS")) != 0)
		return _nc_read_terminfo_dirs(envp, filename, ttn, tp);

	/* Try the system directory.  Note that the TERMINFO_DIRS value, if
	 * defined by the configure script, begins with a ":", which will be
	 * interpreted as TERMINFO.
	 */
#ifdef TERMINFO_DIRS
	return _nc_read_terminfo_dirs(TERMINFO_DIRS, filename, ttn, tp);
#else
	return _nc_read_tic_entry(filename, TERMINFO, ttn, tp);
#endif
}

