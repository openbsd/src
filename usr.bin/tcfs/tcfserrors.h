/*	$OpenBSD: tcfserrors.h,v 1.4 2000/06/19 20:35:48 fgsch Exp $	*/

/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 */

#ifndef _ERRORS_H
#define _ERRORS_H

enum
{
	OK=0,		/* Ok, no error */
	ER_CUSTOM,	/* Custom error message */
	ER_UNKOPT,	/* Unknown command line option */
	ER_AUTH,	/* User authentication error */
	ER_MEM,		/* Out of memory error */
	ER_TCFS,	/* User is not allowed to use TCFS */
	ER_USER,	/* User error */
	ER_PERM,	/* Error calling TCFS_IOC_PERMANENT ioctl */
	ER_ENABLE,	/* Error calling TCFS_IOC_LOGIN ioctl */
	ER_DISABLE,	/* Error calling TCFS_IOC_DISABLE ioctl */
	ER_COUNT	/* Error calling TCFS_IOC_COUNT ioctl */
};

void	tcfs_error __P((int, char *));

#endif
