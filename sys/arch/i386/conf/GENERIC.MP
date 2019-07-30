#	$OpenBSD: GENERIC.MP,v 1.9 2014/09/03 07:44:33 blambert Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
#option		MP_LOCKDEBUG

cpu*		at mainbus?
