/*	$OpenBSD: tcfs_flags.c,v 1.7 2000/06/20 08:28:02 fgsch Exp $	*/

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
#include <sys/stat.h> 
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_fileinfo.h>
#include "tcfsdefines.h"

tcfs_flags
tcfs_getflags(int fd)
{
	tcfs_flags r;
	struct stat s;

	if (fstat(fd, &s) < 0)
		 r.flag = -1;
	else
		 r.flag = s.st_flags;

	return (r);
}
		 
	
tcfs_flags
tcfs_setflags(int fd, tcfs_flags x)
{
	tcfs_flags r, n;

	r = tcfs_getflags(fd);
	
	if (r.flag == -1) {
		r.flag = -1;
		return (r);
	}

	n = x;
	FI_SET_SP(&n, FI_SPURE(&r));

	if (fchflags(fd, n.flag)) {
		perror("fchflags");
		r.flag = -1;
	}

	return (r);
}
