# $OpenBSD: Makefile,v 1.7 2019/12/21 21:40:00 espie Exp $

FILES=	OPENBSD-SNMPD-CONF.txt OPENBSD-BASE-MIB.txt
FILES+=	OPENBSD-MEM-MIB.txt OPENBSD-SENSORS-MIB.txt
FILES+= OPENBSD-CARP-MIB.txt OPENBSD-PF-MIB.txt
FILES+= OPENBSD-RELAYD-MIB.txt

all clean cleandir obj tags: _SUBDIRUSE

realinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 0444 \
	    ${FILES} ${DESTDIR}${BINDIR}/snmp/mibs

.include <bsd.prog.mk>
