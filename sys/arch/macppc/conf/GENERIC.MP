#	$OpenBSD: GENERIC.MP,v 1.1 2007/05/03 21:21:33 kettenis Exp $

include "arch/macppc/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
