#include <stdio.h>
main()
{
	int ptr = (int)fprintf;

	printf("Hello world\n");
	printf("printf = 0x%08x\n", ptr);
}
