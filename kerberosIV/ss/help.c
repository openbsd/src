/*	$Id: help.c,v 1.2 1996/09/16 18:49:08 millert Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#ifndef NPOSIX
#include <fcntl.h>
#endif
#include <sys/wait.h>
#include "ss_internal.h"

extern int errno;

void
ss_help (argc, argv, sci_idx, info_ptr)
    int argc;
    char const * const *argv;
    int sci_idx;
    pointer info_ptr;
{
    char buffer[MAXPATHLEN];
    char const *request_name;
    int code;
    int fd, child;
    register int idx;
    register ss_data *info;

    request_name = ss_current_request(sci_idx, &code);
    if (code != 0) {
	ss_perror(sci_idx, code, "");
	return;		/* no ss_abort_line, if invalid invocation */
    }
    if (argc == 1) {
	ss_list_requests(argc, argv, sci_idx, info_ptr);
	return;
    }
    else if (argc != 2) {
	/* should do something better than this */
	snprintf(buffer, sizeof(buffer),
		 "usage:\n\t%s [topic|command]\nor\t%s\n",
		 request_name, request_name);
	ss_perror(sci_idx, 0, buffer);
	return;
    }
    info = ss_info(sci_idx);
    if (info->info_dirs == (char **)NULL) {
	ss_perror(sci_idx, SS_ET_NO_INFO_DIR, (char *)NULL);
	return;
    }
    if (info->info_dirs[0] == (char *)NULL) {
	ss_perror(sci_idx, SS_ET_NO_INFO_DIR, (char *)NULL);
	return;
    }
    for (idx = 0; info->info_dirs[idx] != (char *)NULL; idx++) {
	(void) strcpy(buffer, info->info_dirs[idx]);
	(void) strcat(buffer, "/");
	(void) strcat(buffer, argv[1]);
	(void) strcat(buffer, ".info");
	if ((fd = open(&buffer[0], O_RDONLY, 0)) >= 0) goto got_it;
    }
    if ((fd = open(&buffer[0], O_RDONLY, 0)) < 0) {
	char buf[MAXPATHLEN];
	strcpy(buf, "No info found for ");
	strcat(buf, argv[1]);
	ss_perror(sci_idx, 0, buf);
	return;
    }
got_it:
    switch (child = fork()) {
    case -1:
	ss_perror(sci_idx, errno, "Can't fork for pager");
	return;
    case 0:
	(void) dup2(fd, 0); /* put file on stdin */
	ss_page_stdin();
    default:
	(void) close(fd); /* what can we do if it fails? */
#ifndef NPOSIX
	while (wait((int *)0) != child) ;	 /* do nothing if wrong pid */
#else
	while (wait((union wait *)0) != child) ; /* do nothing if wrong pid */
#endif
    }
}

#include <sys/types.h>
#ifdef NPOSIX	
#include <sys/dir.h>
#else
#include <dirent.h>
#endif /* NPOSIX */

void
ss_add_info_dir(sci_idx, info_dir, code_ptr)
    int sci_idx;
    char *info_dir;
    int *code_ptr;
{
    register ss_data *info;
    DIR *d;
    int n_dirs;
    register char **dirs;

    info = ss_info(sci_idx);
    if (info_dir == NULL && *info_dir) {
	*code_ptr = SS_ET_NO_INFO_DIR;
	return;
    }
    if ((d = opendir(info_dir)) == (DIR *)NULL) {
	*code_ptr = errno;
	return;
    }
    closedir(d);
    dirs = info->info_dirs;
    for (n_dirs = 0; dirs[n_dirs] != (char *)NULL; n_dirs++)
	;		/* get number of non-NULL dir entries */
    dirs = (char **)realloc((char *)dirs,
			    (unsigned)(n_dirs + 2)*sizeof(char *));
    if (dirs == (char **)NULL) {
	info->info_dirs = (char **)NULL;
	*code_ptr = errno;
	return;
    }
    info->info_dirs = dirs;
    dirs[n_dirs + 1] = (char *)NULL;
    dirs[n_dirs] = malloc((unsigned)strlen(info_dir)+1);
    strcpy(dirs[n_dirs], info_dir);
    *code_ptr = 0;
}

void
ss_delete_info_dir(sci_idx, info_dir, code_ptr)
    int sci_idx;
    char *info_dir;
    int *code_ptr;
{
    register char **i_d;
    register char **info_dirs;

    info_dirs = ss_info(sci_idx)->info_dirs;
    for (i_d = info_dirs; *i_d; i_d++) {
	if (!strcmp(*i_d, info_dir)) {
	    while (*i_d) {
		*i_d = *(i_d+1);
		i_d++;
	    }
	    *code_ptr = 0;
	    return;
	}
    }
    *code_ptr = SS_ET_NO_INFO_DIR;
}
