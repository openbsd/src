/*	$OpenBSD: libsa.h,v 1.7 2013/05/12 10:43:45 miod Exp $	*/

/*
 * libsa prototypes
 */

#include "libbug.h"

/* board.c */
void board_setup(void);

/* bugdev.c */
int bugscopen(struct open_file *, ...);
int bugscclose(struct open_file *);
int bugscioctl(struct open_file *, u_long, void *);
int bugscstrategy(void *, int, daddr32_t, size_t, void *, size_t *);

/* exec_mvme.c */
void exec_mvme(char *, int);

/* fault.c */
int badaddr(void *, int);

/* parse_args.c */
int parse_args(char **, int *);

