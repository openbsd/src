#	$OpenBSD: GENERIC.MP,v 1.1 2018/02/20 23:48:23 kettenis Exp $

include "arch/arm64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG

cpu*		at mainbus?
