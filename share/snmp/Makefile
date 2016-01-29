# $OpenBSD: Makefile,v 1.4 2016/01/29 03:06:00 jsg Exp $

FILES=	OPENBSD-SNMPD-CONF.txt OPENBSD-BASE-MIB.txt
FILES+=	OPENBSD-MEM-MIB.txt OPENBSD-SENSORS-MIB.txt
FILES+= OPENBSD-CARP-MIB.txt OPENBSD-PF-MIB.txt
FILES+= OPENBSD-RELAYD-MIB.txt

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	${INSTALL} -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/snmp/mibs

.include <bsd.prog.mk>
