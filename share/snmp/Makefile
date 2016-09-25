# $OpenBSD: Makefile,v 1.5 2016/09/25 17:43:11 natano Exp $

FILES=	OPENBSD-SNMPD-CONF.txt OPENBSD-BASE-MIB.txt
FILES+=	OPENBSD-MEM-MIB.txt OPENBSD-SENSORS-MIB.txt
FILES+= OPENBSD-CARP-MIB.txt OPENBSD-PF-MIB.txt
FILES+= OPENBSD-RELAYD-MIB.txt

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 0444 \
	    ${FILES} ${DESTDIR}${BINDIR}/snmp/mibs

.include <bsd.prog.mk>
