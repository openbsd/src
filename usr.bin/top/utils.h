/*	$OpenBSD: utils.h,v 1.2 1997/08/22 07:16:32 downsj Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/* prototypes for functions found in utils.c */

int atoiwi __P((char *));
char *itoa __P((int));
char *itoa7 __P((int));
int digits __P((int));
char *strecpy __P((char *, char *));
int string_index __P((char *, char **));
char **argparse __P((char *, int *));
int percentages __P((int, int *, int *, int *, int *));
char *format_time __P((time_t));
char *format_k __P((int));
