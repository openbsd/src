#include "bugargs.h"
#define volatile
int _DYNAMIC;
start(struct bugargs *bugarea)
{
	main(bugarea);
	bug_return();
	/* NOTREACHED */
}
