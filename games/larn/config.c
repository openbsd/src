/*	$OpenBSD: config.c,v 1.4 2000/06/29 07:55:40 pjanzen Exp $	*/
/*	$NetBSD: config.c,v 1.5 1997/10/18 20:03:08 christos Exp $	 */

/*
 * config.c --	This defines the installation dependent variables.
 *		Some strings are modified later.  ANSI C would
 *		allow compile time string concatenation; we must
 *		do runtime concatenation, in main.
 *
 *		Larn is copyrighted 1986 by Noah Morgan.
 */
#include <sys/cdefs.h>
#ifndef lint
static char rcsid[] = "$OpenBSD: config.c,v 1.4 2000/06/29 07:55:40 pjanzen Exp $";
#endif /* not lint */

#include "header.h"
#include "pathnames.h"

/*
 * All these strings will be appended to in main() to be complete filenames
 */

/* the game save filename */
char	savefilename[PATH_MAX];

/* the logging file */
char	logfile[] = _PATH_LOG;

/* the help text file */
char	helpfile[] = _PATH_HELP;

/* the score file */
char	scorefile[] = _PATH_SCORE;

/* the maze data file */
char	larnlevels[] = _PATH_LEVELS;

/* the .larnopts filename */
char	optsfile[PATH_MAX] = "/.larnopts";

/* the player id datafile name */
char	playerids[] = _PATH_PLAYERIDS;

char	diagfile[] = "Diagfile";	/* the diagnostic filename */
char	ckpfile[] = "Larn12.0.ckp";	/* the checkpoint filename */
char	*password = "pvnert(x)";	/* the wizards password <=32 */

#define	WIZID	1
int	wisid = 0;	/* the user id of the only person who can be wizard */
