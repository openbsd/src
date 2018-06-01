#	$OpenBSD: GENERIC.MP,v 1.12 2018/06/01 14:16:25 visa Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
option	WITNESS

cpu*		at mainbus?
