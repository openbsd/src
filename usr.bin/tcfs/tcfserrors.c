/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 *
 *	  $Source: /home/cvs/src/usr.bin/tcfs/Attic/tcfserrors.c,v $
 *	   $State: Exp $
 *	$Revision: 1.1.1.1 $
 *	  $Author: provos $
 *	    $Date: 2000/06/18 22:07:24 $
 *
 */

static const char *RCSid="$id: $";

/* RCS_HEADER_ENDS_HERE */



#include <stdio.h>
#include <unistd.h>
#include "tcfserrors.h"

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
