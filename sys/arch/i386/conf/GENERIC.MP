#	$OpenBSD: GENERIC.MP,v 1.10 2018/06/01 14:16:25 visa Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
#option		MP_LOCKDEBUG
option		WITNESS

cpu*		at mainbus?
