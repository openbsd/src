/* filesubr.c --- subroutines for dealing with files
   Gratuitously adapted toward VMS quirks.

   Jim Blandy <jimb@cyclic.com>
   Benjamin J. Lee <benjamin@cyclic.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"

static int deep_remove_dir PROTO((const char *path));

/*
 * Copies "from" to "to".
 */
void
copy_file (from_file, to_file)
    const char *from_file;
    const char *to_file;
{
    char from[PATH_MAX], to[PATH_MAX];
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(from_file))
      strcpy(from, from_file);
    else
      sprintf(from, "./%s", from_file);

    if (isabsolute(to_file))
      strcpy(to, to_file);
    else
      sprintf(to, "./%s", to_file);

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> copy(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> copy(%s,%s)\n", from, to);
#endif
    if (noexec)
	return;

    if ((fdin = open (from, O_RDONLY)) < 0)
	error (1, errno, "cannot open %s for copying", from);
    if (fstat (fdin, &sb) < 0)
	error (1, errno, "cannot fstat %s", from);
    if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	error (1, errno, "cannot create %s for copying", to);
    if (sb.st_size > 0)
    {
	char buf[BUFSIZ];
	int n;

	for (;;) 
	{
	    n = read (fdin, buf, sizeof(buf));
	    if (n == -1)
	    {
#ifdef EINTR
		if (errno == EINTR)
		    continue;
#endif
		error (1, errno, "cannot read file %s for copying", from);
	    }
            else if (n == 0) 
		break;
  
	    if (write(fdout, buf, n) != n) {
		error (1, errno, "cannot write file %s for copying", to);
	    }
	}

#ifdef HAVE_FSYNC
	if (fsync (fdout)) 
	    error (1, errno, "cannot fsync file %s after copying", to);
#endif
    }

    if (close (fdin) < 0) 
	error (0, errno, "cannot close %s", from);
    if (close (fdout) < 0)
	error (1, errno, "cannot close %s", to);

    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int
isdir (file)
    const char *file;
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (S_ISDIR (sb.st_mode));
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int
islink (file)
    const char *file;
{
#ifdef S_ISLNK
    struct stat sb;

    if (lstat (file, &sb) < 0)
	return (0);
    return (S_ISLNK (sb.st_mode));
#else
    return (0);
#endif
}

/*
 * Returns non-zero if the argument file exists.
 */
int
isfile (file)
    const char *file;
{
    return isaccessible(file, F_OK);
}

/*
 * Returns non-zero if the argument file is readable.
 */
int
isreadable (file)
    const char *file;
{
    return isaccessible(file, R_OK);
}

/*
 * Returns non-zero if the argument file is writable.
 */
int
iswritable (file)
    const char *file;
{
    return isaccessible(file, W_OK);
}

/*
 * Returns non-zero if the argument file is accessable according to
 * mode.  If compiled with SETXID_SUPPORT also works if cvs has setxid
 * bits set.
 */
int
isaccessible (file, mode)
    const char *file;
    const int mode;
{
#ifdef SETXID_SUPPORT
    struct stat sb;
    int umask = 0;
    int gmask = 0;
    int omask = 0;
    int uid;
    
    if (stat(file, &sb) == -1)
	return 0;
    if (mode == F_OK)
	return 1;

    uid = geteuid();
    if (uid == 0)		/* superuser */
    {
	if (mode & X_OK)
	    return sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH);
	else
	    return 1;
    }
	
    if (mode & R_OK)
    {
	umask |= S_IRUSR;
	gmask |= S_IRGRP;
	omask |= S_IROTH;
    }
    if (mode & W_OK)
    {
	umask |= S_IWUSR;
	gmask |= S_IWGRP;
	omask |= S_IWOTH;
    }
    if (mode & X_OK)
    {
	umask |= S_IXUSR;
	gmask |= S_IXGRP;
	omask |= S_IXOTH;
    }

    if (sb.st_uid == uid)
	return (sb.st_mode & umask) == umask;
    else if (sb.st_gid == getegid())
	return (sb.st_mode & gmask) == gmask;
    else
	return (sb.st_mode & omask) == omask;
#else
    return access(file, mode) == 0;
#endif
}

