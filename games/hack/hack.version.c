/*	$OpenBSD: hack.version.c,v 1.2 2001/01/28 23:41:46 niklas Exp $	*/

/*
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: hack.version.c,v 1.2 2001/01/28 23:41:46 niklas Exp $";
#endif /* not lint */

#include	"date.h"

doversion(){
	pline("%s 1.0.3 - last edit %s.", (
#ifdef QUEST
		"Quest"
#else
		"Hack"
#endif QUEST
		), datestring);
	return(0);
}
