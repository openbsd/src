#	$OpenBSD: GENERIC.MP,v 1.7 2006/11/27 19:19:09 jordan Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS

cpu*		at mainbus?
ioapic*		at mainbus?

aapic*		at pci?			# AMD 8131 IO apic
acpimadt*	at acpi?
