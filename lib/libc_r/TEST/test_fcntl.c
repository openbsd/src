#include <stdio.h>
#include <fcntl.h>
#include "test.h"

main()
{
	int flags, child;

	CHECKe(flags = fcntl(0, F_GETFL));
	printf("flags = %x\n", flags);

	CHECKe(child = fork());
	switch(child) {
	case 0: /* child */
		CHECKe(execlp("test_create", "test_create", NULL));
		/* NOTREACHED */
	default: /* parent */
		CHECKe(wait(NULL));
		break;
	}
		
	while(1){
		CHECKe(flags = fcntl(0, F_GETFL));
		printf ("parent %d flags = %x\n", child, flags);
		sleep(1);
	}
}
