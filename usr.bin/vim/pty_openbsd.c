/*	$OpenBSD: pty_openbsd.c,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/

/*
 * A quick, OpenBSD specific pty.c replacement.  It's not even entirely
 * correct; but it's certainly not GPL'd.
 */

#include <sys/types.h>
#include <util.h>

int OpenPTY(name)
	char **name;
{
	static char ttyname[64];
	int mfd, sfd, error;

	error = openpty(&mfd, &sfd, ttyname, NULL, NULL);
	if (error < 0)
		return (-1);

	*name = ttyname;
	return (mfd);
}
