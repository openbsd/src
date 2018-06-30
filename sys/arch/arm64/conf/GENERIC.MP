#	$OpenBSD: GENERIC.MP,v 1.3 2018/06/30 15:23:37 deraadt Exp $

include "arch/arm64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
#option	WITNESS

cpu*		at mainbus?
