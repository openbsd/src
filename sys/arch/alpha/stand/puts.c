/*	$OpenBSD: puts.c,v 1.2 1996/07/29 23:01:27 niklas Exp $	*/


void
puts(s)
	char *s;
{

	while (*s)
		putchar(*s++);
}
