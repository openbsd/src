/*	$OpenBSD: puts.c,v 1.3 1996/10/30 22:40:35 niklas Exp $	*/


void
puts(s)
	char *s;
{

	while (*s)
		putchar(*s++);
}
