/*	$OpenBSD: test_fcntl.c,v 1.4 2001/07/09 07:04:40 deraadt Exp $	*/
/*
 * Test fcntl() flag inheritance across a fork()
 */
#include <stdio.h>
#include <fcntl.h>
#include "test.h"

int
main()
{
	int flags, newflags, child;

	CHECKe(flags = fcntl(0, F_GETFL));
	printf("flags = %x\n", flags);

	CHECKe(child = fork());
	switch(child) {
	case 0: /* child */
		CHECKe(execlp("test_create", "test_create", (char *)NULL));
		/* NOTREACHED */
	default: /* parent */
		CHECKe(wait(NULL));
		break;
	}
		
	while(1){
		CHECKe(newflags = fcntl(0, F_GETFL));
		printf ("parent %d flags = %x\n", child, newflags);
		sleep(1);
	}
}
