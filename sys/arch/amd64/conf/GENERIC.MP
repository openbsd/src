#	$OpenBSD: GENERIC.MP,v 1.6 2005/05/24 00:51:27 brad Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS

cpu*		at mainbus?
ioapic*		at mainbus?

aapic*		at pci?			# AMD 8131 IO apic
