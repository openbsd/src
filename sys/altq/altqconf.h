/*	$OpenBSD: altqconf.h,v 1.1 2001/06/27 05:28:36 kjc Exp $	*/
/*	$NetBSD: altqconf.h,v 1.2 2001/05/30 11:57:16 mrg Exp $	*/

#ifdef ALTQ
#define	NALTQ	1
#else
#define	NALTQ	0
#endif

cdev_decl(altq);

#define cdev_altq_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }
