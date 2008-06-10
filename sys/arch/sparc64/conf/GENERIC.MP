#	$OpenBSD: GENERIC.MP,v 1.2 2008/06/10 00:02:09 kettenis Exp $

include "arch/sparc64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
cpu*	at cmp?
