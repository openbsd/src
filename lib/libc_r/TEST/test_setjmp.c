#include <setjmp.h>
#include "test.h"

int reached;

main()
{
	jmp_buf foo;

	reached = 0;
	if (setjmp(foo)) {
		ASSERT(reached);
		SUCCEED;
	}
	reached = 1;
	longjmp(foo, 1);
	PANIC("longjmp");
}
