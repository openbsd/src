/*
 *		System V system header file
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define	KBLOCK	8192			/* Kill grow.			*/
#define	GOOD	0			/* Good exit status.		*/
#define	MAXPATH	256			/* Maximum length of path for chdir */

typedef int	RSIZE;			/* Type for file/region sizes	*/
typedef short	KCHAR;			/* Type for internal keystrokes	*/

/*
 * Macros used by the buffer name making code.
 * Start at the end of the file name, scan to the left
 * until BDC1 (or BDC2, if defined) is reached. The buffer
 * name starts just to the right of that location, and
 * stops at end of string (or at the next BDC3 character,
 * if defined). BDC2 and BDC3 are mainly for VMS.
 */
#define	BDC1	'/'			/* Buffer names.		*/

#define MALLOCROUND(m)	(m+=7,m&=~7)	/* round up to 8 byte boundry	*/

#define	fncmp		strcmp		/* file name comparison		*/
#define	gettermtype()	getenv("TERM")	/* determine terminal type	*/

struct fileinfo {
	unsigned short fi_mode;
	unsigned short fi_uid;
	unsigned short fi_gid;
};
