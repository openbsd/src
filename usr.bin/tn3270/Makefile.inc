#	$OpenBSD: Makefile.inc,v 1.4 2000/10/12 03:22:06 millert Exp $

CFLAGS += -DSRCRT -DKLUDGELINEMODE -DUSE_TERMIO -DTN3270 -Dunix
KBD = unix.kbd

.include "${.CURDIR}/../../Makefile.inc"
