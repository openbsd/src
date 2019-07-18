/*	$OpenBSD: log.c,v 1.11 2019/07/18 10:50:24 lum Exp $	*/

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
 *
 * Note this file is not compiled into mg by default, you will need to
 * amend the 'Makefile' for that to happen. Because of this, the code
 * is subject to bit-rot. However, I know myself and others have 
 * written similar functionally often enough, that recording the below 
 * in a code repository could aid the developement efforts of mg, even
 * if it requires a bit of effort to get working. The current code is
 * written in the spirit of debugging (quickly and perhaps not ideal,
 * but it does what is required well enough). Should debugging become
 * more formalised within mg, then I would expect that to change.
 *
 * If you open a file with long lines to run through this debugging 
 * code, you may run into problems with the 1st fprintf statement in
 * in the mglog_lines() function. mg sometimes segvs at a strlen call
 * in fprintf - possibly something to do with the format string?
 * 	"%s%p b^%p f.%p %d %d\t%c|%s\n"
 * When I get time I will look into it. But since my debugging 
 * generally revolves around a file like:
 * 
 * abc
 * def
 * ghk
 *
 * I don't experience this bug. Just note it for future investigation.
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
#include "key.h"
#include "kbd.h"
#include "funmap.h"
#include "chrdef.h"

#include "log.h"

static char	*mglogfiles_create(char *);
static int	 mglog_lines(PF);
static int	 mglog_undo(void);
static int	 mglog_window(void);
static int	 mglog_key(KEYMAP *map);

char		*mglogdir;
extern char	*mglogpath_lines;
extern char	*mglogpath_undo;
extern char	*mglogpath_window;
extern char	*mglogpath_key;
extern char     *mglogpath_interpreter;
int		 mgloglevel;

int
mglog(PF funct, KEYMAP *map)
{
	if(!mglog_lines(funct))
		ewprintf("Problem logging lines");
	if(!mglog_undo())
		ewprintf("Problem logging undo");
	if(!mglog_window())
		ewprintf("Problem logging window");
	if(!mglog_key(map))
		ewprintf("Problem logging key");

	return (TRUE);
}


static int
mglog_key(KEYMAP *map)
{
	struct stat      sb;
	FILE            *fd;
	PF		*pfp;

	if(stat(mglogpath_key, &sb))
		 return (FALSE);
	fd = fopen(mglogpath_key, "a");

	if (ISWORD(*key.k_chars)) {
		if (fprintf(fd, "k_count:%d k_chars:%hd\tchr:%c\t", key.k_count,
		    *key.k_chars, CHARMASK(*key.k_chars)) == -1) {
			fclose(fd);
			return (FALSE);
		}
	} else {
		if (fprintf(fd, "k_count:%d k_chars:%hd\t\t", key.k_count,
		    *key.k_chars) == -1) {
			fclose(fd);
			return (FALSE);
		}
	}
	if (fprintf(fd, "map:%p %d %d %p %hd %hd\n",
	    map,
	    map->map_num,
	    map->map_max,
	    map->map_default,
	    map->map_element->k_base,
	    map->map_element->k_num
	    ) == -1) {
		fclose(fd);
		return (FALSE);
	}
	for (pfp = map->map_element->k_funcp; *pfp != '\0'; pfp++)
		fprintf(fd, "%s ", function_name(*pfp));

	fprintf(fd, "\n\n");
	fclose(fd);
	return (TRUE);
}

static int
mglog_window(void)
{
	struct mgwin	*wp;
	struct stat	 sb;
	FILE		*fd;
	int		 i;

	if(stat(mglogpath_window, &sb))
		return (FALSE);
	fd = fopen(mglogpath_window, "a");

	for (wp = wheadp, i = 0; wp != NULL; wp = wp->w_wndp, ++i) {
		if (fprintf(fd,
		    "%d wh%p wlst%p wbfp%p wlp%p wdtp%p wmkp%p wdto%d wmko%d" \
		    " wtpr%d wntr%d wfrm%d wrfl%c wflg%c wwrl%p wdtl%d" \
		    " wmkl%d\n",
		    i,
		    wp,
		    &wp->w_list,
		    wp->w_bufp,
		    wp->w_linep,
		    wp->w_dotp,
		    wp->w_markp,
		    wp->w_doto,
		    wp->w_marko,
		    wp->w_toprow,
		    wp->w_ntrows,
		    wp->w_frame,
		    wp->w_rflag,
		    wp->w_flag,
		    wp->w_wrapline,
		    wp->w_dotline,
		    wp->w_markline) == -1) {
			fclose(fd);
			return (FALSE);
		}
	}
	fclose(fd);
	return (TRUE);
}

