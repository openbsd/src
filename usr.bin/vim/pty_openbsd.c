/*	$OpenBSD: pty_openbsd.c,v 1.2 1996/10/15 08:31:54 downsj Exp $	*/

/*
 * A quick, OpenBSD specific pty.c replacement.  It's not even entirely
 * correct; but it's certainly not GPL'd.
 */

#include <stdio.h>
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
