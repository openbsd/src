#include "bug.h"
start(struct bugenv *bugarea)
{
	main(bugarea);
	bugreturn();
}

__main()
{
	return;
}