static int
mglog_undo(void)
{
	struct undo_rec	*rec;
	struct stat	 sb;
	FILE		*fd;
	char		 buf[4096], tmp[1024];
	int      	 num;
	char		*jptr;

	jptr = "^J"; /* :) */

	if(stat(mglogpath_undo, &sb))
		return (FALSE);
	fd = fopen(mglogpath_undo, "a");
	
	/*
	 * From undo_dump()
	 */
	num = 0;
	TAILQ_FOREACH(rec, &curbp->b_undo, next) {
		num++;
		if (fprintf(fd, "%d:\t %s at %d ", num,
		    (rec->type == DELETE) ? "DELETE":
		    (rec->type == DELREG) ? "DELREGION":
		    (rec->type == INSERT) ? "INSERT":
		    (rec->type == BOUNDARY) ? "----" :
		    (rec->type == MODIFIED) ? "MODIFIED": "UNKNOWN",
		    rec->pos) == -1) {
			fclose(fd);
			return (FALSE);
		}
		if (rec->content) {
			(void)strlcat(buf, "\"", sizeof(buf));
			snprintf(tmp, sizeof(tmp), "%.*s",
			    *rec->content == '\n' ? 2 : rec->region.r_size,
			    *rec->content == '\n' ? jptr : rec->content);
			(void)strlcat(buf, tmp, sizeof(buf));
			(void)strlcat(buf, "\"", sizeof(buf));
		}
		snprintf(tmp, sizeof(tmp), " [%d]", rec->region.r_size);
		if (strlcat(buf, tmp, sizeof(buf)) >= sizeof(buf)) {
			dobeep();
			ewprintf("Undo record too large. Aborted.");
			return (FALSE);
		}
		if (fprintf(fd, "%s\n", buf) == -1) {
			fclose(fd);
			return (FALSE);
		}
		tmp[0] = buf[0] = '\0';
	}
	if (fprintf(fd, "\t [end-of-undo]\n\n") == -1) {
		fclose(fd);
		return (FALSE);
	}
	fclose(fd);

	return (TRUE);
}

