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

#include <stdio.h>
#include <unistd.h>
#include "tcfserrors.h"

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

void tcfs_error (int error_type, char *custom_message)
{
	if (error_type!=ER_CUSTOM && error_type!=OK)
		fprintf (stderr, "Error: ");
	
	switch (error_type)
	{
		case ER_AUTH:
		case ER_MEM:
		case ER_TCFS:
		case ER_PERM:
		case ER_ENABLE:
		case ER_DISABLE:
		case ER_COUNT:
		case ER_USER:
		case OK:
			fprintf (stderr, "%s\n", tcfs_errors_strings[error_type]);
			exit (error_type);
		case ER_CUSTOM:
			fprintf (stderr, "%s\n", custom_message);
			exit (1);
		case ER_UNKOPT:
			if (custom_message)
				fprintf (stderr, "%s: %s\n", tcfs_errors_strings[error_type], custom_message);
			else
				fprintf (stderr, "%s\n", tcfs_errors_strings[error_type]);
			
			exit (error_type);
			break; /* Useless code */
		default:
			fprintf (stderr, "internal error.\n");
			exit (1);
	}
}

void show_usage (char *fmt, char *arg)
{
	printf (fmt, arg);
}
