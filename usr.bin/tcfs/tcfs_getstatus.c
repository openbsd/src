/*	$OpenBSD: tcfs_getstatus.c,v 1.6 2000/06/20 07:09:46 fgsch Exp $	*/

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

#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_cmd.h>
#include "tcfsdefines.h"
#include <sys/ucred.h>

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