static int
mglog_lines(PF funct)
{
	struct line     *lp;
	struct stat      sb;
	char		*curline, *tmp, o;
	FILE            *fd;
	int		 i;

	i = 0;

	if(stat(mglogpath_lines, &sb))
		return (FALSE);

	fd = fopen(mglogpath_lines, "a");
	if (fprintf(fd, "%s\n", function_name(funct)) == -1) {
		fclose(fd);
		return (FALSE);
	}
	lp = bfirstlp(curbp);

	for(;;) {
		i++;
		curline = " ";
		o = ' ';
		if (i == curwp->w_dotline) {
			curline = ">";
			if (lp->l_used > 0 && curwp->w_doto < lp->l_used)
				o = lp->l_text[curwp->w_doto];
			else
				o = '-';
		}
		if (lp->l_size == 0)
			tmp = " ";
		else
			tmp = lp->l_text;

		/* segv on fprintf below with long lines */
		if (fprintf(fd, "%s%p b^%p f.%p %d %d\t%c|%s\n", curline,
		    lp, lp->l_bp, lp->l_fp,
		    lp->l_size, lp->l_used, o, tmp) == -1) {
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
			if (fprintf(fd, "lines:raw:%d buf:%d wdot:%d\n\n",
			    i, curbp->b_lines, curwp->w_dotline) == -1) {
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
 * See what the eval variable code is up to.
 */
int
mglog_isvar(
	const char* const argbuf,
	const char* const argp,
	const int 	  sizof
)
{
	FILE		*fd;

	fd = fopen(mglogpath_interpreter, "a");

	if (fprintf(fd, " argbuf:%s,argp:%s,sizof:%d<\n",
	    argbuf,
	    argp,
	    sizof
	    ) == -1) {
		fclose(fd);
		return (FALSE);
	}
	fclose(fd);
	return (TRUE);
}

/*
 * See what the eval line code is up to.
 */
int
mglog_execbuf(
	const char* const pre,
	const char* const excbuf,
	const char* const argbuf,
    	const char* const argp,
	const int 	  last,
	const int	  inlist,
    	const char* const cmdp,
	const char* const p,
	const char* const contbuf
)
{
	FILE		*fd;

	fd = fopen(mglogpath_interpreter, "a");

	if (fprintf(fd, "%sexcbuf:%s,argbuf:%s,argp:%s,last:%d,inlist:%d,"\
	    "cmdp:%s,p:%s,contbuf:%s<\n",
	    pre,
	    excbuf,
	    argbuf,
	    argp,
	    last,
	    inlist,
	    cmdp,
	    p,
	    contbuf
	    ) == -1) {
		fclose(fd);
		return (FALSE);
	}
	fclose(fd);
	return (TRUE);
}

/*
 * Make sure logging to log files can happen.
 */
int
mgloginit(void)
{
	struct stat	 sb;
	mode_t           dir_mode, f_mode, oumask;
	char		*mglogfile_lines, *mglogfile_undo, *mglogfile_window;
	char		*mglogfile_key, *mglogfile_interpreter;

	mglogdir = "./log/";
	mglogfile_lines = "line.log";
	mglogfile_undo = "undo.log";
	mglogfile_window = "window.log";
	mglogfile_key = "key.log";
	mglogfile_interpreter = "interpreter.log";

	/* 
	 * Change mgloglevel for desired level of logging.
	 * log.h has relevant level info.
	 */
	mgloglevel = 1;

	oumask = umask(0);
	f_mode = 0777& ~oumask;
	dir_mode = f_mode | S_IWUSR | S_IXUSR;

	if(stat(mglogdir, &sb)) {
		if (mkdir(mglogdir, dir_mode) != 0)
			return (FALSE);
		if (chmod(mglogdir, f_mode) == -1)
			return (FALSE);
	}
	mglogpath_lines = mglogfiles_create(mglogfile_lines);
	if (mglogpath_lines == NULL)
		return (FALSE);
	mglogpath_undo = mglogfiles_create(mglogfile_undo);
	if (mglogpath_undo == NULL)
		return (FALSE);
	mglogpath_window = mglogfiles_create(mglogfile_window);
	if (mglogpath_window == NULL)
		return (FALSE);
	mglogpath_key = mglogfiles_create(mglogfile_key);
	if (mglogpath_key == NULL)
		return (FALSE);
	mglogpath_interpreter = mglogfiles_create(mglogfile_interpreter);
	if (mglogpath_interpreter == NULL)
		return (FALSE);

	return (TRUE);
}	


static char *
mglogfiles_create(char *mglogfile)
{
	struct stat	 sb;
	char		 tmp[NFILEN], *tmp2;
	int     	 fd;

	if (strlcpy(tmp, mglogdir, sizeof(tmp)) >
	    sizeof(tmp))
		return (NULL);
	if (strlcat(tmp, mglogfile, sizeof(tmp)) >
	    sizeof(tmp))
		return (NULL);
	if ((tmp2 = strndup(tmp, NFILEN)) == NULL)
		return (NULL);

	if(stat(tmp2, &sb))
		fd = open(tmp2, O_RDWR | O_CREAT | O_TRUNC, 0644);
	else
		fd = open(tmp2, O_RDWR | O_TRUNC, 0644);

	if (fd == -1)
		return (NULL);

	close(fd);	

	return (tmp2);
}

/*
 * Template log function.
 */
/*
int
mglog_?(void)
{
	struct stat      sb;
	FILE            *fd;

	if(stat(mglogpath_?, &sb))
	fd = fopen(mglogpath_?, "a");

	if (fprintf(fd, "%?", ??) == -1) {
		fclose(fd);
		return (FALSE);
	}
	fclose(fd);
	return (TRUE);
}
*/
