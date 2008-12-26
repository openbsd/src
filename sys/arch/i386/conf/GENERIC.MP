#	$OpenBSD: GENERIC.MP,v 1.8 2008/12/26 17:10:45 deraadt Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support

cpu*		at mainbus?