/*
 * Open a file and die if it fails
 */
FILE *
open_file (name, mode)
    const char *name;
    const char *mode;
{
    FILE *fp;

    if ((fp = fopen (name, mode)) == NULL)
	error (1, errno, "cannot open %s", name);
    return (fp);
}

/*
 * Make a directory and die if it fails
 */
void
make_directory (name)
    const char *name;
{
    struct stat sb;

    if (stat (name, &sb) == 0 && (!S_ISDIR (sb.st_mode)))
	    error (0, 0, "%s already exists but is not a directory", name);
    if (!noexec && mkdir (name, 0777) < 0)
	error (1, errno, "cannot make directory %s", name);
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void
make_directories (name)
    const char *name;
{
    char *cp;

    if (noexec)
	return;

    if (mkdir (name, 0777) == 0 || errno == EEXIST)
	return;
    if (! existence_error (errno))
    {
	error (0, errno, "cannot make path to %s", name);
	return;
    }
    if ((cp = strrchr (name, '/')) == NULL)
	return;
    *cp = '\0';
    make_directories (name);
    *cp++ = '/';
    if (*cp == '\0')
	return;
    (void) mkdir (name, 0777);
}

/* Create directory NAME if it does not already exist; fatal error for
   other errors.  Returns 0 if directory was created; 1 if it already
   existed.  */
int
mkdir_if_needed (name)
    char *name;
{
    if (mkdir (name, 0777) < 0)
    {
	if (errno != EEXIST
#ifdef EACCESS
	    /* This was copied over from the OS/2 code; I would guess it
	       isn't needed here but that has not been verified.  */
	    && errno != EACCESS
#endif
	    )
	    error (1, errno, "cannot make directory %s", name);
	return 1;
    }
    return 0;
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Either change honors the current umask setting.
 */
void
xchmod (fname_file, writable)
    char *fname_file;
    int writable;
{
    char fname[PATH_MAX];
    struct stat sb;
    mode_t mode, oumask;

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(fname_file))
      strcpy(fname, fname_file);
    else
      sprintf(fname, "./%s", fname_file);

    if (stat (fname, &sb) < 0)
    {
	if (!noexec)
	    error (0, errno, "cannot stat %s", fname);
	return;
    }
    oumask = umask (0);
    (void) umask (oumask);
    if (writable)
    {
	mode = sb.st_mode | (~oumask
			     & (((sb.st_mode & S_IRUSR) ? S_IWUSR : 0)
				| ((sb.st_mode & S_IRGRP) ? S_IWGRP : 0)
				| ((sb.st_mode & S_IROTH) ? S_IWOTH : 0)));
    }
    else
    {
	mode = sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH) & ~oumask;
    }

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> chmod(%s,%o)\n",
			(server_active) ? 'S' : ' ', fname, mode);
#else
	(void) fprintf (stderr, "-> chmod(%s,%o)\n", fname, mode);
#endif
    if (noexec)
	return;

    if (chmod (fname, mode) < 0)
	error (0, errno, "cannot change mode of file %s", fname);
}

/*
 * Rename a file and die if it fails
 */
void
rename_file (from_file, to_file)
    const char *from_file;
    const char *to_file;
{
    char from[PATH_MAX], to[PATH_MAX];

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(from_file))
      strcpy(from, from_file);
    else
      sprintf(from, "./%s", from_file);

    if (isabsolute(to_file))
      strcpy(to, to_file);
    else
      sprintf(to, "./%s", to_file);

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> rename(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> rename(%s,%s)\n", from, to);
#endif
    if (noexec)
	return;

    if (rename (from, to) < 0)
	error (1, errno, "cannot rename file %s to %s", from, to);
}

/*
 * link a file, if possible.
 */
