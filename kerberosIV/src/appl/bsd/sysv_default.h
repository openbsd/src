/************************************************************************
* Copyright 1995 by Wietse Venema.  All rights reserved.  Some individual
* files may be covered by other copyrights.
*
* This material was originally written and compiled by Wietse Venema at
* Eindhoven University of Technology, The Netherlands, in 1990, 1991,
* 1992, 1993, 1994 and 1995.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that this entire copyright notice
* is duplicated in all such copies.
*
* This software is provided "as is" and without any expressed or implied
* warranties, including, without limitation, the implied warranties of
* merchantibility and fitness for any particular purpose.
************************************************************************/
/* Author: Wietse Venema <wietse@wzv.win.tue.nl> */

/* $KTH: sysv_default.h,v 1.6 2001/06/04 14:08:41 assar Exp $ */

extern char *default_console;
extern char *default_altsh;
extern char *default_passreq;
extern char *default_timezone;
extern char *default_hz;
extern char *default_path;
extern char *default_supath;
extern char *default_ulimit;
extern char *default_timeout;
extern char *default_umask;
extern char *default_sleep;
extern char *default_maxtrys;

void sysv_defaults(void);
