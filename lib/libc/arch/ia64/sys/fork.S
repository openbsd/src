/* $OpenBSD: fork.S,v 1.2 2011/07/05 03:26:18 pirofti Exp $ */
/* $NetBSD: fork.S,v 1.2 2006/09/23 17:39:34 cherry Exp $ */
	
#include "SYS.h"

ENTRY(__fork, 0)
	CALLSYS_ERROR(fork)
	/* XXX:	 get return value for child */
	RET
END(__fork)
