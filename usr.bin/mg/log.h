/*      $OpenBSD: log.h,v 1.3 2019/06/22 13:09:53 lum Exp $   */

/* This file is in the public domain. */

/*
 * Specifically for mg logging functionality.
 *
 */

int			 mglog(PF);
int			 mgloginit(void);

char 			*mglogpath_lines;
char 			*mglogpath_undo;
char 			*mglogpath_window;
