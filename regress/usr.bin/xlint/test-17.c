 /*	$OpenBSD: test-17.c,v 1.3 2006/04/25 01:31:46 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with unreachable break statements.
 */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	int a = 0;

	switch (argc)
	{
	case 1:
		a = 1;
		return 1;
		break; /* should not warn */
	case 2:
		a = 2;
		while (a < 5) {
			a++;
			break;
			break; /* should warn */
		}
		break;
	case 3:
		switch (a) {
		case 0:
			break;
		}
	default:
		break;
	}

	while (argc < 5) {
		if (argc ) {
			return 1;
			break; /* should warn */
		}

		argc++;
		break;
	}

	return a;
}
