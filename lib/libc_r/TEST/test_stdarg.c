/*	$OpenBSD: test_stdarg.c,v 1.2 2000/01/04 02:31:44 d Exp $	*/
/*
 * Test <stdarg.h>
 */

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include "test.h"

int thing;

int
test1(char *fmt, ...)
{
	va_list	ap;

	int	i;
	char	c;
	long	l;
	void *	p;

	va_start(ap, fmt);
	for (; *fmt; fmt++)
	    switch (*fmt) {
	    case 'i':		
		i = va_arg(ap, int); 
		ASSERT(i == 1234);
		break;
	    case 'c':		
		c = va_arg(ap, char); 
		ASSERT(c == 'x');
		break;
	    case 'l':		
		l = va_arg(ap, long); 
		ASSERT(l == 123456789L);
		break;
	    case 'p':		
		p = va_arg(ap, void *); 
		ASSERT(p == &thing);
		break;
	    default:
		ASSERT(0);
	    }
	va_end(ap);
	return 9;
}

void * 
run_test(arg)
	void *arg;
{
	char *msg = (char *)arg;
	int i;

	printf("Testing stdarg: %s\n", msg);
	for (i = 0; i < 1000000; i++) {
		ASSERT(test1("iclp", 1234, 'x', 123456789L, &thing) == 9);
	}
	printf("ok\n");
	return NULL;
}

int
main()
{
	pthread_t t1, t2;

	run_test("in main thread");
	CHECKr(pthread_create(&t1, NULL, run_test, "in child thread 1"));
	CHECKr(pthread_create(&t2, NULL, run_test, "in child thread 2"));
	CHECKr(pthread_join(t1, NULL));
	CHECKr(pthread_join(t2, NULL));
	SUCCEED;
}