int
link_file (from_file, to_file)
    const char *from_file;
    const char *to_file;
{
    char from[PATH_MAX], to[PATH_MAX];

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(from_file))
      strcpy(from, from_file);
    else
      sprintf(from, "./%s", from_file);

    if (isabsolute(to_file))
      strcpy(to, to_file);
    else
      sprintf(to, "./%s", to_file);

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> link(%s,%s)\n",
			(server_active) ? 'S' : ' ', from, to);
#else
	(void) fprintf (stderr, "-> link(%s,%s)\n", from, to);
#endif
    if (noexec)
	return (0);

    return (link (from, to));
}

/*
 * unlink a file, if possible.
 */
int
unlink_file (f_file)
    const char *f_file;
{
    char f[PATH_MAX];

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(f_file))
      strcpy(f, f_file);
    else
      sprintf(f, "./%s", f_file);

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> unlink(%s)\n",
			(server_active) ? 'S' : ' ', f);
#else
	(void) fprintf (stderr, "-> unlink(%s)\n", f);
#endif
    if (noexec)
	return (0);

    return (unlink (f));
}

/*
 * Unlink a file or dir, if possible.  If it is a directory do a deep
 * removal of all of the files in the directory.  Return -1 on error
 * (in which case errno is set).
 */
int
unlink_file_dir (f_file)
    const char *f_file;
{
    char f[PATH_MAX];

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(f_file))
      strcpy(f, f_file);
    else
      sprintf(f, "./%s", f_file);

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> unlink_file_dir(%s)\n",
			(server_active) ? 'S' : ' ', f);
#else
	(void) fprintf (stderr, "-> unlink_file_dir(%s)\n", f);
#endif
    if (noexec)
	return (0);

    if (unlink (f) != 0)
    {
	/* under NEXTSTEP errno is set to return EPERM if
	 * the file is a directory,or if the user is not
	 * allowed to read or write to the file.
	 * [This is probably a bug in the O/S]
	 * other systems will return EISDIR to indicate
	 * that the path is a directory.
	 */
        if (errno == EISDIR || errno == EPERM)
                return deep_remove_dir (f);
        else
		/* The file wasn't a directory and some other
		 * error occured
		 */
                return -1;
    }
    /* We were able to remove the file from the disk */
    return 0;
}

/* Remove a directory and everything it contains.  Returns 0 for
 * success, -1 for failure (in which case errno is set).
 */

static int
deep_remove_dir (path)
    const char *path;
{
    DIR		  *dirp;
    struct dirent *dp;
    char	   buf[PATH_MAX];

    if (rmdir (path) != 0 && (errno == ENOTEMPTY || errno == EEXIST)) 
    {
	if ((dirp = opendir (path)) == NULL)
	    /* If unable to open the directory return
	     * an error
	     */
	    return -1;

	while ((dp = readdir (dirp)) != NULL)
	{
	    if (strcmp (dp->d_name, ".") == 0 ||
			strcmp (dp->d_name, "..") == 0)
		continue;

	    sprintf (buf, "%s/%s", path, dp->d_name);

	    if (unlink (buf) != 0 )
	    {
		if (errno == EISDIR || errno == EPERM)
		{
		    if (deep_remove_dir (buf))
		    {
			closedir (dirp);
			return -1;
		    }
		}
		else
		{
		    /* buf isn't a directory, or there are
		     * some sort of permision problems
		     */
		    closedir (dirp);
		    return -1;
		}
	    }
	}
	closedir (dirp);
	return rmdir (path);
	}

    /* Was able to remove the directory return 0 */
    return 0;
}

/* Read NCHARS bytes from descriptor FD into BUF.
   Return the number of characters successfully read.
   The number returned is always NCHARS unless end-of-file or error.  */
