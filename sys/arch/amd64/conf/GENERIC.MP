#	$OpenBSD: GENERIC.MP,v 1.10 2008/12/22 16:35:28 deraadt Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR

cpu*		at mainbus?
