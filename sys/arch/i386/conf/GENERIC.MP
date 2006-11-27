#	$OpenBSD: GENERIC.MP,v 1.6 2006/11/27 18:50:59 marco Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support

cpu*		at mainbus?
ioapic*		at mainbus?
acpimadt*	at acpi?
