/*	$OpenBSD: tcfsflag.c,v 1.7 2000/06/20 01:29:14 provos Exp $	*/

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
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_fileinfo.h>
#include "tcfsdefines.h"

tcfs_flags tcfs_getflags(int);
tcfs_flags tcfs_setflags(int, tcfs_flags);

int
flags_main(int argc, char *argv[])
{
	int fd, flag;
	tcfs_flags i;
	char cmd;

	seteuid(getuid());
	setuid(getuid());

	if (argc < 3) {
		fprintf(stderr, "tcfsflags [op]{r,x,g} file\n"
			 "\t op can either be + or -.\n\n");
		exit(1);
	}

	fd = open(argv[2], O_RDONLY);
	if (!fd) {
		fprintf(stderr, "open failed\n");
		exit(1);
	}

	i = tcfs_getflags(fd);
	if (i.flag == -1) {
		fprintf(stderr, "getflags error\n");
		close(fd);
		exit(1);
	}

	if (argv[1][0] == '-' || argv[1][0] == '+') {
		cmd = argv[1][1];
		flag = argv[1][0] == '+' ? 1 : 0;
	} else {
		flag = -1;
		cmd = argv[1][0];
	}

	switch(cmd) {
	case 'r':
		printf("%s x:%d g:%d\n", argv[2], FI_CFLAG(&i), FI_GSHAR(&i));
		exit(0);
	case 'x':
		if (flag == -1)
			flag = ~(FI_CFLAG(&i));;
		FI_SET_CF(&i, flag);
		break;
	case 'g':
		if (flag == -1)
			flag = ~(FI_GSHAR(&i));
		FI_SET_GS(&i, flag);
		break;
	default:
		fprintf(stderr, "%s: unknown option: %c\n", argv[0], cmd);
		exit(1);
	}					

	i = tcfs_setflags(fd, i);
	if (i.flag == -1) {
		fprintf(stderr, "setflags error\n");
		exit(1);
	}
	close(fd);

	exit(0);
}
	
