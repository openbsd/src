/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: a64l.c,v 1.2 1996/08/19 08:33:19 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

long
a64l(s)
	const char *s;
{
	long value, digit, shift;
	int i;

	value = 0;
	shift = 0;
	for (i = 0; *s && i < 6; i++, s++) {
		if (*s <= '/')
			digit = *s - '.';
		else if (*s <= '9')
			digit = *s - '0' + 2;
		else if (*s <= 'Z')
			digit = *s - 'A' + 12;
		else
			digit = *s - 'a' + 38; 

		value |= digit << shift;
		shift += 6;
	}

	return (long) value;
}
