#include <lwp.h>

/*
 * This is no test-program for lwp, this is just to make
 * sure we got a process.o that we can link with.
 * Run it if you want a coredump...
 *
 *
 *  Todo: really a test-program
 *
 * $Id: testprocess.c,v 1.2 2000/09/11 14:41:10 art Exp $
 */

int savecontext();
int returnto();

int main(void)
{
    savecontext();
    returnto();
    return 0;
}
