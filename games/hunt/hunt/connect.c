/*	$NetBSD: connect.c,v 1.3 1997/10/11 08:13:40 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: connect.c,v 1.3 1997/10/11 08:13:40 lukem Exp $");
#endif /* not lint */

# include	"hunt.h"
# include	<signal.h>
# include	<unistd.h>

void
do_connect(name, team, enter_status)
	char	*name;
	char	team;
	long	enter_status;
{
	static long	uid;
	static long	mode;

	if (uid == 0)
		uid = htonl(getuid());
	(void) write(Socket, (char *) &uid, LONGLEN);
	(void) write(Socket, name, NAMELEN);
	(void) write(Socket, &team, 1);
	enter_status = htonl(enter_status);
	(void) write(Socket, (char *) &enter_status, LONGLEN);
	(void) strcpy(Buf, ttyname(fileno(stderr)));
	(void) write(Socket, Buf, NAMELEN);
# ifdef INTERNET
	if (Send_message != NULL)
		mode = C_MESSAGE;
	else
# endif
# ifdef MONITOR
	if (Am_monitor)
		mode = C_MONITOR;
	else
# endif
		mode = C_PLAYER;
	mode = htonl(mode);
	(void) write(Socket, (char *) &mode, sizeof mode);
}
