/*	$OpenBSD: test_readdir.c,v 1.4 2000/01/06 06:58:34 d Exp $	*/
/* ==== test_readdir.c ========================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 94/05/19 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include "test.h"

int
main()
{
	struct dirent * file;
	DIR * dot_dir;
	int found = 0;

	CHECKn(dot_dir = opendir("."));
	while ((file = readdir(dot_dir)) != NULL)
		if (strcmp("test_readdir", file->d_name) == 0)
			found = 1;
	CHECKe(closedir(dot_dir));
	ASSERT(found);
	SUCCEED;
}

