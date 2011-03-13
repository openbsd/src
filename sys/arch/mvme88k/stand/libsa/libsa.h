/*	$OpenBSD: libsa.h,v 1.6 2011/03/13 00:13:53 deraadt Exp $	*/

/*
 * libsa prototypes
 */

#include "libbug.h"

/* board.c */
void board_setup();

/* bugdev.c */
int bugscopen(struct open_file *, ...);
int bugscclose(struct open_file *);
int bugscioctl(struct open_file *, u_long, void *);
int bugscstrategy(void *, int, daddr32_t, size_t, void *, size_t *);

/* exec_mvme.c */
void exec_mvme(char *, int);

/* parse_args.c */
int parse_args(char **, int *);

