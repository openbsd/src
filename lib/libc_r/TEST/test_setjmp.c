/*	$OpenBSD: test_setjmp.c,v 1.5 2000/01/06 06:58:34 d Exp $	*/
#include <setjmp.h>
#include "test.h"

int reached;

void *
_jump(arg)
	void *arg;
{
	jmp_buf foo;

	reached = 0;
	if (_setjmp(foo)) {
		ASSERT(reached);
		return NULL;
	}
	reached = 1;
	_longjmp(foo, 1);
	PANIC("_longjmp");
}

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
	printf("_jumping in main thread\n");
	(void)_jump(NULL);

	printf("jumping in child thread\n");
	CHECKr(pthread_create(&child, NULL, jump, NULL));
	CHECKr(pthread_join(child, &res));

	printf("_jumping in child thread\n");
	CHECKr(pthread_create(&child, NULL, _jump, NULL));
	CHECKr(pthread_join(child, &res));

	SUCCEED;
}
