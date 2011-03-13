/*	$OpenBSD: disk.h,v 1.5 2011/03/13 00:13:52 deraadt Exp $	*/
/*	$NetBSD: disk.h,v 1.1 1995/11/23 02:39:42 cgd Exp $	*/

int	diskstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
/* int     diskopen(struct open_file *, int, int, int); */
int     diskclose(struct open_file *);

#define	diskioctl	noioctl
