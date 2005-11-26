/*      $OpenBSD: test-4.c,v 1.1 2005/11/26 20:45:30 cloder Exp $ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test parsing of GNU case ranges.
 */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	int i;
	char c;

	c = 'a';
	switch (c)
	{
	case 'a':
		i = 0;
		break;
	case 'A' ... 'Z':
		i = 1;
		break;
	default:
		i = 1;
	}

	i++;
	return 0;
}
