/*	$OpenBSD: tcfs_getstatus.c,v 1.8 2000/06/20 08:28:02 fgsch Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_cmd.h>

#include "tcfsdefines.h"
#include "tcfslib.h"
#include "tcfspwdb.h"


int
tcfs_getstatus(char *filesystem, struct tcfs_status *st)
{
	int i;
	struct tcfs_args x;

	if (!tcfs_verify_fs(filesystem))
		return (-1);	

	x.cmd = TCFS_GET_STATUS;
	i = tcfs_callfunction(filesystem, &x);
	*st = x.st;

	return (i);
}
