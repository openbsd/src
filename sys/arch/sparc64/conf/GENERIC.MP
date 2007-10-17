#	$OpenBSD: GENERIC.MP,v 1.1 2007/10/17 21:31:13 kettenis Exp $

include "arch/sparc64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
