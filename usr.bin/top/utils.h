/*	$OpenBSD: utils.h,v 1.4 2002/02/16 21:27:55 millert Exp $	*/

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

int atoiwi(char *);
char *itoa(int);
char *itoa7(int);
int digits(int);
char *strecpy(char *, char *);
int string_index(char *, char **);
char **argparse(char *, int *);
int percentages(int, int *, long *, long *, long *);
char *format_time(time_t);
char *format_k(int);
