/*	$OpenBSD: ldt.c,v 1.2 2001/01/28 19:34:29 niklas Exp $	*/

#include <stdio.h>
main()
{
	int ptr = (int)fprintf;

	printf("Hello world\n");
	printf("printf = 0x%08x\n", ptr);
}
