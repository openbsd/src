#	$OpenBSD: GENERIC.MP,v 1.7 2007/01/27 22:41:40 kettenis Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support

cpu*		at mainbus?
ioapic*		at mainbus?
acpimadt0	at acpi?
