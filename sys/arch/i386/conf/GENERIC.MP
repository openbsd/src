#	$OpenBSD: GENERIC.MP,v 1.2 2004/06/13 21:49:15 niklas Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
option		CPU

cpu*		at mainbus? apid ?
ioapic* 	at mainbus? apid ?
