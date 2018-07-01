#	$OpenBSD: GENERIC.MP,v 1.4 2018/07/01 19:30:37 kettenis Exp $

include "arch/arm64/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
#option	WITNESS

cpu*		at mainbus?

xhci*		at acpi?
