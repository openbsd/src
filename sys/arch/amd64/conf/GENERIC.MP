#	$OpenBSD: GENERIC.MP,v 1.8 2007/01/27 22:48:01 kettenis Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS

cpu*		at mainbus?
ioapic*		at mainbus?

aapic*		at pci?			# AMD 8131 IO apic
acpimadt0	at acpi?
