/*	$OpenBSD: top.h,v 1.2 1997/08/22 07:16:30 downsj Exp $	*/

/*
 *  Top - a top users display for Berkeley Unix
 *
 *  General (global) definitions
 */

#include <sys/cdefs.h>

/* Current major version number */
#define VERSION		3

/* Number of lines of header information on the standard screen */
#define Header_lines	6

/* Maximum number of columns allowed for display */
#define MAX_COLS	128

/* Log base 2 of 1024 is 10 (2^10 == 1024) */
#define LOG1024		10

/* Special atoi routine returns either a non-negative number or one of: */
#define Infinity	-1
#define Invalid		-2

/* maximum number we can have */
#define Largest		0x7fffffff

/*
 * The entire display is based on these next numbers being defined as is.
 */

#define NUM_AVERAGES    3

/* externs */
extern const char copyright[];

extern int overstrike;

/* commands.c */
extern void show_help __P((void));
extern int error_count __P((void));
extern void show_errors __P((void));
extern char *kill_procs __P((char *));
extern char *renice_procs __P((char *));

/* top.c */
extern void quit __P((int));

/* username.c */
extern void init_hash __P((void));
extern char *username __P((uid_t));
extern uid_t userid __P((char *));

/* version.c */
extern char *version_string __P((void));
