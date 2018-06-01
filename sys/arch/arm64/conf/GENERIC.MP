#	$OpenBSD: GENERIC.MP,v 1.2 2018/06/01 14:16:25 visa Exp $

include "arch/arm64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
option	WITNESS

cpu*		at mainbus?
