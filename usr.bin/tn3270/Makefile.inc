#	$OpenBSD: Makefile.inc,v 1.2 1996/06/26 05:40:59 deraadt Exp $

CFLAGS += -DTERMCAP -DSRCRT -DKLUDGELINEMODE -DUSE_TERMIO -DTN3270 -Dunix
KBD = unix.kbd

.include "${.CURDIR}/../../Makefile.inc"
