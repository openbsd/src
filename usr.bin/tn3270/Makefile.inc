#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:46:18 deraadt Exp $

CFLAGS += -DTERMCAP -DSRCRT -DKLUDGELINEMODE -DUSE_TERMIO -DTN3270 -Dunix
KBD = unix.kbd

.include "${.CURDIR}/../../Makefile.inc"
