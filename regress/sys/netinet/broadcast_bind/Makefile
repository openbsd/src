# $OpenBSD: Makefile,v 1.1 2015/10/27 16:05:54 vgross Exp $

PROG	 = broadcast_bind
IFACE	 = vether11
INSFX	!= jot -s '.' -r 2 0 255
INADDR	 = 10.${INSFX}.11
BADADDR	 = 10.${INSFX}.22
BCADDR	 = 10.${INSFX}.255

run-regress-broadcast_bind: ${PROG}
	ifconfig ${IFACE} create
	ifconfig ${IFACE} inet ${INADDR}/24 up
	${.OBJDIR}/broadcast_bind ${INADDR} ${BADADDR} ${BCADDR}
	ifconfig ${IFACE} destroy

.include <bsd.regress.mk>
