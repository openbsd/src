/*	$OpenBSD: sysdef.h,v 1.12 2005/04/03 02:09:28 db Exp $	*/

/*
 *		POSIX system header file
 */
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define	KBLOCK		8192	/* Kill grow.			 */
#define	GOOD		0	/* Good exit status.		 */
#define	SYMBLINK	1	/* Handle symbolic links.	 */

typedef int	RSIZE;		/* Type for file/region sizes	 */
typedef short	KCHAR;		/* Type for internal keystrokes	 */

#define MALLOCROUND(m)	(m+=7,m&=~7)	/* round up to 8 byte boundary	 */

struct fileinfo {
	uid_t		fi_uid;
	gid_t		fi_gid;
	mode_t		fi_mode;
};
