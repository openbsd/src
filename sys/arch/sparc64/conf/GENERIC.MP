#	$OpenBSD: GENERIC.MP,v 1.4 2008/07/12 14:26:07 kettenis Exp $

include "arch/sparc64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
cpu*	at ssm?
cpu*	at cmp?
cpu*	at core?
