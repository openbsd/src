/*
 * libsa prototypes 
 */

#include "libbug.h"

/* bugdev.c */
int dsk_open __P((struct open_file *, ...));
int dsk_close __P((struct open_file *));
int dsk_ioctl __P((struct open_file *, u_long, void *));
int dsk_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int net_open __P((struct open_file *, ...));
int net_close __P((struct open_file *));
int net_ioctl __P((struct open_file *, u_long, void *));
int net_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int tape_open __P((struct open_file *, ...));
int tape_close __P((struct open_file *));
int tape_ioctl __P((struct open_file *, u_long, void *));
int tape_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));

/* exec_mvme.c */
void exec_mvme __P((char *, int));

/* parse_args.c */
int parse_args __P((char **, int *));

#define BUGDEV_DISK	0
#define BUGDEV_NET	1
#define BUGDEV_TAPE	2

extern int bootdev_type;
