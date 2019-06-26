/*      $OpenBSD: log.h,v 1.4 2019/06/26 16:54:29 lum Exp $   */

/* This file is in the public domain. */

/*
 * Specifically for mg logging functionality.
 *
 */
int			 mglog(PF, KEYMAP *);
int			 mgloginit(void);

char 			*mglogpath_lines;
char 			*mglogpath_undo;
char 			*mglogpath_window;
char 			*mglogpath_key;
