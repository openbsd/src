#include <setjmp.h>
#include "test.h"

int reached;

void *
jump(arg)
	void *arg;
{
	jmp_buf foo;

	reached = 0;
	if (setjmp(foo)) {
		ASSERT(reached);
		return NULL;
	}
	reached = 1;
	longjmp(foo, 1);
	PANIC("longjmp");
}

int
main()
{
	pthread_t child;
	void *res;

	printf("jumping in main thread\n");
	(void)jump(NULL);

	printf("jumping in child thread\n");
	CHECKr(pthread_create(&child, NULL, jump, NULL));
	CHECKr(pthread_join(child, &res));

	SUCCEED;
}
