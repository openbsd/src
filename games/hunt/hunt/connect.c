/*	$OpenBSD: connect.c,v 1.3 1999/01/29 07:30:33 d Exp $	*/
/*	$NetBSD: connect.c,v 1.3 1997/10/11 08:13:40 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "hunt.h"
#include "client.h"

void
do_connect(name, team, enter_status)
	char	*name;
	char	team;
	long	enter_status;
{
	u_int32_t	uid;
	u_int32_t	mode;
	char *		Ttyname;
	char		buf[NAMELEN];

	uid = htonl(getuid());
	(void) write(Socket, (char *) &uid, sizeof uid);
	(void) write(Socket, name, NAMELEN);
	(void) write(Socket, &team, sizeof team);
	enter_status = htonl(enter_status);
	(void) write(Socket, (char *) &enter_status, sizeof enter_status);
	Ttyname = ttyname(STDOUT_FILENO);
	if (Ttyname == NULL)
		Ttyname = "not a tty";
	(void) strlcpy(buf, Ttyname, sizeof buf);
	(void) write(Socket, buf, NAMELEN);
	if (Send_message != NULL)
		mode = C_MESSAGE;
	else if (Am_monitor)
		mode = C_MONITOR;
	else
		mode = C_PLAYER;
	mode = htonl(mode);
	(void) write(Socket, (char *) &mode, sizeof mode);
}
