/*
 * ls.c --- list directories
 * 
 * Copyright (C) 1997 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>

#include "debugfs.h"

/*
 * list directory
 */

#define LONG_OPT	0x0001

struct list_dir_struct {
	FILE	*f;
	int	col;
	int	options;
};

static const char *monstr[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
					
static void ls_l_file(struct list_dir_struct *ls, char *name, ino_t ino)
{
	struct ext2_inode	inode;
	errcode_t		retval;
	struct tm		*tm_p;
	time_t			modtime;
	char			datestr[80];

	retval = ext2fs_read_inode(current_fs, ino, &inode);
	if (retval) {
		fprintf(ls->f, "%5d --- error ---  %s\n", name);
		return;
	}
	modtime = inode.i_mtime;
	tm_p = localtime(&modtime);
	sprintf(datestr, "%2d-%s-%2d %02d:%02d",
		tm_p->tm_mday, monstr[tm_p->tm_mon], tm_p->tm_year,
		tm_p->tm_hour, tm_p->tm_min);
	fprintf(ls->f, "%6d %6o  %5d  %5d   %5d %s %s\n", ino, inode.i_mode,
	       inode.i_uid, inode.i_gid, inode.i_size, datestr, name);
}

static void ls_file(struct list_dir_struct *ls, char *name,
		    ino_t ino, int rec_len)
{
	char	tmp[EXT2_NAME_LEN + 16];
	int	thislen;

	sprintf(tmp, "%d (%d) %s   ", ino, rec_len, name);
	thislen = strlen(tmp);

	if (ls->col + thislen > 80) {
		fprintf(ls->f, "\n");
		ls->col = 0;
	}
	fprintf(ls->f, "%s", tmp);
	ls->col += thislen;
}	


static int list_dir_proc(struct ext2_dir_entry *dirent,
			 int	offset,
			 int	blocksize,
			 char	*buf,
			 void	*private)
{
	char	name[EXT2_NAME_LEN];
	char	tmp[EXT2_NAME_LEN + 16];

	struct list_dir_struct *ls = (struct list_dir_struct *) private;
	int	thislen;

	thislen = (dirent->name_len < EXT2_NAME_LEN) ? dirent->name_len :
		EXT2_NAME_LEN;
	strncpy(name, dirent->name, thislen);
	name[thislen] = '\0';

	if (ls->options & LONG_OPT) 
		ls_l_file(ls, name, dirent->inode);
	else
		ls_file(ls, name, dirent->inode, dirent->rec_len);
	
	return 0;
}

void do_list_dir(int argc, char *argv[])
{
	ino_t	inode;
	int	retval;
	struct list_dir_struct ls;
	int	argptr = 1;
	
	ls.options = 0;
	if (check_fs_open(argv[0]))
		return;

	if ((argc > argptr) && (argv[argptr][0] == '-')) {
		argptr++;
		ls.options = LONG_OPT;
	}

	if (argc <= argptr)
		inode = cwd;
	else
		inode = string_to_inode(argv[argptr]);
	if (!inode)
		return;

	ls.f = open_pager();
	ls.col = 0;
	retval = ext2fs_dir_iterate(current_fs, inode,
				    DIRENT_FLAG_INCLUDE_EMPTY,
				    0, list_dir_proc, &ls);
	fprintf(ls.f, "\n");
	close_pager(ls.f);
	if (retval)
		com_err(argv[1], retval, "");

	return;
}


