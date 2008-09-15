/*	$OpenBSD: sysdef.h,v 1.16 2008/09/15 16:11:35 kjell Exp $	*/

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
#define	GOOD		0	/* Good exit status.		 */

typedef int	RSIZE;		/* Type for file/region sizes	 */
typedef short	KCHAR;		/* Type for internal keystrokes	 */

#define MALLOCROUND(m)	(m+=7,m&=~7)	/* round up to 8 byte boundary	 */

struct fileinfo {
	uid_t		fi_uid;
	gid_t		fi_gid;
	mode_t		fi_mode;
	struct timespec	fi_mtime;	/* Last modified time */
};
