/*	$OpenBSD: testprocess.c,v 1.1.1.1 1998/09/14 21:53:11 art Exp $	*/
#include <lwp.h>

/*
 * This is no test-program for lwp, this is just to make
 * sure we got a process.o that we can link with.
 * Run it if you want a coredump...
 *
 *
 *  Todo: really a test-program
 *
 * $KTH: testprocess.c,v 1.1 1998/06/09 11:52:53 lha Exp $
 */

int savecontext();
int returnto();

int main(void)
{
    savecontext();
    returnto();
    return 0;
}
