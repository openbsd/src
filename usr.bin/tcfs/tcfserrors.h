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
	OK=0,          /* Ok, no error */
	ER_CUSTOM,     /* Custom error message */
	ER_UNKOPT,     /* Unknown command line option */
	ER_AUTH,       /* User authentication error */
	ER_MEM,        /* Out of memory error */
	ER_TCFS,       /* User is not allowed to use TCFS */
	ER_USER,			/* User error */
	ER_PERM,       /* Error calling TCFS_IOC_PERMANENT ioctl */
	ER_ENABLE,     /* Error calling TCFS_IOC_LOGIN ioctl */
	ER_DISABLE,    /* Error calling TCFS_IOC_DISABLE ioctl */
	ER_COUNT       /* Error calling TCFS_IOC_COUNT ioctl */
};

static char *tcfs_errors_strings[]=
{
	"Ok",
	NULL,
	"unknow option.",
	"authentication error.",
	"out of memory.",
	"you do not have a TCFS key.",
	"Who are you?!",
	"ioctl error while setting permanent flag.",
	"ioctl error while sending.",
	"ioctl error while removing key.",
	"ioctl error while getting key counter."
};

void tcfs_error (int error_type, char *arg);

#endif

/* End of errors.h */
