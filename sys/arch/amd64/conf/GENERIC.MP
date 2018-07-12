0;331;0cwit#	$OpenBSD: GENERIC.MP,v 1.13 2018/07/12 22:09:04 deraadt Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
#option	WITNESS

cpu*		at mainbus?
