/*	$OpenBSD: disk.h,v 1.2 1996/07/29 23:01:39 niklas Exp $	*/
/*	$NetBSD: disk.h,v 1.1 1995/11/23 02:39:42 cgd Exp $	*/

int	diskstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));
/* int     diskopen __P((struct open_file *, int, int, int)); */
int     diskclose __P((struct open_file *));

#define	diskioctl	noioctl
