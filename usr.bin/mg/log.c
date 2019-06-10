/*	$OpenBSD: log.c,v 1.1 2019/06/10 06:52:44 lum Exp $	*/

/* 
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 *
 */

/*
 * Record a history of an mg session for temporal debugging.
 * Sometimes pressing a key will set the scene for a bug only visible 
 * dozens of keystrokes later. gdb has its limitations in this scenario.
 */

#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "log.h"
#include "funmap.h"

char	 	 mglogpath[20];

int
mglog(PF funct)
{
	struct line     *lp;
	struct stat      sb;
	char		*curline;
	FILE            *fd;
	int		 i;

	i = 0;

	if(stat(mglogpath, &sb))
		return (FALSE);

	fd = fopen(mglogpath, "a");
	if (fprintf(fd, "%s\n", function_name(funct)) == -1) {
		fclose(fd);
		return (FALSE);
	}
	lp = bfirstlp(curbp);

	for(;;) {
		i++;
		curline = " ";
		if (i == curwp->w_dotline)
			curline = ">";
		if (fprintf(fd, "%s%p b^%p f.%p %d %d\t|%s\n", curline,
		    lp, lp->l_bp, lp->l_fp,
		    lp->l_size, lp->l_used, lp->l_text) == -1) {
			fclose(fd);
			return (FALSE);
		}
		lp = lforw(lp);
		if (lp == curbp->b_headp) {
			if (fprintf(fd, " %p b^%p f.%p [bhead]\n(EOB)\n",
			    lp, lp->l_bp, lp->l_fp) == -1) {
				fclose(fd);
        	                return (FALSE);
			}
			if (fprintf(fd, "lines:raw%d buf%d wdot%d\n\n",
			    i, curbp->b_lines, curwp->w_dotline)) {
				fclose(fd);
        	                return (FALSE);
			}
			break;
		}
	}
	fclose(fd);

	return (TRUE);
}


/*
 * Make sure logging to log file can happen.
 */
int
mgloginit(void)
{
	struct stat	 sb;
	mode_t           dir_mode, f_mode, oumask;
	char		*mglogdir, *mglogfile;
	int     	 fd;

	mglogdir = "./log/";
	mglogfile = "mg.log";

	oumask = umask(0);
	f_mode = 0777& ~oumask;
	dir_mode = f_mode | S_IWUSR | S_IXUSR;

	if(stat(mglogdir, &sb)) {
		if (mkdir(mglogdir, dir_mode) != 0)
			return (FALSE);
		if (chmod(mglogdir, f_mode) < 0)
			return (FALSE);
	}
	if (strlcpy(mglogpath, mglogdir, sizeof(mglogpath)) >
	    sizeof(mglogpath))
		return (FALSE);
	if (strlcat(mglogpath, mglogfile, sizeof(mglogpath)) >
	    sizeof(mglogpath))
		return (FALSE);

	if(stat(mglogpath, &sb))
		fd = open(mglogpath, O_RDWR | O_CREAT | O_TRUNC, 0644);
	else
		fd = open(mglogpath, O_RDWR | O_TRUNC, 0644);

	if (fd == -1)
		return (FALSE);

	close(fd);	

	return (TRUE);
}
