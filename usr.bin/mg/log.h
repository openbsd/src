/*      $OpenBSD: log.h,v 1.2 2019/06/10 16:48:59 lum Exp $   */

/* This file is in the public domain. */

/*
 * Specifically for mg logging functionality.
 *
 */

int			 mglog(PF);
int			 mgloginit(void);

char 			*mglogpath_lines;
char 			*mglogpath_undo;
