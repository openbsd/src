#	$OpenBSD: GENERIC.MP,v 1.3 2004/09/21 08:34:06 grange Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS
option	CPU

cpu*		at mainbus?
ioapic*		at mainbus?
#aapic*		at pci?
