/*	$OpenBSD: libsa.h,v 1.3 2002/03/14 01:26:41 millert Exp $	*/

/*
 * libsa prototypes 
 */

#include "libbug.h"

/* bugdev.c */
int dsk_open(struct open_file *, ...);
int dsk_close(struct open_file *);
int dsk_ioctl(struct open_file *, u_long, void *);
int dsk_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int net_open(struct open_file *, ...);
int net_close(struct open_file *);
int net_ioctl(struct open_file *, u_long, void *);
int net_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int tape_open(struct open_file *, ...);
int tape_close(struct open_file *);
int tape_ioctl(struct open_file *, u_long, void *);
int tape_strategy(void *, int, daddr_t, size_t, void *, size_t *);

/* exec_mvme.c */
void exec_mvme(char *, int);

/* parse_args.c */
int parse_args(char **, int *);

#define BUGDEV_DISK	0
#define BUGDEV_NET	1
#define BUGDEV_TAPE	2

extern int bootdev_type;
