/*	$OpenBSD: hack.version.c,v 1.3 2001/08/06 22:59:13 pjanzen Exp $	*/

/*
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: hack.version.c,v 1.3 2001/08/06 22:59:13 pjanzen Exp $";
#endif /* not lint */

#include	"date.h"

doversion(){
	pline("%s 1.0.3 - last edit %s.", (
#ifdef QUEST
		"Quest"
#else
		"Hack"
#endif /* QUEST */
		), datestring);
	return(0);
}
