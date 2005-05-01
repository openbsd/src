#	$OpenBSD: GENERIC.MP,v 1.5 2005/05/01 07:54:42 david Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support

cpu*		at mainbus?
ioapic*		at mainbus?