static size_t
block_read (fd, buf, nchars)
    int fd;
    char *buf;
    size_t nchars;
{
    char *bp = buf;
    size_t nread;

    do 
    {
	nread = read (fd, bp, nchars);
	if (nread == (size_t)-1)
	{
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif
	    return (size_t)-1;
	}

	if (nread == 0)
	    break; 

	bp += nread;
	nchars -= nread;
    } while (nchars != 0);

    return bp - buf;
} 

    
/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 */
int
xcmp (file1_file, file2_file)
    const char *file1_file;
    const char *file2_file;
{
    char file1[PATH_MAX], file2[PATH_MAX];
    char *buf1, *buf2;
    struct stat sb1, sb2;
    int fd1, fd2;
    int ret;

    /* Prefer local relative paths to files at expense of logical name
       access to files. */

    if (isabsolute(file1_file))
      strcpy(file1, file1_file);
    else
      sprintf(file1, "./%s", file1_file);

    if (isabsolute(file2_file))
      strcpy(file2, file2_file);
    else
      sprintf(file2, "./%s", file2_file);

    if ((fd1 = open (file1, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file1);
    if ((fd2 = open (file2, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file2);
    if (fstat (fd1, &sb1) < 0)
	error (1, errno, "cannot fstat %s", file1);
    if (fstat (fd2, &sb2) < 0)
	error (1, errno, "cannot fstat %s", file2);

    /* A generic file compare routine might compare st_dev & st_ino here 
       to see if the two files being compared are actually the same file.
       But that won't happen in CVS, so we won't bother. */

    if (sb1.st_size != sb2.st_size)
	ret = 1;
    else if (sb1.st_size == 0)
	ret = 0;
    else
    {
	/* FIXME: compute the optimal buffer size by computing the least
	   common multiple of the files st_blocks field */
	size_t buf_size = 8 * 1024;
	size_t read1;
	size_t read2;

	buf1 = xmalloc (buf_size);
	buf2 = xmalloc (buf_size);

	do 
	{
	    read1 = block_read (fd1, buf1, buf_size);
	    if (read1 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file1);

	    read2 = block_read (fd2, buf2, buf_size);
	    if (read2 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file2);

	    /* assert (read1 == read2); */

	    ret = memcmp(buf1, buf2, read1);
	} while (ret == 0 && read1 == buf_size);

	free (buf1);
	free (buf2);
    }
	
    (void) close (fd1);
    (void) close (fd2);
    return (ret);
}

unsigned char
VMS_filename_classes[] =
{
    0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
    0x20,0x21,0x22,0x23, 0x24,0x25,0x26,0x27,
    0x28,0x29,0x2a,0x2b, 0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33, 0x34,0x35,0x36,0x37,
    0x38,0x39,0x3a,0x3b, 0x3c,0x3d,0x3e,0x3f,
    0x40,0x61,0x62,0x63, 0x64,0x65,0x66,0x67,
    0x68,0x69,0x6a,0x6b, 0x6c,0x6d,0x6e,0x6f,
    0x70,0x71,0x72,0x73, 0x74,0x75,0x76,0x77,
    0x78,0x79,0x7a,0x5b, 0x5c,0x5d,0x5e,0x5f,
    0x60,0x61,0x62,0x63, 0x64,0x65,0x66,0x67,
    0x68,0x69,0x6a,0x6b, 0x6c,0x6d,0x6e,0x6f,
    0x70,0x71,0x72,0x73, 0x74,0x75,0x76,0x77,
    0x78,0x79,0x7a,0x7b, 0x7c,0x7d,0x7e,0x7f,
    0x80,0x81,0x82,0x83, 0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x8b, 0x8c,0x8d,0x8e,0x8f,
    0x90,0x91,0x92,0x93, 0x94,0x95,0x96,0x97,
    0x98,0x99,0x9a,0x9b, 0x9c,0x9d,0x9e,0x9f,
    0xa0,0xa1,0xa2,0xa3, 0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xab, 0xac,0xad,0xae,0xaf,
    0xb0,0xb1,0xb2,0xb3, 0xb4,0xb5,0xb6,0xb7,
    0xb8,0xb9,0xba,0xbb, 0xbc,0xbd,0xbe,0xbf,
    0xc0,0xc1,0xc2,0xc3, 0xc4,0xc5,0xc6,0xc7,
    0xc8,0xc9,0xca,0xcb, 0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd2,0xd3, 0xd4,0xd5,0xd6,0xd7,
    0xd8,0xd9,0xda,0xdb, 0xdc,0xdd,0xde,0xdf,
    0xe0,0xe1,0xe2,0xe3, 0xe4,0xe5,0xe6,0xe7,
    0xe8,0xe9,0xea,0xeb, 0xec,0xed,0xee,0xef,
    0xf0,0xf1,0xf2,0xf3, 0xf4,0xf5,0xf6,0xf7,
    0xf8,0xf9,0xfa,0xfb, 0xfc,0xfd,0xfe,0xff,
};

/* Like strcmp, but with the appropriate tweaks for file names.
   Under VMS, filenames are case-insensitive but case-preserving.
   FIXME: this should compare y.tab.c equal with y_tab.c, at least
   if fnfold is modified (see below).  */
int
fncmp (const char *n1, const char *n2)
{
    while (*n1 && *n2
           && (VMS_filename_classes[(unsigned char) *n1]
	       == VMS_filename_classes[(unsigned char) *n2]))
        n1++, n2++;
    return (VMS_filename_classes[(unsigned char) *n1]
            - VMS_filename_classes[(unsigned char) *n2]);
}

/* Fold characters in FILENAME to their canonical forms.  FIXME: this
   probably should be mapping y.tab.c to y_tab.c but first we have to
   figure out whether fnfold is the right hook for that functionality
   (probable answer: yes, but it should not fold case on OS/2, VMS, or
   NT.  You see, fnfold isn't called anywhere, so we can define it to
   mean whatever makes sense.  Of course to solve the VMS y.tab.c
   problem we'd need to call it where appropriate.  It would need to
   be redocumented as "fold to a form we can create in the filesystem"
   rather than "canonical form").  The idea is that files we create
   would get thusly munged, but CVS can cope with their names being
   different the same way that the NT port copes with it if the user
   renames a file from "foo" to "FOO".

   Alternately, this kind of handling could/should go into CVS_FOPEN
   and friends (if we want to do it like the Mac port, anyway).  */
void
fnfold (char *filename)
{
    while (*filename)
    {
        *filename = FOLD_FN_CHAR (*filename);
	filename++;
    }
}

/* Generate a unique temporary filename.  Returns a pointer to a newly
   malloc'd string containing the name.  Returns successfully or not at
   all.  */
char *
cvs_temp_name ()
{
    char value[L_tmpnam + 1];
    char *retval;

    /* FIXME: what is the VMS equivalent to TMPDIR?  */
    retval = tmpnam (value);
    if (retval == NULL)
	error (1, errno, "cannot generate temporary filename");
    return xstrdup (retval);
}

/* Return non-zero iff FILENAME is absolute.
   Trivial under Unix, but more complicated under other systems.  */
int
isabsolute (filename)
    const char *filename;
{
    if(filename[0] == '/'
       || filename[0] == '['
       || filename[0] == '<'
       || strchr(filename, ':'))
        return 1;
    else
        return 0;
}


/* Return a pointer into PATH's last component.  */
char *
last_component (path)
    char *path;
{
    char *last = strrchr (path, '/');

    if (last && (last != path))
        return last + 1;
    else
        return path;
}

/* Return the home directory.  Returns a pointer to storage
   managed by this function or its callees (currently getenv).  */
char *
get_homedir ()
{
    return getenv ("HOME");
}

/* See cvs.h for description.  On VMS this currently does nothing, although
   I think we should be expanding wildcards here.  */
void
expand_wild (argc, argv, pargc, pargv)
    int argc;
    char **argv;
    int *pargc;
    char ***pargv;
{
    int i;
    *pargc = argc;
    *pargv = (char **) xmalloc (argc * sizeof (char *));
    for (i = 0; i < argc; ++i)
	(*pargv)[i] = xstrdup (argv[i]);
}
