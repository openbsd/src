#	$OpenBSD: GENERIC.MP,v 1.4 2004/09/22 08:48:56 grange Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support

cpu*		at mainbus?
ioapic* 	at mainbus?
