#	$OpenBSD: GENERIC-IP30.MP,v 1.1 2010/01/18 17:05:52 miod Exp $

include "arch/sgi/conf/GENERIC-IP30"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*	at mainbus?
