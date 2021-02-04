#	$OpenBSD: GENERIC.MP,v 1.15 2021/02/04 00:55:41 sashan Exp $

include "arch/amd64/conf/GENERIC"

#option	WITH_PF_LOCK
option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
#option	WITNESS

cpu*		at mainbus?
