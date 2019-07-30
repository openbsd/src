#	$OpenBSD: GENERIC.MP,v 1.11 2014/09/03 07:44:33 blambert Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*		at mainbus?
