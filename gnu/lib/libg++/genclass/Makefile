# Makefile for g++ library genclass 

PROG=		genclass
SRCS=		genclass.sh
NOMAN=
STRIP=

genclass:	genclass.sh
	sed -e 's|^PROTODIR=.*|PROTODIR=${DESTDIR}/usr/include/g++/gen|' \
	    -e 's|<VERSION>|2.4|' ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
