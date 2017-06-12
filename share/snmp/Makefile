# $OpenBSD: Makefile,v 1.6 2017/06/12 18:49:34 espie Exp $

FILES=	OPENBSD-SNMPD-CONF.txt OPENBSD-BASE-MIB.txt
FILES+=	OPENBSD-MEM-MIB.txt OPENBSD-SENSORS-MIB.txt
FILES+= OPENBSD-CARP-MIB.txt OPENBSD-PF-MIB.txt
FILES+= OPENBSD-RELAYD-MIB.txt

all clean cleandir depend obj tags: _SUBDIRUSE

realinstall:
	${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 0444 \
	    ${FILES} ${DESTDIR}${BINDIR}/snmp/mibs

.include <bsd.prog.mk>
