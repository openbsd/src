#	$OpenBSD: GENERIC.MP,v 1.14 2018/07/13 05:25:24 tb Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
#option	WITNESS

cpu*		at mainbus?
