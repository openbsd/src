#	$OpenBSD: GENERIC.MP,v 1.3 2004/09/16 09:14:03 mickey Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
option		CPU

cpu*		at mainbus?
ioapic* 	at mainbus?
