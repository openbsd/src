#	$OpenBSD: Makefile,v 1.2 1997/03/31 03:12:00 weingart Exp $

PROG=	biosboot
SRCS=	biosboot.S
AFLAGS+=-DREL=$(REL) #-Wa,-a
LDFLAGS=-Wl,-T0,-N,-x -nostdlib
STRIP=
MAN=	biosboot.8
LINKS=	${BINDIR}/biosboot ${BINDIR}/fdboot \
	${BINDIR}/biosboot ${BINDIR}/wdboot \
	${BINDIR}/biosboot ${BINDIR}/sdboot \

# XXX - Toby
# These should not output anything, once /boot is loaded, let
# it figure out who to talk to (probe the keyboard).  That way
# we will not need two different types of bootblocks...
#
# Uncomment this to make the boot block talk to a serial port.
# CPPFLAGS+=-DSERIAL=0

.include <bsd.prog.mk>
