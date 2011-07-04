/* $NetBSD: __vfork14.S,v 1.2 2006/09/22 17:59:47 cherry Exp $ */

#include "SYS.h"

SYSCALL(__vfork14)
	/* XXX: watchout for return values for child/parent */ 
	RET
END(__vfork14)	
