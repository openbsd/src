#	$OpenBSD: GENERIC.MP,v 1.4 2004/09/25 05:30:39 grange Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS
option	CPU

cpu*		at mainbus?
ioapic*		at mainbus?
