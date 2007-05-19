#	$OpenBSD: GENERIC.MP,v 1.1 2007/05/19 17:06:11 miod Exp $
#
# For further information on compiling OpenBSD kernels, see the config(8)
# man page.
#
# For further information on hardware support for this architecture, see
# the intro(4) man page.  For further information about kernel options
# for this architecture, see the options(4) man page.  For an explanation
# of each device driver in this file see the section 4 man page for the
# device.

include	"arch/mvme88k/conf/GENERIC"

option	MULTIPROCESSOR
#option	MP_LOCKDEBUG
