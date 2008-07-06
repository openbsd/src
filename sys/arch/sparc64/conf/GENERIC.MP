#	$OpenBSD: GENERIC.MP,v 1.3 2008/07/06 08:53:38 kettenis Exp $

include "arch/sparc64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
cpu*	at ssm?
cpu*	at cmp?
