/*	$OpenBSD: libsa.h,v 1.4 2006/05/16 22:52:26 miod Exp $	*/

/*
 * libsa prototypes
 */

#include "libbug.h"

/* bugdev.c */
int bugscopen(struct open_file *, ...);
int bugscclose(struct open_file *);
int bugscioctl(struct open_file *, u_long, void *);
int bugscstrategy(void *, int, daddr_t, size_t, void *, size_t *);

/* exec_mvme.c */
void exec_mvme(char *, int);

/* parse_args.c */
int parse_args(char **, int *);

