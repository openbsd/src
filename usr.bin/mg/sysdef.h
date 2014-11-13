/*	$OpenBSD: sysdef.h,v 1.17 2014/11/13 21:36:23 florian Exp $	*/

/* This file is in the public domain. */

/*
 *		POSIX system header file
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define	KBLOCK		8192	/* Kill grow.			 */

typedef int	RSIZE;		/* Type for file/region sizes	 */
typedef short	KCHAR;		/* Type for internal keystrokes	 */

struct fileinfo {
	uid_t		fi_uid;
	gid_t		fi_gid;
	mode_t		fi_mode;
	struct timespec	fi_mtime;	/* Last modified time */
};
