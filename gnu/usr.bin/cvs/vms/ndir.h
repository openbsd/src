/*
	<ndir.h> -- definitions for 4.2BSD-compatible directory access

	28-dec-1994	Richard Levitte
	See ChangeLog for more recent modification history.
*/

#ifndef NDIR_H

#if 0
#ifndef FAB$C_BID
#include <fab.h>
#endif
#endif
#ifndef NAM$C_BID
#include <nam.h>
#endif
#if 0
#ifndef RMS$_SUC
#include <rmsdef.h>
#endif
#include <dir.h>
#else
#define	DIR$S_NAME	80
#endif

#define DIRBLKSIZ	512		/* size of directory block */
#ifdef VMS
#define MAXNAMLEN	(DIR$S_NAME + 7) /* 80 plus room for version #.  */
#define MAXFULLSPEC	NAM$C_MAXRSS /* Maximum full spec */
#else
#define MAXNAMLEN	15		/* maximum filename length */
#endif /* VMS */
	/* NOTE:  MAXNAMLEN must be one less than a multiple of 4 */

struct direct				/* data from readdir() */
	{
	long		d_ino;		/* inode number of entry */
	unsigned short	d_reclen;	/* length of this record */
	unsigned short	d_namlen;	/* length of string in d_name */
	char		d_name[MAXNAMLEN+1];	/* name of file */
	};

typedef struct
	{
	int	dd_fd;			/* file descriptor */
	int	dd_loc;			/* offset in block */
	int	dd_size;		/* amount of valid data */
	char	dd_buf[DIRBLKSIZ];	/* directory block */
	}	DIR;			/* stream data from opendir() */

extern DIR		*vms_opendir();
extern struct direct	*vms_readdir();
#ifndef VMS
extern long		telldir();
extern void		seekdir();
#endif
extern int		vms_closedir();

#define rewinddir( dirp )	seekdir( dirp, 0L )

#define NDIR_H 1
#endif /* ndir.h */
